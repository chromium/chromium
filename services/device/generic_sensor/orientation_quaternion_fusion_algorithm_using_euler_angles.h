// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_ORIENTATION_QUATERNION_FUSION_ALGORITHM_USING_EULER_ANGLES_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_ORIENTATION_QUATERNION_FUSION_ALGORITHM_USING_EULER_ANGLES_H_

#include "services/device/generic_sensor/platform_sensor_fusion_algorithm.h"

namespace device {

// Sensor fusion algorithm for implementing *_ORIENTATION_QUATERNION
// using *_ORIENTATION_EULER_ANGLES.
class OrientationQuaternionFusionAlgorithmUsingEulerAngles
    : public PlatformSensorFusionAlgorithm {
 public:
  explicit OrientationQuaternionFusionAlgorithmUsingEulerAngles(bool absolute);

  OrientationQuaternionFusionAlgorithmUsingEulerAngles(
      const OrientationQuaternionFusionAlgorithmUsingEulerAngles&) = delete;
  OrientationQuaternionFusionAlgorithmUsingEulerAngles& operator=(
      const OrientationQuaternionFusionAlgorithmUsingEulerAngles&) = delete;

  ~OrientationQuaternionFusionAlgorithmUsingEulerAngles() override;

 protected:
  bool GetFusedDataInternal(mojom::SensorType which_sensor_changed,
                            SensorReading* fused_reading) override;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_ORIENTATION_QUATERNION_FUSION_ALGORITHM_USING_EULER_ANGLES_H_
