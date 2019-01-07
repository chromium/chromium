// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_EXECUTION_IMPL_MKL_DNN_H_
#define SERVICES_ML_EXECUTION_IMPL_MKL_DNN_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "mojo/public/cpp/system/buffer.h"
#include "services/ml/common.h"
#include "services/ml/public/interfaces/execution.mojom.h"
#include "third_party/mkl-dnn/include/mkldnn.h"

namespace ml {

class CompilationDelegateMklDnn;
struct CompiledModelMklDnn;

class ExecutionImplMklDnn : public mojom::Execution {
 public:
  ExecutionImplMklDnn(std::shared_ptr<CompiledModelMklDnn> compiled_model,
                      mojom::ExecutionInitParamsPtr params);
  ~ExecutionImplMklDnn() override;

  void StartCompute(StartComputeCallback callback) override;

 private:
  mojom::ExecutionInitParamsPtr params_;
  std::shared_ptr<CompiledModelMklDnn> compiled_model_;

  DISALLOW_COPY_AND_ASSIGN(ExecutionImplMklDnn);
};

}  // namespace ml

#endif  // SERVICES_ML_EXECUTION_IMPL_MKL_DNN_H_