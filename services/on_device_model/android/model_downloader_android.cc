// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/android/model_downloader_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "services/on_device_model/android/on_device_model_bridge.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "services/on_device_model/android/jni_headers/AiCoreModelDownloaderWrapper_jni.h"

namespace on_device_model {

namespace {

void LogModelDownloadResult(
    optimization_guide::proto::ModelExecutionFeature feature,
    base::expected<void, ModelDownloaderAndroid::DownloadFailureReason>
        failure_reason) {
  bool success = failure_reason.has_value();
  base::UmaHistogramBoolean("OnDeviceModel.Android.IsModelDownloadSuccessful",
                            success);
  base::UmaHistogramBoolean(
      base::StrCat(
          {"OnDeviceModel.Android.IsModelDownloadSuccessful.",
           optimization_guide::GetStringNameForModelExecutionFeature(feature)}),
      success);
  if (!success) {
    base::UmaHistogramEnumeration(
        "OnDeviceModel.Android.ModelDownloadFailureReason",
        failure_reason.error());
    base::UmaHistogramEnumeration(
        base::StrCat({"OnDeviceModel.Android.ModelDownloadFailureReason.",
                      optimization_guide::GetStringNameForModelExecutionFeature(
                          feature)}),
        failure_reason.error());
  }
}

}  // namespace

ModelDownloaderAndroid::ModelDownloaderAndroid(
    optimization_guide::proto::ModelExecutionFeature feature,
    mojom::DownloaderParamsPtr params)
    : java_downloader_(
          OnDeviceModelBridge::CreateModelDownloader(feature,
                                                     std::move(params))),
      feature_(feature) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

ModelDownloaderAndroid::~ModelDownloaderAndroid() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AiCoreModelDownloaderWrapper_onNativeDestroyed(env, java_downloader_);
}

void ModelDownloaderAndroid::StartDownload(
    OnDownloadCompleteCallback on_download_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  sequence_checker_helper_.PostTask(
      FROM_HERE,
      base::BindOnce(&ModelDownloaderAndroid::OnAvailableOnSequence, weak_ptr_,
                     base_model_name, base_model_version));
}

void ModelDownloaderAndroid::OnAvailableOnSequence(
    const std::string& base_model_name,
    const std::string& base_model_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LogModelDownloadResult(feature_, base::ok());
  std::move(on_download_complete_callback_)
      .Run(BaseModelSpec{.name = base_model_name,
                         .version = base_model_version});
}

void ModelDownloaderAndroid::OnUnavailable(
    DownloadFailureReason failure_reason) {
  sequence_checker_helper_.PostTask(
      FROM_HERE,
      base::BindOnce(&ModelDownloaderAndroid::OnUnavailableOnSequence,
                     weak_ptr_, failure_reason));
}

void ModelDownloaderAndroid::OnUnavailableOnSequence(
    DownloadFailureReason failure_reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LogModelDownloadResult(feature_, base::unexpected(failure_reason));
  std::move(on_download_complete_callback_)
      .Run(base::unexpected(failure_reason));
}

static void JNI_AiCoreModelDownloaderWrapper_OnAvailable(
    JNIEnv* env,
    jlong model_downloader_android,
    const jni_zero::JavaParamRef<jstring>& j_name,
    const jni_zero::JavaParamRef<jstring>& j_version) {
  reinterpret_cast<ModelDownloaderAndroid*>(model_downloader_android)
      ->OnAvailable(base::android::ConvertJavaStringToUTF8(env, j_name),
                    base::android::ConvertJavaStringToUTF8(env, j_version));
}

static void JNI_AiCoreModelDownloaderWrapper_OnUnavailable(
    JNIEnv* env,
    jlong model_downloader_android,
    jint j_reason) {
  reinterpret_cast<ModelDownloaderAndroid*>(model_downloader_android)
      ->OnUnavailable(
          static_cast<ModelDownloaderAndroid::DownloadFailureReason>(j_reason));
}

}  // namespace on_device_model

DEFINE_JNI(AiCoreModelDownloaderWrapper)
