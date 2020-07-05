// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_COMPILATION_IMPL_NN_H_
#define SERVICES_ML_COMPILATION_IMPL_NN_H_

#include <vector>

#include "base/macros.h"
#include "services/ml/common.h"
#include "services/ml/execution_impl_nn.h"
#include "services/ml/model_impl_nn.h"
#include "services/ml/public/mojom/compilation.mojom.h"

#if defined(OS_ANDROID)
#ifdef __ANDROID_API__
#undef __ANDROID_API__
#define __ANDROID_API__ 27
#include "android/NeuralNetworks.h"
#undef __ANDROID_API__
#endif
#endif

namespace ml {

class CompilationImplNN : public mojom::Compilation {
 public:
  explicit CompilationImplNN(const ModelImplNN*, mojom::ModelInfoPtr, mojo::ScopedSharedBufferMapping mapping);
  ~CompilationImplNN() override;

  void Finish(int32_t preference, FinishCallback callback) override;
  void CreateExecution(CreateExecutionCallback callback) override;

 private:
  friend class ExecutionImplNN;
  std::vector<Operand> operands_;
  std::vector<Operation> operations_;
  std::vector<uint32_t> inputs_;
  std::vector<uint32_t> outputs_;

  mojom::ModelInfoPtr model_info_;
  mojo::ScopedSharedBufferMapping mapping_;
#if defined(OS_ANDROID)
  ANeuralNetworksCompilation* nn_compilation_;
#else
  ie_compilation_t* ie_compilation_;
#endif

  DISALLOW_COPY_AND_ASSIGN(CompilationImplNN);
};

}  // namespace ml

#endif  // SERVICES_ML_COMPILATION_IMPL_NN_H_
