// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"

namespace blink {

// static
String MLOperator::OperatorKindToString(MLOperator::OperatorKind kind) {
  switch (kind) {
    case MLOperator::OperatorKind::kClamp:
      return "clamp";
    case MLOperator::OperatorKind::kConcat:
      return "concat";
    case MLOperator::OperatorKind::kConv2d:
      return "conv2d";
    case MLOperator::OperatorKind::kConvTranspose2d:
      return "convTranspose2d";
    case MLOperator::OperatorKind::kAdd:
      return "add";
    case MLOperator::OperatorKind::kSub:
      return "sub";
    case MLOperator::OperatorKind::kMul:
      return "mul";
    case MLOperator::OperatorKind::kDiv:
      return "div";
    case MLOperator::OperatorKind::kLeakyRelu:
      return "leakyRelu";
    case MLOperator::OperatorKind::kMax:
      return "max";
    case MLOperator::OperatorKind::kMin:
      return "min";
    case MLOperator::OperatorKind::kElu:
      return "elu";
    case MLOperator::OperatorKind::kGemm:
      return "gemm";
    case MLOperator::OperatorKind::kHardSwish:
      return "hardSwish";
    case MLOperator::OperatorKind::kAveragePool2d:
      return "averagePool2d";
    case MLOperator::OperatorKind::kMaxPool2d:
      return "maxPool2d";
    case MLOperator::OperatorKind::kPad:
      return "pad";
    case MLOperator::OperatorKind::kPRelu:
      return "prelu";
    case MLOperator::OperatorKind::kRelu:
      return "relu";
    case MLOperator::OperatorKind::kReshape:
      return "reshape";
    case MLOperator::OperatorKind::kResample2d:
      return "resample2d";
    case MLOperator::OperatorKind::kSoftmax:
      return "softmax";
    case MLOperator::OperatorKind::kSigmoid:
      return "sigmoid";
    case MLOperator::OperatorKind::kTranspose:
      return "transpose";
  }
}

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
  DCHECK(!inputs.empty());
  DCHECK(!outputs.empty());
  inputs_ = std::move(inputs);
  outputs_ = std::move(outputs);
  is_connected_ = true;
}

MLPadOperator::MLPadOperator(MLGraphBuilder* builder,
                             const Vector<uint32_t>& beginning_padding,
                             const Vector<uint32_t>& ending_padding,
                             const bindings::DictionaryBase* options)
    : MLOperator(builder, MLOperator::OperatorKind::kPad, options),
      beginning_padding_(beginning_padding),
      ending_padding_(ending_padding) {}

MLPadOperator::~MLPadOperator() = default;

const Vector<uint32_t>& MLPadOperator::BeginningPadding() const {
  return beginning_padding_;
}

const Vector<uint32_t>& MLPadOperator::EndingPadding() const {
  return ending_padding_;
}
}  // namespace blink
