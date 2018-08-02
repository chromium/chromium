// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/model_impl_linux.h"
#include "services/ml/compilation_impl_linux.h"
#include "base/strings/string_number_conversions.h"

namespace ml {

ModelImplLinux::ModelImplLinux() {}
ModelImplLinux::~ModelImplLinux() {}

int32_t ModelImplLinux::AddOperand(int32_t type, const std::vector<uint32_t>& dimensions, float scale, int32_t zeroPoint) {
  DLOG(INFO) << "  ModelImplLinux::AddOperand";
  DLOG(INFO) << "    " << "type: " << type;
  DLOG(INFO) << "    " << "dimensions(" << dimensions.size() << "): " << VectorToString(dimensions.data(), dimensions.size());
  DLOG(INFO) << "    " << "scale: " << scale;
  DLOG(INFO) << "    " << "zeroPoint: " << zeroPoint;
  Operand operand;
  operand.type = type;
  operand.dimensions = dimensions;
  operand.scale = scale;
  operand.zeroPoint = zeroPoint;
  operands_.push_back(operand);
  return mojom::NOT_ERROR;
}

int32_t ModelImplLinux::SetOperandValue(uint32_t index, const void* buffer, uint32_t length) {
  DLOG(INFO) << "  ModelImplLinux::SetOperandValue";
  DLOG(INFO) << "    " << "index: " << index;
  DLOG(INFO) << "    " << "length: " << length;
  if (index > operands_.size()) {
    return mojom::BAD_DATA;
  }
  auto operand = operands_[index];
  if (operand.type == mojom::TENSOR_FLOAT32 || operand.type == mojom::FLOAT32) {
    const float* value = static_cast<const float*>(buffer);
    uint32_t size = length / 4;
    DLOG(INFO) << "    " << "buffer(" << size << "): " << VectorToString(value, size);
  } else if (operand.type == mojom::TENSOR_INT32 || operand.type == mojom::INT32) {
    const int32_t* value = static_cast<const int32_t*>(buffer);
    uint32_t size = length / 4;
    DLOG(INFO) << "    " << "buffer(" << size << "): " << VectorToString(value, size);
  } else if (operand.type == mojom::TENSOR_QUANT8_ASYMM) {
    const int8_t* value = static_cast<const int8_t*>(buffer);
    uint32_t size = length;
    DLOG(INFO) << "    " << "buffer(" << size << "): " << VectorToString(value, size);
  } else if (operand.type == mojom::UINT32) {
    const uint32_t* value = static_cast<const uint32_t*>(buffer);
    uint32_t size = length;
    DLOG(INFO) << "    " << "buffer(" << size << "): " << VectorToString(value, size);
  }
  return mojom::NOT_ERROR;
}

int32_t ModelImplLinux::AddOperation(int32_t type, const std::vector<uint32_t>& inputs, const std::vector<uint32_t>& outputs) {
  DLOG(INFO) << "  ModelImplLinux::AddOperation";
  DLOG(INFO) << "    " << "type: " << type;
  DLOG(INFO) << "    " << "inputs(" << inputs.size() << "): " << VectorToString(inputs.data(), inputs.size());
  DLOG(INFO) << "    " << "outputs(" << outputs.size() << "): " << VectorToString(outputs.data(), outputs.size());
  Operation operation;
  operation.type = type;
  operation.inputs = inputs;
  operation.outputs = outputs;
  operations_.push_back(operation);
  return mojom::NOT_ERROR;
}

int32_t ModelImplLinux::IdentifyInputsAndOutputs(const std::vector<uint32_t>& inputs, const std::vector<uint32_t>& outputs) {
  DLOG(INFO) << "  ModelImplLinux::IdentifyInputsAndOutputs";
  DLOG(INFO) << "    " << "inputs(" << inputs.size() << "): " << VectorToString(inputs.data(), inputs.size());
  DLOG(INFO) << "    " << "outputs(" << outputs.size() << "): " << VectorToString(outputs.data(), outputs.size());
  inputs_ = inputs;
  outputs_ = outputs;
  return mojom::NOT_ERROR;
}

void ModelImplLinux::finish(mojom::ModelInfoPtr model_info, finishCallback callback) {
  DLOG(INFO) << "ModelImplLinux::finish";
  DLOG(INFO) << "operands(" << model_info->operands.size() << ")";
  for (size_t i = 0; i < model_info->operands.size(); ++i ) {
    DLOG(INFO) << "  operand[" << i << "]";
    const mojom::OperandPtr& operand = model_info->operands[i];
    AddOperand(operand->type, operand->dimensions, operand->scale, operand->zeroPoint);
  }
  DLOG(INFO) << "operations(" << model_info->operations.size() << ")";
  for (size_t i = 0; i < model_info->operations.size(); ++i ) {
    DLOG(INFO) << "  operation[" << i << "]";
    const mojom::OperationPtr& operation = model_info->operations[i];
    AddOperation(operation->type, operation->inputs, operation->outputs);
  }
  DLOG(INFO) << "values(" << model_info->values.size() << ")";
  auto mapping = model_info->memory->Map(model_info->memory_size);
  const int8_t* base = static_cast<const int8_t*>(mapping.get());
  for (size_t i = 0; i < model_info->values.size(); ++i ) {
    DLOG(INFO) << "  values[" << i << "]";
    const mojom::OperandValueInfoPtr& value_info = model_info->values[i];
    SetOperandValue(value_info->index,
                    static_cast<const void*>(base + value_info->offset),
                    value_info->length);
  }
  DLOG(INFO) << "inputs(" << model_info->inputs.size() << ")";
  DLOG(INFO) << "outputs(" << model_info->outputs.size() << ")";
  IdentifyInputsAndOutputs(model_info->inputs, model_info->outputs);

  std::move(callback).Run(mojom::NOT_ERROR);
}

void ModelImplLinux::createCompilation(createCompilationCallback callback) {
  DLOG(INFO) << "ModelImplLinux::createCompilation";
  auto init_params = mojom::CompilationInitParams::New();

  auto impl = std::make_unique<CompilationImplLinux>(this);

  mojom::CompilationPtrInfo ptr_info;
  mojo::MakeStrongBinding(std::move(impl),
                          mojo::MakeRequest(&ptr_info));
  init_params->compilation = std::move(ptr_info);
  
  std::move(callback).Run(mojom::NOT_ERROR,
                          std::move(init_params));
}

}  // namespace ml
