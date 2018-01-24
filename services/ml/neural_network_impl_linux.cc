// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/neural_network_impl_linux.h"

namespace ml {

void NeuralNetworkImplLinux::Create(
    ml::mojom::NeuralNetworkRequest request) {
  auto impl = std::make_unique<NeuralNetworkImplLinux>();
  auto* impl_ptr = impl.get();
  impl_ptr->binding_ =
      mojo::MakeStrongBinding(std::move(impl), std::move(request));
}

NeuralNetworkImplLinux::NeuralNetworkImplLinux() {}

NeuralNetworkImplLinux::~NeuralNetworkImplLinux() {}

static uint32_t product(std::vector<uint32_t> dimensions) {
  uint32_t prod = 1;
  for (size_t i = 0; i < dimensions.size(); ++i) prod *= dimensions[i];
  return prod;
}

static void PrintInputsOutputs(const std::vector<uint32_t>& array, std::string name) {
  for (size_t i = 0; i < array.size(); ++i) {
    LOG(INFO) << "  " << name << "[" << i << "]: " << array[i];
  }
}

static void PrintOperand(const mojom::OperandPtr& operand, const mojo::ScopedSharedBufferHandle& buffer) {
  LOG(INFO) << "    " << "type: " << operand->type;
  LOG(INFO) << "    " << "dimensions(" << operand->dimensions.size() << ")";
  for (size_t i = 0; i < operand->dimensions.size(); ++i) {
    LOG(INFO) << "      " << "dimensions[" << i << "]: " << operand->dimensions[i];
  }
  LOG(INFO) << "    " << "scale: " << operand->scale;
  LOG(INFO) << "    " << "zeroPoint: " << operand->zeroPoint;
  LOG(INFO) << "    " << "bufferInfo.offset: " << operand->bufferInfo->offset;
  LOG(INFO) << "    " << "bufferInfo.length: " << operand->bufferInfo->length;

  uint32_t size = product(operand->dimensions);
  if (size > 10) size = 10;
  LOG(INFO) << "    " << "value[0.." << size << "]: ";
  auto mapped = buffer->MapAtOffset(operand->bufferInfo->length, operand->bufferInfo->offset);
  if (operand->type == 3 && operand->bufferInfo->length > 0) {
    float* value = static_cast<float*>(mapped.get());
    for (size_t i = 0; i < size; ++i) {
      LOG(INFO) << "      " << "value[" << i << "]" << value[i];
    }
  }
}

static void PrintOperation(const mojom::OperationPtr& operation) {
  LOG(INFO) << "    " << "type: " << operation->type;
  LOG(INFO) << "    " << "inputs(" << operation->inputs.size() << ")";
  PrintInputsOutputs(operation->inputs, "inputs");
  LOG(INFO) << "    " << "outputs(" << operation->outputs.size() << ")";
  PrintInputsOutputs(operation->outputs, "outputs");
}

void NeuralNetworkImplLinux::compile(mojom::ModelPtr model, int32_t preference, compileCallback callback) {
  LOG(INFO) << "Compile Model";
  LOG(INFO) << "operands(" << model->operands.size() << ")";
  for (size_t i = 0; i < model->operands.size(); ++i ) {
    LOG(INFO) << "  operand[" << i << "]";
    PrintOperand(model->operands[i], model->buffer);
  }
  LOG(INFO) << "operations(" << model->operations.size() << ")";
  for (size_t i = 0; i < model->operations.size(); ++i ) {
    LOG(INFO) << "  operation[" << i << "]";
    PrintOperation(model->operations[i]);
  }
  LOG(INFO) << "inputs(" << model->inputs.size() << ")";
  PrintInputsOutputs(model->inputs, "inputs");
  LOG(INFO) << "outputs(" << model->outputs.size() << ")";
  PrintInputsOutputs(model->outputs, "outputs");
  std::move(callback).Run(0);
}

void NeuralNetworkImplLinux::compute(int32_t id, mojom::ComputeRequestPtr request, computeCallback callback) {
  LOG(INFO) << "Compute Model";
  std::move(callback).Run(0);
}

}  // namespace ml
