// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_constant_operand.h"

#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"

namespace blink {

MLConstantOperand::MLConstantOperand(MLGraphBuilder* builder,
                                     webnn::OperandDescriptor descriptor,
                                     base::span<const uint8_t> bytes)
    : MLOperand(builder,
                webnn::mojom::blink::Operand::Kind::kConstant,
                std::move(descriptor)),
      constant_bytes_(base::HeapArray<uint8_t>::CopiedFrom(bytes)) {
  CHECK_EQ(descriptor_.PackedByteLength(), constant_bytes_.size());
}

MLConstantOperand::~MLConstantOperand() = default;

base::span<const uint8_t> MLConstantOperand::Bytes() const {
  return constant_bytes_;
}

void MLConstantOperand::ReleaseBytes() {
  constant_bytes_ = base::HeapArray<uint8_t>();
}

void MLConstantOperand::Trace(Visitor* visitor) const {
  MLOperand::Trace(visitor);
}

}  // namespace blink
