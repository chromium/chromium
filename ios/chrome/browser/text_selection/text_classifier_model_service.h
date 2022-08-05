// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TEXT_SELECTION_TEXT_CLASSIFIER_MODEL_SERVICE_H_
#define IOS_CHROME_BROWSER_TEXT_SELECTION_TEXT_CLASSIFIER_MODEL_SERVICE_H_

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

// Service that manages models required to support smart text selection and
// entity detection in the browser.
class TextClassifierModelService
    : public KeyedService,
      public optimization_guide::OptimizationTargetModelObserver {
 public:
  explicit TextClassifierModelService(
      optimization_guide::OptimizationGuideModelProvider* opt_guide);
  ~TextClassifierModelService() override;

  const base::FilePath& GetModelPath() const;

  bool HasValidModelPath() const;

  // KeyedService implementation:
  void Shutdown() override;

  // optimization_guide::OptimizationTargetModelObserver implementation:
  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const optimization_guide::ModelInfo& model_info) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(InternalContextMenuProviderTest,
                           TCUsedWhenTCModelAvailable);

  SEQUENCE_CHECKER(sequence_checker_);
  base::FilePath model_path_;
  // Optimization Guide Service that provides model files for this service.
  // Optimization Guide Service is a BrowserContextKeyedServiceFactory and
  // should not be used after Shutdown.
  raw_ptr<optimization_guide::OptimizationGuideModelProvider> opt_guide_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  base::WeakPtrFactory<TextClassifierModelService> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_TEXT_SELECTION_TEXT_CLASSIFIER_MODEL_SERVICE_H_
