// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_VR)
#include "device/vr/public/cpp/features.h"
#endif

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/webxr/android/features_jni/WebXrAndroidFeatureMap_jni.h"

namespace webxr {
static jboolean JNI_WebXrAndroidFeatureMap_IsOpenXrEnabled(JNIEnv* env) {
#if BUILDFLAG(ENABLE_OPENXR)
  return device::features::IsOpenXrEnabled();
#else
  return false;
#endif
}

static jboolean JNI_WebXrAndroidFeatureMap_IsHandTrackingEnabled(JNIEnv* env) {
#if BUILDFLAG(ENABLE_OPENXR)
  return device::features::IsHandTrackingEnabled();
#else
  return false;
#endif
}

}  // namespace webxr
