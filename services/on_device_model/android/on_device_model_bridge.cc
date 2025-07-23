// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/android/on_device_model_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "services/on_device_model/android/jni_headers/OnDeviceModelBridge_jni.h"

namespace on_device_model {

// static
base::android::ScopedJavaLocalRef<jobject>
OnDeviceModelBridge::CreateSession() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_OnDeviceModelBridge_createSession(env);
}

}  // namespace on_device_model
