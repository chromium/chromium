// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_type_converter.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_clamp_options.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"

namespace mojo {

webnn::mojom::blink::Operand::DataType BlinkOperandTypeToMojo(
    blink::V8MLOperandType::Enum type) {
  switch (type) {
    case blink::V8MLOperandType::Enum::kFloat32:
      return webnn::mojom::blink::Operand::DataType::kFloat32;
    case blink::V8MLOperandType::Enum::kFloat16:
      return webnn::mojom::blink::Operand::DataType::kFloat16;
    case blink::V8MLOperandType::Enum::kInt32:
      return webnn::mojom::blink::Operand::DataType::kInt32;
    case blink::V8MLOperandType::Enum::kUint32:
      return webnn::mojom::blink::Operand::DataType::kUint32;
    case blink::V8MLOperandType::Enum::kInt8:
      return webnn::mojom::blink::Operand::DataType::kInt8;
    case blink::V8MLOperandType::Enum::kUint8:
      return webnn::mojom::blink::Operand::DataType::kUint8;
  }
  NOTREACHED_NORETURN();
}

// Converters from IDL to Mojo.
webnn::mojom::blink::OperandPtr
TypeConverter<webnn::mojom::blink::OperandPtr, blink::MLOperand*>::Convert(
    const blink::MLOperand* ml_operand) {
  if (!ml_operand) {
    return nullptr;
  }
  auto mojo_operand = webnn::mojom::blink::Operand::New();
  switch (ml_operand->Kind()) {
    case blink::MLOperand::OperandKind::kInput:
      mojo_operand->kind = webnn::mojom::blink::Operand::Kind::kInput;
      mojo_operand->name = ml_operand->Name();
      break;
    case blink::MLOperand::OperandKind::kConstant:
      mojo_operand->kind = webnn::mojom::blink::Operand::Kind::kConstant;
      break;
    case blink::MLOperand::OperandKind::kOutput:
      mojo_operand->kind = webnn::mojom::blink::Operand::Kind::kOutput;
      break;
  }
  mojo_operand->data_type = BlinkOperandTypeToMojo(ml_operand->Type());
  mojo_operand->dimensions = ml_operand->Dimensions();
  return mojo_operand;
}

}  // namespace mojo

namespace blink {

namespace {

using webnn::mojom::blink::Operator;
using webnn::mojom::blink::OperatorPtr;

// Maps MLOperand to its id which is used to identify the `mojo::Operand` across
// processes.
using OperandToIdMap = HeapHashMap<Member<const MLOperand>, uint64_t>;

uint64_t GetOperatorInputId(const MLOperator* op,
                            const OperandToIdMap& operand_to_id_map,
                            wtf_size_t index = 0) {
  CHECK_NE(op, nullptr);
  CHECK_LE(index, op->Inputs().size());
  const auto* input = op->Inputs()[index].Get();
  return operand_to_id_map.at(input);
}

uint64_t GetOperatorOutputId(const MLOperator* op,
                             const OperandToIdMap& operand_to_id_map,
                             wtf_size_t index = 0) {
  CHECK_NE(op, nullptr);
  CHECK_LE(index, op->Outputs().size());
  const auto* output = op->Outputs()[index].Get();
  return operand_to_id_map.at(output);
}

OperatorPtr CreateClampOperator(const OperandToIdMap& operand_to_id_map,
                                const MLOperator* clamp) {
  const uint64_t input_operand_id =
      GetOperatorInputId(clamp, operand_to_id_map);
  const uint64_t output_operand_id =
      GetOperatorOutputId(clamp, operand_to_id_map);

  auto operator_mojo = webnn::mojom::blink::Operator::New();
  operator_mojo->kind = Operator::Kind::kClamp;
  operator_mojo->input_operands = {input_operand_id};
  operator_mojo->output_operands = {output_operand_id};
  auto clamp_attributes = webnn::mojom::blink::ClampAttributes::New();
  const auto* options = static_cast<const MLClampOptions*>(clamp->Options());
  CHECK(options);
  clamp_attributes->min_value =
      options->getMinValueOr(-std::numeric_limits<float>::infinity());
  clamp_attributes->max_value =
      options->getMaxValueOr(+std::numeric_limits<float>::infinity());
  operator_mojo->attributes = webnn::mojom::blink::OperatorAttributes::NewClamp(
      std::move(clamp_attributes));
  return operator_mojo;
}

OperatorPtr CreateElementWiseBinaryOperator(
    const OperandToIdMap& operand_to_id_map,
    const MLOperator* binary) {
  const uint64_t lhs_operand_id =
      GetOperatorInputId(binary, operand_to_id_map, 0);
  const uint64_t rhs_operand_id =
      GetOperatorInputId(binary, operand_to_id_map, 1);
  const uint64_t output_operand_id =
      GetOperatorOutputId(binary, operand_to_id_map);

  auto operator_mojo = webnn::mojom::blink::Operator::New();
  switch (binary->Kind()) {
    case MLOperator::OperatorKind::kAdd:
      operator_mojo->kind = Operator::Kind::kAdd;
      break;
    case MLOperator::OperatorKind::kSub:
      operator_mojo->kind = Operator::Kind::kSub;
      break;
    case MLOperator::OperatorKind::kMul:
      operator_mojo->kind = Operator::Kind::kMul;
      break;
    case MLOperator::OperatorKind::kDiv:
      operator_mojo->kind = Operator::Kind::kDiv;
      break;
    case MLOperator::OperatorKind::kMax:
      operator_mojo->kind = Operator::Kind::kMax;
      break;
    case MLOperator::OperatorKind::kMin:
      operator_mojo->kind = Operator::Kind::kMin;
      break;
    default:
      NOTREACHED();
  }
  operator_mojo->input_operands = {lhs_operand_id, rhs_operand_id};
  operator_mojo->output_operands = {output_operand_id};
  return operator_mojo;
}

OperatorPtr CreateReluOperator(const OperandToIdMap& operand_to_id_map,
                               const MLOperator* relu) {
  const uint64_t input_operand_id = GetOperatorInputId(relu, operand_to_id_map);
  const uint64_t output_operand_id =
      GetOperatorOutputId(relu, operand_to_id_map);

  auto operator_mojo = webnn::mojom::blink::Operator::New();
  operator_mojo->kind = Operator::Kind::kRelu;
  operator_mojo->input_operands = {input_operand_id};
  operator_mojo->output_operands = {output_operand_id};
  return operator_mojo;
}

OperatorPtr CreateReshapeOperator(const OperandToIdMap& operand_to_id_map,
                                  const MLOperator* reshape) {
  const uint64_t input_operand_id =
      GetOperatorInputId(reshape, operand_to_id_map);
  const uint64_t output_operand_id =
      GetOperatorOutputId(reshape, operand_to_id_map);

  auto operator_mojo = webnn::mojom::blink::Operator::New();
  operator_mojo->kind = Operator::Kind::kReshape;
  operator_mojo->input_operands = {input_operand_id};
  operator_mojo->output_operands = {output_operand_id};
  return operator_mojo;
}

OperatorPtr CreateSoftmaxOperator(const OperandToIdMap& operand_to_id_map,
                                  const MLOperator* softmax) {
  const uint64_t input_operand_id =
      GetOperatorInputId(softmax, operand_to_id_map);
  const uint64_t output_operand_id =
      GetOperatorOutputId(softmax, operand_to_id_map);

  auto operator_mojo = webnn::mojom::blink::Operator::New();
  operator_mojo->kind = Operator::Kind::kSoftmax;
  operator_mojo->input_operands = {input_operand_id};
  operator_mojo->output_operands = {output_operand_id};
  return operator_mojo;
}

}  // namespace

OperatorPtr ConvertToMojoOperator(const OperandToIdMap& operand_to_id_map,
                                  const MLOperator* op) {
  switch (op->Kind()) {
    case MLOperator::OperatorKind::kClamp:
      return CreateClampOperator(operand_to_id_map, op);
    case MLOperator::OperatorKind::kAdd:
    case MLOperator::OperatorKind::kSub:
    case MLOperator::OperatorKind::kMul:
    case MLOperator::OperatorKind::kDiv:
    case MLOperator::OperatorKind::kMin:
    case MLOperator::OperatorKind::kMax:
      return CreateElementWiseBinaryOperator(operand_to_id_map, op);
    case MLOperator::OperatorKind::kRelu:
      return CreateReluOperator(operand_to_id_map, op);
    case MLOperator::OperatorKind::kReshape:
      return CreateReshapeOperator(operand_to_id_map, op);
    case MLOperator::OperatorKind::kSoftmax:
      return CreateSoftmaxOperator(operand_to_id_map, op);
    case MLOperator::OperatorKind::kConv2d:
    case MLOperator::OperatorKind::kGemm:
    case MLOperator::OperatorKind::kAveragePool2d:
    case MLOperator::OperatorKind::kMaxPool2d:
    case MLOperator::OperatorKind::kHardSwish:
    case MLOperator::OperatorKind::kResample2d:
    case MLOperator::OperatorKind::kSigmoid:
    case MLOperator::OperatorKind::kConcat:
    case MLOperator::OperatorKind::kTranspose:
    case MLOperator::OperatorKind::kLeakyRelu:
    case MLOperator::OperatorKind::kConvTranspose2d:
    case MLOperator::OperatorKind::kPRelu:
    case MLOperator::OperatorKind::kPad:
    case MLOperator::OperatorKind::kElu:
    case MLOperator::OperatorKind::kAbs:
    case MLOperator::OperatorKind::kCeil:
    case MLOperator::OperatorKind::kFloor:
    case MLOperator::OperatorKind::kNeg:
    case MLOperator::OperatorKind::kSlice:
    case MLOperator::OperatorKind::kSplit:
    case MLOperator::OperatorKind::kTanh:
      NOTIMPLEMENTED();
      return nullptr;
  }
  NOTREACHED_NORETURN();
}

}  // namespace blink
