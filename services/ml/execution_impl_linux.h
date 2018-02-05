// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_EXECUTION_IMPL_H_
#define SERVICES_ML_EXECUTION_IMPL_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/public/interfaces/execution.mojom.h"
#include "services/ml/public/interfaces/constants.mojom.h"

#include "services/ml/common.h"
#include "services/ml/model_impl_linux.h"
#include "services/ml/compilation_impl_linux.h"

namespace ml {

class ExecutionImplLinux : public mojom::Execution {
 public:
  ExecutionImplLinux(CompilationImplLinux*, mojo::ScopedSharedBufferHandle);
  ~ExecutionImplLinux() override;

  void startCompute(startComputeCallback callback) override;

 private:
  std::vector<Operand> operands_;
  std::vector<Operation> operations_;
  std::vector<uint32_t> inputs_;
  std::vector<uint32_t> outputs_;

  std::vector<std::unique_ptr<OperandInfo>> inputs_info_;
  std::vector<std::unique_ptr<OperandInfo>> outputs_info_;
  mojo::ScopedSharedBufferHandle memory_;

  DISALLOW_COPY_AND_ASSIGN(ExecutionImplLinux);
};

}  // namespace  

#endif  // SERVICES_ML_EXECUTION_IMPL_H_