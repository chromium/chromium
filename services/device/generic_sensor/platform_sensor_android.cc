// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/device/generic_sensor/platform_sensor_android.h"

#include "base/functional/bind.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "services/device/generic_sensor/jni_headers/PlatformSensor_jni.h"

using base::android::JavaRef;
using jni_zero::AttachCurrentThread;

namespace device {
namespace {
void StartSensorBlocking(base::android::ScopedJavaGlobalRef<jobject> j_object,
                         double frequency) {
  device::Java_PlatformSensor_startSensor(AttachCurrentThread(), j_object,
                                          frequency);
}

void StopSensorBlocking(base::android::ScopedJavaGlobalRef<jobject> j_object) {
  device::Java_PlatformSensor_stopSensor(AttachCurrentThread(), j_object);
}
}  // namespace

// static
scoped_refptr<PlatformSensorAndroid> PlatformSensorAndroid::Create(
    mojom::SensorType type,
    SensorReadingSharedBuffer* reading_buffer,
    base::WeakPtr<PlatformSensorProvider> provider,
    const JavaRef<jobject>& java_provider) {
  auto sensor = base::MakeRefCounted<PlatformSensorAndroid>(
      type, reading_buffer, std::move(provider));
  JNIEnv* env = AttachCurrentThread();
  sensor->j_object_.Reset(
      Java_PlatformSensor_create(env, java_provider, static_cast<jint>(type),
                                 reinterpret_cast<jlong>(sensor.get())));
  if (!sensor->j_object_) {
    return nullptr;
  }

  return sensor;
}

PlatformSensorAndroid::PlatformSensorAndroid(
    mojom::SensorType type,
    SensorReadingSharedBuffer* reading_buffer,
    base::WeakPtr<PlatformSensorProvider> provider)
    : PlatformSensor(type, reading_buffer, std::move(provider)) {}

PlatformSensorAndroid::~PlatformSensorAndroid() {
  if (j_object_) {
    StopSensor();
    Java_PlatformSensor_sensorDestroyed(AttachCurrentThread(), j_object_);
  }
}

mojom::ReportingMode PlatformSensorAndroid::GetReportingMode() {
  JNIEnv* env = AttachCurrentThread();
  return static_cast<mojom::ReportingMode>(
      Java_PlatformSensor_getReportingMode(env, j_object_));
}

PlatformSensorConfiguration PlatformSensorAndroid::GetDefaultConfiguration() {
  JNIEnv* env = AttachCurrentThread();
  jdouble frequency =
      Java_PlatformSensor_getDefaultConfiguration(env, j_object_);
  return PlatformSensorConfiguration(frequency);
}

double PlatformSensorAndroid::GetMaximumSupportedFrequency() {
  JNIEnv* env = AttachCurrentThread();
  return Java_PlatformSensor_getMaximumSupportedFrequency(env, j_object_);
}

bool PlatformSensorAndroid::StartSensor(
    const PlatformSensorConfiguration& configuration) {
  sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&StartSensorBlocking, j_object_,
                                configuration.frequency()));
  return true;
}

void PlatformSensorAndroid::StopSensor() {
  sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&StopSensorBlocking, j_object_));
}

bool PlatformSensorAndroid::CheckSensorConfiguration(
    const PlatformSensorConfiguration& configuration) {
  JNIEnv* env = AttachCurrentThread();
  return Java_PlatformSensor_checkSensorConfiguration(
      env, j_object_, configuration.frequency());
}

void PlatformSensorAndroid::NotifyPlatformSensorError(
    JNIEnv*,
    const JavaRef<jobject>& caller) {
  // This function may be called from Java while this object's destructor is
  // being invoked, however we know that to reach this point we must be before
  // the completion of the call to Java_PlatformSensor_sensorDestroyed(). This
  // means that the WeakPtrFactory is still valid. The WeakPtr will detect
  // completion of the destructor.
  PostTaskToMainSequence(
      FROM_HERE,
      base::BindOnce(&PlatformSensorAndroid::NotifySensorError, AsWeakPtr()));
}

void PlatformSensorAndroid::UpdatePlatformSensorReading(
    JNIEnv*,
    const base::android::JavaRef<jobject>& caller,
    jdouble timestamp,
    jdouble value1,
    jdouble value2,
    jdouble value3,
    jdouble value4) {
  SensorReading reading;
  reading.raw.timestamp = timestamp;
  reading.raw.values[0] = value1;
  reading.raw.values[1] = value2;
  reading.raw.values[2] = value3;
  reading.raw.values[3] = value4;

  UpdateSharedBufferAndNotifyClients(reading);
}

void PlatformSensorAndroid::SimulateSensorEventFromJavaForTesting(
    base::android::ScopedJavaGlobalRef<jobject> j_object_,
    jint reading_values_length) {
  Java_PlatformSensor_simulateSensorEventForTesting(  // IN-TEST
      AttachCurrentThread(), j_object_, reading_values_length);
}

}  // namespace device
