// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_provider_linux_base.h"

#include <memory>
#include <utility>

#include "services/device/generic_sensor/absolute_orientation_euler_angles_fusion_algorithm_using_accelerometer_and_magnetometer.h"
#include "services/device/generic_sensor/gravity_fusion_algorithm_using_accelerometer.h"
#include "services/device/generic_sensor/linear_acceleration_fusion_algorithm_using_accelerometer.h"
#include "services/device/generic_sensor/orientation_quaternion_fusion_algorithm_using_euler_angles.h"
#include "services/device/generic_sensor/platform_sensor_fusion.h"
#include "services/device/generic_sensor/relative_orientation_euler_angles_fusion_algorithm_using_accelerometer.h"
#include "services/device/generic_sensor/relative_orientation_euler_angles_fusion_algorithm_using_accelerometer_and_gyroscope.h"

namespace device {

bool PlatformSensorProviderLinuxBase::IsFusionSensorType(
    mojom::SensorType type) const {
  switch (type) {
    case mojom::SensorType::LINEAR_ACCELERATION:
    case mojom::SensorType::GRAVITY:
    case mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES:
    case mojom::SensorType::ABSOLUTE_ORIENTATION_QUATERNION:
    case mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES:
    case mojom::SensorType::RELATIVE_ORIENTATION_QUATERNION:
      return true;
    default:
      return false;
  }
}

void PlatformSensorProviderLinuxBase::CreateFusionSensor(
    mojom::SensorType type,
    CreateSensorCallback callback) {
  DCHECK(IsFusionSensorType(type));
  std::unique_ptr<PlatformSensorFusionAlgorithm> fusion_algorithm;
  switch (type) {
    case mojom::SensorType::LINEAR_ACCELERATION:
      fusion_algorithm = std::make_unique<
          LinearAccelerationFusionAlgorithmUsingAccelerometer>();
      break;
    case mojom::SensorType::GRAVITY:
      fusion_algorithm =
          std::make_unique<GravityFusionAlgorithmUsingAccelerometer>();
      break;
    case mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES:
      fusion_algorithm = std::make_unique<
          AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometer>();
      break;
    case mojom::SensorType::ABSOLUTE_ORIENTATION_QUATERNION:
      fusion_algorithm = std::make_unique<
          OrientationQuaternionFusionAlgorithmUsingEulerAngles>(
          true /* absolute */);
      break;
    case mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES:
      if (IsSensorTypeAvailable(mojom::SensorType::GYROSCOPE)) {
        fusion_algorithm = std::make_unique<
            RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndGyroscope>();
      } else {
        fusion_algorithm = std::make_unique<
            RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometer>();
      }
      break;
    case mojom::SensorType::RELATIVE_ORIENTATION_QUATERNION:
      fusion_algorithm = std::make_unique<
          OrientationQuaternionFusionAlgorithmUsingEulerAngles>(
          false /* absolute */);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  DCHECK(fusion_algorithm);
  PlatformSensorFusion::Create(AsWeakPtr(), std::move(fusion_algorithm),
                               std::move(callback));
}

}  // namespace device
