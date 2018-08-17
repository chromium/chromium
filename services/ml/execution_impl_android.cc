// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/execution_impl_android.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/public/interfaces/constants.mojom.h"

namespace ml {

ExecutionImplAndroid::ExecutionImplAndroid(
    const CompilationImplAndroid* compilation,
    mojo::ScopedSharedBufferHandle memory)
    : operands_(compilation->operands_),
      operations_(compilation->operations_),
      inputs_(compilation->inputs_),
      outputs_(compilation->outputs_),
      memory_(std::move(memory)) {
  uint32_t total_length = 0;
  inputs_info_.reserve(inputs_.size());
  for (size_t i = 0; i < inputs_.size(); ++i) {
    uint32_t offset = total_length;
    uint32_t length = operands_[inputs_[i]].requiredSize();

    inputs_info_.push_back(std::make_unique<OperandInfo>(
        offset, length, memory_->MapAtOffset(length, offset)));
    total_length += length;
  }

  outputs_info_.reserve(outputs_.size());
  for (size_t i = 0; i < outputs_.size(); ++i) {
    uint32_t offset = total_length;
    uint32_t length = operands_[outputs_[i]].requiredSize();

    outputs_info_.push_back(std::make_unique<OperandInfo>(
        offset, length, memory_->MapAtOffset(length, offset)));
    total_length += length;
  }

  int32_t result = ANeuralNetworksExecution_create(compilation->nn_compilation_,
                                                   &nn_execution_);
  DLOG(INFO) << "ANeuralNetworksExecution_create: " << result;

  for (size_t i = 0; i < inputs_info_.size(); ++i) {
    std::unique_ptr<OperandInfo>& info = inputs_info_[i];
    result = ANeuralNetworksExecution_setInput(
        nn_execution_, i, NULL, static_cast<void*>(info->mapping.get()),
        info->length);

    DLOG(INFO) << "ANeuralNetworksExecution_setInput: " << result;
  }

  for (size_t i = 0; i < outputs_info_.size(); ++i) {
    std::unique_ptr<OperandInfo>& info = outputs_info_[i];
    result = ANeuralNetworksExecution_setOutput(
        nn_execution_, i, NULL, static_cast<void*>(info->mapping.get()),
        info->length);

    DLOG(INFO) << "ANeuralNetworksExecution_setOutput: " << result;
  }
}

ExecutionImplAndroid::~ExecutionImplAndroid() {
  ANeuralNetworksExecution_free(nn_execution_);
  DLOG(INFO) << "ANeuralNetworksExecution_free";
}

void ExecutionImplAndroid::StartCompute(StartComputeCallback callback) {
  DLOG(INFO) << "ExecutionImplAndroid::StartCompute";

  ANeuralNetworksEvent* nn_event;
  int32_t result =
      ANeuralNetworksExecution_startCompute(nn_execution_, &nn_event);
  LOG(INFO) << "ANeuralNetworksExecution_startCompute: " << result;
  result = ANeuralNetworksEvent_wait(nn_event);
  LOG(INFO) << "ANeuralNetworksEvent_wait: " << result;
  ANeuralNetworksEvent_free(nn_event);

  std::move(callback).Run(mojom::NOT_ERROR);
}

}  // namespace ml
