// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/execution_impl_linux.h"

namespace ml {

ExecutionImplLinux::ExecutionImplLinux(CompilationImplLinux* compilation, mojo::ScopedSharedBufferHandle memory) {
  operands_ = compilation->operands_;
  operations_ = compilation->operations_;
  inputs_ = compilation->inputs_;
  outputs_ = compilation->outputs_;

  memory_ = std::move(memory);
  uint32_t total_length = 0;
  for (size_t i = 0; i < inputs_.size(); ++i) {
    Operand operand = operands_[inputs_[i]];
    uint32_t offset = total_length;
    uint32_t length = operand.requiredSize();
    mojo::ScopedSharedBufferMapping mapping = memory_->MapAtOffset(length, offset);
    std::unique_ptr<OperandInfo> info(new OperandInfo(offset, length, std::move(mapping)));
    inputs_info_.push_back(std::move(info));
    total_length += length;
  }
  for (size_t i = 0; i < outputs_.size(); ++i) {
    Operand operand = operands_[outputs_[i]];
    uint32_t offset = total_length;
    uint32_t length = operand.requiredSize();
    mojo::ScopedSharedBufferMapping mapping = memory_->MapAtOffset(length, offset);
    std::unique_ptr<OperandInfo> info(new OperandInfo(offset, length, std::move(mapping)));
    outputs_info_.push_back(std::move(info));
    total_length += length;
  }
}

ExecutionImplLinux::~ExecutionImplLinux() {}

void ExecutionImplLinux::startCompute(startComputeCallback callback) {
  DLOG(INFO) << "ExecutionImplLinux::startCompute";

  for (size_t i = 0; i < inputs_.size(); ++i) {
    DLOG(INFO) << "inputs[" << i << "]:";
    auto operand = operands_[inputs_[i]];
    std::unique_ptr<OperandInfo>& info = inputs_info_[i];
    PrintOperand(operand, info);
  }
  for (size_t i = 0; i < outputs_.size(); ++i) {
    std::unique_ptr<OperandInfo>& info = outputs_info_[i];
    DLOG(INFO) << "outputs[" << i << "]: length " << info->length;
    memset(static_cast<void*>(info->mapping.get()), 1, info->length);
  }
  for (size_t i = 0; i < outputs_.size(); ++i) {
    DLOG(INFO) << "outputs[" << i << "]:";
    auto operand = operands_[outputs_[i]];
    std::unique_ptr<OperandInfo>& info = outputs_info_[i];
    PrintOperand(operand, info);
  }
  std::move(callback).Run(mojom::NO_ERROR);
}

}  // namespace ml
