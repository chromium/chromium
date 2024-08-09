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
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/ml/chrome_ml_api.h"
#include "services/on_device_model/ml/language_detector.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

using on_device_model::mojom::LoadModelResult;

namespace ml {

TsModel::TsModel(const ChromeML& chrome_ml,
                 scoped_refptr<LanguageDetector> language_detector)
    : chrome_ml_(chrome_ml), language_detector_(std::move(language_detector)) {}

DISABLE_CFI_DLSYM
TsModel::~TsModel() {
  if (model_ != 0) {
    chrome_ml_->api().ts_api.DestroyModel(model_);
  }
}

// static
DISABLE_CFI_DLSYM
std::unique_ptr<TsModel> TsModel::Create(
    const ChromeML& chrome_ml,
    on_device_model::mojom::ModelAssetsPtr assets,
    scoped_refptr<LanguageDetector> language_detector) {
  auto result =
      base::WrapUnique(new TsModel(chrome_ml, std::move(language_detector)));

  if (!assets->ts_data.IsValid() || !assets->ts_sp_model.IsValid() ||
      !result->data_.Initialize(std::move(assets->ts_data)) ||
      !result->sp_model_.Initialize(std::move(assets->ts_sp_model)) ||
      !result->data_.IsValid() || !result->sp_model_.IsValid()) {
    return nullptr;
  }

  ChromeMLTSModelDescriptor desc = {
      .model = {.data = result->data_.data(), .size = result->data_.length()},
      .sp_model = {.data = result->sp_model_.data(),
                   .size = result->sp_model_.length()},
  };
  result->model_ = chrome_ml.api().ts_api.CreateModel(&desc);
  if (!result->model_) {
    return nullptr;
  }
  return result;
}

DISABLE_CFI_DLSYM
on_device_model::mojom::SafetyInfoPtr TsModel::ClassifyTextSafety(
    const std::string& text) {
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
    safety_info->language = language_detector_->DetectLanguage(text);
  }
  return safety_info;
}

}  // namespace ml
