// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_activation.h"

namespace blink {

MLActivation::MLActivation(MLGraphBuilder* builder,
                           MLOperator::OperatorKind kind,
                           const bindings::DictionaryBase* options)
    : operator_(MakeGarbageCollected<MLOperator>(builder, kind, options)) {}

MLActivation::~MLActivation() = default;

void MLActivation::Trace(Visitor* visitor) const {
  visitor->Trace(operator_);
  ScriptWrappable::Trace(visitor);
}

const MLOperator* MLActivation::Operator() const {
  DCHECK(operator_);
  return operator_;
}

}  // namespace blink
