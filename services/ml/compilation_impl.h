// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_COMPILATION_IMPL_H_
#define SERVICES_ML_COMPILATION_IMPL_H_

#include <vector>

#include "base/macros.h"
#include "services/ml/common.h"
#include "services/ml/public/interfaces/compilation.mojom.h"
#include "services/ml/public/interfaces/model.mojom.h"

namespace ml {

class CompilationImpl;
class ModelImpl;

class CompilationDelegate {
 public:
  explicit CompilationDelegate() = default;
  virtual ~CompilationDelegate() = default;

  virtual int32_t Compile() = 0;
  virtual std::unique_ptr<mojom::Execution> CreateExecution(
      mojom::ExecutionInitParamsPtr params) = 0;
};

class CompilationImpl : public mojom::Compilation {
 public:
  explicit CompilationImpl(mojom::ModelInfoPtr model_info);
  ~CompilationImpl() override;

  void Finish(int32_t preference, FinishCallback callback) override;
  void CreateExecution(CreateExecutionCallback callback) override;

  const mojom::ModelInfoPtr& GetModel() const { return model_info_; }
  int32_t GetScalarInt32(uint32_t index) const;
  float GetScalarFloat(uint32_t index) const;
  mojo::ScopedSharedBufferMapping MapMemory(uint32_t index) const;

 private:
  mojom::ModelInfoPtr model_info_;

  std::unique_ptr<CompilationDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(CompilationImpl);
};

}  // namespace ml

#endif  // SERVICES_ML_COMPILATION_IMPL_H_