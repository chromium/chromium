// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_LINEAR_ACCELERATION_FUSION_ALGORITHM_USING_ACCELEROMETER_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_LINEAR_ACCELERATION_FUSION_ALGORITHM_USING_ACCELEROMETER_H_

#include "services/device/generic_sensor/platform_sensor_fusion_algorithm.h"

namespace device {

// Algotithm that obtains linear acceleration values from data provided by
// accelerometer sensor. Simple low-pass filter is used to isolate gravity
// and subtract it from accelerometer data to get linear acceleration.
class LinearAccelerationFusionAlgorithmUsingAccelerometer final
    : public PlatformSensorFusionAlgorithm {
 public:
  LinearAccelerationFusionAlgorithmUsingAccelerometer();

  LinearAccelerationFusionAlgorithmUsingAccelerometer(
      const LinearAccelerationFusionAlgorithmUsingAccelerometer&) = delete;
  LinearAccelerationFusionAlgorithmUsingAccelerometer& operator=(
      const LinearAccelerationFusionAlgorithmUsingAccelerometer&) = delete;

  ~LinearAccelerationFusionAlgorithmUsingAccelerometer() override;

  void SetFrequency(double frequency) override;
  void Reset() override;

 protected:
  bool GetFusedDataInternal(mojom::SensorType which_sensor_changed,
                            SensorReading* fused_reading) override;

 private:
  unsigned long reading_updates_count_;
  // The time constant for low-pass filter.
  double time_constant_;
  double initial_timestamp_;
  double gravity_x_;
  double gravity_y_;
  double gravity_z_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_LINEAR_ACCELERATION_FUSION_ALGORITHM_USING_ACCELEROMETER_H_
