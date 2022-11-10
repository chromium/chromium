// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"

namespace blink {

namespace {

size_t GetBytesPerElement(V8MLOperandType::Enum operand_type) {
  switch (operand_type) {
    case V8MLOperandType::Enum::kFloat32:
      return sizeof(float);
    case V8MLOperandType::Enum::kFloat16:
      // Using Uint16Array for float16 is a workaround of WebNN spec issue:
      // https://github.com/webmachinelearning/webnn/issues/127
      return sizeof(uint16_t);
    case V8MLOperandType::Enum::kInt32:
      return sizeof(int32_t);
    case V8MLOperandType::Enum::kUint32:
      return sizeof(uint32_t);
    case V8MLOperandType::Enum::kInt8:
      return sizeof(int8_t);
    case V8MLOperandType::Enum::kUint8:
      return sizeof(uint8_t);
  }
}

absl::optional<size_t> ValidateAndCalculateElementsNumber(
    const Vector<uint32_t>& dimensions,
    String& error_message) {
  if (dimensions.empty()) {
    error_message = "The dimensions is empty.";
    return absl::nullopt;
  }
  base::CheckedNumeric<size_t> checked_number_of_elements = 1;
  for (auto& d : dimensions) {
    if (d == 0) {
      error_message = "All dimensions should be positive.";
      return absl::nullopt;
    }
    checked_number_of_elements *= d;
  }
  if (!checked_number_of_elements.IsValid()) {
    error_message = "The number of elements is too large.";
    return absl::nullopt;
  }
  return checked_number_of_elements.ValueOrDie();
}

absl::optional<size_t> ValidateAndCalculateByteLength(
    V8MLOperandType::Enum type,
    const Vector<uint32_t>& dimensions,
    String& error_message) {
  absl::optional<size_t> elements_num =
      ValidateAndCalculateElementsNumber(dimensions, error_message);
  if (!elements_num) {
    return absl::nullopt;
  }
  auto checked_byte_length =
      base::MakeCheckedNum<size_t>(elements_num.value()) *
      GetBytesPerElement(type);
  if (!checked_byte_length.IsValid()) {
    error_message = "The byte length is too large.";
    return absl::nullopt;
  }
  return checked_byte_length.ValueOrDie();
}

}  // namespace

DOMArrayBufferView::ViewType GetArrayBufferViewType(
    V8MLOperandType::Enum operand_type) {
  switch (operand_type) {
    case V8MLOperandType::Enum::kFloat32:
      return DOMArrayBufferView::ViewType::kTypeFloat32;
    case V8MLOperandType::Enum::kFloat16:
      // Using Uint16Array for float16 is a workaround of WebNN spec issue:
      // https://github.com/webmachinelearning/webnn/issues/127
      return DOMArrayBufferView::ViewType::kTypeUint16;
    case V8MLOperandType::Enum::kInt32:
      return DOMArrayBufferView::ViewType::kTypeInt32;
    case V8MLOperandType::Enum::kUint32:
      return DOMArrayBufferView::ViewType::kTypeUint32;
    case V8MLOperandType::Enum::kInt8:
      return DOMArrayBufferView::ViewType::kTypeInt8;
    case V8MLOperandType::Enum::kUint8:
      return DOMArrayBufferView::ViewType::kTypeUint8;
  }
}

// static
MLOperand* MLOperand::ValidateAndCreateInput(MLGraphBuilder* builder,
                                             const V8MLOperandType::Enum type,
                                             Vector<uint32_t> dimensions,
                                             String name,
                                             String& error_message) {
  if (name.empty()) {
    error_message = "The name is empty.";
    return nullptr;
  }
  if (!ValidateAndCalculateByteLength(type, dimensions, error_message)) {
    error_message = "Invalid operand descriptor: " + error_message;
    return nullptr;
  }
  auto* input = MakeGarbageCollected<MLOperand>(builder, OperandKind::kInput,
                                                type, std::move(dimensions));
  input->name_ = std::move(name);
  return input;
}

// static
MLOperand* MLOperand::ValidateAndCreateConstant(
    MLGraphBuilder* builder,
    const V8MLOperandType::Enum type,
    Vector<uint32_t> dimensions,
    const DOMArrayBufferView* array_buffer_view,
    String& error_message) {
  DCHECK(array_buffer_view);
  if (GetArrayBufferViewType(type) != array_buffer_view->GetType()) {
    error_message = "The buffer view type doesn't match the operand type.";
    return nullptr;
  }
  absl::optional<size_t> expected_byte_length =
      ValidateAndCalculateByteLength(type, dimensions, error_message);
  if (!expected_byte_length) {
    error_message = "Invalid operand descriptor: " + error_message;
    return nullptr;
  }
  if (expected_byte_length.value() != array_buffer_view->byteLength()) {
    error_message = String::Format(
        "The buffer view byte length (%zu) doesn't match the "
        "expected byte length (%zu).",
        array_buffer_view->byteLength(), expected_byte_length.value());
    return nullptr;
  }
  auto* constant = MakeGarbageCollected<MLOperand>(
      builder, OperandKind::kConstant, type, std::move(dimensions));
  constant->array_buffer_view_ = array_buffer_view;
  return constant;
}

// static
MLOperand* MLOperand::ValidateAndCreateOutput(MLGraphBuilder* builder,
                                              const V8MLOperandType::Enum type,
                                              Vector<uint32_t> dimensions,
                                              const MLOperator* ml_operator,
                                              String& error_message) {
  DCHECK(ml_operator);
  if (!ValidateAndCalculateByteLength(type, dimensions, error_message)) {
    error_message = "Invalid output operand: " + error_message;
    return nullptr;
  }
  auto* output = MakeGarbageCollected<MLOperand>(builder, OperandKind::kOutput,
                                                 type, std::move(dimensions));
  output->operator_ = ml_operator;
  return output;
}

MLOperand::MLOperand(MLGraphBuilder* builder,
                     OperandKind kind,
                     const V8MLOperandType::Enum type,
                     Vector<uint32_t> dimensions)
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

const Vector<uint32_t>& MLOperand::Dimensions() const {
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

size_t MLOperand::NumberOfElements() const {
  String error_message;
  auto elements_number =
      ValidateAndCalculateElementsNumber(dimensions_, error_message);
  DCHECK(elements_number);
  return elements_number.value();
}

size_t MLOperand::ByteLength() const {
  String error_message;
  auto byte_length =
      ValidateAndCalculateByteLength(type_, dimensions_, error_message);
  DCHECK(byte_length);
  return byte_length.value();
}

void MLOperand::Trace(Visitor* visitor) const {
  visitor->Trace(builder_);
  visitor->Trace(array_buffer_view_);
  visitor->Trace(operator_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
