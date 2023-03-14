// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_ACTIVATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_ACTIVATION_H_

#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

// The MLActivation implementation contains an MLOperator that is not connected
// with any MLOperands. The activation function type and additional attributes
// are represented by the MLOperator's kind and options.
class MODULES_EXPORT MLActivation final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  MLActivation(MLGraphBuilder* builder,
               MLOperator::OperatorKind kind,
               const bindings::DictionaryBase* options = nullptr);

  MLActivation(const MLActivation&) = delete;
  MLActivation& operator=(const MLActivation&) = delete;

  ~MLActivation() override;

  void Trace(Visitor* visitor) const override;

  const MLOperator* Operator() const;

 private:
  // The `operator_` is immutable and always valid.
  const Member<const MLOperator> operator_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_ACTIVATION_H_
