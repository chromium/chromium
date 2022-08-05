// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"

namespace blink {

// static
MLOperand* MLOperand::CreateInput(MLGraphBuilder* builder,
                                  const V8MLOperandType::Enum type,
                                  Vector<int32_t> dimensions,
                                  String name) {
  auto* input = MakeGarbageCollected<MLOperand>(builder, OperandKind::kInput,
                                                type, std::move(dimensions));
  input->name_ = std::move(name);
  return input;
}

// static
MLOperand* MLOperand::CreateConstant(
    MLGraphBuilder* builder,
    const V8MLOperandType::Enum type,
    Vector<int32_t> dimensions,
    const DOMArrayBufferView* array_buffer_view) {
  auto* constant = MakeGarbageCollected<MLOperand>(
      builder, OperandKind::kConstant, type, std::move(dimensions));
  constant->array_buffer_view_ = array_buffer_view;
  return constant;
}

// static
MLOperand* MLOperand::CreateOutput(MLGraphBuilder* builder,
                                   const V8MLOperandType::Enum type,
                                   Vector<int32_t> dimensions,
                                   const MLOperator* ml_operator) {
  auto* output = MakeGarbageCollected<MLOperand>(builder, OperandKind::kOutput,
                                                 type, std::move(dimensions));
  output->operator_ = ml_operator;
  return output;
}

MLOperand::MLOperand(MLGraphBuilder* builder,
                     OperandKind kind,
                     const V8MLOperandType::Enum type,
                     Vector<int32_t> dimensions)
    : builder_(builder),
      kind_(kind),
      type_(type),
      dimensions_(std::move(dimensions)) {}

MLOperand::~MLOperand() = default;

MLGraphBuilder* MLOperand::Builder() const {
  return builder_.Get();
}

MLOperand::OperandKind MLOperand::Kind() const {
  return kind_;
}

V8MLOperandType::Enum MLOperand::Type() const {
  return type_;
}

const Vector<int32_t>& MLOperand::Dimensions() const {
  return dimensions_;
}

const String& MLOperand::Name() const {
  DCHECK_EQ(kind_, OperandKind::kInput);
  return name_;
}

const DOMArrayBufferView* MLOperand::ArrayBufferView() const {
  DCHECK_EQ(kind_, OperandKind::kConstant);
  return array_buffer_view_.Get();
}

const MLOperator* MLOperand::Operator() const {
  DCHECK_EQ(kind_, OperandKind::kOutput);
  return operator_.Get();
}

void MLOperand::Trace(Visitor* visitor) const {
  visitor->Trace(builder_);
  visitor->Trace(array_buffer_view_);
  visitor->Trace(operator_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
