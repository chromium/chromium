// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_EXECUTION_IMPL_NN_H_
#define SERVICES_ML_EXECUTION_IMPL_NN_H_

#include <vector>

#include "base/macros.h"
#include "services/ml/common.h"
#include "services/ml/compilation_impl_nn.h"
#include "services/ml/model_impl_nn.h"
#include "services/ml/public/mojom/execution.mojom.h"

#if defined(OS_ANDROID)
#ifdef __ANDROID_API__
#undef __ANDROID_API__
#define __ANDROID_API__ 27
#include "android/NeuralNetworks.h"
#undef __ANDROID_API__
#endif
#else
#include "third_party/ienn/include/ie_nn_c_api.h"
#endif

namespace ml {

class ExecutionImplNN : public mojom::Execution {
 public:
  ExecutionImplNN(const CompilationImplNN*,
                       mojo::ScopedSharedBufferHandle);
  ~ExecutionImplNN() override;

  void StartCompute(StartComputeCallback callback) override;

 private:
  std::vector<Operand> operands_;
  std::vector<Operation> operations_;
  std::vector<uint32_t> inputs_;
  std::vector<uint32_t> outputs_;

  std::vector<std::unique_ptr<OperandInfo>> inputs_info_;
  std::vector<std::unique_ptr<OperandInfo>> outputs_info_;
  mojo::ScopedSharedBufferHandle memory_;

#if defined(OS_LINUX) || defined(OS_WIN)
  ie_compilation_t* ie_compilation_;
  ie_execution_t* ie_execution_;
#else
  ANeuralNetworksCompilation* nn_compilation_;
#endif
  DISALLOW_COPY_AND_ASSIGN(ExecutionImplNN);
};

}  // namespace ml

#endif  // SERVICES_ML_EXECUTION_IMPL_NN_H_