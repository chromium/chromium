// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_android.h"

#include "base/bind.h"
#include "services/device/generic_sensor/jni_headers/PlatformSensor_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;

namespace device {

// static
scoped_refptr<PlatformSensorAndroid> PlatformSensorAndroid::Create(
    mojom::SensorType type,
    SensorReadingSharedBuffer* reading_buffer,
    PlatformSensorProvider* provider,
    const JavaRef<jobject>& java_provider) {
  auto sensor = base::MakeRefCounted<PlatformSensorAndroid>(
      type, reading_buffer, provider);
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
    PlatformSensorProvider* provider)
    : PlatformSensor(type, reading_buffer, provider) {}

PlatformSensorAndroid::~PlatformSensorAndroid() {
  JNIEnv* env = AttachCurrentThread();
  if (j_object_) {
    Java_PlatformSensor_sensorDestroyed(env, j_object_);
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
  JNIEnv* env = AttachCurrentThread();
  return Java_PlatformSensor_startSensor(env, j_object_,
                                         configuration.frequency());
}

void PlatformSensorAndroid::StopSensor() {
  JNIEnv* env = AttachCurrentThread();
  Java_PlatformSensor_stopSensor(env, j_object_);
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
  PostTaskToMainSequence(
      FROM_HERE,
      base::BindOnce(&PlatformSensorAndroid::NotifySensorError, this));
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

}  // namespace device
