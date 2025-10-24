// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ANDROID_MODEL_DOWNLOADER_ANDROID_H_
#define SERVICES_ON_DEVICE_MODEL_ANDROID_MODEL_DOWNLOADER_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "services/on_device_model/android/downloader_params.mojom.h"
#include "services/on_device_model/android/sequence_checker_helper.h"

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
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.on_device_model
  enum class DownloadFailureReason {
    kUnknownError = 0,
    // The backend API is not constructed. This happens if this is an upstream
    // build.
    kApiNotAvailable = 1,
    // The backend is not able to find the feature ID. This can happen if AICore
    // doesn't enable the feature as part of experiments or device filters.
    kFeatureIsNull = 2,
    // An exception is thrown when getting the feature. This can happen if the
    // AICore APK is not installed on the device.
    kGetFeatureError = 3,
    // An exception is thrown when getting the status of the feature.
    kGetFeatureStatusError = 4,
    // The status of the feature is not available. This can happen if Chrome is
    // not allowlisted to call this feature, attestation verification has
    // failed, or the feature manifest file failed to be downloaded.
    kFeatureNotAvailable = 5,
    // A general exception is thrown when downloading the model.
    kDownloadGeneralError = 6,
    // There is no enough disk space when downloading the model.
    kDownloadNotEnoughDiskSpaceError = 7,
    // The feature is gated by persistent mode, but there is an error when
    // determining if persistent mode is enabled..
    kGetPersistentModeError = 8,
    // The feature is gated by persistent mode, but persistent mode is not
    // enabled.
    kPersistentModeNotEnabled = 9,
    kMaxValue = kPersistentModeNotEnabled,
  };

  using OnDownloadCompleteCallback = base::OnceCallback<void(
      base::expected<BaseModelSpec, DownloadFailureReason>)>;

  ModelDownloaderAndroid(
      optimization_guide::proto::ModelExecutionFeature feature,
      mojom::DownloaderParamsPtr params);
  ~ModelDownloaderAndroid();

  // Starts downloading the model for this feature.
  // `on_download_complete_callback` will be called either when the model is
  // available or when the download fails.
  void StartDownload(OnDownloadCompleteCallback on_download_complete_callback);

  // Methods called from Java (can be called on any thread).
  void OnAvailable(const std::string& base_model_name,
                   const std::string& base_model_version);
  void OnUnavailable(DownloadFailureReason failure_reason);

 private:
  void OnAvailableOnSequence(const std::string& base_model_name,
                             const std::string& base_model_version);
  void OnUnavailableOnSequence(DownloadFailureReason failure_reason);

  base::android::ScopedJavaGlobalRef<jobject> java_downloader_;
  OnDownloadCompleteCallback on_download_complete_callback_;

  // The feature for which this downloader was created.
  const optimization_guide::proto::ModelExecutionFeature feature_;

  SEQUENCE_CHECKER(sequence_checker_);
  SequenceCheckerHelper sequence_checker_helper_;

  // The weak pointer created on the main sequence.
  base::WeakPtr<ModelDownloaderAndroid> weak_ptr_;
  base::WeakPtrFactory<ModelDownloaderAndroid> weak_factory_{this};
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_ANDROID_MODEL_DOWNLOADER_ANDROID_H_
