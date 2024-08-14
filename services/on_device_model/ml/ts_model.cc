// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/ts_model.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "components/language_detection/core/language_detection_provider.h"
#include "components/translate/core/language_detection/language_detection_model.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/ml/chrome_ml_api.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

using on_device_model::mojom::LoadModelResult;

namespace ml {

TsModel::TsModel(
    const ChromeML& chrome_ml,
    std::unique_ptr<translate::LanguageDetectionModel> language_detector)
    : chrome_ml_(chrome_ml), language_detector_(std::move(language_detector)) {}

DISABLE_CFI_DLSYM
TsModel::~TsModel() {
  if (model_ != 0) {
    chrome_ml_->api().ts_api.DestroyModel(model_);
  }
}

// static
base::SequenceBound<std::unique_ptr<TsModel>> TsModel::Create(
    const ChromeML& chrome_ml,
    on_device_model::mojom::ModelAssetsPtr ts_assets,
    base::File language_detection_file) {
  std::unique_ptr<translate::LanguageDetectionModel> language_detector;
  if (language_detection_file.IsValid()) {
    language_detector = std::make_unique<translate::LanguageDetectionModel>(
        &language_detection::GetLanguageDetectionModel());
    language_detector->UpdateWithFile(std::move(language_detection_file));
    if (!language_detector->IsAvailable()) {
      return {};
    }
  }

  auto ts_model =
      base::WrapUnique(new TsModel(chrome_ml, std::move(language_detector)));

  if (ts_assets &&
      (!ts_assets->ts_data.IsValid() || !ts_assets->ts_sp_model.IsValid() ||
       !ts_model->data_.Initialize(std::move(ts_assets->ts_data)) ||
       !ts_model->sp_model_.Initialize(std::move(ts_assets->ts_sp_model)) ||
       !ts_model->data_.IsValid() || !ts_model->sp_model_.IsValid())) {
    return {};
  }
  base::SequenceBound<std::unique_ptr<TsModel>> result(
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}),
      std::move(ts_model));
  if (ts_assets) {
    result.AsyncCall(&TsModel::InitTextSafetyModel);
  }
  return result;
}

DISABLE_CFI_DLSYM
void TsModel::InitTextSafetyModel() {
  ChromeMLTSModelDescriptor desc = {
      .model = {.data = data_.data(), .size = data_.length()},
      .sp_model = {.data = sp_model_.data(), .size = sp_model_.length()},
  };
  model_ = chrome_ml_->api().ts_api.CreateModel(&desc);
  // TODO: b/326240401 - This happens off the main thread so the error does not
  // get propagated. Refactor the loading code if we want to avoid crashing
  // here.
  CHECK(model_);
}

DISABLE_CFI_DLSYM
on_device_model::mojom::SafetyInfoPtr TsModel::ClassifyTextSafety(
    const std::string& text) {
  if (!model_) {
    return nullptr;
  }

  // First query the API to see how much storage we need for class scores.
  size_t num_scores = 0;
  if (chrome_ml_->api().ts_api.ClassifyTextSafety(model_, text.c_str(), nullptr,
                                                  &num_scores) !=
      ChromeMLSafetyResult::kInsufficientStorage) {
    return nullptr;
  }

  auto safety_info = on_device_model::mojom::SafetyInfo::New();
  safety_info->class_scores.resize(num_scores);
  const auto result = chrome_ml_->api().ts_api.ClassifyTextSafety(
      model_, text.c_str(), safety_info->class_scores.data(), &num_scores);
  if (result != ChromeMLSafetyResult::kOk) {
    return nullptr;
  }
  CHECK_EQ(num_scores, safety_info->class_scores.size());
  if (language_detector_) {
    safety_info->language = DetectLanguage(text);
  }
  return safety_info;
}

on_device_model::mojom::LanguageDetectionResultPtr TsModel::DetectLanguage(
    std::string_view text) {
  if (!language_detector_) {
    return nullptr;
  }
  const auto prediction =
      language_detector_->DetectLanguage(base::UTF8ToUTF16(text));
  return on_device_model::mojom::LanguageDetectionResult::New(
      prediction.language, prediction.score);
}

}  // namespace ml
