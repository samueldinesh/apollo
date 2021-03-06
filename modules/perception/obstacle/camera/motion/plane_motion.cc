/******************************************************************************
 * Copyright 2017 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#include "modules/common/log.h"
#include "modules/perception/obstacle/camera/motion/plane_motion.h"

namespace apollo {
namespace perception {

PlaneMotion::PlaneMotion(int s) { set_buffer_size(s); }

PlaneMotion::PlaneMotion(int s, bool sync_time_stamp, float time_unit)
    : time_unit_(time_unit) {
  set_buffer_size(s);
}

PlaneMotion::~PlaneMotion(void) {
  if (mot_buffer_ != nullptr) {
    mot_buffer_->clear();
    mot_buffer_ = nullptr;
  }
}

// Generate the inverse motion for past trajectory
void PlaneMotion::generate_motion_matrix(VehicleStatus *vehicledata) {
  Eigen::Matrix3f motion_2d = Eigen::Matrix3f::Identity();

  float theta = time_unit_ * vehicledata->yaw_rate;
  float displacement = time_unit_ * vehicledata->velocity;

  Eigen::Rotation2Df rot2d(theta);
  Eigen::Vector2f trans;

  trans(0) = displacement * cos(theta);
  trans(1) = displacement * sin(theta);

  motion_2d.block(0, 0, 2, 2) = rot2d.toRotationMatrix().transpose();
  motion_2d.block(0, 2, 2, 1) = -rot2d.toRotationMatrix().transpose() * trans;

  vehicledata->motion = motion_2d;
}

void PlaneMotion::accumulate_motion(VehicleStatus *vehicledata,
                                        float motion_time_dif) {
  generate_motion_matrix(vehicledata);  // compute vehicledata.motion
  // accumulate CAN+IMU / Localization motion
  mat_motion_2d_image_ *= vehicledata->motion;
  time_difference_ += motion_time_dif;
}

void PlaneMotion::update_motion_buffer(VehicleStatus *vehicledata) {
  for (int k = 0; k < static_cast<int>(mot_buffer_->size()); k++) {
    (*mot_buffer_)[k].motion *= mat_motion_2d_image_;
  }

  vehicledata->time_d = time_difference_;
  vehicledata->motion = mat_motion_2d_image_;
  mot_buffer_->push_back(*vehicledata);  // a new image frame is added
  mat_motion_2d_image_ =
      Eigen::Matrix3f::Identity();  // reset image accumulated motion
  time_difference_ = 0;             // reset the accumulated time difference
}
void PlaneMotion::add_new_motion(VehicleStatus *vehicledata,
                                 float motion_time_dif,
                                 int motion_operation_flag) {
  switch (motion_operation_flag) {
    case ACCUM_MOTION:
        accumulate_motion(vehicledata, motion_time_dif);
        break;
    case ACCUM_PUSH_MOTION:
        accumulate_motion(vehicledata, motion_time_dif);
        update_motion_buffer(vehicledata);
        break;
    case PUSH_ACCUM_MOTION:
        update_motion_buffer(vehicledata);
        accumulate_motion(vehicledata, motion_time_dif);
        break;
    default:
        AERROR << "motion operation flag:wrong type";
        return;
  }
}

}  // namespace perception
}  // namespace apollo
