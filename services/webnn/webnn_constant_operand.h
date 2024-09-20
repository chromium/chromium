// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_CONSTANT_OPERAND_H_
#define SERVICES_WEBNN_WEBNN_CONSTANT_OPERAND_H_

#include "base/component_export.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "services/webnn/public/cpp/operand_descriptor.h"

namespace webnn {

// Manages the data associated with an `MLConstantOperand`. An instance of this
// class is owned by a `WebNNGraphBuilderImpl` while the graph is being built,
// and will be destroyed once building the graph succeeds.
//
// TODO(crbug.com/349428379): Consider allowing this class to be extended by
// backend-specific implementations, which can stream the constant data into the
// form needed by the backend.
class COMPONENT_EXPORT(WEBNN_UTILS) WebNNConstantOperand {
 public:
  // Create a constant operand from bytes.
  WebNNConstantOperand(OperandDescriptor descriptor,
                       base::span<const uint8_t> data);

  // Create a constant operand from an existing HeapArray.
  WebNNConstantOperand(OperandDescriptor descriptor,
                       base::HeapArray<uint8_t> data);

  ~WebNNConstantOperand();

  WebNNConstantOperand(const WebNNConstantOperand&) = delete;
  WebNNConstantOperand& operator=(const WebNNConstantOperand&) = delete;

  const OperandDescriptor& descriptor() const { return descriptor_; }

  // TODO(crbug.com/349428379): Consider instead providing a backend-specific
  // accessor.
  base::span<const uint8_t> ByteSpan() const { return data_; }

 private:
  const OperandDescriptor descriptor_;
  const base::HeapArray<uint8_t> data_;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_CONSTANT_OPERAND_H_
