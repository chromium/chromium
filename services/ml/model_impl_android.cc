// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/model_impl_android.h"
#include "services/ml/compilation_impl_android.h"
#include "base/strings/string_number_conversions.h"

namespace ml {

Operand::Operand() = default;
Operand::~Operand() = default;
Operand::Operand(const Operand&) = default;

Operation::Operation() = default;
Operation::~Operation() = default;
Operation::Operation(const Operation&) = default;

ModelImplAndroid::ModelImplAndroid() {}
ModelImplAndroid::~ModelImplAndroid() {}

template<class T>
std::string VectorToString(const T* vect, size_t length) {
  std::string output("[");
  for (size_t i = 0; i < length; ++i) {
    output.append(base::NumberToString(vect[i]));
    if (i != length - 1) {
      output.append(", ");
    }
  }
  output.append("]");
  return output;
}

void PrintBuffer(const mojo::ScopedSharedBufferHandle& buffer, uint32_t length) {
  
}

void ModelImplAndroid::addOperand(int32_t type, const std::vector<uint32_t>& dimensions, float scale, int32_t zeroPoint, addOperandCallback callback) {
  LOG(INFO) << "ModelImplAndroid::addOperand";
  LOG(INFO) << "  " << "type: " << type;
  LOG(INFO) << "  " << "dimensions(" << dimensions.size() << "): " << VectorToString(dimensions.data(), dimensions.size());
  LOG(INFO) << "  " << "scale: " << scale;
  LOG(INFO) << "  " << "zeroPoint: " << zeroPoint;
  Operand operand;
  operand.type = type;
  operand.dimensions = dimensions;
  operand.scale = scale;
  operand.zeroPoint = zeroPoint;
  operands_.push_back(operand);
  std::move(callback).Run(mojom::NO_ERROR);
}

void ModelImplAndroid::setOperandValue(uint32_t index, mojo::ScopedSharedBufferHandle buffer, uint32_t length, setOperandValueCallback callback) {
  LOG(INFO) << "ModelImplAndroid::setOperandValue";
  LOG(INFO) << "  " << "index: " << index;
  LOG(INFO) << "  " << "length: " << length;
  if (index > operands_.size()) {
    std::move(callback).Run(mojom::BAD_DATA);
    return;
  }
  auto mapped = buffer->Map(length);
  auto operand = operands_[index];
  if (operand.type == mojom::TENSOR_FLOAT32 || operand.type == mojom::FLOAT32) {
    float* value = static_cast<float*>(mapped.get());
    uint32_t size = length / 4;
    LOG(INFO) << "  " << "buffer(" << size << "): " << VectorToString(value, size);
  } else if (operand.type == mojom::TENSOR_INT32 || operand.type == mojom::INT32) {
    int32_t* value = static_cast<int32_t*>(mapped.get());
    uint32_t size = length / 4;
    LOG(INFO) << "  " << "buffer(" << size << "): " << VectorToString(value, size);
  } else if (operand.type == mojom::TENSOR_QUANT8_ASYMM) {
    int8_t* value = static_cast<int8_t*>(mapped.get());
    uint32_t size = length;
    LOG(INFO) << "  " << "buffer(" << size << "): " << VectorToString(value, size);
  } else if (operand.type == mojom::UINT32) {
    uint32_t* value = static_cast<uint32_t*>(mapped.get());
    uint32_t size = length;
    LOG(INFO) << "  " << "buffer(" << size << "): " << VectorToString(value, size);
  }
  std::move(callback).Run(mojom::NO_ERROR);
}

void ModelImplAndroid::addOperation(int32_t type, const std::vector<uint32_t>& inputs, const std::vector<uint32_t>& outputs, addOperationCallback callback) {
  LOG(INFO) << "ModelImplAndroid::addOperation";
  LOG(INFO) << "  " << "type: " << type;
  LOG(INFO) << "  " << "inputs(" << inputs.size() << "): " << VectorToString(inputs.data(), inputs.size());
  LOG(INFO) << "  " << "outputs(" << outputs.size() << "): " << VectorToString(outputs.data(), outputs.size());
  Operation operation;
  operation.type = type;
  operation.inputs = inputs;
  operation.outputs = outputs;
  operations_.push_back(operation);
  std::move(callback).Run(mojom::NO_ERROR);
}

void ModelImplAndroid::identifyInputsAndOutputs(const std::vector<uint32_t>& inputs, const std::vector<uint32_t>& outputs, identifyInputsAndOutputsCallback callback) {
  LOG(INFO) << "ModelImplAndroid::identifyInputsAndOutputs";
  LOG(INFO) << "  " << "inputs(" << inputs.size() << "): " << VectorToString(inputs.data(), inputs.size());
  LOG(INFO) << "  " << "outputs(" << outputs.size() << "): " << VectorToString(outputs.data(), outputs.size());
  inputs_ = inputs;
  outputs_ = outputs;
  std::move(callback).Run(mojom::NO_ERROR);
}

void ModelImplAndroid::finish(finishCallback callback) {
  LOG(INFO) << "ModelImplAndroid::finish";
  std::move(callback).Run(mojom::NO_ERROR);
}

void ModelImplAndroid::createCompilation(createCompilationCallback callback) {
  LOG(INFO) << "ModelImplAndroid::createCompilation";
  auto init_params = mojom::CompilationInitParams::New();

  auto impl = std::make_unique<CompilationImplAndroid>(this);

  mojom::CompilationPtrInfo ptr_info;
  mojo::MakeStrongBinding(std::move(impl),
                          mojo::MakeRequest(&ptr_info));
  init_params->compilation = std::move(ptr_info);
  
  std::move(callback).Run(mojom::NO_ERROR,
                          std::move(init_params));
}

}  // namespace ml
