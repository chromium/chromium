// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_EXECUTION_IMPL_WIN_H_
#define SERVICES_ML_EXECUTION_IMPL_WIN_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "mojo/public/cpp/system/buffer.h"
#include "services/ml/common.h"
#include "services/ml/public/interfaces/execution.mojom.h"
#include "third_party/clDNN/api/C/cldnn.h"

namespace ml {

class CompilationImplWin;

class ExecutionImplWin : public mojom::Execution {
 public:
  ExecutionImplWin(const CompilationImplWin*, mojo::ScopedSharedBufferHandle);
  ~ExecutionImplWin() override;

  void StartCompute(StartComputeCallback callback) override;

 private:
  std::vector<Operand> operands_;
  std::vector<Operation> operations_;
  std::vector<uint32_t> inputs_;
  std::vector<uint32_t> outputs_;

  std::vector<std::unique_ptr<OperandInfo>> inputs_info_;
  std::vector<std::unique_ptr<OperandInfo>> outputs_info_;
  mojo::ScopedSharedBufferHandle memory_;

  std::vector<cldnn_memory> input_memories_;
  cldnn_network network_;

  DISALLOW_COPY_AND_ASSIGN(ExecutionImplWin);
};

}  // namespace ml

#endif  // SERVICES_ML_EXECUTION_IMPL_WIN_H_