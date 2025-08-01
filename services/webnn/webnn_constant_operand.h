// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_CONSTANT_OPERAND_H_
#define SERVICES_WEBNN_WEBNN_CONSTANT_OPERAND_H_

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "services/webnn/public/cpp/operand_descriptor.h"

namespace webnn {

// Manages the data associated with an `MLConstantOperand`. Instances of this
// class are generally created from a `WebNNPendingConstantOperand`.
//
// TODO(crbug.com/349428379): Consider allowing this class to be extended by
// backend-specific implementations, which can stream the constant data into the
// form needed by the backend.
class COMPONENT_EXPORT(WEBNN_SERVICE) WebNNConstantOperand {
 public:
  // Create a constant operand from an existing HeapArray.
  WebNNConstantOperand(OperandDescriptor descriptor,
                       base::HeapArray<uint8_t> data);

  ~WebNNConstantOperand();

  WebNNConstantOperand(const WebNNConstantOperand&) = delete;
  WebNNConstantOperand& operator=(const WebNNConstantOperand&) = delete;

  const OperandDescriptor& descriptor() const { return descriptor_; }

  // TODO(crbug.com/349428379): Consider instead providing a backend-specific
  // accessor.
  base::span<const uint8_t> ByteSpan() const LIFETIME_BOUND { return data_; }

  base::HeapArray<uint8_t> TakeData() { return std::move(data_); }
  void SetData(base::HeapArray<uint8_t>&& data);

 private:
  const OperandDescriptor descriptor_;
  base::HeapArray<uint8_t> data_;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_CONSTANT_OPERAND_H_
