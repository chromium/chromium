// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_activation.h"

#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"

namespace blink {

namespace {

webnn::mojom::blink::Operation::Tag ActivationKindToOperationKind(
    webnn::mojom::blink::Activation::Tag kind) {
  switch (kind) {
    case webnn::mojom::blink::Activation::Tag::kClamp:
      return webnn::mojom::blink::Operation::Tag::kClamp;
    case webnn::mojom::blink::Activation::Tag::kElu:
      return webnn::mojom::blink::Operation::Tag::kElu;
    case webnn::mojom::blink::Activation::Tag::kHardSigmoid:
      return webnn::mojom::blink::Operation::Tag::kHardSigmoid;
    case webnn::mojom::blink::Activation::Tag::kLeakyRelu:
      return webnn::mojom::blink::Operation::Tag::kLeakyRelu;
    case webnn::mojom::blink::Activation::Tag::kLinear:
      return webnn::mojom::blink::Operation::Tag::kLinear;
    case webnn::mojom::blink::Activation::Tag::kRelu:
      return webnn::mojom::blink::Operation::Tag::kRelu;
    case webnn::mojom::blink::Activation::Tag::kSigmoid:
      return webnn::mojom::blink::Operation::Tag::kSigmoid;
    case webnn::mojom::blink::Activation::Tag::kSoftmax:
      return webnn::mojom::blink::Operation::Tag::kSoftmax;
    case webnn::mojom::blink::Activation::Tag::kSoftplus:
      return webnn::mojom::blink::Operation::Tag::kSoftplus;
    case webnn::mojom::blink::Activation::Tag::kSoftsign:
      return webnn::mojom::blink::Operation::Tag::kSoftsign;
    case webnn::mojom::blink::Activation::Tag::kTanh:
      return webnn::mojom::blink::Operation::Tag::kTanh;
  }
}

}  // namespace

// static
String MLActivation::ActivationKindToString(
    webnn::mojom::blink::Activation::Tag kind) {
  return MLOperator::OperatorKindToString(ActivationKindToOperationKind(kind));
}

MLActivation::MLActivation(MLGraphBuilder* builder,
                           webnn::mojom::blink::Activation::Tag kind,
                           const bindings::DictionaryBase* options)
    : operator_(
          MakeGarbageCollected<MLOperator>(builder,
                                           ActivationKindToOperationKind(kind),
                                           /*sub_kind=*/absl::monostate{},
                                           options)),
      kind_(kind) {}

MLActivation::~MLActivation() = default;

void MLActivation::Trace(Visitor* visitor) const {
  visitor->Trace(operator_);
  ScriptWrappable::Trace(visitor);
}

const MLOperator* MLActivation::Operator() const {
  DCHECK(operator_);
  return operator_.Get();
}

webnn::mojom::blink::Activation::Tag MLActivation::Kind() const {
  return kind_;
}

}  // namespace blink
