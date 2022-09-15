// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_RELATIVE_ORIENTATION_EULER_ANGLES_FUSION_ALGORITHM_USING_ACCELEROMETER_AND_GYROSCOPE_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_RELATIVE_ORIENTATION_EULER_ANGLES_FUSION_ALGORITHM_USING_ACCELEROMETER_AND_GYROSCOPE_H_

#include "services/device/generic_sensor/platform_sensor_fusion_algorithm.h"

namespace device {

class
    RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndGyroscope
        final : public PlatformSensorFusionAlgorithm {
 public:
  RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndGyroscope();

  RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndGyroscope(
      const RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndGyroscope&) =
      delete;
  RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndGyroscope&
  operator=(
      const RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndGyroscope&) =
      delete;

  ~RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndGyroscope()
      override;

  void Reset() override;

 protected:
  bool GetFusedDataInternal(mojom::SensorType which_sensor_changed,
                            SensorReading* fused_reading) override;

 private:
  double timestamp_;
  double alpha_;
  double beta_;
  double gamma_;
  const double kBias = 0.98;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_RELATIVE_ORIENTATION_EULER_ANGLES_FUSION_ALGORITHM_USING_ACCELEROMETER_AND_GYROSCOPE_H_
