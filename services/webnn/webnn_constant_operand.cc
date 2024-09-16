// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_constant_operand.h"

namespace webnn {

WebNNConstantOperand::WebNNConstantOperand(OperandDescriptor descriptor,
                                           base::span<const uint8_t> data)
    : descriptor_(std::move(descriptor)),
      data_(base::HeapArray<uint8_t>::CopiedFrom(data)) {
  CHECK_EQ(data_.size(), descriptor_.PackedByteLength());
}

WebNNConstantOperand::WebNNConstantOperand(OperandDescriptor descriptor,
                                           base::HeapArray<uint8_t> data)
    : descriptor_(std::move(descriptor)), data_(std::move(data)) {
  CHECK_EQ(data_.size(), descriptor_.PackedByteLength());
}

WebNNConstantOperand::~WebNNConstantOperand() = default;

}  // namespace webnn
