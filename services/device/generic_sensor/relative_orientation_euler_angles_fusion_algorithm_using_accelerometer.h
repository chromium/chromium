// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_RELATIVE_ORIENTATION_EULER_ANGLES_FUSION_ALGORITHM_USING_ACCELEROMETER_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_RELATIVE_ORIENTATION_EULER_ANGLES_FUSION_ALGORITHM_USING_ACCELEROMETER_H_

#include "services/device/generic_sensor/platform_sensor_fusion_algorithm.h"

namespace device {

// Sensor fusion algorithm for implementing RELATIVE_ORIENTATION_EULER_ANGLES
// using ACCELEROMETER.
class RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometer
    : public PlatformSensorFusionAlgorithm {
 public:
  RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometer();

  RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometer(
      const RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometer&) =
      delete;
  RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometer& operator=(
      const RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometer&) =
      delete;

  ~RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometer() override;

 protected:
  bool GetFusedDataInternal(mojom::SensorType which_sensor_changed,
                            SensorReading* fused_reading) override;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_RELATIVE_ORIENTATION_EULER_ANGLES_FUSION_ALGORITHM_USING_ACCELEROMETER_H_
