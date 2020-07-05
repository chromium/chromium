// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/model_impl_nn.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/compilation_impl_nn.h"
#include "services/ml/ienn_symbol_table.h"

namespace ml {

ModelImplNN::ModelImplNN() {
#if defined(OS_ANDROID)
  int32_t result = ANeuralNetworksModel_create(&nn_model_);
#else
  int32_t result = IE(ie_model_create)(&ie_model_);
#endif
  DLOG(INFO) << "ANeuralNetworksModel_create: " << result;
}

ModelImplNN::~ModelImplNN() {
#if defined(OS_ANDROID)
  ANeuralNetworksModel_free(nn_model_);
#else
  IE(ie_model_free)(ie_model_);
#endif
  DLOG(INFO) << "ANeuralNetworksModel_free";
}

int32_t ModelImplNN::AddOperand(int32_t type,
                                const std::vector<uint32_t>& dimensions,
                                float scale,
                                int32_t zeroPoint) {
  // Logging
  DLOG(INFO) << "  ModelImplNN::AddOperand";
  DLOG(INFO) << "    "
             << "type: " << type;
  DLOG(INFO) << "    "
             << "dimensions(" << dimensions.size()
             << "): " << VectorToString(dimensions.data(), dimensions.size());
  DLOG(INFO) << "    "
             << "scale: " << scale;
  DLOG(INFO) << "    "
             << "zeroPoint: " << zeroPoint;

  Operand operand;
  operand.type = type;
  operand.dimensions = dimensions;
  operand.scale = scale;
  operand.zeroPoint = zeroPoint;
  operands_.push_back(operand);

#if defined(OS_ANDROID)
  // Implementation
  ANeuralNetworksOperandType operand_type;
  // TODO: convert from blink operand type to NN API type.
  operand_type.type = type;
  operand_type.dimensionCount = dimensions.size();
  operand_type.dimensions = dimensions.data();
  operand_type.scale = scale;
  operand_type.zeroPoint = zeroPoint;
  return ANeuralNetworksModel_addOperand(nn_model_, &operand_type);
#else
  ie_operand_t ie_operand;
  ie_operand.type = type;
  ie_operand.dimensionCount = dimensions.size();
  ie_operand.dimensions = dimensions.data();
  ie_operand.scale = scale;
  ie_operand.zeroPoint = zeroPoint;
  return IE(ie_model_add_operand)(ie_model_, &ie_operand);
#endif
}

int32_t ModelImplNN::SetOperandValue(uint32_t index,
                                     const void* buffer,
                                     uint32_t length) {
  // Logging
  DLOG(INFO) << "  ModelImplNN::SetOperandValue";
  DLOG(INFO) << "    "
             << "index: " << index;
  DLOG(INFO) << "    "
             << "length: " << length;

  if (index > operands_.size()) {
    return mojom::BAD_DATA;
  }

  // Implementation
  // TODO: optimize the memory copies.
#if defined(OS_ANDROID)
  return ANeuralNetworksModel_setOperandValue(nn_model_, index, buffer, length);
#else
  return IE(ie_model_set_operand_value)(ie_model_, index, buffer, length);
#endif
}

int32_t ModelImplNN::AddOperation(int32_t type,
                                  const std::vector<uint32_t>& inputs,
                                  const std::vector<uint32_t>& outputs) {
  // Logging
  DLOG(INFO) << "  ModelImplNN::AddOperation";
  DLOG(INFO) << "    "
             << "type: " << type;
  DLOG(INFO) << "    "
             << "inputs(" << inputs.size()
             << "): " << VectorToString(inputs.data(), inputs.size());
  DLOG(INFO) << "    "
             << "outputs(" << outputs.size()
             << "): " << VectorToString(outputs.data(), outputs.size());

  Operation operation;
  operation.type = type;
  operation.inputs = inputs;
  operation.outputs = outputs;
  operations_.push_back(operation);

  // Implementation
  // TODO: convert blink operation type to NN API type.
#if defined(OS_ANDROID)
  return ANeuralNetworksModel_addOperation(nn_model_, type, inputs.size(),
                                           inputs.data(), outputs.size(),
                                           outputs.data());
#else
  return IE(ie_model_add_operation)(ie_model_, type, inputs.size(),
                                    inputs.data(), outputs.size(),
                                    outputs.data());
#endif
}

int32_t ModelImplNN::IdentifyInputsAndOutputs(
    const std::vector<uint32_t>& inputs,
    const std::vector<uint32_t>& outputs) {
  DLOG(INFO) << "  ModelImplNN::IdentifyInputsAndOutputs";
  DLOG(INFO) << "    "
             << "inputs(" << inputs.size()
             << "): " << VectorToString(inputs.data(), inputs.size());
  DLOG(INFO) << "    "
             << "outputs(" << outputs.size()
             << "): " << VectorToString(outputs.data(), outputs.size());

  inputs_ = inputs;
  outputs_ = outputs;

#if defined(OS_ANDROID)
  return ANeuralNetworksModel_identifyInputsAndOutputs(
      nn_model_, inputs.size(), inputs.data(), outputs.size(), outputs.data());
#else
  return IE(ie_model_identify_inputs_outputs)(
      ie_model_, inputs.size(), inputs.data(), outputs.size(), outputs.data());
#endif
}

void ModelImplNN::Finish(mojom::ModelInfoPtr model_info,
                         FinishCallback callback) {
  DLOG(INFO) << "ModelImplNN::Finish";
  DLOG(INFO) << "operands(" << model_info->operands.size() << ")";

  int32_t result;
  for (size_t i = 0; i < model_info->operands.size(); ++i) {
    DLOG(INFO) << "  operand[" << i << "]";
    const mojom::OperandPtr& operand = model_info->operands[i];
    result = AddOperand(operand->type, operand->dimensions, operand->scale,
                        operand->zeroPoint);
    if (result != 0) {
      DLOG(ERROR) << "Fail to add operand, result = " << result;
      std::move(callback).Run(result);
      return;
    }
  }

  DLOG(INFO) << "operations(" << model_info->operations.size() << ")";
  for (size_t i = 0; i < model_info->operations.size(); ++i) {
    DLOG(INFO) << "  operation[" << i << "]";
    const mojom::OperationPtr& operation = model_info->operations[i];

    if (operation->type == mojom::RESIZE_BILINEAR &&
        operation->inputs.size() == 4) {
      LOG(WARNING) << "    discard align_corners(" << operation->inputs[3]
                   << ")";
      operation->inputs.pop_back();
    }

    result =
        AddOperation(operation->type, operation->inputs, operation->outputs);
    if (result != 0) {
      DLOG(ERROR) << "Fail to add operation, result = " << result;
      std::move(callback).Run(result);
      return;
    }
  }

  // mojo::ScopedSharedBufferMapping mapping;
  DLOG(INFO) << "values(" << model_info->values.size() << ")";
  if (model_info->values.size() != 0) {
    mapping_ = model_info->memory->Map(model_info->memory_size);
    const int8_t* base = static_cast<const int8_t*>(mapping_.get());
    for (auto itr = model_info->values.begin(); itr != model_info->values.end();
         ++itr) {
      const mojom::OperandValueInfoPtr& value_info = itr->second;
      SetOperandValue(value_info->index,
                      static_cast<const void*>(base + value_info->offset),
                      value_info->length);
    }
  }

  DLOG(INFO) << "inputs(" << model_info->inputs.size() << ")";
  DLOG(INFO) << "outputs(" << model_info->outputs.size() << ")";
  result = IdentifyInputsAndOutputs(model_info->inputs, model_info->outputs);
  if (result != 0) {
    DLOG(ERROR) << "Fail to IdentifyInputsAndOutputs, result = " << result;
    std::move(callback).Run(result);
    return;
  }

#if defined(OS_ANDROID)
  result = ANeuralNetworksModel_finish(nn_model_);
#else
  // ie backend shared the memory from ScopedSharedBufferMapping
  model_info_ = std::move(model_info);
#endif
  DLOG(INFO) << "ANeuralNetworksModel_finish: " << result;

  std::move(callback).Run(result);
}

void ModelImplNN::CreateCompilation(CreateCompilationCallback callback) {
  DLOG(INFO) << "ModelImplNN::CreateCompilation";
  mojom::CompilationPtrInfo ptr_info;
  mojo::MakeStrongBinding(
      std::make_unique<CompilationImplNN>(this, std::move(model_info_),
                                          std::move(mapping_)),
      mojo::MakeRequest(&ptr_info));

  auto init_params = mojom::CompilationInitParams::New();
  init_params->compilation = std::move(ptr_info);

  std::move(callback).Run(mojom::NOT_ERROR, std::move(init_params));
}

}  // namespace ml
