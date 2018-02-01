// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_MODEL_IMPL_ANDROID_H_
#define SERVICES_ML_MODEL_IMPL_ANDROID_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/public/interfaces/model.mojom.h"
#include "services/ml/public/interfaces/constants.mojom.h"

namespace ml {

template<class T>
std::string VectorToString(const T* vect, size_t length);

struct Operand {
  Operand();
  ~Operand();
  Operand(const Operand&);
  int32_t type;
  std::vector<uint32_t> dimensions;
  float scale;
  int32_t zeroPoint;
};

struct Operation {
  Operation();
  ~Operation();
  Operation(const Operation&);
  int32_t type;
  std::vector<uint32_t> inputs;
  std::vector<uint32_t> outputs;
};

class CompilationImplAndroid;

class ModelImplAndroid : public mojom::Model {
 public:
  ModelImplAndroid();
  ~ModelImplAndroid() override;

  void addOperand(int32_t type, const std::vector<uint32_t>& dimensions, float scale, int32_t zeroPoint, addOperandCallback callback) override;

  void setOperandValue(uint32_t index, mojo::ScopedSharedBufferHandle buffer, uint32_t length, setOperandValueCallback callback) override;

  void addOperation(int32_t type, const std::vector<uint32_t>& inputs, const std::vector<uint32_t>& outputs, addOperationCallback callback) override;

  void identifyInputsAndOutputs(const std::vector<uint32_t>& inputs, const std::vector<uint32_t>& outputs, identifyInputsAndOutputsCallback callback) override;

  void finish(finishCallback callback) override;

  void createCompilation(createCompilationCallback callback) override;

 private:
  friend class CompilationImplAndroid;
  std::vector<Operand> operands_;
  std::vector<Operation> operations_;
  std::vector<uint32_t> inputs_;
  std::vector<uint32_t> outputs_;
  DISALLOW_COPY_AND_ASSIGN(ModelImplAndroid);
};

}  // namespace  

#endif  // SERVICES_ML_MODEL_IMPL_ANDROID_H_