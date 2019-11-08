// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_EXECUTION_IMPL_DNNL_H_
#define SERVICES_ML_EXECUTION_IMPL_DNNL_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "mojo/public/cpp/system/buffer.h"
#include "services/ml/common.h"
#include "services/ml/compilation_delegate_dnnl.h"
#include "third_party/dnnl/include/dnnl.h"

namespace ml {

class CompilationDelegateDnnl;
struct CompiledModelDnnl;

class ExecutionImplDnnl : public mojom::Execution {
 public:
  ExecutionImplDnnl(std::shared_ptr<CompiledModelDnnl> compiled_model,
                    mojom::ExecutionInitParamsPtr params);
  ~ExecutionImplDnnl() override;

  void StartCompute(StartComputeCallback callback) override;

 private:
  int32_t DnnlExecuteNet(std::vector<dnnl_primitive_t>&, std::vector<args_t>&);
  int32_t DnnlExecuteCustomOperation(const OperationDnnl&);
  int32_t DnnlExecuteReshape(const OperationDnnl&);
  mojom::ExecutionInitParamsPtr params_;
  std::shared_ptr<CompiledModelDnnl> compiled_model_;

  DISALLOW_COPY_AND_ASSIGN(ExecutionImplDnnl);
};

}  // namespace ml

#endif  // SERVICES_ML_EXECUTION_IMPL_MKL_DNN_H_
