// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ANDROID_MODEL_DOWNLOADER_ANDROID_H_
#define SERVICES_ON_DEVICE_MODEL_ANDROID_MODEL_DOWNLOADER_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/optimization_guide/proto/model_execution.pb.h"

namespace on_device_model {

// This class is used to download models on Android. The Java counterpart will
// be created when this object is created. One object should only handle one
// download request (i.e. call StartDownload() only once).
class ModelDownloaderAndroid {
 public:
  // Specification of the base model.
  struct BaseModelSpec {
    std::string name;
    std::string version;
  };

  // The reason for a download failure.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.on_device_model
  enum class DownloadFailureReason {
    kUnknownError = 0,
    kApiNotAvailable = 1,
  };

  using OnDownloadCompleteCallback = base::OnceCallback<void(
      base::expected<BaseModelSpec, DownloadFailureReason>)>;

  explicit ModelDownloaderAndroid(
      optimization_guide::proto::ModelExecutionFeature feature);
  ~ModelDownloaderAndroid();

  // Starts downloading the model for this feature.
  // `on_download_complete_callback` will be called either when the model is
  // available or when the download fails.
  void StartDownload(OnDownloadCompleteCallback on_download_complete_callback);

  // Methods called from Java.
  void OnAvailable(const std::string& base_model_name,
                   const std::string& base_model_version);
  void OnUnavailable(DownloadFailureReason failure_reason);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_downloader_;
  OnDownloadCompleteCallback on_download_complete_callback_;
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_ANDROID_MODEL_DOWNLOADER_ANDROID_H_
