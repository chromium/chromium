// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/graph_builder_tflite.h"

#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>

#include "base/containers/fixed_flat_set.h"
#include "base/containers/span.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/types/fixed_array.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/graph_validation_utils.h"
#include "services/webnn/public/cpp/webnn_errors.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_utils.h"
#include "third_party/tflite/src/tensorflow/compiler/mlir/lite/schema/schema_generated.h"

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
template <>
struct TensorTypeMap<int64_t> {
  static constexpr ::tflite::TensorType value =
      ::tflite::TensorType::TensorType_INT64;
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

::tflite::TensorType OperandDataTypeToTFLite(OperandDataType data_type) {
  switch (data_type) {
    case OperandDataType::kFloat32:
      return ::tflite::TensorType_FLOAT32;
    case OperandDataType::kFloat16:
      return ::tflite::TensorType_FLOAT16;
    case OperandDataType::kInt32:
      return ::tflite::TensorType_INT32;
    case OperandDataType::kUint32:
      return ::tflite::TensorType_UINT32;
    case OperandDataType::kInt64:
      return ::tflite::TensorType_INT64;
    case OperandDataType::kUint64:
      return ::tflite::TensorType_UINT64;
    case OperandDataType::kInt8:
      return ::tflite::TensorType_INT8;
    case OperandDataType::kUint8:
      return ::tflite::TensorType_UINT8;
    case OperandDataType::kInt4:
      return ::tflite::TensorType_INT4;
    default:
      NOTREACHED() << "Unsupported data type.";
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

::tflite::BuiltinOperator GetRecurrentNetworkActivation(
    mojom::RecurrentNetworkActivation activation) {
  switch (activation) {
    case mojom::RecurrentNetworkActivation::kRelu:
      return ::tflite::BuiltinOperator_RELU;
    case mojom::RecurrentNetworkActivation::kTanh:
      return ::tflite::BuiltinOperator_TANH;
    case mojom::RecurrentNetworkActivation::kSigmoid:
      return ::tflite::BuiltinOperator_LOGISTIC;
  }
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
    uint32_t dilation,
    bool is_transposed_conv2d) {
  auto checked_dilated_filter_size =
      (base::MakeCheckedNum<uint32_t>(filter_size) - 1) * dilation + 1;
  auto checked_input_size = base::MakeCheckedNum<uint32_t>(input_size);
  base::CheckedNumeric<uint32_t> checked_total_padding;
  if (is_transposed_conv2d) {
    // The checked_total_padding (beginningPadding + endingPadding) can be
    // calculated from the expression `outputSize = (inputSize - 1) * stride +
    // (filterSize - 1) * dilation + 1 - beginningPadding - endingPadding` that
    // is documented in the section of computing convtranspose output size:
    // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-convtranspose2d
    checked_total_padding = (checked_input_size - 1) * stride +
                            checked_dilated_filter_size -
                            checked_input_size * stride;
  } else {
    auto checked_output_size = (checked_input_size + stride - 1) / stride;
    auto checked_needed_input_size =
        (checked_output_size - 1) * stride + checked_dilated_filter_size;
    if (!checked_needed_input_size.IsValid()) {
      return std::nullopt;
    }
    checked_total_padding = checked_needed_input_size.ValueOrDie() > input_size
                                ? checked_needed_input_size - input_size
                                : base::MakeCheckedNum<uint32_t>(0);
  }

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
    const mojom::Size2d& dilation,
    bool is_transposed_conv2d) {
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
      input.height, filter.height, stride.height, dilation.height,
      is_transposed_conv2d);
  const auto padding_width = CalculateExplicitPaddingForSamePaddingMode(
      input.width, filter.width, stride.width, dilation.width,
      is_transposed_conv2d);
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

// Sort the indexes of the elements in the axes array based on their values and
// return the sorted index array for adding a transpose operation if needed. For
// example input shape is [2, 1, 4, 3], the shape of the scale and bias is [3,
// 1, 4] if axes is [3, 1, 2], the sorted axes would be [1, 2, 3], then the
// permutation would be (sorted indices array) [1, 2, 0].
std::vector<uint32_t> GetIndexOfSortedValue(base::span<const uint32_t> axes) {
  std::vector<uint32_t> sorted_indices(axes.size());
  std::iota(sorted_indices.begin(), sorted_indices.end(), 0);
  base::ranges::sort(sorted_indices, base::ranges::less(),
                     [axes](uint32_t index) { return axes[index]; });
  return sorted_indices;
}

// An element in row `i` and column `j` of a matrix is in the upper-triangular
// portion if `j >= i + diagonal`. It is in the lower-triangular portion if
// `j <= i + diagonal`.
//
// This function generates an upper-triangular or lower-triangular matrix
// with the given mask value. For example, the matrices below are the upper-
// and lower-triangular [3, 3] tensors with a mask value of 1:
// [ 1, 1, 1                    [ 1, 0, 0
//   0, 1, 1,                     1, 1, 0,
//   0, 0, 1]                     1, 1, 1]
template <typename DataType>
std::vector<DataType> FillMaskTriangular(base::span<const int32_t> dimensions,
                                         bool upper,
                                         int32_t diagonal,
                                         DataType mask) {
  CHECK_EQ(dimensions.size(), 2u);
  const int32_t height = dimensions[0];
  const int32_t width = dimensions[1];
  std::vector<DataType> filled_matrix(height * width);
  for (int32_t i = 0; i < height; ++i) {
    for (int32_t j = 0; j < width; ++j) {
      // `i + diagonal` has been validated to not overflow by the caller.
      bool fill_mask_value = upper ? j >= i + diagonal : j <= i + diagonal;
      if (fill_mask_value) {
        // Get the index in the flat array with the location.
        int32_t index = (i * width) + j;
        CHECK_LT(base::checked_cast<size_t>(index), filled_matrix.size());
        filled_matrix[index] = mask;
      }
    }
  }

  return filled_matrix;
}

}  // namespace

// static
base::expected<flatbuffers::DetachedBuffer, std::string>
GraphBuilderTflite::CreateAndBuild(
    ContextProperties context_properties,
    const mojom::GraphInfo& graph_info,
    const base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>&
        constant_operands) {
  GraphBuilderTflite builder(std::move(context_properties), graph_info,
                             constant_operands);

  for (const mojom::OperationPtr& operation : graph_info.operations) {
    RETURN_IF_ERROR(builder.SerializeOperation(*operation));
  }

  return builder.FinishAndTakeFlatBuffer(graph_info.input_operands,
                                         graph_info.output_operands);
}

// static
ContextProperties GraphBuilderTflite::GetContextProperties() {
  // TODO: crbug.com/345271830 - specify data types for all parameters.
  static constexpr SupportedDataTypes kFloat16To32AndInt8{
      OperandDataType::kFloat16, OperandDataType::kFloat32,
      OperandDataType::kInt8};
  static constexpr SupportedDataTypes kFloat16To32AndInt32To64{
      OperandDataType::kFloat16, OperandDataType::kFloat32,
      OperandDataType::kInt32, OperandDataType::kInt64};
  static constexpr SupportedDataTypes kFloat16To32AndInt32To64AndUint32{
      OperandDataType::kFloat16, OperandDataType::kFloat32,
      OperandDataType::kInt32, OperandDataType::kUint32,
      OperandDataType::kInt64};
  static constexpr SupportedDataTypes kFloat16To32AndInt32{
      OperandDataType::kFloat16, OperandDataType::kFloat32,
      OperandDataType::kInt32};
  static constexpr SupportedDataTypes kFloat16To32AndInt32To64AndUint8{
      OperandDataType::kFloat16, OperandDataType::kFloat32,
      OperandDataType::kInt32, OperandDataType::kInt64,
      OperandDataType::kUint8};
  static constexpr SupportedDataTypes kFloat16To32AndInt8To64AndUint32{
      OperandDataType::kFloat16, OperandDataType::kFloat32,
      OperandDataType::kInt32,   OperandDataType::kUint32,
      OperandDataType::kInt64,   OperandDataType::kInt8};
  static constexpr SupportedDataTypes kFloat16To32AndInt8To32AndUint8{
      OperandDataType::kFloat16, OperandDataType::kFloat32,
      OperandDataType::kInt32, OperandDataType::kInt8, OperandDataType::kUint8};
  static constexpr SupportedDataTypes kFloat16To32AndInt8To64AndUint8{
      OperandDataType::kFloat16, OperandDataType::kFloat32,
      OperandDataType::kInt64,   OperandDataType::kInt32,
      OperandDataType::kInt8,    OperandDataType::kUint8};
  static constexpr SupportedDataTypes kFloat16To32AndInts8To32AndInt64{
      OperandDataType::kFloat16, OperandDataType::kFloat32,
      OperandDataType::kInt8,    OperandDataType::kUint8,
      OperandDataType::kInt32,   OperandDataType::kUint32,
      OperandDataType::kInt64};
  static constexpr SupportedDataTypes kAllDataTypesExceptUint4 = {
      OperandDataType::kFloat32, OperandDataType::kFloat16,
      OperandDataType::kInt32,   OperandDataType::kUint32,
      OperandDataType::kInt64,   OperandDataType::kUint64,
      OperandDataType::kInt8,    OperandDataType::kUint8,
      OperandDataType::kInt4};

  return ContextProperties(
      InputOperandLayout::kNhwc, Resample2DAxes::kChannelsLast,
      {/*input=*/kAllDataTypesExceptUint4,
       /*constant=*/kAllDataTypesExceptUint4,
       /*arg_min_max_input=*/kFloat16To32AndInt8To32AndUint8,
       /*arg_min_max_output=*/DataTypeConstraint::kInt32To64,
       /*batch_normalization_input=*/DataTypeConstraint::kFloat16To32,
       /*cast_input=*/kFloat16To32AndInts8To32AndInt64,
       /*clamp_input=*/DataTypeConstraint::kFloat16To32,
       /*concat_inputs=*/kAllDataTypesExceptUint4,
       /*conv2d_input=*/DataTypeConstraint::kFloat16To32,
       /*conv_transpose2d_input=*/DataTypeConstraint::kFloat16To32,
       // CumulativeSum is not implemented.
       /*cumulative_sum_input=*/{},
       // DequantizeLinear is not implemented.
       /*dequantize_linear_input=*/{},
       /*dequantize_linear_scale=*/{},
       /*add_input=*/kFloat16To32AndInt32To64,
       /*sub_input=*/kFloat16To32AndInt32To64,
       /*mul_input=*/kFloat16To32AndInt32To64AndUint32,
       /*div_input=*/kFloat16To32AndInt32,
       /*max_input=*/kFloat16To32AndInt32To64,
       /*min_input=*/kFloat16To32AndInt32To64,
       /*pow_input=*/kFloat16To32AndInt32,
       /*equal_input=*/kFloat16To32AndInt32To64AndUint8,
       /*greater_input=*/kFloat16To32AndInt32To64,
       /*greater_or_equal_input=*/kFloat16To32AndInt32To64,
       /*lesser_input=*/kFloat16To32AndInt32To64,
       /*lesser_or_equal_input=*/kFloat16To32AndInt32To64,
       /*logical_and_input=*/{},
       /*logical_or_input=*/{},
       /*logical_xor_input=*/{},
       /*logical_not_input=*/DataTypeConstraint::kUint8,
       /*logical_output=*/DataTypeConstraint::kUint8,
       /*abs_input=*/kFloat16To32AndInt32,
       /*ceil_input=*/DataTypeConstraint::kFloat16To32,
       /*cos_input=*/DataTypeConstraint::kFloat16To32,
       /*erf_input=*/DataTypeConstraint::kFloat16To32,
       /*exp_input=*/DataTypeConstraint::kFloat16To32,
       /*floor_input=*/DataTypeConstraint::kFloat16To32,
       // Identity is emulated by reshape.
       /*identity_input=*/kFloat16To32AndInt8To64AndUint8,
       /*log_input=*/DataTypeConstraint::kFloat16To32,
       /*neg_input=*/kFloat16To32AndInt32To64,
       /*reciprocal_input=*/DataTypeConstraint::kFloat16To32,
       /*sign_input=*/kFloat16To32AndInt32,
       /*sin_input=*/DataTypeConstraint::kFloat16To32,
       /*sqrt_input=*/DataTypeConstraint::kFloat16To32,
       /*tan_input=*/DataTypeConstraint::kFloat16To32,
       /*elu_input=*/kFloat16To32AndInt8,
       /*expand_input=*/kFloat16To32AndInts8To32AndInt64,
       /*gather_input=*/kFloat16To32AndInt8To64AndUint8,
       /*gather_indices=*/
       DataTypeConstraint::kGatherScatterIndicesSupportedDataTypes,
       // GatherElements is not implemented.
       /*gather_elements_input=*/{},
       /*gather_elements_indices=*/{},
       // GatherND is not implemented.
       /*gather_nd_input=*/{},
       /*gather_nd_indices=*/{},
       /*gelu_input=*/DataTypeConstraint::kFloat16To32,
       /*gemm_input=*/DataTypeConstraint::kFloat16To32,
       /*gru_input=*/DataTypeConstraint::kFloat16To32,
       /*gru_cell_input=*/DataTypeConstraint::kFloat16To32,
       /*hard_sigmoid_input=*/DataTypeConstraint::kFloat16To32,
       /*hard_swish_input=*/DataTypeConstraint::kFloat16To32,
       /*instance_normalization_input=*/DataTypeConstraint::kFloat16To32,
       /*layer_normalization_input=*/DataTypeConstraint::kFloat16To32,
       /*leaky_relu_input=*/DataTypeConstraint::kFloat16To32,
       // Linear is emulated by mul and add.
       /*linear_input=*/kFloat16To32AndInt32To64,
       /*lstm_input=*/DataTypeConstraint::kFloat16To32,
       /*lstm_cell_input=*/DataTypeConstraint::kFloat16To32,
       /*matmul_input=*/kFloat16To32AndInt8,
       /*pad_input=*/kFloat16To32AndInt32To64AndUint8,
       /*average_pool2d_input=*/DataTypeConstraint::kFloat16To32,
       /*l2_pool2d_input=*/{},
       /*max_pool2d_input=*/DataTypeConstraint::kFloat16To32,
       /*prelu_input=*/DataTypeConstraint::kFloat16To32,
       // QuantizeLinear is not implemented.
       /*quantize_linear_input=*/{},
       /*quantize_linear_zero_point=*/{},
       // ReduceL1 is emulated by abs and reduceSum.
       /*reduce_l1_input=*/kFloat16To32AndInt32,
       // ReduceL2 is emulated by pow and reduceSumSquare.
       /*reduce_l2_input=*/DataTypeConstraint::kFloat16To32,
       // ReduceLogSum is emulated by reduceSum and log.
       /*reduce_log_sum_input=*/DataTypeConstraint::kFloat16To32,
       // ReduceLogSumExp is emulated by reduceSum and exp.
       /*reduce_log_sum_exp_input=*/DataTypeConstraint::kFloat16To32,
       /*reduce_max_input=*/kFloat16To32AndInt32To64,
       /*reduce_mean_input=*/kFloat16To32AndInt32To64AndUint8,
       /*reduce_min_input=*/kFloat16To32AndInt32To64,
       /*reduce_product_input=*/kFloat16To32AndInt32To64,
       /*reduce_sum_input=*/kFloat16To32AndInt32To64,
       // ReduceLogSum is emulated by reduceSum and pow.
       /*reduce_sum_square_input=*/kFloat16To32AndInt32,
       /*relu_input=*/DataTypeConstraint::kFloat16To32,
       /*resample2d_input=*/DataTypeConstraint::kFloat16To32,
       /*reshape_input=*/kAllDataTypesExceptUint4,
       // TODO(crbug.com/363677532): Implement scatterND.
       /*scatter_nd_input=*/{},
       /*scatter_nd_indices=*/{},
       /*sigmoid_input=*/DataTypeConstraint::kFloat16To32,
       /*slice_input=*/kFloat16To32AndInts8To32AndInt64,
       /*softmax_input=*/DataTypeConstraint::kFloat16To32,
       /*softplus_input=*/DataTypeConstraint::kFloat16To32,
       /*softsign_input=*/DataTypeConstraint::kFloat16To32,
       /*split_input=*/kFloat16To32AndInt8To64AndUint8,
       /*tanh_input=*/DataTypeConstraint::kFloat16To32,
       /*tile_input=*/kFloat16To32AndInt32To64AndUint8,
       /*transpose_input=*/kFloat16To32AndInt8To64AndUint8,
       /*triangular_input=*/kFloat16To32AndInt32To64AndUint32,
       /*where_condition=*/DataTypeConstraint::kUint8,
       /*where_value=*/kFloat16To32AndInt8To64AndUint32});
}

GraphBuilderTflite::GraphBuilderTflite(
    ContextProperties context_properties,
    const mojom::GraphInfo& graph_info,
    const base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>&
        constant_operands)
    : context_properties_(std::move(context_properties)),
      graph_info_(graph_info),
      constant_operands_(constant_operands) {
  // TFLite requires the first entry in FlatBuffer to be an empty buffer.
  buffers_.push_back(
      ::tflite::CreateBuffer(builder_, builder_.CreateVector({})));
}

GraphBuilderTflite::~GraphBuilderTflite() = default;

GraphBuilderTflite::TensorInfo::TensorInfo() = default;
GraphBuilderTflite::TensorInfo::TensorInfo(int32_t index,
                                           ::tflite::TensorType data_type,
                                           base::span<const int32_t> dimensions)
    : index(index),
      data_type(data_type),
      dimensions(dimensions.begin(), dimensions.end()) {}
GraphBuilderTflite::TensorInfo::~TensorInfo() = default;

GraphBuilderTflite::TensorInfo::TensorInfo(const TensorInfo&) = default;
GraphBuilderTflite::TensorInfo& GraphBuilderTflite::TensorInfo::operator=(
    const TensorInfo&) = default;

GraphBuilderTflite::TensorInfo::TensorInfo(TensorInfo&& other) = default;
GraphBuilderTflite::TensorInfo& GraphBuilderTflite::TensorInfo::operator=(
    TensorInfo&& other) = default;

base::expected<GraphBuilderTflite::TensorInfo, std::string>
GraphBuilderTflite::SerializeOperand(
    uint64_t operand_id,
    std::optional<::tflite::TensorType> override_tensor_type) {
  // The index of `tflite::Tensor` array, each `Operand` (input, constant,
  // output) will be converted and pushed back into the array, so it's increased
  // by one after each serialization in flat buffer.
  int32_t tensor_index = base::checked_cast<int32_t>(tensors_.size());
  CHECK_GE(tensor_index, 0);

  // The buffer index 0 represents input and output operand because there is no
  // data buffer associated.
  uint32_t buffer_index = 0;
  const mojom::Operand& operand =
      *graph_info_->id_to_operand_map.at(operand_id);
  if (operand.kind == mojom::Operand::Kind::kConstant) {
    // Serialize buffer and return buffer index which starts from 1, it is
    // used to create the constant's tensor.
    auto it = constant_operands_->find(operand_id);
    CHECK(it != constant_operands_->end());
    buffer_index = SerializeBuffer(it->second->ByteSpan());
  }

  // Create `Tensor` with operand shape, the index of buffer and the name.
  ASSIGN_OR_RETURN(std::vector<int32_t> signed_operand_dimensions,
                   ToSignedDimensions(operand.descriptor.shape()));
  const flatbuffers::Offset<flatbuffers::Vector<int32_t>> dimensions =
      builder_.CreateVector<int32_t>(signed_operand_dimensions);
  ::tflite::TensorType operand_type = override_tensor_type.value_or(
      OperandDataTypeToTFLite(operand.descriptor.data_type()));
  const StringOffset operand_name =
      operand.name.has_value() ? builder_.CreateString(*operand.name) : 0;
  tensors_.emplace_back(::tflite::CreateTensor(builder_, std::move(dimensions),
                                               operand_type, buffer_index,
                                               operand_name));
  TensorInfo tensor_info(tensor_index, operand_type, signed_operand_dimensions);
  operand_to_tensor_info_map_.insert({operand_id, tensor_info});

  return tensor_info;
}

base::expected<GraphBuilderTflite::TensorInfo, std::string>
GraphBuilderTflite::SerializeInputTensorInfo(uint64_t operand_id,
                                             bool operation_supports_float16) {
  TensorInfo input_tensor_info;
  auto it = operand_to_tensor_info_map_.find(operand_id);
  if (it == operand_to_tensor_info_map_.end()) {
    ASSIGN_OR_RETURN(input_tensor_info, SerializeOperand(operand_id));
  } else {
    input_tensor_info = it->second;
  }

  // Insert a TFLite dequantize operator to convert float16 to float32 for graph
  // input, constant and intermediate operands if the current operation doesn't
  // support float16 inference. For example the below subgraph, the dequantize
  // operator will be inserted after input and weight nodes since conv2d does
  // not support float16, but since the reshape operator supports float16 the
  // dequantize operator is inserted after the reshape.
  //
  //                       [bias]                                 [bias]
  //                         |                                       |
  //  [input] [weight]  Reshape         [input]       [weight]    Reshape
  //    \         |        /                |            |           |
  //            Conv2d              =>    dequantize   dequantize dequantize
  //              |                            \            |          /
  //           [output]                                  Conv2d
  //                                                        |
  //                                                     [output]
  if (!operation_supports_float16 &&
      input_tensor_info.data_type == ::tflite::TensorType_FLOAT16) {
    // TODO(crbug.com/365168170): Associate the dequantized tensor with the
    // operand.
    return TensorInfo(
        SerializeDequantizeOperation(input_tensor_info.index,
                                     input_tensor_info.dimensions),
        ::tflite::TensorType_FLOAT32, input_tensor_info.dimensions);
  }

  return input_tensor_info;
}

base::expected<GraphBuilderTflite::TensorInfo, std::string>
GraphBuilderTflite::SerializeOutputTensorInfo(
    uint64_t operand_id,
    bool operation_supports_float16,
    std::optional<::tflite::TensorType> override_tensor_type) {
  CHECK(!operand_to_tensor_info_map_.contains(operand_id));
  const mojom::Operand& operand =
      *graph_info_->id_to_operand_map.at(operand_id);
  bool is_graph_output = operand.name.has_value();
  const OperandDataType data_type = operand.descriptor.data_type();
  auto tensor_type = OperandDataTypeToTFLite(data_type);
  if (data_type == OperandDataType::kFloat16 && !is_graph_output) {
    // Uses float32 data type for the intermediate operands if the operation
    // does not support float16. For supported operations such as reshape and
    // concat, the output tensor type must be the same as input , so the
    // override tensor type (for example the input tensor of reshape in below
    // subgraph is float32) should be used rather than float16 data type.
    //
    //                                           [float16 input]
    //                                                   |
    //       [float16 input]                         dequantize
    //            |                                      |
    //          Gelu                                    Gelu
    //            |                                      |
    //         Reshape              =>                Reshape
    //            |                                      |
    //           Relu                                   Relu
    //            |                                      |
    //       [float16 output]                           cast
    //                                                   |
    //                                             [float16 output]
    if (!operation_supports_float16) {
      tensor_type = ::tflite::TensorType_FLOAT32;
    } else if (override_tensor_type) {
      tensor_type = *override_tensor_type;
    }
  }
  ASSIGN_OR_RETURN(TensorInfo output_tensor_info,
                   SerializeOperand(operand_id, tensor_type));

  // Insert a TFLite cast operator to convert float32 to float16 if the operand
  // is graph output and the current operation doesn't support float16
  // inference or override to float32 (for example the output tensor of
  // reshape), the `temporary_tensor_index` will be used by output tensor index
  // (for example the output tensor of relu), the `output_tensor_info.index` is
  // still output tensor of graph.
  //
  //                                             [float16 input]
  //                                                   |
  //       [float16 input]                          dequantize
  //            |                                      |
  //           relu              =>                   relu
  //           /   \                                  /     \
  //      Reshape  [float16 output]               Reshape  cast
  //         |                                       |       |
  //   [float16 output]                             cast  [float16 output]
  //                                                 |
  //                                            [float16 output]
  const bool override_float32_type =
      override_tensor_type == ::tflite::TensorType_FLOAT32;
  if (output_tensor_info.data_type == ::tflite::TensorType_FLOAT16 &&
      (!operation_supports_float16 || override_float32_type) &&
      is_graph_output) {
    const int32_t temporary_tensor_index = SerializeTemporaryTensor(
        output_tensor_info.dimensions, ::tflite::TensorType_FLOAT32);
    graph_output_cast_operators_.emplace_back(SerializeCastOperation(
        temporary_tensor_index,
        /*input_tensor_type=*/::tflite::TensorType_FLOAT32,
        output_tensor_info.index,
        /*output_tensor_type=*/::tflite::TensorType_FLOAT16));
    return TensorInfo(temporary_tensor_index, ::tflite::TensorType_FLOAT32,
                      output_tensor_info.dimensions);
  }

  return output_tensor_info;
}

base::expected<void, std::string> GraphBuilderTflite::SerializeOperation(
    const mojom::Operation& op) {
  OperatorOffset operator_offset;
  switch (op.which()) {
    case mojom::Operation::Tag::kArgMinMax: {
      ASSIGN_OR_RETURN(operator_offset,
                       SerializeArgMinMax(*op.get_arg_min_max()));
      break;
    }
    case mojom::Operation::Tag::kBatchNormalization: {
      ASSIGN_OR_RETURN(operator_offset, SerializeBatchNormalization(
                                            *op.get_batch_normalization()));
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
    case mojom::Operation::Tag::kConcat: {
      ASSIGN_OR_RETURN(operator_offset, SerializeConcat(*op.get_concat()));
      break;
    }
    case mojom::Operation::Tag::kDequantizeLinear:
      return base::unexpected(
          "DequantizeLinear operation is not implemented in tflite.");
    case mojom::Operation::Tag::kElementWiseBinary: {
      ASSIGN_OR_RETURN(operator_offset, SerializeElementWiseBinary(
                                            *op.get_element_wise_binary()));
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
    case mojom::Operation::Tag::kExpand: {
      ASSIGN_OR_RETURN(operator_offset, SerializeExpand(*op.get_expand()));
      break;
    }
    case mojom::Operation::Tag::kGather: {
      ASSIGN_OR_RETURN(operator_offset, SerializeGather(*op.get_gather()));
      break;
    }
    case mojom::Operation::Tag::kGelu: {
      ASSIGN_OR_RETURN(operator_offset, SerializeGelu(*op.get_gelu()));
      break;
    }
    case mojom::Operation::Tag::kGemm: {
      ASSIGN_OR_RETURN(operator_offset, SerializeGemm(*op.get_gemm()));
      break;
    }
    case mojom::Operation::Tag::kGru: {
      ASSIGN_OR_RETURN(operator_offset,
                       SerializeRecurrentNetwork<mojom::Gru>(*op.get_gru()));
      break;
    }
    case mojom::Operation::Tag::kGruCell: {
      ASSIGN_OR_RETURN(operator_offset, SerializeGruCell(*op.get_gru_cell()));
      break;
    }
    case mojom::Operation::Tag::kHardSigmoid: {
      ASSIGN_OR_RETURN(operator_offset,
                       SerializeHardSigmoid(*op.get_hard_sigmoid()));
      break;
    }
    case mojom::Operation::Tag::kHardSwish: {
      ASSIGN_OR_RETURN(operator_offset,
                       SerializeHardSwish(*op.get_hard_swish()));
      break;
    }
    case mojom::Operation::Tag::kInstanceNormalization: {
      ASSIGN_OR_RETURN(operator_offset, SerializeInstanceNormalization(
                                            *op.get_instance_normalization()));
      break;
    }
    case mojom::Operation::Tag::kLayerNormalization: {
      ASSIGN_OR_RETURN(operator_offset, SerializeLayerNormalization(
                                            *op.get_layer_normalization()));
      break;
    }
    case mojom::Operation::Tag::kLeakyRelu: {
      ASSIGN_OR_RETURN(operator_offset,
                       SerializeLeakyRelu(*op.get_leaky_relu()));
      break;
    }
    case mojom::Operation::Tag::kLinear: {
      ASSIGN_OR_RETURN(operator_offset, SerializeLinear(*op.get_linear()));
      break;
    }
    case mojom::Operation::Tag::kLstmCell: {
      ASSIGN_OR_RETURN(operator_offset, SerializeLstmCell(*op.get_lstm_cell()));
      break;
    }
    case mojom::Operation::Tag::kLstm: {
      ASSIGN_OR_RETURN(operator_offset,
                       SerializeRecurrentNetwork<mojom::Lstm>(*op.get_lstm()));
      break;
    }
    case mojom::Operation::Tag::kMatmul: {
      ASSIGN_OR_RETURN(operator_offset, SerializeMatmul(*op.get_matmul()));
      break;
    }
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
    case mojom::Operation::Tag::kQuantizeLinear:
      return base::unexpected(
          "QuantizeLinear operation is not implemented in tflite.");
    case mojom::Operation::Tag::kReduce: {
      ASSIGN_OR_RETURN(operator_offset, SerializeReduce(*op.get_reduce()));
      break;
    }
    case mojom::Operation::Tag::kRelu: {
      ASSIGN_OR_RETURN(operator_offset, SerializeRelu(*op.get_relu()));
      break;
    }
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
    case mojom::Operation::Tag::kSigmoid: {
      ASSIGN_OR_RETURN(operator_offset, SerializeSigmoid(*op.get_sigmoid()));
      break;
    }
    case mojom::Operation::Tag::kSlice: {
      ASSIGN_OR_RETURN(operator_offset, SerializeSlice(*op.get_slice()));
      break;
    }
    case mojom::Operation::Tag::kSoftmax: {
      ASSIGN_OR_RETURN(operator_offset, SerializeSoftmax(*op.get_softmax()));
      break;
    }
    case mojom::Operation::Tag::kSoftplus: {
      ASSIGN_OR_RETURN(operator_offset, SerializeSoftplus(*op.get_softplus()));
      break;
    }
    case mojom::Operation::Tag::kSoftsign: {
      ASSIGN_OR_RETURN(operator_offset, SerializeSoftsign(*op.get_softsign()));
      break;
    }
    case mojom::Operation::Tag::kSplit: {
      ASSIGN_OR_RETURN(operator_offset, SerializeSplit(*op.get_split()));
      break;
    }
    case mojom::Operation::Tag::kTanh: {
      ASSIGN_OR_RETURN(operator_offset, SerializeTanh(*op.get_tanh()));
      break;
    }
    case mojom::Operation::Tag::kTile: {
      ASSIGN_OR_RETURN(operator_offset, SerializeTile(*op.get_tile()));
      break;
    }
    case mojom::Operation::Tag::kTranspose: {
      ASSIGN_OR_RETURN(operator_offset,
                       SerializeTranspose(*op.get_transpose()));
      break;
    }
    case mojom::Operation::Tag::kTriangular: {
      ASSIGN_OR_RETURN(operator_offset,
                       SerializeTriangular(*op.get_triangular()));
      break;
    }
    case mojom::Operation::Tag::kWhere: {
      ASSIGN_OR_RETURN(operator_offset, SerializeWhere(*op.get_where()));
      break;
    }
    case mojom::Operation::Tag::kCumulativeSum:
    case mojom::Operation::Tag::kGatherElements:
    case mojom::Operation::Tag::kGatherNd:
    case mojom::Operation::Tag::kScatterNd:
      return base::unexpected(NotSupportedOperatorError(op));
  }
  operators_.emplace_back(operator_offset);

  return base::ok();
}

flatbuffers::DetachedBuffer GraphBuilderTflite::FinishAndTakeFlatBuffer(
    base::span<const uint64_t> input_operands,
    base::span<const uint64_t> output_operands) {
  CHECK(!is_created_model_);

  int32_t* graph_input_ids = nullptr;
  auto graph_input_ids_index = builder_.CreateUninitializedVector<int32_t>(
      input_operands.size(), &graph_input_ids);
  base::ranges::transform(
      input_operands, graph_input_ids, [&](uint64_t operand_id) {
        return operand_to_tensor_info_map_.at(operand_id).index;
      });

  int32_t* graph_output_ids = nullptr;
  auto graph_output_ids_index = builder_.CreateUninitializedVector<int32_t>(
      output_operands.size(), &graph_output_ids);
  base::ranges::transform(
      output_operands, graph_output_ids, [&](uint64_t operand_id) {
        return operand_to_tensor_info_map_.at(operand_id).index;
      });

  // Insert the cast operator for the graph output operand after the unsupported
  // float16 inference operation.
  for (auto cast_operator_offset : graph_output_cast_operators_) {
    operators_.emplace_back(cast_operator_offset);
  }

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

uint32_t GraphBuilderTflite::SerializeBuffer(base::span<const uint8_t> buffer) {
  const auto buffer_data = builder_.CreateVector(buffer.data(), buffer.size());
  const auto buffer_index = base::checked_cast<uint32_t>(buffers_.size());
  buffers_.emplace_back(::tflite::CreateBuffer(builder_, buffer_data));
  // The index of buffer is referenced by tensors.
  return buffer_index;
}

template <typename DataType>
  requires internal::IsSupportedTensorType<DataType>
int32_t GraphBuilderTflite::SerializeTensorWithBuffer(
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

int32_t GraphBuilderTflite::SerializeTemporaryTensor(
    base::span<const int32_t> dimensions,
    ::tflite::TensorType tensor_type) {
  const int32_t temporary_tensor_index =
      base::checked_cast<int32_t>(tensors_.size());
  tensors_.emplace_back(::tflite::CreateTensor(
      builder_, builder_.CreateVector<int32_t>(dimensions), tensor_type));

  return temporary_tensor_index;
}

uint32_t GraphBuilderTflite::GetOperatorCodeIndex(
    ::tflite::BuiltinOperator code,
    int32_t version) {
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
      /*custom_code=*/0, version, code));

  // The type of operation is determined by the index into the list of the valid
  // OperatorCodes.
  return operator_code_index;
}

const mojom::Operand& GraphBuilderTflite::GetOperand(
    uint64_t operand_id) const {
  return *graph_info_->id_to_operand_map.at(operand_id);
}

auto GraphBuilderTflite::SerializeUnaryOperation(
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

auto GraphBuilderTflite::SerializeCastOperation(
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

auto GraphBuilderTflite::SerializeBinaryOperation(
    ::tflite::BuiltinOperator code,
    int32_t lhs_tensor_index,
    int32_t rhs_tensor_index,
    int32_t output_tensor_index) -> OperatorOffset {
  const uint32_t operator_code_index = GetOperatorCodeIndex(code);
  const std::array<int32_t, 2> op_inputs = {lhs_tensor_index, rhs_tensor_index};
  const std::array<int32_t, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs));
}

auto GraphBuilderTflite::SerializeConcatOperation(
    base::span<const int32_t> input_tensor_indices,
    int32_t output_tensor_index,
    uint32_t axis) -> OperatorOffset {
  // Create `tflite::ConcatenationOptions` with axis.
  const auto concat_options =
      ::tflite::CreateConcatenationOptions(builder_, axis);

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_CONCATENATION);
  const std::array<int32_t, 1> operator_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index,
      builder_.CreateVector<int32_t>(input_tensor_indices),
      builder_.CreateVector<int32_t>(operator_outputs),
      ::tflite::BuiltinOptions_ConcatenationOptions, concat_options.Union());
}

int32_t GraphBuilderTflite::SerializeDequantizeOperation(
    int32_t input_tensor_index,
    base::span<const int32_t> input_dimensions) {
  const int32_t output_tensor_index =
      SerializeTemporaryTensor(input_dimensions, ::tflite::TensorType_FLOAT32);
  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_DEQUANTIZE);
  const std::array<int32_t, 1> op_inputs = {input_tensor_index};
  const std::array<int32_t, 1> op_outputs = {output_tensor_index};
  operators_.emplace_back(::tflite::CreateOperator(
      builder_, operator_code_index, builder_.CreateVector<int32_t>(op_inputs),
      builder_.CreateVector<int32_t>(op_outputs)));

  return output_tensor_index;
}

auto GraphBuilderTflite::SerializeMatmulOperation(int32_t a_tensor_index,
                                                  int32_t b_tensor_index,
                                                  int32_t output_tensor_index)
    -> OperatorOffset {
  const auto matmul_options =
      ::tflite::CreateBatchMatMulOptions(builder_, /*adj_x=*/false,
                                         /*adj_y=*/false);
  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_BATCH_MATMUL);
  const std::array<int32_t, 2> op_inputs = {a_tensor_index, b_tensor_index};
  const std::array<int32_t, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index, builder_.CreateVector<int32_t>(op_inputs),
      builder_.CreateVector<int32_t>(op_outputs),
      ::tflite::BuiltinOptions_BatchMatMulOptions, matmul_options.Union());
}

auto GraphBuilderTflite::SerializeLinearOperation(
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

auto GraphBuilderTflite::SerializeNormalizationOperation(
    base::span<const int32_t> input_dimensions,
    ::tflite::TensorType input_tensor_type,
    int32_t input_tensor_index,
    int32_t output_tensor_index,
    int32_t mean_tensor_index,
    int32_t variance_tensor_index,
    float epsilon,
    std::optional<int32_t> scale_tensor_index,
    std::optional<int32_t> bias_tensor_index) -> OperatorOffset {
  // Emulate normalization follows the expression `Scale * ((Input - Mean) /
  // sqrt(Variance + Epsilon)) + Bias`
  //
  // Serialize the subtraction operation for expression `Input - Mean`.
  CHECK_EQ(input_tensor_type, ::tflite::TensorType_FLOAT32);
  const int32_t output_tensor_index_of_sub =
      SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_SUB, input_tensor_index, mean_tensor_index,
      output_tensor_index_of_sub));

  // Serialize the subexpression `sqrt(Variance + Epsilon)`.
  const int32_t epsilon_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{epsilon},
      /*dimensions=*/{});
  const int32_t output_tensor_index_of_add =
      SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_ADD, variance_tensor_index,
      epsilon_tensor_index, output_tensor_index_of_add));
  const int32_t output_tensor_index_of_sqrt =
      SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeUnaryOperation(
      ::tflite::BuiltinOperator_SQRT, output_tensor_index_of_add,
      output_tensor_index_of_sqrt));

  // Serialize the intermediate expression `Scale * (output_tensor_of_sub /
  // output_tensor_of_sqrt)`.
  int32_t output_tensor_index_of_div = output_tensor_index;
  if (scale_tensor_index || bias_tensor_index) {
    output_tensor_index_of_div =
        SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  }
  OperatorOffset normalization_offset = SerializeBinaryOperation(
      ::tflite::BuiltinOperator_DIV, output_tensor_index_of_sub,
      output_tensor_index_of_sqrt, output_tensor_index_of_div);
  int32_t output_tensor_index_of_mul = output_tensor_index_of_div;
  if (scale_tensor_index) {
    operators_.emplace_back(normalization_offset);
    output_tensor_index_of_mul =
        bias_tensor_index
            ? SerializeTemporaryTensor(input_dimensions, input_tensor_type)
            : output_tensor_index;
    normalization_offset = SerializeBinaryOperation(
        ::tflite::BuiltinOperator_MUL, *scale_tensor_index,
        output_tensor_index_of_div, output_tensor_index_of_mul);
  }

  if (bias_tensor_index) {
    operators_.emplace_back(normalization_offset);
    normalization_offset = SerializeBinaryOperation(
        ::tflite::BuiltinOperator_ADD, output_tensor_index_of_mul,
        *bias_tensor_index, output_tensor_index);
  }

  return normalization_offset;
}

auto GraphBuilderTflite::SerializeReduceOperation(
    ::tflite::BuiltinOperator operator_code,
    int32_t input_tensor_index,
    int32_t output_tensor_index,
    base::span<const int32_t> axes,
    bool keep_dimensions) -> OperatorOffset {
  const std::array<int32_t, 1> axes_tensor_shape = {
      base::checked_cast<int32_t>(axes.size())};
  const int32_t axes_tensor_index =
      SerializeTensorWithBuffer<int32_t>(axes, axes_tensor_shape);

  const auto reduce_options =
      ::tflite::CreateReducerOptions(builder_, keep_dimensions);
  const uint32_t operator_code_index = GetOperatorCodeIndex(operator_code);
  const std::array<int32_t, 2> op_inputs = {input_tensor_index,
                                            axes_tensor_index};
  const std::array<int32_t, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index, builder_.CreateVector<int32_t>(op_inputs),
      builder_.CreateVector<int32_t>(op_outputs),
      ::tflite::BuiltinOptions_ReducerOptions, reduce_options.Union());
}

auto GraphBuilderTflite::SerializeReshapeOperation(
    int32_t input_tensor_index,
    int32_t output_tensor_index,
    base::span<const int32_t> new_shape) -> OperatorOffset {
  const auto reshape_options = ::tflite::CreateReshapeOptions(
      builder_,
      /*new_shape=*/builder_.CreateVector<int32_t>(new_shape));

  return SerializeUnaryOperation(::tflite::BuiltinOperator_RESHAPE,
                                 input_tensor_index, output_tensor_index,
                                 ::tflite::BuiltinOptions_ReshapeOptions,
                                 reshape_options.Union());
}

auto GraphBuilderTflite::SerializeSliceOperation(
    int32_t input_tensor_index,
    int32_t output_tensor_index,
    base::span<const int32_t> slice_starts,
    base::span<const int32_t> slice_sizes)
    -> base::expected<OperatorOffset, std::string> {
  CHECK_EQ(slice_starts.size(), slice_sizes.size());
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
      input_tensor_index, starts_tensor_index, sizes_tensor_index};
  const std::array<int32_t, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs));
}

auto GraphBuilderTflite::SerializeTransposeOperation(
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

auto GraphBuilderTflite::InsertPadOperation(const TensorInfo& input_tensor_info,
                                            base::span<const uint32_t> paddings)
    -> base::expected<int32_t, std::string> {
  // WebNN explicit padding is in [beginning_height, ending_height,
  // beginning_width, ending_width] sequence.
  const auto padding_rank = paddings.size();
  CHECK_EQ(padding_rank, 4u);

  // Create `tflite::Tensor` for the output operand of explicit padding operator
  // with the dimensions and data type.
  CHECK_EQ(input_tensor_info.dimensions.size(), 4u);
  base::FixedArray<int32_t> output_shape(padding_rank);
  for (size_t i = 0; i < padding_rank; ++i) {
    auto checked_dimension =
        base::MakeCheckedNum<int32_t>(input_tensor_info.dimensions[i]);
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
    output_shape[i] = checked_dimension.ValueOrDie();
  }

  const int32_t output_tensor_index =
      base::checked_cast<int32_t>(tensors_.size());
  tensors_.emplace_back(::tflite::CreateTensor(
      builder_, builder_.CreateVector<int32_t>(output_shape),
      input_tensor_info.data_type));

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
  std::array<int32_t, 2> op_inputs = {input_tensor_info.index,
                                      padding_tensor_index};
  const std::array<int32_t, 1> op_outputs = {output_tensor_index};
  operators_.emplace_back(::tflite::CreateOperator(
      builder_, operator_code_index, builder_.CreateVector<int32_t>(op_inputs),
      builder_.CreateVector<int32_t>(op_outputs)));

  return output_tensor_index;
}

int32_t GraphBuilderTflite::InsertTransposeOperation(
    base::span<const int32_t> input_dimensions,
    ::tflite::TensorType input_tensor_type,
    int32_t input_tensor_index,
    base::span<const uint32_t> permutation) {
  // Create `tflite::Tensor` for the output operand of Transpose operator with
  // the dimensions and tensor data type.
  const size_t input_rank = input_dimensions.size();
  CHECK_EQ(permutation.size(), input_rank);
  base::FixedArray<int32_t> output_shape(input_rank);
  for (size_t i = 0; i < input_rank; ++i) {
    output_shape[i] = input_dimensions[permutation[i]];
  }
  const int32_t output_tensor_index =
      SerializeTemporaryTensor(output_shape, input_tensor_type);
  operators_.emplace_back(SerializeTransposeOperation(
      input_tensor_index, output_tensor_index, permutation));

  return output_tensor_index;
}

int32_t GraphBuilderTflite::SerializeSubGraphPowMul(
    base::span<const int32_t> input_dimensions,
    ::tflite::TensorType input_tensor_type,
    int32_t input_tensor_index,
    float pow_exponent,
    float mul_alpha) {
  const int32_t output_tensor_index_of_pow =
      SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  const int32_t pow_exponent_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{pow_exponent},
      /*dimensions=*/{});
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_POW, input_tensor_index,
      pow_exponent_tensor_index, output_tensor_index_of_pow));

  const int32_t output_tensor_index_of_mul =
      SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  const int32_t mul_alpha_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{mul_alpha},
      /*dimensions=*/{});
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_MUL, output_tensor_index_of_pow,
      mul_alpha_tensor_index, output_tensor_index_of_mul));

  return output_tensor_index_of_mul;
}

auto GraphBuilderTflite::SerializeArgMinMax(const mojom::ArgMinMax& arg_min_max)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.arg_min_max_input.Has(
      GetOperand(arg_min_max.input_operand_id).descriptor.data_type()));
  CHECK(context_properties_.data_type_limits.arg_min_max_output.Has(
      GetOperand(arg_min_max.output_operand_id).descriptor.data_type()));

  // The WebNN axis option is uint32 data type, but TFLite axis needs int32
  // type, so the axis need to be validated here to not overflow.
  auto checked_axis = base::MakeCheckedNum<int32_t>(arg_min_max.axis);
  if (!checked_axis.IsValid()) {
    return base::unexpected("The axis in arg_min_max operation is too large.");
  }
  const std::array<int32_t, 1> axis_buffer = {checked_axis.ValueOrDie()};
  const std::array<int32_t, 1> axis_dimensions = {axis_buffer.size()};
  const int32_t axis_tensor_index =
      SerializeTensorWithBuffer<int32_t>(axis_buffer, axis_dimensions);

  ::tflite::BuiltinOperator operator_code;
  ::tflite::BuiltinOptions builtin_options_type;
  flatbuffers::Offset<void> builtin_options;
  const mojom::Operand& output_operand =
      GetOperand(arg_min_max.output_operand_id);

  ::tflite::TensorType output_type = ::tflite::TensorType_INT64;
  if (output_operand.descriptor.data_type() == OperandDataType::kInt32) {
    output_type = ::tflite::TensorType_INT32;
  } else {
    CHECK_EQ(output_operand.descriptor.data_type(), OperandDataType::kInt64);
  }
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

  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(arg_min_max.input_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(arg_min_max.output_operand_id));
  const uint32_t operator_code_index = GetOperatorCodeIndex(operator_code);
  const std::array<int32_t, 2> op_inputs = {input_tensor_info.index,
                                            axis_tensor_index};
  const std::array<int32_t, 1> op_outputs = {output_tensor_info.index};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs),
                                  builtin_options_type, builtin_options);
}

auto GraphBuilderTflite::SerializeBatchNormalization(
    const mojom::BatchNormalization& batch_normalization)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.batch_normalization_input.Has(
      GetOperand(batch_normalization.input_operand_id).descriptor.data_type()));
  ASSIGN_OR_RETURN(
      const TensorInfo& input_tensor_info,
      SerializeInputTensorInfo(batch_normalization.input_operand_id));
  CHECK_LT(batch_normalization.axis, input_tensor_info.dimensions.size());
  const ::tflite::TensorType input_tensor_type = input_tensor_info.data_type;
  const int32_t dimension_on_axis =
      input_tensor_info.dimensions[batch_normalization.axis];
  std::vector<int32_t> new_shape(input_tensor_info.dimensions.size(), 1);
  new_shape[batch_normalization.axis] = dimension_on_axis;

  // Reshape the 1-D tensor of the mean operand to the new shape.
  ASSIGN_OR_RETURN(
      const TensorInfo& mean_tensor_info,
      SerializeInputTensorInfo(batch_normalization.mean_operand_id));
  CHECK_EQ(mean_tensor_info.dimensions.size(), 1u);
  const int32_t reshape_mean_tensor_index =
      SerializeTemporaryTensor(new_shape, input_tensor_type);
  operators_.emplace_back(SerializeReshapeOperation(
      mean_tensor_info.index, reshape_mean_tensor_index, new_shape));

  // Reshape the 1-D tensor of the variance operand to the new shape.
  ASSIGN_OR_RETURN(
      const TensorInfo& variance_tensor_info,
      SerializeInputTensorInfo(batch_normalization.variance_operand_id));
  CHECK_EQ(variance_tensor_info.dimensions.size(), 1u);
  const int32_t reshape_variance_tensor_index =
      SerializeTemporaryTensor(new_shape, input_tensor_type);
  operators_.emplace_back(SerializeReshapeOperation(
      variance_tensor_info.index, reshape_variance_tensor_index, new_shape));

  // Reshape the 1-D tensor of the scale operand to the new shape if needed.
  std::optional<int32_t> reshape_scale_tensor_index;
  if (batch_normalization.scale_operand_id) {
    ASSIGN_OR_RETURN(
        const TensorInfo& scale_tensor_info,
        SerializeInputTensorInfo(*batch_normalization.scale_operand_id));
    CHECK_EQ(scale_tensor_info.dimensions.size(), 1u);
    reshape_scale_tensor_index =
        SerializeTemporaryTensor(new_shape, input_tensor_type);
    operators_.emplace_back(SerializeReshapeOperation(
        scale_tensor_info.index, *reshape_scale_tensor_index, new_shape));
  }

  // Reshape the 1-D tensor of the bias operand to the new shape if needed.
  std::optional<int32_t> reshape_bias_tensor_index;
  if (batch_normalization.bias_operand_id) {
    ASSIGN_OR_RETURN(
        const TensorInfo& bias_tensor_info,
        SerializeInputTensorInfo(*batch_normalization.bias_operand_id));
    CHECK_EQ(bias_tensor_info.dimensions.size(), 1u);
    reshape_bias_tensor_index =
        SerializeTemporaryTensor(new_shape, input_tensor_type);
    operators_.emplace_back(SerializeReshapeOperation(
        bias_tensor_info.index, *reshape_bias_tensor_index, new_shape));
  }

  ASSIGN_OR_RETURN(
      const TensorInfo& output_tensor_info,
      SerializeOutputTensorInfo(batch_normalization.output_operand_id));
  return SerializeNormalizationOperation(
      new_shape, input_tensor_type, input_tensor_info.index,
      output_tensor_info.index, reshape_mean_tensor_index,
      reshape_variance_tensor_index, batch_normalization.epsilon,
      reshape_scale_tensor_index, reshape_bias_tensor_index);
}

auto GraphBuilderTflite::SerializeClamp(const mojom::Clamp& clamp)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.clamp_input.Has(
      GetOperand(clamp.input_operand_id).descriptor.data_type()));

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

  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(clamp.input_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(clamp.output_operand_id));
  return SerializeUnaryOperation(code, input_tensor_info.index,
                                 output_tensor_info.index);
}

auto GraphBuilderTflite::SerializeConcat(const mojom::Concat& concat)
    -> base::expected<OperatorOffset, std::string> {
  // TODO(crbug.com/369649350): Support float16 without dequantize operator.
  base::FixedArray<int32_t> operator_inputs_index(
      concat.input_operand_ids.size());
  for (size_t i = 0; i < concat.input_operand_ids.size(); ++i) {
    ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                     SerializeInputTensorInfo(concat.input_operand_ids[i]));
    operator_inputs_index[i] = input_tensor_info.index;
  }
  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(concat.output_operand_id));

  return SerializeConcatOperation(operator_inputs_index,
                                  output_tensor_info.index, concat.axis);
}

auto GraphBuilderTflite::SerializeConv2d(const mojom::Conv2d& conv2d)
    -> base::expected<OperatorOffset, std::string> {
  const mojom::Operand& input_operand = GetOperand(conv2d.input_operand_id);
  const OperandDataType input_data_type = input_operand.descriptor.data_type();
  switch (conv2d.kind) {
    case mojom::Conv2d::Kind::kDirect:
      // TODO(crbug.com/328733319): Support other tensor data types.
      CHECK(context_properties_.data_type_limits.conv2d_input.Has(
          input_data_type));
      break;
    case mojom::Conv2d::Kind::kTransposed:
      // TODO(crbug.com/328733319): Support other tensor data types.
      CHECK(context_properties_.data_type_limits.conv_transpose2d_input.Has(
          input_data_type));
      // TODO(crbug.com/364348906): Support dilations and groups parameter for
      // convTranspose2d
      if (conv2d.dilations->height != 1 || conv2d.dilations->width != 1 ||
          conv2d.groups != 1) {
        return base::unexpected(
            "convTranspose2d doesn't support dilations and groups.");
      }
      break;
  }

  // Get tflite padding mode with the size2d of input, filter, dilation.
  const auto& input_shape = input_operand.descriptor.shape();
  CHECK_EQ(input_shape.size(), 4u);
  const uint32_t input_channels = input_shape[3];
  const mojom::Operand& output_operand = GetOperand(conv2d.output_operand_id);
  const auto& output_shape = output_operand.descriptor.shape();
  CHECK_EQ(output_shape.size(), 4u);
  const uint32_t output_channels = output_shape[3];
  const webnn::Size2d<uint32_t> input_size2d = {.height = input_shape[1],
                                                .width = input_shape[2]};
  // For nhwc input layout, the default filter layout is ohwi for
  // regular/transpose conv2d and ihwo for depthwise conv2d.
  const mojom::Operand& filter_operand = GetOperand(conv2d.filter_operand_id);
  CHECK_EQ(filter_operand.descriptor.Rank(), 4u);
  const auto& filter_shape = filter_operand.descriptor.shape();
  CHECK_EQ(filter_shape.size(), 4u);
  const webnn::Size2d<uint32_t> filter_size2d = {.height = filter_shape[1],
                                                 .width = filter_shape[2]};
  ASSIGN_OR_RETURN(
      TfLitePadding padding_mode,
      GetTfLitePaddingMode(*conv2d.padding, input_size2d, filter_size2d,
                           *conv2d.strides, *conv2d.dilations,
                           conv2d.kind == mojom::Conv2d::Kind::kTransposed));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(conv2d.input_operand_id));
  // Insert a Pad operator before TfLite Conv2d if needed for explicit padding.
  std::optional<int32_t> explicit_pad_index;
  if (padding_mode.paddings) {
    ASSIGN_OR_RETURN(
        explicit_pad_index,
        InsertPadOperation(input_tensor_info, padding_mode.paddings.value()));
  }

  // If there is no bias operand, serialize a empty buffer with the size of
  // output channel.
  int32_t bias_index;
  if (conv2d.bias_operand_id) {
    ASSIGN_OR_RETURN(const TensorInfo& bias_tensor_info,
                     SerializeInputTensorInfo(*conv2d.bias_operand_id));
    bias_index = bias_tensor_info.index;
  } else {
    const std::array<int32_t, 1> bias_shape = {
        base::checked_cast<int32_t>(output_channels)};
    bias_index = SerializeTensorWithBuffer<float>(
        std::vector<float>(output_channels), std::move(bias_shape));
  }

  // TODO(crbug.com/344633746): Consider fusing Conv2D activations when
  // possible.

  ASSIGN_OR_RETURN(const TensorInfo& filter_tensor_info,
                   SerializeInputTensorInfo(conv2d.filter_operand_id));
  std::vector<int32_t> op_inputs;
  ::tflite::BuiltinOperator operator_kind;
  ::tflite::BuiltinOptions builtin_options_type;
  flatbuffers::Offset<void> builtin_options;
  if (conv2d.kind == mojom::Conv2d::Kind::kDirect) {
    op_inputs = {explicit_pad_index.value_or(input_tensor_info.index),
                 filter_tensor_info.index, bias_index};
    if (webnn::IsDepthwiseConv2d(input_channels, output_channels,
                                 conv2d.groups)) {
      operator_kind = ::tflite::BuiltinOperator_DEPTHWISE_CONV_2D;
      builtin_options = ::tflite::CreateDepthwiseConv2DOptions(
                            builder_, padding_mode.mode, conv2d.strides->width,
                            conv2d.strides->height, /*depth_multiplier=*/1,
                            ::tflite::ActivationFunctionType_NONE,
                            conv2d.dilations->width, conv2d.dilations->height)
                            .Union();
      builtin_options_type = ::tflite::BuiltinOptions_DepthwiseConv2DOptions;
    } else {
      operator_kind = ::tflite::BuiltinOperator_CONV_2D;
      builtin_options =
          ::tflite::CreateConv2DOptions(
              builder_, padding_mode.mode, conv2d.strides->width,
              conv2d.strides->height, ::tflite::ActivationFunctionType_NONE,
              conv2d.dilations->width, conv2d.dilations->height)
              .Union();
      builtin_options_type = ::tflite::BuiltinOptions_Conv2DOptions;
    }
  } else {
    const auto signed_output_dimensions = ToSignedDimensions(output_shape);
    CHECK(signed_output_dimensions.has_value());
    const std::array<int32_t, 1> output_tensor_shape = {
        base::checked_cast<int32_t>(output_shape.size())};
    const int32_t output_shape_tensor_index =
        SerializeTensorWithBuffer<int32_t>(*signed_output_dimensions,
                                           output_tensor_shape);
    op_inputs = {output_shape_tensor_index, filter_tensor_info.index,
                 explicit_pad_index.value_or(input_tensor_info.index),
                 bias_index};
    operator_kind = ::tflite::BuiltinOperator_TRANSPOSE_CONV;
    builtin_options =
        ::tflite::CreateTransposeConvOptions(
            builder_, padding_mode.mode, conv2d.strides->width,
            conv2d.strides->height, ::tflite::ActivationFunctionType_NONE)
            .Union();
    builtin_options_type = ::tflite::BuiltinOptions_TransposeConvOptions;
  }
  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(conv2d.output_operand_id));
  const auto operator_code_index = GetOperatorCodeIndex(operator_kind);
  const std::array<int32_t, 1> op_outputs = {output_tensor_info.index};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs),
                                  builtin_options_type, builtin_options);
}

auto GraphBuilderTflite::SerializeElementWiseBinary(
    const mojom::ElementWiseBinary& op)
    -> base::expected<OperatorOffset, std::string> {
  const OperandDataType input_data_type =
      GetOperand(op.lhs_operand_id).descriptor.data_type();
  ::tflite::BuiltinOperator code;
  switch (op.kind) {
    case mojom::ElementWiseBinary::Kind::kAdd:
      CHECK(
          context_properties_.data_type_limits.add_input.Has(input_data_type));
      code = ::tflite::BuiltinOperator_ADD;
      break;
    case mojom::ElementWiseBinary::Kind::kSub:
      CHECK(
          context_properties_.data_type_limits.sub_input.Has(input_data_type));
      code = ::tflite::BuiltinOperator_SUB;
      break;
    case mojom::ElementWiseBinary::Kind::kMul:
      CHECK(
          context_properties_.data_type_limits.mul_input.Has(input_data_type));
      code = ::tflite::BuiltinOperator_MUL;
      break;
    case mojom::ElementWiseBinary::Kind::kDiv:
      CHECK(
          context_properties_.data_type_limits.div_input.Has(input_data_type));
      code = ::tflite::BuiltinOperator_DIV;
      break;
    case mojom::ElementWiseBinary::Kind::kMax:
      CHECK(
          context_properties_.data_type_limits.max_input.Has(input_data_type));
      code = ::tflite::BuiltinOperator_MAXIMUM;
      break;
    case mojom::ElementWiseBinary::Kind::kMin:
      CHECK(
          context_properties_.data_type_limits.min_input.Has(input_data_type));
      code = ::tflite::BuiltinOperator_MINIMUM;
      break;
    case mojom::ElementWiseBinary::Kind::kPow:
      CHECK(
          context_properties_.data_type_limits.pow_input.Has(input_data_type));
      code = ::tflite::BuiltinOperator_POW;
      break;
    case mojom::ElementWiseBinary::Kind::kEqual:
      CHECK(context_properties_.data_type_limits.equal_input.Has(
          input_data_type));
      code = ::tflite::BuiltinOperator_EQUAL;
      break;
    case mojom::ElementWiseBinary::Kind::kGreater:
      CHECK(context_properties_.data_type_limits.greater_input.Has(
          input_data_type));
      code = ::tflite::BuiltinOperator_GREATER;
      break;
    case mojom::ElementWiseBinary::Kind::kGreaterOrEqual:
      CHECK(context_properties_.data_type_limits.greater_or_equal_input.Has(
          input_data_type));
      code = ::tflite::BuiltinOperator_GREATER_EQUAL;
      break;
    case mojom::ElementWiseBinary::Kind::kLesser:
      CHECK(context_properties_.data_type_limits.lesser_input.Has(
          input_data_type));
      code = ::tflite::BuiltinOperator_LESS;
      break;
    case mojom::ElementWiseBinary::Kind::kLesserOrEqual:
      CHECK(context_properties_.data_type_limits.lesser_or_equal_input.Has(
          input_data_type));
      code = ::tflite::BuiltinOperator_LESS_EQUAL;
      break;
    case mojom::ElementWiseBinary::Kind::kLogicalAnd:
    case mojom::ElementWiseBinary::Kind::kLogicalOr:
    case mojom::ElementWiseBinary::Kind::kLogicalXor:
      // TODO(crbug.com/368085791): Implement logical binary ops for TFLite.
      return base::unexpected(
          "logicalAnd, logicalXor, and logicalXor are not yet supported on "
          "TFLite.");
  }

  ASSIGN_OR_RETURN(const TensorInfo& lhs_tensor_info,
                   SerializeInputTensorInfo(op.lhs_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& rhs_tensor_info,
                   SerializeInputTensorInfo(op.rhs_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(op.output_operand_id));
  return SerializeBinaryOperation(code, lhs_tensor_info.index,
                                  rhs_tensor_info.index,
                                  output_tensor_info.index);
}

auto GraphBuilderTflite::SerializeElementWiseUnary(
    const mojom::ElementWiseUnary& op)
    -> base::expected<OperatorOffset, std::string> {
  ASSIGN_OR_RETURN(
      const TensorInfo& input_tensor_info,
      SerializeInputTensorInfo(op.input_operand_id,
                               /*operation_supports_float16=*/op.kind ==
                                   mojom::ElementWiseUnary::Kind::kCast));
  ASSIGN_OR_RETURN(
      const TensorInfo& output_tensor_info,
      SerializeOutputTensorInfo(op.output_operand_id,
                                /*operation_supports_float16=*/op.kind ==
                                    mojom::ElementWiseUnary::Kind::kCast));
  const int32_t input_tensor_index = input_tensor_info.index;
  const int32_t output_tensor_index = output_tensor_info.index;
  const OperandDataType input_data_type =
      GetOperand(op.input_operand_id).descriptor.data_type();

  const DataTypeLimits data_type_limits = context_properties_.data_type_limits;
  switch (op.kind) {
    case mojom::ElementWiseUnary::Kind::kAbs: {
      CHECK(data_type_limits.abs_input.Has(input_data_type));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_ABS,
                                     input_tensor_index, output_tensor_index);
    }
    case mojom::ElementWiseUnary::Kind::kCast: {
      CHECK(data_type_limits.cast_input.Has(input_data_type));
      return SerializeCastOperation(
          input_tensor_index,
          OperandDataTypeToTFLite(
              GetOperand(op.input_operand_id).descriptor.data_type()),
          output_tensor_index,
          OperandDataTypeToTFLite(
              GetOperand(op.output_operand_id).descriptor.data_type()));
    }
    case mojom::ElementWiseUnary::Kind::kCeil: {
      CHECK(data_type_limits.ceil_input.Has(input_data_type));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_CEIL,
                                     input_tensor_index, output_tensor_index);
    }
    case mojom::ElementWiseUnary::Kind::kCos: {
      CHECK(data_type_limits.cos_input.Has(input_data_type));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_COS,
                                     input_tensor_index, output_tensor_index);
    }
    case mojom::ElementWiseUnary::Kind::kExp: {
      CHECK(data_type_limits.exp_input.Has(input_data_type));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_EXP,
                                     input_tensor_index, output_tensor_index);
    }
    case mojom::ElementWiseUnary::Kind::kFloor: {
      CHECK(data_type_limits.floor_input.Has(input_data_type));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_FLOOR,
                                     input_tensor_index, output_tensor_index);
    }
    case mojom::ElementWiseUnary::Kind::kIdentity: {
      CHECK(data_type_limits.identity_input.Has(input_data_type));
      // Implement WebNN identity operation with TFLite reshape operator, the
      // output shape is the same as input.
      // TODO(crbug.com/336399247): Skip identity implementation with
      // redirecting output tensor to input.
      return SerializeReshapeOperation(input_tensor_info.index,
                                       output_tensor_info.index,
                                       output_tensor_info.dimensions);
    }
    case mojom::ElementWiseUnary::Kind::kLog: {
      CHECK(data_type_limits.log_input.Has(input_data_type));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_LOG,
                                     input_tensor_index, output_tensor_index);
    }
    case mojom::ElementWiseUnary::Kind::kLogicalNot: {
      CHECK(data_type_limits.logical_not_input.Has(input_data_type));
      return SerializeLogicalNot(input_tensor_info, output_tensor_info);
    }
    case mojom::ElementWiseUnary::Kind::kNeg: {
      CHECK(data_type_limits.neg_input.Has(input_data_type));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_NEG,
                                     input_tensor_index, output_tensor_index);
    }
    case mojom::ElementWiseUnary::Kind::kReciprocal: {
      CHECK(data_type_limits.reciprocal_input.Has(input_data_type));
      return SerializeReciprocal(input_tensor_info, output_tensor_info);
    }
    case mojom::ElementWiseUnary::Kind::kSign: {
      CHECK(data_type_limits.sign_input.Has(input_data_type));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_SIGN,
                                     input_tensor_index, output_tensor_index);
    }
    case mojom::ElementWiseUnary::Kind::kSin: {
      CHECK(data_type_limits.sin_input.Has(input_data_type));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_SIN,
                                     input_tensor_index, output_tensor_index);
    }
    case mojom::ElementWiseUnary::Kind::kSqrt: {
      CHECK(data_type_limits.sqrt_input.Has(input_data_type));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_SQRT,
                                     input_tensor_index, output_tensor_index);
    }
    case mojom::ElementWiseUnary::Kind::kTan: {
      CHECK(data_type_limits.tan_input.Has(input_data_type));
      return SerializeTan(input_tensor_info, output_tensor_info);
    }
    case mojom::ElementWiseUnary::Kind::kErf: {
      CHECK(data_type_limits.erf_input.Has(input_data_type));
      return SerializeErf(input_tensor_info, output_tensor_info);
    }
  }
}

auto GraphBuilderTflite::SerializeElu(const mojom::Elu& elu)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.elu_input.Has(
      GetOperand(elu.input_operand_id).descriptor.data_type()));

  if (elu.alpha != 1.0) {
    // TODO: crbug.com/328736354 - Support custom alpha values.
    return base::unexpected(
        "Setting a custom alpha is not supported in tflite schema.");
  }

  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(elu.input_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(elu.output_operand_id));
  return SerializeUnaryOperation(::tflite::BuiltinOperator_ELU,
                                 input_tensor_info.index,
                                 output_tensor_info.index);
}

auto GraphBuilderTflite::SerializeErf(const TensorInfo& input_tensor_info,
                                      const TensorInfo& output_tensor_info)
    -> base::expected<OperatorOffset, std::string> {
  // Emulate the erf operation with the expression `erf(x) = sign(x) * (1 - (a1
  // * t + a2 * pow(t, 2) + ... + a5 * pow(t, 5)) * exp(-pow(x, 2)))`, the `t`
  // is the subexpression `1 / (1 + p * |x|)` as documented here:
  // https://en.wikipedia.org/wiki/Error_function
  const std::array<float, 5> constants = {/*a1*/ 0.254829592,
                                          /*a2*/ -0.284496736,
                                          /*a3*/ 1.421413741,
                                          /*a4*/ -1.453152027,
                                          /*a5*/ 1.061405429};
  const float p = 0.3275911;

  // Compute the subexpression `t = 1 / (1 + p * |x|)`.
  const int32_t input_tensor_index = input_tensor_info.index;
  const int32_t output_tensor_index_of_abs = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeUnaryOperation(::tflite::BuiltinOperator_ABS,
                                                  input_tensor_index,
                                                  output_tensor_index_of_abs));
  const int32_t output_tensor_index_of_line = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeLinearOperation(
      input_tensor_info.dimensions, input_tensor_info.data_type,
      output_tensor_index_of_abs, output_tensor_index_of_line, p, 1.0));
  const int32_t constant_one_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{1.0},
      /*dimensions=*/{});
  const int32_t t_expression_tensor_index = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_DIV, constant_one_tensor_index,
      output_tensor_index_of_line, t_expression_tensor_index));

  // Compute subexpression `(a1 * t + a2 * pow(t, 2) + ... + a5 * pow(t, 5))`.
  std::optional<int32_t> sum_pow_mul_tensor_index;
  for (size_t i = 0; i < constants.size(); ++i) {
    const int32_t output_tensor_index_of_pow_mul = SerializeSubGraphPowMul(
        input_tensor_info.dimensions, input_tensor_info.data_type,
        t_expression_tensor_index,
        /*pow_exponent=*/i + 1,
        /*mul_alpha=*/constants[i]);
    if (sum_pow_mul_tensor_index) {
      const int32_t output_tensor_index_of_add = SerializeTemporaryTensor(
          input_tensor_info.dimensions, input_tensor_info.data_type);
      operators_.emplace_back(SerializeBinaryOperation(
          ::tflite::BuiltinOperator_ADD, output_tensor_index_of_pow_mul,
          *sum_pow_mul_tensor_index, output_tensor_index_of_add));
      sum_pow_mul_tensor_index = output_tensor_index_of_add;
    } else {
      sum_pow_mul_tensor_index = output_tensor_index_of_pow_mul;
    }
  }

  // Compute the subexpression `exp(-pow(x, 2))`.
  const int32_t output_tensor_index_of_pow = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  const int32_t pow_exponent_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{2.0},
      /*dimensions=*/{});
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_POW, input_tensor_index,
      pow_exponent_tensor_index, output_tensor_index_of_pow));
  const int32_t output_tensor_index_of_neg = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeUnaryOperation(::tflite::BuiltinOperator_NEG,
                                                  output_tensor_index_of_pow,
                                                  output_tensor_index_of_neg));
  const int32_t output_tensor_index_of_exp = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeUnaryOperation(::tflite::BuiltinOperator_EXP,
                                                  output_tensor_index_of_neg,
                                                  output_tensor_index_of_exp));

  // Compute `1 - (the sum of pow mul subexpression) * (the pow exp
  // subexpression)`.
  const int32_t output_tensor_index_of_mul = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_MUL, output_tensor_index_of_exp,
      *sum_pow_mul_tensor_index, output_tensor_index_of_mul));
  const int32_t output_tensor_index_of_sub = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_SUB, constant_one_tensor_index,
      output_tensor_index_of_mul, output_tensor_index_of_sub));

  // Compute the subexpression `sign = sign(x)`
  const int32_t output_tensor_index_of_sign = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(
      SerializeUnaryOperation(::tflite::BuiltinOperator_SIGN,
                              input_tensor_index, output_tensor_index_of_sign));

  return SerializeBinaryOperation(
      ::tflite::BuiltinOperator_MUL, output_tensor_index_of_sign,
      output_tensor_index_of_sub, output_tensor_info.index);
}

auto GraphBuilderTflite::SerializeExpand(const mojom::Expand& expand)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.expand_input.Has(
      GetOperand(expand.output_operand_id).descriptor.data_type()));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(expand.input_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(expand.output_operand_id));

  // Serialize the expanded shape to tflite tensor with output dimensions.
  const int32_t output_rank =
      base::checked_cast<int32_t>(output_tensor_info.dimensions.size());
  const int32_t new_shape_tensor_index = SerializeTensorWithBuffer<int32_t>(
      output_tensor_info.dimensions, std::array<int32_t, 1>{output_rank});

  const uint32_t operator_code_index = GetOperatorCodeIndex(
      ::tflite::BuiltinOperator_BROADCAST_TO, /*version=*/2);
  const std::array<int32_t, 2> op_inputs = {input_tensor_info.index,
                                            new_shape_tensor_index};
  const std::array<int32_t, 1> op_outputs = {output_tensor_info.index};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs));
}

auto GraphBuilderTflite::SerializeGather(const mojom::Gather& gather)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.gather_input.Has(
      GetOperand(gather.input_operand_id).descriptor.data_type()));
  ASSIGN_OR_RETURN(const TensorInfo& indices_tensor_info,
                   SerializeInputTensorInfo(gather.indices_operand_id));
  // The WebNN indices must be one of type uint32, int32, int64, but TFLite
  // indices need int32 or int64 type, so a cast operation need to be inserted
  // before Gather if indices data type is uint32.
  int32_t indices_tensor_index = indices_tensor_info.index;
  if (indices_tensor_info.data_type == ::tflite::TensorType_UINT32) {
    indices_tensor_index = SerializeTemporaryTensor(
        indices_tensor_info.dimensions, ::tflite::TensorType_INT64);

    operators_.emplace_back(SerializeCastOperation(
        indices_tensor_info.index,
        /*input_tensor_type=*/::tflite::TensorType_UINT32, indices_tensor_index,
        /*output_tensor_type=*/::tflite::TensorType_INT64));
  } else {
    CHECK(indices_tensor_info.data_type == ::tflite::TensorType_INT64 ||
          indices_tensor_info.data_type == ::tflite::TensorType_INT32);
  }

  // The WebNN axis option is uint32 data type, but TFLite axis needs int32
  // type, so the axis need to be validated here to not overflow.
  auto checked_axis = base::MakeCheckedNum<int32_t>(gather.axis);
  if (!checked_axis.IsValid()) {
    return base::unexpected("The axis in gather operation is too large.");
  }
  const auto gather_options =
      ::tflite::CreateGatherOptions(builder_, checked_axis.ValueOrDie());

  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(gather.input_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(gather.output_operand_id));
  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_GATHER);
  const std::array<int32_t, 2> op_inputs = {input_tensor_info.index,
                                            indices_tensor_index};
  const std::array<int32_t, 1> op_outputs = {output_tensor_info.index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index, builder_.CreateVector<int32_t>(op_inputs),
      builder_.CreateVector<int32_t>(op_outputs),
      ::tflite::BuiltinOptions_GatherOptions, gather_options.Union());
}

auto GraphBuilderTflite::SerializeGelu(const mojom::Gelu& gelu)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.gelu_input.Has(
      GetOperand(gelu.input_operand_id).descriptor.data_type()));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(gelu.input_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(gelu.output_operand_id));

  return SerializeUnaryOperation(::tflite::BuiltinOperator_GELU,
                                 input_tensor_info.index,
                                 output_tensor_info.index);
}

auto GraphBuilderTflite::SerializeGemm(const mojom::Gemm& gemm)
    -> base::expected<OperatorOffset, std::string> {
  // Check for unsupported inputs.
  CHECK(context_properties_.data_type_limits.gemm_input.Has(
      GetOperand(gemm.output_operand_id).descriptor.data_type()));

  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(gemm.output_operand_id));
  CHECK_EQ(output_tensor_info.dimensions.size(), 2u);
  std::optional<TensorInfo> c_tensor_info;
  if (gemm.c_operand_id.has_value()) {
    // The TFLite fully connected operator only supports a 1-D bias tensor with
    // `output_channels` dimensions.
    const int32_t output_channels = output_tensor_info.dimensions[1];
    ASSIGN_OR_RETURN(c_tensor_info,
                     SerializeInputTensorInfo(*gemm.c_operand_id));
    if (c_tensor_info->dimensions.size() != 1 ||
        c_tensor_info->dimensions[0] != output_channels) {
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
  ASSIGN_OR_RETURN(const TensorInfo& b_tensor_info,
                   SerializeInputTensorInfo(gemm.b_operand_id));
  int32_t filter_index = b_tensor_info.index;
  if (!gemm.b_transpose) {
    CHECK_EQ(b_tensor_info.dimensions.size(), 2u);
    const std::array<uint32_t, 2> permutation = {1u, 0u};
    filter_index = InsertTransposeOperation(b_tensor_info.dimensions,
                                            b_tensor_info.data_type,
                                            b_tensor_info.index, permutation);
  }

  ASSIGN_OR_RETURN(const TensorInfo& a_tensor_info,
                   SerializeInputTensorInfo(gemm.a_operand_id));
  std::vector<int32_t> op_inputs = {a_tensor_info.index, filter_index};
  if (gemm.c_operand_id.has_value()) {
    op_inputs.push_back(c_tensor_info->index);
  }

  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_FULLY_CONNECTED);
  const std::array<int32_t, 1> op_outputs = {output_tensor_info.index};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs));
}

// Serialize a sub graph (input * weight + bias) for gru cell.
//
//     [input]   [weight]
//         \        /
//           Matmul   [bias]
//             \        /
//                 add
//                  |
//              [output]
int32_t GraphBuilderTflite::SerializeSubGraphMatmulAdd(
    base::span<const int32_t> input_dimensions,
    ::tflite::TensorType input_tensor_type,
    int32_t input_tensor_index,
    int32_t weight_tensor_index,
    std::optional<int32_t> bias_tensor_index) {
  CHECK_EQ(input_tensor_type, ::tflite::TensorType_FLOAT32);
  const int32_t output_tensor_index_of_matmul =
      SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeMatmulOperation(
      input_tensor_index, weight_tensor_index, output_tensor_index_of_matmul));

  int32_t output_tensor_index = output_tensor_index_of_matmul;
  if (bias_tensor_index) {
    output_tensor_index =
        SerializeTemporaryTensor(input_dimensions, input_tensor_type);
    operators_.emplace_back(SerializeBinaryOperation(
        ::tflite::BuiltinOperator_ADD, output_tensor_index_of_matmul,
        *bias_tensor_index, output_tensor_index));
  }

  return output_tensor_index;
}

// Serialize a sub graph (slice appending transpose operation) for gru cell.
//
//     [input]
//        |
//      slice
//        |
//     transpose
//        |
//     [output]
auto GraphBuilderTflite::SerializeSubGraphSliceTranspose(
    ::tflite::TensorType input_tensor_type,
    int32_t input_tensor_index,
    base::span<const int32_t> slice_starts,
    base::span<const int32_t> slice_sizes)
    -> base::expected<int32_t, std::string> {
  CHECK_EQ(input_tensor_type, ::tflite::TensorType_FLOAT32);
  // The output is a tensor the same as the specified slice sizes.
  const int32_t output_tensor_index_of_slice =
      SerializeTemporaryTensor(slice_sizes, input_tensor_type);
  ASSIGN_OR_RETURN(
      OperatorOffset operator_offset,
      SerializeSliceOperation(input_tensor_index, output_tensor_index_of_slice,
                              slice_starts, slice_sizes));
  operators_.emplace_back(operator_offset);

  const int32_t output_tensor_index =
      SerializeTemporaryTensor(slice_sizes, input_tensor_type);
  std::vector<uint32_t> permutation(slice_sizes.size());
  std::iota(permutation.rbegin(), permutation.rend(), 0);
  operators_.emplace_back(SerializeTransposeOperation(
      output_tensor_index_of_slice, output_tensor_index, permutation));

  return output_tensor_index;
}

auto GraphBuilderTflite::SerializeGruGate(
    const GruCellOperation& gru_cell,
    GruGateType type,
    std::optional<int32_t> reset_gate_tensor_index)
    -> base::expected<int32_t, std::string> {
  CHECK_EQ(gru_cell.input_dimensions.size(), 2u);
  const int32_t hidden_size = gru_cell.hidden_size;
  const std::array<int32_t, 2> output_shape = {gru_cell.input_dimensions[0],
                                               hidden_size};
  const int32_t input_size = gru_cell.input_dimensions[1];

  base::FixedArray<::tflite::BuiltinOperator> activation_operator_codes(
      gru_cell.activations.size());
  for (size_t i = 0; i < gru_cell.activations.size(); ++i) {
    activation_operator_codes[i] =
        GetRecurrentNetworkActivation(gru_cell.activations[i]);
  }

  ::tflite::BuiltinOperator activation_code;
  int32_t slice_start;
  switch (type) {
    case GruGateType::kUpdate: {
      activation_code = activation_operator_codes[0];
      slice_start =
          gru_cell.layout == mojom::GruWeightLayout::kRzn ? hidden_size : 0;
      break;
    }
    case GruGateType::kReset: {
      activation_code = activation_operator_codes[0];
      slice_start =
          gru_cell.layout == mojom::GruWeightLayout::kZrn ? hidden_size : 0;
      break;
    }
    case GruGateType::kNew: {
      activation_code = activation_operator_codes[1];
      slice_start = 2 * hidden_size;
      break;
    }
  }

  // input * weight + bias.
  ::tflite::TensorType input_tensor_type = gru_cell.input_tensor_type;
  std::optional<int32_t> bias_tensor_index;
  if (gru_cell.bias_tensor_index) {
    bias_tensor_index = SerializeTemporaryTensor(
        std::array<int32_t, 1>({hidden_size}), input_tensor_type);
    ASSIGN_OR_RETURN(
        OperatorOffset operator_offset,
        SerializeSliceOperation(*gru_cell.bias_tensor_index, *bias_tensor_index,
                                std::array<int32_t, 1>({slice_start}),
                                std::array<int32_t, 1>({hidden_size})));
    operators_.emplace_back(operator_offset);
  }
  ASSIGN_OR_RETURN(const int32_t weight_tensor_index,
                   SerializeSubGraphSliceTranspose(
                       input_tensor_type, gru_cell.weight_tensor_index,
                       std::array<int32_t, 2>({slice_start, 0}),
                       std::array<int32_t, 2>({hidden_size, input_size})));
  const int32_t output_tensor_index_of_weight = SerializeSubGraphMatmulAdd(
      output_shape, input_tensor_type, gru_cell.input_tensor_index,
      weight_tensor_index, bias_tensor_index);

  // hiddenState * recurrentWeight + recurrentBias.
  std::optional<int32_t> recurrent_bias_tensor_index;
  if (gru_cell.recurrent_bias_tensor_index) {
    recurrent_bias_tensor_index = SerializeTemporaryTensor(
        std::array<int32_t, 1>({hidden_size}), input_tensor_type);
    ASSIGN_OR_RETURN(
        OperatorOffset operator_offset,
        SerializeSliceOperation(*gru_cell.recurrent_bias_tensor_index,
                                *recurrent_bias_tensor_index,
                                std::array<int32_t, 1>({slice_start}),
                                std::array<int32_t, 1>({hidden_size})));
    operators_.emplace_back(operator_offset);
  }
  ASSIGN_OR_RETURN(
      const int32_t recurrent_weight_tensor_index,
      SerializeSubGraphSliceTranspose(
          input_tensor_type, gru_cell.recurrent_weight_tensor_index,
          std::array<int32_t, 2>({slice_start, 0}),
          std::array<int32_t, 2>({hidden_size, hidden_size})));
  int32_t hidden_state_tensor_index = gru_cell.hidden_state_tensor_index;
  // Apply the reset gate before matrix multiplication for new gate if needed.
  if (type == GruGateType::kNew && !gru_cell.reset_after) {
    const int32_t output_tensor_index_of_mul =
        SerializeTemporaryTensor(output_shape, input_tensor_type);
    operators_.emplace_back(SerializeBinaryOperation(
        ::tflite::BuiltinOperator_MUL, *reset_gate_tensor_index,
        hidden_state_tensor_index, output_tensor_index_of_mul));
    hidden_state_tensor_index = output_tensor_index_of_mul;
  }
  int32_t output_tensor_index_of_recurrent_weight = SerializeSubGraphMatmulAdd(
      output_shape, input_tensor_type, hidden_state_tensor_index,
      recurrent_weight_tensor_index, recurrent_bias_tensor_index);
  // Apply the reset gate after matrix multiplication for new gate if needed.
  if (type == GruGateType::kNew && gru_cell.reset_after) {
    const int32_t output_tensor_index_of_mul =
        SerializeTemporaryTensor(output_shape, input_tensor_type);
    operators_.emplace_back(SerializeBinaryOperation(
        ::tflite::BuiltinOperator_MUL, *reset_gate_tensor_index,
        output_tensor_index_of_recurrent_weight, output_tensor_index_of_mul));
    output_tensor_index_of_recurrent_weight = output_tensor_index_of_mul;
  }

  // Add the result of the above two expressions (element-wise multiplication
  // between the input / hiddenState and the respective weights / recurrent
  // weights).
  const int32_t output_tensor_index_of_add =
      SerializeTemporaryTensor(output_shape, input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_ADD, output_tensor_index_of_weight,
      output_tensor_index_of_recurrent_weight, output_tensor_index_of_add));

  // Apply first activation for the update and reset gate, the second activation
  // for the new gate.
  const int32_t output_tensor_index_of_gate =
      SerializeTemporaryTensor(output_shape, input_tensor_type);
  operators_.emplace_back(SerializeUnaryOperation(activation_code,
                                                  output_tensor_index_of_add,
                                                  output_tensor_index_of_gate));

  return output_tensor_index_of_gate;
}

GraphBuilderTflite::RecurrentNetworkBase::RecurrentNetworkBase(
    base::span<const int32_t> input_dimensions,
    ::tflite::TensorType input_tensor_type,
    int32_t input_tensor_index,
    int32_t weight_tensor_index,
    int32_t recurrent_weight_tensor_index,
    std::optional<int32_t> bias_tensor_index,
    std::optional<int32_t> recurrent_bias_tensor_index,
    int32_t hidden_state_tensor_index,
    int32_t hidden_size,
    base::span<const mojom::RecurrentNetworkActivation> activations)
    : input_dimensions(input_dimensions),
      input_tensor_type(input_tensor_type),
      input_tensor_index(input_tensor_index),
      weight_tensor_index(weight_tensor_index),
      recurrent_weight_tensor_index(recurrent_weight_tensor_index),
      bias_tensor_index(bias_tensor_index),
      recurrent_bias_tensor_index(recurrent_bias_tensor_index),
      hidden_state_tensor_index(hidden_state_tensor_index),
      hidden_size(hidden_size),
      activations(activations) {}

GraphBuilderTflite::RecurrentNetworkBase::~RecurrentNetworkBase() = default;

GraphBuilderTflite::GruCellOperation::GruCellOperation(
    base::span<const int32_t> input_dimensions,
    ::tflite::TensorType input_tensor_type,
    int32_t input_tensor_index,
    int32_t output_tensor_index,
    int32_t weight_tensor_index,
    int32_t recurrent_weight_tensor_index,
    std::optional<int32_t> bias_tensor_index,
    std::optional<int32_t> recurrent_bias_tensor_index,
    int32_t hidden_state_tensor_index,
    int32_t hidden_size,
    bool reset_after,
    mojom::GruWeightLayout layout,
    base::span<const mojom::RecurrentNetworkActivation> activations)
    : RecurrentNetworkBase(input_dimensions,
                           input_tensor_type,
                           input_tensor_index,
                           weight_tensor_index,
                           recurrent_weight_tensor_index,
                           bias_tensor_index,
                           recurrent_bias_tensor_index,
                           hidden_state_tensor_index,
                           hidden_size,
                           activations),
      output_tensor_index(output_tensor_index),
      reset_after(reset_after),
      layout(layout) {}

GraphBuilderTflite::GruCellOperation::~GruCellOperation() = default;

auto GraphBuilderTflite::SerializeGruCell(const mojom::GruCell& gru_cell)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.gru_cell_input.Has(
      GetOperand(gru_cell.input_operand_id).descriptor.data_type()));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(gru_cell.input_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& weight_tensor_info,
                   SerializeInputTensorInfo(gru_cell.weight_operand_id));
  ASSIGN_OR_RETURN(
      const TensorInfo& recurrent_weight_tensor_info,
      SerializeInputTensorInfo(gru_cell.recurrent_weight_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& hidden_state_tensor_info,
                   SerializeInputTensorInfo(gru_cell.hidden_state_operand_id));
  std::optional<int32_t> bias_tensor_index;
  if (gru_cell.bias_operand_id) {
    ASSIGN_OR_RETURN(const TensorInfo& bias_tensor_info,
                     SerializeInputTensorInfo(*gru_cell.bias_operand_id));
    bias_tensor_index = bias_tensor_info.index;
  }
  std::optional<int32_t> recurrent_bias_tensor_index;
  if (gru_cell.recurrent_bias_operand_id) {
    ASSIGN_OR_RETURN(
        const TensorInfo& recurrent_bias_tensor_info,
        SerializeInputTensorInfo(*gru_cell.recurrent_bias_operand_id));
    recurrent_bias_tensor_index = recurrent_bias_tensor_info.index;
  }
  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(gru_cell.output_operand_id));

  CHECK_EQ(input_tensor_info.dimensions.size(), 2u);
  const auto checked_hidden_size =
      base::MakeCheckedNum<int32_t>(gru_cell.hidden_size);
  if (!checked_hidden_size.IsValid()) {
    return base::unexpected("The hidden size is too large.");
  }

  GruCellOperation gru_cell_operation(
      input_tensor_info.dimensions, input_tensor_info.data_type,
      input_tensor_info.index, output_tensor_info.index,
      weight_tensor_info.index, recurrent_weight_tensor_info.index,
      bias_tensor_index, recurrent_bias_tensor_index,
      hidden_state_tensor_info.index, checked_hidden_size.ValueOrDie(),
      gru_cell.reset_after, gru_cell.layout, gru_cell.activations);

  return SerializeGruCellOperation(gru_cell_operation);
}

auto GraphBuilderTflite::SerializeGruCellOperation(
    const GruCellOperation& gru_cell)
    -> base::expected<OperatorOffset, std::string> {
  // Compute the update gate.
  ASSIGN_OR_RETURN(const int32_t update_gate_tensor_index,
                   SerializeGruGate(gru_cell, GruGateType::kUpdate));

  // Compute the reset gate.
  ASSIGN_OR_RETURN(const int32_t reset_gate_tensor_index,
                   SerializeGruGate(gru_cell, GruGateType::kReset));

  // Compute the new gate.
  ASSIGN_OR_RETURN(
      const int32_t new_gate_tensor_index,
      SerializeGruGate(gru_cell, GruGateType::kNew, reset_gate_tensor_index));

  const std::array<int32_t, 2> output_shape = {gru_cell.input_dimensions[0],
                                               gru_cell.hidden_size};
  // Compute mul(newGate, sub(one, updateGate)).
  const int32_t scalar_one_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{1.0},
      /*dimensions=*/{});
  const int32_t output_tensor_index_of_sub =
      SerializeTemporaryTensor(output_shape, gru_cell.input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_SUB, scalar_one_tensor_index,
      update_gate_tensor_index, output_tensor_index_of_sub));
  const int32_t new_gate_tensor_index_of_mul =
      SerializeTemporaryTensor(output_shape, gru_cell.input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_MUL, new_gate_tensor_index,
      output_tensor_index_of_sub, new_gate_tensor_index_of_mul));

  // Compute mul(updateGate, hiddenState).
  const int32_t update_gate_tensor_index_of_mul =
      SerializeTemporaryTensor(output_shape, gru_cell.input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_MUL, update_gate_tensor_index,
      gru_cell.hidden_state_tensor_index, update_gate_tensor_index_of_mul));

  // Compute the new hidden state with adding the new gate and update gate.
  return SerializeBinaryOperation(
      ::tflite::BuiltinOperator_ADD, new_gate_tensor_index_of_mul,
      update_gate_tensor_index_of_mul, gru_cell.output_tensor_index);
}

GraphBuilderTflite::LstmCellOperation::LstmCellOperation(
    base::span<const int32_t> input_dimensions,
    ::tflite::TensorType input_tensor_type,
    int32_t input_tensor_index,
    base::span<const int32_t> output_tensor_indices,
    int32_t weight_tensor_index,
    int32_t recurrent_weight_tensor_index,
    std::optional<int32_t> bias_tensor_index,
    std::optional<int32_t> recurrent_bias_tensor_index,
    int32_t hidden_state_tensor_index,
    int32_t hidden_size,
    int32_t cell_state_tensor_index,
    std::optional<int32_t> peephole_weight_tensor_index,
    mojom::LstmWeightLayout layout,
    base::span<const mojom::RecurrentNetworkActivation> activations)
    : RecurrentNetworkBase(input_dimensions,
                           input_tensor_type,
                           input_tensor_index,
                           weight_tensor_index,
                           recurrent_weight_tensor_index,
                           bias_tensor_index,
                           recurrent_bias_tensor_index,
                           hidden_state_tensor_index,
                           hidden_size,
                           activations),
      output_tensor_indices(output_tensor_indices),
      cell_state_tensor_index(cell_state_tensor_index),
      peephole_weight_tensor_index(peephole_weight_tensor_index),
      layout(layout) {}

GraphBuilderTflite::LstmCellOperation::~LstmCellOperation() = default;

base::expected<int32_t, std::string> GraphBuilderTflite::SerializeLstmGate(
    const LstmCellOperation& lstm_cell,
    LstmGateType type) {
  CHECK_EQ(lstm_cell.input_dimensions.size(), 2u);
  const int32_t hidden_size = lstm_cell.hidden_size;
  const std::array<int32_t, 2> output_shape = {lstm_cell.input_dimensions[0],
                                               hidden_size};
  const int32_t input_size = lstm_cell.input_dimensions[1];

  base::FixedArray<::tflite::BuiltinOperator> activation_operator_codes(
      lstm_cell.activations.size());
  for (size_t i = 0; i < lstm_cell.activations.size(); ++i) {
    activation_operator_codes[i] =
        GetRecurrentNetworkActivation(lstm_cell.activations[i]);
  }

  CHECK(lstm_cell.layout == mojom::LstmWeightLayout::kIofg ||
        lstm_cell.layout == mojom::LstmWeightLayout::kIfgo);
  ::tflite::BuiltinOperator activation_code;
  int32_t slice_start;
  switch (type) {
    case LstmGateType::kInput: {
      activation_code = activation_operator_codes[0];
      slice_start = 0;
      break;
    }
    case LstmGateType::kForget: {
      activation_code = activation_operator_codes[0];
      slice_start = lstm_cell.layout == mojom::LstmWeightLayout::kIofg
                        ? 2 * hidden_size
                        : hidden_size;
      break;
    }
    case LstmGateType::kCell: {
      activation_code = activation_operator_codes[1];
      slice_start = lstm_cell.layout == mojom::LstmWeightLayout::kIofg
                        ? 3 * hidden_size
                        : 2 * hidden_size;
      break;
    }
    case LstmGateType::kOutput: {
      activation_code = activation_operator_codes[0];
      slice_start = lstm_cell.layout == mojom::LstmWeightLayout::kIofg
                        ? hidden_size
                        : 3 * hidden_size;
      break;
    }
  }

  // input * weight + bias.
  const ::tflite::TensorType input_tensor_type = lstm_cell.input_tensor_type;
  std::optional<int32_t> bias_tensor_index;
  if (lstm_cell.bias_tensor_index) {
    bias_tensor_index = SerializeTemporaryTensor(
        std::array<int32_t, 1>({hidden_size}), input_tensor_type);
    ASSIGN_OR_RETURN(OperatorOffset operator_offset,
                     SerializeSliceOperation(
                         *lstm_cell.bias_tensor_index, *bias_tensor_index,
                         std::array<int32_t, 1>({slice_start}),
                         std::array<int32_t, 1>({hidden_size})));
    operators_.emplace_back(operator_offset);
  }
  ASSIGN_OR_RETURN(const int32_t weight_tensor_index,
                   SerializeSubGraphSliceTranspose(
                       input_tensor_type, lstm_cell.weight_tensor_index,
                       std::array<int32_t, 2>({slice_start, 0}),
                       std::array<int32_t, 2>({hidden_size, input_size})));
  const int32_t output_tensor_index_of_weight = SerializeSubGraphMatmulAdd(
      output_shape, input_tensor_type, lstm_cell.input_tensor_index,
      weight_tensor_index, bias_tensor_index);

  // hiddenState * recurrentWeight + recurrentBias.
  std::optional<int32_t> recurrent_bias_tensor_index;
  if (lstm_cell.recurrent_bias_tensor_index) {
    recurrent_bias_tensor_index = SerializeTemporaryTensor(
        std::array<int32_t, 1>({hidden_size}), input_tensor_type);
    ASSIGN_OR_RETURN(
        OperatorOffset operator_offset,
        SerializeSliceOperation(*lstm_cell.recurrent_bias_tensor_index,
                                *recurrent_bias_tensor_index,
                                std::array<int32_t, 1>({slice_start}),
                                std::array<int32_t, 1>({hidden_size})));
    operators_.emplace_back(operator_offset);
  }
  ASSIGN_OR_RETURN(
      const int32_t recurrent_weight_tensor_index,
      SerializeSubGraphSliceTranspose(
          input_tensor_type, lstm_cell.recurrent_weight_tensor_index,
          std::array<int32_t, 2>({slice_start, 0}),
          std::array<int32_t, 2>({hidden_size, hidden_size})));
  int32_t output_tensor_index_of_recurrent_weight = SerializeSubGraphMatmulAdd(
      output_shape, input_tensor_type, lstm_cell.hidden_state_tensor_index,
      recurrent_weight_tensor_index, recurrent_bias_tensor_index);

  // Add the result of the above two expressions (element-wise multiplication
  // between the input / hiddenState and the respective weights / recurrent
  // weights).
  int32_t updated_state_tensor_index =
      SerializeTemporaryTensor(output_shape, input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_ADD, output_tensor_index_of_weight,
      output_tensor_index_of_recurrent_weight, updated_state_tensor_index));

  // mul(cellState, peepholeWeight) + updatedState.
  if (lstm_cell.peephole_weight_tensor_index && type != LstmGateType::kCell) {
    int32_t output_tensor_index_of_slice = SerializeTemporaryTensor(
        std::array<int32_t, 1>({hidden_size}), input_tensor_type);
    ASSIGN_OR_RETURN(
        OperatorOffset operator_offset,
        SerializeSliceOperation(*lstm_cell.peephole_weight_tensor_index,
                                output_tensor_index_of_slice,
                                std::array<int32_t, 1>({slice_start}),
                                std::array<int32_t, 1>({hidden_size})));
    operators_.emplace_back(operator_offset);

    const int32_t output_tensor_index_of_mul =
        SerializeTemporaryTensor(output_shape, input_tensor_type);
    operators_.emplace_back(SerializeBinaryOperation(
        ::tflite::BuiltinOperator_MUL, lstm_cell.cell_state_tensor_index,
        output_tensor_index_of_slice, output_tensor_index_of_mul));

    const int32_t output_tensor_index_of_add =
        SerializeTemporaryTensor(output_shape, input_tensor_type);
    operators_.emplace_back(SerializeBinaryOperation(
        ::tflite::BuiltinOperator_ADD, output_tensor_index_of_mul,
        updated_state_tensor_index, output_tensor_index_of_add));
    updated_state_tensor_index = output_tensor_index_of_add;
  }

  // Apply first activation for the input, forget and output gate, the second
  // activation for the cell gate.
  const int32_t output_tensor_index_of_gate =
      SerializeTemporaryTensor(output_shape, input_tensor_type);
  operators_.emplace_back(SerializeUnaryOperation(activation_code,
                                                  updated_state_tensor_index,
                                                  output_tensor_index_of_gate));

  return output_tensor_index_of_gate;
}

auto GraphBuilderTflite::SerializeLstmCellOperation(
    const LstmCellOperation& lstm_cell)
    -> base::expected<OperatorOffset, std::string> {
  // Compute the input gate.
  ASSIGN_OR_RETURN(const int32_t input_gate_tensor_index,
                   SerializeLstmGate(lstm_cell, LstmGateType::kInput));

  // Compute the forget gate.
  ASSIGN_OR_RETURN(const int32_t forgat_gate_tensor_index,
                   SerializeLstmGate(lstm_cell, LstmGateType::kForget));

  // Compute the cell gate.
  ASSIGN_OR_RETURN(const int32_t cell_gate_tensor_index,
                   SerializeLstmGate(lstm_cell, LstmGateType::kCell));

  // Compute the output gate.
  ASSIGN_OR_RETURN(const int32_t output_gate_tensor_index,
                   SerializeLstmGate(lstm_cell, LstmGateType::kOutput));

  const std::array<int32_t, 2> output_shape = {lstm_cell.input_dimensions[0],
                                               lstm_cell.hidden_size};
  // Compute add(mul(forgetGete, cellState), mul(cellGate, inputGate).
  const int32_t forget_cell_gete_tensor_index =
      SerializeTemporaryTensor(output_shape, lstm_cell.input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_MUL, forgat_gate_tensor_index,
      lstm_cell.cell_state_tensor_index, forget_cell_gete_tensor_index));
  const int32_t input_cell_gete_tensor_index =
      SerializeTemporaryTensor(output_shape, lstm_cell.input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_MUL, cell_gate_tensor_index,
      input_gate_tensor_index, input_cell_gete_tensor_index));
  CHECK_EQ(lstm_cell.output_tensor_indices.size(), 2u);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_ADD, forget_cell_gete_tensor_index,
      input_cell_gete_tensor_index, lstm_cell.output_tensor_indices[1]));

  // Compute the new hidden state with adding the output gate and the new cell
  // state.
  const int32_t activation_tensor_index =
      SerializeTemporaryTensor(output_shape, lstm_cell.input_tensor_type);
  CHECK_EQ(lstm_cell.activations.size(), 3u);
  operators_.emplace_back(SerializeUnaryOperation(
      GetRecurrentNetworkActivation(lstm_cell.activations[2]),
      lstm_cell.output_tensor_indices[1], activation_tensor_index));
  return SerializeBinaryOperation(
      ::tflite::BuiltinOperator_MUL, output_gate_tensor_index,
      activation_tensor_index, lstm_cell.output_tensor_indices[0]);
}

// Serialize a sub graph (slice appending squeeze operation) for gru.
//
//     [input]
//        |
//      slice
//        |
//     squeeze
//        |
//     [output]
auto GraphBuilderTflite::SerializeSubGraphSliceSqueeze(
    ::tflite::TensorType input_tensor_type,
    int32_t input_tensor_index,
    base::span<const int32_t> slice_starts,
    base::span<const int32_t> slice_sizes,
    int32_t squeeze_axis) -> base::expected<int32_t, std::string> {
  CHECK_EQ(input_tensor_type, ::tflite::TensorType_FLOAT32);
  // The output is a tensor the same as the specified slice sizes.
  const int32_t output_tensor_index_of_slice =
      SerializeTemporaryTensor(slice_sizes, input_tensor_type);
  ASSIGN_OR_RETURN(
      OperatorOffset operator_offset,
      SerializeSliceOperation(input_tensor_index, output_tensor_index_of_slice,
                              slice_starts, slice_sizes));
  operators_.emplace_back(operator_offset);

  base::FixedArray<int32_t> squeeze_output_shape(slice_sizes.size());
  for (size_t i = 0; i < slice_sizes.size(); ++i) {
    if (base::checked_cast<size_t>(squeeze_axis) != i) {
      squeeze_output_shape[i] = slice_sizes[i];
    }
  }
  const int32_t output_tensor_index =
      SerializeTemporaryTensor(squeeze_output_shape, input_tensor_type);
  flatbuffers::Offset<void> builtin_options =
      ::tflite::CreateSqueezeOptions(
          builder_, builder_.CreateVector<int32_t>({squeeze_axis}))
          .Union();
  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_SQUEEZE);
  const std::array<int32_t, 1> op_inputs = {output_tensor_index_of_slice};
  const std::array<int32_t, 1> op_outputs = {output_tensor_index};
  operators_.emplace_back(::tflite::CreateOperator(
      builder_, operator_code_index, builder_.CreateVector<int32_t>(op_inputs),
      builder_.CreateVector<int32_t>(op_outputs),
      ::tflite::BuiltinOptions_SqueezeOptions, builtin_options));

  return output_tensor_index;
}

// `RecurrentNetworkType` must be `mojom::Gru` or `mojom::Lstm`.
template <typename RecurrentNetworkType>
  requires(std::is_same_v<RecurrentNetworkType, mojom::Gru> ||
           std::is_same_v<RecurrentNetworkType, mojom::Lstm>)
auto GraphBuilderTflite::SerializeRecurrentNetwork(
    const RecurrentNetworkType& recurrent_network)
    -> base::expected<OperatorOffset, std::string> {
  const OperandDataType input_data_type =
      GetOperand(recurrent_network.input_operand_id).descriptor.data_type();
  if constexpr (std::is_same_v<RecurrentNetworkType, mojom::Lstm>) {
    CHECK(context_properties_.data_type_limits.lstm_input.Has(input_data_type));
  } else /* `RecurrentNetworkType` is  `mojom::Gru` */ {
    CHECK(context_properties_.data_type_limits.gru_input.Has(input_data_type));
  }

  ASSIGN_OR_RETURN(
      const TensorInfo& input_tensor_info,
      SerializeInputTensorInfo(recurrent_network.input_operand_id));
  CHECK_EQ(input_tensor_info.dimensions.size(), 3u);
  const ::tflite::TensorType input_tensor_type = input_tensor_info.data_type;
  const auto checked_hidden_size =
      base::MakeCheckedNum<int32_t>(recurrent_network.hidden_size);
  if (!checked_hidden_size.IsValid()) {
    return base::unexpected("The hidden size is too large.");
  }
  const auto checked_steps =
      base::MakeCheckedNum<int32_t>(recurrent_network.steps);
  if (!checked_steps.IsValid()) {
    return base::unexpected("The steps size is too large.");
  }
  const int32_t num_directions =
      recurrent_network.direction == mojom::RecurrentNetworkDirection::kBoth
          ? 2
          : 1;
  const int32_t batch_size = input_tensor_info.dimensions[1];
  const int32_t input_size = input_tensor_info.dimensions[2];
  const int32_t hidden_size = checked_hidden_size.ValueOrDie();
  const int32_t recurrent_steps = checked_steps.ValueOrDie();

  const std::array<int32_t, 3> initial_hidden_cell_state_shape = {
      num_directions, batch_size, hidden_size};
  ASSIGN_OR_RETURN(int32_t hidden_state_tensor_index,
                   GetInitialHiddenAndCellState(
                       recurrent_network.initial_hidden_state_operand_id,
                       initial_hidden_cell_state_shape));
  // The cell state tensor index only be used by lstm.
  int32_t lstm_cell_state_tensor_index;
  if constexpr (std::is_same<RecurrentNetworkType, mojom::Lstm>::value) {
    ASSIGN_OR_RETURN(lstm_cell_state_tensor_index,
                     GetInitialHiddenAndCellState(
                         recurrent_network.initial_cell_state_operand_id,
                         initial_hidden_cell_state_shape));
  }

  base::FixedArray<int32_t> cell_weight_tensor_indices(num_directions);
  base::FixedArray<int32_t> cell_recurrent_weight_tensor_indices(
      num_directions);
  std::vector<int32_t> cell_bias_tensor_indices;
  cell_bias_tensor_indices.reserve(num_directions);
  std::vector<int32_t> cell_recurrent_bias_tensor_indices;
  cell_recurrent_bias_tensor_indices.reserve(num_directions);
  ASSIGN_OR_RETURN(
      const TensorInfo& weight_tensor_info,
      SerializeInputTensorInfo(recurrent_network.weight_operand_id));
  ASSIGN_OR_RETURN(
      const TensorInfo& recurrent_weight_tensor_info,
      SerializeInputTensorInfo(recurrent_network.recurrent_weight_operand_id));
  std::optional<TensorInfo> bias_tensor_info;
  if (recurrent_network.bias_operand_id) {
    ASSIGN_OR_RETURN(bias_tensor_info, SerializeInputTensorInfo(
                                           *recurrent_network.bias_operand_id));
  }
  std::optional<TensorInfo> recurrent_bias_tensor_info;
  if (recurrent_network.recurrent_bias_operand_id) {
    ASSIGN_OR_RETURN(
        recurrent_bias_tensor_info,
        SerializeInputTensorInfo(*recurrent_network.recurrent_bias_operand_id));
  }
  // The cell peephole weight tensor indices only be used by lstm.
  std::optional<TensorInfo> lstm_peephole_weight_tensor_info;
  std::vector<int32_t> lstm_cell_peephole_weight_tensor_indices;
  if constexpr (std::is_same<RecurrentNetworkType, mojom::Lstm>::value) {
    if (recurrent_network.peephole_weight_operand_id) {
      ASSIGN_OR_RETURN(lstm_peephole_weight_tensor_info,
                       SerializeInputTensorInfo(
                           *recurrent_network.peephole_weight_operand_id));
      lstm_cell_peephole_weight_tensor_indices.reserve(num_directions);
    }
  }
  for (int32_t slot = 0; slot < num_directions; ++slot) {
    int32_t slice_height;
    if constexpr (std::is_same<RecurrentNetworkType, mojom::Gru>::value) {
      slice_height = 3 * hidden_size;
    } else /* `RecurrentNetworkType` is `mojom::Lstm` */ {
      slice_height = 4 * hidden_size;
    }
    ASSIGN_OR_RETURN(const int32_t cell_weight_tensor_index,
                     SerializeSubGraphSliceSqueeze(
                         input_tensor_type, weight_tensor_info.index,
                         /*slice_starts=*/std::array<int32_t, 3>({slot, 0, 0}),
                         /*slice_sizes=*/
                         std::array<int32_t, 3>({1, slice_height, input_size}),
                         /*squeeze_axis=*/0));
    cell_weight_tensor_indices[slot] = cell_weight_tensor_index;

    ASSIGN_OR_RETURN(const int32_t cell_recurrent_weight_tensor_index,
                     SerializeSubGraphSliceSqueeze(
                         input_tensor_type, recurrent_weight_tensor_info.index,
                         /*slice_starts=*/std::array<int32_t, 3>({slot, 0, 0}),
                         /*slice_sizes=*/
                         std::array<int32_t, 3>({1, slice_height, hidden_size}),
                         /*squeeze_axis=*/0));
    cell_recurrent_weight_tensor_indices[slot] =
        cell_recurrent_weight_tensor_index;

    if (recurrent_network.bias_operand_id) {
      ASSIGN_OR_RETURN(const int32_t cell_bias_tensor_index,
                       SerializeSubGraphSliceSqueeze(
                           input_tensor_type, bias_tensor_info->index,
                           /*slice_starts=*/std::array<int32_t, 2>({slot, 0}),
                           /*slice_sizes=*/
                           std::array<int32_t, 2>({1, slice_height}),
                           /*squeeze_axis=*/0));
      cell_bias_tensor_indices.push_back(cell_bias_tensor_index);
    }

    if (recurrent_network.recurrent_bias_operand_id) {
      ASSIGN_OR_RETURN(const int32_t cell_recurrent_bias_tensor_index,
                       SerializeSubGraphSliceSqueeze(
                           input_tensor_type, recurrent_bias_tensor_info->index,
                           /*slice_starts=*/std::array<int32_t, 2>({slot, 0}),
                           /*slice_sizes=*/
                           std::array<int32_t, 2>({1, slice_height}),
                           /*squeeze_axis=*/0));
      cell_recurrent_bias_tensor_indices.push_back(
          cell_recurrent_bias_tensor_index);
    }

    if constexpr (std::is_same<RecurrentNetworkType, mojom::Lstm>::value) {
      if (recurrent_network.peephole_weight_operand_id) {
        ASSIGN_OR_RETURN(
            const int32_t peephole_weight_tensor_index,
            SerializeSubGraphSliceSqueeze(
                input_tensor_type, lstm_peephole_weight_tensor_info->index,
                /*slice_starts=*/std::array<int32_t, 2>({slot, 0}),
                /*slice_sizes=*/
                std::array<int32_t, 2>({1, 3 * hidden_size}),
                /*squeeze_axis=*/0));
        lstm_cell_peephole_weight_tensor_indices.push_back(
            peephole_weight_tensor_index);
      }
    }
  }

  std::optional<int32_t> sequence_tensor_index;
  for (int32_t step = 0; step < recurrent_steps; ++step) {
    base::FixedArray<int32_t> current_hidden_tensor_indices(num_directions);
    std::vector<int32_t> lstm_current_cell_tensor_indices;
    if constexpr (std::is_same<RecurrentNetworkType, mojom::Lstm>::value) {
      lstm_current_cell_tensor_indices.reserve(num_directions);
    }
    for (int32_t slot = 0; slot < num_directions; ++slot) {
      ASSIGN_OR_RETURN(
          const int32_t cell_hidden_tensor_index,
          SerializeSubGraphSliceSqueeze(
              input_tensor_type, hidden_state_tensor_index,
              /*slice_starts=*/std::array<int32_t, 3>({slot, 0, 0}),
              /*slice_sizes=*/
              std::array<int32_t, 3>({1, batch_size, hidden_size}),
              /*squeeze_axis=*/0));
      current_hidden_tensor_indices[slot] = cell_hidden_tensor_index;

      if constexpr (std::is_same<RecurrentNetworkType, mojom::Lstm>::value) {
        ASSIGN_OR_RETURN(
            const int32_t lstm_cell_tensor_index,
            SerializeSubGraphSliceSqueeze(
                input_tensor_type, lstm_cell_state_tensor_index,
                /*slice_starts=*/std::array<int32_t, 3>({slot, 0, 0}),
                /*slice_sizes=*/
                std::array<int32_t, 3>({1, batch_size, hidden_size}),
                /*squeeze_axis=*/0));
        lstm_current_cell_tensor_indices.push_back(lstm_cell_tensor_index);
      }
    }

    std::optional<int32_t> next_hidden_tensor_index;
    std::optional<int32_t> lstm_next_cell_tensor_index;
    for (int32_t slot = 0; slot < num_directions; ++slot) {
      const int32_t slice_start =
          (slot == 1 || recurrent_network.direction ==
                            mojom::RecurrentNetworkDirection::kBackward
               ? recurrent_steps - step - 1
               : step);
      ASSIGN_OR_RETURN(
          const int32_t cell_input_tensor_index,
          SerializeSubGraphSliceSqueeze(
              input_tensor_type, input_tensor_info.index,
              /*slice_starts=*/std::array<int32_t, 3>({slice_start, 0, 0}),
              /*slice_sizes=*/
              std::array<int32_t, 3>({1, batch_size, input_size}),
              /*squeeze_axis=*/0));

      const std::array<int32_t, 2> cell_input_dimensions = {batch_size,
                                                            input_size};
      const std::array<int32_t, 2> cell_output_dimensions = {batch_size,
                                                             hidden_size};
      const int32_t output_hidden_state_tensor_index =
          SerializeTemporaryTensor(cell_output_dimensions, input_tensor_type);
      // The output cell state tensor index is only used by lstm.
      int32_t lstm_output_cell_state_tensor_index;

      std::optional<int32_t> bias_tensor_index;
      if (recurrent_network.bias_operand_id) {
        bias_tensor_index = cell_bias_tensor_indices[slot];
      }
      std::optional<int32_t> recurrent_bias_tensor_index;
      if (recurrent_network.recurrent_bias_operand_id) {
        recurrent_bias_tensor_index = cell_recurrent_bias_tensor_indices[slot];
      }

      if constexpr (std::is_same<RecurrentNetworkType, mojom::Gru>::value) {
        GruCellOperation gru_cell_operation(
            cell_input_dimensions, input_tensor_type, cell_input_tensor_index,
            output_hidden_state_tensor_index, cell_weight_tensor_indices[slot],
            cell_recurrent_weight_tensor_indices[slot], bias_tensor_index,
            recurrent_bias_tensor_index, current_hidden_tensor_indices[slot],
            hidden_size, recurrent_network.reset_after,
            recurrent_network.layout, recurrent_network.activations);

        ASSIGN_OR_RETURN(OperatorOffset operator_offset,
                         SerializeGruCellOperation(gru_cell_operation));
        operators_.emplace_back(operator_offset);
      } else /* `RecurrentNetworkType` is `mojom::Lstm` */ {
        std::optional<int32_t> lstm_peephole_weight_tensor_index;
        if (recurrent_network.peephole_weight_operand_id) {
          lstm_peephole_weight_tensor_index =
              lstm_cell_peephole_weight_tensor_indices[slot];
        }
        lstm_output_cell_state_tensor_index =
            SerializeTemporaryTensor(cell_output_dimensions, input_tensor_type);
        const std::array<int32_t, 2> lstm_cell_output_tensor_indices = {
            output_hidden_state_tensor_index,
            lstm_output_cell_state_tensor_index};
        LstmCellOperation lstm_cell_operation(
            cell_input_dimensions, input_tensor_type, cell_input_tensor_index,
            lstm_cell_output_tensor_indices, cell_weight_tensor_indices[slot],
            cell_recurrent_weight_tensor_indices[slot], bias_tensor_index,
            recurrent_bias_tensor_index, current_hidden_tensor_indices[slot],
            hidden_size, lstm_current_cell_tensor_indices[slot],
            lstm_peephole_weight_tensor_index, recurrent_network.layout,
            recurrent_network.activations);

        ASSIGN_OR_RETURN(OperatorOffset operator_offset,
                         SerializeLstmCellOperation(lstm_cell_operation));
        operators_.emplace_back(operator_offset);
      }

      // Reshape the output hidden state tensor of gru / lstm cell to the 3-D
      // tensor [1, batchSize, hiddenSize].
      const std::array<int32_t, 3> new_shape = {1, batch_size, hidden_size};
      const std::array<int32_t, 3> concat_output_shape = {slot + 1, batch_size,
                                                          hidden_size};
      next_hidden_tensor_index = ReshapeHiddenAndCellState(
          input_tensor_type, output_hidden_state_tensor_index, new_shape,
          next_hidden_tensor_index, concat_output_shape);

      if constexpr (std::is_same<RecurrentNetworkType, mojom::Lstm>::value) {
        // Reshape the output cell state tensor of lstm cell to the 3-D tensor
        // [1, batchSize, hiddenSize].
        lstm_next_cell_tensor_index = ReshapeHiddenAndCellState(
            input_tensor_type, lstm_output_cell_state_tensor_index, new_shape,
            lstm_next_cell_tensor_index, concat_output_shape);
      }
    }

    // Update hidden state.
    hidden_state_tensor_index = *next_hidden_tensor_index;
    // Update cell state for Lstm.
    if constexpr (std::is_same<RecurrentNetworkType, mojom::Lstm>::value) {
      lstm_cell_state_tensor_index = *lstm_next_cell_tensor_index;
    }

    if (recurrent_network.return_sequence) {
      // Reshape the output tensor of gru cell to the 3-D tensor [1,
      // num_directions, batchSize, hiddenSize].
      const std::array<int32_t, 4> new_shape = {1, num_directions, batch_size,
                                                hidden_size};
      const int32_t reshape_cells_tensor_index =
          SerializeTemporaryTensor(new_shape, input_tensor_type);
      operators_.emplace_back(SerializeReshapeOperation(
          *next_hidden_tensor_index, reshape_cells_tensor_index, new_shape));

      if (sequence_tensor_index) {
        const std::array<int32_t, 4> concat_output_shape = {
            step + 1, num_directions, batch_size, hidden_size};
        const int32_t concat_tensor_index =
            SerializeTemporaryTensor(concat_output_shape, input_tensor_type);
        operators_.emplace_back(SerializeConcatOperation(
            std::array<int32_t, 2>(
                {*sequence_tensor_index, reshape_cells_tensor_index}),
            concat_tensor_index, 0));
        sequence_tensor_index = concat_tensor_index;
      } else {
        sequence_tensor_index = reshape_cells_tensor_index;
      }
    }
  }

  base::FixedArray<int32_t> output_tensor_indices(
      recurrent_network.output_operand_ids.size());
  for (size_t i = 0; i < recurrent_network.output_operand_ids.size(); ++i) {
    ASSIGN_OR_RETURN(
        const TensorInfo& output_tensor_info,
        SerializeOutputTensorInfo(recurrent_network.output_operand_ids[i]));
    output_tensor_indices[i] = output_tensor_info.index;
  }
  if (recurrent_network.return_sequence) {
    int32_t output_sequence_tensor_index;
    if constexpr (std::is_same<RecurrentNetworkType, mojom::Gru>::value) {
      CHECK_EQ(recurrent_network.output_operand_ids.size(), 2u);
      output_sequence_tensor_index = output_tensor_indices[1];
    } else /* `RecurrentNetworkType` is `mojom::Lstm` */ {
      CHECK_EQ(recurrent_network.output_operand_ids.size(), 3u);
      output_sequence_tensor_index = output_tensor_indices[2];
    }
    const std::array<int32_t, 4> new_shape = {recurrent_steps, num_directions,
                                              batch_size, hidden_size};
    operators_.emplace_back(SerializeReshapeOperation(
        *sequence_tensor_index, output_sequence_tensor_index, new_shape));
  }

  // Return cell state for Lstm.
  if constexpr (std::is_same<RecurrentNetworkType, mojom::Lstm>::value) {
    CHECK_GE(recurrent_network.output_operand_ids.size(), 2u);
    operators_.emplace_back(SerializeReshapeOperation(
        lstm_cell_state_tensor_index, output_tensor_indices[1],
        std::array<int32_t, 3>({num_directions, batch_size, hidden_size})));
  }

  return SerializeReshapeOperation(
      hidden_state_tensor_index, output_tensor_indices[0],
      std::array<int32_t, 3>({num_directions, batch_size, hidden_size}));
}

auto GraphBuilderTflite::SerializeHardSigmoid(
    const mojom::HardSigmoid& hard_sigmoid)
    -> base::expected<OperatorOffset, std::string> {
  // Emulate the hardSigmoid operation with function `y = max(0, min(1, alpha *
  // x + beta))` that is applied to the input tensor element-wise.
  //
  // The subexpression `alpha * x + beta` is considered a linear operation.
  CHECK(context_properties_.data_type_limits.hard_sigmoid_input.Has(
      GetOperand(hard_sigmoid.input_operand_id).descriptor.data_type()));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(hard_sigmoid.input_operand_id));
  const int32_t output_tensor_index_of_linear = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeLinearOperation(
      input_tensor_info.dimensions, input_tensor_info.data_type,
      input_tensor_info.index, output_tensor_index_of_linear,
      hard_sigmoid.alpha, hard_sigmoid.beta));

  // The expression `max(0, min(1, linear))` can be implemented with TFLite
  // RELU_0_TO_1 operator.
  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(hard_sigmoid.output_operand_id));
  return SerializeUnaryOperation(::tflite::BuiltinOperator_RELU_0_TO_1,
                                 output_tensor_index_of_linear,
                                 output_tensor_info.index);
}

auto GraphBuilderTflite::SerializeHardSwish(const mojom::HardSwish& hard_swish)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.hard_swish_input.Has(
      GetOperand(hard_swish.input_operand_id).descriptor.data_type()));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(hard_swish.input_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(hard_swish.output_operand_id));

  return SerializeUnaryOperation(::tflite::BuiltinOperator_HARD_SWISH,
                                 input_tensor_info.index,
                                 output_tensor_info.index);
}

std::tuple<int32_t, int32_t>
GraphBuilderTflite::ComputeMeanAndVarianceForNormalization(
    base::span<const int32_t> input_dimensions,
    ::tflite::TensorType input_tensor_type,
    int32_t input_tensor_index,
    base::span<const int32_t> spatial_dimensions) {
  // Get mean values with reduceMean over the spatial dimensions of the input.
  std::vector<int32_t> reduce_dimensions(input_dimensions.begin(),
                                         input_dimensions.end());
  for (auto dimension : spatial_dimensions) {
    reduce_dimensions[dimension] = 1;
  }
  const int32_t mean_tensor_index =
      SerializeTemporaryTensor(reduce_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeReduceOperation(
      ::tflite::BuiltinOperator_MEAN, input_tensor_index, mean_tensor_index,
      spatial_dimensions, /*keep_dimensions=*/true));

  // Get variance with expression `Variance = ReduceMean(Pow(Input - Mean, 2))`
  // over the spatial dimensions of the input.
  const int32_t output_tensor_index_of_sub =
      SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_SUB, input_tensor_index, mean_tensor_index,
      output_tensor_index_of_sub));
  const int32_t pow_constant_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{2.0},
      /*dimensions=*/{});
  const int32_t output_tensor_index_of_pow =
      SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_POW, output_tensor_index_of_sub,
      pow_constant_tensor_index, output_tensor_index_of_pow));
  const int32_t variance_tensor_index =
      SerializeTemporaryTensor(reduce_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeReduceOperation(
      ::tflite::BuiltinOperator_MEAN, output_tensor_index_of_pow,
      variance_tensor_index, spatial_dimensions, /*keep_dimensions=*/true));

  return std::make_tuple(mean_tensor_index, variance_tensor_index);
}

int32_t GraphBuilderTflite::TransposeAndReshapeLayerNormalizationScaleBias(
    base::span<const int32_t> input_dimensions,
    const TensorInfo& scale_or_bias_tensor_info,
    base::span<const uint32_t> axes) {
  std::vector<int32_t> compatible_shape(input_dimensions.size(), 1);
  for (auto axis : axes) {
    compatible_shape[axis] = input_dimensions[axis];
  }

  // The shape of the scale and bias tensors is determined by the axes selected
  // from the input tensor. These tensors need to be reshaped and/or transposed
  // so that they can be element-wise multiplied (for scale) or added (for bias)
  // during normalization.
  //
  // For example, if the input shape is [2, 1, 4, 3] and the axes are [3, 1, 2]
  // then the scale shape will be [3, 1, 4]. The scale shape needs to be
  // transposed to [1, 4, 3] and then reshaped to [1, 1, 4, 3].
  std::optional<int32_t> transpose_tensor_index;
  const std::vector<uint32_t> sorted_indices = GetIndexOfSortedValue(axes);
  if (!base::ranges::is_sorted(sorted_indices)) {
    transpose_tensor_index = InsertTransposeOperation(
        scale_or_bias_tensor_info.dimensions,
        scale_or_bias_tensor_info.data_type, scale_or_bias_tensor_info.index,
        sorted_indices);
  }

  const int32_t reshape_tensor_index = SerializeTemporaryTensor(
      compatible_shape, scale_or_bias_tensor_info.data_type);
  operators_.emplace_back(SerializeReshapeOperation(
      transpose_tensor_index.value_or(scale_or_bias_tensor_info.index),
      reshape_tensor_index, compatible_shape));

  return reshape_tensor_index;
}

auto GraphBuilderTflite::SerializeInstanceNormalization(
    const mojom::InstanceNormalization& instance_normalization)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.instance_normalization_input.Has(
      GetOperand(instance_normalization.input_operand_id)
          .descriptor.data_type()));
  ASSIGN_OR_RETURN(
      const TensorInfo& input_tensor_info,
      SerializeInputTensorInfo(instance_normalization.input_operand_id));
  CHECK_EQ(input_tensor_info.dimensions.size(), 4u);
  const ::tflite::TensorType input_tensor_type = input_tensor_info.data_type;
  std::array<int32_t, 2> spatial_dimensions;
  uint32_t channel_axis;
  switch (instance_normalization.layout) {
    case mojom::InputOperandLayout::kChannelsFirst: {
      spatial_dimensions = {2, 3};
      channel_axis = 1;
      break;
    }
    case mojom::InputOperandLayout::kChannelsLast:
      spatial_dimensions = {1, 2};
      channel_axis = 3;
      break;
  }
  std::vector<int32_t> new_shape(input_tensor_info.dimensions.size(), 1);
  new_shape[channel_axis] = input_tensor_info.dimensions[channel_axis];

  const int32_t input_tensor_index = input_tensor_info.index;
  const auto [mean_tensor_index, variance_tensor_index] =
      ComputeMeanAndVarianceForNormalization(
          input_tensor_info.dimensions, input_tensor_type, input_tensor_index,
          spatial_dimensions);

  // Reshape the 1-D tensor of the scale operand to the new shape if needed.
  std::optional<int32_t> reshape_scale_tensor_index;
  if (instance_normalization.scale_operand_id) {
    ASSIGN_OR_RETURN(
        const TensorInfo& scale_tensor_info,
        SerializeInputTensorInfo(*instance_normalization.scale_operand_id));
    CHECK_EQ(scale_tensor_info.dimensions.size(), 1u);
    reshape_scale_tensor_index =
        SerializeTemporaryTensor(new_shape, input_tensor_type);
    operators_.emplace_back(SerializeReshapeOperation(
        scale_tensor_info.index, *reshape_scale_tensor_index, new_shape));
  }

  // Reshape the 1-D tensor of the bias operand to the new shape if needed.
  std::optional<int32_t> reshape_bias_tensor_index;
  if (instance_normalization.bias_operand_id) {
    ASSIGN_OR_RETURN(
        const TensorInfo& bias_tensor_info,
        SerializeInputTensorInfo(*instance_normalization.bias_operand_id));
    CHECK_EQ(bias_tensor_info.dimensions.size(), 1u);
    reshape_bias_tensor_index =
        SerializeTemporaryTensor(new_shape, input_tensor_type);
    operators_.emplace_back(SerializeReshapeOperation(
        bias_tensor_info.index, *reshape_bias_tensor_index, new_shape));
  }

  ASSIGN_OR_RETURN(
      const TensorInfo& output_tensor_info,
      SerializeOutputTensorInfo(instance_normalization.output_operand_id));
  return SerializeNormalizationOperation(
      input_tensor_info.dimensions, input_tensor_type, input_tensor_index,
      output_tensor_info.index, mean_tensor_index, variance_tensor_index,
      instance_normalization.epsilon, reshape_scale_tensor_index,
      reshape_bias_tensor_index);
}

auto GraphBuilderTflite::SerializeLayerNormalization(
    const mojom::LayerNormalization& layer_normalization)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.layer_normalization_input.Has(
      GetOperand(layer_normalization.input_operand_id).descriptor.data_type()));
  ASSIGN_OR_RETURN(
      const TensorInfo& input_tensor_info,
      SerializeInputTensorInfo(layer_normalization.input_operand_id));
  std::optional<int32_t> scale_tensor_index;
  if (layer_normalization.scale_operand_id) {
    ASSIGN_OR_RETURN(
        const TensorInfo& scale_tensor_info,
        SerializeInputTensorInfo(*layer_normalization.scale_operand_id));
    scale_tensor_index = TransposeAndReshapeLayerNormalizationScaleBias(
        input_tensor_info.dimensions, scale_tensor_info,
        layer_normalization.axes);
  }
  std::optional<int32_t> bias_tensor_index;
  if (layer_normalization.bias_operand_id) {
    ASSIGN_OR_RETURN(
        const TensorInfo& bias_tensor_info,
        SerializeInputTensorInfo(*layer_normalization.bias_operand_id));
    bias_tensor_index = TransposeAndReshapeLayerNormalizationScaleBias(
        input_tensor_info.dimensions, bias_tensor_info,
        layer_normalization.axes);
  }

  // Get mean and variance values with reduceMean on the fly across all the
  // input features of each individual sample in the batch.
  ASSIGN_OR_RETURN(const std::vector<int32_t> signed_axes,
                   ToSignedDimensions(layer_normalization.axes));
  const int32_t input_tensor_index = input_tensor_info.index;
  const ::tflite::TensorType input_tensor_type = input_tensor_info.data_type;
  const auto [mean_tensor_index, variance_tensor_index] =
      ComputeMeanAndVarianceForNormalization(input_tensor_info.dimensions,
                                             input_tensor_type,
                                             input_tensor_index, signed_axes);

  ASSIGN_OR_RETURN(
      const TensorInfo& output_tensor_info,
      SerializeOutputTensorInfo(layer_normalization.output_operand_id));
  return SerializeNormalizationOperation(
      input_tensor_info.dimensions, input_tensor_type, input_tensor_index,
      output_tensor_info.index, mean_tensor_index, variance_tensor_index,
      layer_normalization.epsilon, scale_tensor_index, bias_tensor_index);
}

auto GraphBuilderTflite::SerializeLeakyRelu(const mojom::LeakyRelu& leaky_relu)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.leaky_relu_input.Has(
      GetOperand(leaky_relu.input_operand_id).descriptor.data_type()));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(leaky_relu.input_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(leaky_relu.output_operand_id));

  const auto leaky_rely_options =
      ::tflite::CreateLeakyReluOptions(builder_, leaky_relu.alpha);

  return SerializeUnaryOperation(
      ::tflite::BuiltinOperator_LEAKY_RELU, input_tensor_info.index,
      output_tensor_info.index, ::tflite::BuiltinOptions_LeakyReluOptions,
      leaky_rely_options.Union());
}

auto GraphBuilderTflite::SerializeLinear(const mojom::Linear& linear)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.linear_input.Has(
      GetOperand(linear.input_operand_id).descriptor.data_type()));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(linear.input_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(linear.output_operand_id));

  return SerializeLinearOperation(
      input_tensor_info.dimensions, input_tensor_info.data_type,
      input_tensor_info.index, output_tensor_info.index, linear.alpha,
      linear.beta);
}

auto GraphBuilderTflite::SerializeLogicalNot(
    const TensorInfo& input_tensor_info,
    const TensorInfo& output_tensor_info) -> OperatorOffset {
  // The data type of WebNN LogicalNot operation is uint8, but TFLite LogicalNot
  // builtin operation needs bool type, so a cast operation need to be inserted
  // before LogicalNot to convert uint8 to bool for input tensor and a cast
  // operation after LogicalNot to convert bool to uint8 for output tensor.
  //
  // Create two temporary tensors with bool type for TFLite LogicalNot.
  std::array<int32_t, 2> bool_tensor_indexes;
  for (auto& bool_tensor_index : bool_tensor_indexes) {
    bool_tensor_index = SerializeTemporaryTensor(input_tensor_info.dimensions,
                                                 ::tflite::TensorType_BOOL);
  }

  CHECK_EQ(input_tensor_info.data_type, ::tflite::TensorType_UINT8);
  operators_.emplace_back(SerializeCastOperation(
      input_tensor_info.index,
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
      /*input_tensor_type=*/::tflite::TensorType_BOOL, output_tensor_info.index,
      /*output_tensor_type=*/::tflite::TensorType_UINT8);
}

auto GraphBuilderTflite::SerializeLstmCell(const mojom::LstmCell& lstm_cell)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.lstm_cell_input.Has(
      GetOperand(lstm_cell.input_operand_id).descriptor.data_type()));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(lstm_cell.input_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& weight_tensor_info,
                   SerializeInputTensorInfo(lstm_cell.weight_operand_id));
  ASSIGN_OR_RETURN(
      const TensorInfo& recurrent_weight_tensor_info,
      SerializeInputTensorInfo(lstm_cell.recurrent_weight_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& hidden_state_tensor_info,
                   SerializeInputTensorInfo(lstm_cell.hidden_state_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& cell_state_tensor_info,
                   SerializeInputTensorInfo(lstm_cell.cell_state_operand_id));
  std::optional<int32_t> bias_tensor_index;
  if (lstm_cell.bias_operand_id) {
    ASSIGN_OR_RETURN(const TensorInfo& bias_tensor_info,
                     SerializeInputTensorInfo(*lstm_cell.bias_operand_id));
    bias_tensor_index = bias_tensor_info.index;
  }
  std::optional<int32_t> recurrent_bias_tensor_index;
  if (lstm_cell.recurrent_bias_operand_id) {
    ASSIGN_OR_RETURN(
        const TensorInfo& recurrent_bias_tensor_info,
        SerializeInputTensorInfo(*lstm_cell.recurrent_bias_operand_id));
    recurrent_bias_tensor_index = recurrent_bias_tensor_info.index;
  }
  std::optional<int32_t> peephole_weight_tensor_index;
  if (lstm_cell.peephole_weight_operand_id) {
    ASSIGN_OR_RETURN(
        const TensorInfo& peephole_weight_tensor_info,
        SerializeInputTensorInfo(*lstm_cell.peephole_weight_operand_id));
    peephole_weight_tensor_index = peephole_weight_tensor_info.index;
  }
  base::FixedArray<int32_t> output_tensor_indices(lstm_cell.output_operand_ids.size());
  for (size_t i = 0; i < lstm_cell.output_operand_ids.size(); ++i) {
    ASSIGN_OR_RETURN(
        const TensorInfo& output_tensor_info,
        SerializeOutputTensorInfo(lstm_cell.output_operand_ids[i]));
    output_tensor_indices[i] = output_tensor_info.index;
  }

  CHECK_EQ(input_tensor_info.dimensions.size(), 2u);
  const auto checked_hidden_size =
      base::MakeCheckedNum<int32_t>(lstm_cell.hidden_size);
  if (!checked_hidden_size.IsValid()) {
    return base::unexpected("The hidden size is too large.");
  }

  LstmCellOperation lstm_cell_operation(
      input_tensor_info.dimensions, input_tensor_info.data_type,
      input_tensor_info.index, output_tensor_indices, weight_tensor_info.index,
      recurrent_weight_tensor_info.index, bias_tensor_index,
      recurrent_bias_tensor_index, hidden_state_tensor_info.index,
      checked_hidden_size.ValueOrDie(), cell_state_tensor_info.index,
      peephole_weight_tensor_index, lstm_cell.layout, lstm_cell.activations);

  return SerializeLstmCellOperation(lstm_cell_operation);
}

auto GraphBuilderTflite::GetInitialHiddenAndCellState(
    std::optional<uint64_t> state_operand_id,
    base::span<const int32_t> state_dimensions)
    -> base::expected<int32_t, std::string> {
  int32_t state_tensor_index;
  if (state_operand_id) {
    ASSIGN_OR_RETURN(const TensorInfo& state_tensor_info,
                     SerializeInputTensorInfo(*state_operand_id));
    state_tensor_index = state_tensor_info.index;
  } else {
    CHECK_EQ(state_dimensions.size(), 3u);
    const std::vector<float> initial_hidden_state_value(
        std::accumulate(state_dimensions.begin(), state_dimensions.end(), 1,
                        std::multiplies()));
    state_tensor_index = SerializeTensorWithBuffer<float>(
        initial_hidden_state_value, state_dimensions);
  }
  return state_tensor_index;
}

int32_t GraphBuilderTflite::ReshapeHiddenAndCellState(
    ::tflite::TensorType input_tensor_type,
    int32_t input_tensor_index,
    base::span<const int32_t> new_shape,
    std::optional<int32_t> concat_input_tensor_index,
    base::span<const int32_t> concat_output_shape) {
  // Reshape the output hidden or cell state tensor of gru / lstm cell to the
  // 3-D tensor [1, batchSize, hiddenSize].
  const int32_t out_tensor_index_of_shape =
      SerializeTemporaryTensor(new_shape, input_tensor_type);
  operators_.emplace_back(SerializeReshapeOperation(
      input_tensor_index, out_tensor_index_of_shape, new_shape));
  if (concat_input_tensor_index) {
    const int32_t concat_output_tensor_index =
        SerializeTemporaryTensor(concat_output_shape, input_tensor_type);
    operators_.emplace_back(SerializeConcatOperation(
        std::array<int32_t, 2>(
            {*concat_input_tensor_index, out_tensor_index_of_shape}),
        concat_output_tensor_index, /*axis=*/0));
    return concat_output_tensor_index;
  }

  return out_tensor_index_of_shape;
}

auto GraphBuilderTflite::SerializeMatmul(const mojom::Matmul& matmul)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.matmul_input.Has(
      GetOperand(matmul.a_operand_id).descriptor.data_type()));
  ASSIGN_OR_RETURN(const TensorInfo& a_tensor_info,
                   SerializeInputTensorInfo(matmul.a_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& b_tensor_info,
                   SerializeInputTensorInfo(matmul.b_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(matmul.output_operand_id));

  return SerializeMatmulOperation(a_tensor_info.index, b_tensor_info.index,
                                  output_tensor_info.index);
}

auto GraphBuilderTflite::SerializePad(const mojom::Pad& pad)
    -> base::expected<OperatorOffset, std::string> {
  CHECK_EQ(pad.beginning_padding.size(), pad.ending_padding.size());
  CHECK(context_properties_.data_type_limits.pad_input.Has(
      GetOperand(pad.input_operand_id).descriptor.data_type()));

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

  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(pad.input_operand_id));
  std::vector<int32_t> op_inputs = {input_tensor_info.index, paddings_index};

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

  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(pad.output_operand_id));
  const uint32_t operator_code_index = GetOperatorCodeIndex(operator_code);
  const std::array<int32_t, 1> op_outputs = {output_tensor_info.index};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs),
                                  builtin_options_type, builtin_options);
}

auto GraphBuilderTflite::SerializePool2d(const mojom::Pool2d& pool2d)
    -> base::expected<OperatorOffset, std::string> {
  // The dilations are not supported in tflite schema.
  if (pool2d.dilations->height != 1 || pool2d.dilations->width != 1) {
    return base::unexpected("Pool2d in tflite doesn't support dilations.");
  }

  const mojom::Operand& input_operand = GetOperand(pool2d.input_operand_id);
  const auto& input_shape = input_operand.descriptor.shape();
  CHECK_EQ(input_shape.size(), 4u);
  const webnn::Size2d<uint32_t> input_size2d = {.height = input_shape[1],
                                                .width = input_shape[2]};
  webnn::Size2d<uint32_t> filter_size2d = {
      .height = pool2d.window_dimensions->height,
      .width = pool2d.window_dimensions->width};
  ASSIGN_OR_RETURN(
      TfLitePadding padding_mode,
      GetTfLitePaddingMode(*pool2d.padding, input_size2d, filter_size2d,
                           *pool2d.strides, *pool2d.dilations,
                           /*is_transposed_conv2d=*/false));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(pool2d.input_operand_id));
  // Insert a Pad operator before TfLite Pool2d if needed for explicit padding.
  std::optional<int32_t> explicit_pad_index;
  if (padding_mode.paddings) {
    ASSIGN_OR_RETURN(
        explicit_pad_index,
        InsertPadOperation(input_tensor_info, padding_mode.paddings.value()));
  }

  ::tflite::BuiltinOperator operator_code;
  switch (pool2d.kind) {
    case mojom::Pool2d::Kind::kAveragePool2d:
      CHECK(context_properties_.data_type_limits.average_pool2d_input.Has(
          input_operand.descriptor.data_type()));
      operator_code = ::tflite::BuiltinOperator_AVERAGE_POOL_2D;
      break;
    case mojom::Pool2d::Kind::kMaxPool2d:
      CHECK(context_properties_.data_type_limits.max_pool2d_input.Has(
          input_operand.descriptor.data_type()));
      operator_code = ::tflite::BuiltinOperator_MAX_POOL_2D;
      break;
    case mojom::Pool2d::Kind::kL2Pool2d:
      return base::unexpected("L2Pool2d is not supported in tflite.");
  }

  const auto pool_2d_options = ::tflite::CreatePool2DOptions(
      builder_, padding_mode.mode, pool2d.strides->width,
      pool2d.strides->height, filter_size2d.width, filter_size2d.height,
      ::tflite::ActivationFunctionType_NONE);

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(pool2d.output_operand_id));
  const uint32_t operator_code_index = GetOperatorCodeIndex(operator_code);
  const std::array<int32_t, 1> op_inputs = {explicit_pad_index
                                                ? explicit_pad_index.value()
                                                : input_tensor_info.index};
  const std::array<int32_t, 1> op_outputs = {output_tensor_info.index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index, builder_.CreateVector<int32_t>(op_inputs),
      builder_.CreateVector<int32_t>(op_outputs),
      ::tflite::BuiltinOptions_Pool2DOptions, pool_2d_options.Union());
}

auto GraphBuilderTflite::SerializePrelu(const mojom::Prelu& prelu)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.prelu_input.Has(
      GetOperand(prelu.input_operand_id).descriptor.data_type()));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(prelu.input_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& slope_tensor_info,
                   SerializeInputTensorInfo(prelu.slope_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(prelu.output_operand_id));

  // `ValidatePreluAndInferOutput` function has checked broadcastable shapes
  // between input and slope operand, but TFLite XNNPACK delegate doesn't
  // support to broadcast last dimension.
  // TODO(crbug.com/335517470): Support last dimension broadcastable.
  if (input_tensor_info.dimensions.size() != 0 &&
      slope_tensor_info.dimensions.size() != 0 &&
      input_tensor_info.dimensions.back() !=
          slope_tensor_info.dimensions.back()) {
    return base::unexpected(
        "The input and slope should have the same last dimension.");
  }

  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_PRELU);
  const std::array<int32_t, 2> op_inputs = {input_tensor_info.index,
                                            slope_tensor_info.index};
  const std::array<int32_t, 1> op_outputs = {output_tensor_info.index};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs));
}

auto GraphBuilderTflite::SerializeReciprocal(
    const TensorInfo& input_tensor_info,
    const TensorInfo& output_tensor_info)
    -> base::expected<OperatorOffset, std::string> {
  // Emulate the reciprocal operation whose calculation follows the expression
  // `1 / x`.
  CHECK_EQ(input_tensor_info.data_type, ::tflite::TensorType_FLOAT32);

  const int32_t constant_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{1.0},
      /*dimensions=*/{});

  return SerializeBinaryOperation(
      ::tflite::BuiltinOperator_DIV, constant_tensor_index,
      input_tensor_info.index, output_tensor_info.index);
}

auto GraphBuilderTflite::SerializeReduce(const mojom::Reduce& reduce)
    -> base::expected<OperatorOffset, std::string> {
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(reduce.input_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(reduce.output_operand_id));

  // Serialize the axes tensor to reduce input tensor.
  ASSIGN_OR_RETURN(const std::vector<int32_t> signed_axes,
                   ToSignedDimensions(reduce.axes));

  ::tflite::BuiltinOperator operator_code;
  int32_t input_tensor_index = input_tensor_info.index;
  const DataTypeLimits& data_type_limits = context_properties_.data_type_limits;
  const OperandDataType input_data_type =
      GetOperand(reduce.input_operand_id).descriptor.data_type();
  switch (reduce.kind) {
    case mojom::Reduce::Kind::kMax:
      CHECK(data_type_limits.reduce_max_input.Has(input_data_type));
      operator_code = ::tflite::BuiltinOperator_REDUCE_MAX;
      break;
    case mojom::Reduce::Kind::kMean:
      CHECK(data_type_limits.reduce_mean_input.Has(input_data_type));
      operator_code = ::tflite::BuiltinOperator_MEAN;
      break;
    case mojom::Reduce::Kind::kMin:
      CHECK(data_type_limits.reduce_min_input.Has(input_data_type));
      operator_code = ::tflite::BuiltinOperator_REDUCE_MIN;
      break;
    case mojom::Reduce::Kind::kProduct:
      CHECK(data_type_limits.reduce_product_input.Has(input_data_type));
      operator_code = ::tflite::BuiltinOperator_REDUCE_PROD;
      break;
    case mojom::Reduce::Kind::kSum:
      CHECK(data_type_limits.reduce_sum_input.Has(input_data_type));
      operator_code = ::tflite::BuiltinOperator_SUM;
      break;
    case mojom::Reduce::Kind::kLogSum:
      CHECK(data_type_limits.reduce_log_sum_input.Has(input_data_type));
      // The reduceLogSum can be emulated with appending log operation after
      // reduceSum.
      operator_code = ::tflite::BuiltinOperator_SUM;
      break;
    case mojom::Reduce::Kind::kLogSumExp: {
      // The reduceLogSumExp can be emulated with adding exp operation before
      // reduceSum and appending log operation after the reduceSum.
      CHECK(data_type_limits.reduce_log_sum_exp_input.Has(input_data_type));
      const int32_t output_tensor_index_of_exp = SerializeTemporaryTensor(
          input_tensor_info.dimensions, input_tensor_info.data_type);
      operators_.emplace_back(SerializeUnaryOperation(
          ::tflite::BuiltinOperator_EXP, input_tensor_index,
          output_tensor_index_of_exp));
      input_tensor_index = output_tensor_index_of_exp;
      // A log operation will be appended after the reduce sum.
      operator_code = ::tflite::BuiltinOperator_SUM;
      break;
    }
    case mojom::Reduce::Kind::kL2: {
      CHECK(data_type_limits.reduce_l2_input.Has(input_data_type));
      // The reduceL2 can be emulated with appending pow(x, 0.5) operation after
      // reduceSumSquare.
      const int32_t output_tensor_index_of_sum = SerializeTemporaryTensor(
          input_tensor_info.dimensions, input_tensor_info.data_type);
      ASSIGN_OR_RETURN(OperatorOffset operator_offset,
                       SerializeReduceSumSquare(input_tensor_info, signed_axes,
                                                reduce.keep_dimensions,
                                                output_tensor_index_of_sum));
      operators_.emplace_back(operator_offset);
      const int32_t pow_constant_tensor_index =
          SerializeTensorWithBuffer<float>(
              /*buffer=*/std::array<float, 1>{0.5},
              /*dimensions=*/{});
      return SerializeBinaryOperation(
          ::tflite::BuiltinOperator_POW, output_tensor_index_of_sum,
          pow_constant_tensor_index, output_tensor_info.index);
    }
    case mojom::Reduce::Kind::kSumSquare: {
      // The reduceSumSquare can be emulated with adding pow operation before
      // reduceSum.
      CHECK(data_type_limits.reduce_sum_square_input.Has(input_data_type));
      return SerializeReduceSumSquare(input_tensor_info, signed_axes,
                                      reduce.keep_dimensions,
                                      output_tensor_info.index);
    }
    case mojom::Reduce::Kind::kL1: {
      CHECK(data_type_limits.reduce_l1_input.Has(input_data_type));
      // The reduceL1 can be emulated with adding abs operation before
      // reduceSum.
      const int32_t output_tensor_index_of_abs = SerializeTemporaryTensor(
          input_tensor_info.dimensions, input_tensor_info.data_type);
      operators_.emplace_back(SerializeUnaryOperation(
          ::tflite::BuiltinOperator_ABS, input_tensor_index,
          output_tensor_index_of_abs));
      input_tensor_index = output_tensor_index_of_abs;
      operator_code = ::tflite::BuiltinOperator_SUM;
      break;
    }
  }

  if (reduce.kind == mojom::Reduce::Kind::kLogSum ||
      reduce.kind == mojom::Reduce::Kind::kLogSumExp) {
    const int32_t output_tensor_index_of_sum = SerializeTemporaryTensor(
        input_tensor_info.dimensions, input_tensor_info.data_type);
    operators_.emplace_back(SerializeReduceOperation(
        operator_code, input_tensor_index, output_tensor_index_of_sum,
        signed_axes, reduce.keep_dimensions));
    return SerializeUnaryOperation(::tflite::BuiltinOperator_LOG,
                                   output_tensor_index_of_sum,
                                   output_tensor_info.index);
  }

  return SerializeReduceOperation(operator_code, input_tensor_index,
                                  output_tensor_info.index, signed_axes,
                                  reduce.keep_dimensions);
}

auto GraphBuilderTflite::SerializeReduceSumSquare(
    const TensorInfo& input_tensor_info,
    base::span<const int32_t> axes,
    bool keep_dimensions,
    int32_t output_tensor_index)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(input_tensor_info.data_type == ::tflite::TensorType_FLOAT32 ||
        input_tensor_info.data_type == ::tflite::TensorType_INT32);
  // The reduceSumSquare can be emulated with adding pow operation before
  // reduceSum.
  int32_t pow_constant_tensor_index = -1;
  if (input_tensor_info.data_type == ::tflite::TensorType_FLOAT32) {
    pow_constant_tensor_index = SerializeTensorWithBuffer<float>(
        /*buffer=*/std::array<float, 1>{2.0},
        /*dimensions=*/{});
  } else if (input_tensor_info.data_type == ::tflite::TensorType_INT32) {
    pow_constant_tensor_index = SerializeTensorWithBuffer<int32_t>(
        /*buffer=*/std::array<int32_t, 1>{2},
        /*dimensions=*/{});
  }
  const int32_t output_tensor_index_of_pow = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_POW, input_tensor_info.index,
      pow_constant_tensor_index, output_tensor_index_of_pow));

  return SerializeReduceOperation(::tflite::BuiltinOperator_SUM,
                                  output_tensor_index_of_pow,
                                  output_tensor_index, axes, keep_dimensions);
}

auto GraphBuilderTflite::SerializeRelu(const mojom::Relu& relu)
    -> base::expected<OperatorOffset, std::string> {
  // TODO(crbug.com/354625677): Support 32-bit signed integer with
  // TFL::MaximumOp
  // https://www.tensorflow.org/mlir/tfl_ops#tflmaximum_tflmaximumop.
  CHECK(context_properties_.data_type_limits.relu_input.Has(
      GetOperand(relu.input_operand_id).descriptor.data_type()));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(relu.input_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(relu.output_operand_id));

  return SerializeUnaryOperation(
      ::tflite::BuiltinOperator::BuiltinOperator_RELU, input_tensor_info.index,
      output_tensor_info.index);
}

auto GraphBuilderTflite::SerializeResample2d(
    const mojom::Resample2d& resample2d)
    -> base::expected<OperatorOffset, std::string> {
  // TODO: crbug.com/329543543 - `resample2d.scales` is dropped on the floor.
  CHECK(context_properties_.data_type_limits.resample2d_input.Has(
      GetOperand(resample2d.input_operand_id).descriptor.data_type()));

  const std::array<uint32_t, 2> supported_axes = {1, 2};
  CHECK(base::ranges::equal(resample2d.axes, supported_axes));

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

  // Serialize the target sizes for the dimensions [OutputHeight,
  // OutputWidth].);
  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(resample2d.output_operand_id));
  int32_t output_height = output_tensor_info.dimensions[resample2d.axes[0]];
  int32_t output_width = output_tensor_info.dimensions[resample2d.axes[1]];

  const std::array<int32_t, 2> resize_data = {output_height, output_width};
  const std::array<int32_t, 1> resize_shape = {resize_data.size()};
  const int32_t resize_tensor_index =
      SerializeTensorWithBuffer<int32_t>(resize_data, resize_shape);

  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(resample2d.input_operand_id));
  const uint32_t operator_code_index = GetOperatorCodeIndex(operator_code);
  const std::array<int32_t, 2> op_inputs = {input_tensor_info.index,
                                            resize_tensor_index};
  const std::array<int32_t, 1> op_outputs = {output_tensor_info.index};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs),
                                  builtin_options_type, builtin_options);
}

auto GraphBuilderTflite::SerializeReshape(uint64_t input_operand_id,
                                          uint64_t output_operand_id)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.reshape_input.Has(
      GetOperand(input_operand_id).descriptor.data_type()));
  ASSIGN_OR_RETURN(
      const TensorInfo& input_tensor_info,
      SerializeInputTensorInfo(input_operand_id,
                               /*operation_supports_float16=*/true));
  ASSIGN_OR_RETURN(
      const TensorInfo& output_tensor_info,
      SerializeOutputTensorInfo(output_operand_id,
                                /*operation_supports_float16=*/true,
                                input_tensor_info.data_type));

  return SerializeReshapeOperation(input_tensor_info.index,
                                   output_tensor_info.index,
                                   output_tensor_info.dimensions);
}

auto GraphBuilderTflite::SerializeSigmoid(const mojom::Sigmoid& sigmoid)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.sigmoid_input.Has(
      GetOperand(sigmoid.input_operand_id).descriptor.data_type()));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(sigmoid.input_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(sigmoid.output_operand_id));

  return SerializeUnaryOperation(::tflite::BuiltinOperator_LOGISTIC,
                                 input_tensor_info.index,
                                 output_tensor_info.index);
}

auto GraphBuilderTflite::SerializeSlice(const mojom::Slice& slice)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.slice_input.Has(
      GetOperand(slice.input_operand_id).descriptor.data_type()));

  // The number of starts and sizes are the same as input rank that is verified
  // in ValidateSliceAndInferOutput() function.
  base::FixedArray<int32_t> slice_starts(slice.starts_and_sizes.size());
  base::FixedArray<int32_t> slice_sizes(slice.starts_and_sizes.size());
  for (size_t i = 0; i < slice.starts_and_sizes.size(); ++i) {
    const auto& start_and_size = slice.starts_and_sizes[i];
    auto checked_start = base::MakeCheckedNum<int32_t>(start_and_size->start);
    auto checked_size = base::MakeCheckedNum<int32_t>(start_and_size->size);
    if (!checked_start.IsValid() || !checked_size.IsValid()) {
      return base::unexpected("The start or size of slice is too large.");
    }
    slice_starts[i] = checked_start.ValueOrDie();
    slice_sizes[i] = checked_size.ValueOrDie();
  }

  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(slice.input_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(slice.output_operand_id));
  return SerializeSliceOperation(input_tensor_info.index,
                                 output_tensor_info.index, slice_starts,
                                 slice_sizes);
}

auto GraphBuilderTflite::SerializeSoftmax(const mojom::Softmax& softmax)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.softmax_input.Has(
      GetOperand(softmax.input_operand_id).descriptor.data_type()));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(softmax.input_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(softmax.output_operand_id));

  const size_t input_rank = input_tensor_info.dimensions.size();
  const auto softmax_options =
      ::tflite::CreateSoftmaxOptions(builder_, /*beta=*/1.0);
  if (softmax.axis == input_rank - 1) {
    // The axis is the last dimension, so the softmax operation can be directly
    // serialized.
    return SerializeUnaryOperation(
        ::tflite::BuiltinOperator_SOFTMAX, input_tensor_info.index,
        output_tensor_info.index, ::tflite::BuiltinOptions_SoftmaxOptions,
        softmax_options.Union());
  }
  // Transpose the input tensor to make the axis to be the last dimension.
  std::vector<uint32_t> permutation(input_rank);
  std::iota(permutation.begin(), permutation.end(), 0);
  std::swap(permutation[softmax.axis], permutation[input_rank - 1]);
  std::vector<int32_t> transpose_dimensions = input_tensor_info.dimensions;
  std::swap(transpose_dimensions[softmax.axis],
            transpose_dimensions[input_rank - 1]);

  const int32_t output_tensor_index_of_transpose = SerializeTemporaryTensor(
      transpose_dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeTransposeOperation(
      input_tensor_info.index, output_tensor_index_of_transpose, permutation));

  // Perform softmax.
  const int32_t output_tensor_index_of_softmax = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeUnaryOperation(
      ::tflite::BuiltinOperator_SOFTMAX, output_tensor_index_of_transpose,
      output_tensor_index_of_softmax, ::tflite::BuiltinOptions_SoftmaxOptions,
      softmax_options.Union()));

  // Transpose the last dimension back to the original axis.
  return SerializeTransposeOperation(output_tensor_index_of_softmax,
                                     output_tensor_info.index, permutation);
}

auto GraphBuilderTflite::SerializeSoftplus(const mojom::Softplus& softplus)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.softplus_input.Has(
      GetOperand(softplus.input_operand_id).descriptor.data_type()));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(softplus.input_operand_id));

  // Emulate the softplus operation whose calculation follows the expression
  // `ln(1 + exp(x))`.
  const int32_t output_tensor_index_of_exp = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeUnaryOperation(::tflite::BuiltinOperator_EXP,
                                                  input_tensor_info.index,
                                                  output_tensor_index_of_exp));

  // Add constant value `1` to the output tensor of element-wise exp operation.
  // TODO(crbug.com/339654398): Convert the 32-bit floating-point data to 16-bit
  // floating-point data with fp16_ieee_from_fp32_value function if some
  // delegates support 16-bit float inference.
  const int32_t constant_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{1},
      /*dimensions=*/{});
  const int32_t output_tensor_index_of_add = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_ADD, constant_tensor_index,
      output_tensor_index_of_exp, output_tensor_index_of_add));

  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(softplus.output_operand_id));
  return SerializeUnaryOperation(::tflite::BuiltinOperator_LOG,
                                 output_tensor_index_of_add,
                                 output_tensor_info.index);
}

auto GraphBuilderTflite::SerializeSoftsign(const mojom::Softsign& softsign)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.softsign_input.Has(
      GetOperand(softsign.input_operand_id).descriptor.data_type()));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(softsign.input_operand_id));

  // Emulate the softsign operation whose calculation follows the expression
  // `x / (1 + |x|)`.
  const int32_t output_tensor_index_of_abs = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeUnaryOperation(::tflite::BuiltinOperator_ABS,
                                                  input_tensor_info.index,
                                                  output_tensor_index_of_abs));

  // Add constant value `1` to the output tensor of element-wise abs operation.
  // TODO(crbug.com/339654398): Convert the 32-bit floating-point data to 16-bit
  // floating-point data with fp16_ieee_from_fp32_value function if some
  // delegates support 16-bit float inference.
  CHECK_EQ(input_tensor_info.data_type, ::tflite::TensorType_FLOAT32);
  const int32_t constant_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{1},
      /*dimensions=*/{});
  const int32_t output_tensor_index_of_add = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_ADD, constant_tensor_index,
      output_tensor_index_of_abs, output_tensor_index_of_add));

  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(softsign.output_operand_id));
  return SerializeBinaryOperation(
      ::tflite::BuiltinOperator_DIV, input_tensor_info.index,
      output_tensor_index_of_add, output_tensor_info.index);
}

auto GraphBuilderTflite::SerializeSplit(const mojom::Split& split)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.split_input.Has(
      GetOperand(split.input_operand_id).descriptor.data_type()));

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
  base::FixedArray<int32_t> split_sizes(outputs_size);
  base::FixedArray<int32_t> op_outputs(outputs_size);
  for (size_t i = 0; i < outputs_size; ++i) {
    ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                     SerializeOutputTensorInfo(split.output_operand_ids[i]));
    CHECK_LT(split.axis, output_tensor_info.dimensions.size());
    split_sizes[i] = output_tensor_info.dimensions[split.axis];
    op_outputs[i] = output_tensor_info.index;
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

  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(split.input_operand_id));
  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_SPLIT_V);
  // The order of inputs is input, split sizes tensor and then axis tensor as
  // the described https://www.tensorflow.org/mlir/tfl_ops#operands_130.
  const std::array<int32_t, 3> op_inputs = {
      input_tensor_info.index, sizes_tensor_index, axis_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index, builder_.CreateVector<int32_t>(op_inputs),
      builder_.CreateVector<int32_t>(op_outputs),
      ::tflite::BuiltinOptions_SplitVOptions, split_options.Union());
}

auto GraphBuilderTflite::SerializeTan(const TensorInfo& input_tensor_info,
                                      const TensorInfo& output_tensor_info)
    -> OperatorOffset {
  // The tangent operation defines the expression `opposite side / adjacent
  // side` to a right triangle as the described here
  // https://www.mathworks.com/help/matlab/ref/tan.html, it can be emulated with
  // `sin(x)/cos(x)` element-wise.
  const int32_t output_tensor_index_of_sin = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeUnaryOperation(::tflite::BuiltinOperator_SIN,
                                                  input_tensor_info.index,
                                                  output_tensor_index_of_sin));

  const int32_t output_tensor_index_of_cos = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeUnaryOperation(::tflite::BuiltinOperator_COS,
                                                  input_tensor_info.index,
                                                  output_tensor_index_of_cos));
  return SerializeBinaryOperation(
      ::tflite::BuiltinOperator_DIV, output_tensor_index_of_sin,
      output_tensor_index_of_cos, output_tensor_info.index);
}

auto GraphBuilderTflite::SerializeTanh(const mojom::Tanh& tanh)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.tanh_input.Has(
      GetOperand(tanh.input_operand_id).descriptor.data_type()));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(tanh.input_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(tanh.output_operand_id));

  return SerializeUnaryOperation(::tflite::BuiltinOperator_TANH,
                                 input_tensor_info.index,
                                 output_tensor_info.index);
}

auto GraphBuilderTflite::SerializeTile(const mojom::Tile& tile)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.tile_input.Has(
      GetOperand(tile.input_operand_id).descriptor.data_type()));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(tile.input_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(tile.output_operand_id));

  ASSIGN_OR_RETURN(const std::vector<int32_t> signed_repetitions,
                   ToSignedDimensions(tile.repetitions));
  const std::array<int32_t, 1> repetitions_tensor_shape = {
      base::checked_cast<int32_t>(signed_repetitions.size())};
  const int32_t repetitions_tensor_index = SerializeTensorWithBuffer<int32_t>(
      signed_repetitions, repetitions_tensor_shape);

  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_TILE);
  const std::array<int32_t, 2> op_inputs = {input_tensor_info.index,
                                            repetitions_tensor_index};
  const std::array<int32_t, 1> op_outputs = {output_tensor_info.index};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs));
}

auto GraphBuilderTflite::SerializeTriangular(
    const mojom::Triangular& triangular)
    -> base::expected<OperatorOffset, std::string> {
  OperandDataType data_type =
      GetOperand(triangular.input_operand_id).descriptor.data_type();
  CHECK(context_properties_.data_type_limits.triangular_input.Has(data_type));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(triangular.input_operand_id));

  auto input_rank = input_tensor_info.dimensions.size();
  CHECK_GE(input_rank, 2u);
  const int32_t height = input_tensor_info.dimensions[input_rank - 2];
  const int32_t width = input_tensor_info.dimensions[input_rank - 1];
  const std::array<int32_t, 2> mask_dimensions = {height, width};
  auto checked_diagonal = base::MakeCheckedNum<int32_t>(triangular.diagonal) +
                          std::max(height, width);
  if (!checked_diagonal.IsValid()) {
    return base::unexpected("The diagonal is too large.");
  }

  // Mask the input with an element-wise multiplication. For example:
  // [ 2, 3                           [0, 0,           [0, 0,
  //   4, 5,   element-wise mul        1, 0,      =>    4, 0,
  //   6, 7]                           1, 1]            6, 7]
  //
  // TODO(crbug.com/359729258): Save GPU memory consumption.
  int32_t mask_tensor_index;
  switch (data_type) {
    case OperandDataType::kFloat16:
      // The float16 data type has been casted to float32.
      [[fallthrough]];
    case OperandDataType::kFloat32:
      mask_tensor_index = SerializeTensorWithBuffer<float>(
          /*buffer=*/FillMaskTriangular<float>(
              mask_dimensions, triangular.upper, triangular.diagonal, 1.0),
          /*dimensions=*/mask_dimensions);
      break;
    case OperandDataType::kInt32:
      mask_tensor_index = SerializeTensorWithBuffer<int32_t>(
          /*buffer=*/FillMaskTriangular<int32_t>(
              mask_dimensions, triangular.upper, triangular.diagonal, 1),
          /*dimensions=*/mask_dimensions);
      break;
    case OperandDataType::kUint32:
      mask_tensor_index = SerializeTensorWithBuffer<uint32_t>(
          /*buffer=*/FillMaskTriangular<uint32_t>(
              mask_dimensions, triangular.upper, triangular.diagonal, 1u),
          /*dimensions=*/mask_dimensions);
      break;
    case OperandDataType::kInt64:
      mask_tensor_index = SerializeTensorWithBuffer<int64_t>(
          /*buffer=*/FillMaskTriangular<int64_t>(
              mask_dimensions, triangular.upper, triangular.diagonal, 1),
          /*dimensions=*/mask_dimensions);
      break;
    case OperandDataType::kInt8:
    case OperandDataType::kUint8:
    case OperandDataType::kUint64:
    case OperandDataType::kInt4:
    case OperandDataType::kUint4:
      NOTREACHED() << "This data type is not supported by triangular.";
  }

  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(triangular.output_operand_id));
  return SerializeBinaryOperation(::tflite::BuiltinOperator_MUL,
                                  input_tensor_info.index, mask_tensor_index,
                                  output_tensor_info.index);
}

auto GraphBuilderTflite::SerializeTranspose(const mojom::Transpose& transpose)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.transpose_input.Has(
      GetOperand(transpose.input_operand_id).descriptor.data_type()));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(transpose.input_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(transpose.output_operand_id));

  return SerializeTransposeOperation(
      input_tensor_info.index, output_tensor_info.index, transpose.permutation);
}

auto GraphBuilderTflite::SerializeWhere(const mojom::Where& where)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.where_condition.Has(
      GetOperand(where.condition_operand_id).descriptor.data_type()));
  CHECK(context_properties_.data_type_limits.where_value.Has(
      GetOperand(where.true_value_operand_id).descriptor.data_type()));
  CHECK(context_properties_.data_type_limits.where_value.Has(
      GetOperand(where.false_value_operand_id).descriptor.data_type()));
  ASSIGN_OR_RETURN(const TensorInfo& condition_tensor_info,
                   SerializeInputTensorInfo(where.condition_operand_id));
  // The data type of WebNN condition operand is uint8, but TFLite requires the
  // condition operand to be of type bool, so a cast operation need to be
  // inserted before the operation to convert uint8 to bool for the condition
  // operand.
  const int32_t condition_bool_tensor_index = SerializeTemporaryTensor(
      condition_tensor_info.dimensions, ::tflite::TensorType_BOOL);
  CHECK_EQ(condition_tensor_info.data_type, ::tflite::TensorType_UINT8);
  operators_.emplace_back(
      SerializeCastOperation(condition_tensor_info.index,
                             /*input_tensor_type=*/::tflite::TensorType_UINT8,
                             condition_bool_tensor_index,
                             /*output_tensor_type=*/::tflite::TensorType_BOOL));

  // TFLite SELECT_V2 builtin operator supports broadcastable shapes between
  // `condition`, `true` and `false` operand.
  ASSIGN_OR_RETURN(const TensorInfo& true_value_tensor_info,
                   SerializeInputTensorInfo(where.true_value_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& false_value_tensor_info,
                   SerializeInputTensorInfo(where.false_value_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& output_tensor_info,
                   SerializeOutputTensorInfo(where.output_operand_id));
  const uint32_t operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_SELECT_V2);
  const std::array<int32_t, 3> op_inputs = {condition_bool_tensor_index,
                                            true_value_tensor_info.index,
                                            false_value_tensor_info.index};
  const std::array<int32_t, 1> op_outputs = {output_tensor_info.index};
  return ::tflite::CreateOperator(builder_, operator_code_index,
                                  builder_.CreateVector<int32_t>(op_inputs),
                                  builder_.CreateVector<int32_t>(op_outputs));
}

}  // namespace webnn::tflite
