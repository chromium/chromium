// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_PENDING_CONSTANT_OPERAND_H_
#define SERVICES_WEBNN_WEBNN_PENDING_CONSTANT_OPERAND_H_

#include "base/component_export.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/webnn_object_impl.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace webnn {

class WebNNConstantOperand;

// Manages the data associated with an `MLConstantOperand` which has been built
// by an `MLGraphBuilder` but not yet been included in an `MLGraph`. Notably,
// this class does not include a shape since the shape of the constant data will
// not be known until after constant folding optimizations have been performed.
//
// An instance of this class is owned by a `WebNNGraphBuilderImpl` while the
// graph is being built, and then will either be:
//  - destroyed, if graph-building fails or the resulting graph does not include
//    this constant operand, or
//  - converted into a `WebNNConstantOperand`, otherwise.
//
// TODO(crbug.com/349428379): Consider allowing this class to be extended by
// backend-specific implementations, which can stream the constant data into the
// form needed by the backend.
class COMPONENT_EXPORT(WEBNN_SERVICE) WebNNPendingConstantOperand
    : public WebNNObjectImpl<blink::WebNNPendingConstantToken> {
 public:
  // Create a constant operand from bytes with an unknown shape.
  WebNNPendingConstantOperand(blink::WebNNPendingConstantToken handle,
                              OperandDataType data_type,
                              base::span<const uint8_t> data);

  ~WebNNPendingConstantOperand() override;

  WebNNPendingConstantOperand(const WebNNPendingConstantOperand&) = delete;
  WebNNPendingConstantOperand& operator=(const WebNNPendingConstantOperand&) =
      delete;

  // Vend a real operand by giving this pending operand a concrete shape.
  // Returns `nullptr` if `descriptor` is not compatible with this.
  std::unique_ptr<WebNNConstantOperand> TakeAsConstantOperand(
      OperandDescriptor descriptor);

  bool IsValidWithDescriptor(OperandDescriptor descriptor) const;

 private:
  const OperandDataType data_type_;

  base::HeapArray<uint8_t> data_;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_PENDING_CONSTANT_OPERAND_H_
