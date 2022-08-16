// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"

namespace blink {

MLOperator::MLOperator(MLGraphBuilder* builder,
                       OperatorKind kind,
                       const bindings::DictionaryBase* options)
    : builder_(builder), kind_(kind), options_(options) {}

MLOperator::~MLOperator() = default;

void MLOperator::Trace(Visitor* visitor) const {
  visitor->Trace(builder_);
  visitor->Trace(options_);
  visitor->Trace(inputs_);
  visitor->Trace(outputs_);
  ScriptWrappable::Trace(visitor);
}

MLOperator::OperatorKind MLOperator::Kind() const {
  return kind_;
}

const bindings::DictionaryBase* MLOperator::Options() const {
  return options_;
}

bool MLOperator::IsConnected() const {
  return is_connected_;
}

const HeapVector<Member<const MLOperand>>& MLOperator::Inputs() const {
  return inputs_;
}

const HeapVector<Member<const MLOperand>>& MLOperator::Outputs() const {
  return outputs_;
}

void MLOperator::Connect(HeapVector<Member<const MLOperand>> inputs,
                         HeapVector<Member<const MLOperand>> outputs) {
  DCHECK(!is_connected_);
  DCHECK(!inputs.IsEmpty());
  DCHECK(!outputs.IsEmpty());
  inputs_ = std::move(inputs);
  outputs_ = std::move(outputs);
  is_connected_ = true;
}

}  // namespace blink
