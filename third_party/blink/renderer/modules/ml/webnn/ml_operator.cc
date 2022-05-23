// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"

namespace blink {

MLOperator::MLOperator(MLGraphBuilder* graph_builder)
    : graph_builder_(graph_builder) {}

MLOperator::~MLOperator() = default;

void MLOperator::Trace(Visitor* visitor) const {
  visitor->Trace(graph_builder_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
