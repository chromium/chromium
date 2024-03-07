// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/graph_builder.h"
#include <cstdint>

#include "base/containers/span.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected_macros.h"
#include "components/ml/webnn/graph_validation_utils.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "third_party/tflite/src/tensorflow/lite/schema/schema_generated.h"

namespace webnn::tflite {

namespace {

// The version number of the Schema. Ideally all changes will be backward
// compatible. If that ever changes, we must ensure that version is the first
// entry in the new tflite root so that we can see that version is not 1.
#define TFLITE_SCHEMA_VERSION (3)

// Maps a DataType to a `::tflite::TensorType`. Other `TensorTypeMap` overloads
// may be declared below as needed.
//
// Example: TensorTypeMap<uint32_t>::value -> ::tflite::TensorType_UINT32
template <typename DataType>
  requires IsSupportedTensorType<DataType>
struct TensorTypeMap;

template <>
struct TensorTypeMap<uint32_t> {
  static constexpr ::tflite::TensorType value =
      ::tflite::TensorType::TensorType_UINT32;
};

// Useful for converting dimension arrays coming from mojo as uint32 to the
// int32 vectors used by TFLite.
base::expected<std::vector<int32_t>, std::string> ToSignedDimensions(
    base::span<const uint32_t> input_dimensions) {
  std::vector<int32_t> output_dimensions;
  output_dimensions.reserve(input_dimensions.size());
  for (auto dimension : input_dimensions) {
    auto checked_dimension = base::MakeCheckedNum<int32_t>(dimension);
    if (!checked_dimension.IsValid()) {
      return base::unexpected("The dimension is too large.");
    }
    output_dimensions.push_back(checked_dimension.ValueOrDie());
  }
  return output_dimensions;
}

::tflite::TensorType MojoOperandTypeToTFLite(
    mojom::Operand::DataType data_type) {
  switch (data_type) {
    case mojom::Operand::DataType::kFloat32:
      return ::tflite::TensorType_FLOAT32;
    case mojom::Operand::DataType::kFloat16:
      return ::tflite::TensorType_FLOAT16;
    case mojom::Operand::DataType::kInt32:
      return ::tflite::TensorType_INT32;
    case mojom::Operand::DataType::kUint32:
      return ::tflite::TensorType_UINT32;
    case mojom::Operand::DataType::kInt64:
      return ::tflite::TensorType_INT64;
    case mojom::Operand::DataType::kUint64:
      return ::tflite::TensorType_UINT64;
    case mojom::Operand::DataType::kInt8:
      return ::tflite::TensorType_INT8;
    case mojom::Operand::DataType::kUint8:
      return ::tflite::TensorType_UINT8;
  }
}

enum class ClampRange { kRelu, kRelu1, kRelu6 };

base::expected<ClampRange, std::string> GetClampRange(
    const mojom::Clamp& clamp) {
  // TODO(crbug.com/326156496): Use RELU_0_TO_1 to support min = 0.0f and max
  // = 1.0f.
  if (clamp.min_value == -1.0f && clamp.max_value == 1.0f) {
    return ClampRange::kRelu1;
  } else if (clamp.min_value == 0.0f && clamp.max_value == 6.0f) {
    return ClampRange::kRelu6;
  } else if (clamp.min_value == 0.0f &&
             clamp.max_value == std::numeric_limits<float>::infinity()) {
    return ClampRange::kRelu;
  }

  // TODO(crbug.com/326156496): Support other range.
  return base::unexpected(
      "The range of clamp is not supported in tflite schema.");
}

}  // namespace

// static
base::expected<flatbuffers::DetachedBuffer, std::string>
GraphBuilder::CreateAndBuild(const mojom::GraphInfo& graph_info) {
  GraphBuilder builder(graph_info);

  for (const auto& [operand_id, operand] : graph_info.id_to_operand_map) {
    RETURN_IF_ERROR(builder.SerializeOperand(operand_id, *operand));
  }

  for (const mojom::OperationPtr& operation : graph_info.operations) {
    RETURN_IF_ERROR(builder.SerializeOperation(*operation));
  }

  return builder.FinishAndTakeFlatBuffer(graph_info.input_operands,
                                         graph_info.output_operands);
}

GraphBuilder::GraphBuilder(const mojom::GraphInfo& graph_info)
    : graph_info_(graph_info) {
  // TFLite requires the first entry in FlatBuffer to be an empty buffer.
  buffers_.push_back(
      ::tflite::CreateBuffer(builder_, builder_.CreateVector({})));
}

GraphBuilder::~GraphBuilder() = default;

base::expected<void, std::string> GraphBuilder::SerializeOperand(
    uint64_t operand_id,
    const mojom::Operand& operand) {
  // The index of `tflite::Tensor` array, each `Operand` (input, constant,
  // output) will be converted and pushed back into the array, so it's increased
  // by one after each serialization in flat buffer.
  int32_t tensor_index = base::checked_cast<int32_t>(tensors_.size());
  CHECK_GE(tensor_index, 0);

  // The buffer index 0 represents input and output operand because there is no
  // data buffer associated.
  uint32_t buffer_index = 0;
  if (operand.kind == mojom::Operand::Kind::kConstant) {
    // Serialize buffer and return buffer index which starts from 1, it is
    // used to create the constant's tensor.
    buffer_index =
        SerializeBuffer(graph_info_->constant_id_to_buffer_map.at(operand_id));
  }

  // Create `Tensor` with operand shape, the index of buffer and the name.
  const auto signed_operand_dimensions = ToSignedDimensions(operand.dimensions);
  RETURN_IF_ERROR(signed_operand_dimensions);
  const flatbuffers::Offset<flatbuffers::Vector<int32_t>> dimensions =
      builder_.CreateVector<int32_t>(*std::move(signed_operand_dimensions));
  const auto operand_type = MojoOperandTypeToTFLite(operand.data_type);
  const StringOffset operand_name =
      operand.name.has_value() ? builder_.CreateString(*operand.name) : 0;
  tensors_.emplace_back(::tflite::CreateTensor(builder_, std::move(dimensions),
                                               operand_type, buffer_index,
                                               operand_name));
  operand_to_index_map_.insert({operand_id, tensor_index});
  return base::ok();
}

base::expected<void, std::string> GraphBuilder::SerializeOperation(
    const mojom::Operation& op) {
  OperatorOffset operator_offset;
  switch (op.which()) {
    case mojom::Operation::Tag::kClamp: {
      const auto clamp_result = SerializeClamp(*op.get_clamp());
      RETURN_IF_ERROR(clamp_result);
      operator_offset = clamp_result.value();
      break;
    }
    case mojom::Operation::Tag::kConcat:
      operator_offset = SerializeConcat(*op.get_concat());
      break;
    case mojom::Operation::Tag::kElementWiseBinary: {
      const auto elementwise_result =
          SerializeElementWiseBinary(*op.get_element_wise_binary());
      RETURN_IF_ERROR(elementwise_result);
      operator_offset = elementwise_result.value();
      break;
    }
    case mojom::Operation::Tag::kElementWiseUnary: {
      const auto elementwise_result =
          SerializeElementWiseUnary(*op.get_element_wise_unary());
      RETURN_IF_ERROR(elementwise_result);
      operator_offset = elementwise_result.value();
      break;
    }
    case mojom::Operation::Tag::kReshape: {
      const auto reshape_result = SerializeReshape(*op.get_reshape());
      RETURN_IF_ERROR(reshape_result);
      operator_offset = reshape_result.value();
      break;
    }
    case mojom::Operation::Tag::kSigmoid:
      operator_offset = SerializeSigmoid(*op.get_sigmoid());
      break;
    case mojom::Operation::Tag::kSoftmax:
      operator_offset = SerializeSoftmax(*op.get_softmax());
      break;
    case mojom::Operation::Tag::kTranspose:
      operator_offset = SerializeTranspose(*op.get_transpose());
      break;
    case mojom::Operation::Tag::kArgMinMax:
      return base::unexpected("argMinMax is not implemented");
    case mojom::Operation::Tag::kBatchNormalization:
      return base::unexpected("batchNormalization is not implemented");
    case mojom::Operation::Tag::kConv2d:
      return base::unexpected("conv2d is not implemented");
    case mojom::Operation::Tag::kElu:
      return base::unexpected("elu is not implemented");
    case mojom::Operation::Tag::kExpand:
      return base::unexpected("expand is not implemented");
    case mojom::Operation::Tag::kGather:
      return base::unexpected("gather is not implemented");
    case mojom::Operation::Tag::kGemm:
      return base::unexpected("gemm is not implemented");
    case mojom::Operation::Tag::kHardSigmoid:
      return base::unexpected("hardSigmoid is not implemented");
    case mojom::Operation::Tag::kHardSwish:
      return base::unexpected("hardSwish is not implemented");
    case mojom::Operation::Tag::kLayerNormalization:
      return base::unexpected("layerNormalization is not implemented");
    case mojom::Operation::Tag::kInstanceNormalization:
      return base::unexpected("instanceNormalization is not implemented");
    case mojom::Operation::Tag::kLeakyRelu:
      return base::unexpected("leakyRelu is not implemented");
    case mojom::Operation::Tag::kLinear:
      return base::unexpected("linear is not implemented");
    case mojom::Operation::Tag::kMatmul:
      return base::unexpected("matmul is not implemented");
    case mojom::Operation::Tag::kPad:
      return base::unexpected("pad is not implemented");
    case mojom::Operation::Tag::kPool2d:
      return base::unexpected("pool2d is not implemented");
    case mojom::Operation::Tag::kPrelu:
      return base::unexpected("prelu is not implemented");
    case mojom::Operation::Tag::kReduce:
      return base::unexpected("reduce is not implemented");
    case mojom::Operation::Tag::kRelu:
      return base::unexpected("relu is not implemented");
    case mojom::Operation::Tag::kResample2d:
      return base::unexpected("resample2d is not implemented");
    case mojom::Operation::Tag::kSlice:
      return base::unexpected("slice is not implemented");
    case mojom::Operation::Tag::kSoftplus:
      return base::unexpected("softplus is not implemented");
    case mojom::Operation::Tag::kSoftsign:
      return base::unexpected("softsign is not implemented");
    case mojom::Operation::Tag::kSplit:
      return base::unexpected("split is not implemented");
    case mojom::Operation::Tag::kTanh:
      return base::unexpected("tanh is not implemented");
    case mojom::Operation::Tag::kWhere:
      return base::unexpected("where is not implemented");
  }
  operators_.emplace_back(operator_offset);

  return base::ok();
}

flatbuffers::DetachedBuffer GraphBuilder::FinishAndTakeFlatBuffer(
    base::span<const uint64_t> input_operands,
    base::span<const uint64_t> output_operands) {
  CHECK(!is_created_model_);

  int32_t* graph_input_ids = nullptr;
  auto graph_input_ids_index = builder_.CreateUninitializedVector<int32_t>(
      input_operands.size(), &graph_input_ids);
  base::ranges::transform(input_operands, graph_input_ids,
                          [&](uint64_t operand_id) {
                            return operand_to_index_map_.at(operand_id);
                          });

  int32_t* graph_output_ids = nullptr;
  auto graph_output_ids_index = builder_.CreateUninitializedVector<int32_t>(
      output_operands.size(), &graph_output_ids);
  base::ranges::transform(output_operands, graph_output_ids,
                          [&](uint64_t operand_id) {
                            return operand_to_index_map_.at(operand_id);
                          });

  // Create `tflite::SubGraph`, which typically represents an entire model.
  // The inputs of subgraph are the list of non-static tensors that feed into
  // the subgraph for inference. The outputs of subgraph are considered the
  // product of the subgraph's inference. The operators are in execution order.
  flatbuffers::Offset<::tflite::SubGraph> subgraph = ::tflite::CreateSubGraph(
      builder_, builder_.CreateVector(tensors_.data(), tensors_.size()),
      graph_input_ids_index, graph_output_ids_index,
      builder_.CreateVector(operators_.data(), operators_.size()));

  StringOffset description =
      builder_.CreateString("TFLite model converted from WebNN Graph");

  // The operator codes used in this model are kept in order because operators
  // carry an index into this std::vector.
  // There is only one subgraph in the model. The buffers of the model must be
  // initialized an empty buffer.
  flatbuffers::Offset<::tflite::Model> model_buffer = ::tflite::CreateModel(
      builder_, TFLITE_SCHEMA_VERSION,
      builder_.CreateVector(operator_codes_.data(), operator_codes_.size()),
      builder_.CreateVector(&subgraph, 1), description,
      builder_.CreateVector(buffers_.data(), buffers_.size()));

  ::tflite::FinishModelBuffer(builder_, model_buffer);
  is_created_model_ = true;

  return builder_.Release();
}

uint32_t GraphBuilder::SerializeBuffer(const mojo_base::BigBuffer& constant) {
  const auto buffer_data =
      builder_.CreateVector(constant.data(), constant.size());
  const auto buffer_index = base::checked_cast<uint32_t>(buffers_.size());
  buffers_.emplace_back(::tflite::CreateBuffer(builder_, buffer_data));
  // The index of buffer is referenced by tensors.
  return buffer_index;
}

template <typename DataType>
  requires IsSupportedTensorType<DataType>
uint32_t GraphBuilder::SerializeTensorWithBuffer(
    base::span<const DataType> buffer,
    base::span<const int32_t> dimensions) {
  const auto buffer_index = base::checked_cast<uint32_t>(buffers_.size());
  const auto buffer_data =
      builder_.CreateVector<uint8_t>(base::as_byte_span(buffer));
  buffers_.emplace_back(::tflite::CreateBuffer(builder_, buffer_data));

  // Create `tflite::Tensor` with the dimensions and the index of buffer.
  const int32_t tensor_index = base::checked_cast<int32_t>(tensors_.size());
  tensors_.emplace_back(::tflite::CreateTensor(
      builder_, builder_.CreateVector<int32_t>(dimensions),
      TensorTypeMap<DataType>::value, buffer_index));

  return tensor_index;
}

uint32_t GraphBuilder::GetOperatorCodeIndex(::tflite::BuiltinOperator code) {
  auto operator_code_index =
      base::checked_cast<uint32_t>(operator_codes_.size());
  operator_codes_.push_back(::tflite::CreateOperatorCode(builder_, code));
  // The type of operation is determined by the index into the list of the valid
  // OperatorCodes.
  return operator_code_index;
}

const mojom::Operand& GraphBuilder::GetOperand(uint64_t operand_id) const {
  return *graph_info_->id_to_operand_map.at(operand_id);
}

auto GraphBuilder::SerializeUnaryOperator(::tflite::BuiltinOperator code,
                                          uint64_t input_operand_id,
                                          uint64_t output_operand_id)
    -> OperatorOffset {
  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const uint32_t operator_code_index = GetOperatorCodeIndex(code);
  const std::array<int32_t, 1> op_inputs = {
      operand_to_index_map_.at(input_operand_id)};
  const std::array<int32_t, 1> op_outputs = {
      operand_to_index_map_.at(output_operand_id)};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs));
}

auto GraphBuilder::SerializeClamp(const mojom::Clamp& clamp)
    -> base::expected<OperatorOffset, std::string> {
  const auto range_result = GetClampRange(clamp);
  RETURN_IF_ERROR(range_result);

  ::tflite::BuiltinOperator code;
  switch (range_result.value()) {
    case ClampRange::kRelu:
      code = ::tflite::BuiltinOperator_RELU;
      break;
    case ClampRange::kRelu1:
      code = ::tflite::BuiltinOperator_RELU_N1_TO_1;
      break;
    case ClampRange::kRelu6:
      code = ::tflite::BuiltinOperator_RELU6;
      break;
  }

  return SerializeUnaryOperator(code, clamp.input_operand_id,
                                clamp.output_operand_id);
}

auto GraphBuilder::SerializeConcat(const mojom::Concat& concat)
    -> OperatorOffset {
  int32_t* operator_inputs = nullptr;
  auto operator_inputs_index = builder_.CreateUninitializedVector<int32_t>(
      concat.input_operand_ids.size(), &operator_inputs);
  base::ranges::transform(concat.input_operand_ids, operator_inputs,
                          [&](uint64_t operand_id) {
                            return operand_to_index_map_.at(operand_id);
                          });

  const int32_t output_index =
      operand_to_index_map_.at(concat.output_operand_id);

  // Create `tflite::ConcatenationOptions` with axis.
  const auto concat_options =
      ::tflite::CreateConcatenationOptions(builder_, concat.axis);

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_CONCATENATION);
  const std::array<int32_t, 1> operator_outputs = {output_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index, operator_inputs_index,
      builder_.CreateVector<int32_t>(operator_outputs),
      ::tflite::BuiltinOptions_ConcatenationOptions, concat_options.Union());
}

auto GraphBuilder::SerializeElementWiseBinary(
    const mojom::ElementWiseBinary& op)
    -> base::expected<OperatorOffset, std::string> {
  ::tflite::BuiltinOperator code;
  switch (op.kind) {
    case mojom::ElementWiseBinary_Kind::kAdd:
      code = ::tflite::BuiltinOperator_ADD;
      break;
    case mojom::ElementWiseBinary_Kind::kSub:
      code = ::tflite::BuiltinOperator_SUB;
      break;
    case mojom::ElementWiseBinary_Kind::kMul:
      code = ::tflite::BuiltinOperator_MUL;
      break;
    case mojom::ElementWiseBinary_Kind::kDiv:
      code = ::tflite::BuiltinOperator_DIV;
      break;
    case mojom::ElementWiseBinary_Kind::kMax:
      code = ::tflite::BuiltinOperator_MAXIMUM;
      break;
    case mojom::ElementWiseBinary_Kind::kMin:
      code = ::tflite::BuiltinOperator_MINIMUM;
      break;
    case mojom::ElementWiseBinary_Kind::kPow:
      code = ::tflite::BuiltinOperator_POW;
      break;
    case mojom::ElementWiseBinary_Kind::kEqual:
    case mojom::ElementWiseBinary_Kind::kGreater:
    case mojom::ElementWiseBinary_Kind::kGreaterOrEqual:
    case mojom::ElementWiseBinary_Kind::kLesser:
    case mojom::ElementWiseBinary_Kind::kLesserOrEqual:
      return base::unexpected(
          base::StrCat({base::ToString(op.kind), " is not implemented."}));
  }

  const uint32_t operator_code_index = GetOperatorCodeIndex(code);
  const std::array<int32_t, 2> op_inputs = {
      operand_to_index_map_.at(op.lhs_operand),
      operand_to_index_map_.at(op.rhs_operand)};
  const std::array<int32_t, 1> op_outputs = {
      operand_to_index_map_.at(op.output_operand)};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs));
}

auto GraphBuilder::SerializeElementWiseUnary(const mojom::ElementWiseUnary& op)
    -> base::expected<OperatorOffset, std::string> {
  switch (op.kind) {
    case mojom::ElementWiseUnary::Kind::kAbs:
      return SerializeUnaryOperator(::tflite::BuiltinOperator_ABS,
                                    op.input_operand_id, op.output_operand_id);
    case mojom::ElementWiseUnary::Kind::kCeil:
      return SerializeUnaryOperator(::tflite::BuiltinOperator_CEIL,
                                    op.input_operand_id, op.output_operand_id);
    case mojom::ElementWiseUnary::Kind::kCos:
      return SerializeUnaryOperator(::tflite::BuiltinOperator_COS,
                                    op.input_operand_id, op.output_operand_id);
    case mojom::ElementWiseUnary::Kind::kExp:
      return SerializeUnaryOperator(::tflite::BuiltinOperator_EXP,
                                    op.input_operand_id, op.output_operand_id);
    case mojom::ElementWiseUnary::Kind::kFloor:
      return SerializeUnaryOperator(::tflite::BuiltinOperator_FLOOR,
                                    op.input_operand_id, op.output_operand_id);
    case mojom::ElementWiseUnary::Kind::kLog:
      return SerializeUnaryOperator(::tflite::BuiltinOperator_LOG,
                                    op.input_operand_id, op.output_operand_id);
    case mojom::ElementWiseUnary::Kind::kNeg:
      return SerializeUnaryOperator(::tflite::BuiltinOperator_NEG,
                                    op.input_operand_id, op.output_operand_id);
    case mojom::ElementWiseUnary::Kind::kSin:
      return SerializeUnaryOperator(::tflite::BuiltinOperator_SIN,
                                    op.input_operand_id, op.output_operand_id);
    case mojom::ElementWiseUnary::Kind::kSqrt:
      return SerializeUnaryOperator(::tflite::BuiltinOperator_SQRT,
                                    op.input_operand_id, op.output_operand_id);
    case mojom::ElementWiseUnary::Kind::kTan:
    case mojom::ElementWiseUnary::Kind::kLogicalNot:
    case mojom::ElementWiseUnary::Kind::kIdentity:
    case mojom::ElementWiseUnary::Kind::kErf:
    case mojom::ElementWiseUnary::Kind::kReciprocal:
    case mojom::ElementWiseUnary::Kind::kCast:
      return base::unexpected(
          base::StrCat({base::ToString(op.kind), " is not implemented."}));
  }
}

auto GraphBuilder::SerializeReshape(const mojom::Reshape& reshape)
    -> base::expected<OperatorOffset, std::string> {
  // Get the shape of the output tensor, such that this operator can reshape the
  // input to it.
  const mojom::Operand& output_operand = GetOperand(reshape.output_operand_id);
  auto signed_output_dimensions = ToSignedDimensions(output_operand.dimensions);
  RETURN_IF_ERROR(signed_output_dimensions);

  const auto reshape_options = ::tflite::CreateReshapeOptions(
      builder_,
      /*new_shape=*/builder_.CreateVector<int32_t>(
          *std::move(signed_output_dimensions)));

  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_RESHAPE);
  const std::array<int32_t, 1> op_inputs = {
      operand_to_index_map_.at(reshape.input_operand_id)};
  const std::array<int32_t, 1> op_outputs = {
      operand_to_index_map_.at(reshape.output_operand_id)};
  return ::tflite::CreateOperator(
      builder_, operator_code_index, builder_.CreateVector<int32_t>(op_inputs),
      builder_.CreateVector<int32_t>(op_outputs),
      ::tflite::BuiltinOptions_ReshapeOptions, reshape_options.Union());
}

auto GraphBuilder::SerializeSigmoid(const mojom::Sigmoid& sigmoid)
    -> OperatorOffset {
  return SerializeUnaryOperator(::tflite::BuiltinOperator_LOGISTIC,
                                sigmoid.input_operand_id,
                                sigmoid.output_operand_id);
}

auto GraphBuilder::SerializeSoftmax(const mojom::Softmax& softmax)
    -> OperatorOffset {
  const auto softmax_options =
      ::tflite::CreateSoftmaxOptions(builder_, /*beta=*/1.0);

  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_SOFTMAX);
  const std::array<int32_t, 1> op_inputs = {
      operand_to_index_map_.at(softmax.input_operand_id)};
  const std::array<int32_t, 1> op_outputs = {
      operand_to_index_map_.at(softmax.output_operand_id)};
  return ::tflite::CreateOperator(
      builder_, operator_code_index, builder_.CreateVector<int32_t>(op_inputs),
      builder_.CreateVector<int32_t>(op_outputs),
      ::tflite::BuiltinOptions_SoftmaxOptions, softmax_options.Union());
}

auto GraphBuilder::SerializeTranspose(const mojom::Transpose& transpose)
    -> OperatorOffset {
  const std::array<int32_t, 1> permutation_shape = {
      base::checked_cast<int32_t>(transpose.permutation.size())};
  const uint32_t permutation_tensor_index = SerializeTensorWithBuffer<uint32_t>(
      transpose.permutation, permutation_shape);

  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_TRANSPOSE);
  const std::array<int32_t, 2> op_inputs = {
      operand_to_index_map_.at(transpose.input_operand_id),
      base::checked_cast<int32_t>(permutation_tensor_index)};
  const std::array<int32_t, 1> op_outputs = {
      operand_to_index_map_.at(transpose.output_operand_id)};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs));
}

}  // namespace webnn::tflite
