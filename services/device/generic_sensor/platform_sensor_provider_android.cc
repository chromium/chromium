// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_provider_android.h"

#include <utility>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/memory/ref_counted.h"
#include "services/device/generic_sensor/absolute_orientation_euler_angles_fusion_algorithm_using_accelerometer_and_magnetometer.h"
#include "services/device/generic_sensor/gravity_fusion_algorithm_using_accelerometer.h"
#include "services/device/generic_sensor/linear_acceleration_fusion_algorithm_using_accelerometer.h"
#include "services/device/generic_sensor/orientation_euler_angles_fusion_algorithm_using_quaternion.h"
#include "services/device/generic_sensor/orientation_quaternion_fusion_algorithm_using_euler_angles.h"
#include "services/device/generic_sensor/platform_sensor_android.h"
#include "services/device/generic_sensor/platform_sensor_fusion.h"
#include "services/device/generic_sensor/relative_orientation_euler_angles_fusion_algorithm_using_accelerometer.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "services/device/generic_sensor/jni_headers/PlatformSensorProvider_jni.h"

using base::android::ScopedJavaLocalRef;
using jni_zero::AttachCurrentThread;

namespace device {

PlatformSensorProviderAndroid::PlatformSensorProviderAndroid() {
  JNIEnv* env = AttachCurrentThread();
  j_object_.Reset(Java_PlatformSensorProvider_create(env));
}

PlatformSensorProviderAndroid::~PlatformSensorProviderAndroid() = default;

base::WeakPtr<PlatformSensorProvider>
PlatformSensorProviderAndroid::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void PlatformSensorProviderAndroid::SetSensorManagerToNullForTesting() {
  JNIEnv* env = AttachCurrentThread();
  Java_PlatformSensorProvider_setSensorManagerToNullForTesting(env, j_object_);
}

void PlatformSensorProviderAndroid::CreateSensorInternal(
    mojom::SensorType type,
    CreateSensorCallback callback) {
  JNIEnv* env = AttachCurrentThread();

  // Some of the sensors may not be available depending on the device and
  // Android version, so the fallback ensures selection of the best possible
  // option.
  switch (type) {
    case mojom::SensorType::GRAVITY:
      CreateGravitySensor(env, std::move(callback));
      break;
    case mojom::SensorType::LINEAR_ACCELERATION:
      CreateLinearAccelerationSensor(env, std::move(callback));
      break;
    case mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES:
      CreateAbsoluteOrientationEulerAnglesSensor(env, std::move(callback));
      break;
    case mojom::SensorType::ABSOLUTE_ORIENTATION_QUATERNION:
      CreateAbsoluteOrientationQuaternionSensor(env, std::move(callback));
      break;
    case mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES:
      CreateRelativeOrientationEulerAnglesSensor(env, std::move(callback));
      break;
    default: {
      std::move(callback).Run(PlatformSensorAndroid::Create(
          type, GetSensorReadingSharedBufferForType(type), AsWeakPtr(),
          j_object_));
      break;
    }
  }
}

// For GRAVITY we see if the platform supports it directly through
// TYPE_GRAVITY. If not we use a fusion algorithm to remove the
// contribution of linear acceleration from the raw ACCELEROMETER.
void PlatformSensorProviderAndroid::CreateGravitySensor(
    JNIEnv* env,
    CreateSensorCallback callback) {
  auto sensor = PlatformSensorAndroid::Create(
      mojom::SensorType::GRAVITY,
      GetSensorReadingSharedBufferForType(mojom::SensorType::GRAVITY),
      AsWeakPtr(), j_object_);

  if (sensor) {
    std::move(callback).Run(std::move(sensor));
  } else {
    auto sensor_fusion_algorithm =
        std::make_unique<GravityFusionAlgorithmUsingAccelerometer>();

    // If this PlatformSensorFusion object is successfully initialized,
    // |callback| will be run with a reference to this object.
    PlatformSensorFusion::Create(
        AsWeakPtr(), std::move(sensor_fusion_algorithm), std::move(callback));
  }
}

// For LINEAR_ACCELERATION we see if the platform supports it directly through
// TYPE_LINEAR_ACCELERATION. If not we use a fusion algorithm to remove the
// contribution of gravity from the raw ACCELEROMETER.
void PlatformSensorProviderAndroid::CreateLinearAccelerationSensor(
    JNIEnv* env,
    CreateSensorCallback callback) {
  auto sensor =
      PlatformSensorAndroid::Create(mojom::SensorType::LINEAR_ACCELERATION,
                                    GetSensorReadingSharedBufferForType(
                                        mojom::SensorType::LINEAR_ACCELERATION),
                                    AsWeakPtr(), j_object_);

  if (sensor) {
    std::move(callback).Run(std::move(sensor));
  } else {
    auto sensor_fusion_algorithm =
        std::make_unique<LinearAccelerationFusionAlgorithmUsingAccelerometer>();

    // If this PlatformSensorFusion object is successfully initialized,
    // |callback| will be run with a reference to this object.
    PlatformSensorFusion::Create(
        AsWeakPtr(), std::move(sensor_fusion_algorithm), std::move(callback));
  }
}

// For ABSOLUTE_ORIENTATION_EULER_ANGLES we use a 3-way fallback approach
// where up to 3 different sets of sensors are attempted if necessary. The
// sensors to be used are determined in the following order:
//   A: ABSOLUTE_ORIENTATION_QUATERNION (if it uses TYPE_ROTATION_VECTOR
//      directly)
//   B: TODO(juncai): Combination of ACCELEROMETER, MAGNETOMETER and
//      GYROSCOPE
//   C: Combination of ACCELEROMETER and MAGNETOMETER
void PlatformSensorProviderAndroid::CreateAbsoluteOrientationEulerAnglesSensor(
    JNIEnv* env,
    CreateSensorCallback callback) {
  if (static_cast<bool>(Java_PlatformSensorProvider_hasSensorType(
          env, j_object_,
          static_cast<jint>(
              mojom::SensorType::ABSOLUTE_ORIENTATION_QUATERNION)))) {
    auto sensor_fusion_algorithm =
        std::make_unique<OrientationEulerAnglesFusionAlgorithmUsingQuaternion>(
            true /* absolute */);

    // If this PlatformSensorFusion object is successfully initialized,
    // |callback| will be run with a reference to this object.
    PlatformSensorFusion::Create(
        AsWeakPtr(), std::move(sensor_fusion_algorithm), std::move(callback));
  } else {
    auto sensor_fusion_algorithm = std::make_unique<
        AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometer>();

    // If this PlatformSensorFusion object is successfully initialized,
    // |callback| will be run with a reference to this object.
    PlatformSensorFusion::Create(
        AsWeakPtr(), std::move(sensor_fusion_algorithm), std::move(callback));
  }
}

// For ABSOLUTE_ORIENTATION_QUATERNION we use a 2-way fallback approach
// where up to 2 different sets of sensors are attempted if necessary. The
// sensors to be used are determined in the following order:
//   A: Use TYPE_ROTATION_VECTOR directly
//   B: ABSOLUTE_ORIENTATION_EULER_ANGLES
void PlatformSensorProviderAndroid::CreateAbsoluteOrientationQuaternionSensor(
    JNIEnv* env,
    CreateSensorCallback callback) {
  auto sensor = PlatformSensorAndroid::Create(
      mojom::SensorType::ABSOLUTE_ORIENTATION_QUATERNION,
      GetSensorReadingSharedBufferForType(
          mojom::SensorType::ABSOLUTE_ORIENTATION_QUATERNION),
      AsWeakPtr(), j_object_);

  if (sensor) {
    std::move(callback).Run(std::move(sensor));
  } else {
    auto sensor_fusion_algorithm =
        std::make_unique<OrientationQuaternionFusionAlgorithmUsingEulerAngles>(
            true /* absolute */);

    // If this PlatformSensorFusion object is successfully initialized,
    // |callback| will be run with a reference to this object.
    PlatformSensorFusion::Create(
        AsWeakPtr(), std::move(sensor_fusion_algorithm), std::move(callback));
  }
}

// For RELATIVE_ORIENTATION_EULER_ANGLES we use RELATIVE_ORIENTATION_QUATERNION
// (if it uses TYPE_GAME_ROTATION_VECTOR directly).
void PlatformSensorProviderAndroid::CreateRelativeOrientationEulerAnglesSensor(
    JNIEnv* env,
    CreateSensorCallback callback) {
  if (static_cast<bool>(Java_PlatformSensorProvider_hasSensorType(
          env, j_object_,
          static_cast<jint>(
              mojom::SensorType::RELATIVE_ORIENTATION_QUATERNION)))) {
    auto sensor_fusion_algorithm =
        std::make_unique<OrientationEulerAnglesFusionAlgorithmUsingQuaternion>(
            false /* absolute */);

    // If this PlatformSensorFusion object is successfully initialized,
    // |callback| will be run with a reference to this object.
    PlatformSensorFusion::Create(
        AsWeakPtr(), std::move(sensor_fusion_algorithm), std::move(callback));
  } else {
    std::move(callback).Run(nullptr);
  }
}

}  // namespace device
