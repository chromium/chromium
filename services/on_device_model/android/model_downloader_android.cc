// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/android/model_downloader_android.h"

#include "base/android/jni_android.h"
#include "services/on_device_model/android/on_device_model_bridge.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "services/on_device_model/android/jni_headers/AiCoreModelDownloader_jni.h"

namespace on_device_model {

ModelDownloaderAndroid::ModelDownloaderAndroid(
    optimization_guide::proto::ModelExecutionFeature feature)
    : java_downloader_(OnDeviceModelBridge::CreateModelDownloader(feature)) {}

ModelDownloaderAndroid::~ModelDownloaderAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AiCoreModelDownloader_onNativeDestroyed(env, java_downloader_);
}

void ModelDownloaderAndroid::StartDownload(
    OnDownloadCompleteCallback on_download_complete_callback) {
  CHECK(!on_download_complete_callback_)
      << "StartDownload() can only be called once.";
  on_download_complete_callback_ = std::move(on_download_complete_callback);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AiCoreModelDownloader_startDownload(env, java_downloader_,
                                           reinterpret_cast<intptr_t>(this));
}

void ModelDownloaderAndroid::OnAvailable() {
  std::move(on_download_complete_callback_).Run(true);
}

void ModelDownloaderAndroid::OnUnavailable() {
  std::move(on_download_complete_callback_).Run(false);
}

void JNI_AiCoreModelDownloader_OnAvailable(JNIEnv* env,
                                           jlong model_downloader_android) {
  reinterpret_cast<ModelDownloaderAndroid*>(model_downloader_android)
      ->OnAvailable();
}

void JNI_AiCoreModelDownloader_OnUnavailable(JNIEnv* env,
                                             jlong model_downloader_android) {
  reinterpret_cast<ModelDownloaderAndroid*>(model_downloader_android)
      ->OnUnavailable();
}

}  // namespace on_device_model
