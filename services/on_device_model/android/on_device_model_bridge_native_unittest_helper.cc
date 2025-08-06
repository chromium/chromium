// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/android/on_device_model_bridge_native_unittest_helper.h"

#include "base/android/jni_android.h"
#include "services/on_device_model/android/native_j_unittests_jni_headers/OnDeviceModelBridgeNativeUnitTestHelper_jni.h"

namespace on_device_model {

OnDeviceModelBridgeNativeUnitTestHelper::
    OnDeviceModelBridgeNativeUnitTestHelper() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_helper_ = Java_OnDeviceModelBridgeNativeUnitTestHelper_create(env);
}

OnDeviceModelBridgeNativeUnitTestHelper::
    ~OnDeviceModelBridgeNativeUnitTestHelper() = default;

void OnDeviceModelBridgeNativeUnitTestHelper::SetMockAiCoreFactory() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_OnDeviceModelBridgeNativeUnitTestHelper_setMockAiCoreFactory(
      env, java_helper_);
}

void OnDeviceModelBridgeNativeUnitTestHelper::VerifySessionParams(
    optimization_guide::proto::ModelExecutionFeature feature,
    int top_k,
    float temperature) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_OnDeviceModelBridgeNativeUnitTestHelper_verifySessionParams(
      env, java_helper_, static_cast<int>(feature), top_k, temperature);
}

void OnDeviceModelBridgeNativeUnitTestHelper::VerifyGenerateOptions(
    int max_output_tokens) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_OnDeviceModelBridgeNativeUnitTestHelper_verifyGenerateOptions(
      env, java_helper_, max_output_tokens);
}

void OnDeviceModelBridgeNativeUnitTestHelper::SetGenerateResult(
    BackendSessionImplAndroid::GenerateResult result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_OnDeviceModelBridgeNativeUnitTestHelper_setGenerateResult(
      env, java_helper_, static_cast<int>(result));
}

void OnDeviceModelBridgeNativeUnitTestHelper::SetCompleteAsync() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_OnDeviceModelBridgeNativeUnitTestHelper_setCompleteAsync(env,
                                                                java_helper_);
}

void OnDeviceModelBridgeNativeUnitTestHelper::ResumeOnCompleteCallback() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_OnDeviceModelBridgeNativeUnitTestHelper_resumeOnCompleteCallback(
      env, java_helper_);
}

void OnDeviceModelBridgeNativeUnitTestHelper::TriggerDownloaderOnUnavailable() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_OnDeviceModelBridgeNativeUnitTestHelper_triggerDownloaderOnUnavailable(
      env, java_helper_);
}

void OnDeviceModelBridgeNativeUnitTestHelper::TriggerDownloaderOnAvailable() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_OnDeviceModelBridgeNativeUnitTestHelper_triggerDownloaderOnAvailable(
      env, java_helper_);
}

}  // namespace on_device_model
