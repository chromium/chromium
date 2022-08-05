// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/text_selection/text_classifier_model_service.h"

#import <string>

#import "base/files/file_path.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "components/optimization_guide/core/optimization_guide_model_provider.h"
#import "components/optimization_guide/proto/models.pb.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TextClassifierModelService::TextClassifierModelService(
    optimization_guide::OptimizationGuideModelProvider* opt_guide)
    : opt_guide_(opt_guide) {
  DCHECK(opt_guide_);
  opt_guide_->AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OPTIMIZATION_TARGET_TEXT_CLASSIFIER,
      /*model_metadata=*/absl::nullopt, this);
  background_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
}

TextClassifierModelService::~TextClassifierModelService() {}

const base::FilePath& TextClassifierModelService::GetModelPath() const {
  return model_path_;
}

bool TextClassifierModelService::HasValidModelPath() const {
  return !model_path_.empty();
}

void TextClassifierModelService::Shutdown() {}

void TextClassifierModelService::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    const optimization_guide::ModelInfo& model_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (optimization_target !=
      optimization_guide::proto::OPTIMIZATION_TARGET_TEXT_CLASSIFIER) {
    return;
  }
  model_path_ = model_info.GetModelFilePath();
}
