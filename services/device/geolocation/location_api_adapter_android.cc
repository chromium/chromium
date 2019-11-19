// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/location_api_adapter_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "services/device/geolocation/geolocation_jni_headers/LocationProviderAdapter_jni.h"
#include "services/device/geolocation/location_provider_android.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using device::LocationApiAdapterAndroid;

static void JNI_LocationProviderAdapter_NewLocationAvailable(
    JNIEnv* env,
    jdouble latitude,
    jdouble longitude,
    jdouble time_stamp,
    jboolean has_altitude,
    jdouble altitude,
    jboolean has_accuracy,
    jdouble accuracy,
    jboolean has_heading,
    jdouble heading,
    jboolean has_speed,
    jdouble speed) {
  LocationApiAdapterAndroid::OnNewLocationAvailable(
      latitude, longitude, time_stamp, has_altitude, altitude, has_accuracy,
      accuracy, has_heading, heading, has_speed, speed);
}

static void JNI_LocationProviderAdapter_NewErrorAvailable(
    JNIEnv* env,
    const JavaParamRef<jstring>& message) {
  LocationApiAdapterAndroid::OnNewErrorAvailable(env, message);
}

namespace device {

void LocationApiAdapterAndroid::Start(OnGeopositionCB on_geoposition_callback,
                                      bool high_accuracy) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(on_geoposition_callback);

  JNIEnv* env = AttachCurrentThread();

  if (!on_geoposition_callback_) {
    on_geoposition_callback_ = on_geoposition_callback;

    DCHECK(java_location_provider_adapter_.is_null());
    java_location_provider_adapter_.Reset(
        Java_LocationProviderAdapter_create(env));
  }

  // At this point we should have all our pre-conditions ready, and they'd only
  // change in Stop() which must be called on the same thread as here. We'll
  // start receiving notifications from java in the main thread looper until
  // Stop() is called.
  DCHECK(!java_location_provider_adapter_.is_null());
  Java_LocationProviderAdapter_start(env, java_location_provider_adapter_,
                                     high_accuracy);
}

void LocationApiAdapterAndroid::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!on_geoposition_callback_)
    return;

  on_geoposition_callback_.Reset();

  JNIEnv* env = AttachCurrentThread();
  Java_LocationProviderAdapter_stop(env, java_location_provider_adapter_);
  java_location_provider_adapter_.Reset();
}

// static
void LocationApiAdapterAndroid::OnNewLocationAvailable(double latitude,
                                                       double longitude,
                                                       double time_stamp,
                                                       bool has_altitude,
                                                       double altitude,
                                                       bool has_accuracy,
                                                       double accuracy,
                                                       bool has_heading,
                                                       double heading,
                                                       bool has_speed,
                                                       double speed) {
  mojom::Geoposition position;
  position.latitude = latitude;
  position.longitude = longitude;
  position.timestamp = base::Time::FromDoubleT(time_stamp);
  if (has_altitude)
    position.altitude = altitude;
  if (has_accuracy)
    position.accuracy = accuracy;
  if (has_heading)
    position.heading = heading;
  if (has_speed)
    position.speed = speed;

  LocationApiAdapterAndroid* self = GetInstance();
  self->task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LocationApiAdapterAndroid::NotifyNewGeoposition,
                     base::Unretained(self), position));
}

// static
void LocationApiAdapterAndroid::OnNewErrorAvailable(JNIEnv* env,
                                                    jstring message) {
  mojom::Geoposition position_error;
  position_error.error_code =
      mojom::Geoposition::ErrorCode::POSITION_UNAVAILABLE;
  position_error.error_message =
      base::android::ConvertJavaStringToUTF8(env, message);

  LocationApiAdapterAndroid* self = GetInstance();
  self->task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LocationApiAdapterAndroid::NotifyNewGeoposition,
                     base::Unretained(self), position_error));
}

// static
LocationApiAdapterAndroid* LocationApiAdapterAndroid::GetInstance() {
  return base::Singleton<LocationApiAdapterAndroid>::get();
}

LocationApiAdapterAndroid::LocationApiAdapterAndroid()
    : task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

LocationApiAdapterAndroid::~LocationApiAdapterAndroid() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void LocationApiAdapterAndroid::NotifyNewGeoposition(
    const mojom::Geoposition& geoposition) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (on_geoposition_callback_)
    on_geoposition_callback_.Run(geoposition);
}

}  // namespace device
