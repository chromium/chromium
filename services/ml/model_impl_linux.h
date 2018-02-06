// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_MODEL_IMPL_LINUX_H_
#define SERVICES_ML_MODEL_IMPL_LINUX_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/public/interfaces/model.mojom.h"
#include "services/ml/public/interfaces/constants.mojom.h"
#include "services/ml/common.h"

namespace ml {

class CompilationImplLinux;

class ModelImplLinux : public mojom::Model {
 public:
  ModelImplLinux();
  ~ModelImplLinux() override;

  void finish(mojom::ModelInfoPtr model_info, finishCallback callback) override;

  void createCompilation(createCompilationCallback callback) override;
 
 private:
  int32_t AddOperand(int32_t type, const std::vector<uint32_t>& dimensions, float scale, int32_t zeroPoint);
  int32_t SetOperandValue(uint32_t index, const void* buffer, uint32_t length);
  int32_t AddOperation(int32_t type, const std::vector<uint32_t>& inputs, const std::vector<uint32_t>& outputs);
  int32_t IdentifyInputsAndOutputs(const std::vector<uint32_t>& inputs, const std::vector<uint32_t>& outputs);

 private:
  friend class CompilationImplLinux;
  std::vector<Operand> operands_;
  std::vector<Operation> operations_;
  std::vector<uint32_t> inputs_;
  std::vector<uint32_t> outputs_;
  DISALLOW_COPY_AND_ASSIGN(ModelImplLinux);
};

}  // namespace  

#endif  // SERVICES_ML_MODEL_IMPL_LINUX_H_