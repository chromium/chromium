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
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "build/build_config.h"
#include "components/language_detection/core/language_detection_provider.h"
#include "components/translate/core/language_detection/language_detection_model.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/ml/chrome_ml_api.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

using on_device_model::mojom::LoadModelResult;

namespace ml {

class TsModel final : public on_device_model::mojom::TextSafetyModel {
 public:
  ~TsModel() override;

  static std::unique_ptr<TsModel> Create(
      const ChromeML& chrome_ml,
      on_device_model::mojom::TextSafetyModelParamsPtr params);

  // on_device_model::mojom::TextSafetyModel
  void ClassifyTextSafety(const std::string& text,
                          ClassifyTextSafetyCallback callback) override;
  void DetectLanguage(const std::string& text,
                      DetectLanguageCallback callback) override;

  on_device_model::mojom::SafetyInfoPtr ClassifyTextSafety(
      const std::string& text);
  on_device_model::mojom::LanguageDetectionResultPtr DetectLanguage(
      std::string_view text);

 private:
  explicit TsModel(const ChromeML& chrome_ml);
  bool InitLanguageDetection(
      on_device_model::mojom::LanguageModelAssetsPtr assets);
  bool InitTextSafetyModel(
      on_device_model::mojom::TextSafetyModelAssetsPtr assets);

  const raw_ref<const ChromeML> chrome_ml_;
  ChromeMLTSModel model_ = 0;
  std::unique_ptr<translate::LanguageDetectionModel> language_detector_;
  base::MemoryMappedFile data_;
  base::MemoryMappedFile sp_model_;
};

TsModel::TsModel(const ChromeML& chrome_ml) : chrome_ml_(chrome_ml) {}

DISABLE_CFI_DLSYM
TsModel::~TsModel() {
  if (model_ != 0) {
    chrome_ml_->api().ts_api.DestroyModel(model_);
  }
}

// static
std::unique_ptr<TsModel> TsModel::Create(
    const ChromeML& chrome_ml,
    on_device_model::mojom::TextSafetyModelParamsPtr params) {
  auto ts_model = base::WrapUnique(new TsModel(chrome_ml));
  if (params->language_assets &&
      !ts_model->InitLanguageDetection(std::move(params->language_assets))) {
    return {};
  }
  if (params->ts_assets &&
      !ts_model->InitTextSafetyModel(std::move(params->ts_assets))) {
    return {};
  }
  return ts_model;
}

bool TsModel::InitLanguageDetection(
    on_device_model::mojom::LanguageModelAssetsPtr assets) {
  auto tflite_model =
      std::make_unique<language_detection::LanguageDetectionModel>();
  tflite_model->UpdateWithFile(std::move(assets->model));

  language_detector_ = std::make_unique<translate::LanguageDetectionModel>(
      std::move(tflite_model));
  return language_detector_->IsAvailable();
}

DISABLE_CFI_DLSYM
bool TsModel::InitTextSafetyModel(
    on_device_model::mojom::TextSafetyModelAssetsPtr assets) {
  if (!data_.Initialize(std::move(assets->data)) ||
      !sp_model_.Initialize(std::move(assets->sp_model))) {
    return false;
  }
  ChromeMLTSModelDescriptor desc = {
      .model = {.data = data_.data(), .size = data_.length()},
      .sp_model = {.data = sp_model_.data(), .size = sp_model_.length()},
  };
  model_ = chrome_ml_->api().ts_api.CreateModel(&desc);
  return bool(model_);
}

void TsModel::ClassifyTextSafety(const std::string& text,
                                 ClassifyTextSafetyCallback callback) {
  std::move(callback).Run(ClassifyTextSafety(text));
}
void TsModel::DetectLanguage(const std::string& text,
                             DetectLanguageCallback callback) {
  std::move(callback).Run(DetectLanguage(text));
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

TsHolder::TsHolder(raw_ref<const ChromeML> chrome_ml) : chrome_ml_(chrome_ml) {}
TsHolder::~TsHolder() = default;

// static
base::SequenceBound<TsHolder> TsHolder::Create(
    raw_ref<const ChromeML> chrome_ml) {
  return base::SequenceBound<TsHolder>(
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}),
      chrome_ml);
}

void TsHolder::Reset(
    on_device_model::mojom::TextSafetyModelParamsPtr params,
    mojo::PendingReceiver<on_device_model::mojom::TextSafetyModel> model) {
  model_.Clear();
  auto impl = TsModel::Create(*chrome_ml_, std::move(params));
  if (impl) {
    model_.Add(std::move(impl), std::move(model));
  }
}

}  // namespace ml
