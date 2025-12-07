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
base::android::ScopedJavaLocalRef<jobject> OnDeviceModelBridge::CreateSession(
    optimization_guide::proto::ModelExecutionFeature feature,
    on_device_model::mojom::SessionParamsPtr params) {
  CHECK_NE(feature, optimization_guide::proto::ModelExecutionFeature::
                        MODEL_EXECUTION_FEATURE_UNSPECIFIED)
      << "Feature is required to create a session.";
  CHECK(params) << "SessionParams is required to create a session.";
  JNIEnv* env = base::android::AttachCurrentThread();
  // There isn't a generic mojo utility for converting c++ mojo struct to java,
  // so disassemble the struct here and reassemble it in java.
  // Only passing the parameters that are supported on Android.
  return Java_OnDeviceModelBridge_createSession(env, feature, params->top_k,
                                                params->temperature);
}

// static
base::android::ScopedJavaLocalRef<jobject>
OnDeviceModelBridge::CreateModelDownloader(
    optimization_guide::proto::ModelExecutionFeature feature,
    on_device_model::mojom::DownloaderParamsPtr params) {
  CHECK_NE(feature, optimization_guide::proto::ModelExecutionFeature::
                        MODEL_EXECUTION_FEATURE_UNSPECIFIED)
      << "Feature is required to create a downloader.";
  CHECK(params) << "DownloaderParams is required to create a downloader.";
  JNIEnv* env = base::android::AttachCurrentThread();
  // There isn't a generic mojo utility for converting c++ mojo struct to java,
  // so disassemble the struct here and reassemble it in java.
  // Only passing the parameters that are supported on Android.
  return Java_OnDeviceModelBridge_createModelDownloader(
      env, feature, params->require_persistent_mode);
}

}  // namespace on_device_model

DEFINE_JNI(OnDeviceModelBridge)
