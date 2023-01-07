// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_ORIENTATION_EULER_ANGLES_FUSION_ALGORITHM_USING_QUATERNION_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_ORIENTATION_EULER_ANGLES_FUSION_ALGORITHM_USING_QUATERNION_H_

#include "services/device/generic_sensor/platform_sensor_fusion_algorithm.h"

namespace device {

// Sensor fusion algorithm for implementing *_ORIENTATION_EULER_ANGLES
// using *_ORIENTATION_QUATERNION.
class OrientationEulerAnglesFusionAlgorithmUsingQuaternion
    : public PlatformSensorFusionAlgorithm {
 public:
  explicit OrientationEulerAnglesFusionAlgorithmUsingQuaternion(bool absolute);

  OrientationEulerAnglesFusionAlgorithmUsingQuaternion(
      const OrientationEulerAnglesFusionAlgorithmUsingQuaternion&) = delete;
  OrientationEulerAnglesFusionAlgorithmUsingQuaternion& operator=(
      const OrientationEulerAnglesFusionAlgorithmUsingQuaternion&) = delete;

  ~OrientationEulerAnglesFusionAlgorithmUsingQuaternion() override;

 protected:
  bool GetFusedDataInternal(mojom::SensorType which_sensor_changed,
                            SensorReading* fused_reading) override;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_ORIENTATION_EULER_ANGLES_FUSION_ALGORITHM_USING_QUATERNION_H_
