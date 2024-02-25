// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/generic_sensor/orientation_util.h"

#include <cmath>

#include "base/numerics/angle_conversions.h"

namespace device {

bool ComputeQuaternionFromEulerAngles(double alpha,
                                      double beta,
                                      double gamma,
                                      SensorReading* out_reading) {
  // Ensure that the angles are within the boundaries specified by the Device
  // Orientation API.
  if (alpha < 0.0 || alpha >= 360.0 || beta < -180.0 || beta >= 180.0 ||
      gamma < -90.0 || gamma >= 90.0) {
    return false;
  }

  const double half_x_angle = base::DegToRad(beta) * 0.5;
  const double half_y_angle = base::DegToRad(gamma) * 0.5;
  const double half_z_angle = base::DegToRad(alpha) * 0.5;

  const double cos_z = std::cos(half_z_angle);
  const double sin_z = std::sin(half_z_angle);
  const double cos_y = std::cos(half_y_angle);
  const double sin_y = std::sin(half_y_angle);
  const double cos_x = std::cos(half_x_angle);
  const double sin_x = std::sin(half_x_angle);

  out_reading->orientation_quat.x =
      sin_x * cos_y * cos_z - cos_x * sin_y * sin_z;
  out_reading->orientation_quat.y =
      cos_x * sin_y * cos_z + sin_x * cos_y * sin_z;
  out_reading->orientation_quat.z =
      cos_x * cos_y * sin_z + sin_x * sin_y * cos_z;
  out_reading->orientation_quat.w =
      cos_x * cos_y * cos_z - sin_x * sin_y * sin_z;
  return true;
}

}  // namespace device
