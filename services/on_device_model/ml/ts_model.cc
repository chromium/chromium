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
#include "base/memory/scoped_refptr.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/ml/chrome_ml_api.h"
#include "services/on_device_model/ml/language_detector.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

using on_device_model::mojom::LoadModelResult;

namespace ml {

TsModel::TsModel(const ChromeML& chrome_ml,
                 ChromeMLTSModel model,
                 scoped_refptr<LanguageDetector> language_detector)
    : chrome_ml_(chrome_ml),
      model_(model),
      language_detector_(std::move(language_detector)) {}

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
  base::MemoryMappedFile data;
  base::MemoryMappedFile sp_model;
  if (!assets->ts_data.IsValid() || !assets->ts_sp_model.IsValid() ||
      !data.Initialize(std::move(assets->ts_data)) ||
      !sp_model.Initialize(std::move(assets->ts_sp_model)) || !data.IsValid() ||
      !sp_model.IsValid()) {
    return nullptr;
  }

  ChromeMLTSModelDescriptor desc = {
      .model = {.data = data.data(), .size = data.length()},
      .sp_model = {.data = sp_model.data(), .size = sp_model.length()},
  };
  ChromeMLTSModel created = chrome_ml.api().ts_api.CreateModel(&desc);
  if (!created) {
    return nullptr;
  }
  return std::make_unique<TsModel>(chrome_ml, created,
                                   std::move(language_detector));
}

DISABLE_CFI_DLSYM
on_device_model::mojom::SafetyInfoPtr TsModel::ClassifyTextSafety(
    const std::string& text) {
  // First query the API to see how much storage we need for class scores.
  size_t num_scores = 0;
  if (chrome_ml_->api().ClassifyTextSafety(model_, text.c_str(), nullptr,
                                           &num_scores) !=
      ChromeMLSafetyResult::kInsufficientStorage) {
    return nullptr;
  }

  auto safety_info = on_device_model::mojom::SafetyInfo::New();
  safety_info->class_scores.resize(num_scores);
  const auto result = chrome_ml_->api().ClassifyTextSafety(
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
