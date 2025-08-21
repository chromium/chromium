// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/safety/bert_safety_model.h"

#include "components/translate/core/language_detection/language_detection_model.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"
#include "services/on_device_model/safety/bert_safety_op_resolver.h"
#include "services/on_device_model/safety/safety_util.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/text/nlclassifier/nl_classifier.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/text/proto/nl_classifier_options.pb.h"

namespace on_device_model {

BertSafetyModel::BertSafetyModel() = default;
BertSafetyModel::~BertSafetyModel() = default;

std::unique_ptr<BertSafetyModel> BertSafetyModel::Create(
    mojom::TextSafetyModelParamsPtr params) {
  auto bs_model = base::WrapUnique(new BertSafetyModel());
  if (params->language_assets &&
      !bs_model->InitLanguageDetection(std::move(params->language_assets))) {
    return nullptr;
  }
  if (params->safety_assets && !bs_model->InitTextSafetyModel(std::move(
                                   params->safety_assets->get_bs_assets()))) {
    return nullptr;
  }
  return bs_model;
}

bool BertSafetyModel::InitTextSafetyModel(
    mojom::BertSafetyModelAssetsPtr assets) {
  tflite::task::text::NLClassifierOptions options;
  auto* mutable_file_descriptor_meta = options.mutable_base_options()
                                           ->mutable_model_file()
                                           ->mutable_file_descriptor_meta();
  base::File& model_file = assets->model;
#if BUILDFLAG(IS_WIN)
  mutable_file_descriptor_meta->set_handle(
      reinterpret_cast<uint64_t>(model_file.GetPlatformFile()));
#else
  mutable_file_descriptor_meta->set_fd(model_file.GetPlatformFile());
#endif

  options.mutable_base_options()
      ->mutable_compute_settings()
      ->mutable_tflite_settings()
      ->mutable_cpu_settings()
      ->set_num_threads(-1);  // Use default num_threads.

  auto maybe_nl_classifier =
      tflite::task::text::nlclassifier::NLClassifier::CreateFromOptions(
          std::move(options), std::make_unique<BertSafetyOpResolver>());
  if (maybe_nl_classifier.ok()) {
    loaded_bert_model_ = std::move(maybe_nl_classifier.value());
  }

  return static_cast<bool>(loaded_bert_model_);
}
bool BertSafetyModel::InitLanguageDetection(
    mojom::LanguageModelAssetsPtr assets) {
  auto tflite_model =
      std::make_unique<language_detection::LanguageDetectionModel>();
  tflite_model->UpdateWithFile(std::move(assets->model));

  language_detector_ = std::make_unique<translate::LanguageDetectionModel>(
      std::move(tflite_model));
  return language_detector_->IsAvailable();
}

void BertSafetyModel::StartSession(
    mojo::PendingReceiver<mojom::TextSafetySession> session) {
  sessions_.Add(this, std::move(session));
}

void BertSafetyModel::ClassifyTextSafety(const std::string& text,
                                         ClassifyTextSafetyCallback callback) {
  std::move(callback).Run(ClassifyTextSafety(text));
}

void BertSafetyModel::DetectLanguage(const std::string& text,
                                     DetectLanguageCallback callback) {
  std::move(callback).Run(DetectLanguage(text));
}

void BertSafetyModel::Clone(
    mojo::PendingReceiver<mojom::TextSafetySession> session) {
  StartSession(std::move(session));
}

mojom::SafetyInfoPtr BertSafetyModel::ClassifyTextSafety(
    const std::string& text) {
  if (!loaded_bert_model_) {
    return nullptr;
  }

  auto status_or_result =
      static_cast<tflite::task::text::nlclassifier::NLClassifier*>(
          loaded_bert_model_.get())
          ->ClassifyText(text);
  if (absl::IsCancelled(status_or_result.status()) || !status_or_result.ok()) {
    return nullptr;
  }

  auto safety_info = mojom::SafetyInfo::New();
  safety_info->class_scores.reserve(status_or_result->size());

  for (auto& category : *status_or_result) {
    safety_info->class_scores.push_back(category.score);
  }

  if (language_detector_) {
    safety_info->language = DetectLanguage(text);
  }
  return safety_info;
}

mojom::LanguageDetectionResultPtr BertSafetyModel::DetectLanguage(
    std::string_view text) {
  if (!language_detector_) {
    return nullptr;
  }
  language_detection::Prediction prediction =
      PredictLanguage(language_detector_->tflite_model(), text);
  return mojom::LanguageDetectionResult::New(prediction.language,
                                             prediction.score);
}

}  // namespace on_device_model
