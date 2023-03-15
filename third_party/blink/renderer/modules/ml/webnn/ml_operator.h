// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_OPERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_OPERATOR_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/dictionary_base.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class MLGraphBuilder;
class MLOperand;

class MODULES_EXPORT MLOperator final : public GarbageCollected<MLOperator> {
 public:
  enum class OperatorKind {
    // Keep the order as the same as build methods of MLGraphBuilder.
    kClamp,
    kConcat,
    kConv2d,
    kAdd,
    kSub,
    kMul,
    kDiv,
    kLeakyRelu,
    kMax,
    kMin,
    kGemm,
    kHardSwish,
    kAveragePool2d,
    kMaxPool2d,
    kRelu,
    kReshape,
    kResample2d,
    kSoftmax,
    kSigmoid,
    kTranspose
  };

  static String OperatorKindToString(MLOperator::OperatorKind kind);

  // It is safe for a caller, usually a MLGraphBuidler operation build method,
  // that passes the reference of the options dictionary argument received from
  // Blink to MLOperator constructor and stores it in this object. This is
  // because that WebIDL spec (https://webidl.spec.whatwg.org/#idl-dictionaries)
  // mentiones that "an operation that accepts a dictionary as an argument will
  // perform a one-time conversion from the given ECMAScript value into the
  // dictionary, based on the current properties of the ECMAScript object.
  // Modifications to the dictionary will not be reflected in the corresponding
  // ECMAScript object, and vice-versa". Blink code generator follows the spec
  // and does a deep-copy of the members of an options dictionary, e.g.,
  // MLConv2dOptions::FillMembersFromV8Object, before passing it to a
  // MLGraphBuilder operation build method.
  MLOperator(MLGraphBuilder* builder,
             OperatorKind kind,
             const bindings::DictionaryBase* options = nullptr);

  MLOperator(const MLOperator&) = delete;
  MLOperator& operator=(const MLOperator&) = delete;

  ~MLOperator();

  void Trace(Visitor* visitor) const;

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
  // An operator without operand connections could be used by an MLActivation
  // to represent an activation function that is fused into another operator.
  bool is_connected_{false};
  HeapVector<Member<const MLOperand>> inputs_;
  HeapVector<Member<const MLOperand>> outputs_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_OPERATOR_H_
