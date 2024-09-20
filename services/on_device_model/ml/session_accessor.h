// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_SESSION_ACCESSOR_H_
#define SERVICES_ON_DEVICE_MODEL_ML_SESSION_ACCESSOR_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace ml {

// Allows for safely accessing ChromeMLSession on a task runner. ChromeMLSession
// may make blocking calls, so it can't be used on the main thread.
class COMPONENT_EXPORT(ON_DEVICE_MODEL_ML) SessionAccessor {
 public:
  using Ptr = std::unique_ptr<SessionAccessor, base::OnTaskRunnerDeleter>;

  static Ptr Create(const ChromeML& chrome_ml,
                    scoped_refptr<base::SequencedTaskRunner> task_runner,
                    ChromeMLModel model,
                    on_device_model::AdaptationAssets adaptation_assets =
                        on_device_model::AdaptationAssets());

  ~SessionAccessor();

  // These methods forward to the relevant ChromeMLSession methods on the task
  // runner.
  Ptr Clone();
  ChromeMLCancelFn Execute(on_device_model::mojom::InputOptionsPtr input,
                           ChromeMLExecutionOutputFn output_fn,
                           ChromeMLContextSavedFn context_saved_fn);
  void Score(const std::string& text, ChromeMLScoreFn score_fn);
  void SizeInTokens(on_device_model::mojom::InputPtr input,
                    ChromeMLSizeInTokensFn size_in_tokens_fn);

 private:
  class Canceler;

  SessionAccessor(const ChromeML& chrome_ml,
                  scoped_refptr<base::SequencedTaskRunner> task_runner,
                  ChromeMLModel model);

  void CloneFrom(SessionAccessor* other);
  void CreateInternal(on_device_model::AdaptationAssets adaptation_assets);
  void ExecuteInternal(on_device_model::mojom::InputOptionsPtr input,
                       ChromeMLExecutionOutputFn output_fn,
                       ChromeMLContextSavedFn context_saved_fn,
                       scoped_refptr<Canceler> canceler);
  void ScoreInternal(const std::string& text, ChromeMLScoreFn score_fn);
  void SizeInTokensInternal(on_device_model::mojom::InputPtr input,
                            ChromeMLSizeInTokensFn size_in_tokens_fn);

  const raw_ref<const ChromeML> chrome_ml_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  ChromeMLModel model_;
  ChromeMLSession session_ = 0;
};

}  // namespace ml

#endif  // SERVICES_ON_DEVICE_MODEL_ML_SESSION_ACCESSOR_H_
