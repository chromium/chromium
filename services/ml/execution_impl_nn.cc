// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/execution_impl_nn.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/public/mojom/constants.mojom.h"

namespace ml {

// TODO:: CompilationImplNN* => std::unique<CompilationImplNN> so that 
// ie_compilation_free(ie_compilation_); can host in class CompilationImplNN.
ExecutionImplNN::ExecutionImplNN(
    const CompilationImplNN* compilation,
    mojo::ScopedSharedBufferHandle memory)
    : operands_(compilation->operands_),
      operations_(compilation->operations_),
      inputs_(compilation->inputs_),
      outputs_(compilation->outputs_),
      memory_(std::move(memory)),
#if defined(OS_ANDROID)
      nn_compilation_(compilation->nn_compilation_) {
#else
      ie_compilation_(compilation->ie_compilation_) {
#endif
#if defined(OS_LINUX) || defined(OS_WIN)
  // Create Execution
  int32_t result = ie_execution_create(ie_compilation_, &ie_execution_);
#endif
  uint32_t total_length = 0;
  inputs_info_.reserve(inputs_.size());
  for (size_t i = 0; i < inputs_.size(); ++i) {
    uint32_t offset = total_length;
    uint32_t length = operands_[inputs_[i]].requiredSize();

    inputs_info_.push_back(std::make_unique<OperandInfo>(
        offset, length, memory_->MapAtOffset(length, offset)));
    total_length += length;
#if defined(OS_LINUX) || defined(OS_WIN)
    result = ie_execution_set_input(
            ie_execution_, inputs_[i], inputs_info_[i]->mapping.get(),
            length);
#endif
  }

  outputs_info_.reserve(outputs_.size());
  for (size_t i = 0; i < outputs_.size(); ++i) {
    uint32_t offset = total_length;
    uint32_t length = operands_[outputs_[i]].requiredSize();

    outputs_info_.push_back(std::make_unique<OperandInfo>(
        offset, length, memory_->MapAtOffset(length, offset)));
    total_length += length;
#if defined(OS_LINUX) || defined(OS_WIN)
    result = ie_execution_set_output(
            ie_execution_, outputs_[i], outputs_info_[i]->mapping.get(),
            length);
#endif
  }
}

ExecutionImplNN::~ExecutionImplNN() {
#if defined(OS_ANDROID)
  ANeuralNetworksCompilation_free(nn_compilation_);
#else
  ie_compilation_free(ie_compilation_);
  ie_execution_free(ie_execution_);
#endif
  DLOG(INFO) << "ANeuralNetworksCompilation_free";
}

void ExecutionImplNN::StartCompute(StartComputeCallback callback) {
  DLOG(INFO) << "ExecutionImplNN::StartCompute";

#if defined(OS_LINUX) || defined(OS_WIN)
  int32_t result = ie_execution_start_compute(ie_execution_);
  LOG(INFO) << "ie_execution_start_compute: " << result;
#else
  ANeuralNetworksExecution* nn_execution;
  int32_t result =
      ANeuralNetworksExecution_create(nn_compilation_, &nn_execution);
  DLOG(INFO) << "ANeuralNetworksExecution_create: " << result;
  for (size_t i = 0; i < inputs_info_.size(); ++i) {
    std::unique_ptr<OperandInfo>& info = inputs_info_[i];
    result = ANeuralNetworksExecution_setInput(
        nn_execution, i, NULL, static_cast<void*>(info->mapping.get()),
        info->length);
    DLOG(INFO) << "ANeuralNetworksExecution_setInput: " << result;
  }

  for (size_t i = 0; i < outputs_info_.size(); ++i) {
    std::unique_ptr<OperandInfo>& info = outputs_info_[i];
    result = ANeuralNetworksExecution_setOutput(
        nn_execution, i, NULL, static_cast<void*>(info->mapping.get()),
        info->length);
    DLOG(INFO) << "ANeuralNetworksExecution_setOutput: " << result;
  }

  ANeuralNetworksEvent* nn_event;
  result = ANeuralNetworksExecution_startCompute(nn_execution, &nn_event);
  LOG(INFO) << "ANeuralNetworksExecution_startCompute: " << result;
  result = ANeuralNetworksEvent_wait(nn_event);
  LOG(INFO) << "ANeuralNetworksEvent_wait: " << result;
  ANeuralNetworksEvent_free(nn_event);

  ANeuralNetworksExecution_free(nn_execution);
#endif

  std::move(callback).Run(mojom::NOT_ERROR);
}
}  // namespace ml
