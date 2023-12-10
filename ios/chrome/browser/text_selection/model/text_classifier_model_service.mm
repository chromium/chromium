// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/text_selection/model/text_classifier_model_service.h"

#import <string>

#import "base/files/file_path.h"
#import "components/optimization_guide/core/optimization_guide_logger.h"
#import "components/optimization_guide/core/optimization_guide_model_provider.h"
#import "components/optimization_guide/proto/models.pb.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"

TextClassifierModelService::TextClassifierModelService(
    OptimizationGuideService* opt_guide_service)
    : opt_guide_service_(opt_guide_service) {
  DCHECK(opt_guide_service_);
  opt_guide_service_->AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OPTIMIZATION_TARGET_TEXT_CLASSIFIER,
      /*model_metadata=*/std::nullopt, this);
}

TextClassifierModelService::~TextClassifierModelService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

const base::FilePath& TextClassifierModelService::GetModelPath() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return model_path_;
}

bool TextClassifierModelService::HasValidModelPath() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !model_path_.empty();
}

void TextClassifierModelService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  opt_guide_service_->RemoveObserverForOptimizationTargetModel(
      optimization_guide::proto::OPTIMIZATION_TARGET_TEXT_CLASSIFIER, this);
}

void TextClassifierModelService::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (optimization_target !=
      optimization_guide::proto::OPTIMIZATION_TARGET_TEXT_CLASSIFIER) {
    return;
  }
  if (!model_info.has_value()) {
    model_path_ = base::FilePath();
    return;
  }
  model_path_ = model_info->GetModelFilePath();
}

bool TextClassifierModelService::ShouldRecordInternalsPageLog() const {
  return opt_guide_service_->GetOptimizationGuideLogger()
      ->ShouldEnableDebugLogs();
}

void TextClassifierModelService::RecordInternalsPageLog(
    const std::string& debug) {
  OPTIMIZATION_GUIDE_LOGGER(
      optimization_guide_common::mojom::LogSource::TEXT_CLASSIFIER,
      opt_guide_service_->GetOptimizationGuideLogger())
      << debug;
}
