// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/model_impl_linux.h"
#include "services/ml/compilation_impl_linux.h"
#include "base/strings/string_number_conversions.h"

namespace ml {

ModelImplLinux::ModelImplLinux() {}
ModelImplLinux::~ModelImplLinux() {}

void ModelImplLinux::addOperand(int32_t type, const std::vector<uint32_t>& dimensions, float scale, int32_t zeroPoint, addOperandCallback callback) {
  DLOG(INFO) << "ModelImplLinux::addOperand";
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
  std::move(callback).Run(mojom::NO_ERROR);
}

void ModelImplLinux::setOperandValue(uint32_t index, mojo::ScopedSharedBufferHandle buffer, uint32_t length, setOperandValueCallback callback) {
  DLOG(INFO) << "ModelImplLinux::setOperandValue";
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
  std::move(callback).Run(mojom::NO_ERROR);
}

void ModelImplLinux::addOperation(int32_t type, const std::vector<uint32_t>& inputs, const std::vector<uint32_t>& outputs, addOperationCallback callback) {
  DLOG(INFO) << "ModelImplLinux::addOperation";
  DLOG(INFO) << "  " << "type: " << type;
  DLOG(INFO) << "  " << "inputs(" << inputs.size() << "): " << VectorToString(inputs.data(), inputs.size());
  DLOG(INFO) << "  " << "outputs(" << outputs.size() << "): " << VectorToString(outputs.data(), outputs.size());
  Operation operation;
  operation.type = type;
  operation.inputs = inputs;
  operation.outputs = outputs;
  operations_.push_back(operation);
  std::move(callback).Run(mojom::NO_ERROR);
}

void ModelImplLinux::identifyInputsAndOutputs(const std::vector<uint32_t>& inputs, const std::vector<uint32_t>& outputs, identifyInputsAndOutputsCallback callback) {
  DLOG(INFO) << "ModelImplLinux::identifyInputsAndOutputs";
  DLOG(INFO) << "  " << "inputs(" << inputs.size() << "): " << VectorToString(inputs.data(), inputs.size());
  DLOG(INFO) << "  " << "outputs(" << outputs.size() << "): " << VectorToString(outputs.data(), outputs.size());
  inputs_ = inputs;
  outputs_ = outputs;
  std::move(callback).Run(mojom::NO_ERROR);
}

void ModelImplLinux::finish(finishCallback callback) {
  DLOG(INFO) << "ModelImplLinux::finish";
  std::move(callback).Run(mojom::NO_ERROR);
}

void ModelImplLinux::createCompilation(createCompilationCallback callback) {
  DLOG(INFO) << "ModelImplLinux::createCompilation";
  auto init_params = mojom::CompilationInitParams::New();

  auto impl = std::make_unique<CompilationImplLinux>(this);

  mojom::CompilationPtrInfo ptr_info;
  mojo::MakeStrongBinding(std::move(impl),
                          mojo::MakeRequest(&ptr_info));
  init_params->compilation = std::move(ptr_info);
  
  std::move(callback).Run(mojom::NO_ERROR,
                          std::move(init_params));
}

}  // namespace ml
