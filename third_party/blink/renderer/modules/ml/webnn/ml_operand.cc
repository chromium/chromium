// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"

namespace blink {

MLOperand::MLOperand(MLGraphBuilder* graph_builder)
    : graph_builder_(graph_builder) {}

MLOperand::~MLOperand() = default;

void MLOperand::Trace(Visitor* visitor) const {
  visitor->Trace(graph_builder_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
