// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"

#include <functional>

#include "base/numerics/safe_conversions.h"
#include "base/types/expected_macros.h"
#include "services/webnn/public/cpp/graph_validation_utils.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_constant_operand.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"

namespace blink {

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
  CHECK_EQ(kind_, webnn::mojom::blink::Operand::Kind::kInput);
  return name_;
}

const MLOperator* MLOperand::Operator() const {
  CHECK_EQ(kind_, webnn::mojom::blink::Operand::Kind::kOutput);
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
  static_assert(sizeof(descriptor_.Rank()) == sizeof(wtf_size_t));
  return static_cast<wtf_size_t>(descriptor_.Rank());
}

Vector<uint32_t> MLOperand::shape() const {
  return Vector<uint32_t>(descriptor_.shape());
}

V8MLOperandDataType MLOperand::dataType() const {
  return ToBlinkDataType(descriptor_.data_type());
}

MLConstantOperand const* MLOperand::AsConstantOperand() const {
  CHECK_EQ(kind_, webnn::mojom::blink::Operand::Kind::kConstant);
  return static_cast<MLConstantOperand const*>(this);
}

void MLOperand::Trace(Visitor* visitor) const {
  visitor->Trace(builder_);
  visitor->Trace(operator_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
