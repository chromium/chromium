// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_COMPILATION_IMPL_MAC_H_
#define SERVICES_ML_COMPILATION_IMPL_MAC_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/public/interfaces/compilation.mojom.h"
#include "services/ml/public/interfaces/constants.mojom.h"

#include "services/ml/common.h"
#include "services/ml/model_impl_mac.h"
#include "services/ml/execution_impl_mac.h"

namespace ml {

class CompilationImplMac : public mojom::Compilation {
 public:
  CompilationImplMac(ModelImplMac*);
  ~CompilationImplMac() override;

  void finish(int32_t preference, finishCallback callback) override;
  void createExecution(createExecutionCallback callback) override;

 private:
  bool CompileConv2D(const Operation&);
  bool CompileDepthwiseConv2D(const Operation&);
  bool CompileAveragePool2D(const Operation&);
  bool CompileSoftmax(const Operation&);
  bool CompileReshape(const Operation&);

 private:
  friend class ExecutionImplMac;
  std::vector<Operand> operands_;
  std::vector<Operation> operations_;
  std::map<uint32_t, ValueInfo> values_;
  std::vector<uint32_t> inputs_;
  std::vector<uint32_t> outputs_;
  std::unique_ptr<int8_t []> memory_;
  uint32_t memory_size_;
  DISALLOW_COPY_AND_ASSIGN(CompilationImplMac);
};

}  // namespace  

#endif  // SERVICES_ML_COMPILATION_IMPL_MAC_H_