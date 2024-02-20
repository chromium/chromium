// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/relative_orientation_euler_angles_fusion_algorithm_using_accelerometer.h"

#include <cmath>

#include "base/check_op.h"
#include "base/numerics/angle_conversions.h"
#include "base/numerics/math_constants.h"
#include "services/device/generic_sensor/platform_sensor_fusion.h"

namespace device {

namespace {

void ComputeRelativeOrientationFromAccelerometer(double acceleration_x,
                                                 double acceleration_y,
                                                 double acceleration_z,
                                                 double* alpha_in_degrees,
                                                 double* beta_in_degrees,
                                                 double* gamma_in_degrees) {
  // Transform the accelerometer values to W3C draft angles.
  //
  // Accelerometer values are just dot products of the sensor axes
  // by the gravity vector 'g' with the result for the z axis inverted.
  //
  // To understand this transformation calculate the 3rd row of the z-x-y
  // Euler angles rotation matrix (because of the 'g' vector, only 3rd row
  // affects to the result). Note that z-x-y matrix means R = Ry * Rx * Rz.
  // Then, assume alpha = 0 and you get this:
  //
  // x_acc = sin(gamma)
  // y_acc = - cos(gamma) * sin(beta)
  // z_acc = cos(beta) * cos(gamma)
  //
  // After that the rest is just a bit of trigonometry.
  //
  // Also note that alpha can't be provided but it's assumed to be always zero.
  // This is necessary in order to provide enough information to solve
  // the equations.
  *alpha_in_degrees = NAN;
  *beta_in_degrees =
      base::RadToDeg(std::atan2(-acceleration_y, acceleration_z));
  *gamma_in_degrees =
      base::RadToDeg(std::asin(acceleration_x / base::kMeanGravityDouble));

  // Convert beta and gamma to fit the intervals in the specification. Beta is
  // [-180, 180) and gamma is [-90, 90).
  if (*beta_in_degrees >= 180.0)
    *beta_in_degrees = -180.0;
  if (*gamma_in_degrees >= 90.0)
    *gamma_in_degrees = -90.0;

  DCHECK_GE(*beta_in_degrees, -180.0);
  DCHECK_LT(*beta_in_degrees, 180.0);
  DCHECK_GE(*gamma_in_degrees, -90.0);
  DCHECK_LT(*gamma_in_degrees, 90.0);
}

}  // namespace

RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometer::
    RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometer()
    : PlatformSensorFusionAlgorithm(
          mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES,
          {mojom::SensorType::ACCELEROMETER}) {}

RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometer::
    ~RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometer() =
        default;

bool RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometer::
    GetFusedDataInternal(mojom::SensorType which_sensor_changed,
                         SensorReading* fused_reading) {
  DCHECK(fusion_sensor_);

  SensorReading reading;
  if (!fusion_sensor_->GetSourceReading(mojom::SensorType::ACCELEROMETER,
                                        &reading)) {
    return false;
  }

  ComputeRelativeOrientationFromAccelerometer(
      reading.accel.x, reading.accel.y, reading.accel.z,
      &fused_reading->orientation_euler.z.value() /* alpha_in_degrees */,
      &fused_reading->orientation_euler.x.value() /* beta_in_degrees */,
      &fused_reading->orientation_euler.y.value() /* gamma_in_degrees */);

  return true;
}

}  // namespace device
