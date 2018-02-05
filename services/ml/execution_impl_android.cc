// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/execution_impl_android.h"

namespace ml {

ExecutionImplAndroid::ExecutionImplAndroid(CompilationImplAndroid* compilation) : output_buffer_(nullptr) {
  operands_ = compilation->operands_;
  operations_ = compilation->operations_;
  inputs_ = compilation->inputs_;
  outputs_ = compilation->outputs_;

  int32_t result = ANeuralNetworksExecution_create(compilation->nn_compilation_, &nn_execution_);
  LOG(INFO) << "ANeuralNetworksExecution_create: " << result;
}

ExecutionImplAndroid::~ExecutionImplAndroid() {
  ANeuralNetworksExecution_free(nn_execution_);
  LOG(INFO) << "ANeuralNetworksExecution_free";
}

void ExecutionImplAndroid::setInput(uint32_t index, mojo::ScopedSharedBufferHandle buffer, uint32_t length, setInputCallback callback) {
  LOG(INFO) << "ExecutionImplAndroid::setInput";
  LOG(INFO) << "  " << "index: " << index;
  LOG(INFO) << "  " << "length: " << length;
  if (index > inputs_.size()) {
    std::move(callback).Run(mojom::BAD_DATA);
    return;
  }
  uint32_t inputOperandIndex = inputs_[index];
  if (inputOperandIndex > operands_.size()) {
    std::move(callback).Run(mojom::BAD_DATA);
    return;
  }
  auto mapped = buffer->Map(length);
  auto operand = operands_[inputOperandIndex];
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

  auto itr = input_buffers_.find(index);
  if (itr == input_buffers_.end()) {
    std::pair<std::map<uint32_t, void*>::iterator, bool> ret;
    ret = input_buffers_.insert(
        std::pair<uint32_t, void*>(index, malloc(length)));
    itr = ret.first;
  }
  void* input = itr->second;
  mojo::ScopedSharedBufferMapping mapping = buffer->Map(length);
  memcpy(input, static_cast<const void*>(mapping.get()), length);
  int32_t result = ANeuralNetworksExecution_setInput(
      nn_execution_, index, NULL, input, length);

  LOG(INFO) << "ANeuralNetworksExecution_setInput: " << result;

  std::move(callback).Run(result);
}

void ExecutionImplAndroid::setOutput(uint32_t index, mojo::ScopedSharedBufferHandle buffer, uint32_t length, setOutputCallback callback) {
  LOG(INFO) << "ExecutionImplAndroid::setOutput";
  LOG(INFO) << "  " << "index: " << index;
  LOG(INFO) << "  " << "length: " << length;

  if (!output_buffer_) {
    output_buffer_ = malloc(length);
    outout_length_ = length;
  }
  output_handle_ = std::move(buffer);
  
  int32_t result = ANeuralNetworksExecution_setOutput(
      nn_execution_, index, NULL, output_buffer_, length);

  LOG(INFO) << "ANeuralNetworksExecution_setOutput: " << result;

  std::move(callback).Run(result);
}

void ExecutionImplAndroid::startCompute(startComputeCallback callback) {
  LOG(INFO) << "ExecutionImplAndroid::startCompute";

  ANeuralNetworksEvent* nn_event;
  int32_t result = ANeuralNetworksExecution_startCompute(nn_execution_, &nn_event);
  LOG(INFO) << "ANeuralNetworksExecution_startCompute: " << result;
  result = ANeuralNetworksEvent_wait(nn_event);
  LOG(INFO) << "ANeuralNetworksEvent_wait: " << result;
  ANeuralNetworksEvent_free(nn_event);

  mojo::ScopedSharedBufferMapping mapping = output_handle_->Map(outout_length_);
  memcpy(static_cast<void*>(mapping.get()), output_buffer_, outout_length_);

  std::move(callback).Run(mojom::NO_ERROR);
}

}  // namespace ml
