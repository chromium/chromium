// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/relative_orientation_euler_angles_fusion_algorithm_using_accelerometer_and_gyroscope.h"

#include <cmath>

#include "base/check.h"
#include "base/numerics/math_constants.h"
#include "services/device/generic_sensor/generic_sensor_consts.h"
#include "services/device/generic_sensor/platform_sensor_fusion.h"
#include "ui/gfx/geometry/angle_conversions.h"

namespace device {

RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndGyroscope::
    RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndGyroscope()
    : PlatformSensorFusionAlgorithm(
          mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES,
          {mojom::SensorType::ACCELEROMETER, mojom::SensorType::GYROSCOPE}) {
  Reset();
}

RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndGyroscope::
    ~RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndGyroscope() =
        default;

void RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndGyroscope::
    Reset() {
  timestamp_ = 0.0;
  alpha_ = 0.0;
  beta_ = 0.0;
  gamma_ = 0.0;
}

bool RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndGyroscope::
    GetFusedDataInternal(mojom::SensorType which_sensor_changed,
                         SensorReading* fused_reading) {
  DCHECK(fusion_sensor_);

  // Only generate a new sensor value when the gyroscope reading changes.
  if (which_sensor_changed != mojom::SensorType::GYROSCOPE)
    return false;

  SensorReading accelerometer_reading;
  SensorReading gyroscope_reading;
  if (!fusion_sensor_->GetSourceReading(mojom::SensorType::ACCELEROMETER,
                                        &accelerometer_reading) ||
      !fusion_sensor_->GetSourceReading(mojom::SensorType::GYROSCOPE,
                                        &gyroscope_reading)) {
    return false;
  }

  double dt =
      (timestamp_ != 0.0) ? (gyroscope_reading.timestamp() - timestamp_) : 0.0;
  timestamp_ = gyroscope_reading.timestamp();

  double accel_x = accelerometer_reading.accel.x;
  double accel_y = accelerometer_reading.accel.y;
  double accel_z = accelerometer_reading.accel.z;
  double gyro_x = gyroscope_reading.gyro.x;
  double gyro_y = gyroscope_reading.gyro.y;
  double gyro_z = gyroscope_reading.gyro.z;

  // Treat the acceleration vector as an orientation vector by normalizing it.
  // Keep in mind that the if the device is flipped, the vector will just be
  // pointing in the other direction, so we have no way to know from the
  // accelerometer data which way the device is oriented.
  double norm =
      std::sqrt(accel_x * accel_x + accel_y * accel_y + accel_z * accel_z);
  double norm_reciprocal = 0.0;
  // Avoid dividing by zero.
  if (norm > kEpsilon)
    norm_reciprocal = 1 / norm;

  // As we only can cover half (PI rad) of the full spectrum (2*PI rad) we
  // multiply the unit vector with values from [-1, 1] with PI/2, covering
  // [-PI/2, PI/2].
  double scale = base::kPiDouble / 2;

  alpha_ += gyro_z * dt;
  // Make sure |alpha_| is in [0, 2*PI).
  alpha_ = std::fmod(alpha_, 2 * base::kPiDouble);
  if (alpha_ < 0)
    alpha_ += 2 * base::kPiDouble;

  beta_ = kBias * (beta_ + gyro_x * dt) +
          (1.0 - kBias) * (accel_x * scale * norm_reciprocal);
  // Make sure |beta_| is in [-PI, PI).
  beta_ = std::fmod(beta_, 2 * base::kPiDouble);
  if (beta_ >= base::kPiDouble)
    beta_ -= 2 * base::kPiDouble;
  else if (beta_ < -base::kPiDouble)
    beta_ += 2 * base::kPiDouble;

  gamma_ = kBias * (gamma_ + gyro_y * dt) +
           (1.0 - kBias) * (accel_y * -scale * norm_reciprocal);
  // Make sure |gamma_| is in [-PI/2, PI/2).
  gamma_ = std::fmod(gamma_, base::kPiDouble);
  if (gamma_ >= base::kPiDouble / 2)
    gamma_ -= base::kPiDouble;
  else if (gamma_ < -base::kPiDouble / 2)
    gamma_ += base::kPiDouble;

  fused_reading->orientation_euler.z = gfx::RadToDeg(alpha_);
  fused_reading->orientation_euler.x = gfx::RadToDeg(beta_);
  fused_reading->orientation_euler.y = gfx::RadToDeg(gamma_);

  return true;
}

}  // namespace device
