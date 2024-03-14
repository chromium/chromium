// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"

#include <functional>
#include <numeric>

#include "base/types/expected_macros.h"
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
base::expected<MLOperand::ValidatedDescriptor, String>
MLOperand::ValidatedDescriptor::Create(V8MLOperandDataType::Enum data_type,
                                       Vector<uint32_t> dimensions) {
  base::CheckedNumeric<size_t> checked_number_of_elements = 1;
  for (const uint32_t& dimension : dimensions) {
    if (dimension == 0) {
      // TODO: crbug.com/329471677 - Consider supporting size 0 dimensions.
      // See spec issue: https://github.com/webmachinelearning/webnn/issues/391.
      return base::unexpected(
          "Invalid operand descriptor: All dimensions should be positive.");
    }
    checked_number_of_elements *= dimension;
  }
  if (!checked_number_of_elements.IsValid()) {
    return base::unexpected(
        "Invalid operand descriptor: The number of elements is too large.");
  }

  // TODO: crbug.com/329482489 - Check the max rank of `dimensions`.
  // See spec issue: https://github.com/webmachinelearning/webnn/issues/456.

  base::CheckedNumeric<size_t> checked_byte_length =
      checked_number_of_elements * GetBytesPerElement(data_type);
  if (!checked_byte_length.IsValid()) {
    return base::unexpected(
        "Invalid operand descriptor: The byte length is too large.");
  }

  return ValidatedDescriptor(data_type, std::move(dimensions));
}

MLOperand::ValidatedDescriptor::ValidatedDescriptor(
    V8MLOperandDataType::Enum data_type,
    Vector<uint32_t> dimensions)
    : data_type_(data_type), dimensions_(std::move(dimensions)) {}

size_t MLOperand::ValidatedDescriptor::ByteLength() const {
  return NumberOfElements() * GetBytesPerElement(data_type_);
}

size_t MLOperand::ValidatedDescriptor::NumberOfElements() const {
  return std::accumulate(dimensions_.begin(), dimensions_.end(), 1u,
                         std::multiplies<size_t>());
}

// static
base::expected<MLOperand*, String> MLOperand::ValidateAndCreateInput(
    MLGraphBuilder* builder,
    V8MLOperandDataType::Enum data_type,
    Vector<uint32_t> dimensions,
    String name) {
  ASSIGN_OR_RETURN(MLOperand::ValidatedDescriptor validated_descriptor,
                   ValidatedDescriptor::Create(data_type, dimensions));

  if (name.empty()) {
    return base::unexpected("The name is empty.");
  }

  auto* input = MakeGarbageCollected<MLOperand>(
      builder, webnn::mojom::blink::Operand::Kind::kInput,
      std::move(validated_descriptor));
  input->name_ = std::move(name);
  return input;
}

// static
base::expected<MLOperand*, String> MLOperand::ValidateAndCreateConstant(
    MLGraphBuilder* builder,
    V8MLOperandDataType::Enum data_type,
    Vector<uint32_t> dimensions,
    const DOMArrayBufferView* array_buffer_view) {
  CHECK(array_buffer_view);

  ASSIGN_OR_RETURN(MLOperand::ValidatedDescriptor validated_descriptor,
                   ValidatedDescriptor::Create(data_type, dimensions));

  if (GetArrayBufferViewType(validated_descriptor.DataType()) !=
      array_buffer_view->GetType()) {
    return base::unexpected(
        "The buffer view type doesn't match the operand data type.");
  }
  if (validated_descriptor.ByteLength() != array_buffer_view->byteLength()) {
    return base::unexpected(String::Format(
        "The buffer view byte length (%zu) doesn't match the "
        "expected byte length (%zu).",
        array_buffer_view->byteLength(), validated_descriptor.ByteLength()));
  }
  auto* constant = MakeGarbageCollected<MLOperand>(
      builder, webnn::mojom::blink::Operand::Kind::kConstant,
      std::move(validated_descriptor));
  constant->array_buffer_view_ = array_buffer_view;
  return constant;
}

// static
base::expected<MLOperand*, String> MLOperand::ValidateAndCreateOutput(
    MLGraphBuilder* builder,
    V8MLOperandDataType::Enum data_type,
    Vector<uint32_t> dimensions,
    const MLOperator* ml_operator) {
  CHECK(ml_operator);

  ASSIGN_OR_RETURN(MLOperand::ValidatedDescriptor validated_descriptor,
                   ValidatedDescriptor::Create(data_type, dimensions));

  auto* output = MakeGarbageCollected<MLOperand>(
      builder, webnn::mojom::blink::Operand::Kind::kOutput,
      std::move(validated_descriptor));
  output->operator_ = ml_operator;
  return output;
}

MLOperand::MLOperand(MLGraphBuilder* builder,
                     webnn::mojom::blink::Operand::Kind kind,
                     ValidatedDescriptor descriptor)
    : builder_(builder), kind_(kind), descriptor_(std::move(descriptor)) {}

MLOperand::~MLOperand() = default;

MLGraphBuilder* MLOperand::Builder() const {
  return builder_.Get();
}

webnn::mojom::blink::Operand::Kind MLOperand::Kind() const {
  return kind_;
}

V8MLOperandDataType::Enum MLOperand::DataType() const {
  return descriptor_.DataType();
}

const Vector<uint32_t>& MLOperand::Dimensions() const {
  return descriptor_.Dimensions();
}

const String& MLOperand::Name() const {
  DCHECK_EQ(kind_, webnn::mojom::blink::Operand::Kind::kInput);
  return name_;
}

const DOMArrayBufferView* MLOperand::ArrayBufferView() const {
  DCHECK_EQ(kind_, webnn::mojom::blink::Operand::Kind::kConstant);
  return array_buffer_view_.Get();
}

const MLOperator* MLOperand::Operator() const {
  DCHECK_EQ(kind_, webnn::mojom::blink::Operand::Kind::kOutput);
  return operator_.Get();
}

size_t MLOperand::NumberOfElements() const {
  return descriptor_.NumberOfElements();
}

size_t MLOperand::ByteLength() const {
  return descriptor_.ByteLength();
}

Vector<uint32_t> MLOperand::shape() const {
  return descriptor_.Dimensions();
}

V8MLOperandDataType MLOperand::dataType() const {
  return V8MLOperandDataType(descriptor_.DataType());
}

void MLOperand::Trace(Visitor* visitor) const {
  visitor->Trace(builder_);
  visitor->Trace(array_buffer_view_);
  visitor->Trace(operator_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
