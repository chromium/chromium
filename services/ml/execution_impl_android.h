// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_EXECUTION_IMPL_ANDROID_H_
#define SERVICES_ML_EXECUTION_IMPL_ANDROID_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/public/interfaces/execution.mojom.h"
#include "services/ml/public/interfaces/constants.mojom.h"

#include "services/ml/model_impl_android.h"
#include "services/ml/compilation_impl_android.h"

namespace ml {

class ExecutionImplAndroid : public mojom::Execution {
 public:
  ExecutionImplAndroid(CompilationImplAndroid*);
  ~ExecutionImplAndroid() override;

  void setInput(uint32_t index, mojo::ScopedSharedBufferHandle buffer, uint32_t length, setInputCallback callback) override;
  void setOutput(uint32_t index, mojo::ScopedSharedBufferHandle buffer, uint32_t length, setOutputCallback callback) override;
  void startCompute(startComputeCallback callback) override;

 private:
  std::vector<Operand> operands_;
  std::vector<Operation> operations_;
  std::vector<uint32_t> inputs_;
  std::vector<uint32_t> outputs_;
  DISALLOW_COPY_AND_ASSIGN(ExecutionImplAndroid);
};

}  // namespace  

#endif  // SERVICES_ML_EXECUTION_IMPL_ANDROID_H_