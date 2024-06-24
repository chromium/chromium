// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"

#include <functional>
#include <numeric>

#include "base/numerics/safe_conversions.h"
#include "base/types/expected_macros.h"
#include "services/webnn/public/cpp/graph_validation_utils.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"

namespace blink {

DOMArrayBufferView::ViewType GetArrayBufferViewType(
    webnn::OperandDataType data_type) {
  switch (data_type) {
    case webnn::OperandDataType::kFloat32:
      return DOMArrayBufferView::ViewType::kTypeFloat32;
    case webnn::OperandDataType::kFloat16:
      // Using Uint16Array for float16 is a workaround of WebNN spec issue:
      // https://github.com/webmachinelearning/webnn/issues/127
      return DOMArrayBufferView::ViewType::kTypeUint16;
    case webnn::OperandDataType::kInt32:
      return DOMArrayBufferView::ViewType::kTypeInt32;
    case webnn::OperandDataType::kUint32:
      return DOMArrayBufferView::ViewType::kTypeUint32;
    case webnn::OperandDataType::kInt64:
      return DOMArrayBufferView::ViewType::kTypeBigInt64;
    case webnn::OperandDataType::kUint64:
      return DOMArrayBufferView::ViewType::kTypeBigUint64;
    case webnn::OperandDataType::kInt8:
      return DOMArrayBufferView::ViewType::kTypeInt8;
    case webnn::OperandDataType::kUint8:
      return DOMArrayBufferView::ViewType::kTypeUint8;
  }
}

// static
base::expected<MLOperand*, String> MLOperand::ValidateAndCreateInput(
    MLGraphBuilder* builder,
    V8MLOperandDataType::Enum data_type,
    Vector<uint32_t> dimensions,
    String name) {
  ASSIGN_OR_RETURN(webnn::OperandDescriptor descriptor,
                   webnn::OperandDescriptor::Create(
                       FromBlinkDataType(data_type), dimensions),
                   [](std::string error) { return String(error); });

  if (name.empty()) {
    return base::unexpected("The name is empty.");
  }

  auto* input = MakeGarbageCollected<MLOperand>(
      builder, webnn::mojom::blink::Operand::Kind::kInput,
      std::move(descriptor));
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

  ASSIGN_OR_RETURN(webnn::OperandDescriptor descriptor,
                   webnn::OperandDescriptor::Create(
                       FromBlinkDataType(data_type), dimensions),
                   [](std::string error) { return String(error); });

  if (GetArrayBufferViewType(descriptor.data_type()) !=
      array_buffer_view->GetType()) {
    return base::unexpected(
        "The buffer view type doesn't match the operand data type.");
  }
  if (descriptor.PackedByteLength() != array_buffer_view->byteLength()) {
    return base::unexpected(String::Format(
        "The buffer view byte length (%zu) doesn't match the "
        "expected byte length (%zu).",
        array_buffer_view->byteLength(), descriptor.PackedByteLength()));
  }
  auto* constant = MakeGarbageCollected<MLOperand>(
      builder, webnn::mojom::blink::Operand::Kind::kConstant,
      std::move(descriptor));
  constant->constant_bytes_ = Vector<uint8_t>(array_buffer_view->ByteSpan());
  return constant;
}

// static
MLOperand* MLOperand::CreateOutput(MLGraphBuilder* builder,
                                   webnn::OperandDescriptor descriptor,
                                   const MLOperator* ml_operator) {
  CHECK(ml_operator);

  auto* output = MakeGarbageCollected<MLOperand>(
      builder, webnn::mojom::blink::Operand::Kind::kOutput,
      std::move(descriptor));
  output->operator_ = ml_operator;
  return output;
}

MLOperand::MLOperand(MLGraphBuilder* builder,
                     webnn::mojom::blink::Operand::Kind kind,
                     webnn::OperandDescriptor descriptor)
    : builder_(builder), kind_(kind), descriptor_(std::move(descriptor)) {}

MLOperand::~MLOperand() = default;

MLGraphBuilder* MLOperand::Builder() const {
  return builder_.Get();
}

webnn::mojom::blink::Operand::Kind MLOperand::Kind() const {
  return kind_;
}

const String& MLOperand::Name() const {
  DCHECK_EQ(kind_, webnn::mojom::blink::Operand::Kind::kInput);
  return name_;
}

base::span<const uint8_t> MLOperand::Bytes() const {
  DCHECK_EQ(kind_, webnn::mojom::blink::Operand::Kind::kConstant);
  return constant_bytes_;
}

const MLOperator* MLOperand::Operator() const {
  DCHECK_EQ(kind_, webnn::mojom::blink::Operand::Kind::kOutput);
  return operator_.Get();
}

const webnn::OperandDescriptor& MLOperand::Descriptor() const {
  return descriptor_;
}

webnn::OperandDataType MLOperand::DataType() const {
  return descriptor_.data_type();
}

const std::vector<uint32_t>& MLOperand::Shape() const {
  return descriptor_.shape();
}

size_t MLOperand::NumberOfElements() const {
  return descriptor_.NumberOfElements();
}

size_t MLOperand::ByteLength() const {
  return descriptor_.PackedByteLength();
}

wtf_size_t MLOperand::Rank() const {
  // TODO(crbug.com/325598628): Make this a static_cast if validation is added
  // to check that the rank is less than the max uint32_t.
  return base::checked_cast<wtf_size_t>(descriptor_.Rank());
}

Vector<uint32_t> MLOperand::shape() const {
  return Vector<uint32_t>(descriptor_.shape());
}

V8MLOperandDataType MLOperand::dataType() const {
  return ToBlinkDataType(descriptor_.data_type());
}

void MLOperand::Trace(Visitor* visitor) const {
  visitor->Trace(builder_);
  visitor->Trace(operator_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
