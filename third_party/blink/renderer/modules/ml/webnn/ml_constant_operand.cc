// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_constant_operand.h"

#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_tensor.h"

namespace blink {

MLConstantOperand::MLConstantOperand(MLGraphBuilder* builder,
                                     webnn::OperandDescriptor descriptor)
    : MLOperand(builder,
                webnn::mojom::blink::Operand::Kind::kConstant,
                std::move(descriptor)) {}

MLConstantOperand::MLConstantOperand(MLGraphBuilder* builder,
                                     webnn::OperandDescriptor descriptor,
                                     WebNNPendingConstantToken handle)
    : MLOperand(builder,
                webnn::mojom::blink::Operand::Kind::kConstant,
                std::move(descriptor)),
      handle_(std::move(handle)) {}

MLConstantOperand::MLConstantOperand(MLGraphBuilder* builder, MLTensor* tensor)
    : MLOperand(builder,
                webnn::mojom::blink::Operand::Kind::kConstant,
                tensor->Descriptor()),
      tensor_(tensor) {}

MLConstantOperand::~MLConstantOperand() = default;

void MLConstantOperand::Trace(Visitor* visitor) const {
  visitor->Trace(tensor_);
  MLOperand::Trace(visitor);
}

void MLConstantOperand::SetPendingPermutation(
    base::span<const uint32_t> permutation) {
  descriptor_.SetPendingPermutation(permutation);
}

}  // namespace blink
