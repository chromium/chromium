// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_constant_operand.h"

#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"

namespace blink {

MLConstantOperand::MLConstantOperand(MLGraphBuilder* builder,
                                     webnn::OperandDescriptor descriptor)
    : MLOperand(builder,
                webnn::mojom::blink::Operand::Kind::kConstant,
                std::move(descriptor)) {}

MLConstantOperand::~MLConstantOperand() = default;

void MLConstantOperand::Trace(Visitor* visitor) const {
  MLOperand::Trace(visitor);
}

}  // namespace blink
