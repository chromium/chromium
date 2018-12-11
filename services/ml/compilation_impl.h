// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_COMPILATION_IMPL_H_
#define SERVICES_ML_COMPILATION_IMPL_H_

#include <vector>

#include "base/macros.h"
#include "services/ml/common.h"
#include "services/ml/public/interfaces/compilation.mojom.h"

namespace ml {

class CompilationImpl;
class ModelImpl;

class CompilationDelegate {
 public:
  CompilationDelegate() = default;
  virtual ~CompilationDelegate() = default;

  virtual int32_t Init(CompilationImpl*) = 0;
  virtual int32_t Compile() = 0;
  virtual std::unique_ptr<mojom::Execution>
  CreateExecution(mojo::ScopedSharedBufferHandle) = 0;
};

class CompilationImpl : public mojom::Compilation {
 public:
  explicit CompilationImpl(const ModelImpl*);
  ~CompilationImpl() override;

  void Finish(int32_t preference, FinishCallback callback) override;
  void CreateExecution(CreateExecutionCallback callback) override;

 private:
  friend class CompilationDelegateClDnn;
  std::vector<Operand> operands_;
  std::vector<Operation> operations_;
  std::map<uint32_t, ValueInfo> values_;
  std::vector<uint32_t> inputs_;
  std::vector<uint32_t> outputs_;
  std::vector<uint32_t> constants_;
  std::unique_ptr<int8_t[]> memory_;
  uint32_t memory_size_;

  std::unique_ptr<CompilationDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(CompilationImpl);
};

}  // namespace ml

#endif  // SERVICES_ML_COMPILATION_IMPL_H_