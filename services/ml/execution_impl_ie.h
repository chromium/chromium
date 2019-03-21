// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_EXECUTION_IMPL_IE_H_
#define SERVICES_ML_EXECUTION_IMPL_IE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "mojo/public/cpp/system/buffer.h"
#include "services/ml/common.h"
#include "services/ml/public/mojom/execution.mojom.h"

namespace InferenceEngine {
class InferRequest;
class ExecutableNetwork;
class InferencePlugin;
}  // namespace InferenceEngine

namespace ml {

class CompilationDelegateIe;

class ExecutionImplIe : public mojom::Execution {
 public:
  ExecutionImplIe(const CompilationDelegateIe*,
                  mojom::ExecutionInitParamsPtr params);
  ~ExecutionImplIe() override;

  int32_t Init(int32_t preference);

  void StartCompute(StartComputeCallback callback) override;

 private:
  bool initialized_;

  const CompilationDelegateIe* compilation_;
  mojom::ExecutionInitParamsPtr params_;

  std::unique_ptr<InferenceEngine::InferRequest> infer_request_;
  std::unique_ptr<InferenceEngine::InferencePlugin> plugin_;
  std::unique_ptr<InferenceEngine::ExecutableNetwork> execution_;

  DISALLOW_COPY_AND_ASSIGN(ExecutionImplIe);
};

}  // namespace ml

#endif  // SERVICES_ML_EXECUTION_IMPL_IE_H_