// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_descriptor.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"

namespace blink {

// static
MLGraphBuilder* MLGraphBuilder::Create(MLContext* context) {
  return MakeGarbageCollected<MLGraphBuilder>(context);
}

MLGraphBuilder::MLGraphBuilder(MLContext* context) : ml_context_(context) {}

MLGraphBuilder::~MLGraphBuilder() = default;

void MLGraphBuilder::Trace(Visitor* visitor) const {
  visitor->Trace(ml_context_);
  ScriptWrappable::Trace(visitor);
}

MLOperand* MLGraphBuilder::input(String name, const MLOperandDescriptor* desc) {
  return MakeGarbageCollected<MLOperand>(GetContext());
}

MLOperand* MLGraphBuilder::constant(const MLOperandDescriptor* desc,
                                    NotShared<DOMArrayBufferView> buffer_view) {
  return MakeGarbageCollected<MLOperand>(GetContext());
}

MLOperand* MLGraphBuilder::add(const MLOperand* a, const MLOperand* b) {
  return MakeGarbageCollected<MLOperand>(GetContext());
}

MLContext* MLGraphBuilder::GetContext() const {
  return ml_context_.Get();
}

}  // namespace blink
