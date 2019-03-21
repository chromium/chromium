// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_COMPILATION_IMPL_ANDROID_H_
#define SERVICES_ML_COMPILATION_IMPL_ANDROID_H_

#include <vector>

#include "base/macros.h"
#include "services/ml/common.h"
#include "services/ml/execution_impl_android.h"
#include "services/ml/model_impl_android.h"
#include "services/ml/public/mojom/compilation.mojom.h"

#ifdef __ANDROID_API__
#undef __ANDROID_API__
#define __ANDROID_API__ 27
#include "android/NeuralNetworks.h"
#undef __ANDROID_API__
#endif

namespace ml {

class CompilationImplAndroid : public mojom::Compilation {
 public:
  explicit CompilationImplAndroid(const ModelImplAndroid*);
  ~CompilationImplAndroid() override;

  void Finish(int32_t preference, FinishCallback callback) override;
  void CreateExecution(CreateExecutionCallback callback) override;

 private:
  friend class ExecutionImplAndroid;
  std::vector<Operand> operands_;
  std::vector<Operation> operations_;
  std::vector<uint32_t> inputs_;
  std::vector<uint32_t> outputs_;

  ANeuralNetworksCompilation* nn_compilation_;

  DISALLOW_COPY_AND_ASSIGN(CompilationImplAndroid);
};

}  // namespace ml

#endif  // SERVICES_ML_COMPILATION_IMPL_ANDROID_H_
