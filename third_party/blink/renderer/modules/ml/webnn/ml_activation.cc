// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_activation.h"

#include <variant>

#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"

namespace blink {

MLActivation::MLActivation(MLGraphBuilder* builder,
                           webnn::mojom::blink::Operation::Tag kind,
                           const bindings::DictionaryBase* options)
    : operator_(MakeGarbageCollected<MLOperator>(builder,
                                                 kind,
                                                 /*sub_kind=*/std::monostate{},
                                                 options)) {}

MLActivation::~MLActivation() = default;

void MLActivation::Trace(Visitor* visitor) const {
  visitor->Trace(operator_);
  ScriptWrappable::Trace(visitor);
}

const MLOperator* MLActivation::Operator() const {
  DCHECK(operator_);
  return operator_.Get();
}

}  // namespace blink
