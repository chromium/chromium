// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"

#include "components/ml/webnn/graph_validation_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"

namespace blink {

namespace {

size_t GetBytesPerElement(V8MLOperandDataType::Enum operand_type) {
  switch (operand_type) {
    case V8MLOperandDataType::Enum::kFloat32:
      return sizeof(float);
    case V8MLOperandDataType::Enum::kFloat16:
      // Using Uint16Array for float16 is a workaround of WebNN spec issue:
      // https://github.com/webmachinelearning/webnn/issues/127
      return sizeof(uint16_t);
    case V8MLOperandDataType::Enum::kInt32:
      return sizeof(int32_t);
    case V8MLOperandDataType::Enum::kUint32:
      return sizeof(uint32_t);
    case V8MLOperandDataType::Enum::kInt64:
      return sizeof(int64_t);
    case V8MLOperandDataType::Enum::kUint64:
      return sizeof(uint64_t);
    case V8MLOperandDataType::Enum::kInt8:
      return sizeof(int8_t);
    case V8MLOperandDataType::Enum::kUint8:
      return sizeof(uint8_t);
  }
}

base::expected<size_t, String> ValidateAndCalculateElementsNumber(
    const Vector<uint32_t>& dimensions) {
  auto number_of_elements =
      webnn::ValidateAndCalculateElementsNumber(base::make_span(dimensions));
  if (!number_of_elements.has_value()) {
    return base::unexpected(WTF::String::FromUTF8(number_of_elements.error()));
  }
  return number_of_elements.value();
}

base::expected<size_t, String> ValidateAndCalculateByteLength(
    V8MLOperandDataType::Enum data_type,
    const Vector<uint32_t>& dimensions) {
  auto byte_length = webnn::ValidateAndCalculateByteLength(
      GetBytesPerElement(data_type), base::make_span(dimensions));
  if (!byte_length.has_value()) {
    return base::unexpected(WTF::String::FromUTF8(byte_length.error()));
  }
  return byte_length.value();
}

}  // namespace

DOMArrayBufferView::ViewType GetArrayBufferViewType(
    V8MLOperandDataType::Enum operand_type) {
  switch (operand_type) {
    case V8MLOperandDataType::Enum::kFloat32:
      return DOMArrayBufferView::ViewType::kTypeFloat32;
    case V8MLOperandDataType::Enum::kFloat16:
      // Using Uint16Array for float16 is a workaround of WebNN spec issue:
      // https://github.com/webmachinelearning/webnn/issues/127
      return DOMArrayBufferView::ViewType::kTypeUint16;
    case V8MLOperandDataType::Enum::kInt32:
      return DOMArrayBufferView::ViewType::kTypeInt32;
    case V8MLOperandDataType::Enum::kUint32:
      return DOMArrayBufferView::ViewType::kTypeUint32;
    case V8MLOperandDataType::Enum::kInt64:
      return DOMArrayBufferView::ViewType::kTypeBigInt64;
    case V8MLOperandDataType::Enum::kUint64:
      return DOMArrayBufferView::ViewType::kTypeBigUint64;
    case V8MLOperandDataType::Enum::kInt8:
      return DOMArrayBufferView::ViewType::kTypeInt8;
    case V8MLOperandDataType::Enum::kUint8:
      return DOMArrayBufferView::ViewType::kTypeUint8;
  }
}

// static
base::expected<MLOperand*, String> MLOperand::ValidateAndCreateInput(
    MLGraphBuilder* builder,
    const V8MLOperandDataType::Enum data_type,
    Vector<uint32_t> dimensions,
    String name) {
  if (name.empty()) {
    return base::unexpected("The name is empty.");
  }
  auto result = ValidateAndCalculateByteLength(data_type, dimensions);
  if (!result.has_value()) {
    return base::unexpected("Invalid operand descriptor: " + result.error());
  }
  auto* input = MakeGarbageCollected<MLOperand>(
      builder, OperandKind::kInput, data_type, std::move(dimensions));
  input->name_ = std::move(name);
  return input;
}

// static
base::expected<MLOperand*, String> MLOperand::ValidateAndCreateConstant(
    MLGraphBuilder* builder,
    const V8MLOperandDataType::Enum data_type,
    Vector<uint32_t> dimensions,
    const DOMArrayBufferView* array_buffer_view) {
  DCHECK(array_buffer_view);
  if (GetArrayBufferViewType(data_type) != array_buffer_view->GetType()) {
    return base::unexpected(
        "The buffer view type doesn't match the operand data type.");
  }
  auto expected_byte_length =
      ValidateAndCalculateByteLength(data_type, dimensions);
  if (!expected_byte_length.has_value()) {
    return base::unexpected("Invalid operand descriptor: " +
                            expected_byte_length.error());
  }
  if (expected_byte_length.value() != array_buffer_view->byteLength()) {
    return base::unexpected(String::Format(
        "The buffer view byte length (%zu) doesn't match the "
        "expected byte length (%zu).",
        array_buffer_view->byteLength(), expected_byte_length.value()));
  }
  auto* constant = MakeGarbageCollected<MLOperand>(
      builder, OperandKind::kConstant, data_type, std::move(dimensions));
  constant->array_buffer_view_ = array_buffer_view;
  return constant;
}

// static
base::expected<MLOperand*, String> MLOperand::ValidateAndCreateOutput(
    MLGraphBuilder* builder,
    const V8MLOperandDataType::Enum data_type,
    Vector<uint32_t> dimensions,
    const MLOperator* ml_operator) {
  DCHECK(ml_operator);
  auto result = ValidateAndCalculateByteLength(data_type, dimensions);
  if (!result.has_value()) {
    return base::unexpected("Invalid output operand: " + result.error());
  }
  auto* output = MakeGarbageCollected<MLOperand>(
      builder, OperandKind::kOutput, data_type, std::move(dimensions));
  output->operator_ = ml_operator;
  return output;
}

MLOperand::MLOperand(MLGraphBuilder* builder,
                     OperandKind kind,
                     const V8MLOperandDataType::Enum data_type,
                     Vector<uint32_t> dimensions)
    : builder_(builder),
      kind_(kind),
      data_type_(data_type),
      dimensions_(std::move(dimensions)) {}

MLOperand::~MLOperand() = default;

MLGraphBuilder* MLOperand::Builder() const {
  return builder_.Get();
}

MLOperand::OperandKind MLOperand::Kind() const {
  return kind_;
}

V8MLOperandDataType::Enum MLOperand::DataType() const {
  return data_type_;
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
  auto elements_number = ValidateAndCalculateElementsNumber(dimensions_);
  DCHECK(elements_number.has_value());
  return elements_number.value();
}

size_t MLOperand::ByteLength() const {
  auto byte_length = ValidateAndCalculateByteLength(data_type_, dimensions_);
  DCHECK(byte_length.has_value());
  return byte_length.value();
}

Vector<uint32_t> MLOperand::shape() const {
  return dimensions_;
}

V8MLOperandDataType MLOperand::dataType() const {
  return V8MLOperandDataType(data_type_);
}

void MLOperand::Trace(Visitor* visitor) const {
  visitor->Trace(builder_);
  visitor->Trace(array_buffer_view_);
  visitor->Trace(operator_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
