// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/model_impl_android.h"
#include "services/ml/compilation_impl_android.h"
#include "base/strings/string_number_conversions.h"

namespace ml {

ModelImplAndroid::ModelImplAndroid() {
  int32_t result = ANeuralNetworksModel_create(&nn_model_);
  DLOG(INFO) << "ANeuralNetworksModel_create: " << result;
}

ModelImplAndroid::~ModelImplAndroid() {
  ANeuralNetworksModel_free(nn_model_);
  DLOG(INFO) << "ANeuralNetworksModel_free";
}

void ModelImplAndroid::addOperand(int32_t type, const std::vector<uint32_t>& dimensions, float scale, int32_t zeroPoint, addOperandCallback callback) {
  DLOG(INFO) << "ModelImplAndroid::addOperand";
  DLOG(INFO) << "  " << "type: " << type;
  DLOG(INFO) << "  " << "dimensions(" << dimensions.size() << "): " << VectorToString(dimensions.data(), dimensions.size());
  DLOG(INFO) << "  " << "scale: " << scale;
  DLOG(INFO) << "  " << "zeroPoint: " << zeroPoint;
  Operand operand;
  operand.type = type;
  operand.dimensions = dimensions;
  operand.scale = scale;
  operand.zeroPoint = zeroPoint;
  operands_.push_back(operand);

  ANeuralNetworksOperandType operand_type;
  // TODO: convert from blink operand type to NN API type.
  operand_type.type = type;
  operand_type.dimensionCount = dimensions.size();
  operand_type.dimensions = dimensions.data();
  operand_type.scale = scale;
  operand_type.zeroPoint = zeroPoint;
  int32_t result = ANeuralNetworksModel_addOperand(nn_model_, &operand_type);
  DLOG(INFO) << "ANeuralNetworksModel_addOperand: " << result;

  std::move(callback).Run(result);
}

void ModelImplAndroid::setOperandValue(uint32_t index, mojo::ScopedSharedBufferHandle buffer, uint32_t length, setOperandValueCallback callback) {
  DLOG(INFO) << "ModelImplAndroid::setOperandValue";
  DLOG(INFO) << "  " << "index: " << index;
  DLOG(INFO) << "  " << "length: " << length;
  if (index > operands_.size()) {
    std::move(callback).Run(mojom::BAD_DATA);
    return;
  }
  auto mapped = buffer->Map(length);
  auto operand = operands_[index];
  if (operand.type == mojom::TENSOR_FLOAT32 || operand.type == mojom::FLOAT32) {
    float* value = static_cast<float*>(mapped.get());
    uint32_t size = length / 4;
    DLOG(INFO) << "  " << "buffer(" << size << "): " << VectorToString(value, size);
  } else if (operand.type == mojom::TENSOR_INT32 || operand.type == mojom::INT32) {
    int32_t* value = static_cast<int32_t*>(mapped.get());
    uint32_t size = length / 4;
    DLOG(INFO) << "  " << "buffer(" << size << "): " << VectorToString(value, size);
  } else if (operand.type == mojom::TENSOR_QUANT8_ASYMM) {
    int8_t* value = static_cast<int8_t*>(mapped.get());
    uint32_t size = length;
    DLOG(INFO) << "  " << "buffer(" << size << "): " << VectorToString(value, size);
  } else if (operand.type == mojom::UINT32) {
    uint32_t* value = static_cast<uint32_t*>(mapped.get());
    uint32_t size = length;
    DLOG(INFO) << "  " << "buffer(" << size << "): " << VectorToString(value, size);
  }

  // TODO: optimize the memory copies.
  int32_t result = 0;
  if (length > ANEURALNETWORKS_MAX_SIZE_OF_IMMEDIATELY_COPIED_VALUES) {
    void* memory = malloc(length);
    memcpy(memory, static_cast<const void*>(mapped.get()), length);
    operand_memories_.push_back(memory);
    result = ANeuralNetworksModel_setOperandValue(
      nn_model_, index, memory, length);
  } else {
    result = ANeuralNetworksModel_setOperandValue(
      nn_model_, index, static_cast<const void*>(mapped.get()), length);
  }

  DLOG(INFO) << "ANeuralNetworksModel_setOperandValue: " << result;

  std::move(callback).Run(result);
}

void ModelImplAndroid::addOperation(int32_t type, const std::vector<uint32_t>& inputs, const std::vector<uint32_t>& outputs, addOperationCallback callback) {
  DLOG(INFO) << "ModelImplAndroid::addOperation";
  DLOG(INFO) << "  " << "type: " << type;
  DLOG(INFO) << "  " << "inputs(" << inputs.size() << "): " << VectorToString(inputs.data(), inputs.size());
  DLOG(INFO) << "  " << "outputs(" << outputs.size() << "): " << VectorToString(outputs.data(), outputs.size());
  Operation operation;
  operation.type = type;
  operation.inputs = inputs;
  operation.outputs = outputs;
  operations_.push_back(operation);

  // TODO: convert blink operation type to NN API type.
  int32_t result = ANeuralNetworksModel_addOperation(
    nn_model_, type, inputs.size(), inputs.data(), outputs.size(), outputs.data());
  DLOG(INFO) << "ANeuralNetworksModel_addOperation: " << result;

  std::move(callback).Run(result);
}

void ModelImplAndroid::identifyInputsAndOutputs(const std::vector<uint32_t>& inputs, const std::vector<uint32_t>& outputs, identifyInputsAndOutputsCallback callback) {
  DLOG(INFO) << "ModelImplAndroid::identifyInputsAndOutputs";
  DLOG(INFO) << "  " << "inputs(" << inputs.size() << "): " << VectorToString(inputs.data(), inputs.size());
  DLOG(INFO) << "  " << "outputs(" << outputs.size() << "): " << VectorToString(outputs.data(), outputs.size());
  inputs_ = inputs;
  outputs_ = outputs;

  int32_t result = ANeuralNetworksModel_identifyInputsAndOutputs(
      nn_model_, inputs.size(), inputs.data(), outputs.size(), outputs.data());
  DLOG(INFO) << "ANeuralNetworksModel_identifyInputsAndOutputs: " << result;

  std::move(callback).Run(result);
}

void ModelImplAndroid::finish(finishCallback callback) {
  DLOG(INFO) << "ModelImplAndroid::finish";

  int32_t result = ANeuralNetworksModel_finish(nn_model_);
  DLOG(INFO) << "ANeuralNetworksModel_finish: " << result;

  //for (size_t i = 0; i < operand_memories_.size(); ++i) {
  //  free(operand_memories_[i]);
  //}
  //operand_memories_.resize(0);

  std::move(callback).Run(result);
}

void ModelImplAndroid::createCompilation(createCompilationCallback callback) {
  DLOG(INFO) << "ModelImplAndroid::createCompilation";
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
