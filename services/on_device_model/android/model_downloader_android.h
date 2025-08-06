// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ANDROID_MODEL_DOWNLOADER_ANDROID_H_
#define SERVICES_ON_DEVICE_MODEL_ANDROID_MODEL_DOWNLOADER_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "components/optimization_guide/proto/model_execution.pb.h"

namespace on_device_model {

// This class is used to download models on Android. The Java counterpart will
// be created when this object is created. One object should only handle one
// download request (i.e. call StartDownload() only once).
class ModelDownloaderAndroid {
 public:
  // The bool indicates whether the download is successful.
  // TODO(crbug.com/crbug.com/425408635): Return base::expected instead.
  // On failure, return the failure reason. On success, return the base model
  // name and version.
  using OnDownloadCompleteCallback = base::OnceCallback<void(bool)>;

  explicit ModelDownloaderAndroid(
      optimization_guide::proto::ModelExecutionFeature feature);
  ~ModelDownloaderAndroid();

  // Starts downloading the model for this feature.
  // `on_download_complete_callback` will be called either when the model is
  // available or when the download fails.
  void StartDownload(OnDownloadCompleteCallback on_download_complete_callback);

  // Methods called from Java.
  // TODO(crbug.com/425408635): Return the base model name and version.
  void OnAvailable();
  // TODO(crbug.com/425408635): Return an error code.
  void OnUnavailable();

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_downloader_;
  OnDownloadCompleteCallback on_download_complete_callback_;
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_ANDROID_MODEL_DOWNLOADER_ANDROID_H_
