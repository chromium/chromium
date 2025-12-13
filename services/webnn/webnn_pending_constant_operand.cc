// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_pending_constant_operand.h"

#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/webnn_constant_operand.h"

namespace webnn {

WebNNPendingConstantOperand::WebNNPendingConstantOperand(
    blink::WebNNPendingConstantToken handle,
    OperandDataType data_type,
    base::span<const uint8_t> data)
    : handle_(std::move(handle)),
      data_type_(data_type),
      data_(base::HeapArray<uint8_t>::CopiedFrom(data)) {}

WebNNPendingConstantOperand::~WebNNPendingConstantOperand() = default;

std::unique_ptr<WebNNConstantOperand>
WebNNPendingConstantOperand::TakeAsConstantOperand(
    OperandDescriptor descriptor) {
  // If `data_` has moved, `TakeAsConstantOperand()` has already been called
  // previously
  CHECK(data_.data());

  if (!IsValidWithDescriptor(descriptor)) {
    return nullptr;
  }

  return std::make_unique<WebNNConstantOperand>(std::move(descriptor),
                                                std::move(data_));
}

bool WebNNPendingConstantOperand::IsValidWithDescriptor(
    OperandDescriptor descriptor) const {
  return data_.size() == descriptor.PackedByteLength() &&
         data_type_ == descriptor.data_type();
}

}  // namespace webnn
