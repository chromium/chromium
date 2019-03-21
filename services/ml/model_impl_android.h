// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_MODEL_IMPL_ANDROID_H_
#define SERVICES_ML_MODEL_IMPL_ANDROID_H_

#include <vector>

#include "base/macros.h"
#include "services/ml/common.h"
#include "services/ml/public/mojom/constants.mojom.h"
#include "services/ml/public/mojom/model.mojom.h"

#ifdef __ANDROID_API__
#undef __ANDROID_API__
#define __ANDROID_API__ 27
#include "android/NeuralNetworks.h"
#undef __ANDROID_API__
#endif

namespace ml {

class CompilationImplAndroid;

class ModelImplAndroid : public mojom::Model {
 public:
  ModelImplAndroid();
  ~ModelImplAndroid() override;

  void Finish(mojom::ModelInfoPtr model_info, FinishCallback callback) override;

  void CreateCompilation(CreateCompilationCallback callback) override;

 private:
  int32_t AddOperand(int32_t type,
                     const std::vector<uint32_t>& dimensions,
                     float scale,
                     int32_t zeroPoint);
  int32_t SetOperandValue(uint32_t index, const void* buffer, uint32_t length);
  int32_t AddOperation(int32_t type,
                       const std::vector<uint32_t>& inputs,
                       const std::vector<uint32_t>& outputs);
  int32_t IdentifyInputsAndOutputs(const std::vector<uint32_t>& inputs,
                                   const std::vector<uint32_t>& outputs);

 private:
  friend class CompilationImplAndroid;
  std::vector<Operand> operands_;
  std::vector<Operation> operations_;
  std::vector<uint32_t> inputs_;
  std::vector<uint32_t> outputs_;

  ANeuralNetworksModel* nn_model_;

  std::vector<void*> operand_memories_;

  DISALLOW_COPY_AND_ASSIGN(ModelImplAndroid);
};

}  // namespace ml

#endif  // SERVICES_ML_MODEL_IMPL_ANDROID_H_