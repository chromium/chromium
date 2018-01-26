// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/execution_impl_linux.h"

namespace ml {

ExecutionImplLinux::ExecutionImplLinux(CompilationImplLinux* compilation) {
  operands_ = compilation->operands_;
  operations_ = compilation->operations_;
  inputs_ = compilation->inputs_;
  outputs_ = compilation->outputs_;
}

ExecutionImplLinux::~ExecutionImplLinux() {}

void ExecutionImplLinux::setInput(uint32_t index, mojo::ScopedSharedBufferHandle buffer, uint32_t length, setInputCallback callback) {
  LOG(INFO) << "ExecutionImplLinux::setInput";
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
  std::move(callback).Run(mojom::NO_ERROR);
}

void ExecutionImplLinux::setOutput(uint32_t index, mojo::ScopedSharedBufferHandle buffer, uint32_t length, setOutputCallback callback) {
  LOG(INFO) << "ExecutionImplLinux::setOutput";
  LOG(INFO) << "  " << "index: " << index;
  LOG(INFO) << "  " << "length: " << length;
  std::move(callback).Run(mojom::NO_ERROR);
}

void ExecutionImplLinux::startCompute(startComputeCallback callback) {
  LOG(INFO) << "ExecutionImplLinux::startCompute";
  std::move(callback).Run(mojom::NO_ERROR);
}

}  // namespace ml
