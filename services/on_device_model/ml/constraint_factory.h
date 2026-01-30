// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_CONSTRAINT_FACTORY_H_
#define SERVICES_ON_DEVICE_MODEL_ML_CONSTRAINT_FACTORY_H_

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "services/on_device_model/ml/chrome_ml.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

struct LlgTokenizer;

namespace ml {

class ConstraintFactory {
 public:
  using Ptr = std::unique_ptr<ConstraintFactory, base::OnTaskRunnerDeleter>;

  ~ConstraintFactory();

  static Ptr Create(const ChromeML& chrome_ml,
                    scoped_refptr<base::SequencedTaskRunner> task_runner);

  ChromeMLConstraint CreateConstraint(
      ChromeMLSession session,
      ChromeMLModel model,
      const on_device_model::mojom::ResponseConstraint& response_constraint,
      const std::optional<std::string>& prefix);

 private:
  ConstraintFactory(const ChromeML& chrome_ml,
                    scoped_refptr<base::SequencedTaskRunner> task_runner);

  bool GetTokenizerParams(ChromeMLModel model,
                          ChromeMLSession session,
                          const ChromeMLGetTokenizerParamsFn& fn);

  const raw_ref<const ChromeML> chrome_ml_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Disabling dangling pointer detection because this uses C functions to
  // allocate/free from rust.
  raw_ptr<LlgTokenizer, DisableDanglingPtrDetection> tokenizer_ = nullptr;
};

}  // namespace ml

#endif  // SERVICES_ON_DEVICE_MODEL_ML_CONSTRAINT_FACTORY_H_
