// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_ABSOLUTE_ORIENTATION_EULER_ANGLES_FUSION_ALGORITHM_USING_ACCELEROMETER_AND_MAGNETOMETER_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_ABSOLUTE_ORIENTATION_EULER_ANGLES_FUSION_ALGORITHM_USING_ACCELEROMETER_AND_MAGNETOMETER_H_

#include "services/device/generic_sensor/platform_sensor_fusion_algorithm.h"

namespace device {

// Sensor fusion algorithm for implementing ABSOLUTE_ORIENTATION_EULER_ANGLES
// using ACCELEROMETER and MAGNETOMETER.
class
    AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometer
    : public PlatformSensorFusionAlgorithm {
 public:
  AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometer();

  AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometer(
      const AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometer&) =
      delete;
  AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometer&
  operator=(
      const AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometer&) =
      delete;

  ~AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometer()
      override;

 protected:
  bool GetFusedDataInternal(mojom::SensorType which_sensor_changed,
                            SensorReading* fused_reading) override;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_ABSOLUTE_ORIENTATION_EULER_ANGLES_FUSION_ALGORITHM_USING_ACCELEROMETER_AND_MAGNETOMETER_H_
