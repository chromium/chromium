// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_android.h"

#include "base/bind.h"
#include "services/device/generic_sensor/jni_headers/PlatformSensor_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;

namespace device {

PlatformSensorAndroid::PlatformSensorAndroid(
    mojom::SensorType type,
    SensorReadingSharedBuffer* reading_buffer,
    PlatformSensorProvider* provider,
    const JavaRef<jobject>& java_sensor)
    : PlatformSensor(type, reading_buffer, provider) {
  JNIEnv* env = AttachCurrentThread();
  j_object_.Reset(java_sensor);

  Java_PlatformSensor_initPlatformSensorAndroid(env, j_object_,
                                                reinterpret_cast<jlong>(this));
}

PlatformSensorAndroid::~PlatformSensorAndroid() {
  JNIEnv* env = AttachCurrentThread();
  Java_PlatformSensor_sensorDestroyed(env, j_object_);
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
  task_runner_->PostTask(
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
