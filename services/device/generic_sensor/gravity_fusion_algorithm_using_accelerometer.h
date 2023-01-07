// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_GRAVITY_FUSION_ALGORITHM_USING_ACCELEROMETER_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_GRAVITY_FUSION_ALGORITHM_USING_ACCELEROMETER_H_

#include "services/device/generic_sensor/platform_sensor_fusion_algorithm.h"

namespace device {

// Algorithm that obtains gravity values from data provided by
// accelerometer sensor.
class GravityFusionAlgorithmUsingAccelerometer final
    : public PlatformSensorFusionAlgorithm {
 public:
  GravityFusionAlgorithmUsingAccelerometer();

  GravityFusionAlgorithmUsingAccelerometer(
      const GravityFusionAlgorithmUsingAccelerometer&) = delete;
  GravityFusionAlgorithmUsingAccelerometer& operator=(
      const GravityFusionAlgorithmUsingAccelerometer&) = delete;

  ~GravityFusionAlgorithmUsingAccelerometer() override;

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

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_GRAVITY_FUSION_ALGORITHM_USING_ACCELEROMETER_H_
