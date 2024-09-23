// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_ANDROID_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "services/device/generic_sensor/platform_sensor_provider.h"

namespace device {

class PlatformSensorProviderAndroid : public PlatformSensorProvider {
 public:
  PlatformSensorProviderAndroid();

  PlatformSensorProviderAndroid(const PlatformSensorProviderAndroid&) = delete;
  PlatformSensorProviderAndroid& operator=(
      const PlatformSensorProviderAndroid&) = delete;

  ~PlatformSensorProviderAndroid() override;

  base::WeakPtr<PlatformSensorProvider> AsWeakPtr() override;

  void SetSensorManagerToNullForTesting();

 protected:
  void CreateSensorInternal(mojom::SensorType type,
                            CreateSensorCallback callback) override;

 private:
  void CreateGravitySensor(JNIEnv* env,
                           CreateSensorCallback callback);
  void CreateLinearAccelerationSensor(JNIEnv* env,
                                      CreateSensorCallback callback);
  void CreateAbsoluteOrientationEulerAnglesSensor(
      JNIEnv* env,
      CreateSensorCallback callback);
  void CreateAbsoluteOrientationQuaternionSensor(
      JNIEnv* env,
      CreateSensorCallback callback);
  void CreateRelativeOrientationEulerAnglesSensor(
      JNIEnv* env,
      CreateSensorCallback callback);

  // Java object org.chromium.device.sensors.PlatformSensorProvider
  base::android::ScopedJavaGlobalRef<jobject> j_object_;

  base::WeakPtrFactory<PlatformSensorProviderAndroid> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_ANDROID_H_
