// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_COMPILATION_IMPL_LINUX_H_
#define SERVICES_ML_COMPILATION_IMPL_LINUX_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/public/interfaces/compilation.mojom.h"
#include "services/ml/public/interfaces/constants.mojom.h"

#include "services/ml/common.h"
#include "services/ml/model_impl_linux.h"
#include "services/ml/execution_impl_linux.h"

namespace ml {

class CompilationImplLinux : public mojom::Compilation {
 public:
  CompilationImplLinux(ModelImplLinux*);
  ~CompilationImplLinux() override;

  void setPreference(int32_t preference, setPreferenceCallback callback) override;
  void finish(finishCallback callback) override;
  void createExecution(createExecutionCallback callback) override;

 private:
  friend class ExecutionImplLinux;
  std::vector<Operand> operands_;
  std::vector<Operation> operations_;
  std::vector<uint32_t> inputs_;
  std::vector<uint32_t> outputs_;

  DISALLOW_COPY_AND_ASSIGN(CompilationImplLinux);
};

}  // namespace  

#endif  // SERVICES_ML_COMPILATION_IMPL_LINUX_H_