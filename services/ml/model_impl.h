// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_MODEL_IMPL_H_
#define SERVICES_ML_MODEL_IMPL_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/public/interfaces/model.mojom.h"
#include "services/ml/public/interfaces/constants.mojom.h"
#include "services/ml/common.h"

namespace ml {

class ModelImpl : public mojom::Model {
 public:
  ModelImpl();
  ~ModelImpl() override;

  void Finish(mojom::ModelInfoPtr model_info, FinishCallback callback) override;

  void CreateCompilation(CreateCompilationCallback callback) override;

 private:
  int32_t AddOperand(int32_t type, const std::vector<uint32_t>& dimensions, float scale, int32_t zeroPoint);
  int32_t SetOperandValue(uint32_t index, const void* buffer, uint32_t length);
  int32_t AddOperation(int32_t type, const std::vector<uint32_t>& inputs, const std::vector<uint32_t>& outputs);
  int32_t IdentifyInputsAndOutputs(const std::vector<uint32_t>& inputs, const std::vector<uint32_t>& outputs);

 private:
  friend class CompilationImpl;
  std::vector<Operand> operands_;
  std::vector<Operation> operations_;
  std::map<uint32_t, ValueInfo> values_;
  std::vector<uint32_t> inputs_;
  std::vector<uint32_t> outputs_;
  std::unique_ptr<int8_t []> memory_;
  uint32_t memory_size_;
  DISALLOW_COPY_AND_ASSIGN(ModelImpl);
};

}  // namespace ml

#endif  // SERVICES_ML_MODEL_IMPL_MAC_H_
