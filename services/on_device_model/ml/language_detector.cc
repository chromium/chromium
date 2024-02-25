// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/language_detector.h"

#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "components/translate/core/language_detection/language_detection_model.h"

namespace ml {

LanguageDetector::LanguageDetector(
    base::PassKey<LanguageDetector>,
    std::unique_ptr<translate::LanguageDetectionModel> model)
    : model_(std::move(model)) {}

LanguageDetector::~LanguageDetector() = default;

scoped_refptr<LanguageDetector> LanguageDetector::Create(
    base::File model_file) {
  auto model = std::make_unique<translate::LanguageDetectionModel>();
  model->UpdateWithFile(std::move(model_file));
  if (!model->IsAvailable()) {
    return nullptr;
  }
  return base::MakeRefCounted<LanguageDetector>(
      base::PassKey<LanguageDetector>(), std::move(model));
}

on_device_model::mojom::LanguageDetectionResultPtr
LanguageDetector::DetectLanguage(std::string_view text) {
  const auto prediction = model_->DetectLanguage(base::UTF8ToUTF16(text));
  return on_device_model::mojom::LanguageDetectionResult::New(
      prediction.language, prediction.reliability);
}

}  // namespace ml
