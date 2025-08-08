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

  void VerifySessionParams(
      optimization_guide::proto::ModelExecutionFeature feature,
      int top_k,
      float temperature);
  void VerifyGenerateOptions(int max_output_tokens);

  void SetGenerateResult(BackendSessionImplAndroid::GenerateResult result);

  void SetCompleteAsync();
  void ResumeOnCompleteCallback();

  void TriggerDownloaderOnUnavailable(
      ModelDownloaderAndroid::DownloadFailureReason reason);
  void TriggerDownloaderOnAvailable(const std::string& name,
                                    const std::string& version);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_helper_;
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_ANDROID_ON_DEVICE_MODEL_BRIDGE_NATIVE_UNITTEST_HELPER_H_
