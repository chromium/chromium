// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/android/model_downloader_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/types/expected.h"
#include "services/on_device_model/android/on_device_model_bridge.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "services/on_device_model/android/jni_headers/AiCoreModelDownloaderWrapper_jni.h"

namespace on_device_model {

ModelDownloaderAndroid::ModelDownloaderAndroid(
    optimization_guide::proto::ModelExecutionFeature feature)
    : java_downloader_(OnDeviceModelBridge::CreateModelDownloader(feature)) {}

ModelDownloaderAndroid::~ModelDownloaderAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AiCoreModelDownloaderWrapper_onNativeDestroyed(env, java_downloader_);
}

void ModelDownloaderAndroid::StartDownload(
    OnDownloadCompleteCallback on_download_complete_callback) {
  CHECK(!on_download_complete_callback_)
      << "StartDownload() can only be called once.";
  on_download_complete_callback_ = std::move(on_download_complete_callback);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AiCoreModelDownloaderWrapper_startDownload(
      env, java_downloader_, reinterpret_cast<intptr_t>(this));
}

void ModelDownloaderAndroid::OnAvailable(
    const std::string& base_model_name,
    const std::string& base_model_version) {
  std::move(on_download_complete_callback_)
      .Run(BaseModelSpec{.name = base_model_name,
                         .version = base_model_version});
}

void ModelDownloaderAndroid::OnUnavailable(
    DownloadFailureReason failure_reason) {
  std::move(on_download_complete_callback_)
      .Run(base::unexpected(failure_reason));
}

void JNI_AiCoreModelDownloaderWrapper_OnAvailable(
    JNIEnv* env,
    jlong model_downloader_android,
    const jni_zero::JavaParamRef<jstring>& j_name,
    const jni_zero::JavaParamRef<jstring>& j_version) {
  reinterpret_cast<ModelDownloaderAndroid*>(model_downloader_android)
      ->OnAvailable(base::android::ConvertJavaStringToUTF8(env, j_name),
                    base::android::ConvertJavaStringToUTF8(env, j_version));
}

void JNI_AiCoreModelDownloaderWrapper_OnUnavailable(
    JNIEnv* env,
    jlong model_downloader_android,
    jint j_reason) {
  reinterpret_cast<ModelDownloaderAndroid*>(model_downloader_android)
      ->OnUnavailable(
          static_cast<ModelDownloaderAndroid::DownloadFailureReason>(j_reason));
}

}  // namespace on_device_model
