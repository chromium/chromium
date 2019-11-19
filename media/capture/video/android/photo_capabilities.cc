// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/android/photo_capabilities.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "media/capture/video/android/capture_jni_headers/PhotoCapabilities_jni.h"

using base::android::AttachCurrentThread;

namespace media {

PhotoCapabilities::PhotoCapabilities(
    base::android::ScopedJavaLocalRef<jobject> object)
    : object_(object) {}

PhotoCapabilities::~PhotoCapabilities() {}

bool PhotoCapabilities::getBool(PhotoCapabilityBool capability) const {
  DCHECK(!object_.is_null());
  DCHECK(capability != PhotoCapabilityBool::NUM_ENTRIES);
  return Java_PhotoCapabilities_getBool(
      AttachCurrentThread(), object_,
      JniIntWrapper(static_cast<int>(capability)));
}

double PhotoCapabilities::getDouble(PhotoCapabilityDouble capability) const {
  DCHECK(!object_.is_null());
  DCHECK(capability != PhotoCapabilityDouble::NUM_ENTRIES);
  return Java_PhotoCapabilities_getDouble(
      AttachCurrentThread(), object_,
      JniIntWrapper(static_cast<int>(capability)));
}

int PhotoCapabilities::getInt(PhotoCapabilityInt capability) const {
  DCHECK(!object_.is_null());
  DCHECK(capability != PhotoCapabilityInt::NUM_ENTRIES);
  return Java_PhotoCapabilities_getInt(
      AttachCurrentThread(), object_,
      JniIntWrapper(static_cast<int>(capability)));
}

std::vector<PhotoCapabilities::AndroidFillLightMode>
PhotoCapabilities::getFillLightModeArray() const {
  DCHECK(!object_.is_null());

  JNIEnv* env = AttachCurrentThread();

  std::vector<AndroidFillLightMode> modes;
  static_assert(
      std::is_same<int,
                   std::underlying_type<AndroidFillLightMode>::type>::value,
      "AndroidFillLightMode underlying type should be int");

  base::android::ScopedJavaLocalRef<jintArray> jni_modes =
      Java_PhotoCapabilities_getFillLightModeArray(env, object_);
  if (jni_modes.obj()) {
    base::android::JavaIntArrayToIntVector(
        env, jni_modes, reinterpret_cast<std::vector<int>*>(&modes));
  }
  return modes;
}

PhotoCapabilities::AndroidMeteringMode PhotoCapabilities::getMeteringMode(
    MeteringModeType type) const {
  DCHECK(!object_.is_null());
  DCHECK(type != MeteringModeType::NUM_ENTRIES);
  return static_cast<AndroidMeteringMode>(
      Java_PhotoCapabilities_getMeteringMode(
          AttachCurrentThread(), object_,
          JniIntWrapper(static_cast<int>(type))));
}

std::vector<PhotoCapabilities::AndroidMeteringMode>
PhotoCapabilities::getMeteringModeArray(MeteringModeType type) const {
  DCHECK(!object_.is_null());
  DCHECK(type != MeteringModeType::NUM_ENTRIES);

  JNIEnv* env = AttachCurrentThread();
  std::vector<PhotoCapabilities::AndroidMeteringMode> modes;
  static_assert(
      std::is_same<int,
                   std::underlying_type<
                       PhotoCapabilities::AndroidMeteringMode>::type>::value,
      "AndroidMeteringMode underlying type should be int");

  base::android::ScopedJavaLocalRef<jintArray> jni_modes =
      Java_PhotoCapabilities_getMeteringModeArray(
          env, object_, JniIntWrapper(static_cast<int>(type)));
  if (jni_modes.obj()) {
    base::android::JavaIntArrayToIntVector(
        env, jni_modes, reinterpret_cast<std::vector<int>*>(&modes));
  }
  return modes;
}

}  // namespace media
