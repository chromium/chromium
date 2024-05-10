// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/graph_builder.h"

#include <cstdint>
#include <vector>

#include "base/containers/fixed_flat_set.h"
#include "base/containers/span.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/ml/webnn/graph_validation_utils.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_utils.h"
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
  requires internal::IsSupportedTensorType<DataType>
struct TensorTypeMap;

template <>
struct TensorTypeMap<float> {
  static constexpr ::tflite::TensorType value =
      ::tflite::TensorType::TensorType_FLOAT32;
};
template <>
struct TensorTypeMap<int32_t> {
  static constexpr ::tflite::TensorType value =
      ::tflite::TensorType::TensorType_INT32;
};
template <>
struct TensorTypeMap<uint32_t> {
  static constexpr ::tflite::TensorType value =
      ::tflite::TensorType::TensorType_UINT32;
};

static constexpr auto kFloatDataTypes =
    base::MakeFixedFlatSet<mojom::Operand::DataType>(
        {mojom::Operand::DataType::kFloat16,
         mojom::Operand::DataType::kFloat32});

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

struct PaddingSizes {
  uint32_t begin;
  uint32_t end;
};

// Helper to calculate the explicit padding for tflite::Padding_SAME mode with
// https://www.tensorflow.org/versions/r2.14/api_docs/python/tf/nn#notes_on_padding_2.
std::optional<PaddingSizes> CalculateExplicitPaddingForSamePaddingMode(
    uint32_t input_size,
    uint32_t filter_size,
    uint32_t stride,
    uint32_t dilation) {
  auto checked_output_size =
      (base::MakeCheckedNum<uint32_t>(input_size) + stride - 1) / stride;
  auto checked_dilated_filter_size =
      (base::MakeCheckedNum<uint32_t>(filter_size) - 1) * dilation + 1;
  auto checked_needed_input_size =
      (checked_output_size - 1) * stride + checked_dilated_filter_size;
  if (!checked_needed_input_size.IsValid()) {
    return std::nullopt;
  }
  auto checked_total_padding =
      checked_needed_input_size.ValueOrDie() > input_size
          ? checked_needed_input_size - input_size
          : base::MakeCheckedNum<uint32_t>(0);
  // Same upper padding.
  auto checked_padding_begin = checked_total_padding / 2;
  auto checked_padding_end = (checked_total_padding + 1) / 2;
  uint32_t padding_begin, padding_end;
  if (!checked_padding_begin.AssignIfValid(&padding_begin) ||
      !checked_padding_end.AssignIfValid(&padding_end)) {
    return std::nullopt;
  }
  return PaddingSizes({.begin = padding_begin, .end = padding_end});
}

struct TfLitePadding {
  ::tflite::Padding mode;
  // The explicit paddings are used to create TfLite Pad operator.
  std::optional<std::array<uint32_t, 4>> paddings;
};

// Helper to get tflite padding mode for convolution 2d or pooling 2d.
base::expected<TfLitePadding, std::string> GetTfLitePaddingMode(
    const mojom::Padding2d& padding2d,
    const webnn::Size2d<uint32_t>& input,
    const webnn::Size2d<uint32_t>& filter,
    const mojom::Size2d& stride,
    const mojom::Size2d& dilation) {
  // WebNN explicit padding is in [beginning_height, ending_height,
  // beginning_width, ending_width] sequence.
  std::array<uint32_t, 4> explicit_padding = {
      padding2d.beginning->height, padding2d.ending->height,
      padding2d.beginning->width, padding2d.ending->width};
  std::array<uint32_t, 4> no_padding = {0, 0, 0, 0};
  if (explicit_padding == no_padding) {
    return TfLitePadding{.mode = ::tflite::Padding_VALID};
  }

  // Convert the explicit padding to tflite same padding mode, The TFLite PAD
  // operator need to be inserted if the calculated padding are not the same as
  // explicit padding.
  const auto padding_height = CalculateExplicitPaddingForSamePaddingMode(
      input.height, filter.height, stride.height, dilation.height);
  const auto padding_width = CalculateExplicitPaddingForSamePaddingMode(
      input.width, filter.width, stride.width, dilation.width);
  if (!padding_height || !padding_width) {
    return base::unexpected("Failed to calculate explicit padding.");
  }
  std::array<uint32_t, 4> upper_padding = {
      padding_height->begin, padding_height->end, padding_width->begin,
      padding_width->end};
  if (explicit_padding == upper_padding) {
    return TfLitePadding{.mode = ::tflite::Padding_SAME};
  }

  // The explicit padding are used to insert a TfLite PAD operator.
  return TfLitePadding{.mode = ::tflite::Padding_VALID,
                       .paddings = explicit_padding};
}

base::expected<::tflite::ActivationFunctionType, std::string>
GetActivationTypeForClamp(const mojom::Clamp& clamp) {
  ASSIGN_OR_RETURN(ClampRange clamp_range, GetClampRange(clamp));
  switch (clamp_range) {
    case ClampRange::kRelu:
      return ::tflite::ActivationFunctionType_RELU;
    case ClampRange::kRelu1:
      return ::tflite::ActivationFunctionType_RELU_N1_TO_1;
    case ClampRange::kRelu6:
      return ::tflite::ActivationFunctionType_RELU6;
  }
}

base::expected<::tflite::ActivationFunctionType, std::string>
GetActivationFunctionType(const mojom::Activation& activation) {
  switch (activation.which()) {
    case mojom::Activation::Tag::kClamp: {
      return GetActivationTypeForClamp(*activation.get_clamp());
    }
    case mojom::Activation::Tag::kRelu:
      return ::tflite::ActivationFunctionType_RELU;
    case mojom::Activation::Tag::kTanh:
      return ::tflite::ActivationFunctionType_TANH;
    case mojom::Activation::Tag::kElu:
      return base::unexpected("Elu activation is not supported.");
    case mojom::Activation::Tag::kGelu:
      return base::unexpected("Gelu activation is not supported.");
    case mojom::Activation::Tag::kHardSigmoid:
      return base::unexpected("HardSigmoid activation is not supported.");
    case mojom::Activation::Tag::kLeakyRelu:
      return base::unexpected("LeakyRelu activation is not supported.");
    case mojom::Activation::Tag::kLinear:
      return base::unexpected("Linear activation is not supported.");
    case mojom::Activation::Tag::kSigmoid:
      return base::unexpected("Sigmoid activation is not supported.");
    case mojom::Activation::Tag::kSoftmax:
      return base::unexpected("Softmax activation is not supported.");
    case mojom::Activation::Tag::kSoftplus:
      return base::unexpected("Softplus activation is not supported.");
    case mojom::Activation::Tag::kSoftsign:
      return base::unexpected("Softsign activation is not supported.");
  }
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
  ASSIGN_OR_RETURN(std::vector<int32_t> signed_operand_dimensions,
                   ToSignedDimensions(operand.dimensions));
  const flatbuffers::Offset<flatbuffers::Vector<int32_t>> dimensions =
      builder_.CreateVector<int32_t>(std::move(signed_operand_dimensions));
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
    case mojom::Operation::Tag::kArgMinMax: {
      ASSIGN_OR_RETURN(operator_offset,
                       SerializeArgMinMax(*op.get_arg_min_max()));
      break;
    }
    case mojom::Operation::Tag::kClamp: {
      ASSIGN_OR_RETURN(operator_offset, SerializeClamp(*op.get_clamp()));
      break;
    }
    case mojom::Operation::Tag::kConv2d: {
      ASSIGN_OR_RETURN(operator_offset, SerializeConv2d(*op.get_conv2d()));
      break;
    }
    case mojom::Operation::Tag::kConcat:
      operator_offset = SerializeConcat(*op.get_concat());
      break;
    case mojom::Operation::Tag::kElementWiseBinary: {
      operator_offset =
          SerializeElementWiseBinary(*op.get_element_wise_binary());
      break;
    }
    case mojom::Operation::Tag::kElementWiseUnary: {
      ASSIGN_OR_RETURN(operator_offset,
                       SerializeElementWiseUnary(*op.get_element_wise_unary()));
      break;
    }
    case mojom::Operation::Tag::kElu: {
      ASSIGN_OR_RETURN(operator_offset, SerializeElu(*op.get_elu()));
      break;
    }
    case mojom::Operation::Tag::kGather: {
      ASSIGN_OR_RETURN(operator_offset, SerializeGather(*op.get_gather()));
      break;
    }
    case mojom::Operation::Tag::kGemm: {
      ASSIGN_OR_RETURN(operator_offset, SerializeGemm(*op.get_gemm()));
      break;
    }
    case mojom::Operation::Tag::kHardSigmoid:
      operator_offset = SerializeHardSigmoid(*op.get_hard_sigmoid());
      break;
    case mojom::Operation::Tag::kHardSwish:
      operator_offset = SerializeHardSwish(*op.get_hard_swish());
      break;
    case mojom::Operation::Tag::kLeakyRelu:
      operator_offset = SerializeLeakyRelu(*op.get_leaky_relu());
      break;
    case mojom::Operation::Tag::kLinear:
      operator_offset = SerializeLinear(*op.get_linear());
      break;
    case mojom::Operation::Tag::kMatmul:
      operator_offset = SerializeMatmul(*op.get_matmul());
      break;
    case mojom::Operation::Tag::kPad: {
      ASSIGN_OR_RETURN(operator_offset, SerializePad(*op.get_pad()));
      break;
    }
    case mojom::Operation::Tag::kPool2d: {
      ASSIGN_OR_RETURN(operator_offset, SerializePool2d(*op.get_pool2d()));
      break;
    }
    case mojom::Operation::Tag::kPrelu: {
      ASSIGN_OR_RETURN(operator_offset, SerializePrelu(*op.get_prelu()));
      break;
    }
    case mojom::Operation::Tag::kReduce: {
      ASSIGN_OR_RETURN(operator_offset, SerializeReduce(*op.get_reduce()));
      break;
    }
    case mojom::Operation::Tag::kRelu:
      operator_offset = SerializeRelu(*op.get_relu());
      break;
    case mojom::Operation::Tag::kResample2d: {
      ASSIGN_OR_RETURN(operator_offset,
                       SerializeResample2d(*op.get_resample2d()));
      break;
    }
    case mojom::Operation::Tag::kReshape: {
      const mojom::Reshape& reshape = *op.get_reshape();
      ASSIGN_OR_RETURN(operator_offset,
                       SerializeReshape(reshape.input_operand_id,
                                        reshape.output_operand_id));
      break;
    }
    case mojom::Operation::Tag::kSigmoid:
      operator_offset = SerializeSigmoid(*op.get_sigmoid());
      break;
    case mojom::Operation::Tag::kSlice: {
      ASSIGN_OR_RETURN(operator_offset, SerializeSlice(*op.get_slice()));
      break;
    }
    case mojom::Operation::Tag::kSoftmax:
      operator_offset = SerializeSoftmax(*op.get_softmax());
      break;
    case mojom::Operation::Tag::kSoftplus:
      operator_offset = SerializeSoftplus(*op.get_softplus());
      break;
    case mojom::Operation::Tag::kSplit: {
      ASSIGN_OR_RETURN(operator_offset, SerializeSplit(*op.get_split()));
      break;
    }
    case mojom::Operation::Tag::kTanh:
      operator_offset = SerializeTanh(*op.get_tanh());
      break;
    case mojom::Operation::Tag::kTranspose:
      operator_offset = SerializeTranspose(*op.get_transpose());
      break;
    case mojom::Operation::Tag::kWhere:
      operator_offset = SerializeWhere(*op.get_where());
      break;
    case mojom::Operation::Tag::kBatchNormalization:
    case mojom::Operation::Tag::kExpand:
    case mojom::Operation::Tag::kGelu:
    case mojom::Operation::Tag::kGru:
    case mojom::Operation::Tag::kGruCell:
    case mojom::Operation::Tag::kLayerNormalization:
    case mojom::Operation::Tag::kInstanceNormalization:
    case mojom::Operation::Tag::kLstm:
    case mojom::Operation::Tag::kLstmCell:
    case mojom::Operation::Tag::kSoftsign:
    case mojom::Operation::Tag::kTriangular:
      return base::unexpected(NotSupportedOperatorError(op));
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
  requires internal::IsSupportedTensorType<DataType>
int32_t GraphBuilder::SerializeTensorWithBuffer(
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

int32_t GraphBuilder::SerializeTemporaryTensor(
    base::span<const int32_t> dimensions,
    ::tflite::TensorType tensor_type) {
  const int32_t temporary_tensor_index =
      base::checked_cast<int32_t>(tensors_.size());
  tensors_.emplace_back(::tflite::CreateTensor(
      builder_, builder_.CreateVector<int32_t>(dimensions), tensor_type));

  return temporary_tensor_index;
}

uint32_t GraphBuilder::GetOperatorCodeIndex(::tflite::BuiltinOperator code) {
  // New builtin operators, whose operator code is larger than 127, can not be
  // assigned to the `deprecated_code` field. In such cases, the value of the
  // `code` field should be used for the builtin operator code, the value 127
  // will be the value of the `deprecated_code`.
  const ::tflite::BuiltinOperator deprecated_code = std::min(
      code, ::tflite::BuiltinOperator_PLACEHOLDER_FOR_GREATER_OP_CODES);

  auto operator_code_index =
      base::checked_cast<uint32_t>(operator_codes_.size());
  operator_codes_.push_back(::tflite::CreateOperatorCode(
      builder_, base::checked_cast<int8_t>(deprecated_code),
      /*custom_code=*/0, /*version=*/1, code));

  // The type of operation is determined by the index into the list of the valid
  // OperatorCodes.
  return operator_code_index;
}

const mojom::Operand& GraphBuilder::GetOperand(uint64_t operand_id) const {
  return *graph_info_->id_to_operand_map.at(operand_id);
}

auto GraphBuilder::SerializeUnaryOperation(
    ::tflite::BuiltinOperator code,
    int32_t input_tensor_index,
    int32_t output_tensor_index,
    ::tflite::BuiltinOptions builtin_options_type,
    flatbuffers::Offset<void> builtin_options) -> OperatorOffset {
  CHECK_EQ(builtin_options_type == ::tflite::BuiltinOptions_NONE,
           builtin_options.IsNull());

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const uint32_t operator_code_index = GetOperatorCodeIndex(code);
  const std::array<int32_t, 1> op_inputs = {input_tensor_index};
  const std::array<int32_t, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs),
                                  builtin_options_type, builtin_options);
}

auto GraphBuilder::SerializeCastOperation(
    int32_t input_tensor_index,
    ::tflite::TensorType input_tensor_type,
    int32_t output_tensor_index,
    ::tflite::TensorType output_tensor_type) -> OperatorOffset {
  const auto cast_options = ::tflite::CreateCastOptions(
      builder_, input_tensor_type, output_tensor_type);

  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_CAST);
  const std::array<int32_t, 1> op_inputs = {input_tensor_index};
  const std::array<int32_t, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index, builder_.CreateVector<int32_t>(op_inputs),
      builder_.CreateVector<int32_t>(op_outputs),
      ::tflite::BuiltinOptions_CastOptions, cast_options.Union());
}

auto GraphBuilder::SerializeBinaryOperation(::tflite::BuiltinOperator code,
                                            int32_t lhs_tensor_index,
                                            int32_t rhs_tensor_index,
                                            int32_t output_tensor_index)
    -> OperatorOffset {
  const uint32_t operator_code_index = GetOperatorCodeIndex(code);
  const std::array<int32_t, 2> op_inputs = {lhs_tensor_index, rhs_tensor_index};
  const std::array<int32_t, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs));
}

auto GraphBuilder::SerializeLinearOperation(
    base::span<const int32_t> input_dimensions,
    ::tflite::TensorType input_tensor_type,
    int32_t input_tensor_index,
    int32_t output_tensor_index,
    float alpha,
    float beta) -> OperatorOffset {
  // Emulate a linear operation whose calculation follows the expression `alpha
  // * x + beta`.
  const int32_t alpha_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{alpha},
      /*dimensions=*/{});
  const int32_t output_tensor_index_of_mul =
      SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_MUL, input_tensor_index, alpha_tensor_index,
      output_tensor_index_of_mul));

  const int32_t beta_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{beta},
      /*dimensions=*/{});
  return SerializeBinaryOperation(::tflite::BuiltinOperator_ADD,
                                  beta_tensor_index, output_tensor_index_of_mul,
                                  output_tensor_index);
}

auto GraphBuilder::SerializeTransposeOperation(
    int32_t input_tensor_index,
    int32_t output_tensor_index,
    base::span<const uint32_t> permutation) -> OperatorOffset {
  const std::array<int32_t, 1> permutation_shape = {
      base::checked_cast<int32_t>(permutation.size())};
  const int32_t permutation_tensor_index =
      SerializeTensorWithBuffer<uint32_t>(permutation, permutation_shape);

  const auto operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_TRANSPOSE);
  const std::array<int32_t, 2> op_inputs = {input_tensor_index,
                                            permutation_tensor_index};
  const std::array<int32_t, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs));
}

auto GraphBuilder::InsertPadOperation(const mojom::Operand& input_operand,
                                      int32_t input_tensor_index,
                                      base::span<const uint32_t> paddings)
    -> base::expected<int32_t, std::string> {
  // WebNN explicit padding is in [beginning_height, ending_height,
  // beginning_width, ending_width] sequence.
  const auto padding_rank = paddings.size();
  CHECK_EQ(padding_rank, 4u);

  // Create `tflite::Tensor` for the output operand of explicit padding operator
  // with the dimensions and data type.
  const std::vector<uint32_t>& input_shape = input_operand.dimensions;
  CHECK_EQ(input_shape.size(), 4u);
  std::vector<int32_t> output_shape;
  output_shape.reserve(padding_rank);
  for (size_t i = 0; i < padding_rank; ++i) {
    auto checked_dimension = base::MakeCheckedNum<int32_t>(input_shape[i]);
    // Calculate output height with padding beginning and ending height.
    if (i == 1) {
      checked_dimension +=
          base::MakeCheckedNum<int32_t>(paddings[0]) + paddings[1];
    } else if (i == 2) {
      // Calculate output width with padding beginning and ending width.
      checked_dimension +=
          base::MakeCheckedNum<int32_t>(paddings[2]) + paddings[3];
    }
    if (!checked_dimension.IsValid()) {
      return base::unexpected("The input dimension or padding is too large.");
    }
    output_shape.push_back(checked_dimension.ValueOrDie());
  }

  const ::tflite::TensorType input_tensor_type =
      MojoOperandTypeToTFLite(input_operand.data_type);
  const int32_t output_tensor_index =
      base::checked_cast<int32_t>(tensors_.size());
  tensors_.emplace_back(::tflite::CreateTensor(
      builder_, builder_.CreateVector<int32_t>(output_shape),
      input_tensor_type));

  // TfLite padding is a signed integer tensor array filled with pre and post
  // padding. For NHWC input layout, the sequence will be [[0, 0],
  // [beginning_height, ending_height], [beginning_width, ending_width], [0,
  // 0]].
  std::array<int32_t, 8> tflite_paddings = {};
  base::ranges::copy(paddings, tflite_paddings.begin() + 2);

  // The shape of padding is [n, 2], where n is the rank of input as described
  // here https://www.tensorflow.org/mlir/tfl_ops#tflmirror_pad_tflmirrorpadop.
  std::array<int32_t, 2> paddings_shape = {
      base::checked_cast<int32_t>(padding_rank), 2};
  const int32_t padding_tensor_index = SerializeTensorWithBuffer<int32_t>(
      std::move(tflite_paddings), std::move(paddings_shape));

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const auto operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_PAD);
  std::array<int32_t, 2> op_inputs = {input_tensor_index, padding_tensor_index};
  const std::array<int32_t, 1> op_outputs = {output_tensor_index};
  operators_.emplace_back(::tflite::CreateOperator(
      builder_, operator_code_index, builder_.CreateVector<int32_t>(op_inputs),
      builder_.CreateVector<int32_t>(op_outputs)));

  return output_tensor_index;
}

int32_t GraphBuilder::InsertTransposeOperation(
    const mojom::Operand& input_operand,
    int32_t input_tensor_index,
    base::span<const uint32_t> permutation) {
  // Create `tflite::Tensor` for the output operand of Transpose operator with
  // the dimensions and tensor data type.
  const std::vector<uint32_t>& input_shape = input_operand.dimensions;
  const size_t input_rank = input_shape.size();
  CHECK_EQ(permutation.size(), input_rank);
  std::vector<int32_t> output_shape;
  output_shape.reserve(input_rank);
  for (size_t i = 0; i < input_rank; ++i) {
    // The input shape has been validated the overflow before creating tensor.
    output_shape.push_back(
        base::checked_cast<int32_t>(input_shape[permutation[i]]));
  }
  const ::tflite::TensorType input_tensor_type =
      MojoOperandTypeToTFLite(input_operand.data_type);
  const int32_t output_tensor_index =
      base::checked_cast<int32_t>(tensors_.size());
  tensors_.emplace_back(::tflite::CreateTensor(
      builder_, builder_.CreateVector<int32_t>(output_shape),
      input_tensor_type));

  operators_.emplace_back(SerializeTransposeOperation(
      input_tensor_index, output_tensor_index, permutation));

  return output_tensor_index;
}

auto GraphBuilder::SerializeArgMinMax(const mojom::ArgMinMax& arg_min_max)
    -> base::expected<OperatorOffset, std::string> {
  // The axis is a scalar constraint in arg_min_max::Prepare() function here
  // third_party/tflite/src/tensorflow/lite/kernels/arg_min_max.cc, the tensor
  // axes are being discussed in the working group here
  // https://github.com/webmachinelearning/webnn/issues/629.
  // TODO(crbug.com/331977830): Support empty axis that means no dimensions are
  // reduced.
  if (arg_min_max.axes.size() != 1) {
    return base::unexpected(OpKindToString(arg_min_max.kind) +
                            ": Only supports scalar axis.");
  }
  if (arg_min_max.select_last_index) {
    return base::unexpected(OpKindToString(arg_min_max.kind) +
                            ": Only first index can be selected.");
  }
  ASSIGN_OR_RETURN(std::vector<int32_t> signed_axes,
                   ToSignedDimensions(arg_min_max.axes));
  const std::array<int32_t, 1> axis_tensor_shape = {
      base::checked_cast<int32_t>(signed_axes.size())};
  const int32_t axis_tensor_index =
      SerializeTensorWithBuffer<int32_t>(signed_axes, axis_tensor_shape);

  ::tflite::BuiltinOperator operator_code;
  ::tflite::BuiltinOptions builtin_options_type;
  flatbuffers::Offset<void> builtin_options;
  ::tflite::TensorType output_type = ::tflite::TensorType_INT64;
  switch (arg_min_max.kind) {
    case mojom::ArgMinMax::Kind::kMax: {
      operator_code = ::tflite::BuiltinOperator_ARG_MAX;
      builtin_options_type = ::tflite::BuiltinOptions_ArgMaxOptions;
      builtin_options =
          ::tflite::CreateArgMaxOptions(builder_, output_type).Union();
      break;
    }
    case mojom::ArgMinMax::Kind::kMin: {
      operator_code = ::tflite::BuiltinOperator_ARG_MIN;
      builtin_options_type = ::tflite::BuiltinOptions_ArgMinOptions;
      builtin_options =
          ::tflite::CreateArgMinOptions(builder_, output_type).Union();
      break;
    }
  }

  const uint32_t operator_code_index = GetOperatorCodeIndex(operator_code);
  const std::array<int32_t, 2> op_inputs = {
      operand_to_index_map_.at(arg_min_max.input_operand_id),
      axis_tensor_index};
  const std::array<int32_t, 1> op_outputs = {
      operand_to_index_map_.at(arg_min_max.output_operand_id)};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs),
                                  builtin_options_type, builtin_options);
}

auto GraphBuilder::SerializeClamp(const mojom::Clamp& clamp)
    -> base::expected<OperatorOffset, std::string> {
  ASSIGN_OR_RETURN(ClampRange clamp_range, GetClampRange(clamp));
  ::tflite::BuiltinOperator code;
  switch (clamp_range) {
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

  return SerializeUnaryOperation(
      code, operand_to_index_map_.at(clamp.input_operand_id),
      operand_to_index_map_.at(clamp.output_operand_id));
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

  // Create `tflite::ConcatenationOptions` with axis.
  const auto concat_options =
      ::tflite::CreateConcatenationOptions(builder_, concat.axis);

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_CONCATENATION);
  const std::array<int32_t, 1> operator_outputs = {
      operand_to_index_map_.at(concat.output_operand_id)};
  return ::tflite::CreateOperator(
      builder_, operator_code_index, operator_inputs_index,
      builder_.CreateVector<int32_t>(operator_outputs),
      ::tflite::BuiltinOptions_ConcatenationOptions, concat_options.Union());
}

auto GraphBuilder::SerializeConv2d(const mojom::Conv2d& conv2d)
    -> base::expected<OperatorOffset, std::string> {
  if (conv2d.kind != mojom::Conv2d::Kind::kDirect) {
    return base::unexpected("convTranspose2d is not implemented.");
  }
  // TODO(crbug.com/327941466): Transpose input operand to support other layouts
  // because tflite only support nhwc layout.
  if (conv2d.input_layout != mojom::InputOperandLayout::kChannelsLast) {
    return base::unexpected("The channel first input layout is not supported.");
  }

  const mojom::Operand& input_operand = GetOperand(conv2d.input_operand_id);
  // TODO(crbug.com/328733319): Support other tensor data types.
  if (input_operand.data_type != mojom::Operand::DataType::kFloat32) {
    return base::unexpected("The data type of input is not supported.");
  }

  const auto& input_shape = input_operand.dimensions;
  CHECK_EQ(input_shape.size(), 4u);
  const uint32_t input_channels = input_shape[3];
  const mojom::Operand& output_operand = GetOperand(conv2d.output_operand_id);
  const auto& output_shape = output_operand.dimensions;
  CHECK_EQ(output_shape.size(), 4u);
  const uint32_t output_channels = output_shape[3];
  const bool depthwise =
      webnn::IsDepthwiseConv2d(input_channels, output_channels, conv2d.groups);

  // Validate activation operator that is partial supported in tflite schema and
  // convert to tflite function type.
  ::tflite::ActivationFunctionType activation =
      ::tflite::ActivationFunctionType_NONE;
  if (conv2d.activation) {
    ASSIGN_OR_RETURN(activation, GetActivationFunctionType(*conv2d.activation));
  }

  // Get tflite padding mode with the size2d of input, filter, dilation.
  const webnn::Size2d<uint32_t> input_size2d = {.height = input_shape[1],
                                                .width = input_shape[2]};
  // For nhwc input layout, the default filter layout is ohwi for regular conv2d
  // and ihwo for depthwise conv2d.
  const mojom::Operand& filter_operand = GetOperand(conv2d.filter_operand_id);
  CHECK_EQ(filter_operand.dimensions.size(), 4u);
  const auto& filter_shape = filter_operand.dimensions;
  CHECK_EQ(filter_shape.size(), 4u);
  const webnn::Size2d<uint32_t> filter_size2d = {.height = filter_shape[1],
                                                 .width = filter_shape[2]};
  ASSIGN_OR_RETURN(
      TfLitePadding padding_mode,
      GetTfLitePaddingMode(*conv2d.padding, input_size2d, filter_size2d,
                           *conv2d.strides, *conv2d.dilations));
  const int32_t input_index = operand_to_index_map_.at(conv2d.input_operand_id);
  // Insert a Pad operator before TfLite Conv2d if needed for explicit padding.
  std::optional<int32_t> explicit_pad_index;
  if (padding_mode.paddings) {
    ASSIGN_OR_RETURN(explicit_pad_index,
                     InsertPadOperation(input_operand, input_index,
                                        padding_mode.paddings.value()));
  }

  ::tflite::BuiltinOperator operator_kind;
  ::tflite::BuiltinOptions builtin_options_type;
  flatbuffers::Offset<void> builtin_options;
  if (depthwise) {
    const uint32_t depth_multiplier = 1;
    operator_kind = ::tflite::BuiltinOperator_DEPTHWISE_CONV_2D;
    builtin_options = ::tflite::CreateDepthwiseConv2DOptions(
                          builder_, padding_mode.mode, conv2d.strides->width,
                          conv2d.strides->height, depth_multiplier, activation,
                          conv2d.dilations->width, conv2d.dilations->height)
                          .Union();
    builtin_options_type = ::tflite::BuiltinOptions_DepthwiseConv2DOptions;
  } else {
    operator_kind = ::tflite::BuiltinOperator_CONV_2D;
    builtin_options = ::tflite::CreateConv2DOptions(
                          builder_, padding_mode.mode, conv2d.strides->width,
                          conv2d.strides->height, activation,
                          conv2d.dilations->width, conv2d.dilations->height)
                          .Union();
    builtin_options_type = ::tflite::BuiltinOptions_Conv2DOptions;
  }

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const auto operator_code_index = GetOperatorCodeIndex(operator_kind);
  // If there is no bias operand, serialize a empty buffer with the size of
  // output channel.
  int32_t bias_index;
  if (conv2d.bias_operand_id) {
    bias_index = operand_to_index_map_.at(conv2d.bias_operand_id.value());
  } else {
    const std::array<int32_t, 1> bias_shape = {
        base::checked_cast<int32_t>(output_channels)};
    bias_index = SerializeTensorWithBuffer<float>(
        std::vector<float>(output_channels), std::move(bias_shape));
  }

  const std::array<int32_t, 3> op_inputs = {
      explicit_pad_index ? explicit_pad_index.value() : input_index,
      operand_to_index_map_.at(conv2d.filter_operand_id), bias_index};
  const std::array<int32_t, 1> op_outputs = {
      operand_to_index_map_.at(conv2d.output_operand_id)};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs),
                                  builtin_options_type, builtin_options);
}

auto GraphBuilder::SerializeElementWiseBinary(
    const mojom::ElementWiseBinary& op) -> OperatorOffset {
  ::tflite::BuiltinOperator code;
  switch (op.kind) {
    case mojom::ElementWiseBinary::Kind::kAdd:
      code = ::tflite::BuiltinOperator_ADD;
      break;
    case mojom::ElementWiseBinary::Kind::kSub:
      code = ::tflite::BuiltinOperator_SUB;
      break;
    case mojom::ElementWiseBinary::Kind::kMul:
      code = ::tflite::BuiltinOperator_MUL;
      break;
    case mojom::ElementWiseBinary::Kind::kDiv:
      code = ::tflite::BuiltinOperator_DIV;
      break;
    case mojom::ElementWiseBinary::Kind::kMax:
      code = ::tflite::BuiltinOperator_MAXIMUM;
      break;
    case mojom::ElementWiseBinary::Kind::kMin:
      code = ::tflite::BuiltinOperator_MINIMUM;
      break;
    case mojom::ElementWiseBinary::Kind::kPow:
      code = ::tflite::BuiltinOperator_POW;
      break;
    case mojom::ElementWiseBinary::Kind::kEqual:
      code = ::tflite::BuiltinOperator_EQUAL;
      break;
    case mojom::ElementWiseBinary::Kind::kGreater:
      code = ::tflite::BuiltinOperator_GREATER;
      break;
    case mojom::ElementWiseBinary::Kind::kGreaterOrEqual:
      code = ::tflite::BuiltinOperator_GREATER_EQUAL;
      break;
    case mojom::ElementWiseBinary::Kind::kLesser:
      code = ::tflite::BuiltinOperator_LESS;
      break;
    case mojom::ElementWiseBinary::Kind::kLesserOrEqual:
      code = ::tflite::BuiltinOperator_LESS_EQUAL;
      break;
  }

  return SerializeBinaryOperation(
      code, operand_to_index_map_.at(op.lhs_operand_id),
      operand_to_index_map_.at(op.rhs_operand_id),
      operand_to_index_map_.at(op.output_operand_id));
}

auto GraphBuilder::SerializeElementWiseUnary(const mojom::ElementWiseUnary& op)
    -> base::expected<OperatorOffset, std::string> {
  const int32_t input_tensor_index =
      operand_to_index_map_.at(op.input_operand_id);
  const int32_t output_tensor_index =
      operand_to_index_map_.at(op.output_operand_id);
  const mojom::Operand::DataType input_data_type =
      GetOperand(op.input_operand_id).data_type;
  switch (op.kind) {
    case mojom::ElementWiseUnary::Kind::kAbs:
      CHECK(kFloatDataTypes.contains(input_data_type) ||
            input_data_type == mojom::Operand::DataType::kInt32 ||
            input_data_type == mojom::Operand::DataType::kInt8);
      return SerializeUnaryOperation(::tflite::BuiltinOperator_ABS,
                                     input_tensor_index, output_tensor_index);
    case mojom::ElementWiseUnary::Kind::kCeil:
      CHECK(kFloatDataTypes.contains(input_data_type));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_CEIL,
                                     input_tensor_index, output_tensor_index);
    case mojom::ElementWiseUnary::Kind::kCos:
      CHECK(kFloatDataTypes.contains(input_data_type));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_COS,
                                     input_tensor_index, output_tensor_index);
    case mojom::ElementWiseUnary::Kind::kExp:
      CHECK(kFloatDataTypes.contains(input_data_type));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_EXP,
                                     input_tensor_index, output_tensor_index);
    case mojom::ElementWiseUnary::Kind::kFloor:
      CHECK(kFloatDataTypes.contains(input_data_type));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_FLOOR,
                                     input_tensor_index, output_tensor_index);
    case mojom::ElementWiseUnary::Kind::kLog:
      CHECK(kFloatDataTypes.contains(input_data_type));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_LOG,
                                     input_tensor_index, output_tensor_index);
    case mojom::ElementWiseUnary::Kind::kNeg:
      CHECK(kFloatDataTypes.contains(input_data_type) ||
            input_data_type == mojom::Operand::DataType::kInt32 ||
            input_data_type == mojom::Operand::DataType::kInt8);
      return SerializeUnaryOperation(::tflite::BuiltinOperator_NEG,
                                     input_tensor_index, output_tensor_index);
    case mojom::ElementWiseUnary::Kind::kSin:
      CHECK(kFloatDataTypes.contains(input_data_type));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_SIN,
                                     input_tensor_index, output_tensor_index);
    case mojom::ElementWiseUnary::Kind::kSqrt:
      CHECK(kFloatDataTypes.contains(input_data_type));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_SQRT,
                                     input_tensor_index, output_tensor_index);
    case mojom::ElementWiseUnary::Kind::kCast:
      return SerializeCastOperation(
          input_tensor_index,
          MojoOperandTypeToTFLite(GetOperand(op.input_operand_id).data_type),
          output_tensor_index,
          MojoOperandTypeToTFLite(GetOperand(op.output_operand_id).data_type));
    case mojom::ElementWiseUnary::Kind::kLogicalNot:
      CHECK_EQ(input_data_type, mojom::Operand::DataType::kUint8);
      return SerializeLogicalNot(op);
    case mojom::ElementWiseUnary::Kind::kIdentity:
      // Implement WebNN identity operation with TFLite reshape operator, the
      // output shape is the same as input.
      // TODO(crbug.com/336399247): Skip identity implementation with
      // redirecting output tensor to input.
      return SerializeReshape(op.input_operand_id, op.output_operand_id);
    case mojom::ElementWiseUnary::Kind::kTan:
      CHECK(kFloatDataTypes.contains(input_data_type));
      return SerializeTan(op);
    case mojom::ElementWiseUnary::Kind::kErf:
    case mojom::ElementWiseUnary::Kind::kReciprocal:
      CHECK(kFloatDataTypes.contains(input_data_type));
      return base::unexpected(
          base::StrCat({base::ToString(op.kind), " is not implemented."}));
  }
}

auto GraphBuilder::SerializeElu(const mojom::Elu& elu)
    -> base::expected<OperatorOffset, std::string> {
  if (elu.alpha != 1.0) {
    // TODO: crbug.com/328736354 - Support custom alpha values.
    return base::unexpected(
        "Setting a custom alpha is not supported in tflite schema.");
  }
  return SerializeUnaryOperation(
      ::tflite::BuiltinOperator_ELU,
      operand_to_index_map_.at(elu.input_operand_id),
      operand_to_index_map_.at(elu.output_operand_id));
}

auto GraphBuilder::SerializeGather(const mojom::Gather& gather)
    -> base::expected<OperatorOffset, std::string> {
  // The WebNN indices must be one of type uint32 or int64, but TFLite indices
  // need int32 or int64 type, so a cast operation need to be inserted before
  // Gather if indices data type is uint32.
  int32_t indices_tensor_index =
      operand_to_index_map_.at(gather.indices_operand_id);
  const mojom::Operand& indices_operand = GetOperand(gather.indices_operand_id);
  if (indices_operand.data_type == mojom::Operand::DataType::kUint32) {
    ASSIGN_OR_RETURN(const std::vector<int32_t> signed_indices_dimensions,
                     ToSignedDimensions(indices_operand.dimensions));
    indices_tensor_index = SerializeTemporaryTensor(signed_indices_dimensions,
                                                    ::tflite::TensorType_INT64);

    operators_.emplace_back(SerializeCastOperation(
        operand_to_index_map_.at(gather.indices_operand_id),
        /*input_tensor_type=*/::tflite::TensorType_UINT32, indices_tensor_index,
        /*output_tensor_type=*/::tflite::TensorType_INT64));
  } else {
    CHECK_EQ(indices_operand.data_type, mojom::Operand::DataType::kInt64);
  }

  // The WebNN axis option is uint32 data type, but TFLite axis needs int32
  // type, so the axis need to be validated here to not overflow.
  auto checked_axis = base::MakeCheckedNum<int32_t>(gather.axis);
  if (!checked_axis.IsValid()) {
    return base::unexpected("The axis in gather operation is too large.");
  }
  const auto gather_options =
      ::tflite::CreateGatherOptions(builder_, checked_axis.ValueOrDie());

  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_GATHER);
  const std::array<int32_t, 2> op_inputs = {
      operand_to_index_map_.at(gather.input_operand_id), indices_tensor_index};
  const std::array<int32_t, 1> op_outputs = {
      operand_to_index_map_.at(gather.output_operand_id)};
  return ::tflite::CreateOperator(
      builder_, operator_code_index, builder_.CreateVector<int32_t>(op_inputs),
      builder_.CreateVector<int32_t>(op_outputs),
      ::tflite::BuiltinOptions_GatherOptions, gather_options.Union());
}

auto GraphBuilder::SerializeGemm(const mojom::Gemm& gemm)
    -> base::expected<OperatorOffset, std::string> {
  // Check for unsupported inputs.
  const mojom::Operand& output_operand = GetOperand(gemm.output_operand_id);
  CHECK_EQ(output_operand.dimensions.size(), 2u);
  CHECK(kFloatDataTypes.contains(output_operand.data_type));
  const uint32_t output_channels = output_operand.dimensions[1];
  if (gemm.c_operand_id.has_value()) {
    // The TFLite fully connected operator only supports a 1-D bias tensor with
    // `output_channels` dimensions.
    const mojom::Operand& bias_operand = GetOperand(*gemm.c_operand_id);
    if (bias_operand.dimensions.size() != 1u ||
        bias_operand.dimensions[0] != output_channels) {
      // TODO(crbug.com/328652105): Support the bias with other dimensions by
      // element-wise addition operator.
      return base::unexpected(base::StringPrintf(
          "The dimensions of bias must be [%u].", output_channels));
    }
  }
  if (gemm.alpha != 1.0f) {
    // TODO(crbug.com/328652105): Support alpha by using element-wise
    // multiplication operator.
    return base::unexpected("gemm doesn't support alpha option.");
  }
  if (gemm.beta != 1.0f) {
    // TODO(crbug.com/328652105): Support beta by using element-wise
    // multiplication operator.
    return base::unexpected("gemm doesn't support beta option.");
  }
  if (gemm.a_transpose) {
    // TODO(crbug.com/328652105): Support aTranspose by using transpose
    // operator.
    return base::unexpected("gemm doesn't support aTranspose option.");
  }

  // The WebNN Gemm follows the expression `alpha * A * B + beta * C`, where
  // A is a 2-D tensor with shape [M, K], B is a 2-D tensor with shape [K,
  // N] by default options, but Tflite Fully Connected's input and filter
  // shapes are [batch, input_channels] and [output_channels,
  // input_channels], so the Transpose operator need to be inserted before
  // Gemm When bTranspose option is false.
  std::optional<int32_t> transposed_filter_index;
  const uint32_t filter_operand_id = gemm.b_operand_id;
  const int32_t filter_index = operand_to_index_map_.at(filter_operand_id);
  if (!gemm.b_transpose) {
    const mojom::Operand& filter_operand = GetOperand(filter_operand_id);
    CHECK_EQ(filter_operand.dimensions.size(), 2u);

    const std::array<uint32_t, 2> permutation = {1u, 0u};
    transposed_filter_index =
        InsertTransposeOperation(filter_operand, filter_index, permutation);
  }

  std::vector<int32_t> op_inputs = {operand_to_index_map_.at(gemm.a_operand_id),
                                    transposed_filter_index.has_value()
                                        ? *transposed_filter_index
                                        : filter_index};
  if (gemm.c_operand_id.has_value()) {
    op_inputs.push_back(operand_to_index_map_.at(*gemm.c_operand_id));
  }

  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_FULLY_CONNECTED);
  const std::array<int32_t, 1> op_outputs = {
      operand_to_index_map_.at(gemm.output_operand_id)};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs));
}

auto GraphBuilder::SerializeHardSigmoid(const mojom::HardSigmoid& hard_sigmoid)
    -> OperatorOffset {
  // Emulate the hardSigmoid operation with function `y = max(0, min(1, alpha *
  // x + beta))` that is applied to the input tensor element-wise.
  //
  // The subset expression `alpha * x + beta` is considered a linear operation.
  const mojom::Operand& input_operand =
      GetOperand(hard_sigmoid.input_operand_id);
  CHECK(input_operand.data_type == mojom::Operand::DataType::kFloat16 ||
        input_operand.data_type == mojom::Operand::DataType::kFloat32);
  // The input shape has been validated to not overflow before creating tensor.
  const auto signed_input_dimensions =
      ToSignedDimensions(input_operand.dimensions);
  CHECK(signed_input_dimensions.has_value());
  const ::tflite::TensorType input_tensor_type =
      MojoOperandTypeToTFLite(input_operand.data_type);
  const int32_t output_tensor_index_of_linear =
      SerializeTemporaryTensor(*signed_input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeLinearOperation(
      *signed_input_dimensions, input_tensor_type,
      operand_to_index_map_.at(hard_sigmoid.input_operand_id),
      output_tensor_index_of_linear, hard_sigmoid.alpha, hard_sigmoid.beta));

  // The expression `max(0, min(1, linear))` can be implemented with TFLite
  // RELU_0_TO_1 operator.
  return SerializeUnaryOperation(
      ::tflite::BuiltinOperator_RELU_0_TO_1, output_tensor_index_of_linear,
      operand_to_index_map_.at(hard_sigmoid.output_operand_id));
}

auto GraphBuilder::SerializeHardSwish(const mojom::HardSwish& hard_swish)
    -> OperatorOffset {
  return SerializeUnaryOperation(
      ::tflite::BuiltinOperator_HARD_SWISH,
      operand_to_index_map_.at(hard_swish.input_operand_id),
      operand_to_index_map_.at(hard_swish.output_operand_id));
}

auto GraphBuilder::SerializeLeakyRelu(const mojom::LeakyRelu& leaky_relu)
    -> OperatorOffset {
  const auto leaky_rely_options =
      ::tflite::CreateLeakyReluOptions(builder_, leaky_relu.alpha);

  return SerializeUnaryOperation(
      ::tflite::BuiltinOperator_LEAKY_RELU,
      operand_to_index_map_.at(leaky_relu.input_operand_id),
      operand_to_index_map_.at(leaky_relu.output_operand_id),
      ::tflite::BuiltinOptions_LeakyReluOptions, leaky_rely_options.Union());
}

auto GraphBuilder::SerializeLinear(const mojom::Linear& linear)
    -> OperatorOffset {
  const auto& input_operand = GetOperand(linear.input_operand_id);
  // The input shape has been validated to not overflow before creating tensor.
  const auto signed_input_dimensions =
      ToSignedDimensions(input_operand.dimensions);
  CHECK(signed_input_dimensions.has_value());
  return SerializeLinearOperation(
      *signed_input_dimensions,
      MojoOperandTypeToTFLite(input_operand.data_type),
      operand_to_index_map_.at(linear.input_operand_id),
      operand_to_index_map_.at(linear.output_operand_id), linear.alpha,
      linear.beta);
}

auto GraphBuilder::SerializeLogicalNot(
    const mojom::ElementWiseUnary& logical_not) -> OperatorOffset {
  // The data type of WebNN LogicalNot operation is uint8, but TFLite LogicalNot
  // builtin operation needs bool type, so a cast operation need to be inserted
  // before LogicalNot to convert uint8 to bool for input tensor and a cast
  // operation after LogicalNot to convert bool to uint8 for output tensor.
  //
  // Create two temporary tensors with bool type for TFLite LogicalNot.
  std::array<int32_t, 2> bool_tensor_indexes;
  const auto& input_operand = GetOperand(logical_not.input_operand_id);
  // The input shape has been validated to not overflow before creating tensor.
  const auto signed_input_dimensions =
      ToSignedDimensions(input_operand.dimensions);
  CHECK(signed_input_dimensions.has_value());
  for (auto& bool_tensor_index : bool_tensor_indexes) {
    bool_tensor_index = SerializeTemporaryTensor(*signed_input_dimensions,
                                                 ::tflite::TensorType_BOOL);
  }

  CHECK_EQ(input_operand.data_type, mojom::Operand::DataType::kUint8);
  operators_.emplace_back(SerializeCastOperation(
      operand_to_index_map_.at(logical_not.input_operand_id),
      /*input_tensor_type=*/::tflite::TensorType_UINT8, bool_tensor_indexes[0],
      /*output_tensor_type=*/::tflite::TensorType_BOOL));

  // Serialize TFLite LogicalNot operation.
  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_LOGICAL_NOT);
  const std::array<int32_t, 1> op_inputs = {bool_tensor_indexes[0]};
  const std::array<int32_t, 1> op_outputs = {bool_tensor_indexes[1]};
  operators_.emplace_back(::tflite::CreateOperator(
      builder_, operator_code_index, builder_.CreateVector<int32_t>(op_inputs),
      builder_.CreateVector<int32_t>(op_outputs)));

  return SerializeCastOperation(
      bool_tensor_indexes[1],
      /*input_tensor_type=*/::tflite::TensorType_BOOL,
      operand_to_index_map_.at(logical_not.output_operand_id),
      /*output_tensor_type=*/::tflite::TensorType_UINT8);
}

auto GraphBuilder::SerializeMatmul(const mojom::Matmul& matmul)
    -> OperatorOffset {
  const mojom::Operand::DataType a_operand_data_type =
      GetOperand(matmul.a_operand_id).data_type;
  CHECK(kFloatDataTypes.contains(a_operand_data_type));

  const auto matmul_options =
      ::tflite::CreateBatchMatMulOptions(builder_, /*adj_x=*/false,
                                         /*adj_y=*/false);
  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_BATCH_MATMUL);
  const std::array<int32_t, 2> op_inputs = {
      operand_to_index_map_.at(matmul.a_operand_id),
      operand_to_index_map_.at(matmul.b_operand_id)};
  const std::array<int32_t, 1> op_outputs = {
      operand_to_index_map_.at(matmul.output_operand_id)};
  return ::tflite::CreateOperator(
      builder_, operator_code_index, builder_.CreateVector<int32_t>(op_inputs),
      builder_.CreateVector<int32_t>(op_outputs),
      ::tflite::BuiltinOptions_BatchMatMulOptions, matmul_options.Union());
}

auto GraphBuilder::SerializePad(const mojom::Pad& pad)
    -> base::expected<OperatorOffset, std::string> {
  CHECK_EQ(pad.beginning_padding.size(), pad.ending_padding.size());

  std::vector<int32_t> paddings;
  paddings.resize(pad.beginning_padding.size() * 2);
  for (size_t i = 0; i < pad.beginning_padding.size(); ++i) {
    auto checked_pre_padding =
        base::MakeCheckedNum<int32_t>(pad.beginning_padding[i]);
    auto checked_post_padding =
        base::MakeCheckedNum<int32_t>(pad.ending_padding[i]);
    if (!checked_pre_padding.IsValid() || !checked_post_padding.IsValid()) {
      return base::unexpected("The padding is too large.");
    }
    paddings[i * 2] = checked_pre_padding.ValueOrDie();
    paddings[i * 2 + 1] = checked_post_padding.ValueOrDie();
  }

  // The shape of padding is [n, 2], where n is the rank of input as described
  // here https://www.tensorflow.org/mlir/tfl_ops#tflmirror_pad_tflmirrorpadop.
  std::array<int32_t, 2> paddings_shape{
      {base::checked_cast<int32_t>(pad.beginning_padding.size()), 2}};
  const int32_t paddings_index =
      SerializeTensorWithBuffer<int32_t>(paddings, paddings_shape);

  std::vector<int32_t> op_inputs = {
      operand_to_index_map_.at(pad.input_operand_id), paddings_index};

  ::tflite::BuiltinOptions builtin_options_type =
      ::tflite::BuiltinOptions::BuiltinOptions_NONE;
  flatbuffers::Offset<void> builtin_options;
  ::tflite::BuiltinOperator operator_code;
  switch (pad.mode->which()) {
    case mojom::PaddingMode::Tag::kConstant: {
      operator_code = ::tflite::BuiltinOperator::BuiltinOperator_PADV2;
      builtin_options_type =
          ::tflite::BuiltinOptions::BuiltinOptions_PadV2Options;
      builtin_options = ::tflite::CreatePadV2Options(builder_).Union();

      // Add the padding value as an input.
      //
      // TODO: crbug.com/328567884 - This is not correct to always use floats,
      // though for now WebNN only supports passing a float32 constant value.
      // https://www.tensorflow.org/mlir/tfl_ops#tflpadv2_tflpadv2op specifies
      // that this constant value should match the type of the input operand.
      const std::array<float, 1> padding_value_buffer = {
          pad.mode->get_constant()->value};
      const std::array<int32_t, 1> padding_value_dimensions = {1};
      const int32_t padding_value_index = SerializeTensorWithBuffer<float>(
          padding_value_buffer, padding_value_dimensions);
      op_inputs.push_back(padding_value_index);
      break;
    }
    case mojom::PaddingMode::Tag::kEdge:
      // TODO: crbug.com/328547551 - Support the edge padding mode.
      return base::unexpected(
          "The edge padding mode is not supported in tflite schema.");
    case mojom::PaddingMode::Tag::kReflection: {
      operator_code = ::tflite::BuiltinOperator::BuiltinOperator_MIRROR_PAD;
      builtin_options_type =
          ::tflite::BuiltinOptions::BuiltinOptions_MirrorPadOptions;
      builtin_options = ::tflite::CreateMirrorPadOptions(
                            builder_, ::tflite::MirrorPadMode_REFLECT)
                            .Union();
      break;
    }
    case mojom::PaddingMode::Tag::kSymmetric: {
      operator_code = ::tflite::BuiltinOperator::BuiltinOperator_MIRROR_PAD;
      builtin_options_type =
          ::tflite::BuiltinOptions::BuiltinOptions_MirrorPadOptions;
      builtin_options = ::tflite::CreateMirrorPadOptions(
                            builder_, ::tflite::MirrorPadMode_SYMMETRIC)
                            .Union();
      break;
    }
  }

  const uint32_t operator_code_index = GetOperatorCodeIndex(operator_code);
  const std::array<int32_t, 1> op_outputs = {
      operand_to_index_map_.at(pad.output_operand_id)};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs),
                                  builtin_options_type, builtin_options);
}

auto GraphBuilder::SerializePool2d(const mojom::Pool2d& pool2d)
    -> base::expected<OperatorOffset, std::string> {
  // TODO(crbug.com/40206287): Transpose input operand to support other layouts
  // because tflite only support nhwc layout.
  if (pool2d.layout != mojom::InputOperandLayout::kChannelsLast) {
    return base::unexpected("The channel first input layout is not supported.");
  }

  // The dilations are not supported in tflite schema.
  if (pool2d.dilations->height != 1 || pool2d.dilations->width != 1) {
    return base::unexpected("Pool2d in tflite doesn't support dilations.");
  }

  const mojom::Operand& input_operand = GetOperand(pool2d.input_operand_id);
  const auto& input_shape = input_operand.dimensions;
  CHECK_EQ(input_shape.size(), 4u);
  const webnn::Size2d<uint32_t> input_size2d = {.height = input_shape[1],
                                                .width = input_shape[2]};
  webnn::Size2d<uint32_t> filter_size2d = {
      .height = pool2d.window_dimensions->height,
      .width = pool2d.window_dimensions->width};
  ASSIGN_OR_RETURN(
      TfLitePadding padding_mode,
      GetTfLitePaddingMode(*pool2d.padding, input_size2d, filter_size2d,
                           *pool2d.strides, *pool2d.dilations));
  // Insert a Pad operator before TfLite Pool2d if needed for explicit padding.
  std::optional<int32_t> explicit_pad_index;
  const int32_t input_index = operand_to_index_map_.at(pool2d.input_operand_id);
  if (padding_mode.paddings) {
    ASSIGN_OR_RETURN(explicit_pad_index,
                     InsertPadOperation(input_operand, input_index,
                                        padding_mode.paddings.value()));
  }

  ::tflite::BuiltinOperator operator_code;
  switch (pool2d.kind) {
    case mojom::Pool2d::Kind::kAveragePool2d:
      CHECK(kFloatDataTypes.contains(input_operand.data_type));
      operator_code = ::tflite::BuiltinOperator_AVERAGE_POOL_2D;
      break;
    case mojom::Pool2d::Kind::kMaxPool2d:
      operator_code = ::tflite::BuiltinOperator_MAX_POOL_2D;
      break;
    case mojom::Pool2d::Kind::kL2Pool2d:
      CHECK(kFloatDataTypes.contains(input_operand.data_type));
      return base::unexpected("L2Pool2d is not supported in tflite.");
  }

  const auto pool_2d_options = ::tflite::CreatePool2DOptions(
      builder_, padding_mode.mode, pool2d.strides->width,
      pool2d.strides->height, filter_size2d.width, filter_size2d.height,
      ::tflite::ActivationFunctionType_NONE);

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const uint32_t operator_code_index = GetOperatorCodeIndex(operator_code);
  const std::array<int32_t, 1> op_inputs = {
      explicit_pad_index ? explicit_pad_index.value() : input_index};
  const std::array<int32_t, 1> op_outputs = {
      operand_to_index_map_.at(pool2d.output_operand_id)};
  return ::tflite::CreateOperator(
      builder_, operator_code_index, builder_.CreateVector<int32_t>(op_inputs),
      builder_.CreateVector<int32_t>(op_outputs),
      ::tflite::BuiltinOptions_Pool2DOptions, pool_2d_options.Union());
}

auto GraphBuilder::SerializePrelu(const mojom::Prelu& prelu)
    -> base::expected<OperatorOffset, std::string> {
  const mojom::Operand& input_operand = GetOperand(prelu.input_operand_id);
  CHECK(input_operand.data_type == mojom::Operand::DataType::kFloat32 ||
        input_operand.data_type == mojom::Operand::DataType::kFloat16 ||
        input_operand.data_type == mojom::Operand::DataType::kInt32 ||
        input_operand.data_type == mojom::Operand::DataType::kInt8);
  const mojom::Operand& slope_operand = GetOperand(prelu.slope_operand_id);
  // `ValidatePreluAndInferOutput` function has checked broadcastable shapes
  // between input and slope operand, but TFLite XNNPACK delegate doesn't
  // support to broadcast last dimension.
  // TODO(crbug.com/335517470): Support last dimension broadcastable.
  if (!input_operand.dimensions.empty() && !slope_operand.dimensions.empty() &&
      input_operand.dimensions.back() != slope_operand.dimensions.back()) {
    return base::unexpected(
        "The input and slope should have the same last dimension.");
  }

  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_PRELU);
  const std::array<int32_t, 2> op_inputs = {
      operand_to_index_map_.at(prelu.input_operand_id),
      operand_to_index_map_.at(prelu.slope_operand_id)};
  const std::array<int32_t, 1> op_outputs = {
      operand_to_index_map_.at(prelu.output_operand_id)};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs));
}

auto GraphBuilder::SerializeReduce(const mojom::Reduce& reduce)
    -> base::expected<OperatorOffset, std::string> {
  // Serialize the axes tensor to reduce input tensor.
  ASSIGN_OR_RETURN(const std::vector<int32_t> signed_axes,
                   ToSignedDimensions(reduce.axes));
  const std::array<int32_t, 1> axes_tensor_shape = {
      base::checked_cast<int32_t>(signed_axes.size())};
  const int32_t axes_tensor_index =
      SerializeTensorWithBuffer<int32_t>(signed_axes, axes_tensor_shape);

  ::tflite::BuiltinOperator operator_code;
  switch (reduce.kind) {
    case mojom::Reduce::Kind::kMax:
      operator_code = ::tflite::BuiltinOperator_REDUCE_MAX;
      break;
    case mojom::Reduce::Kind::kMean:
      operator_code = ::tflite::BuiltinOperator_MEAN;
      break;
    case mojom::Reduce::Kind::kMin:
      operator_code = ::tflite::BuiltinOperator_REDUCE_MIN;
      break;
    case mojom::Reduce::Kind::kProduct:
      operator_code = ::tflite::BuiltinOperator_REDUCE_PROD;
      break;
    case mojom::Reduce::Kind::kSum:
      operator_code = ::tflite::BuiltinOperator_SUM;
      break;
    case mojom::Reduce::Kind::kLogSum:
    // TODO(crbug.com/333952108): Support reduceLogSum by decomposition.
    case mojom::Reduce::Kind::kLogSumExp:
    // TODO(crbug.com/333952108): Support reduceLogSumExp by decomposition.
    case mojom::Reduce::Kind::kSumSquare:
    // TODO(crbug.com/333952108): Support reduceSumSquare by decomposition.
    case mojom::Reduce::Kind::kL1:
    case mojom::Reduce::Kind::kL2:
      return base::unexpected(OpKindToString(reduce.kind) +
                              " is not implemented.");
  }

  const auto reduce_options =
      ::tflite::CreateReducerOptions(builder_, reduce.keep_dimensions);
  const uint32_t operator_code_index = GetOperatorCodeIndex(operator_code);
  const std::array<int32_t, 2> op_inputs = {
      operand_to_index_map_.at(reduce.input_operand_id), axes_tensor_index};
  const std::array<int32_t, 1> op_outputs = {
      operand_to_index_map_.at(reduce.output_operand_id)};
  return ::tflite::CreateOperator(
      builder_, operator_code_index, builder_.CreateVector<int32_t>(op_inputs),
      builder_.CreateVector<int32_t>(op_outputs),
      ::tflite::BuiltinOptions_ReducerOptions, reduce_options.Union());
}

auto GraphBuilder::SerializeRelu(const mojom::Relu& relu) -> OperatorOffset {
  const mojom::Operand::DataType input_data_type =
      GetOperand(relu.input_operand_id).data_type;
  CHECK(kFloatDataTypes.contains(input_data_type) ||
        input_data_type == mojom::Operand::DataType::kInt32 ||
        input_data_type == mojom::Operand::DataType::kInt8);

  return SerializeUnaryOperation(
      ::tflite::BuiltinOperator::BuiltinOperator_RELU,
      operand_to_index_map_.at(relu.input_operand_id),
      operand_to_index_map_.at(relu.output_operand_id));
}

auto GraphBuilder::SerializeResample2d(const mojom::Resample2d& resample2d)
    -> base::expected<OperatorOffset, std::string> {
  // TODO: crbug.com/329543543 - `resample2d.scales` is dropped on the floor.

  const mojom::Operand& input_operand = GetOperand(resample2d.input_operand_id);
  CHECK(kFloatDataTypes.contains(input_operand.data_type));
  const std::array<uint32_t, 2> supported_axes = {1, 2};
  if (!base::ranges::equal(resample2d.axes, supported_axes)) {
    // TODO: crbug.com/329658123: Support axes of {0, 1} and {2, 3}.
    return base::unexpected(
        "Resample2d only supports axes = {1, 2} in tflite schema.");
  }

  // Create tflite builtin options for resize mode that is align_corner = false
  // and half_pixel_center = true by default. WebNN will support coordinate
  // transformation modes for Resample2d and it's tracked by the issue:
  // https://github.com/webmachinelearning/webnn/issues/270.
  ::tflite::BuiltinOperator operator_code;
  ::tflite::BuiltinOptions builtin_options_type;
  flatbuffers::Offset<void> builtin_options;
  switch (resample2d.mode) {
    case mojom::Resample2d::InterpolationMode::kNearestNeighbor:
      operator_code = ::tflite::BuiltinOperator_RESIZE_NEAREST_NEIGHBOR;
      builtin_options_type =
          ::tflite::BuiltinOptions_ResizeNearestNeighborOptions;
      builtin_options = ::tflite::CreateResizeNearestNeighborOptions(
                            builder_, /*align_corners=*/false,
                            /*half_pixel_centers=*/true)
                            .Union();
      break;
    case mojom::Resample2d::InterpolationMode::kLinear:
      operator_code = ::tflite::BuiltinOperator_RESIZE_BILINEAR;
      builtin_options_type = ::tflite::BuiltinOptions_ResizeBilinearOptions;
      builtin_options = ::tflite::CreateResizeBilinearOptions(
                            builder_, /*align_corners=*/false,
                            /*half_pixel_centers=*/true)
                            .Union();
      break;
  }

  // Serialize the target sizes for the dimensions [OutputHeight, OutputWidth].
  ASSIGN_OR_RETURN(
      std::vector<int32_t> signed_output_dimensions,
      ToSignedDimensions(GetOperand(resample2d.output_operand_id).dimensions));
  CHECK_EQ(signed_output_dimensions.size(), 4u);

  int32_t output_height = signed_output_dimensions[resample2d.axes[0]];
  int32_t output_width = signed_output_dimensions[resample2d.axes[1]];

  const std::array<int32_t, 2> resize_data = {output_height, output_width};
  const std::array<int32_t, 1> resize_shape = {resize_data.size()};
  const int32_t resize_tensor_index =
      SerializeTensorWithBuffer<int32_t>(resize_data, resize_shape);

  const uint32_t operator_code_index = GetOperatorCodeIndex(operator_code);
  const std::array<int32_t, 2> op_inputs = {
      operand_to_index_map_.at(resample2d.input_operand_id),
      resize_tensor_index};
  const std::array<int32_t, 1> op_outputs = {
      operand_to_index_map_.at(resample2d.output_operand_id)};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs),
                                  builtin_options_type, builtin_options);
}

auto GraphBuilder::SerializeReshape(uint64_t input_operand_id,
                                    uint64_t output_operand_id)
    -> base::expected<OperatorOffset, std::string> {
  // Get the shape of the output tensor, such that this operator can reshape the
  // input to it.
  const mojom::Operand& output_operand = GetOperand(output_operand_id);
  ASSIGN_OR_RETURN(std::vector<int32_t> signed_output_dimensions,
                   ToSignedDimensions(output_operand.dimensions));

  const auto reshape_options = ::tflite::CreateReshapeOptions(
      builder_,
      /*new_shape=*/builder_.CreateVector<int32_t>(
          std::move(signed_output_dimensions)));

  return SerializeUnaryOperation(::tflite::BuiltinOperator_RESHAPE,
                                 operand_to_index_map_.at(input_operand_id),
                                 operand_to_index_map_.at(output_operand_id),
                                 ::tflite::BuiltinOptions_ReshapeOptions,
                                 reshape_options.Union());
}

auto GraphBuilder::SerializeSigmoid(const mojom::Sigmoid& sigmoid)
    -> OperatorOffset {
  return SerializeUnaryOperation(
      ::tflite::BuiltinOperator_LOGISTIC,
      operand_to_index_map_.at(sigmoid.input_operand_id),
      operand_to_index_map_.at(sigmoid.output_operand_id));
}

auto GraphBuilder::SerializeSlice(const mojom::Slice& slice)
    -> base::expected<OperatorOffset, std::string> {
  // The number of starts and sizes are the same as input rank that is verified
  // in ValidateSliceAndInferOutput() function.
  std::vector<int32_t> slice_starts;
  slice_starts.reserve(slice.starts_and_sizes.size());
  std::vector<int32_t> slice_sizes;
  slice_sizes.reserve(slice.starts_and_sizes.size());
  for (auto& start_and_size : slice.starts_and_sizes) {
    auto checked_start = base::MakeCheckedNum<int32_t>(start_and_size->start);
    auto checked_size = base::MakeCheckedNum<int32_t>(start_and_size->size);
    if (!checked_start.IsValid() || !checked_size.IsValid()) {
      return base::unexpected("The start or size of slice is too large.");
    }
    slice_starts.push_back(checked_start.ValueOrDie());
    slice_sizes.push_back(checked_size.ValueOrDie());
  }

  // Serialize the starting index of each input dimension.
  auto checked_number = base::MakeCheckedNum<int32_t>(slice_starts.size());
  if (!checked_number.IsValid()) {
    return base::unexpected("The number of starts and sizes is too large.");
  }
  const std::array<int32_t, 1> starts_and_sizes_shape = {
      checked_number.ValueOrDie()};
  const int32_t starts_tensor_index = SerializeTensorWithBuffer<int32_t>(
      std::move(slice_starts), starts_and_sizes_shape);

  // Serialize the number of elements to slice each input dimension.
  const int32_t sizes_tensor_index = SerializeTensorWithBuffer<int32_t>(
      std::move(slice_sizes), starts_and_sizes_shape);

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_SLICE);
  const std::array<int32_t, 3> op_inputs = {
      operand_to_index_map_.at(slice.input_operand_id), starts_tensor_index,
      sizes_tensor_index};
  const std::array<int32_t, 1> op_outputs = {
      operand_to_index_map_.at(slice.output_operand_id)};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs));
}

auto GraphBuilder::SerializeSoftmax(const mojom::Softmax& softmax)
    -> OperatorOffset {
  const auto softmax_options =
      ::tflite::CreateSoftmaxOptions(builder_, /*beta=*/1.0);

  return SerializeUnaryOperation(
      ::tflite::BuiltinOperator_SOFTMAX,
      operand_to_index_map_.at(softmax.input_operand_id),
      operand_to_index_map_.at(softmax.output_operand_id),
      ::tflite::BuiltinOptions_SoftmaxOptions, softmax_options.Union());
}

auto GraphBuilder::SerializeSoftplus(const mojom::Softplus& softplus)
    -> OperatorOffset {
  // Emulate the softplus operation whose calculation follows the expression
  // `ln(1 + exp(x))`.
  const mojom::Operand& input_operand = GetOperand(softplus.input_operand_id);
  CHECK(input_operand.data_type == mojom::Operand::DataType::kFloat16 ||
        input_operand.data_type == mojom::Operand::DataType::kFloat32);
  // The input shape has been validated to not overflow before creating tensor.
  const auto signed_input_dimensions =
      ToSignedDimensions(input_operand.dimensions);
  CHECK(signed_input_dimensions.has_value());
  const ::tflite::TensorType input_tensor_type =
      MojoOperandTypeToTFLite(input_operand.data_type);
  const int32_t output_tensor_index_of_exp =
      SerializeTemporaryTensor(*signed_input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeUnaryOperation(
      ::tflite::BuiltinOperator_EXP,
      operand_to_index_map_.at(softplus.input_operand_id),
      output_tensor_index_of_exp));

  // Add constant value `1` to the output tensor of element-wise exp operation.
  const int32_t constant_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{1},
      /*dimensions=*/{});
  const int32_t output_tensor_index_of_add =
      SerializeTemporaryTensor(*signed_input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_ADD, constant_tensor_index,
      output_tensor_index_of_exp, output_tensor_index_of_add));

  return SerializeUnaryOperation(
      ::tflite::BuiltinOperator_LOG, output_tensor_index_of_add,
      operand_to_index_map_.at(softplus.output_operand_id));
}

auto GraphBuilder::SerializeSplit(const mojom::Split& split)
    -> base::expected<OperatorOffset, std::string> {
  // Serialize the axis tensor to split input tensor along it.
  const auto checked_axis = base::MakeCheckedNum<int32_t>(split.axis);
  if (!checked_axis.IsValid()) {
    return base::unexpected("The axis is too large.");
  }
  const int32_t axis_tensor_index = SerializeTensorWithBuffer<int32_t>(
      /*buffer=*/std::array<int32_t, 1>{checked_axis.ValueOrDie()},
      /*dimensions=*/{});

  // Serialize the split sizes tensor that specifies the sizes of each output
  // tensor along the axis.
  const size_t outputs_size = split.output_operand_ids.size();
  std::vector<int32_t> split_sizes;
  split_sizes.reserve(outputs_size);
  std::vector<int32_t> op_outputs;
  op_outputs.reserve(outputs_size);
  for (uint64_t output_id : split.output_operand_ids) {
    // The output shape has been validated to not overflow before creating
    // tensor.
    const std::vector<uint32_t>& output_shape =
        GetOperand(output_id).dimensions;
    CHECK_LT(split.axis, output_shape.size());
    split_sizes.push_back(output_shape[split.axis]);

    op_outputs.push_back(operand_to_index_map_.at(output_id));
  }
  const auto checked_split_size =
      base::MakeCheckedNum<int32_t>(split_sizes.size());
  if (!checked_split_size.IsValid()) {
    return base::unexpected("The split size is too large.");
  }
  const std::array<int32_t, 1> split_sizes_shape = {
      checked_split_size.ValueOrDie()};
  const int32_t sizes_tensor_index =
      SerializeTensorWithBuffer<int32_t>(split_sizes, split_sizes_shape);

  // Create `tflite::SplitOptions` with the split size.
  const auto split_options = ::tflite::CreateSplitOptions(
      builder_, /*num_splits=*/checked_split_size.ValueOrDie());

  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_SPLIT_V);
  // The order of inputs is input, split sizes tensor and then axis tensor as
  // the described https://www.tensorflow.org/mlir/tfl_ops#operands_130.
  const std::array<int32_t, 3> op_inputs = {
      operand_to_index_map_.at(split.input_operand_id), sizes_tensor_index,
      axis_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index, builder_.CreateVector<int32_t>(op_inputs),
      builder_.CreateVector<int32_t>(op_outputs),
      ::tflite::BuiltinOptions_SplitVOptions, split_options.Union());
}

auto GraphBuilder::SerializeTan(const mojom::ElementWiseUnary& tan)
    -> OperatorOffset {
  // The tangent operation defines the expression `opposite side / adjacent
  // side` to a right triangle as the described here
  // https://www.mathworks.com/help/matlab/ref/tan.html, it can be emulated with
  // `sin(x)/cos(x)` element-wise.
  const mojom::Operand& input_operand = GetOperand(tan.input_operand_id);
  // The input shape has been validated to not overflow before creating tensor.
  const auto signed_input_dimensions =
      ToSignedDimensions(input_operand.dimensions);
  CHECK(signed_input_dimensions.has_value());
  const ::tflite::TensorType input_tensor_type =
      MojoOperandTypeToTFLite(input_operand.data_type);
  const int32_t output_tensor_index_of_sin =
      SerializeTemporaryTensor(*signed_input_dimensions, input_tensor_type);
  const int32_t input_tensor_index =
      operand_to_index_map_.at(tan.input_operand_id);
  operators_.emplace_back(SerializeUnaryOperation(::tflite::BuiltinOperator_SIN,
                                                  input_tensor_index,
                                                  output_tensor_index_of_sin));

  const int32_t output_tensor_index_of_cos =
      SerializeTemporaryTensor(*signed_input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeUnaryOperation(::tflite::BuiltinOperator_COS,
                                                  input_tensor_index,
                                                  output_tensor_index_of_cos));
  return SerializeBinaryOperation(
      ::tflite::BuiltinOperator_DIV, output_tensor_index_of_sin,
      output_tensor_index_of_cos,
      operand_to_index_map_.at(tan.output_operand_id));
}

auto GraphBuilder::SerializeTanh(const mojom::Tanh& tanh) -> OperatorOffset {
  return SerializeUnaryOperation(
      ::tflite::BuiltinOperator_TANH,
      operand_to_index_map_.at(tanh.input_operand_id),
      operand_to_index_map_.at(tanh.output_operand_id));
}

auto GraphBuilder::SerializeTranspose(const mojom::Transpose& transpose)
    -> OperatorOffset {
  return SerializeTransposeOperation(
      operand_to_index_map_.at(transpose.input_operand_id),
      operand_to_index_map_.at(transpose.output_operand_id),
      transpose.permutation);
}

auto GraphBuilder::SerializeWhere(const mojom::Where& where) -> OperatorOffset {
  // The data type of WebNN condition operand is uint8, but TFLite requires the
  // condition operand to be of type bool, so a cast operation need to be
  // inserted before the operation to convert uint8 to bool for the condition
  // operand.
  const mojom::Operand& condition_operand =
      GetOperand(where.condition_operand_id);
  // The shape of condition operand has been validated to not overflow before
  // creating tensor.
  const auto signed_condition_dimensions =
      ToSignedDimensions(condition_operand.dimensions);
  CHECK(signed_condition_dimensions.has_value());
  const int32_t condition_bool_tensor_index = SerializeTemporaryTensor(
      *signed_condition_dimensions, ::tflite::TensorType_BOOL);

  CHECK_EQ(condition_operand.data_type, mojom::Operand::DataType::kUint8);
  operators_.emplace_back(SerializeCastOperation(
      operand_to_index_map_.at(where.condition_operand_id),
      /*input_tensor_type=*/::tflite::TensorType_UINT8,
      condition_bool_tensor_index,
      /*output_tensor_type=*/::tflite::TensorType_BOOL));

  // TFLite SELECT_V2 builtin operator supports broadcastable shapes between
  // `condition`, `true` and `false` operand.
  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_SELECT_V2);
  const std::array<int32_t, 3> op_inputs = {
      condition_bool_tensor_index,
      operand_to_index_map_.at(where.true_value_operand_id),
      operand_to_index_map_.at(where.false_value_operand_id)};
  const std::array<int32_t, 1> op_outputs = {
      operand_to_index_map_.at(where.output_operand_id)};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs));
}

}  // namespace webnn::tflite
