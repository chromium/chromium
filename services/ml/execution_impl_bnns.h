// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_EXECUTION_IMPL_BNNS_H_
#define SERVICES_ML_EXECUTION_IMPL_BNNS_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "mojo/public/cpp/system/buffer.h"
#include "base/memory/scoped_refptr.h"
#include "services/ml/common.h"
#include "services/ml/compilation_delegate_bnns.h"
#include "services/ml/public/mojom/execution.mojom.h"

namespace ml {

class CompilationDelegateBnns;
class CompiledModelBnns;

class ExecutionImplBnns : public mojom::Execution {
 public:
  ExecutionImplBnns(scoped_refptr<CompiledModelBnns> compiled_model,
                    mojom::ExecutionInitParamsPtr params);
  ~ExecutionImplBnns() override;

  void StartCompute(StartComputeCallback callback) override;

 private:
  mojom::ExecutionInitParamsPtr params_;

  std::vector<std::unique_ptr<OperandInfo>> inputs_info_;
  std::vector<std::unique_ptr<OperandInfo>> outputs_info_;
  std::map<size_t, float*> bnns_operands_memory_map_;

  // std::shared_ptr<CompiledModelBnns> compiled_model_;
  scoped_refptr<CompiledModelBnns> compiled_model_;
  bool PrepareBnnsOperandsMemory();

  DISALLOW_COPY_AND_ASSIGN(ExecutionImplBnns);
};

}  // namespace ml

#endif  // SERVICES_ML_EXECUTION_IMPL_BNNS_H_