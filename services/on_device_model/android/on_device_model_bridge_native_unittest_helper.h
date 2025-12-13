// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ANDROID_ON_DEVICE_MODEL_BRIDGE_NATIVE_UNITTEST_HELPER_H_
#define SERVICES_ON_DEVICE_MODEL_ANDROID_ON_DEVICE_MODEL_BRIDGE_NATIVE_UNITTEST_HELPER_H_

#include "base/android/scoped_java_ref.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "services/on_device_model/android/backend_session_impl_android.h"
#include "services/on_device_model/android/model_downloader_android.h"

namespace on_device_model {

// A test helper for calling the Java test helper. This allows centralizing the
// JNI calls and avoiding `-Wunused-function` warnings in multiple places.
class OnDeviceModelBridgeNativeUnitTestHelper {
 public:
  OnDeviceModelBridgeNativeUnitTestHelper();
  ~OnDeviceModelBridgeNativeUnitTestHelper();

  void SetMockAiCoreFactory();

  // `index` is the index of the session backend in the list of session backends
  // created by the mock AiCoreFactory. The list is in the order of the calls to
  // `AiCoreFactory.createSessionBackend()`.
  void VerifySessionParams(
      int index,
      optimization_guide::proto::ModelExecutionFeature feature,
      int top_k,
      float temperature);
  void VerifyGenerateOptions(int index, int max_output_tokens);

  void SetGenerateResult(BackendSessionImplAndroid::GenerateResult result);

  void SetCompleteAsync();
  void SetCallbackOnDifferentThread();
  void ResumeOnCompleteCallback();
  void SetDownloaderCallbackOnDifferentThread();

  void VerifyDownloaderParams(
      optimization_guide::proto::ModelExecutionFeature feature,
      bool require_persistent_mode);

  void TriggerDownloaderOnUnavailable(
      ModelDownloaderAndroid::DownloadFailureReason reason);
  void TriggerDownloaderOnAvailable(const std::string& name,
                                    const std::string& version);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_helper_;
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_ANDROID_ON_DEVICE_MODEL_BRIDGE_NATIVE_UNITTEST_HELPER_H_
