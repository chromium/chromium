// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_OPERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_OPERATOR_H_

#include "third_party/blink/renderer/platform/bindings/dictionary_base.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class MLGraphBuilder;
class MLOperand;

class MLOperator final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum OperatorKind {
    // Keep the order as the same as build methods of MLGraphBuilder.
    kClamp
  };
  MLOperator(MLGraphBuilder* builder,
             OperatorKind kind,
             const bindings::DictionaryBase* options = nullptr);

  MLOperator(const MLOperator&) = delete;
  MLOperator& operator=(const MLOperator&) = delete;

  ~MLOperator() override;

  void Trace(Visitor* visitor) const override;

  OperatorKind Kind() const;
  const bindings::DictionaryBase* Options() const;
  bool IsConnected() const;
  const HeapVector<Member<const MLOperand>>& Inputs() const;
  const HeapVector<Member<const MLOperand>>& Outputs() const;

  // According to WebNN programming model
  // https://www.w3.org/TR/webnn/#programming-model, neural networks are
  // represented as computational graphs of mathematical operators (nodes)
  // connected by operands (edges). This method connects the operator with its
  // input and output operands during a computational graph building session. An
  // operator is only allowed to be connected once.
  void Connect(HeapVector<Member<const MLOperand>> inputs,
               HeapVector<Member<const MLOperand>> outputs);

 private:
  Member<MLGraphBuilder> builder_;
  OperatorKind kind_;
  // The correct type of options_ depends on OperatorKind. For example, if the
  // OperatorKind is kClamp, options_ could static_cast to MLClampOptions.
  Member<const bindings::DictionaryBase> options_;
  // is_conneted_ indicates whether the operator is connected with operands.
  // According to WebNN spec https://www.w3.org/TR/webnn/#api-mloperator, an
  // operator without operand connections could be used as an activation
  // function that is fused into another operator.
  bool is_connected_{false};
  HeapVector<Member<const MLOperand>> inputs_;
  HeapVector<Member<const MLOperand>> outputs_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_OPERATOR_H_
