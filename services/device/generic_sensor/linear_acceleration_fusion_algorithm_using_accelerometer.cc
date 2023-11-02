// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/linear_acceleration_fusion_algorithm_using_accelerometer.h"

#include "base/check.h"
#include "services/device/generic_sensor/platform_sensor_fusion.h"

namespace device {

LinearAccelerationFusionAlgorithmUsingAccelerometer::
    LinearAccelerationFusionAlgorithmUsingAccelerometer()
    : PlatformSensorFusionAlgorithm(mojom::SensorType::LINEAR_ACCELERATION,
                                    {mojom::SensorType::ACCELEROMETER}) {
  Reset();
}

LinearAccelerationFusionAlgorithmUsingAccelerometer::
    ~LinearAccelerationFusionAlgorithmUsingAccelerometer() = default;

void LinearAccelerationFusionAlgorithmUsingAccelerometer::SetFrequency(
    double frequency) {
  DCHECK(frequency > 0);
  // Set time constant to be bound to requested rate, if sensor can provide
  // data at such rate, low-pass filter alpha would be ~0.5 that is effective
  // at smoothing data and provides minimal latency from LPF.
  time_constant_ = 1 / frequency;
}

void LinearAccelerationFusionAlgorithmUsingAccelerometer::Reset() {
  reading_updates_count_ = 0;
  time_constant_ = 0.0;
  initial_timestamp_ = 0.0;
  gravity_x_ = 0.0;
  gravity_y_ = 0.0;
  gravity_z_ = 0.0;
}

bool LinearAccelerationFusionAlgorithmUsingAccelerometer::GetFusedDataInternal(
    mojom::SensorType which_sensor_changed,
    SensorReading* fused_reading) {
  DCHECK(fusion_sensor_);

  ++reading_updates_count_;

  SensorReading reading;
  if (!fusion_sensor_->GetSourceReading(mojom::SensorType::ACCELEROMETER,
                                        &reading)) {
    return false;
  }

  // First reading.
  if (initial_timestamp_ == 0.0) {
    initial_timestamp_ = reading.timestamp();
    return false;
  }

  double delivery_rate =
      (reading.timestamp() - initial_timestamp_) / reading_updates_count_;
  double alpha = time_constant_ / (time_constant_ + delivery_rate);

  double acceleration_x = reading.accel.x;
  double acceleration_y = reading.accel.y;
  double acceleration_z = reading.accel.z;

  // Isolate gravity.
  gravity_x_ = alpha * gravity_x_ + (1 - alpha) * acceleration_x;
  gravity_y_ = alpha * gravity_y_ + (1 - alpha) * acceleration_y;
  gravity_z_ = alpha * gravity_z_ + (1 - alpha) * acceleration_z;

  // Get linear acceleration.
  fused_reading->accel.x = acceleration_x - gravity_x_;
  fused_reading->accel.y = acceleration_y - gravity_y_;
  fused_reading->accel.z = acceleration_z - gravity_z_;

  return true;
}

}  // namespace device
