// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/graph_builder_tflite.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <concepts>
#include <functional>
#include <iterator>
#include <numeric>
#include <optional>
#include <vector>

#include "base/command_line.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/data_type_limits.h"
#include "services/webnn/public/cpp/graph_validation_utils.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/cpp/supported_tensors.h"
#include "services/webnn/public/cpp/webnn_errors.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_switches.h"
#include "services/webnn/webnn_utils.h"
#include "third_party/tflite/src/tensorflow/compiler/mlir/lite/schema/schema_generated.h"
#include "third_party/tflite/src/tensorflow/compiler/mlir/lite/tools/optimize/reduced_precision_metadata.h"

namespace webnn::tflite {

namespace {

// This feature flag allows us to compare performance between fused vs unfused
// quantized graphs.
BASE_FEATURE(kApplyQDQFusion,
             "ApplyQDQFusion",
             base::FEATURE_ENABLED_BY_DEFAULT);

// The version number of the Schema. Ideally all changes will be backward
// compatible. If that ever changes, we must ensure that version is the first
// entry in the new tflite root so that we can see that version is not 1.
#define TFLITE_SCHEMA_VERSION (3)

constexpr size_t kWeightsAlignment = 8;

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
template <>
struct TensorTypeMap<int8_t> {
  static constexpr ::tflite::TensorType value =
      ::tflite::TensorType::TensorType_INT8;
};
template <>
struct TensorTypeMap<uint8_t> {
  static constexpr ::tflite::TensorType value =
      ::tflite::TensorType::TensorType_UINT8;
};
template <>
struct TensorTypeMap<bool> {
  static constexpr ::tflite::TensorType value =
      ::tflite::TensorType::TensorType_BOOL;
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
    case OperandDataType::kUint4:
    default:
      NOTREACHED() << "Unsupported data type.";
  }
}

std::optional<::tflite::BuiltinOperator> GetClampOperatorCode(float min_value,
                                                              float max_value) {
  if (min_value == -1.0f && max_value == 1.0f) {
    return ::tflite::BuiltinOperator_RELU_N1_TO_1;
  } else if (min_value == 0.0f && max_value == 1.0f) {
    return ::tflite::BuiltinOperator_RELU_0_TO_1;
  } else if (min_value == 0.0f && max_value == 6.0f) {
    return ::tflite::BuiltinOperator_RELU6;
  } else if (min_value == 0.0f &&
             max_value == std::numeric_limits<float>::infinity()) {
    return ::tflite::BuiltinOperator_RELU;
  }

  return std::nullopt;
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
  std::ranges::sort(sorted_indices, std::ranges::less(),
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
base::FixedArray<DataType> FillMaskTriangular(
    base::span<const int32_t> dimensions,
    bool upper,
    int32_t diagonal,
    DataType mask) {
  CHECK_EQ(dimensions.size(), 2u);
  const int32_t height = dimensions[0];
  const int32_t width = dimensions[1];
  base::FixedArray<DataType> filled_matrix(height * width);
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

// Converts the index in a flat array into a tuple of coordinates that
// represents the corresponding position in a multi-dimensional array.
//
// The coordinates are calculated by iteratively dividing the flat index by the
// stride of each dimension. For example a shape (3, 2) array can map to the
// following coordinates:
//       index                        row, col
//     [[0,  1,]                  [[0, 0], [0, 1],
//      [2,  3,]      =>           [1, 0], [1, 1]
//      [4,  5,]]                  [2, 0], [2, 1]]
template <typename DataType>
  requires(std::is_same_v<DataType, int32_t> ||
           std::is_same_v<DataType, int64_t>)
base::expected<base::FixedArray<DataType>, std::string>
GetCoordinatesNDFromIndex(size_t flat_index,
                          base::span<const uint32_t> strides) {
  const size_t rank = strides.size();
  base::FixedArray<DataType> coordinates(rank);
  for (size_t i = 0; i < rank; ++i) {
    size_t coordinate = flat_index / strides[i];
    flat_index -= coordinate * strides[i];

    auto checked_coordinate = base::MakeCheckedNum<DataType>(coordinate);
    if (!checked_coordinate.IsValid()) {
      return base::unexpected("The coordinate is too large.");
    }
    coordinates[i] = checked_coordinate.ValueOrDie();
  }
  return coordinates;
}

}  // namespace

GraphBuilderTflite::Result::Result(
    flatbuffers::DetachedBuffer buffer,
    base::flat_map<std::string, int> input_name_to_index,
    base::flat_map<std::string, int> output_name_to_index,
    std::vector<uint8_t> buffer_data,
    bool graph_requires_fp32_precision)
    : buffer(std::move(buffer)),
      input_name_to_index(std::move(input_name_to_index)),
      output_name_to_index(std::move(output_name_to_index)),
      buffer_data(std::move(buffer_data)),
      graph_requires_fp32_precision(graph_requires_fp32_precision) {}

GraphBuilderTflite::Result::Result(Result&&) = default;

GraphBuilderTflite::Result& GraphBuilderTflite::Result::operator=(Result&&) =
    default;

GraphBuilderTflite::Result::~Result() = default;

// static
base::expected<GraphBuilderTflite::Result, std::string>
GraphBuilderTflite::CreateAndBuild(
    ContextProperties context_properties,
    const mojom::GraphInfo& graph_info,
    const base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>&
        constant_operands,
    const base::flat_map<OperandId, base::flat_set<OperationId>>&
        operand_to_dependent_operations,
    const base::flat_map<OperandId, OperationId>&
        operand_to_producing_operation) {
  GraphBuilderTflite builder(std::move(context_properties), graph_info,
                             constant_operands, operand_to_dependent_operations,
                             operand_to_producing_operation);

  bool graph_requires_fp32_precision = false;
  for (size_t i = 0; i < graph_info.operations.size(); ++i) {
    const mojom::OperationPtr& operation = graph_info.operations[i];
    if (operation->is_dequantize_linear()) {
      RETURN_IF_ERROR(builder.TryTraverseToSerializeQuantizedInput(
          *operation->get_dequantize_linear()));
    }
  }
  for (size_t i = 0; i < graph_info.operations.size(); ++i) {
    const mojom::OperationPtr& operation = graph_info.operations[i];
    if (!graph_requires_fp32_precision &&
        builder.RequiresFloat32Precision(*operation)) {
      graph_requires_fp32_precision = true;
    }
    RETURN_IF_ERROR(builder.SerializeOperation(*operation, i));
  }
  // Make sure to serialize `dequantizeLinear` if it is used as an output.
  for (OperandId operand_id : graph_info.output_operands) {
    auto it = builder.lazy_serialized_dequantize_operations_.find(operand_id);
    if (it != builder.lazy_serialized_dequantize_operations_.end() &&
        !it->second.second) {
      ASSIGN_OR_RETURN(OperatorOffset operator_offset,
                       builder.SerializeDequantizeLinear(
                           builder.GetDequantizeOp(operand_id)));

      builder.operators_.emplace_back(operator_offset);
    }
  }

  return builder.FinishAndTakeResult(graph_info.input_operands,
                                     graph_info.output_operands,
                                     graph_requires_fp32_precision);
}

// static
ContextProperties GraphBuilderTflite::GetContextProperties() {
  // TODO: crbug.com/345271830 - specify data types for all parameters.
  static constexpr SupportedDataTypes kInt4AndInts8Int32 = {
      OperandDataType::kInt4, OperandDataType::kUint8, OperandDataType::kInt8,
      OperandDataType::kInt32};
  static constexpr SupportedDataTypes kInts8AndInt32 = {
      OperandDataType::kUint8, OperandDataType::kInt8, OperandDataType::kInt32};
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

  // Limit to INT_MAX for security reasons (similar to PartitionAlloc).
  static constexpr uint64_t kTensorByteLengthLimit =
      std::numeric_limits<int32_t>::max();

  return ContextProperties(
      InputOperandLayout::kNhwc, Resample2DAxes::kChannelsLast,
      BatchNormalizationAxis::kAny,
      /*tensor_byte_length_limit=*/kTensorByteLengthLimit,
      {/*input=*/kAllDataTypesExceptUint4,
       /*constant=*/kAllDataTypesExceptUint4,
       /*arg_min_max_input=*/
       {kFloat16To32AndInt8To32AndUint8, SupportedRanks::NonScalarUpTo(8)},
       /*arg_min_max_output=*/DataTypeConstraint::kInt32To64,
       // BatchNormalization is emulated by sub, mul, add and div ops that only
       // support max rank up to 5.
       /*batch_normalization_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(5)},
       /*batch_normalization_mean=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(1)},
       /*cast_input=*/
       {kFloat16To32AndInts8To32AndInt64, SupportedRanks::UpTo(8)},
       // Polyfilled using MIN and MAX.
       /*clamp_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(5)},
       /*concat_inputs=*/{kAllDataTypesExceptUint4, SupportedRanks::UpTo(8)},
       // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/kernels/conv.cc
       /*conv2d_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(4)},
       /*conv2d_bias=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(8)},
       /*conv_transpose2d_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(4)},
       /*conv_transpose2d_bias=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(8)},
       /*cumulative_sum_input=*/
       {kFloat16To32AndInt32To64, SupportedRanks::NonScalarUpTo(8)},
       // DequantizeLinear may be emulated by sub and mul ops that only support
       // max rank up to 6.
       /*dequantize_linear_input=*/
       {kInt4AndInts8Int32, SupportedRanks::UpTo(6)},
       // TODO(crbug.com/376722724): Support float16 scale.
       /*dequantize_linear_scale=*/
       {DataTypeConstraint::kFloat32, SupportedRanks::UpTo(6)},
       /*dequantize_linear_zero_point=*/
       {kInt4AndInts8Int32, SupportedRanks::UpTo(6)},
       // Limited to 6D when broadcasting is required:
       // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/kernels/add.cc
       /*add_input=*/{kFloat16To32AndInt32To64, SupportedRanks::UpTo(6)},
       // Limited to 6D when broadcasting is required:
       // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/kernels/internal/reference/sub.h
       /*sub_input=*/{kFloat16To32AndInt32To64, SupportedRanks::UpTo(6)},
       // Limited to 6D when broadcasting is required:
       // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/kernels/mul.cc
       /*mul_input=*/
       {kFloat16To32AndInt32To64AndUint32, SupportedRanks::UpTo(6)},
       // Limited to 5D when broadcasting is required:
       // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/kernels/internal/reference/div.h
       /*div_input=*/{kFloat16To32AndInt32, SupportedRanks::UpTo(5)},
       // MAX and MIN are limited to 5D when broadcasting is required:
       // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/kernels/internal/reference/maximum_minimum.h
       /*max_input=*/{kFloat16To32AndInt32To64, SupportedRanks::UpTo(5)},
       /*min_input=*/{kFloat16To32AndInt32To64, SupportedRanks::UpTo(5)},
       // Limited to 4D when broadcasting is required:
       // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/kernels/pow.cc
       /*pow_input=*/{kFloat16To32AndInt32, SupportedRanks::UpTo(4)},
       // Comparisons are limited to 4D when broadcasting is required:
       // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/kernels/comparisons.cc
       /*equal_input=*/{kFloat16To32AndInt32To64, SupportedRanks::UpTo(4)},
       /*greater_input=*/{kFloat16To32AndInt32To64, SupportedRanks::UpTo(4)},
       /*greater_or_equal_input=*/
       {kFloat16To32AndInt32To64, SupportedRanks::UpTo(4)},
       /*lesser_input=*/{kFloat16To32AndInt32To64, SupportedRanks::UpTo(4)},
       /*lesser_or_equal_input=*/
       {kFloat16To32AndInt32To64, SupportedRanks::UpTo(4)},
       /*not_equal_input=*/
       {kFloat16To32AndInt32To64, SupportedRanks::UpTo(4)},
       // Logical binary operators are limited to 4D when broadcasting is
       // required:
       // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/kernels/logical.cc
       /*logical_and_input=*/
       {DataTypeConstraint::kUint8, SupportedRanks::UpTo(4)},
       /*logical_or_input=*/
       {DataTypeConstraint::kUint8, SupportedRanks::UpTo(4)},
       // Polyfilled using a cast to BOOL and NOT_EQUAL.
       /*logical_xor_input=*/
       {DataTypeConstraint::kUint8, SupportedRanks::UpTo(4)},
       /*logical_not_input=*/
       {DataTypeConstraint::kUint8, SupportedRanks::UpTo(8)},
       /*logical_output=*/DataTypeConstraint::kUint8,
       /*abs_input=*/{kFloat16To32AndInt32, SupportedRanks::UpTo(8)},
       /*ceil_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(8)},
       /*cos_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(8)},
       // Polyfilled using DIV and POW. Limited by DIV because POW is only
       // used with integer powers which works for any dimension.
       /*erf_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(5)},
       /*exp_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(8)},
       /*floor_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(8)},
       // Identity is emulated by reshape.
       /*identity_input=*/
       {kFloat16To32AndInt8To64AndUint8, SupportedRanks::UpTo(8)},
       /*log_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(8)},
       /*neg_input=*/{kFloat16To32AndInt32To64, SupportedRanks::UpTo(8)},
       // Polyfilled with DIV.
       /*reciprocal_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(5)},
       /*sign_input=*/{kFloat16To32AndInt32, SupportedRanks::UpTo(8)},
       /*sin_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(8)},
       /*sqrt_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(8)},
       // Polyfilled with SIN, COS and DIV however since it will never require
       // broadcasting this doesn't have those limitations.
       /*tan_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(8)},
       /*elu_input=*/{kFloat16To32AndInt8, SupportedRanks::UpTo(8)},
       /*expand_input=*/
       {kFloat16To32AndInts8To32AndInt64, SupportedRanks::UpTo(8)},
       /*gather_input=*/
       {kFloat16To32AndInt8To64AndUint8, SupportedRanks::UpTo(8)},
       /*gather_indices=*/
       {DataTypeConstraint::kGatherScatterIndicesSupportedDataTypes,
        SupportedRanks::UpTo(8)},
       // Scalar is not supported:
       // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/kernels/gather_nd.cc
       /*gather_elements_input=*/
       {kFloat16To32AndInt8To64AndUint8, SupportedRanks::NonScalarUpTo(8)},
       /*gather_elements_indices=*/
       {DataTypeConstraint::kGatherScatterIndicesSupportedDataTypes,
        SupportedRanks::NonScalarUpTo(8)},
       /*gather_nd_input=*/
       {kFloat16To32AndInt8To64AndUint8, SupportedRanks::NonScalarUpTo(8)},
       /*gather_nd_indices=*/
       {DataTypeConstraint::kGatherScatterIndicesSupportedDataTypes,
        SupportedRanks::NonScalarUpTo(8)},
       /*gelu_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(8)},
       /*gemm_a=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(2)},
       /*gemm_c=*/{DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(2)},
       /*gru_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(3)},
       /*gru_bias=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(2)},
       /*gru_cell_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(2)},
       /*gru_cell_bias=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(1)},
       // Polyfilled with ADD and MUL.
       /*hard_sigmoid_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(6)},
       // Polyfilled with ADD and MUL.
       /*hard_swish_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(6)},
       /*instance_normalization_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(4)},
       /*instance_normalization_scale=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(1)},
       // LayerNormalization is emulated by sub, mul, add and div(broadcated to
       // input rank before executing div operator) ops that only support max
       // rank up to 6.
       /*layer_normalization_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(6)},
       /*leaky_relu_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(8)},
       // Linear is emulated by mul and add.
       /*linear_input=*/{kFloat16To32AndInt32To64, SupportedRanks::UpTo(6)},
       /*lstm_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(3)},
       /*lstm_bias=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(2)},
       /*lstm_cell_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(2)},
       /*lstm_cell_bias=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(1)},
       // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/kernels/internal/reference/batch_matmul.h
       /*matmul_input=*/{kFloat16To32AndInt8, SupportedRanks::UpTo(5)},
       // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/kernels/internal/reference/pad.h
       /*pad_input=*/
       {kFloat16To32AndInt32To64AndUint8, SupportedRanks::UpTo(5)},
       // Pooling operators are limited to 4D.
       // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/kernels/internal/reference/pooling.h
       /*average_pool2d_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(4)},
       /*l2_pool2d_input=*/{},
       /*max_pool2d_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(4)},
       // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/kernels/internal/reference/prelu.h
       /*prelu_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(4)},
       // TODO(crbug.com/376722724): Support float16 input.
       // QuantizeLinear may be emulated by div and add ops that only support
       // max rank up to 5.
       /*quantize_linear_input=*/
       {DataTypeConstraint::kFloat32, SupportedRanks::UpTo(5)},
       // TFLite doesn't support int4 quantization that is tracked in
       // https://github.com/tensorflow/tensorflow/issues/80335
       /*quantize_linear_zero_point=*/
       {kInts8AndInt32, SupportedRanks::UpTo(5)},
       // ReduceL1 is emulated by abs and reduceSum.
       /*reduce_l1_input=*/{kFloat16To32AndInt32, SupportedRanks::UpTo(8)},
       // ReduceL2 is emulated by reduceSumSquare followed by sqrt.
       /*reduce_l2_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(8)},
       // ReduceLogSum is emulated by reduceSum and log.
       /*reduce_log_sum_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(8)},
       // ReduceLogSumExp is emulated by reduceSum and exp.
       /*reduce_log_sum_exp_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(8)},
       /*reduce_max_input=*/{kFloat16To32AndInt32To64, SupportedRanks::UpTo(8)},
       /*reduce_mean_input=*/
       {kFloat16To32AndInt32To64AndUint8, SupportedRanks::UpTo(8)},
       /*reduce_min_input=*/{kFloat16To32AndInt32To64, SupportedRanks::UpTo(8)},
       /*reduce_product_input=*/
       {kFloat16To32AndInt32To64, SupportedRanks::UpTo(8)},
       /*reduce_sum_input=*/{kFloat16To32AndInt32To64, SupportedRanks::UpTo(8)},
       // ReduceSumSquare is emulated by reduceSum and pow. pow(x, 2) works for
       // any dimension.
       /*reduce_sum_square_input=*/
       {kFloat16To32AndInt32, SupportedRanks::UpTo(8)},
       /*relu_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(8)},
       // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/kernels/internal/reference/resize_bilinear.h
       // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/kernels/internal/reference/resize_nearest_neighbor.h
       /*resample2d_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(4)},
       /*reshape_input=*/{kAllDataTypesExceptUint4, SupportedRanks::UpTo(8)},
       /*reverse_input=*/
       {kFloat16To32AndInt8To32AndUint8, SupportedRanks::UpTo(8)},
       // scatter_elements is emulated by scatter_nd, so keep their data types
       // and rank ranges aligned.
       /*scatter_elements_input=*/
       {kFloat16To32AndInt8To64AndUint32, SupportedRanks::NonScalarUpTo(8)},
       // The indices data type is the same as scatter_nd.
       /*scatter_elements_indices=*/
       {{OperandDataType::kInt32}, SupportedRanks::NonScalarUpTo(8)},
       // Scalar is not supported:
       // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/kernels/scatter_nd.cc
       /*scatter_nd_input=*/
       {kFloat16To32AndInt8To64AndUint32, SupportedRanks::NonScalarUpTo(8)},
       // The indices of tfl.scatter_nd only support int32.
       // https://www.tensorflow.org/mlir/tfl_ops#operands_117
       /*scatter_nd_indices=*/
       {{OperandDataType::kInt32}, SupportedRanks::NonScalarUpTo(8)},
       /*scatter_nd_updates=*/
       {kFloat16To32AndInt8To64AndUint32, SupportedRanks::NonScalarUpTo(8)},
       // Polyfilled with linear.
       /*sigmoid_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(6)},
       // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/kernels/internal/reference/slice.h
       /*slice_input=*/
       {kFloat16To32AndInts8To32AndInt64, SupportedRanks::UpTo(5)},
       /*softmax_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::NonScalarUpTo(8)},
       // Polyfilled with a broadcasted ADD.
       /*softplus_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(6)},
       // Polyfilled with a broadcasted ADD.
       /*softsign_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(6)},
       /*split_input=*/
       {kFloat16To32AndInt8To64AndUint8, SupportedRanks::UpTo(8)},
       /*tanh_input=*/
       {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(8)},
       /*tile_input=*/
       {kFloat16To32AndInt32To64AndUint8, SupportedRanks::UpTo(8)},
       // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/kernels/internal/reference/transpose.h
       /*transpose_input=*/
       {kFloat16To32AndInt8To64AndUint8, SupportedRanks::UpTo(6)},
       // Polyfilled with MUL, requires broadcasting.
       /*triangular_input=*/
       {kFloat16To32AndInt32To64AndUint32, SupportedRanks::UpTo(6)},
       /*where_condition=*/
       {DataTypeConstraint::kUint8, SupportedRanks::UpTo(5)},
       /*where_value=*/
       {kFloat16To32AndInt8To64AndUint32, SupportedRanks::UpTo(5)}});
}

GraphBuilderTflite::GraphBuilderTflite(
    ContextProperties context_properties,
    const mojom::GraphInfo& graph_info,
    const base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>&
        constant_operands,
    const base::flat_map<OperandId, base::flat_set<OperationId>>&
        operand_to_dependent_operations,
    const base::flat_map<OperandId, OperationId>&
        operand_to_producing_operation)
    : context_properties_(std::move(context_properties)),
      graph_info_(graph_info),
      constant_operands_(constant_operands),
      operand_to_dependent_operations_(operand_to_dependent_operations),
      operand_to_producing_operation_(operand_to_producing_operation) {
  // TFLite requires the first entry in FlatBuffer to be an empty buffer.
  buffers_.push_back(
      ::tflite::CreateBuffer(builder_, builder_.CreateVector({})));
  // TFLite requires that offsets into the weights file are greater than 1 and
  // we need anything we add to be aligned.
  std::fill_n(std::back_inserter(buffer_data_), kWeightsAlignment, 0);
}

GraphBuilderTflite::~GraphBuilderTflite() = default;

GraphBuilderTflite::TensorInfo::TensorInfo() = default;
GraphBuilderTflite::TensorInfo::TensorInfo(
    TensorIndex index,
    ::tflite::TensorType data_type,
    base::span<const int32_t> dimensions,
    std::optional<std::string> name,
    QuantizateParametersOffset quantize_params)
    : index(index),
      data_type(data_type),
      dimensions(dimensions.begin(), dimensions.end()),
      name(std::move(name)),
      quantize_params(quantize_params) {}
GraphBuilderTflite::TensorInfo::~TensorInfo() = default;

GraphBuilderTflite::TensorInfo::TensorInfo(const TensorInfo&) = default;
GraphBuilderTflite::TensorInfo& GraphBuilderTflite::TensorInfo::operator=(
    const TensorInfo&) = default;

GraphBuilderTflite::TensorInfo::TensorInfo(TensorInfo&& other) = default;
GraphBuilderTflite::TensorInfo& GraphBuilderTflite::TensorInfo::operator=(
    TensorInfo&& other) = default;

GraphBuilderTflite::TensorInfo GraphBuilderTflite::SerializeOperand(
    OperandId operand_id,
    QuantizateParametersOffset quantize_params,
    std::optional<::tflite::TensorType> override_tensor_type) {
  // The index of `tflite::Tensor` array, each `Operand` (input, constant,
  // output) will be converted and pushed back into the array, so it's increased
  // by one after each serialization in flat buffer.
  TensorIndex tensor_index = base::checked_cast<TensorIndex>(tensors_.size());
  CHECK_GE(tensor_index, 0);

  // The buffer index 0 represents input and output operand because there is no
  // data buffer associated.
  BufferIndex buffer_index = 0;
  const mojom::Operand& operand = GetOperand(operand_id);
  if (operand.kind == mojom::Operand::Kind::kConstant) {
    // Serialize buffer and return buffer index which starts from 1, it is
    // used to create the constant's tensor.
    auto it = constant_operands_->find(operand_id);
    CHECK(it != constant_operands_->end());
    buffer_index = SerializeBuffer(it->second->ByteSpan());
  }

  // Create `Tensor` with operand shape, the index of buffer and the name.
  // Operand dimensions have already been validated to be within int32_t limits
  // in `ValidateAndGetByteLength()`.
  const auto signed_operand_dimensions =
      ToSignedDimensions(operand.descriptor.shape());
  CHECK(signed_operand_dimensions.has_value());
  const flatbuffers::Offset<flatbuffers::Vector<int32_t>> dimensions =
      builder_.CreateVector<int32_t>(*signed_operand_dimensions);
  ::tflite::TensorType operand_type = override_tensor_type.value_or(
      OperandDataTypeToTFLite(operand.descriptor.data_type()));
  const StringOffset operand_name =
      operand.name.has_value() ? builder_.CreateString(*operand.name) : 0;
  tensors_.emplace_back(::tflite::CreateTensor(builder_, std::move(dimensions),
                                               operand_type, buffer_index,
                                               operand_name, quantize_params));
  TensorInfo tensor_info(tensor_index, operand_type, *signed_operand_dimensions,
                         operand.name, quantize_params);
  operand_to_tensor_info_map_.insert({operand_id, tensor_info});

  return tensor_info;
}

base::expected<GraphBuilderTflite::TensorInfo, std::string>
GraphBuilderTflite::SerializeInputTensorInfo(
    OperandId operand_id,
    QuantizateParametersOffset quantize_params,
    bool operation_supports_float16,
    bool fuse_dequantize) {
  auto dequantize_it = lazy_serialized_dequantize_operations_.find(operand_id);
  if (dequantize_it != lazy_serialized_dequantize_operations_.end()) {
    auto& [dequantize_op_index, serialized] = dequantize_it->second;
    const mojom::DequantizeLinear& dequantize_op =
        *graph_info_->operations[dequantize_op_index]->get_dequantize_linear();
    if (!fuse_dequantize && !serialized) {
      ASSIGN_OR_RETURN(OperatorOffset operator_offset,
                       SerializeDequantizeLinear(dequantize_op));
      operators_.emplace_back(operator_offset);
      serialized = true;
    }
    if (fuse_dequantize) {
      operand_id = dequantize_op.input_operand_id;
    }
    // The `operand_id` should already be serialized by
    // `SerializeDequantizeLinear` or `TrySerializeQuantizedInput`.
    auto it = operand_to_tensor_info_map_.find(operand_id);
    CHECK(it != operand_to_tensor_info_map_.end());
    return it->second;
  }
  auto it = operand_to_tensor_info_map_.find(operand_id);
  TensorInfo input_tensor_info =
      it == operand_to_tensor_info_map_.end()
          ? SerializeOperand(operand_id, quantize_params)
          : it->second;
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

GraphBuilderTflite::TensorInfo GraphBuilderTflite::SerializeOutputTensorInfo(
    OperandId operand_id,
    QuantizateParametersOffset quantize_params,
    bool operation_supports_float16,
    std::optional<::tflite::TensorType> override_tensor_type) {
  auto it = operand_to_tensor_info_map_.find(operand_id);
  if (it != operand_to_tensor_info_map_.end()) {
    return it->second;
  }
  const mojom::Operand& operand = *graph_info_->operands.at(operand_id.value());
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
  const TensorInfo output_tensor_info =
      SerializeOperand(operand_id, quantize_params, tensor_type);

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
    const TensorIndex temporary_tensor_index = SerializeTemporaryTensor(
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
    const mojom::Operation& op,
    OperationId operation_index) {
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
    case mojom::Operation::Tag::kCumulativeSum: {
      ASSIGN_OR_RETURN(operator_offset,
                       SerializeCumulativeSum(*op.get_cumulative_sum()));
      break;
    }
    case mojom::Operation::Tag::kDequantizeLinear: {
      auto operation = *op.get_dequantize_linear();
      // Don't serialize the dequantize right now until the dequantized output
      // is needed for a subsequent operation. During `SerializeInputTensorInfo`
      // for subsequent operations, it will check whether it needs to inject
      // a dequantize operation.
      if (base::FeatureList::IsEnabled(kApplyQDQFusion) &&
          TrySerializeQuantizedInput(operation, operation_index)) {
        return base::ok();
      }
      ASSIGN_OR_RETURN(operator_offset, SerializeDequantizeLinear(operation));
      break;
    }
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
    case mojom::Operation::Tag::kGatherElements: {
      ASSIGN_OR_RETURN(operator_offset,
                       SerializeGatherElements(*op.get_gather_elements()));
      break;
    }
    case mojom::Operation::Tag::kGatherNd: {
      ASSIGN_OR_RETURN(operator_offset, SerializeGatherND(*op.get_gather_nd()));
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
    case mojom::Operation::Tag::kQuantizeLinear: {
      if (quantize_ops_to_skip_.contains(operation_index)) {
        return base::ok();
      }
      ASSIGN_OR_RETURN(operator_offset,
                       SerializeQuantizeLinear(*op.get_quantize_linear()));
      break;
    }
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
      ASSIGN_OR_RETURN(operator_offset, SerializeReshape(*op.get_reshape()));
      break;
    }
    case mojom::Operation::Tag::kReverse: {
      ASSIGN_OR_RETURN(operator_offset, SerializeReverse(*op.get_reverse()));
      break;
    }
    case mojom::Operation::Tag::kScatterElements: {
      ASSIGN_OR_RETURN(operator_offset,
                       SerializeScatterElements(*op.get_scatter_elements()));
      break;
    }
    case mojom::Operation::Tag::kScatterNd: {
      ASSIGN_OR_RETURN(operator_offset,
                       SerializeScatterND(*op.get_scatter_nd()));
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
  }
  operators_.emplace_back(operator_offset);

  return base::ok();
}

bool GraphBuilderTflite::RequiresFloat32Precision(const mojom::Operation& op) {
  OperandId input_operand_id;

  // Only need to check the first input for operation with multiple inputs,
  // because they all require to be the same data type.
  switch (op.which()) {
    // Ignore `quantizeLinear` and `cast` from float32. A graph is considered a
    // fp16 graph if it casts input float32 to float16 and performs all
    // other operations in float16.
    // Ignore no-op `identity` operation.
    case mojom::Operation::Tag::kElementWiseUnary: {
      mojom::ElementWiseUnaryPtr& operation = op.get_element_wise_unary();
      if (operation->kind == mojom::ElementWiseUnary::Kind::kIdentity ||
          operation->kind == mojom::ElementWiseUnary::Kind::kCast) {
        return false;
      }
      input_operand_id = operation->input_operand_id;
      break;
    }
    case mojom::Operation::Tag::kQuantizeLinear:
      return false;
    // These operations just move data around and don't do arithmetic, so they
    // don't care about precision.
    case mojom::Operation::Tag::kConcat:
    case mojom::Operation::Tag::kExpand:
    case mojom::Operation::Tag::kGather:
    case mojom::Operation::Tag::kGatherElements:
    case mojom::Operation::Tag::kGatherNd:
    case mojom::Operation::Tag::kPad:
    case mojom::Operation::Tag::kReshape:
    case mojom::Operation::Tag::kReverse:
    case mojom::Operation::Tag::kScatterElements:
    case mojom::Operation::Tag::kScatterNd:
    case mojom::Operation::Tag::kSlice:
    case mojom::Operation::Tag::kSplit:
    case mojom::Operation::Tag::kTile:
    case mojom::Operation::Tag::kTranspose:
    case mojom::Operation::Tag::kTriangular:
    case mojom::Operation::Tag::kWhere:
      return false;
    case mojom::Operation::Tag::kArgMinMax:
      input_operand_id = op.get_arg_min_max()->input_operand_id;
      break;
    case mojom::Operation::Tag::kBatchNormalization:
      input_operand_id = op.get_batch_normalization()->input_operand_id;
      break;
    case mojom::Operation::Tag::kClamp:
      input_operand_id = op.get_clamp()->input_operand_id;
      break;
    case mojom::Operation::Tag::kConv2d:
      input_operand_id = op.get_conv2d()->input_operand_id;
      break;
    case mojom::Operation::Tag::kCumulativeSum:
      input_operand_id = op.get_cumulative_sum()->input_operand_id;
      break;
    case mojom::Operation::Tag::kDequantizeLinear:
      input_operand_id = op.get_dequantize_linear()->input_operand_id;
      break;
    case mojom::Operation::Tag::kElementWiseBinary:
      input_operand_id = op.get_element_wise_binary()->lhs_operand_id;
      break;
    case mojom::Operation::Tag::kElu:
      input_operand_id = op.get_elu()->input_operand_id;
      break;
    case mojom::Operation::Tag::kGelu:
      input_operand_id = op.get_gelu()->input_operand_id;
      break;
    case mojom::Operation::Tag::kGemm:
      input_operand_id = op.get_gemm()->a_operand_id;
      break;
    case mojom::Operation::Tag::kGru:
      input_operand_id = op.get_gru()->input_operand_id;
      break;
    case mojom::Operation::Tag::kGruCell:
      input_operand_id = op.get_gru_cell()->input_operand_id;
      break;
    case mojom::Operation::Tag::kHardSigmoid:
      input_operand_id = op.get_hard_sigmoid()->input_operand_id;
      break;
    case mojom::Operation::Tag::kHardSwish:
      input_operand_id = op.get_hard_swish()->input_operand_id;
      break;
    case mojom::Operation::Tag::kLayerNormalization:
      input_operand_id = op.get_layer_normalization()->input_operand_id;
      break;
    case mojom::Operation::Tag::kInstanceNormalization:
      input_operand_id = op.get_instance_normalization()->input_operand_id;
      break;
    case mojom::Operation::Tag::kLeakyRelu:
      input_operand_id = op.get_leaky_relu()->input_operand_id;
      break;
    case mojom::Operation::Tag::kLinear:
      input_operand_id = op.get_linear()->input_operand_id;
      break;
    case mojom::Operation::Tag::kLstm:
      input_operand_id = op.get_lstm()->input_operand_id;
      break;
    case mojom::Operation::Tag::kLstmCell:
      input_operand_id = op.get_lstm_cell()->input_operand_id;
      break;
    case mojom::Operation::Tag::kMatmul:
      input_operand_id = op.get_matmul()->a_operand_id;
      break;
    case mojom::Operation::Tag::kPool2d:
      input_operand_id = op.get_pool2d()->input_operand_id;
      break;
    case mojom::Operation::Tag::kPrelu:
      input_operand_id = op.get_prelu()->input_operand_id;
      break;
    case mojom::Operation::Tag::kReduce:
      input_operand_id = op.get_reduce()->input_operand_id;
      break;
    case mojom::Operation::Tag::kRelu:
      input_operand_id = op.get_relu()->input_operand_id;
      break;
    case mojom::Operation::Tag::kResample2d:
      input_operand_id = op.get_resample2d()->input_operand_id;
      break;
    case mojom::Operation::Tag::kSigmoid:
      input_operand_id = op.get_sigmoid()->input_operand_id;
      break;
    case mojom::Operation::Tag::kSoftmax:
      input_operand_id = op.get_softmax()->input_operand_id;
      break;
    case mojom::Operation::Tag::kSoftplus:
      input_operand_id = op.get_softplus()->input_operand_id;
      break;
    case mojom::Operation::Tag::kSoftsign:
      input_operand_id = op.get_softsign()->input_operand_id;
      break;
    case mojom::Operation::Tag::kTanh:
      input_operand_id = op.get_tanh()->input_operand_id;
      break;
  }
  return (GetOperand(input_operand_id).descriptor.data_type() ==
          OperandDataType::kFloat32);
}

std::optional<GraphBuilderTflite::TensorInfo>
GraphBuilderTflite::CanFuseQuantizeAndGetOutput(const mojom::Conv2d& conv2d) {
  // TODO(crbug.com/401281047): Construct a quantized empty bias tensor if not
  // provided.
  if (!IsDequantizeOutput(conv2d.input_operand_id) ||
      !IsDequantizeOutput(conv2d.filter_operand_id) ||
      !conv2d.bias_operand_id || !IsDequantizeOutput(*conv2d.bias_operand_id)) {
    return std::nullopt;
  }

  // TODO(crbug.com/401281047): Support quantization fusion for transposed
  // conv2d.
  if (conv2d.kind != mojom::Conv2d::Kind::kDirect) {
    return std::nullopt;
  }

  // Filter and input have to be dequantized from (u)int8 or (u)int16, WebNN
  // doesn't support (u)int16.
  const mojom::DequantizeLinear& input_dequantize =
      GetDequantizeOp(conv2d.input_operand_id);
  const mojom::DequantizeLinear& filter_dequantize =
      GetDequantizeOp(conv2d.filter_operand_id);
  const OperandDataType quantized_type =
      GetOperand(input_dequantize.input_operand_id).descriptor.data_type();
  if (!IsInts8AndScalarScale(input_dequantize) ||
      GetOperand(filter_dequantize.input_operand_id).descriptor.data_type() !=
          quantized_type) {
    return std::nullopt;
  }

  // Filter must have all-zero zero-points.
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/kernels/conv.cc;l=363;drc=e433dac46a0bb8ffa4b6e600d4d94751768392c0
  auto filter_zero_point_constant_it =
      constant_operands_->find(filter_dequantize.zero_point_operand_id);
  CHECK(filter_zero_point_constant_it != constant_operands_->end());
  for (uint8_t byte : filter_zero_point_constant_it->second->ByteSpan()) {
    if (byte != 0) {
      return std::nullopt;
    }
  }

  // Bias must be int32 and have all-zero zero-points.
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/kernels/conv.cc;l=384;drc=e433dac46a0bb8ffa4b6e600d4d94751768392c0
  const mojom::DequantizeLinear& bias_dequantize =
      GetDequantizeOp(*conv2d.bias_operand_id);
  if (GetOperand(bias_dequantize.input_operand_id).descriptor.data_type() !=
      OperandDataType::kInt32) {
    return std::nullopt;
  }
  auto bias_zero_point_constant_it =
      constant_operands_->find(bias_dequantize.zero_point_operand_id);
  CHECK(bias_zero_point_constant_it != constant_operands_->end());

  for (uint8_t byte : bias_zero_point_constant_it->second->ByteSpan()) {
    if (byte != 0) {
      return std::nullopt;
    }
  }

  std::optional<std::pair<OperationId, QuantizateParametersOffset>> next_op =
      IsNextOpQuantize(conv2d.output_operand_id, {quantized_type});
  if (!next_op) {
    return std::nullopt;
  }
  const mojom::QuantizeLinear& quantize_linear = GetQuantizeOp(next_op->first);
  // For XNNPack delegate, input and output scale have to be scaler, both filter
  // and bias scale can be either scalar or vector that matches the output
  // channel.
  base::span<const float> input_scale_values =
      GetConstantValue<float>(input_dequantize.scale_operand_id);
  base::span<const float> output_scale_values =
      GetConstantValue<float>(quantize_linear.scale_operand_id);
  base::span<const float> bias_scale_values =
      GetConstantValue<float>(bias_dequantize.scale_operand_id);
  base::span<const float> filter_scale_values =
      GetConstantValue<float>(filter_dequantize.scale_operand_id);
  if (output_scale_values.size() != 1) {
    return std::nullopt;
  }

  const uint32_t output_channels =
      GetOperand(conv2d.output_operand_id).descriptor.shape()[3];
  if ((bias_scale_values.size() != 1 &&
       bias_scale_values.size() != output_channels) ||
      (filter_scale_values.size() != 1 &&
       filter_scale_values.size() != output_channels)) {
    return std::nullopt;
  }

  // uint8 only allows scaler filter scale and bias scale:
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/kernels/kernel_util.cc;l=239;drc=2f4e6fd051b670e0e032cc45c6492dd42d054a1c
  if (quantized_type == OperandDataType::kUint8 &&
      filter_scale_values.size() != 1 && bias_scale_values.size() != 1) {
    return std::nullopt;
  }
  // For XNNPack delegate, the bias scale is not really used so it requires
  // input_scale * filter_scale and bias_scale to be about the same.
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/kernels/kernel_util.cc;l=303;drc=492dc9719f6e1845f4f5c0553cd5c7651115f671
  const double input_scale = static_cast<double>(input_scale_values[0]);
  const double output_scale = static_cast<double>(output_scale_values[0]);
  const bool scalar_filter_scale = filter_scale_values.size() == 1;
  auto input_product_scalar_filter = base::MakeCheckedNum<double>(input_scale);
  if (scalar_filter_scale) {
    input_product_scalar_filter *= filter_scale_values[0];
  }
  for (size_t i = 0; i < bias_scale_values.size(); ++i) {
    base::CheckedNumeric<double> scale_diff =
        scalar_filter_scale
            ? input_product_scalar_filter
            : base::CheckMul(input_scale,
                             static_cast<double>(filter_scale_values[i]));
    scale_diff -= static_cast<double>(bias_scale_values[i]);
    scale_diff = scale_diff.Abs() / output_scale;

    if (!scale_diff.IsValid() || scale_diff.ValueOrDie() > 0.02) {
      return std::nullopt;
    }
  }
  return SerializeQuantizedOutput(*next_op);
}

std::optional<GraphBuilderTflite::TensorInfo>
GraphBuilderTflite::CanFuseQuantizeAndGetOutput(const mojom::Concat& concat) {
  std::optional<OperandDataType> first_input_quantized_type;
  if (!std::ranges::all_of(
          concat.input_operand_ids, [&](OperandId input_operand_id) {
            if (!IsDequantizeOutput(input_operand_id)) {
              return false;
            }
            const mojom::DequantizeLinear& input_dequantize =
                GetDequantizeOp(input_operand_id);
            if (!IsInts8AndScalarScale(input_dequantize)) {
              return false;
            }
            const OperandDataType quantized_type =
                GetOperand(input_dequantize.input_operand_id)
                    .descriptor.data_type();
            if (!first_input_quantized_type) {
              first_input_quantized_type = quantized_type;
            } else {
              return *first_input_quantized_type == quantized_type;
            }
            return true;
          })) {
    return std::nullopt;
  }
  CHECK(first_input_quantized_type.has_value());

  std::optional<std::pair<OperationId, QuantizateParametersOffset>> next_op =
      IsNextOpQuantize(concat.output_operand_id, {*first_input_quantized_type});
  if (!next_op) {
    return std::nullopt;
  }
  const mojom::QuantizeLinear& output_quantize = GetQuantizeOp(next_op->first);

  // TODO(crbug.com/413083273): Consider the restriction in GPU delegate.
  // For XNNPack delegate and the kernel of concatenation, the scale and zero
  // point of output tensor must be the same as inputs.
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/delegates/xnnpack/xnnpack_delegate.cc;l=3443;drc=b6620a02fa498df5297e53241b54a31f488ca440
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/kernels/concatenation.cc;l=217;drc=87b24bc831966733aa45ad8d1a3ea00d3950b245
  base::span<const float> output_scale_values =
      GetConstantValue<float>(output_quantize.scale_operand_id);
  base::FixedArray<int64_t> output_zero_point_values =
      GetConstantInt64Value(output_quantize.zero_point_operand_id);
  if (output_scale_values.size() != 1 || output_zero_point_values.size() != 1) {
    return std::nullopt;
  }
  for (auto input_operand_id : concat.input_operand_ids) {
    const mojom::DequantizeLinear& input_dequantize =
        GetDequantizeOp(input_operand_id);
    base::span<const float> input_scale_values =
        GetConstantValue<float>(input_dequantize.scale_operand_id);
    CHECK_EQ(input_scale_values.size(), 1u);
    if (input_scale_values[0] != output_scale_values[0]) {
      return std::nullopt;
    }

    base::FixedArray<int64_t> input_zero_point_values =
        GetConstantInt64Value(input_dequantize.zero_point_operand_id);
    CHECK_EQ(input_zero_point_values.size(), 1u);
    if (output_zero_point_values[0] != input_zero_point_values[0]) {
      return std::nullopt;
    }
  }

  return SerializeQuantizedOutput(*next_op);
}

std::optional<GraphBuilderTflite::TensorInfo>
GraphBuilderTflite::CanFuseQuantizeAndGetOutput(
    const mojom::ElementWiseBinary& binary) {
  if (!IsDequantizeOutput(binary.lhs_operand_id) ||
      !IsDequantizeOutput(binary.rhs_operand_id)) {
    return std::nullopt;
  }

  const mojom::DequantizeLinear& lhs_dequantize =
      GetDequantizeOp(binary.lhs_operand_id);
  const mojom::DequantizeLinear& rhs_dequantize =
      GetDequantizeOp(binary.rhs_operand_id);
  const OperandDataType quantized_type =
      GetOperand(lhs_dequantize.input_operand_id).descriptor.data_type();
  if (!IsInts8AndScalarScale(lhs_dequantize) ||
      !IsInts8AndScalarScale(rhs_dequantize) ||
      GetOperand(rhs_dequantize.input_operand_id).descriptor.data_type() !=
          quantized_type) {
    return std::nullopt;
  }

  std::optional<std::pair<OperationId, QuantizateParametersOffset>> next_op =
      IsNextOpQuantize(binary.output_operand_id, {quantized_type});
  if (!next_op) {
    return std::nullopt;
  }
  const mojom::QuantizeLinear& output_quantize = GetQuantizeOp(next_op->first);
  base::span<const float> lhs_scale_values =
      GetConstantValue<float>(lhs_dequantize.scale_operand_id);
  base::span<const float> rhs_scale_values =
      GetConstantValue<float>(rhs_dequantize.scale_operand_id);
  base::span<const float> output_scale_values =
      GetConstantValue<float>(output_quantize.scale_operand_id);
  // The shape of scale and zero point is the same, that has been verified in
  // the function ValidateScaleZeroPointOperandShapeIsCompatibleWithInput.
  if (output_scale_values.size() != 1) {
    return std::nullopt;
  }
  CHECK_EQ(GetOperand(output_quantize.zero_point_operand_id)
               .descriptor.NumberOfElements(),
           1u);

  // TODO(crbug.com/413083273): Consider the restriction in GPU delegate.
  // For XNNPack delegate, there are some restriction for inputs and output
  // scale value.
  const float lhs_scale_value = lhs_scale_values[0];
  const float rhs_scale_value = rhs_scale_values[0];
  const float output_scale_value = output_scale_values[0];
  if (binary.kind == mojom::ElementWiseBinary::Kind::kAdd ||
      binary.kind == mojom::ElementWiseBinary::Kind::kSub) {
    // The `input scale / output scale` must be in the range.
    // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/delegates/xnnpack/xnnpack_delegate.cc;l=3957;drc=f667feb8a5c6f227b49328ce78a062acc4f81187
    const float scale_min = 1.0f / 1024.0f;
    const float scale_max = 256.0f;

    auto checked_lhs_output_scale =
        base::MakeCheckedNum<float>(lhs_scale_value);
    checked_lhs_output_scale /= output_scale_value;
    if (!checked_lhs_output_scale.IsValid() ||
        checked_lhs_output_scale.ValueOrDie() < scale_min ||
        checked_lhs_output_scale.ValueOrDie() >= scale_max) {
      return std::nullopt;
    }
    auto checked_rhs_output_scale =
        base::MakeCheckedNum<float>(rhs_scale_value) / output_scale_value;
    if (!checked_rhs_output_scale.IsValid() ||
        checked_rhs_output_scale.ValueOrDie() < scale_min ||
        checked_rhs_output_scale.ValueOrDie() >= scale_max) {
      return std::nullopt;
    }
  } else if (binary.kind == mojom::ElementWiseBinary::Kind::kMul) {
    // The `lhs * rhs input scale / output scale` must be in the range.
    // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/delegates/xnnpack/xnnpack_delegate.cc;l=3985;drc=f667feb8a5c6f227b49328ce78a062acc4f81187
    const float scale_min = 1.0f / 65536.0f;
    const float scale_max = 256.0f;
    auto checked_product_output_scale =
        (lhs_scale_value * base::MakeCheckedNum<float>(rhs_scale_value)) /
        output_scale_value;
    if (!checked_product_output_scale.IsValid() ||
        checked_product_output_scale.ValueOrDie() < scale_min ||
        checked_product_output_scale.ValueOrDie() >= scale_max) {
      return std::nullopt;
    }
  } else if (binary.kind == mojom::ElementWiseBinary::Kind::kMax ||
             binary.kind == mojom::ElementWiseBinary::Kind::kMin) {
    // Inputs and output must have the same scale and zero_point.
    // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/kernels/internal/optimized/optimized_ops.h;l=7101;drc=467a8e68f685f9cfa47ee3fbfca20c22f7f6e893
    if (lhs_scale_value != rhs_scale_value ||
        lhs_scale_value != output_scale_value) {
      return std::nullopt;
    }
    base::FixedArray<int64_t> lhs_zero_point_values =
        GetConstantInt64Value(lhs_dequantize.zero_point_operand_id);
    base::FixedArray<int64_t> rhs_zero_point_values =
        GetConstantInt64Value(rhs_dequantize.zero_point_operand_id);
    base::FixedArray<int64_t> output_zero_point_values =
        GetConstantInt64Value(output_quantize.zero_point_operand_id);
    if (!std::ranges::equal(lhs_zero_point_values, rhs_zero_point_values) ||
        !std::ranges::equal(lhs_zero_point_values, output_zero_point_values)) {
      return std::nullopt;
    }
  } else {
    NOTREACHED() << "Unsupported quantize operators";
  }

  return SerializeQuantizedOutput(*next_op);
}

std::optional<GraphBuilderTflite::TensorInfo>
GraphBuilderTflite::CanFuseQuantizeAndGetOutput(const mojom::Elu& elu) {
  if (!IsDequantizeOutput(elu.input_operand_id)) {
    return std::nullopt;
  }

  // TODO(crbug.com/413083273): Consider the restriction in GPU delegate.
  // For XNNPack delegate, the input must be dequantized from int8, the input
  // and output scale must be scaler.
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/delegates/xnnpack/xnnpack_delegate.cc;l=4136;drc=f667feb8a5c6f227b49328ce78a062acc4f81187
  const mojom::DequantizeLinear& input_dequantize =
      GetDequantizeOp(elu.input_operand_id);
  if (GetOperand(input_dequantize.input_operand_id).descriptor.data_type() !=
      OperandDataType::kInt8) {
    return std::nullopt;
  }

  if (GetOperand(input_dequantize.scale_operand_id)
          .descriptor.NumberOfElements() != 1) {
    return std::nullopt;
  }

  std::optional<std::pair<OperationId, QuantizateParametersOffset>> next_op =
      IsNextOpQuantize(elu.output_operand_id, {OperandDataType::kInt8});
  if (!next_op) {
    return std::nullopt;
  }

  const mojom::QuantizeLinear& output_quantize = GetQuantizeOp(next_op->first);
  if (GetOperand(output_quantize.scale_operand_id)
          .descriptor.NumberOfElements() != 1) {
    return std::nullopt;
  }

  return SerializeQuantizedOutput(*next_op);
}

std::optional<GraphBuilderTflite::TensorInfo>
GraphBuilderTflite::CanFuseQuantizeAndGetOutput(const mojom::Gather& gather) {
  if (!IsDequantizeOutput(gather.input_operand_id)) {
    return std::nullopt;
  }

  // TODO(crbug.com/413083273): Consider the restriction in GPU delegate.
  // Input and output must all have same scale/zero_point, see quantization
  // requirements of gather at
  // https://ai.google.dev/edge/litert/models/quantization_spec#int8_quantized_operator_specifications
  const mojom::DequantizeLinear& input_dequantize =
      GetDequantizeOp(gather.input_operand_id);
  const OperandDataType quantized_type =
      GetOperand(input_dequantize.input_operand_id).descriptor.data_type();
  if (!DataTypeConstraint::kInts8.Has(quantized_type)) {
    return std::nullopt;
  }

  std::optional<std::pair<OperationId, QuantizateParametersOffset>> next_op =
      IsNextOpQuantize(gather.output_operand_id, {quantized_type});
  if (!next_op) {
    return std::nullopt;
  }

  const mojom::QuantizeLinear& output_quantize = GetQuantizeOp(next_op->first);
  base::span<const float> input_scale_values =
      GetConstantValue<float>(input_dequantize.scale_operand_id);
  base::span<const float> output_scale_values =
      GetConstantValue<float>(output_quantize.scale_operand_id);
  if (!std::ranges::equal(input_scale_values, output_scale_values)) {
    return std::nullopt;
  }
  base::FixedArray<int64_t> input_zero_point_values =
      GetConstantInt64Value(input_dequantize.zero_point_operand_id);
  base::FixedArray<int64_t> output_zero_point_values =
      GetConstantInt64Value(output_quantize.zero_point_operand_id);
  if (!std::ranges::equal(input_zero_point_values, output_zero_point_values)) {
    return std::nullopt;
  }

  return SerializeQuantizedOutput(*next_op);
}

std::optional<GraphBuilderTflite::TensorInfo>
GraphBuilderTflite::CanFuseQuantizeAndGetOutput(const mojom::Pool2d& pool2d) {
  // L2Pool doesn't support quantized implementation.
  CHECK_NE(pool2d.kind, mojom::Pool2d::Kind::kL2Pool2d);

  if (!IsDequantizeOutput(pool2d.input_operand_id)) {
    return std::nullopt;
  }

  const mojom::DequantizeLinear& input_dequantize =
      GetDequantizeOp(pool2d.input_operand_id);
  if (!IsInts8AndScalarScale(input_dequantize)) {
    return std::nullopt;
  }

  // TODO(crbug.com/413083273): Consider the restriction in GPU delegate.
  // For the kernel of pooling, the `|input scale - output scale|` must be less
  // than 1.0e-6, and zero point of output tensor must be the same as input.
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/kernels/pooling.cc;drc=edd09bcc365dcc696d0f23ca7c3dc18f5e1dcdab;l=101
  std::optional<std::pair<OperationId, QuantizateParametersOffset>> next_op =
      IsNextOpQuantize(pool2d.output_operand_id,
                       {GetOperand(input_dequantize.input_operand_id)
                            .descriptor.data_type()});
  if (!next_op) {
    return std::nullopt;
  }
  const mojom::QuantizeLinear& output_quantize = GetQuantizeOp(next_op->first);
  if (!IsInts8AndScalarScale(output_quantize)) {
    return std::nullopt;
  }
  base::span<const float> input_scale_values =
      GetConstantValue<float>(input_dequantize.scale_operand_id);
  base::span<const float> output_scale_values =
      GetConstantValue<float>(output_quantize.scale_operand_id);
  base::CheckedNumeric<float> checked_sub_scale =
      base::MakeCheckedNum<float>(input_scale_values[0]) -
      output_scale_values[0];
  if (!checked_sub_scale.IsValid() ||
      checked_sub_scale.Abs().ValueOrDie() > 1.0e-6) {
    return std::nullopt;
  }

  base::FixedArray<int64_t> input_zero_point_values =
      GetConstantInt64Value(input_dequantize.zero_point_operand_id);
  base::FixedArray<int64_t> output_zero_point_values =
      GetConstantInt64Value(output_quantize.zero_point_operand_id);
  if (input_zero_point_values[0] != output_zero_point_values[0]) {
    return std::nullopt;
  }

  return SerializeQuantizedOutput(*next_op);
}

std::optional<GraphBuilderTflite::TensorInfo>
GraphBuilderTflite::CanFuseQuantizeAndGetOutput(const mojom::Reduce& reduce) {
  // QDQ fusion only support reduce operation with kSum, kMean, kMax and kMin
  // kinds.
  if (reduce.kind != mojom::Reduce::Kind::kSum &&
      reduce.kind != mojom::Reduce::Kind::kMean &&
      reduce.kind != mojom::Reduce::Kind::kMax &&
      reduce.kind != mojom::Reduce::Kind::kMin) {
    return std::nullopt;
  }

  if (!IsDequantizeOutput(reduce.input_operand_id)) {
    return std::nullopt;
  }

  // TODO(crbug.com/413083273): Consider the restriction in GPU delegate.
  // For XNNPack delegate, input and output operands have to be dequantized from
  // ints8, the scale and zero point of input and output have to be scaler.
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/delegates/xnnpack/xnnpack_delegate.cc;l=4683;drc=884710320aa8a793be1407d8b8091b538658f5e6
  const mojom::DequantizeLinear& input_dequantize =
      GetDequantizeOp(reduce.input_operand_id);
  if (!IsInts8AndScalarScale(input_dequantize)) {
    return std::nullopt;
  }

  std::optional<std::pair<OperationId, QuantizateParametersOffset>> next_op =
      IsNextOpQuantize(reduce.output_operand_id,
                       {GetOperand(input_dequantize.input_operand_id)
                            .descriptor.data_type()});
  if (!next_op) {
    return std::nullopt;
  }

  const mojom::QuantizeLinear& output_quantize = GetQuantizeOp(next_op->first);
  if (!IsInts8AndScalarScale(output_quantize)) {
    return std::nullopt;
  }

  return SerializeQuantizedOutput(*next_op);
}

std::optional<GraphBuilderTflite::TensorInfo>
GraphBuilderTflite::CanFuseQuantizeAndGetOutput(const mojom::Reshape& reshape) {
  if (!IsDequantizeOutput(reshape.input_operand_id)) {
    return std::nullopt;
  }

  // TODO(crbug.com/413083273): Consider the restriction in GPU delegate.
  // For XNNPack delegate, the scale and zero point of input and output have to
  // be scaler, and the scale and zero point of output must be the same as
  // input.
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/delegates/xnnpack/xnnpack_delegate.cc;l=5199;drc=1379ddb0f0535ff846ce0fbad8ee49af303140c4
  const mojom::DequantizeLinear& input_dequantize =
      GetDequantizeOp(reshape.input_operand_id);
  if (!IsInts8AndScalarScale(input_dequantize)) {
    return std::nullopt;
  }

  std::optional<std::pair<OperationId, QuantizateParametersOffset>> next_op =
      IsNextOpQuantize(reshape.output_operand_id,
                       {GetOperand(input_dequantize.input_operand_id)
                            .descriptor.data_type()});
  if (!next_op) {
    return std::nullopt;
  }

  const mojom::QuantizeLinear& output_quantize = GetQuantizeOp(next_op->first);
  if (!IsInts8AndScalarScale(output_quantize)) {
    return std::nullopt;
  }

  base::span<const float> input_scale_values =
      GetConstantValue<float>(input_dequantize.scale_operand_id);
  base::span<const float> output_scale_values =
      GetConstantValue<float>(output_quantize.scale_operand_id);
  if (input_scale_values[0] != output_scale_values[0]) {
    return std::nullopt;
  }

  base::FixedArray<int64_t> input_zero_point_values =
      GetConstantInt64Value(input_dequantize.zero_point_operand_id);
  base::FixedArray<int64_t> output_zero_point_values =
      GetConstantInt64Value(output_quantize.zero_point_operand_id);
  if (input_zero_point_values[0] != output_zero_point_values[0]) {
    return std::nullopt;
  }

  return SerializeQuantizedOutput(*next_op);
}

std::optional<GraphBuilderTflite::TensorInfo>
GraphBuilderTflite::CanFuseQuantizeAndGetOutput(const mojom::Slice& slice) {
  if (!IsDequantizeOutput(slice.input_operand_id)) {
    return std::nullopt;
  }

  // TODO(crbug.com/413083273): Consider the restriction in GPU delegate.
  // For XNNPack delegate, the scale and zero point of input and output have to
  // be scaler.
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/delegates/xnnpack/xnnpack_delegate.cc;l=5302;drc=02446d66622a0a811448be7bb4ac8939c5b00aa9
  const mojom::DequantizeLinear& input_dequantize =
      GetDequantizeOp(slice.input_operand_id);
  if (!IsInts8AndScalarScale(input_dequantize)) {
    return std::nullopt;
  }

  std::optional<std::pair<OperationId, QuantizateParametersOffset>> next_op =
      IsNextOpQuantize(slice.output_operand_id,
                       {GetOperand(input_dequantize.input_operand_id)
                            .descriptor.data_type()});
  if (!next_op) {
    return std::nullopt;
  }

  const mojom::QuantizeLinear& output_quantize = GetQuantizeOp(next_op->first);
  if (!IsInts8AndScalarScale(output_quantize)) {
    return std::nullopt;
  }

  return SerializeQuantizedOutput(*next_op);
}

std::optional<GraphBuilderTflite::TensorInfo>
GraphBuilderTflite::CanFuseQuantizeAndGetOutput(const mojom::Softmax& softmax) {
  if (!IsDequantizeOutput(softmax.input_operand_id)) {
    return std::nullopt;
  }

  // TODO(crbug.com/413083273): Consider the restriction in GPU delegate.
  // For TFLite kernel, the scale of output should be approximately equal
  // to 1.0f / 256.0f and the zero point of output should be equal to -128
  // if data type is int8.
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/kernels/activations.cc;l=541;drc=f667feb8a5c6f227b49328ce78a062acc4f81187
  const mojom::DequantizeLinear& input_dequantize =
      GetDequantizeOp(softmax.input_operand_id);
  if (!IsInts8AndScalarScale(input_dequantize)) {
    return std::nullopt;
  }

  const OperandDataType quantized_type =
      GetOperand(input_dequantize.input_operand_id).descriptor.data_type();
  std::optional<std::pair<OperationId, QuantizateParametersOffset>> next_op =
      IsNextOpQuantize(softmax.output_operand_id, {quantized_type});
  if (!next_op) {
    return std::nullopt;
  }

  const mojom::QuantizeLinear& output_quantize = GetQuantizeOp(next_op->first);
  if (!IsInts8AndScalarScale(output_quantize)) {
    return std::nullopt;
  }

  if (quantized_type == OperandDataType::kInt8) {
    const float expected_scale_value = 1.0f / 256.0f;
    base::span<const float> output_scale_values =
        GetConstantValue<float>(output_quantize.scale_operand_id);
    base::CheckedNumeric<float> checked_scale =
        base::MakeCheckedNum<float>(output_scale_values[0]) -
        expected_scale_value;
    if (!checked_scale.IsValid() ||
        checked_scale.Abs().ValueOrDie() > 0.001f * expected_scale_value) {
      return std::nullopt;
    }

    base::FixedArray<int64_t> output_zero_point_values =
        GetConstantInt64Value(output_quantize.zero_point_operand_id);
    if (output_zero_point_values[0] != -128) {
      return std::nullopt;
    }
  }

  return SerializeQuantizedOutput(*next_op);
}

std::optional<base::FixedArray<GraphBuilderTflite::TensorInfo>>
GraphBuilderTflite::CanFuseQuantizeAndGetOutput(const mojom::Split& split) {
  if (!IsDequantizeOutput(split.input_operand_id)) {
    return std::nullopt;
  }

  // TODO(crbug.com/413083273): Consider the restriction in GPU delegate.
  // For XNNPack delegate, the scale and zero point of input and output have to
  // be scaler, and the number of outputs should be in the range of [2, 4]. But
  // there is no limitation on the number of outputs for TFLite kernel, so relax
  // the output number restriction here.
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/delegates/xnnpack/xnnpack_delegate.cc;l=5558;drc=1379ddb0f0535ff846ce0fbad8ee49af303140c4
  const mojom::DequantizeLinear& input_dequantize =
      GetDequantizeOp(split.input_operand_id);
  if (!IsInts8AndScalarScale(input_dequantize)) {
    return std::nullopt;
  }

  const size_t outputs_size = split.output_operand_ids.size();
  const OperandDataType quantized_type =
      GetOperand(input_dequantize.input_operand_id).descriptor.data_type();
  base::FixedArray<std::pair<OperationId, QuantizateParametersOffset>>
      quantize_ops(outputs_size);
  for (size_t i = 0; i < outputs_size; ++i) {
    std::optional<std::pair<OperationId, QuantizateParametersOffset>> next_op =
        IsNextOpQuantize(split.output_operand_ids[i], {quantized_type});
    if (!next_op) {
      return std::nullopt;
    }

    OperationId quantize_op_id = next_op->first;
    const mojom::QuantizeLinear& output_quantize =
        GetQuantizeOp(quantize_op_id);
    if (!IsInts8AndScalarScale(output_quantize)) {
      return std::nullopt;
    }
    quantize_ops[i] = std::move(*next_op);
  }

  base::FixedArray<TensorInfo> output_tensor_infos(outputs_size);
  for (size_t i = 0; i < outputs_size; ++i) {
    output_tensor_infos[i] = SerializeQuantizedOutput(quantize_ops[i]);
  }

  return output_tensor_infos;
}

std::optional<GraphBuilderTflite::TensorInfo>
GraphBuilderTflite::CanFuseQuantizeAndGetOutput(
    const mojom::Transpose& transpose) {
  if (!IsDequantizeOutput(transpose.input_operand_id)) {
    return std::nullopt;
  }

  const mojom::DequantizeLinear& input_dequantize =
      GetDequantizeOp(transpose.input_operand_id);
  if (!IsInts8AndScalarScale(input_dequantize)) {
    return std::nullopt;
  }

  std::optional<std::pair<OperationId, QuantizateParametersOffset>> next_op =
      IsNextOpQuantize(transpose.output_operand_id,
                       {GetOperand(input_dequantize.input_operand_id)
                            .descriptor.data_type()});

  return next_op ? SerializeQuantizedOutput(*next_op)
                 : std::optional<TensorInfo>{};
}

std::optional<GraphBuilderTflite::TensorInfo>
GraphBuilderTflite::CanFuseQuantizeAndGetOutput(const mojom::Tanh& tanh) {
  std::optional<std::pair<OperationId, QuantizateParametersOffset>> next_op =
      CanFuseQuantizeForActivationOperation(tanh);
  return next_op ? SerializeQuantizedOutput(*next_op)
                 : std::optional<TensorInfo>{};
}

std::optional<GraphBuilderTflite::TensorInfo>
GraphBuilderTflite::CanFuseQuantizeAndGetOutput(const mojom::Sigmoid& sigmoid) {
  std::optional<std::pair<OperationId, QuantizateParametersOffset>> next_op =
      CanFuseQuantizeForActivationOperation(sigmoid);
  if (!next_op) {
    return std::nullopt;
  }

  const mojom::QuantizeLinear& output_quantize = GetQuantizeOp(next_op->first);
  base::span<const float> output_scale_values =
      GetConstantValue<float>(output_quantize.scale_operand_id);
  // The output scale value must be 1.0f / 256.0f.
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/kernels/activations.cc;l=463;drc=f667feb8a5c6f227b49328ce78a062acc4f81187
  if (output_scale_values[0] != 1.0f / 256.0f) {
    return std::nullopt;
  }

  return SerializeQuantizedOutput(*next_op);
}

std::optional<GraphBuilderTflite::TensorInfo>
GraphBuilderTflite::CanFuseQuantizeAndGetOutput(
    const mojom::LeakyRelu& leaky_relu) {
  std::optional<std::pair<OperationId, QuantizateParametersOffset>> next_op =
      CanFuseQuantizeForActivationOperation(leaky_relu);
  if (!next_op) {
    return std::nullopt;
  }

  // The alpha value can't be 0.0f.
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/delegates/xnnpack/xnnpack_delegate.cc;l=4151;drc=f667feb8a5c6f227b49328ce78a062acc4f81187
  if (leaky_relu.alpha == 0.0f) {
    return std::nullopt;
  }

  const mojom::DequantizeLinear& input_dequantize =
      GetDequantizeOp(leaky_relu.input_operand_id);
  const mojom::QuantizeLinear& output_quantize = GetQuantizeOp(next_op->first);
  base::span<const float> input_scale_values =
      GetConstantValue<float>(input_dequantize.scale_operand_id);
  base::span<const float> output_scale_values =
      GetConstantValue<float>(output_quantize.scale_operand_id);
  const float scale_positive_min = 1.0f / 256.0f;
  const float scale_positive_max = 128.0f;
  const float scale_negative_min = -127.99609375f;
  // The `input scale / output scale` must be in the range.
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/delegates/xnnpack/xnnpack_delegate.cc;l=4162;drc=f667feb8a5c6f227b49328ce78a062acc4f81187
  base::CheckedNumeric<float> checked_positive_scale =
      base::MakeCheckedNum<float>(input_scale_values[0]) /
      output_scale_values[0];
  if (!checked_positive_scale.IsValid() ||
      checked_positive_scale.ValueOrDie() < scale_positive_min ||
      checked_positive_scale.ValueOrDie() > scale_positive_max) {
    return std::nullopt;
  }

  // The `input scale * alpha / output scale` must be in the range.
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/delegates/xnnpack/xnnpack_delegate.cc;l=4171;drc=f667feb8a5c6f227b49328ce78a062acc4f81187
  base::CheckedNumeric<float> checked_negative_scale =
      checked_positive_scale * leaky_relu.alpha;
  if (!checked_negative_scale.IsValid() ||
      checked_negative_scale.ValueOrDie() < scale_negative_min ||
      checked_negative_scale.ValueOrDie() > scale_positive_max ||
      checked_negative_scale.Abs().ValueOrDie() < scale_positive_min) {
    return std::nullopt;
  }

  return SerializeQuantizedOutput(*next_op);
}

template <typename OpType>
std::optional<
    std::pair<OperationId, GraphBuilderTflite::QuantizateParametersOffset>>
GraphBuilderTflite::CanFuseQuantizeForActivationOperation(const OpType& op) {
  if constexpr (!std::is_same_v<OpType, mojom::Tanh> &&
                !std::is_same_v<OpType, mojom::Sigmoid> &&
                !std::is_same_v<OpType, mojom::LeakyRelu>) {
    NOTREACHED() << "Unsupported quantize operators";
  }

  if (!IsDequantizeOutput(op.input_operand_id)) {
    return std::nullopt;
  }

  const mojom::DequantizeLinear& input_dequantize =
      GetDequantizeOp(op.input_operand_id);
  if (!IsInts8AndScalarScale(input_dequantize)) {
    return std::nullopt;
  }

  std::optional<std::pair<OperationId, QuantizateParametersOffset>> next_op =
      IsNextOpQuantize(op.output_operand_id,
                       {GetOperand(input_dequantize.input_operand_id)
                            .descriptor.data_type()});
  if (!next_op) {
    return std::nullopt;
  }

  const mojom::QuantizeLinear& output_quantize = GetQuantizeOp(next_op->first);
  if (!IsInts8AndScalarScale(output_quantize)) {
    return std::nullopt;
  }

  return next_op;
}

bool GraphBuilderTflite::IsDequantizeOutput(OperandId operand_id) {
  return lazy_serialized_dequantize_operations_.contains(operand_id);
}

const mojom::DequantizeLinear& GraphBuilderTflite::GetDequantizeOp(
    OperandId operand_id) {
  auto it = lazy_serialized_dequantize_operations_.find(operand_id);
  CHECK(it != lazy_serialized_dequantize_operations_.end());

  OperationId operation_index = it->second.first;
  CHECK_LT(operation_index, graph_info_->operations.size());

  const mojom::Operation& operation = *graph_info_->operations[operation_index];
  CHECK(operation.is_dequantize_linear());
  return *operation.get_dequantize_linear();
}

const mojom::QuantizeLinear& GraphBuilderTflite::GetQuantizeOp(
    OperationId operation_index) {
  CHECK_LT(operation_index, graph_info_->operations.size());

  const mojom::Operation& operation = *graph_info_->operations[operation_index];
  CHECK(operation.is_quantize_linear());
  return *operation.get_quantize_linear();
}

base::expected<void, std::string>
GraphBuilderTflite::TryTraverseToSerializeQuantizedInput(
    const mojom::DequantizeLinear& dequantize_linear) {
  const mojom::Operand& input_operand =
      GetOperand(dequantize_linear.input_operand_id);
  // Required by all the quantization agnostic operations.
  if (!IsInts8AndScalarScale(dequantize_linear)) {
    return base::ok();
  }

  std::optional<QuantizateParametersOffset> quantize_params =
      SerializeQuantizeParams(dequantize_linear.zero_point_operand_id,
                              dequantize_linear.scale_operand_id,
                              input_operand.descriptor.shape().size());
  if (!quantize_params) {
    return base::ok();
  }
  auto producing_operation_it =
      operand_to_producing_operation_->find(dequantize_linear.input_operand_id);

  std::vector<OperandId> operands_to_serialize{
      dequantize_linear.input_operand_id};
  while (producing_operation_it != operand_to_producing_operation_->end()) {
    OperationId operation_index = producing_operation_it->second;
    const mojom::Operation& operation =
        *graph_info_->operations[operation_index];

    OperandId input_operand_id;
    switch (operation.which()) {
      case (mojom::Operation::Tag::kTranspose): {
        input_operand_id = operation.get_transpose()->input_operand_id;
        break;
      }
      case (mojom::Operation::Tag::kReshape): {
        input_operand_id = operation.get_reshape()->input_operand_id;
        break;
      }
      // Can't serialize with quantization params if there is an upstream
      // operation that's not quantization agnostic.
      default:
        return base::ok();
    }
    operands_to_serialize.push_back(input_operand_id);
    producing_operation_it =
        operand_to_producing_operation_->find(input_operand_id);
  }

  for (OperandId operand_id : operands_to_serialize) {
    RETURN_IF_ERROR(SerializeInputTensorInfo(operand_id, *quantize_params));
  }
  return base::ok();
}

bool GraphBuilderTflite::TrySerializeQuantizedInput(
    const mojom::DequantizeLinear& dequantize_linear,
    OperationId operation_index) {
  const mojom::Operand& input_operand =
      GetOperand(dequantize_linear.input_operand_id);
  if (!BroadcastShapes(
          GetOperand(dequantize_linear.scale_operand_id).descriptor.shape(),
          input_operand.descriptor.shape(),
          /*bidirectional=*/false)) {
    return false;
  }

  std::optional<QuantizateParametersOffset> quantize_params =
      SerializeQuantizeParams(dequantize_linear.zero_point_operand_id,
                              dequantize_linear.scale_operand_id,
                              input_operand.descriptor.shape().size());
  if (!quantize_params) {
    return false;
  }

  // The input is already serialized and the qint tensor has a different
  // quantize params, so we have to dequantize explicitly with the new quantize
  // params.
  if (IsSerializedWithMismatchQuantizeParameters(
          dequantize_linear.input_operand_id, *quantize_params)) {
    return false;
  }

  // Eagerly serialize input with `quantize_params` so that when the
  // dequantizeLinear is skipped, the `quantize_params` is already attached to
  // the input.
  if (!SerializeInputTensorInfo(dequantize_linear.input_operand_id,
                                *quantize_params)
           .has_value()) {
    return false;
  }

  CHECK(lazy_serialized_dequantize_operations_
            .try_emplace(dequantize_linear.output_operand_id,
                         std::make_pair(operation_index, /*serialized=*/false))
            .second);
  return true;
}

std::optional<
    std::pair<OperationId, GraphBuilderTflite::QuantizateParametersOffset>>
GraphBuilderTflite::IsNextOpQuantize(
    OperandId output_operand_id,
    SupportedDataTypes supported_quantized_types) {
  auto next_next_ops_it =
      operand_to_dependent_operations_->find(output_operand_id);
  // The only next op should be `quantizeLinear`.
  if (next_next_ops_it == operand_to_dependent_operations_->end() ||
      next_next_ops_it->second.size() != 1) {
    return std::nullopt;
  }
  OperationId quantize_op_idx = *next_next_ops_it->second.begin();
  CHECK_LT(quantize_op_idx, graph_info_->operations.size());
  const mojom::Operation& quantize_op =
      *graph_info_->operations[quantize_op_idx];
  if (!quantize_op.is_quantize_linear()) {
    return std::nullopt;
  }

  const mojom::QuantizeLinear& quantize_linear =
      *quantize_op.get_quantize_linear();
  if (!supported_quantized_types.Has(
          GetOperand(quantize_linear.output_operand_id)
              .descriptor.data_type())) {
    return std::nullopt;
  }

  // qint tensors don't support blockwise quantization.
  if (!BroadcastShapes(
          GetOperand(quantize_linear.scale_operand_id).descriptor.shape(),
          GetOperand(quantize_linear.input_operand_id).descriptor.shape(),
          /*bidirectional=*/false)) {
    return std::nullopt;
  }

  std::optional<QuantizateParametersOffset> quantize_params =
      SerializeQuantizeParams(quantize_linear.zero_point_operand_id,
                              quantize_linear.scale_operand_id,
                              GetOperand(quantize_linear.input_operand_id)
                                  .descriptor.shape()
                                  .size());
  if (!quantize_params) {
    return std::nullopt;
  }

  return std::make_pair(quantize_op_idx, quantize_params.value());
}

template <typename OpType>
  requires(std::is_same_v<OpType, mojom::DequantizeLinear> ||
           std::is_same_v<OpType, mojom::QuantizeLinear>)
bool GraphBuilderTflite::IsInts8AndScalarScale(const OpType& op) {
  if constexpr (std::is_same_v<OpType, mojom::DequantizeLinear>) {
    if (!DataTypeConstraint::kInts8.Has(
            GetOperand(op.input_operand_id).descriptor.data_type())) {
      return false;
    }
  }

  if (GetOperand(op.scale_operand_id).descriptor.NumberOfElements() != 1) {
    return false;
  }

  // The shape of scale and zero point is the same that has been verified in
  // the function ValidateScaleZeroPointOperandShapeIsCompatibleWithInput.
  CHECK_EQ(GetOperand(op.zero_point_operand_id).descriptor.NumberOfElements(),
           1u);
  return true;
}

GraphBuilderTflite::TensorInfo GraphBuilderTflite::SerializeQuantizedOutput(
    std::pair<OperationId, QuantizateParametersOffset> quantize_op_info) {
  const OperationId quantize_op_idx = quantize_op_info.first;
  const mojom::QuantizeLinear& quantize_linear = GetQuantizeOp(quantize_op_idx);
  quantize_ops_to_skip_.insert(quantize_op_idx);
  return SerializeOutputTensorInfo(quantize_linear.output_operand_id,
                                   quantize_op_info.second);
}

bool GraphBuilderTflite::IsSerializedWithMismatchQuantizeParameters(
    OperandId operand_id,
    QuantizateParametersOffset quantize_params) {
  auto it = operand_to_tensor_info_map_.find(operand_id);
  if (it == operand_to_tensor_info_map_.end()) {
    return false;
  }
  QuantizateParametersOffset existing_quantize_params =
      it->second.quantize_params;

  if (quantize_params.IsNull()) {
    return !existing_quantize_params.IsNull();
  }

  if (existing_quantize_params.IsNull()) {
    return !quantize_params.IsNull();
  }

  auto lhs_it = quantize_param_data_.find(quantize_params.o);
  CHECK(lhs_it != quantize_param_data_.end());
  auto [lhs_scale, lhs_zero_point] = lhs_it->second;
  auto rhs_it = quantize_param_data_.find(existing_quantize_params.o);
  CHECK(rhs_it != quantize_param_data_.end());
  auto [rhs_scale, rhs_zero_point] = rhs_it->second;

  return (!AreConstantOperandsEqual(lhs_scale, rhs_scale) ||
          !AreConstantOperandsEqual(lhs_zero_point, rhs_zero_point));
}

bool GraphBuilderTflite::AreConstantOperandsEqual(OperandId lhs_operand_id,
                                                  OperandId rhs_operand_id) {
  if (lhs_operand_id == rhs_operand_id) {
    return true;
  }
  auto lhs_operand_id_constant_it = constant_operands_->find(lhs_operand_id);
  CHECK(lhs_operand_id_constant_it != constant_operands_->end());
  auto rhs_operand_id_constant_it = constant_operands_->find(rhs_operand_id);
  CHECK(rhs_operand_id_constant_it != constant_operands_->end());
  return lhs_operand_id_constant_it->second->ByteSpan() ==
         rhs_operand_id_constant_it->second->ByteSpan();
}

auto GraphBuilderTflite::FinishAndTakeResult(
    base::span<const OperandId> input_operands,
    base::span<const OperandId> output_operands,
    bool graph_requires_fp32_precision) -> Result {
  CHECK(!is_created_model_);

  auto get_index = [&](OperandId operand_id) {
    return operand_to_tensor_info_map_.at(operand_id).index;
  };

  auto get_name_and_index = [&](OperandId operand_id) {
    const TensorInfo& info = operand_to_tensor_info_map_.at(operand_id);
    CHECK(info.name.has_value() && !info.name.value().empty());
    return std::make_pair(info.name.value(), info.index);
  };

  TensorIndex* graph_input_ids = nullptr;
  auto graph_input_ids_index = builder_.CreateUninitializedVector<TensorIndex>(
      input_operands.size(), &graph_input_ids);
  std::ranges::transform(input_operands, graph_input_ids, get_index);

  std::vector<std::pair<std::string, int>> input_name_to_index;
  input_name_to_index.reserve(input_operands.size());
  std::ranges::transform(input_operands,
                         std::back_inserter(input_name_to_index),
                         get_name_and_index);

  TensorIndex* graph_output_ids = nullptr;
  auto graph_output_ids_index = builder_.CreateUninitializedVector<TensorIndex>(
      output_operands.size(), &graph_output_ids);
  std::ranges::transform(output_operands, graph_output_ids, get_index);

  std::vector<std::pair<std::string, int>> output_name_to_index;
  output_name_to_index.reserve(output_operands.size());
  std::ranges::transform(output_operands,
                         std::back_inserter(output_name_to_index),
                         get_name_and_index);

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

  std::vector<flatbuffers::Offset<::tflite::Metadata>> metadata;
  if (!graph_requires_fp32_precision) {
    const auto precision_mask =
        ::tflite::optimize::ReducedPrecisionSupport::Float16Inference |
        ::tflite::optimize::ReducedPrecisionSupport::Float16Accumulation;
    const std::pair<std::string, std::string> precision_metadata =
        MetadataForReducedPrecisionSupport(precision_mask);
    base::span<const uint8_t> metadata_value =
        base::as_byte_span(precision_metadata.second);

    buffers_.push_back(::tflite::CreateBuffer(
        builder_,
        builder_.CreateVector(metadata_value.data(), metadata_value.size())));

    metadata.push_back(::tflite::CreateMetadata(
        builder_, builder_.CreateString(precision_metadata.first),
        /*buffer=*/buffers_.size() - 1));
  }

  // The operator codes used in this model are kept in order because operators
  // carry an index into this std::vector.
  // There is only one subgraph in the model. The buffers of the model must be
  // initialized an empty buffer.
  flatbuffers::Offset<::tflite::Model> model_buffer = ::tflite::CreateModel(
      builder_, TFLITE_SCHEMA_VERSION,
      builder_.CreateVector(operator_codes_.data(), operator_codes_.size()),
      builder_.CreateVector(&subgraph, 1), description,
      builder_.CreateVector(buffers_.data(), buffers_.size()),
      /*metadata_buffer=*/0,  // deprecated, metadata buffer is in `buffers_`.
      builder_.CreateVector(metadata));

  ::tflite::FinishModelBuffer(builder_, model_buffer);
  is_created_model_ = true;

  return {builder_.Release(), std::move(input_name_to_index),
          std::move(output_name_to_index), std::move(buffer_data_),
          graph_requires_fp32_precision};
}

GraphBuilderTflite::BufferIndex GraphBuilderTflite::SerializeBuffer(
    base::span<const uint8_t> buffer) {
  const auto buffer_index = base::checked_cast<BufferIndex>(buffers_.size());
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebNNTfliteDumpModel)) {
    buffers_.emplace_back(::tflite::CreateBuffer(
        builder_, builder_.CreateVector(buffer.data(), buffer.size())));
  } else {
    size_t offset = base::bits::AlignUp(buffer_data_.size(), kWeightsAlignment);
    CHECK_GT(offset, 1u);
    size_t padding = offset - buffer_data_.size();
    std::fill_n(std::back_inserter(buffer_data_), padding, 0);
    CHECK_EQ(buffer_data_.size() % kWeightsAlignment, 0u);

    std::ranges::copy(buffer, std::back_inserter(buffer_data_));
    buffers_.emplace_back(
        ::tflite::CreateBuffer(builder_, /*data=*/0, offset, buffer.size()));
  }

  // The index of buffer is referenced by tensors.
  return buffer_index;
}

template <typename DataType>
  requires internal::IsSupportedTensorType<DataType>
GraphBuilderTflite::TensorIndex GraphBuilderTflite::SerializeTensorWithBuffer(
    base::span<const DataType> buffer,
    base::span<const int32_t> dimensions) {
  base::span<const uint8_t> buffer_span;
  if constexpr (std::floating_point<DataType>) {
    // Floating point types do not have unique object representations, but
    // this code appears to be using a byte span to type-erase, which is fine.
    buffer_span = base::as_byte_span(base::allow_nonunique_obj, buffer);
  } else {
    buffer_span = base::as_byte_span(buffer);
  }
  const BufferIndex buffer_index = SerializeBuffer(buffer_span);

  // Create `tflite::Tensor` with the dimensions and the index of buffer.
  const TensorIndex tensor_index =
      base::checked_cast<TensorIndex>(tensors_.size());
  tensors_.emplace_back(::tflite::CreateTensor(
      builder_, builder_.CreateVector<int32_t>(dimensions),
      TensorTypeMap<DataType>::value, buffer_index));

  return tensor_index;
}

GraphBuilderTflite::TensorIndex GraphBuilderTflite::SerializeTemporaryTensor(
    base::span<const int32_t> dimensions,
    ::tflite::TensorType tensor_type,
    QuantizateParametersOffset quantize_params) {
  const TensorIndex temporary_tensor_index =
      base::checked_cast<TensorIndex>(tensors_.size());
  tensors_.emplace_back(::tflite::CreateTensor(
      builder_, builder_.CreateVector<int32_t>(dimensions), tensor_type,
      /*buffer=*/0, /*name=*/0, quantize_params));

  return temporary_tensor_index;
}

GraphBuilderTflite::OperatorCodeIndex GraphBuilderTflite::GetOperatorCodeIndex(
    ::tflite::BuiltinOperator code,
    int32_t version) {
  // New builtin operators, whose operator code is larger than 127, can not be
  // assigned to the `deprecated_code` field. In such cases, the value of the
  // `code` field should be used for the builtin operator code, the value 127
  // will be the value of the `deprecated_code`.
  const ::tflite::BuiltinOperator deprecated_code = std::min(
      code, ::tflite::BuiltinOperator_PLACEHOLDER_FOR_GREATER_OP_CODES);

  auto operator_code_index =
      base::checked_cast<OperatorCodeIndex>(operator_codes_.size());
  operator_codes_.push_back(::tflite::CreateOperatorCode(
      builder_, base::checked_cast<int8_t>(deprecated_code),
      /*custom_code=*/0, version, code));

  // The type of operation is determined by the index into the list of the valid
  // OperatorCodes.
  return operator_code_index;
}

const mojom::Operand& GraphBuilderTflite::GetOperand(
    OperandId operand_id) const {
  return *graph_info_->operands.at(operand_id.value());
}

template <typename DataType>
  requires internal::IsSupportedTensorType<DataType>
base::span<const DataType> GraphBuilderTflite::GetConstantValue(
    OperandId operand_id) {
  const mojom::Operand& operand = GetOperand(operand_id);
  CHECK_EQ(operand.kind, mojom::Operand::Kind::kConstant);
  CHECK_EQ(TensorTypeMap<DataType>::value,
           OperandDataTypeToTFLite(operand.descriptor.data_type()));

  auto it = constant_operands_->find(operand_id);
  CHECK(it != constant_operands_->end());
  const DataType* typed_value =
      reinterpret_cast<const DataType*>(it->second->ByteSpan().data());

  return UNSAFE_BUFFERS(
      base::span(typed_value, operand.descriptor.NumberOfElements()));
}

auto GraphBuilderTflite::SerializeUnaryOperation(
    ::tflite::BuiltinOperator code,
    TensorIndex input_tensor_index,
    TensorIndex output_tensor_index,
    ::tflite::BuiltinOptions builtin_options_type,
    flatbuffers::Offset<void> builtin_options) -> OperatorOffset {
  CHECK_EQ(builtin_options_type == ::tflite::BuiltinOptions_NONE,
           builtin_options.IsNull());

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const OperatorCodeIndex operator_code_index = GetOperatorCodeIndex(code);
  const std::array<TensorIndex, 1> op_inputs = {input_tensor_index};
  const std::array<TensorIndex, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index,
      builder_.CreateVector<TensorIndex>(op_inputs),
      builder_.CreateVector<TensorIndex>(op_outputs), builtin_options_type,
      builtin_options);
}

auto GraphBuilderTflite::SerializeCastOperation(
    TensorIndex input_tensor_index,
    ::tflite::TensorType input_tensor_type,
    TensorIndex output_tensor_index,
    ::tflite::TensorType output_tensor_type) -> OperatorOffset {
  const std::array<TensorIndex, 1> op_inputs = {input_tensor_index};
  const std::array<TensorIndex, 1> op_outputs = {output_tensor_index};

  if (input_tensor_type == ::tflite::TensorType_FLOAT16 &&
      output_tensor_type == ::tflite::TensorType_FLOAT32) {
    // TFLite expects the DEQUANTIZE operator to be used to pass float16
    // weights to float32 operators, but WebNN represents this with the cast
    // operator.
    return ::tflite::CreateOperator(
        builder_, GetOperatorCodeIndex(::tflite::BuiltinOperator_DEQUANTIZE),
        builder_.CreateVector<TensorIndex>(op_inputs),
        builder_.CreateVector<TensorIndex>(op_outputs));
  }

  const auto cast_options = ::tflite::CreateCastOptions(
      builder_, input_tensor_type, output_tensor_type);
  return ::tflite::CreateOperator(
      builder_, GetOperatorCodeIndex(::tflite::BuiltinOperator_CAST),
      builder_.CreateVector<TensorIndex>(op_inputs),
      builder_.CreateVector<TensorIndex>(op_outputs),
      ::tflite::BuiltinOptions_CastOptions, cast_options.Union());
}

auto GraphBuilderTflite::SerializeSquareOperation(
    TensorIndex input_tensor_index,
    ::tflite::TensorType input_tensor_type,
    TensorIndex output_tensor_index) -> OperatorOffset {
  // TFLite only supports float32 for the built-in square operator,
  // everything else needs to use a fallback.
  if (input_tensor_type == ::tflite::TensorType_FLOAT32) {
    return SerializeUnaryOperation(::tflite::BuiltinOperator_SQUARE,
                                   input_tensor_index, output_tensor_index);
  } else if (input_tensor_type == ::tflite::TensorType_INT32) {
    return SerializeBinaryOperation(::tflite::BuiltinOperator_MUL,
                                    input_tensor_index, input_tensor_index,
                                    output_tensor_index);
  } else {
    NOTREACHED() << "Unsupported data type for square";
  }
}

auto GraphBuilderTflite::SerializeSquareRootOperation(
    TensorIndex input_tensor_index,
    ::tflite::TensorType input_tensor_type,
    TensorIndex output_tensor_index)
    -> base::expected<OperatorOffset, std::string> {
  if (input_tensor_type == ::tflite::TensorType_FLOAT32) {
    return SerializeUnaryOperation(::tflite::BuiltinOperator_SQRT,
                                   input_tensor_index, output_tensor_index);
  } else {
    NOTREACHED() << "Unsupported data type for sqrt";
  }
}

auto GraphBuilderTflite::SerializeBinaryOperation(
    ::tflite::BuiltinOperator code,
    TensorIndex lhs_tensor_index,
    TensorIndex rhs_tensor_index,
    TensorIndex output_tensor_index) -> OperatorOffset {
  const OperatorCodeIndex operator_code_index = GetOperatorCodeIndex(code);
  const std::array<TensorIndex, 2> op_inputs = {lhs_tensor_index,
                                                rhs_tensor_index};
  const std::array<TensorIndex, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index,
      builder_.CreateVector<TensorIndex>(op_inputs),
      builder_.CreateVector<TensorIndex>(op_outputs));
}

auto GraphBuilderTflite::SerializeConcatOperation(
    base::span<const TensorIndex> input_tensor_indices,
    TensorIndex output_tensor_index,
    uint32_t axis) -> OperatorOffset {
  // Create `tflite::ConcatenationOptions` with axis.
  const auto concat_options =
      ::tflite::CreateConcatenationOptions(builder_, axis);

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const OperatorCodeIndex operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_CONCATENATION);
  const std::array<TensorIndex, 1> operator_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index,
      builder_.CreateVector<TensorIndex>(input_tensor_indices),
      builder_.CreateVector<TensorIndex>(operator_outputs),
      ::tflite::BuiltinOptions_ConcatenationOptions, concat_options.Union());
}

GraphBuilderTflite::TensorIndex
GraphBuilderTflite::SerializeDequantizeOperation(
    TensorIndex input_tensor_index,
    base::span<const int32_t> input_dimensions) {
  const TensorIndex output_tensor_index =
      SerializeTemporaryTensor(input_dimensions, ::tflite::TensorType_FLOAT32);
  const OperatorCodeIndex operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_DEQUANTIZE);
  const std::array<TensorIndex, 1> op_inputs = {input_tensor_index};
  const std::array<TensorIndex, 1> op_outputs = {output_tensor_index};
  operators_.emplace_back(
      ::tflite::CreateOperator(builder_, operator_code_index,
                               builder_.CreateVector<TensorIndex>(op_inputs),
                               builder_.CreateVector<TensorIndex>(op_outputs)));

  return output_tensor_index;
}

auto GraphBuilderTflite::SerializeMatmulOperation(
    TensorIndex a_tensor_index,
    TensorIndex b_tensor_index,
    TensorIndex output_tensor_index) -> OperatorOffset {
  const auto matmul_options =
      ::tflite::CreateBatchMatMulOptions(builder_, /*adj_x=*/false,
                                         /*adj_y=*/false);
  const OperatorCodeIndex operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_BATCH_MATMUL);
  const std::array<TensorIndex, 2> op_inputs = {a_tensor_index, b_tensor_index};
  const std::array<TensorIndex, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index,
      builder_.CreateVector<TensorIndex>(op_inputs),
      builder_.CreateVector<TensorIndex>(op_outputs),
      ::tflite::BuiltinOptions_BatchMatMulOptions, matmul_options.Union());
}

auto GraphBuilderTflite::SerializeLinearOperation(
    base::span<const int32_t> input_dimensions,
    ::tflite::TensorType input_tensor_type,
    TensorIndex input_tensor_index,
    TensorIndex output_tensor_index,
    float alpha,
    float beta) -> OperatorOffset {
  // Emulate a linear operation whose calculation follows the expression `alpha
  // * x + beta`.
  const TensorIndex alpha_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{alpha},
      /*dimensions=*/{});
  const TensorIndex output_tensor_index_of_mul =
      SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_MUL, input_tensor_index, alpha_tensor_index,
      output_tensor_index_of_mul));

  const TensorIndex beta_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{beta},
      /*dimensions=*/{});
  return SerializeBinaryOperation(::tflite::BuiltinOperator_ADD,
                                  beta_tensor_index, output_tensor_index_of_mul,
                                  output_tensor_index);
}

auto GraphBuilderTflite::SerializeNormalizationOperation(
    base::span<const int32_t> input_dimensions,
    ::tflite::TensorType input_tensor_type,
    TensorIndex input_tensor_index,
    TensorIndex output_tensor_index,
    TensorIndex mean_tensor_index,
    TensorIndex variance_tensor_index,
    float epsilon,
    std::optional<TensorIndex> scale_tensor_index,
    std::optional<TensorIndex> bias_tensor_index) -> OperatorOffset {
  // Emulate normalization follows the expression `Scale * ((Input - Mean) /
  // sqrt(Variance + Epsilon)) + Bias`
  //
  // Serialize the subtraction operation for expression `Input - Mean`.
  CHECK_EQ(input_tensor_type, ::tflite::TensorType_FLOAT32);
  const TensorIndex output_tensor_index_of_sub =
      SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_SUB, input_tensor_index, mean_tensor_index,
      output_tensor_index_of_sub));

  // Serialize the subexpression `sqrt(Variance + Epsilon)`.
  const TensorIndex epsilon_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{epsilon},
      /*dimensions=*/{});
  const TensorIndex output_tensor_index_of_add =
      SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_ADD, variance_tensor_index,
      epsilon_tensor_index, output_tensor_index_of_add));
  const TensorIndex output_tensor_index_of_sqrt =
      SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeUnaryOperation(
      ::tflite::BuiltinOperator_SQRT, output_tensor_index_of_add,
      output_tensor_index_of_sqrt));

  // Serialize the intermediate expression `Scale * (output_tensor_of_sub /
  // output_tensor_of_sqrt)`.
  TensorIndex output_tensor_index_of_div = output_tensor_index;
  if (scale_tensor_index || bias_tensor_index) {
    output_tensor_index_of_div =
        SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  }
  OperatorOffset normalization_offset = SerializeBinaryOperation(
      ::tflite::BuiltinOperator_DIV, output_tensor_index_of_sub,
      output_tensor_index_of_sqrt, output_tensor_index_of_div);
  TensorIndex output_tensor_index_of_mul = output_tensor_index_of_div;
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
    TensorIndex input_tensor_index,
    TensorIndex output_tensor_index,
    base::span<const int32_t> axes,
    bool keep_dimensions) -> OperatorOffset {
  const std::array<int32_t, 1> axes_tensor_shape = {
      base::checked_cast<int32_t>(axes.size())};
  const TensorIndex axes_tensor_index =
      SerializeTensorWithBuffer<int32_t>(axes, axes_tensor_shape);

  const auto reduce_options =
      ::tflite::CreateReducerOptions(builder_, keep_dimensions);
  const OperatorCodeIndex operator_code_index =
      GetOperatorCodeIndex(operator_code);
  const std::array<TensorIndex, 2> op_inputs = {input_tensor_index,
                                                axes_tensor_index};
  const std::array<TensorIndex, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index,
      builder_.CreateVector<TensorIndex>(op_inputs),
      builder_.CreateVector<TensorIndex>(op_outputs),
      ::tflite::BuiltinOptions_ReducerOptions, reduce_options.Union());
}

auto GraphBuilderTflite::SerializeReshapeOperation(
    TensorIndex input_tensor_index,
    TensorIndex output_tensor_index,
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
    TensorIndex input_tensor_index,
    TensorIndex output_tensor_index,
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
  const TensorIndex starts_tensor_index = SerializeTensorWithBuffer<int32_t>(
      std::move(slice_starts), starts_and_sizes_shape);

  // Serialize the number of elements to slice each input dimension.
  const TensorIndex sizes_tensor_index = SerializeTensorWithBuffer<int32_t>(
      std::move(slice_sizes), starts_and_sizes_shape);

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const OperatorCodeIndex operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_SLICE);
  const std::array<TensorIndex, 3> op_inputs = {
      input_tensor_index, starts_tensor_index, sizes_tensor_index};
  const std::array<TensorIndex, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index,
      builder_.CreateVector<TensorIndex>(op_inputs),
      builder_.CreateVector<TensorIndex>(op_outputs));
}

auto GraphBuilderTflite::SerializeTransposeOperation(
    TensorIndex input_tensor_index,
    TensorIndex output_tensor_index,
    base::span<const int32_t> input_shape,
    base::span<const uint32_t> permutation) -> OperatorOffset {
  if (input_shape.empty()) {
    CHECK(permutation.empty());
    return SerializeIdentityOperation(input_tensor_index, output_tensor_index,
                                      input_shape);
  }
  const std::array<int32_t, 1> permutation_shape = {
      base::checked_cast<int32_t>(permutation.size())};
  const TensorIndex permutation_tensor_index =
      SerializeTensorWithBuffer<uint32_t>(permutation, permutation_shape);

  const OperatorCodeIndex operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_TRANSPOSE);
  const std::array<TensorIndex, 2> op_inputs = {input_tensor_index,
                                                permutation_tensor_index};
  const std::array<TensorIndex, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index,
      builder_.CreateVector<TensorIndex>(op_inputs),
      builder_.CreateVector<TensorIndex>(op_outputs));
}

auto GraphBuilderTflite::SerializeTFLiteScatterND(
    base::span<const int32_t> input_shapes,
    TensorIndex indices_tensor_index,
    TensorIndex updates_tensor_index,
    TensorIndex output_tensor_index) -> OperatorOffset {
  const TensorIndex input_shape_tensor_index =
      SerializeTensorWithBuffer<int32_t>(
          /*buffer=*/input_shapes, /*dimensions=*/std::array<int32_t, 1>(
              {base::checked_cast<int32_t>(input_shapes.size())}));

  const OperatorCodeIndex operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_SCATTER_ND);
  const std::array<TensorIndex, 3> op_inputs = {
      indices_tensor_index, updates_tensor_index, input_shape_tensor_index};
  const std::array<TensorIndex, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index,
      builder_.CreateVector<TensorIndex>(op_inputs),
      builder_.CreateVector<TensorIndex>(op_outputs));
}

auto GraphBuilderTflite::SerializeReverseOperation(
    TensorIndex input_tensor_index,
    base::span<const int32_t> axes,
    TensorIndex output_tensor_index) -> OperatorOffset {
  const TensorIndex axes_tensor_index = SerializeTensorWithBuffer<int32_t>(
      /*buffer=*/axes,
      /*dimensions=*/std::array<int32_t, 1>(
          {base::checked_cast<int32_t>(axes.size())}));
  const OperatorCodeIndex operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_REVERSE_V2);
  const std::array<TensorIndex, 2> op_inputs = {input_tensor_index,
                                                axes_tensor_index};
  const std::array<TensorIndex, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index,
      builder_.CreateVector<TensorIndex>(op_inputs),
      builder_.CreateVector<TensorIndex>(op_outputs));
}

auto GraphBuilderTflite::SerializeWhereOperation(
    TensorIndex condition_tensor_index,
    TensorIndex true_tensor_index,
    TensorIndex false_tensor_index,
    TensorIndex output_tensor_index) -> OperatorOffset {
  // TFLite SELECT_V2 builtin operator supports broadcastable shapes between
  // `condition`, `true` and `false` operand.
  const OperatorCodeIndex operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_SELECT_V2);
  const std::array<TensorIndex, 3> op_inputs = {
      condition_tensor_index, true_tensor_index, false_tensor_index};
  const std::array<TensorIndex, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index,
      builder_.CreateVector<TensorIndex>(op_inputs),
      builder_.CreateVector<TensorIndex>(op_outputs));
}

auto GraphBuilderTflite::InsertPadOperation(const TensorInfo& input_tensor_info,
                                            base::span<const uint32_t> paddings)
    -> base::expected<TensorIndex, std::string> {
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

  const TensorIndex output_tensor_index =
      base::checked_cast<TensorIndex>(tensors_.size());
  tensors_.emplace_back(::tflite::CreateTensor(
      builder_, builder_.CreateVector<int32_t>(output_shape),
      input_tensor_info.data_type));

  // TfLite padding is a signed integer tensor array filled with pre and post
  // padding. For NHWC input layout, the sequence will be [[0, 0],
  // [beginning_height, ending_height], [beginning_width, ending_width], [0,
  // 0]].
  std::array<int32_t, 8> tflite_paddings = {};
  std::ranges::copy(paddings, tflite_paddings.begin() + 2);

  // The shape of padding is [n, 2], where n is the rank of input as described
  // here https://www.tensorflow.org/mlir/tfl_ops#tflmirror_pad_tflmirrorpadop.
  std::array<int32_t, 2> paddings_shape = {
      base::checked_cast<int32_t>(padding_rank), 2};
  const TensorIndex padding_tensor_index = SerializeTensorWithBuffer<int32_t>(
      std::move(tflite_paddings), std::move(paddings_shape));

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const OperatorCodeIndex operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_PAD);
  std::array<TensorIndex, 2> op_inputs = {input_tensor_info.index,
                                          padding_tensor_index};
  const std::array<TensorIndex, 1> op_outputs = {output_tensor_index};
  operators_.emplace_back(
      ::tflite::CreateOperator(builder_, operator_code_index,
                               builder_.CreateVector<TensorIndex>(op_inputs),
                               builder_.CreateVector<TensorIndex>(op_outputs)));

  return output_tensor_index;
}

GraphBuilderTflite::TensorIndex GraphBuilderTflite::InsertTransposeOperation(
    const TensorInfo& input_tensor_info,
    base::span<const uint32_t> permutation) {
  // Create `tflite::Tensor` for the output operand of Transpose operator with
  // the dimensions and tensor data type.
  const size_t input_rank = input_tensor_info.dimensions.size();
  CHECK_EQ(permutation.size(), input_rank);
  base::FixedArray<int32_t> output_shape(input_rank);
  for (size_t i = 0; i < input_rank; ++i) {
    output_shape[i] = input_tensor_info.dimensions[permutation[i]];
  }
  const TensorIndex output_tensor_index =
      SerializeTemporaryTensor(output_shape, input_tensor_info.data_type);
  operators_.emplace_back(
      SerializeTransposeOperation(input_tensor_info.index, output_tensor_index,
                                  input_tensor_info.dimensions, permutation));

  return output_tensor_index;
}

GraphBuilderTflite::TensorIndex GraphBuilderTflite::SerializeSubGraphPowMul(
    base::span<const int32_t> input_dimensions,
    ::tflite::TensorType input_tensor_type,
    TensorIndex input_tensor_index,
    int pow_exponent,
    float mul_alpha) {
  // TFLite has a special optimization for broadcasting the POW operator with
  // an integer exponent to any dimension, but the MUL operator only broadcasts
  // to 6D.
  CHECK_LE(input_dimensions.size(), 6u);

  const TensorIndex output_tensor_index_of_pow =
      SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  const TensorIndex pow_exponent_tensor_index =
      SerializeTensorWithBuffer<float>(
          /*buffer=*/std::array<float, 1>{static_cast<float>(pow_exponent)},
          /*dimensions=*/{});
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_POW, input_tensor_index,
      pow_exponent_tensor_index, output_tensor_index_of_pow));

  const TensorIndex output_tensor_index_of_mul =
      SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  const TensorIndex mul_alpha_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{mul_alpha},
      /*dimensions=*/{});
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_MUL, output_tensor_index_of_pow,
      mul_alpha_tensor_index, output_tensor_index_of_mul));

  return output_tensor_index_of_mul;
}

auto GraphBuilderTflite::SerializeArgMinMax(const mojom::ArgMinMax& arg_min_max)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.arg_min_max_input.Supports(
      GetOperand(arg_min_max.input_operand_id).descriptor));
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
  const TensorIndex axis_tensor_index =
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
  const TensorIndex output_tensor_index =
      SerializeOutputTensorInfo(arg_min_max.output_operand_id).index;
  const OperatorCodeIndex operator_code_index =
      GetOperatorCodeIndex(operator_code);
  const std::array<TensorIndex, 2> op_inputs = {input_tensor_info.index,
                                                axis_tensor_index};
  const std::array<TensorIndex, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index,
      builder_.CreateVector<TensorIndex>(op_inputs),
      builder_.CreateVector<TensorIndex>(op_outputs), builtin_options_type,
      builtin_options);
}

auto GraphBuilderTflite::SerializeBatchNormalization(
    const mojom::BatchNormalization& batch_normalization)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.batch_normalization_input.Supports(
      GetOperand(batch_normalization.input_operand_id).descriptor));
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
  CHECK(context_properties_.data_type_limits.batch_normalization_mean.Supports(
      GetOperand(batch_normalization.mean_operand_id).descriptor));
  ASSIGN_OR_RETURN(
      const TensorInfo& mean_tensor_info,
      SerializeInputTensorInfo(batch_normalization.mean_operand_id));
  const TensorIndex reshape_mean_tensor_index =
      SerializeTemporaryTensor(new_shape, input_tensor_type);
  operators_.emplace_back(SerializeReshapeOperation(
      mean_tensor_info.index, reshape_mean_tensor_index, new_shape));

  // Reshape the 1-D tensor of the variance operand to the new shape.
  CHECK(context_properties_.data_type_limits.batch_normalization_mean.Supports(
      GetOperand(batch_normalization.variance_operand_id).descriptor));
  ASSIGN_OR_RETURN(
      const TensorInfo& variance_tensor_info,
      SerializeInputTensorInfo(batch_normalization.variance_operand_id));
  const TensorIndex reshape_variance_tensor_index =
      SerializeTemporaryTensor(new_shape, input_tensor_type);
  operators_.emplace_back(SerializeReshapeOperation(
      variance_tensor_info.index, reshape_variance_tensor_index, new_shape));

  // Reshape the 1-D tensor of the scale operand to the new shape if needed.
  std::optional<TensorIndex> reshape_scale_tensor_index;
  if (batch_normalization.scale_operand_id) {
    CHECK(
        context_properties_.data_type_limits.batch_normalization_mean.Supports(
            GetOperand(*batch_normalization.scale_operand_id).descriptor));
    ASSIGN_OR_RETURN(
        const TensorInfo& scale_tensor_info,
        SerializeInputTensorInfo(*batch_normalization.scale_operand_id));
    reshape_scale_tensor_index =
        SerializeTemporaryTensor(new_shape, input_tensor_type);
    operators_.emplace_back(SerializeReshapeOperation(
        scale_tensor_info.index, *reshape_scale_tensor_index, new_shape));
  }

  // Reshape the 1-D tensor of the bias operand to the new shape if needed.
  std::optional<TensorIndex> reshape_bias_tensor_index;
  if (batch_normalization.bias_operand_id) {
    CHECK(
        context_properties_.data_type_limits.batch_normalization_mean.Supports(
            GetOperand(*batch_normalization.bias_operand_id).descriptor));
    ASSIGN_OR_RETURN(
        const TensorInfo& bias_tensor_info,
        SerializeInputTensorInfo(*batch_normalization.bias_operand_id));
    reshape_bias_tensor_index =
        SerializeTemporaryTensor(new_shape, input_tensor_type);
    operators_.emplace_back(SerializeReshapeOperation(
        bias_tensor_info.index, *reshape_bias_tensor_index, new_shape));
  }

  const TensorIndex output_tensor_index =
      SerializeOutputTensorInfo(batch_normalization.output_operand_id).index;
  return SerializeNormalizationOperation(
      new_shape, input_tensor_type, input_tensor_info.index,
      output_tensor_index, reshape_mean_tensor_index,
      reshape_variance_tensor_index, batch_normalization.epsilon,
      reshape_scale_tensor_index, reshape_bias_tensor_index);
}

template <typename DataType>
auto GraphBuilderTflite::SerializeSubGraphMaxMin(
    const TensorInfo& input_tensor_info,
    TensorIndex output_tensor_index,
    base::span<const DataType> min_values,
    base::span<const DataType> max_values) -> OperatorOffset {
  const TensorIndex min_value_tensor_index =
      SerializeTensorWithBuffer<DataType>(
          /*buffer=*/min_values,
          /*dimensions=*/std::array<int32_t, 1>{
              base::checked_cast<int32_t>(min_values.size())});
  const TensorIndex output_tensor_index_of_max = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_MAXIMUM, input_tensor_info.index,
      min_value_tensor_index, output_tensor_index_of_max));

  const TensorIndex max_value_tensor_index =
      SerializeTensorWithBuffer<DataType>(
          /*buffer=*/max_values,
          /*dimensions=*/std::array<int32_t, 1>{
              base::checked_cast<int32_t>(max_values.size())});
  return SerializeBinaryOperation(::tflite::BuiltinOperator_MINIMUM,
                                  output_tensor_index_of_max,
                                  max_value_tensor_index, output_tensor_index);
}

auto GraphBuilderTflite::SerializeClamp(const mojom::Clamp& clamp)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.clamp_input.Supports(
      GetOperand(clamp.input_operand_id).descriptor));

  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(clamp.input_operand_id));
  const TensorIndex output_tensor_index =
      SerializeOutputTensorInfo(clamp.output_operand_id).index;

  const float min_value = clamp.min_value;
  const float max_value = clamp.max_value;
  std::optional<::tflite::BuiltinOperator> operator_code =
      GetClampOperatorCode(min_value, max_value);
  if (operator_code.has_value()) {
    return SerializeUnaryOperation(*operator_code, input_tensor_info.index,
                                   output_tensor_index);
  } else {
    // Emulate clamp operation with min and max.
    return SerializeSubGraphMaxMin<float>(
        input_tensor_info, output_tensor_index, std::array<float, 1>{min_value},
        std::array<float, 1>{max_value});
  }
}

auto GraphBuilderTflite::SerializeConcat(const mojom::Concat& concat)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(std::ranges::all_of(
      concat.input_operand_ids, [&](OperandId input_operand_id) {
        return context_properties_.data_type_limits.concat_inputs.Supports(
            GetOperand(input_operand_id).descriptor);
      }));

  std::optional<TensorInfo> quantized_output =
      CanFuseQuantizeAndGetOutput(concat);
  // TODO(crbug.com/369649350): Support float16 without dequantize operator.
  base::FixedArray<TensorIndex> operator_inputs_index(
      concat.input_operand_ids.size());
  for (size_t i = 0; i < concat.input_operand_ids.size(); ++i) {
    ASSIGN_OR_RETURN(
        const TensorInfo& input_tensor_info,
        SerializeInputTensorInfo(
            concat.input_operand_ids[i],
            /*quantize_params=*/0,
            /*operation_supports_float16=*/false,
            /*fuse_dequantize_quantize=*/quantized_output.has_value()));
    operator_inputs_index[i] = input_tensor_info.index;
  }
  TensorIndex output_tensor_index =
      quantized_output
          ? quantized_output->index
          : SerializeOutputTensorInfo(concat.output_operand_id).index;

  return SerializeConcatOperation(operator_inputs_index, output_tensor_index,
                                  concat.axis);
}

auto GraphBuilderTflite::SerializeCumulativeSum(
    const mojom::CumulativeSum& cumulative_sum)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.cumulative_sum_input.Supports(
      GetOperand(cumulative_sum.input_operand_id).descriptor));

  // The axis is validated by ValidateCumulativeSumAndInferOutput(), so the axis
  // doesn't overflow.
  const TensorIndex axis_tensor_index = SerializeTensorWithBuffer<int32_t>(
      /*buffer=*/std::array<int32_t, 1>{base::checked_cast<int32_t>(
          cumulative_sum.axis)},
      /*dimensions=*/{});

  const auto cumulative_sum_options = ::tflite::CreateCumsumOptions(
      builder_, cumulative_sum.exclusive, cumulative_sum.reversed);

  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(cumulative_sum.input_operand_id));
  const TensorIndex output_tensor_index =
      SerializeOutputTensorInfo(cumulative_sum.output_operand_id).index;
  const OperatorCodeIndex operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_CUMSUM);
  const std::array<TensorIndex, 2> op_inputs = {input_tensor_info.index,
                                                axis_tensor_index};
  const std::array<TensorIndex, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index,
      builder_.CreateVector<TensorIndex>(op_inputs),
      builder_.CreateVector<TensorIndex>(op_outputs),
      ::tflite::BuiltinOptions_CumsumOptions, cumulative_sum_options.Union());
}

auto GraphBuilderTflite::SerializeConv2d(const mojom::Conv2d& conv2d)
    -> base::expected<OperatorOffset, std::string> {
  const mojom::Operand& input_operand = GetOperand(conv2d.input_operand_id);
  const mojom::Operand& filter_operand = GetOperand(conv2d.filter_operand_id);
  switch (conv2d.kind) {
    case mojom::Conv2d::Kind::kDirect:
      // TODO(crbug.com/328733319): Support other tensor data types.
      CHECK(context_properties_.data_type_limits.conv2d_input.SupportsAll(
          {input_operand.descriptor, filter_operand.descriptor}));

      // TFLite internally performs a truncating cast. See crbug.com/384999508.
      if (!base::IsValueInRangeForNumericType<int16_t>(
              conv2d.dilations->height) ||
          !base::IsValueInRangeForNumericType<int16_t>(
              conv2d.dilations->width)) {
        return base::unexpected(
            "Dilation width and height must fit within the int16 range");
      }
      break;
    case mojom::Conv2d::Kind::kTransposed:
      // TODO(crbug.com/328733319): Support other tensor data types.
      CHECK(context_properties_.data_type_limits.conv_transpose2d_input
                .SupportsAll(
                    {input_operand.descriptor, filter_operand.descriptor}));

      // TODO(crbug.com/364348906): Support dilations and groups parameter for
      // convTranspose2d
      if (conv2d.dilations->height != 1 || conv2d.dilations->width != 1 ||
          conv2d.groups != 1) {
        return base::unexpected(
            "convTranspose2d doesn't support dilations and groups.");
      }
      break;
  }

  // TFLite internally performs a truncating cast. See crbug.com/384999508
  if (!base::IsValueInRangeForNumericType<int16_t>(conv2d.strides->height) ||
      !base::IsValueInRangeForNumericType<int16_t>(conv2d.strides->width)) {
    return base::unexpected(
        "Stride width and height must fit within the int16 range");
  }

  // Get tflite padding mode with the size2d of input, filter, dilation.
  const auto& input_shape = input_operand.descriptor.shape();
  const uint32_t input_channels = input_shape[3];
  const mojom::Operand& output_operand = GetOperand(conv2d.output_operand_id);
  const auto& output_shape = output_operand.descriptor.shape();
  CHECK_EQ(output_shape.size(), 4u);
  const uint32_t output_channels = output_shape[3];
  const webnn::Size2d<uint32_t> input_size2d = {.height = input_shape[1],
                                                .width = input_shape[2]};
  // For nhwc input layout, the default filter layout is ohwi for
  // regular/transpose conv2d and ihwo for depthwise conv2d.
  const auto& filter_shape = filter_operand.descriptor.shape();
  const webnn::Size2d<uint32_t> filter_size2d = {.height = filter_shape[1],
                                                 .width = filter_shape[2]};
  ASSIGN_OR_RETURN(
      TfLitePadding padding_mode,
      GetTfLitePaddingMode(*conv2d.padding, input_size2d, filter_size2d,
                           *conv2d.strides, *conv2d.dilations,
                           conv2d.kind == mojom::Conv2d::Kind::kTransposed));

  std::optional<TensorInfo> quantized_output =
      CanFuseQuantizeAndGetOutput(conv2d);
  const bool fuse_dequantize = quantized_output.has_value();

  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(
                       conv2d.input_operand_id, /*quantize_params=*/0,
                       /*operation_supports_float16=*/false, fuse_dequantize));
  // Insert a Pad operator before TfLite Conv2d if needed for explicit padding.
  std::optional<TensorIndex> explicit_pad_index;
  if (padding_mode.paddings) {
    ASSIGN_OR_RETURN(
        explicit_pad_index,
        InsertPadOperation(input_tensor_info, padding_mode.paddings.value()));
  }

  // If there is no bias operand, serialize a empty buffer with the size of
  // output channel.
  TensorIndex bias_index;
  if (conv2d.bias_operand_id) {
    const mojom::Operand& bias_operand = GetOperand(*conv2d.bias_operand_id);
    if (conv2d.kind == mojom::Conv2d::Kind::kDirect) {
      CHECK(context_properties_.data_type_limits.conv2d_bias.Supports(
          bias_operand.descriptor));
    } else {
      CHECK(context_properties_.data_type_limits.conv_transpose2d_bias.Supports(
          bias_operand.descriptor));
    }

    ASSIGN_OR_RETURN(
        const TensorInfo& bias_tensor_info,
        SerializeInputTensorInfo(*conv2d.bias_operand_id,
                                 /*quantize_params=*/0,
                                 /*operation_supports_float16=*/false,
                                 fuse_dequantize));
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
                   SerializeInputTensorInfo(
                       conv2d.filter_operand_id, /*quantize_params=*/0,
                       /*operation_supports_float16=*/false, fuse_dequantize));
  std::vector<TensorIndex> op_inputs;
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
    const TensorIndex output_shape_tensor_index =
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

  TensorIndex output_tensor_index =
      quantized_output
          ? quantized_output->index
          : SerializeOutputTensorInfo(conv2d.output_operand_id).index;

  const OperatorCodeIndex operator_code_index =
      GetOperatorCodeIndex(operator_kind);
  const std::array<TensorIndex, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index,
      builder_.CreateVector<TensorIndex>(op_inputs),
      builder_.CreateVector<TensorIndex>(op_outputs), builtin_options_type,
      builtin_options);
}

auto GraphBuilderTflite::SerializeElementWiseBinary(
    const mojom::ElementWiseBinary& op)
    -> base::expected<OperatorOffset, std::string> {
  std::optional<TensorInfo> quantized_output;
  const OperandDescriptor& lhs_operand_descriptor =
      GetOperand(op.lhs_operand_id).descriptor;
  const OperandDescriptor& rhs_operand_descriptor =
      GetOperand(op.rhs_operand_id).descriptor;
  ::tflite::BuiltinOperator code;
  switch (op.kind) {
    case mojom::ElementWiseBinary::Kind::kAdd:
      CHECK(context_properties_.data_type_limits.add_input.SupportsAll(
          {lhs_operand_descriptor, rhs_operand_descriptor}));
      code = ::tflite::BuiltinOperator_ADD;
      quantized_output = CanFuseQuantizeAndGetOutput(op);
      break;
    case mojom::ElementWiseBinary::Kind::kSub:
      CHECK(context_properties_.data_type_limits.sub_input.SupportsAll(
          {lhs_operand_descriptor, rhs_operand_descriptor}));
      code = ::tflite::BuiltinOperator_SUB;
      quantized_output = CanFuseQuantizeAndGetOutput(op);
      break;
    case mojom::ElementWiseBinary::Kind::kMul:
      CHECK(context_properties_.data_type_limits.mul_input.SupportsAll(
          {lhs_operand_descriptor, rhs_operand_descriptor}));
      code = ::tflite::BuiltinOperator_MUL;
      quantized_output = CanFuseQuantizeAndGetOutput(op);
      break;
    case mojom::ElementWiseBinary::Kind::kDiv:
      CHECK(context_properties_.data_type_limits.div_input.SupportsAll(
          {lhs_operand_descriptor, rhs_operand_descriptor}));
      code = ::tflite::BuiltinOperator_DIV;
      break;
    case mojom::ElementWiseBinary::Kind::kMax:
      CHECK(context_properties_.data_type_limits.max_input.SupportsAll(
          {lhs_operand_descriptor, rhs_operand_descriptor}));
      code = ::tflite::BuiltinOperator_MAXIMUM;
      quantized_output = CanFuseQuantizeAndGetOutput(op);
      break;
    case mojom::ElementWiseBinary::Kind::kMin:
      CHECK(context_properties_.data_type_limits.min_input.SupportsAll(
          {lhs_operand_descriptor, rhs_operand_descriptor}));
      code = ::tflite::BuiltinOperator_MINIMUM;
      quantized_output = CanFuseQuantizeAndGetOutput(op);
      break;
    case mojom::ElementWiseBinary::Kind::kPow:
      CHECK(context_properties_.data_type_limits.pow_input.SupportsAll(
          {lhs_operand_descriptor, rhs_operand_descriptor}));
      code = ::tflite::BuiltinOperator_POW;
      break;
    case mojom::ElementWiseBinary::Kind::kEqual:
      CHECK(context_properties_.data_type_limits.equal_input.SupportsAll(
          {lhs_operand_descriptor, rhs_operand_descriptor}));
      code = ::tflite::BuiltinOperator_EQUAL;
      break;
    case mojom::ElementWiseBinary::Kind::kGreater:
      CHECK(context_properties_.data_type_limits.greater_input.SupportsAll(
          {lhs_operand_descriptor, rhs_operand_descriptor}));
      code = ::tflite::BuiltinOperator_GREATER;
      break;
    case mojom::ElementWiseBinary::Kind::kGreaterOrEqual:
      CHECK(context_properties_.data_type_limits.greater_or_equal_input
                .SupportsAll({lhs_operand_descriptor, rhs_operand_descriptor}));
      code = ::tflite::BuiltinOperator_GREATER_EQUAL;
      break;
    case mojom::ElementWiseBinary::Kind::kLesser:
      CHECK(context_properties_.data_type_limits.lesser_input.SupportsAll(
          {lhs_operand_descriptor, rhs_operand_descriptor}));
      code = ::tflite::BuiltinOperator_LESS;
      break;
    case mojom::ElementWiseBinary::Kind::kLesserOrEqual:
      CHECK(context_properties_.data_type_limits.lesser_or_equal_input
                .SupportsAll({lhs_operand_descriptor, rhs_operand_descriptor}));
      code = ::tflite::BuiltinOperator_LESS_EQUAL;
      break;
    case mojom::ElementWiseBinary::Kind::kNotEqual:
      CHECK(context_properties_.data_type_limits.not_equal_input.SupportsAll(
          {lhs_operand_descriptor, rhs_operand_descriptor}));
      code = ::tflite::BuiltinOperator_NOT_EQUAL;
      break;
    case mojom::ElementWiseBinary::Kind::kLogicalAnd:
      CHECK(context_properties_.data_type_limits.logical_and_input.SupportsAll(
          {lhs_operand_descriptor, rhs_operand_descriptor}));
      code = ::tflite::BuiltinOperator_LOGICAL_AND;
      break;
    case mojom::ElementWiseBinary::Kind::kLogicalOr:
      CHECK(context_properties_.data_type_limits.logical_or_input.SupportsAll(
          {lhs_operand_descriptor, rhs_operand_descriptor}));
      code = ::tflite::BuiltinOperator_LOGICAL_OR;
      break;
    case mojom::ElementWiseBinary::Kind::kLogicalXor:
      CHECK(context_properties_.data_type_limits.logical_xor_input.SupportsAll(
          {lhs_operand_descriptor, rhs_operand_descriptor}));
      // TFLite does not have a logical_xor operator. Since the inputs are
      // converted to bools below, we can use the not_equal operator to get the
      // same results as logical_xor.
      code = ::tflite::BuiltinOperator_NOT_EQUAL;
      break;
  }

  const bool fuse_dequantize = quantized_output.has_value();

  ASSIGN_OR_RETURN(const TensorInfo& lhs_tensor_info,
                   SerializeInputTensorInfo(
                       op.lhs_operand_id, /*quantize_params=*/0,
                       /*operation_supports_float16=*/false, fuse_dequantize));
  ASSIGN_OR_RETURN(const TensorInfo& rhs_tensor_info,
                   SerializeInputTensorInfo(
                       op.rhs_operand_id, /*quantize_params=*/0,
                       /*operation_supports_float16=*/false, fuse_dequantize));

  const TensorInfo output_tensor_info =
      SerializeOutputTensorInfo(op.output_operand_id);

  if (IsLogicalElementWiseBinary(op.kind)) {
    TensorIndex lhs_tensor_index = lhs_tensor_info.index;
    TensorIndex rhs_tensor_index = rhs_tensor_info.index;
    if (op.kind == mojom::ElementWiseBinary::Kind::kLogicalAnd ||
        op.kind == mojom::ElementWiseBinary::Kind::kLogicalOr ||
        op.kind == mojom::ElementWiseBinary::Kind::kLogicalXor) {
      // The data types of the inputs for these binary logical operators are
      // uint8 in WebNN. However, TFLite requires them to be bools, so we need
      // to cast the inputs to temporary bool tensors, perform the actual
      // operation.
      CHECK_EQ(lhs_tensor_info.data_type, ::tflite::TensorType_UINT8);
      lhs_tensor_index = SerializeTemporaryTensor(lhs_tensor_info.dimensions,
                                                  ::tflite::TensorType_BOOL);
      operators_.emplace_back(SerializeCastOperation(
          lhs_tensor_info.index,
          /*input_tensor_type=*/::tflite::TensorType_UINT8, lhs_tensor_index,
          /*output_tensor_type=*/::tflite::TensorType_BOOL));

      CHECK_EQ(rhs_tensor_info.data_type, ::tflite::TensorType_UINT8);
      rhs_tensor_index = SerializeTemporaryTensor(rhs_tensor_info.dimensions,
                                                  ::tflite::TensorType_BOOL);
      operators_.emplace_back(SerializeCastOperation(
          rhs_tensor_info.index,
          /*input_tensor_type=*/::tflite::TensorType_UINT8, rhs_tensor_index,
          /*output_tensor_type=*/::tflite::TensorType_BOOL));
    }

    // The data types of the output for all the binary logical operators are
    // uint8 in WebNN. However, TFLite returns bools, so we need to cast the
    // output to uint8.
    CHECK_EQ(output_tensor_info.data_type, ::tflite::TensorType_UINT8);
    TensorIndex output_tensor_bool_index = SerializeTemporaryTensor(
        output_tensor_info.dimensions, ::tflite::TensorType_BOOL);

    operators_.emplace_back(SerializeBinaryOperation(
        code, lhs_tensor_index, rhs_tensor_index, output_tensor_bool_index));

    // Cast the output from bool to uint8, since that's what WebNN expects back.
    return SerializeCastOperation(
        output_tensor_bool_index,
        /*input_tensor_type=*/::tflite::TensorType_BOOL,
        output_tensor_info.index,
        /*output_tensor_type=*/output_tensor_info.data_type);
  }

  return SerializeBinaryOperation(
      code, lhs_tensor_info.index, rhs_tensor_info.index,
      quantized_output ? quantized_output->index : output_tensor_info.index);
}

auto GraphBuilderTflite::SerializeElementWiseUnary(
    const mojom::ElementWiseUnary& op)
    -> base::expected<OperatorOffset, std::string> {
  ASSIGN_OR_RETURN(
      const TensorInfo& input_tensor_info,
      SerializeInputTensorInfo(op.input_operand_id, /*quantize_params=*/0,
                               /*operation_supports_float16=*/op.kind ==
                                   mojom::ElementWiseUnary::Kind::kCast));
  const TensorInfo output_tensor_info =
      SerializeOutputTensorInfo(op.output_operand_id, /*quantize_params=*/0,
                                /*operation_supports_float16=*/op.kind ==
                                    mojom::ElementWiseUnary::Kind::kCast);
  const TensorIndex input_tensor_index = input_tensor_info.index;
  const TensorIndex output_tensor_index = output_tensor_info.index;
  const OperandDescriptor& input_descriptor =
      GetOperand(op.input_operand_id).descriptor;

  const DataTypeLimits data_type_limits = context_properties_.data_type_limits;
  switch (op.kind) {
    case mojom::ElementWiseUnary::Kind::kAbs: {
      CHECK(data_type_limits.abs_input.Supports(input_descriptor));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_ABS,
                                     input_tensor_index, output_tensor_index);
    }
    case mojom::ElementWiseUnary::Kind::kCast: {
      CHECK(data_type_limits.cast_input.Supports(input_descriptor));
      return SerializeCastOperation(
          input_tensor_index, input_tensor_info.data_type, output_tensor_index,
          output_tensor_info.data_type);
    }
    case mojom::ElementWiseUnary::Kind::kCeil: {
      CHECK(data_type_limits.ceil_input.Supports(input_descriptor));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_CEIL,
                                     input_tensor_index, output_tensor_index);
    }
    case mojom::ElementWiseUnary::Kind::kCos: {
      CHECK(data_type_limits.cos_input.Supports(input_descriptor));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_COS,
                                     input_tensor_index, output_tensor_index);
    }
    case mojom::ElementWiseUnary::Kind::kExp: {
      CHECK(data_type_limits.exp_input.Supports(input_descriptor));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_EXP,
                                     input_tensor_index, output_tensor_index);
    }
    case mojom::ElementWiseUnary::Kind::kFloor: {
      CHECK(data_type_limits.floor_input.Supports(input_descriptor));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_FLOOR,
                                     input_tensor_index, output_tensor_index);
    }
    case mojom::ElementWiseUnary::Kind::kIdentity: {
      CHECK(context_properties_.data_type_limits.identity_input.Supports(
          input_descriptor));
      return SerializeIdentityOperation(input_tensor_info.index,
                                        output_tensor_info.index,
                                        input_tensor_info.dimensions);
    }
    case mojom::ElementWiseUnary::Kind::kLog: {
      CHECK(data_type_limits.log_input.Supports(input_descriptor));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_LOG,
                                     input_tensor_index, output_tensor_index);
    }
    case mojom::ElementWiseUnary::Kind::kLogicalNot: {
      CHECK(data_type_limits.logical_not_input.Supports(input_descriptor));
      return SerializeLogicalNot(input_tensor_info, output_tensor_info);
    }
    case mojom::ElementWiseUnary::Kind::kNeg: {
      CHECK(data_type_limits.neg_input.Supports(input_descriptor));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_NEG,
                                     input_tensor_index, output_tensor_index);
    }
    case mojom::ElementWiseUnary::Kind::kReciprocal: {
      CHECK(data_type_limits.reciprocal_input.Supports(input_descriptor));
      return SerializeReciprocal(input_tensor_info, output_tensor_info);
    }
    case mojom::ElementWiseUnary::Kind::kSign: {
      CHECK(data_type_limits.sign_input.Supports(input_descriptor));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_SIGN,
                                     input_tensor_index, output_tensor_index);
    }
    case mojom::ElementWiseUnary::Kind::kSin: {
      CHECK(data_type_limits.sin_input.Supports(input_descriptor));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_SIN,
                                     input_tensor_index, output_tensor_index);
    }
    case mojom::ElementWiseUnary::Kind::kSqrt: {
      CHECK(data_type_limits.sqrt_input.Supports(input_descriptor));
      return SerializeUnaryOperation(::tflite::BuiltinOperator_SQRT,
                                     input_tensor_index, output_tensor_index);
    }
    case mojom::ElementWiseUnary::Kind::kTan: {
      CHECK(data_type_limits.tan_input.Supports(input_descriptor));
      return SerializeTan(input_tensor_info, output_tensor_info);
    }
    case mojom::ElementWiseUnary::Kind::kErf: {
      CHECK(data_type_limits.erf_input.Supports(input_descriptor));
      return SerializeErf(input_tensor_info, output_tensor_info);
    }
  }
}

auto GraphBuilderTflite::SerializeElu(const mojom::Elu& elu)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.elu_input.Supports(
      GetOperand(elu.input_operand_id).descriptor));

  if (elu.alpha != 1.0) {
    // TODO: crbug.com/328736354 - Support custom alpha values.
    return base::unexpected(
        "Setting a custom alpha is not supported in tflite schema.");
  }

  std::optional<TensorInfo> quantized_output = CanFuseQuantizeAndGetOutput(elu);
  const bool fuse_dequantize = quantized_output.has_value();
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(
                       elu.input_operand_id, /*quantize_params=*/0,
                       /*operation_supports_float16=*/false, fuse_dequantize));

  TensorIndex output_tensor_index =
      fuse_dequantize ? quantized_output->index
                      : SerializeOutputTensorInfo(elu.output_operand_id).index;

  return SerializeUnaryOperation(::tflite::BuiltinOperator_ELU,
                                 input_tensor_info.index, output_tensor_index);
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
  const TensorIndex input_tensor_index = input_tensor_info.index;
  const TensorIndex output_tensor_index_of_abs = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeUnaryOperation(::tflite::BuiltinOperator_ABS,
                                                  input_tensor_index,
                                                  output_tensor_index_of_abs));
  const TensorIndex output_tensor_index_of_line = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeLinearOperation(
      input_tensor_info.dimensions, input_tensor_info.data_type,
      output_tensor_index_of_abs, output_tensor_index_of_line, p, 1.0));
  const TensorIndex constant_one_tensor_index =
      SerializeTensorWithBuffer<float>(
          /*buffer=*/std::array<float, 1>{1.0},
          /*dimensions=*/{});
  const TensorIndex t_expression_tensor_index = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_DIV, constant_one_tensor_index,
      output_tensor_index_of_line, t_expression_tensor_index));

  // Compute subexpression `(a1 * t + a2 * pow(t, 2) + ... + a5 * pow(t, 5))`.
  std::optional<TensorIndex> sum_pow_mul_tensor_index;
  for (size_t i = 0; i < constants.size(); ++i) {
    const TensorIndex output_tensor_index_of_pow_mul = SerializeSubGraphPowMul(
        input_tensor_info.dimensions, input_tensor_info.data_type,
        t_expression_tensor_index,
        /*pow_exponent=*/i + 1,
        /*mul_alpha=*/constants[i]);
    if (sum_pow_mul_tensor_index) {
      const TensorIndex output_tensor_index_of_add = SerializeTemporaryTensor(
          input_tensor_info.dimensions, input_tensor_info.data_type);
      operators_.emplace_back(SerializeBinaryOperation(
          ::tflite::BuiltinOperator_ADD, output_tensor_index_of_pow_mul,
          *sum_pow_mul_tensor_index, output_tensor_index_of_add));
      sum_pow_mul_tensor_index = output_tensor_index_of_add;
    } else {
      sum_pow_mul_tensor_index = output_tensor_index_of_pow_mul;
    }
  }

  // Compute the subexpression `exp(-square(x))`.
  const TensorIndex output_tensor_index_of_square = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeSquareOperation(
      input_tensor_info.index, input_tensor_info.data_type,
      output_tensor_index_of_square));
  const TensorIndex output_tensor_index_of_neg = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeUnaryOperation(::tflite::BuiltinOperator_NEG,
                                                  output_tensor_index_of_square,
                                                  output_tensor_index_of_neg));
  const TensorIndex output_tensor_index_of_exp = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeUnaryOperation(::tflite::BuiltinOperator_EXP,
                                                  output_tensor_index_of_neg,
                                                  output_tensor_index_of_exp));

  // Compute `1 - (the sum of pow mul subexpression) * (the pow exp
  // subexpression)`.
  const TensorIndex output_tensor_index_of_mul = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_MUL, output_tensor_index_of_exp,
      *sum_pow_mul_tensor_index, output_tensor_index_of_mul));
  const TensorIndex output_tensor_index_of_sub = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_SUB, constant_one_tensor_index,
      output_tensor_index_of_mul, output_tensor_index_of_sub));

  // Compute the subexpression `sign = sign(x)`
  const TensorIndex output_tensor_index_of_sign = SerializeTemporaryTensor(
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
  CHECK(context_properties_.data_type_limits.expand_input.Supports(
      GetOperand(expand.output_operand_id).descriptor));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(expand.input_operand_id));
  const TensorInfo output_tensor_info =
      SerializeOutputTensorInfo(expand.output_operand_id);

  // Serialize the expanded shape to tflite tensor with output dimensions.
  const int32_t output_rank =
      base::checked_cast<int32_t>(output_tensor_info.dimensions.size());
  const TensorIndex new_shape_tensor_index = SerializeTensorWithBuffer<int32_t>(
      output_tensor_info.dimensions, std::array<int32_t, 1>{output_rank});

  const OperatorCodeIndex operator_code_index = GetOperatorCodeIndex(
      ::tflite::BuiltinOperator_BROADCAST_TO, /*version=*/2);
  const std::array<TensorIndex, 2> op_inputs = {input_tensor_info.index,
                                                new_shape_tensor_index};
  const std::array<TensorIndex, 1> op_outputs = {output_tensor_info.index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index,
      builder_.CreateVector<TensorIndex>(op_inputs),
      builder_.CreateVector<TensorIndex>(op_outputs));
}

int32_t GraphBuilderTflite::CastGatherIndices(
    const TensorInfo& indices_tensor_info) {
  // The WebNN indices must be one of type uint32, int32, int64, but TFLite
  // indices need int32 or int64 type, so a cast operation need to be inserted
  // before gather if indices data type is uint32.
  if (indices_tensor_info.data_type == ::tflite::TensorType_UINT32) {
    const TensorIndex cast_tensor_index = SerializeTemporaryTensor(
        indices_tensor_info.dimensions, ::tflite::TensorType_INT64);

    operators_.emplace_back(SerializeCastOperation(
        indices_tensor_info.index,
        /*input_tensor_type=*/::tflite::TensorType_UINT32, cast_tensor_index,
        /*output_tensor_type=*/::tflite::TensorType_INT64));
    return cast_tensor_index;
  } else {
    CHECK(indices_tensor_info.data_type == ::tflite::TensorType_INT64 ||
          indices_tensor_info.data_type == ::tflite::TensorType_INT32);
    return indices_tensor_info.index;
  }
}

auto GraphBuilderTflite::SerializeGather(const mojom::Gather& gather)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.gather_input.Supports(
      GetOperand(gather.input_operand_id).descriptor));
  CHECK(context_properties_.data_type_limits.gather_indices.Supports(
      GetOperand(gather.indices_operand_id).descriptor));
  ASSIGN_OR_RETURN(const TensorInfo& indices_tensor_info,
                   SerializeInputTensorInfo(gather.indices_operand_id));
  const TensorIndex indices_tensor_index =
      CastGatherIndices(indices_tensor_info);

  // The WebNN axis option is uint32 data type, but TFLite axis needs int32
  // type, so the axis need to be validated here to not overflow.
  auto checked_axis = base::MakeCheckedNum<int32_t>(gather.axis);
  if (!checked_axis.IsValid()) {
    return base::unexpected("The axis in gather operation is too large.");
  }
  const auto gather_options =
      ::tflite::CreateGatherOptions(builder_, checked_axis.ValueOrDie());

  std::optional<TensorInfo> quantized_output =
      CanFuseQuantizeAndGetOutput(gather);
  const bool fuse_dequantize = quantized_output.has_value();
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(
                       gather.input_operand_id,
                       /*quantize_params=*/0,
                       /*operation_supports_float16=*/false, fuse_dequantize));
  const TensorIndex output_tensor_index =
      fuse_dequantize
          ? quantized_output->index
          : SerializeOutputTensorInfo(gather.output_operand_id).index;
  const OperatorCodeIndex operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_GATHER);
  const std::array<TensorIndex, 2> op_inputs = {input_tensor_info.index,
                                                indices_tensor_index};
  const std::array<TensorIndex, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index,
      builder_.CreateVector<TensorIndex>(op_inputs),
      builder_.CreateVector<TensorIndex>(op_outputs),
      ::tflite::BuiltinOptions_GatherOptions, gather_options.Union());
}

template <typename DataType>
auto GraphBuilderTflite::SerializeGatherNDIndices(
    const TensorInfo& indices_tensor_info,
    const TensorInfo& input_tensor_info)
    -> base::expected<TensorIndex, std::string> {
  // The values in `indices` are computed at runtime, so they can exceed the
  // boundary of the `axis` dimension of input. If unchecked, such indices will
  // cause runtime error on TFLite CPU backend, so clamp the values in `indices`
  // to be in range of `-N` (inclusive) to `N` (exclusive), where `N =
  // input.dimensions[axis]`, but TFLite doesn't support the negative index
  // (index from the end of the `axis` dimension).
  // TODO(crbug.com/373192621): Support negative indices to avoid the runtime
  // error.
  const size_t indices_rank = indices_tensor_info.dimensions.size();
  const size_t indices_nd = indices_tensor_info.dimensions[indices_rank - 1];
  if (indices_nd == input_tensor_info.dimensions.size()) {
    return base::unexpected(
        "TFLite doesn't support to gather input into one dimension.");
  }
  base::FixedArray<DataType> min_values(indices_nd);
  base::FixedArray<DataType> max_values(indices_nd);
  for (size_t axis = 0; axis < indices_nd; ++axis) {
    min_values[axis] = -(input_tensor_info.dimensions[axis]);
    max_values[axis] = input_tensor_info.dimensions[axis] - 1;
  }
  TensorIndex indices_tensor_index = CastGatherIndices(indices_tensor_info);
  ::tflite::TensorType cast_tensor_type =
      indices_tensor_info.data_type == ::tflite::TensorType_UINT32
          ? ::tflite::TensorType_INT64
          : indices_tensor_info.data_type;
  TensorIndex clamp_tensor_index = SerializeTemporaryTensor(
      indices_tensor_info.dimensions, cast_tensor_type);
  operators_.emplace_back(SerializeSubGraphMaxMin<DataType>(
      TensorInfo(indices_tensor_index, cast_tensor_type,
                 indices_tensor_info.dimensions),
      clamp_tensor_index, min_values, max_values));

  return clamp_tensor_index;
}

auto GraphBuilderTflite::SerializeGatherNDOperation(
    TensorIndex input_tensor_index,
    TensorIndex indices_tensor_index,
    TensorIndex output_tensor_index) -> OperatorOffset {
  const OperatorCodeIndex operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_GATHER_ND);
  const std::array<TensorIndex, 2> op_inputs = {input_tensor_index,
                                                indices_tensor_index};
  const std::array<TensorIndex, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index,
      builder_.CreateVector<TensorIndex>(op_inputs),
      builder_.CreateVector<TensorIndex>(op_outputs));
}

template <typename DataType>
  requires(std::is_same_v<DataType, int32_t> ||
           std::is_same_v<DataType, int64_t>)
auto GraphBuilderTflite::SerializeElementsCoordinates(
    base::span<const uint32_t> indices_dimensions,
    base::span<const DataType> indices_value,
    base::span<const int32_t> input_dimensions,
    int32_t axis) -> base::expected<int32_t, std::string> {
  const std::vector<uint32_t> indices_strides =
      CalculateStrides(indices_dimensions);
  const size_t indices_rank = indices_strides.size();

  // Clamp the values in `indices` to be in range of `-N` (inclusive) to `N`
  // (exclusive), where `N = input.dimensions[axis]`
  const DataType axis_dimension = input_dimensions[axis];
  const DataType min_values = -(axis_dimension);
  const DataType max_values = axis_dimension - 1;
  base::FixedArray<DataType> indices_coordinates(indices_value.size() *
                                                 indices_rank);
  for (size_t i = 0; i < indices_value.size(); ++i) {
    DataType clamp_value =
        std::min(std::max(min_values, indices_value[i]), max_values);
    if (clamp_value < 0) {
      clamp_value += axis_dimension;
    }

    // Get coordinates from the index of the flat array.
    ASSIGN_OR_RETURN(base::FixedArray<DataType> coordinates,
                     GetCoordinatesNDFromIndex<DataType>(i, indices_strides));
    // Update the coordinates with WebNN indices operand along the axis.
    //
    //   unravelled index   WebNN indices   axis = 0      TFLite indices
    //  [[0, 0], [0, 1],     [[1, 0],                    [[1 ,0], [0, 1],
    //   [1, 0], [1, 1]       [2, 1],         =>          [2, 0], [1, 1],
    //   [2, 0], [2, 1]]      [0, 2]]                     [0, 0], [2, 1]]
    coordinates[axis] = clamp_value;
    CHECK_EQ(coordinates.size(), indices_rank);
    base::span(indices_coordinates)
        .subspan(i * indices_rank, indices_rank)
        .copy_from(coordinates);
  }

  return SerializeTensorWithBuffer<DataType>(
      /*buffer=*/indices_coordinates,
      /*dimensions=*/std::array<int32_t, 2>{
          base::checked_cast<int32_t>(indices_value.size()),
          base::checked_cast<int32_t>(indices_rank)});
}

auto GraphBuilderTflite::SerializeGatherElements(
    const mojom::GatherElements& gather_elements)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.gather_elements_input.Supports(
      GetOperand(gather_elements.input_operand_id).descriptor));
  const mojom::Operand& indices_operand =
      GetOperand(gather_elements.indices_operand_id);
  CHECK(context_properties_.data_type_limits.gather_elements_indices.Supports(
      indices_operand.descriptor));
  if (indices_operand.kind != mojom::Operand::Kind::kConstant) {
    // TODO(crbug.com/377615324): Support user input indices.
    return base::unexpected("gatherElements only supports constant indices.");
  }

  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(gather_elements.input_operand_id));
  ASSIGN_OR_RETURN(
      const TensorIndex indices_tensor_index,
      SerializeElementsCoordinates<int64_t>(
          indices_operand.descriptor.shape(),
          GetConstantInt64Value(gather_elements.indices_operand_id),
          input_tensor_info.dimensions, gather_elements.axis));
  const TensorIndex output_tensor_index =
      SerializeOutputTensorInfo(gather_elements.output_operand_id).index;
  return SerializeGatherNDOperation(input_tensor_info.index,
                                    indices_tensor_index, output_tensor_index);
}

auto GraphBuilderTflite::SerializeGatherND(const mojom::GatherND& gather_nd)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.gather_nd_input.Supports(
      GetOperand(gather_nd.input_operand_id).descriptor));
  CHECK(context_properties_.data_type_limits.gather_nd_indices.Supports(
      GetOperand(gather_nd.indices_operand_id).descriptor));
  ASSIGN_OR_RETURN(const TensorInfo& indices_tensor_info,
                   SerializeInputTensorInfo(gather_nd.indices_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(gather_nd.input_operand_id));

  TensorIndex indices_tensor_index;
  if (indices_tensor_info.data_type == ::tflite::TensorType_UINT32 ||
      indices_tensor_info.data_type == ::tflite::TensorType_INT64) {
    ASSIGN_OR_RETURN(indices_tensor_index,
                     SerializeGatherNDIndices<int64_t>(indices_tensor_info,
                                                       input_tensor_info));
  } else {
    CHECK_EQ(indices_tensor_info.data_type, ::tflite::TensorType_INT32);
    ASSIGN_OR_RETURN(indices_tensor_index,
                     SerializeGatherNDIndices<int32_t>(indices_tensor_info,
                                                       input_tensor_info));
  }

  const TensorIndex output_tensor_index =
      SerializeOutputTensorInfo(gather_nd.output_operand_id).index;
  return SerializeGatherNDOperation(input_tensor_info.index,
                                    indices_tensor_index, output_tensor_index);
}

auto GraphBuilderTflite::SerializeGelu(const mojom::Gelu& gelu)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.gelu_input.Supports(
      GetOperand(gelu.input_operand_id).descriptor));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(gelu.input_operand_id));
  const TensorIndex output_tensor_index =
      SerializeOutputTensorInfo(gelu.output_operand_id).index;

  return SerializeUnaryOperation(::tflite::BuiltinOperator_GELU,
                                 input_tensor_info.index, output_tensor_index);
}

auto GraphBuilderTflite::SerializeGemm(const mojom::Gemm& gemm)
    -> base::expected<OperatorOffset, std::string> {
  // Check for unsupported inputs.
  CHECK(context_properties_.data_type_limits.gemm_a.SupportsAll(
      {GetOperand(gemm.a_operand_id).descriptor,
       GetOperand(gemm.b_operand_id).descriptor}));

  ASSIGN_OR_RETURN(const TensorInfo& a_tensor_info,
                   SerializeInputTensorInfo(gemm.a_operand_id));
  TensorIndex a_tensor_index = a_tensor_info.index;
  // The permutation transpose first or second 2-D tensor.
  static constexpr std::array<uint32_t, 2> permutation = {1u, 0u};
  if (gemm.a_transpose) {
    a_tensor_index = InsertTransposeOperation(a_tensor_info, permutation);
  }
  // TODO(crbug.com/372932099): Avoid executing alpha * A * B if gemma.alpha ==
  // 0.0f.
  if (gemm.alpha != 1.0f) {
    const TensorIndex alpha_tensor_index = SerializeTensorWithBuffer<float>(
        /*buffer=*/std::array<float, 1>{gemm.alpha},
        /*dimensions=*/{});
    const TensorIndex output_tensor_index_of_mul = SerializeTemporaryTensor(
        a_tensor_info.dimensions, a_tensor_info.data_type);
    operators_.emplace_back(SerializeBinaryOperation(
        ::tflite::BuiltinOperator_MUL, a_tensor_index, alpha_tensor_index,
        output_tensor_index_of_mul));
    a_tensor_index = output_tensor_index_of_mul;
  }

  // The WebNN Gemm follows the expression `alpha * A * B + beta * C`, where
  // A is a 2-D tensor with shape [M, K], B is a 2-D tensor with shape [K,
  // N] by default options, but Tflite Fully Connected's input and filter
  // shapes are [batch, input_channels] and [output_channels,
  // input_channels], so the Transpose operator need to be inserted before
  // Gemm When bTranspose option is false.
  ASSIGN_OR_RETURN(const TensorInfo& b_tensor_info,
                   SerializeInputTensorInfo(gemm.b_operand_id));
  TensorIndex b_tensor_index = b_tensor_info.index;
  if (!gemm.b_transpose) {
    b_tensor_index = InsertTransposeOperation(b_tensor_info, permutation);
  }
  std::vector<TensorIndex> fully_connected_inputs = {a_tensor_index,
                                                     b_tensor_index};

  const TensorInfo output_tensor_info =
      SerializeOutputTensorInfo(gemm.output_operand_id);
  CHECK_EQ(output_tensor_info.dimensions.size(), 2u);
  std::optional<TensorIndex> c_tensor_index;
  if (gemm.c_operand_id && gemm.beta != 0.0f) {
    CHECK(context_properties_.data_type_limits.gemm_c.Supports(
        GetOperand(gemm.c_operand_id.value()).descriptor));
    ASSIGN_OR_RETURN(const TensorInfo& c_tensor_info,
                     SerializeInputTensorInfo(*gemm.c_operand_id));
    c_tensor_index = c_tensor_info.index;
    if (gemm.beta != 1.0f) {
      const TensorIndex beta_tensor_index = SerializeTensorWithBuffer<float>(
          /*buffer=*/std::array<float, 1>{gemm.beta},
          /*dimensions=*/{});
      const TensorIndex output_tensor_index_of_mul = SerializeTemporaryTensor(
          c_tensor_info.dimensions, c_tensor_info.data_type);
      operators_.emplace_back(SerializeBinaryOperation(
          ::tflite::BuiltinOperator_MUL, c_tensor_info.index, beta_tensor_index,
          output_tensor_index_of_mul));
      c_tensor_index = output_tensor_index_of_mul;
    }

    // The TFLite fully connected operator only supports a 1-D bias tensor with
    // `output_channels` dimensions.
    const int32_t output_channels = output_tensor_info.dimensions[1];
    if (c_tensor_info.dimensions.size() == 1 &&
        c_tensor_info.dimensions[0] == output_channels) {
      fully_connected_inputs.push_back(*c_tensor_index);
    }
  }

  // Add the `beta * C` subexpression if it's not fused into FULLY_CONNECTED
  // operator.
  const bool addition_c_expression =
      c_tensor_index && fully_connected_inputs.size() == 2;
  TensorIndex output_tensor_index = output_tensor_info.index;
  if (addition_c_expression) {
    output_tensor_index = SerializeTemporaryTensor(
        output_tensor_info.dimensions, output_tensor_info.data_type);
  }
  const OperatorCodeIndex operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_FULLY_CONNECTED);
  const std::array<TensorIndex, 1> op_outputs = {output_tensor_index};
  OperatorOffset operator_offset = ::tflite::CreateOperator(
      builder_, operator_code_index,
      builder_.CreateVector<TensorIndex>(fully_connected_inputs),
      builder_.CreateVector<TensorIndex>(op_outputs));

  if (addition_c_expression) {
    operators_.push_back(operator_offset);
    operator_offset = SerializeBinaryOperation(
        ::tflite::BuiltinOperator_ADD, output_tensor_index, *c_tensor_index,
        output_tensor_info.index);
  }
  return operator_offset;
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
GraphBuilderTflite::TensorIndex GraphBuilderTflite::SerializeSubGraphMatmulAdd(
    base::span<const int32_t> input_dimensions,
    ::tflite::TensorType input_tensor_type,
    TensorIndex input_tensor_index,
    TensorIndex weight_tensor_index,
    std::optional<TensorIndex> bias_tensor_index) {
  CHECK_EQ(input_tensor_type, ::tflite::TensorType_FLOAT32);
  const TensorIndex output_tensor_index_of_matmul =
      SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeMatmulOperation(
      input_tensor_index, weight_tensor_index, output_tensor_index_of_matmul));

  TensorIndex output_tensor_index = output_tensor_index_of_matmul;
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
    TensorIndex input_tensor_index,
    base::span<const int32_t> slice_starts,
    base::span<const int32_t> slice_sizes)
    -> base::expected<TensorIndex, std::string> {
  CHECK_EQ(input_tensor_type, ::tflite::TensorType_FLOAT32);
  // The output is a tensor the same as the specified slice sizes.
  const TensorIndex output_tensor_index_of_slice =
      SerializeTemporaryTensor(slice_sizes, input_tensor_type);
  ASSIGN_OR_RETURN(
      OperatorOffset operator_offset,
      SerializeSliceOperation(input_tensor_index, output_tensor_index_of_slice,
                              slice_starts, slice_sizes));
  operators_.emplace_back(operator_offset);

  const TensorIndex output_tensor_index =
      SerializeTemporaryTensor(slice_sizes, input_tensor_type);
  std::vector<uint32_t> permutation(slice_sizes.size());
  std::iota(permutation.rbegin(), permutation.rend(), 0);
  operators_.emplace_back(SerializeTransposeOperation(
      output_tensor_index_of_slice, output_tensor_index, slice_sizes,
      permutation));

  return output_tensor_index;
}

auto GraphBuilderTflite::SerializeGruGate(
    const GruCellOperation& gru_cell,
    GruGateType type,
    std::optional<TensorIndex> reset_gate_tensor_index)
    -> base::expected<TensorIndex, std::string> {
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
  std::optional<TensorIndex> bias_tensor_index;
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
  ASSIGN_OR_RETURN(const TensorIndex weight_tensor_index,
                   SerializeSubGraphSliceTranspose(
                       input_tensor_type, gru_cell.weight_tensor_index,
                       std::array<int32_t, 2>({slice_start, 0}),
                       std::array<int32_t, 2>({hidden_size, input_size})));
  const TensorIndex output_tensor_index_of_weight = SerializeSubGraphMatmulAdd(
      output_shape, input_tensor_type, gru_cell.input_tensor_index,
      weight_tensor_index, bias_tensor_index);

  // hiddenState * recurrentWeight + recurrentBias.
  std::optional<TensorIndex> recurrent_bias_tensor_index;
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
      const TensorIndex recurrent_weight_tensor_index,
      SerializeSubGraphSliceTranspose(
          input_tensor_type, gru_cell.recurrent_weight_tensor_index,
          std::array<int32_t, 2>({slice_start, 0}),
          std::array<int32_t, 2>({hidden_size, hidden_size})));
  TensorIndex hidden_state_tensor_index = gru_cell.hidden_state_tensor_index;
  // Apply the reset gate before matrix multiplication for new gate if needed.
  if (type == GruGateType::kNew && !gru_cell.reset_after) {
    const TensorIndex output_tensor_index_of_mul =
        SerializeTemporaryTensor(output_shape, input_tensor_type);
    operators_.emplace_back(SerializeBinaryOperation(
        ::tflite::BuiltinOperator_MUL, *reset_gate_tensor_index,
        hidden_state_tensor_index, output_tensor_index_of_mul));
    hidden_state_tensor_index = output_tensor_index_of_mul;
  }
  TensorIndex output_tensor_index_of_recurrent_weight =
      SerializeSubGraphMatmulAdd(
          output_shape, input_tensor_type, hidden_state_tensor_index,
          recurrent_weight_tensor_index, recurrent_bias_tensor_index);
  // Apply the reset gate after matrix multiplication for new gate if needed.
  if (type == GruGateType::kNew && gru_cell.reset_after) {
    const TensorIndex output_tensor_index_of_mul =
        SerializeTemporaryTensor(output_shape, input_tensor_type);
    operators_.emplace_back(SerializeBinaryOperation(
        ::tflite::BuiltinOperator_MUL, *reset_gate_tensor_index,
        output_tensor_index_of_recurrent_weight, output_tensor_index_of_mul));
    output_tensor_index_of_recurrent_weight = output_tensor_index_of_mul;
  }

  // Add the result of the above two expressions (element-wise multiplication
  // between the input / hiddenState and the respective weights / recurrent
  // weights).
  const TensorIndex output_tensor_index_of_add =
      SerializeTemporaryTensor(output_shape, input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_ADD, output_tensor_index_of_weight,
      output_tensor_index_of_recurrent_weight, output_tensor_index_of_add));

  // Apply first activation for the update and reset gate, the second activation
  // for the new gate.
  const TensorIndex output_tensor_index_of_gate =
      SerializeTemporaryTensor(output_shape, input_tensor_type);
  operators_.emplace_back(SerializeUnaryOperation(activation_code,
                                                  output_tensor_index_of_add,
                                                  output_tensor_index_of_gate));

  return output_tensor_index_of_gate;
}

GraphBuilderTflite::RecurrentNetworkBase::RecurrentNetworkBase(
    base::span<const int32_t> input_dimensions,
    ::tflite::TensorType input_tensor_type,
    TensorIndex input_tensor_index,
    TensorIndex weight_tensor_index,
    TensorIndex recurrent_weight_tensor_index,
    std::optional<TensorIndex> bias_tensor_index,
    std::optional<TensorIndex> recurrent_bias_tensor_index,
    TensorIndex hidden_state_tensor_index,
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
    TensorIndex input_tensor_index,
    TensorIndex output_tensor_index,
    TensorIndex weight_tensor_index,
    TensorIndex recurrent_weight_tensor_index,
    std::optional<TensorIndex> bias_tensor_index,
    std::optional<TensorIndex> recurrent_bias_tensor_index,
    TensorIndex hidden_state_tensor_index,
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
  CHECK(context_properties_.data_type_limits.gru_cell_input.SupportsAll(
      {GetOperand(gru_cell.input_operand_id).descriptor,
       GetOperand(gru_cell.weight_operand_id).descriptor,
       GetOperand(gru_cell.recurrent_weight_operand_id).descriptor,
       GetOperand(gru_cell.hidden_state_operand_id).descriptor}));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(gru_cell.input_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& weight_tensor_info,
                   SerializeInputTensorInfo(gru_cell.weight_operand_id));
  ASSIGN_OR_RETURN(
      const TensorInfo& recurrent_weight_tensor_info,
      SerializeInputTensorInfo(gru_cell.recurrent_weight_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& hidden_state_tensor_info,
                   SerializeInputTensorInfo(gru_cell.hidden_state_operand_id));
  std::optional<TensorIndex> bias_tensor_index;
  if (gru_cell.bias_operand_id) {
    CHECK(context_properties_.data_type_limits.gru_cell_bias.Supports(
        GetOperand(gru_cell.bias_operand_id.value()).descriptor));
    ASSIGN_OR_RETURN(const TensorInfo& bias_tensor_info,
                     SerializeInputTensorInfo(*gru_cell.bias_operand_id));
    bias_tensor_index = bias_tensor_info.index;
  }
  std::optional<TensorIndex> recurrent_bias_tensor_index;
  if (gru_cell.recurrent_bias_operand_id) {
    CHECK(context_properties_.data_type_limits.gru_cell_bias.Supports(
        GetOperand(gru_cell.recurrent_bias_operand_id.value()).descriptor));
    ASSIGN_OR_RETURN(
        const TensorInfo& recurrent_bias_tensor_info,
        SerializeInputTensorInfo(*gru_cell.recurrent_bias_operand_id));
    recurrent_bias_tensor_index = recurrent_bias_tensor_info.index;
  }
  const TensorIndex output_tensor_index =
      SerializeOutputTensorInfo(gru_cell.output_operand_id).index;

  const auto checked_hidden_size =
      base::MakeCheckedNum<int32_t>(gru_cell.hidden_size);
  if (!checked_hidden_size.IsValid()) {
    return base::unexpected("The hidden size is too large.");
  }

  GruCellOperation gru_cell_operation(
      input_tensor_info.dimensions, input_tensor_info.data_type,
      input_tensor_info.index, output_tensor_index, weight_tensor_info.index,
      recurrent_weight_tensor_info.index, bias_tensor_index,
      recurrent_bias_tensor_index, hidden_state_tensor_info.index,
      checked_hidden_size.ValueOrDie(), gru_cell.reset_after, gru_cell.layout,
      gru_cell.activations);

  return SerializeGruCellOperation(gru_cell_operation);
}

auto GraphBuilderTflite::SerializeGruCellOperation(
    const GruCellOperation& gru_cell)
    -> base::expected<OperatorOffset, std::string> {
  // Compute the update gate.
  ASSIGN_OR_RETURN(const TensorIndex update_gate_tensor_index,
                   SerializeGruGate(gru_cell, GruGateType::kUpdate));

  // Compute the reset gate.
  ASSIGN_OR_RETURN(const TensorIndex reset_gate_tensor_index,
                   SerializeGruGate(gru_cell, GruGateType::kReset));

  // Compute the new gate.
  ASSIGN_OR_RETURN(
      const TensorIndex new_gate_tensor_index,
      SerializeGruGate(gru_cell, GruGateType::kNew, reset_gate_tensor_index));

  const std::array<int32_t, 2> output_shape = {gru_cell.input_dimensions[0],
                                               gru_cell.hidden_size};
  // Compute mul(newGate, sub(one, updateGate)).
  const TensorIndex scalar_one_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{1.0},
      /*dimensions=*/{});
  const TensorIndex output_tensor_index_of_sub =
      SerializeTemporaryTensor(output_shape, gru_cell.input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_SUB, scalar_one_tensor_index,
      update_gate_tensor_index, output_tensor_index_of_sub));
  const TensorIndex new_gate_tensor_index_of_mul =
      SerializeTemporaryTensor(output_shape, gru_cell.input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_MUL, new_gate_tensor_index,
      output_tensor_index_of_sub, new_gate_tensor_index_of_mul));

  // Compute mul(updateGate, hiddenState).
  const TensorIndex update_gate_tensor_index_of_mul =
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
    TensorIndex input_tensor_index,
    base::span<const TensorIndex> output_tensor_indices,
    TensorIndex weight_tensor_index,
    TensorIndex recurrent_weight_tensor_index,
    std::optional<TensorIndex> bias_tensor_index,
    std::optional<TensorIndex> recurrent_bias_tensor_index,
    TensorIndex hidden_state_tensor_index,
    int32_t hidden_size,
    TensorIndex cell_state_tensor_index,
    std::optional<TensorIndex> peephole_weight_tensor_index,
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

base::expected<GraphBuilderTflite::TensorIndex, std::string>
GraphBuilderTflite::SerializeLstmGate(const LstmCellOperation& lstm_cell,
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
  std::optional<TensorIndex> bias_tensor_index;
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
  ASSIGN_OR_RETURN(const TensorIndex weight_tensor_index,
                   SerializeSubGraphSliceTranspose(
                       input_tensor_type, lstm_cell.weight_tensor_index,
                       std::array<int32_t, 2>({slice_start, 0}),
                       std::array<int32_t, 2>({hidden_size, input_size})));
  const TensorIndex output_tensor_index_of_weight = SerializeSubGraphMatmulAdd(
      output_shape, input_tensor_type, lstm_cell.input_tensor_index,
      weight_tensor_index, bias_tensor_index);

  // hiddenState * recurrentWeight + recurrentBias.
  std::optional<TensorIndex> recurrent_bias_tensor_index;
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
      const TensorIndex recurrent_weight_tensor_index,
      SerializeSubGraphSliceTranspose(
          input_tensor_type, lstm_cell.recurrent_weight_tensor_index,
          std::array<int32_t, 2>({slice_start, 0}),
          std::array<int32_t, 2>({hidden_size, hidden_size})));
  TensorIndex output_tensor_index_of_recurrent_weight =
      SerializeSubGraphMatmulAdd(
          output_shape, input_tensor_type, lstm_cell.hidden_state_tensor_index,
          recurrent_weight_tensor_index, recurrent_bias_tensor_index);

  // Add the result of the above two expressions (element-wise multiplication
  // between the input / hiddenState and the respective weights / recurrent
  // weights).
  TensorIndex updated_state_tensor_index =
      SerializeTemporaryTensor(output_shape, input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_ADD, output_tensor_index_of_weight,
      output_tensor_index_of_recurrent_weight, updated_state_tensor_index));

  // mul(cellState, peepholeWeight) + updatedState.
  if (lstm_cell.peephole_weight_tensor_index && type != LstmGateType::kCell) {
    TensorIndex output_tensor_index_of_slice = SerializeTemporaryTensor(
        std::array<int32_t, 1>({hidden_size}), input_tensor_type);
    ASSIGN_OR_RETURN(
        OperatorOffset operator_offset,
        SerializeSliceOperation(*lstm_cell.peephole_weight_tensor_index,
                                output_tensor_index_of_slice,
                                std::array<int32_t, 1>({slice_start}),
                                std::array<int32_t, 1>({hidden_size})));
    operators_.emplace_back(operator_offset);

    const TensorIndex output_tensor_index_of_mul =
        SerializeTemporaryTensor(output_shape, input_tensor_type);
    operators_.emplace_back(SerializeBinaryOperation(
        ::tflite::BuiltinOperator_MUL, lstm_cell.cell_state_tensor_index,
        output_tensor_index_of_slice, output_tensor_index_of_mul));

    const TensorIndex output_tensor_index_of_add =
        SerializeTemporaryTensor(output_shape, input_tensor_type);
    operators_.emplace_back(SerializeBinaryOperation(
        ::tflite::BuiltinOperator_ADD, output_tensor_index_of_mul,
        updated_state_tensor_index, output_tensor_index_of_add));
    updated_state_tensor_index = output_tensor_index_of_add;
  }

  // Apply first activation for the input, forget and output gate, the second
  // activation for the cell gate.
  const TensorIndex output_tensor_index_of_gate =
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
  ASSIGN_OR_RETURN(const TensorIndex input_gate_tensor_index,
                   SerializeLstmGate(lstm_cell, LstmGateType::kInput));

  // Compute the forget gate.
  ASSIGN_OR_RETURN(const TensorIndex forgat_gate_tensor_index,
                   SerializeLstmGate(lstm_cell, LstmGateType::kForget));

  // Compute the cell gate.
  ASSIGN_OR_RETURN(const TensorIndex cell_gate_tensor_index,
                   SerializeLstmGate(lstm_cell, LstmGateType::kCell));

  // Compute the output gate.
  ASSIGN_OR_RETURN(const TensorIndex output_gate_tensor_index,
                   SerializeLstmGate(lstm_cell, LstmGateType::kOutput));

  const std::array<int32_t, 2> output_shape = {lstm_cell.input_dimensions[0],
                                               lstm_cell.hidden_size};
  // Compute add(mul(forgetGete, cellState), mul(cellGate, inputGate).
  const TensorIndex forget_cell_gete_tensor_index =
      SerializeTemporaryTensor(output_shape, lstm_cell.input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_MUL, forgat_gate_tensor_index,
      lstm_cell.cell_state_tensor_index, forget_cell_gete_tensor_index));
  const TensorIndex input_cell_gete_tensor_index =
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
  const TensorIndex activation_tensor_index =
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
    TensorIndex input_tensor_index,
    base::span<const int32_t> slice_starts,
    base::span<const int32_t> slice_sizes,
    int32_t squeeze_axis) -> base::expected<TensorIndex, std::string> {
  CHECK_EQ(input_tensor_type, ::tflite::TensorType_FLOAT32);
  // The output is a tensor the same as the specified slice sizes.
  const TensorIndex output_tensor_index_of_slice =
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
  const TensorIndex output_tensor_index =
      SerializeTemporaryTensor(squeeze_output_shape, input_tensor_type);
  flatbuffers::Offset<void> builtin_options =
      ::tflite::CreateSqueezeOptions(
          builder_, builder_.CreateVector<int32_t>({squeeze_axis}))
          .Union();
  const OperatorCodeIndex operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_SQUEEZE);
  const std::array<TensorIndex, 1> op_inputs = {output_tensor_index_of_slice};
  const std::array<TensorIndex, 1> op_outputs = {output_tensor_index};
  operators_.emplace_back(::tflite::CreateOperator(
      builder_, operator_code_index,
      builder_.CreateVector<TensorIndex>(op_inputs),
      builder_.CreateVector<TensorIndex>(op_outputs),
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
  if constexpr (std::is_same_v<RecurrentNetworkType, mojom::Lstm>) {
    CHECK(context_properties_.data_type_limits.lstm_input.SupportsAll(
        {GetOperand(recurrent_network.input_operand_id).descriptor,
         GetOperand(recurrent_network.weight_operand_id).descriptor,
         GetOperand(recurrent_network.recurrent_weight_operand_id)
             .descriptor}));
  } else /* `RecurrentNetworkType` is  `mojom::Gru` */ {
    CHECK(context_properties_.data_type_limits.gru_input.SupportsAll(
        {GetOperand(recurrent_network.input_operand_id).descriptor,
         GetOperand(recurrent_network.weight_operand_id).descriptor,
         GetOperand(recurrent_network.recurrent_weight_operand_id)
             .descriptor}));
  }

  ASSIGN_OR_RETURN(
      const TensorInfo& input_tensor_info,
      SerializeInputTensorInfo(recurrent_network.input_operand_id));
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
  ASSIGN_OR_RETURN(TensorIndex hidden_state_tensor_index,
                   GetInitialHiddenAndCellState(
                       recurrent_network.initial_hidden_state_operand_id,
                       initial_hidden_cell_state_shape));
  // The cell state tensor index only be used by lstm.
  TensorIndex lstm_cell_state_tensor_index;
  if constexpr (std::is_same<RecurrentNetworkType, mojom::Lstm>::value) {
    ASSIGN_OR_RETURN(lstm_cell_state_tensor_index,
                     GetInitialHiddenAndCellState(
                         recurrent_network.initial_cell_state_operand_id,
                         initial_hidden_cell_state_shape));
  }

  base::FixedArray<TensorIndex> cell_weight_tensor_indices(num_directions);
  base::FixedArray<TensorIndex> cell_recurrent_weight_tensor_indices(
      num_directions);
  std::vector<TensorIndex> cell_bias_tensor_indices;
  cell_bias_tensor_indices.reserve(num_directions);
  std::vector<TensorIndex> cell_recurrent_bias_tensor_indices;
  cell_recurrent_bias_tensor_indices.reserve(num_directions);
  ASSIGN_OR_RETURN(
      const TensorInfo& weight_tensor_info,
      SerializeInputTensorInfo(recurrent_network.weight_operand_id));
  ASSIGN_OR_RETURN(
      const TensorInfo& recurrent_weight_tensor_info,
      SerializeInputTensorInfo(recurrent_network.recurrent_weight_operand_id));
  std::optional<TensorInfo> bias_tensor_info;
  if (recurrent_network.bias_operand_id) {
    if constexpr (std::is_same_v<RecurrentNetworkType, mojom::Lstm>) {
      CHECK(context_properties_.data_type_limits.lstm_bias.Supports(
          GetOperand(recurrent_network.bias_operand_id.value()).descriptor));
    } else /* `RecurrentNetworkType` is `mojom::Gru` */ {
      CHECK(context_properties_.data_type_limits.gru_bias.Supports(
          GetOperand(recurrent_network.bias_operand_id.value()).descriptor));
    }
    ASSIGN_OR_RETURN(bias_tensor_info, SerializeInputTensorInfo(
                                           *recurrent_network.bias_operand_id));
  }
  std::optional<TensorInfo> recurrent_bias_tensor_info;
  if (recurrent_network.recurrent_bias_operand_id) {
    if constexpr (std::is_same_v<RecurrentNetworkType, mojom::Lstm>) {
      CHECK(context_properties_.data_type_limits.lstm_bias.Supports(
          GetOperand(recurrent_network.recurrent_bias_operand_id.value())
              .descriptor));
    } else /* `RecurrentNetworkType` is `mojom::Gru` */ {
      CHECK(context_properties_.data_type_limits.gru_bias.Supports(
          GetOperand(recurrent_network.recurrent_bias_operand_id.value())
              .descriptor));
    }
    ASSIGN_OR_RETURN(
        recurrent_bias_tensor_info,
        SerializeInputTensorInfo(*recurrent_network.recurrent_bias_operand_id));
  }
  // The cell peephole weight tensor indices only be used by lstm.
  std::optional<TensorInfo> lstm_peephole_weight_tensor_info;
  std::vector<TensorIndex> lstm_cell_peephole_weight_tensor_indices;
  if constexpr (std::is_same<RecurrentNetworkType, mojom::Lstm>::value) {
    if (recurrent_network.peephole_weight_operand_id) {
      CHECK(context_properties_.data_type_limits.lstm_bias.Supports(
          GetOperand(recurrent_network.peephole_weight_operand_id.value())
              .descriptor));
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
    ASSIGN_OR_RETURN(const TensorIndex cell_weight_tensor_index,
                     SerializeSubGraphSliceSqueeze(
                         input_tensor_type, weight_tensor_info.index,
                         /*slice_starts=*/std::array<int32_t, 3>({slot, 0, 0}),
                         /*slice_sizes=*/
                         std::array<int32_t, 3>({1, slice_height, input_size}),
                         /*squeeze_axis=*/0));
    cell_weight_tensor_indices[slot] = cell_weight_tensor_index;

    ASSIGN_OR_RETURN(const TensorIndex cell_recurrent_weight_tensor_index,
                     SerializeSubGraphSliceSqueeze(
                         input_tensor_type, recurrent_weight_tensor_info.index,
                         /*slice_starts=*/std::array<int32_t, 3>({slot, 0, 0}),
                         /*slice_sizes=*/
                         std::array<int32_t, 3>({1, slice_height, hidden_size}),
                         /*squeeze_axis=*/0));
    cell_recurrent_weight_tensor_indices[slot] =
        cell_recurrent_weight_tensor_index;

    if (recurrent_network.bias_operand_id) {
      ASSIGN_OR_RETURN(const TensorIndex cell_bias_tensor_index,
                       SerializeSubGraphSliceSqueeze(
                           input_tensor_type, bias_tensor_info->index,
                           /*slice_starts=*/std::array<int32_t, 2>({slot, 0}),
                           /*slice_sizes=*/
                           std::array<int32_t, 2>({1, slice_height}),
                           /*squeeze_axis=*/0));
      cell_bias_tensor_indices.push_back(cell_bias_tensor_index);
    }

    if (recurrent_network.recurrent_bias_operand_id) {
      ASSIGN_OR_RETURN(const TensorIndex cell_recurrent_bias_tensor_index,
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
            const TensorIndex peephole_weight_tensor_index,
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

  std::optional<TensorIndex> sequence_tensor_index;
  for (int32_t step = 0; step < recurrent_steps; ++step) {
    base::FixedArray<TensorIndex> current_hidden_tensor_indices(num_directions);
    std::vector<TensorIndex> lstm_current_cell_tensor_indices;
    if constexpr (std::is_same<RecurrentNetworkType, mojom::Lstm>::value) {
      lstm_current_cell_tensor_indices.reserve(num_directions);
    }
    for (int32_t slot = 0; slot < num_directions; ++slot) {
      ASSIGN_OR_RETURN(
          const TensorIndex cell_hidden_tensor_index,
          SerializeSubGraphSliceSqueeze(
              input_tensor_type, hidden_state_tensor_index,
              /*slice_starts=*/std::array<int32_t, 3>({slot, 0, 0}),
              /*slice_sizes=*/
              std::array<int32_t, 3>({1, batch_size, hidden_size}),
              /*squeeze_axis=*/0));
      current_hidden_tensor_indices[slot] = cell_hidden_tensor_index;

      if constexpr (std::is_same<RecurrentNetworkType, mojom::Lstm>::value) {
        ASSIGN_OR_RETURN(
            const TensorIndex lstm_cell_tensor_index,
            SerializeSubGraphSliceSqueeze(
                input_tensor_type, lstm_cell_state_tensor_index,
                /*slice_starts=*/std::array<int32_t, 3>({slot, 0, 0}),
                /*slice_sizes=*/
                std::array<int32_t, 3>({1, batch_size, hidden_size}),
                /*squeeze_axis=*/0));
        lstm_current_cell_tensor_indices.push_back(lstm_cell_tensor_index);
      }
    }

    std::optional<TensorIndex> next_hidden_tensor_index;
    std::optional<TensorIndex> lstm_next_cell_tensor_index;
    for (int32_t slot = 0; slot < num_directions; ++slot) {
      const int32_t slice_start =
          (slot == 1 || recurrent_network.direction ==
                            mojom::RecurrentNetworkDirection::kBackward
               ? recurrent_steps - step - 1
               : step);
      ASSIGN_OR_RETURN(
          const TensorIndex cell_input_tensor_index,
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
      const TensorIndex output_hidden_state_tensor_index =
          SerializeTemporaryTensor(cell_output_dimensions, input_tensor_type);
      // The output cell state tensor index is only used by lstm.
      TensorIndex lstm_output_cell_state_tensor_index;

      std::optional<TensorIndex> bias_tensor_index;
      if (recurrent_network.bias_operand_id) {
        bias_tensor_index = cell_bias_tensor_indices[slot];
      }
      std::optional<TensorIndex> recurrent_bias_tensor_index;
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
        std::optional<TensorIndex> lstm_peephole_weight_tensor_index;
        if (recurrent_network.peephole_weight_operand_id) {
          lstm_peephole_weight_tensor_index =
              lstm_cell_peephole_weight_tensor_indices[slot];
        }
        lstm_output_cell_state_tensor_index =
            SerializeTemporaryTensor(cell_output_dimensions, input_tensor_type);
        const std::array<TensorIndex, 2> lstm_cell_output_tensor_indices = {
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
      const TensorIndex reshape_cells_tensor_index =
          SerializeTemporaryTensor(new_shape, input_tensor_type);
      operators_.emplace_back(SerializeReshapeOperation(
          *next_hidden_tensor_index, reshape_cells_tensor_index, new_shape));

      if (sequence_tensor_index) {
        const std::array<int32_t, 4> concat_output_shape = {
            step + 1, num_directions, batch_size, hidden_size};
        const TensorIndex concat_tensor_index =
            SerializeTemporaryTensor(concat_output_shape, input_tensor_type);
        operators_.emplace_back(SerializeConcatOperation(
            std::array<TensorIndex, 2>(
                {*sequence_tensor_index, reshape_cells_tensor_index}),
            concat_tensor_index, 0));
        sequence_tensor_index = concat_tensor_index;
      } else {
        sequence_tensor_index = reshape_cells_tensor_index;
      }
    }
  }

  base::FixedArray<TensorIndex> output_tensor_indices(
      recurrent_network.output_operand_ids.size());
  for (size_t i = 0; i < recurrent_network.output_operand_ids.size(); ++i) {
    output_tensor_indices[i] =
        SerializeOutputTensorInfo(recurrent_network.output_operand_ids[i])
            .index;
  }
  if (recurrent_network.return_sequence) {
    TensorIndex output_sequence_tensor_index;
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
  CHECK(context_properties_.data_type_limits.hard_sigmoid_input.Supports(
      GetOperand(hard_sigmoid.input_operand_id).descriptor));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(hard_sigmoid.input_operand_id));
  const TensorIndex output_tensor_index_of_linear = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeLinearOperation(
      input_tensor_info.dimensions, input_tensor_info.data_type,
      input_tensor_info.index, output_tensor_index_of_linear,
      hard_sigmoid.alpha, hard_sigmoid.beta));

  // The expression `max(0, min(1, linear))` can be implemented with TFLite
  // RELU_0_TO_1 operator.
  const TensorIndex output_tensor_index =
      SerializeOutputTensorInfo(hard_sigmoid.output_operand_id).index;
  return SerializeUnaryOperation(::tflite::BuiltinOperator_RELU_0_TO_1,
                                 output_tensor_index_of_linear,
                                 output_tensor_index);
}

auto GraphBuilderTflite::SerializeHardSwish(const mojom::HardSwish& hard_swish)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.hard_swish_input.Supports(
      GetOperand(hard_swish.input_operand_id).descriptor));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(hard_swish.input_operand_id));
  const TensorIndex output_tensor_index =
      SerializeOutputTensorInfo(hard_swish.output_operand_id).index;

  return SerializeUnaryOperation(::tflite::BuiltinOperator_HARD_SWISH,
                                 input_tensor_info.index, output_tensor_index);
}

std::tuple<GraphBuilderTflite::TensorIndex, GraphBuilderTflite::TensorIndex>
GraphBuilderTflite::ComputeMeanAndVarianceForNormalization(
    base::span<const int32_t> input_dimensions,
    ::tflite::TensorType input_tensor_type,
    TensorIndex input_tensor_index,
    base::span<const int32_t> spatial_dimensions) {
  // Get mean values with reduceMean over the spatial dimensions of the input.
  std::vector<int32_t> reduce_dimensions(input_dimensions.begin(),
                                         input_dimensions.end());
  for (auto dimension : spatial_dimensions) {
    reduce_dimensions[dimension] = 1;
  }
  const TensorIndex mean_tensor_index =
      SerializeTemporaryTensor(reduce_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeReduceOperation(
      ::tflite::BuiltinOperator_MEAN, input_tensor_index, mean_tensor_index,
      spatial_dimensions, /*keep_dimensions=*/true));

  // Get variance with expression `Variance = ReduceMean(Square(Input - Mean))`
  // over the spatial dimensions of the input.
  const TensorIndex output_tensor_index_of_sub =
      SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_SUB, input_tensor_index, mean_tensor_index,
      output_tensor_index_of_sub));
  const TensorIndex output_tensor_index_of_square =
      SerializeTemporaryTensor(input_dimensions, input_tensor_type);
  operators_.emplace_back(
      SerializeSquareOperation(output_tensor_index_of_sub, input_tensor_type,
                               output_tensor_index_of_square));
  const TensorIndex variance_tensor_index =
      SerializeTemporaryTensor(reduce_dimensions, input_tensor_type);
  operators_.emplace_back(SerializeReduceOperation(
      ::tflite::BuiltinOperator_MEAN, output_tensor_index_of_square,
      variance_tensor_index, spatial_dimensions, /*keep_dimensions=*/true));

  return std::make_tuple(mean_tensor_index, variance_tensor_index);
}

GraphBuilderTflite::TensorIndex
GraphBuilderTflite::TransposeAndReshapeLayerNormalizationScaleBias(
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
  std::optional<TensorIndex> transpose_tensor_index;
  const std::vector<uint32_t> sorted_indices = GetIndexOfSortedValue(axes);
  if (!std::ranges::is_sorted(sorted_indices)) {
    transpose_tensor_index =
        InsertTransposeOperation(scale_or_bias_tensor_info, sorted_indices);
  }

  const TensorIndex reshape_tensor_index = SerializeTemporaryTensor(
      compatible_shape, scale_or_bias_tensor_info.data_type);
  operators_.emplace_back(SerializeReshapeOperation(
      transpose_tensor_index.value_or(scale_or_bias_tensor_info.index),
      reshape_tensor_index, compatible_shape));

  return reshape_tensor_index;
}

auto GraphBuilderTflite::SerializeIdentityOperation(
    TensorIndex input_tensor_index,
    TensorIndex output_tensor_index,
    base::span<const int32_t> shape) -> OperatorOffset {
  // Implement WebNN identity operation with TFLite reshape operator, the
  // output shape is the same as input.
  // TODO(crbug.com/336399247): Skip identity implementation with
  // redirecting output tensor to input.
  return SerializeReshapeOperation(input_tensor_index, output_tensor_index,
                                   shape);
}
auto GraphBuilderTflite::SerializeInstanceNormalization(
    const mojom::InstanceNormalization& instance_normalization)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(
      context_properties_.data_type_limits.instance_normalization_input
          .Supports(
              GetOperand(instance_normalization.input_operand_id).descriptor));
  ASSIGN_OR_RETURN(
      const TensorInfo& input_tensor_info,
      SerializeInputTensorInfo(instance_normalization.input_operand_id));
  const ::tflite::TensorType input_tensor_type = input_tensor_info.data_type;
  CHECK_EQ(context_properties_.input_operand_layout, InputOperandLayout::kNhwc);
  std::array<int32_t, 2> spatial_dimensions = {1, 2};
  uint32_t channel_axis = 3;
  std::vector<int32_t> new_shape(input_tensor_info.dimensions.size(), 1);
  new_shape[channel_axis] = input_tensor_info.dimensions[channel_axis];

  const TensorIndex input_tensor_index = input_tensor_info.index;
  const auto [mean_tensor_index, variance_tensor_index] =
      ComputeMeanAndVarianceForNormalization(
          input_tensor_info.dimensions, input_tensor_type, input_tensor_index,
          spatial_dimensions);

  // Reshape the 1-D tensor of the scale operand to the new shape if needed.
  std::optional<TensorIndex> reshape_scale_tensor_index;
  if (instance_normalization.scale_operand_id) {
    CHECK(context_properties_.data_type_limits.instance_normalization_scale
              .Supports(GetOperand(*instance_normalization.scale_operand_id)
                            .descriptor));
    ASSIGN_OR_RETURN(
        const TensorInfo& scale_tensor_info,
        SerializeInputTensorInfo(*instance_normalization.scale_operand_id));
    reshape_scale_tensor_index =
        SerializeTemporaryTensor(new_shape, input_tensor_type);
    operators_.emplace_back(SerializeReshapeOperation(
        scale_tensor_info.index, *reshape_scale_tensor_index, new_shape));
  }

  // Reshape the 1-D tensor of the bias operand to the new shape if needed.
  std::optional<TensorIndex> reshape_bias_tensor_index;
  if (instance_normalization.bias_operand_id) {
    CHECK(context_properties_.data_type_limits.instance_normalization_scale
              .Supports(GetOperand(*instance_normalization.bias_operand_id)
                            .descriptor));
    ASSIGN_OR_RETURN(
        const TensorInfo& bias_tensor_info,
        SerializeInputTensorInfo(*instance_normalization.bias_operand_id));
    reshape_bias_tensor_index =
        SerializeTemporaryTensor(new_shape, input_tensor_type);
    operators_.emplace_back(SerializeReshapeOperation(
        bias_tensor_info.index, *reshape_bias_tensor_index, new_shape));
  }

  const TensorIndex output_tensor_index =
      SerializeOutputTensorInfo(instance_normalization.output_operand_id).index;
  return SerializeNormalizationOperation(
      input_tensor_info.dimensions, input_tensor_type, input_tensor_index,
      output_tensor_index, mean_tensor_index, variance_tensor_index,
      instance_normalization.epsilon, reshape_scale_tensor_index,
      reshape_bias_tensor_index);
}

auto GraphBuilderTflite::SerializeLayerNormalization(
    const mojom::LayerNormalization& layer_normalization)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.layer_normalization_input.Supports(
      GetOperand(layer_normalization.input_operand_id).descriptor));
  ASSIGN_OR_RETURN(
      const TensorInfo& input_tensor_info,
      SerializeInputTensorInfo(layer_normalization.input_operand_id));
  std::optional<TensorIndex> scale_tensor_index;
  if (layer_normalization.scale_operand_id) {
    ASSIGN_OR_RETURN(
        const TensorInfo& scale_tensor_info,
        SerializeInputTensorInfo(*layer_normalization.scale_operand_id));
    scale_tensor_index = TransposeAndReshapeLayerNormalizationScaleBias(
        input_tensor_info.dimensions, scale_tensor_info,
        layer_normalization.axes);
  }
  std::optional<TensorIndex> bias_tensor_index;
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
  const TensorIndex input_tensor_index = input_tensor_info.index;
  const ::tflite::TensorType input_tensor_type = input_tensor_info.data_type;
  const auto [mean_tensor_index, variance_tensor_index] =
      ComputeMeanAndVarianceForNormalization(input_tensor_info.dimensions,
                                             input_tensor_type,
                                             input_tensor_index, signed_axes);

  const TensorIndex output_tensor_index =
      SerializeOutputTensorInfo(layer_normalization.output_operand_id).index;
  return SerializeNormalizationOperation(
      input_tensor_info.dimensions, input_tensor_type, input_tensor_index,
      output_tensor_index, mean_tensor_index, variance_tensor_index,
      layer_normalization.epsilon, scale_tensor_index, bias_tensor_index);
}

auto GraphBuilderTflite::SerializeLeakyRelu(const mojom::LeakyRelu& leaky_relu)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.leaky_relu_input.Supports(
      GetOperand(leaky_relu.input_operand_id).descriptor));

  std::optional<TensorInfo> quantized_output =
      CanFuseQuantizeAndGetOutput(leaky_relu);
  const bool fuse_dequantize = quantized_output.has_value();
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(
                       leaky_relu.input_operand_id, /*quantize_params=*/0,
                       /*operation_supports_float16=*/false, fuse_dequantize));

  TensorIndex output_tensor_index =
      fuse_dequantize
          ? quantized_output->index
          : SerializeOutputTensorInfo(leaky_relu.output_operand_id).index;

  const auto leaky_rely_options =
      ::tflite::CreateLeakyReluOptions(builder_, leaky_relu.alpha);

  return SerializeUnaryOperation(::tflite::BuiltinOperator_LEAKY_RELU,
                                 input_tensor_info.index, output_tensor_index,
                                 ::tflite::BuiltinOptions_LeakyReluOptions,
                                 leaky_rely_options.Union());
}

auto GraphBuilderTflite::SerializeLinear(const mojom::Linear& linear)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.linear_input.Supports(
      GetOperand(linear.input_operand_id).descriptor));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(linear.input_operand_id));
  const TensorIndex output_tensor_index =
      SerializeOutputTensorInfo(linear.output_operand_id).index;

  return SerializeLinearOperation(
      input_tensor_info.dimensions, input_tensor_info.data_type,
      input_tensor_info.index, output_tensor_index, linear.alpha, linear.beta);
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
  std::array<TensorIndex, 2> bool_tensor_indexes;
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
  const OperatorCodeIndex operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_LOGICAL_NOT);
  const std::array<TensorIndex, 1> op_inputs = {bool_tensor_indexes[0]};
  const std::array<TensorIndex, 1> op_outputs = {bool_tensor_indexes[1]};
  operators_.emplace_back(
      ::tflite::CreateOperator(builder_, operator_code_index,
                               builder_.CreateVector<TensorIndex>(op_inputs),
                               builder_.CreateVector<TensorIndex>(op_outputs)));

  return SerializeCastOperation(
      bool_tensor_indexes[1],
      /*input_tensor_type=*/::tflite::TensorType_BOOL, output_tensor_info.index,
      /*output_tensor_type=*/::tflite::TensorType_UINT8);
}

auto GraphBuilderTflite::SerializeLstmCell(const mojom::LstmCell& lstm_cell)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.lstm_cell_input.SupportsAll(
      {GetOperand(lstm_cell.input_operand_id).descriptor,
       GetOperand(lstm_cell.weight_operand_id).descriptor,
       GetOperand(lstm_cell.recurrent_weight_operand_id).descriptor,
       GetOperand(lstm_cell.hidden_state_operand_id).descriptor,
       GetOperand(lstm_cell.cell_state_operand_id).descriptor}));
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
  std::optional<TensorIndex> bias_tensor_index;
  if (lstm_cell.bias_operand_id) {
    CHECK(context_properties_.data_type_limits.lstm_cell_bias.Supports(
        GetOperand(lstm_cell.bias_operand_id.value()).descriptor));
    ASSIGN_OR_RETURN(const TensorInfo& bias_tensor_info,
                     SerializeInputTensorInfo(*lstm_cell.bias_operand_id));
    bias_tensor_index = bias_tensor_info.index;
  }
  std::optional<TensorIndex> recurrent_bias_tensor_index;
  if (lstm_cell.recurrent_bias_operand_id) {
    CHECK(context_properties_.data_type_limits.lstm_cell_bias.Supports(
        GetOperand(lstm_cell.recurrent_bias_operand_id.value()).descriptor));
    ASSIGN_OR_RETURN(
        const TensorInfo& recurrent_bias_tensor_info,
        SerializeInputTensorInfo(*lstm_cell.recurrent_bias_operand_id));
    recurrent_bias_tensor_index = recurrent_bias_tensor_info.index;
  }
  std::optional<TensorIndex> peephole_weight_tensor_index;
  if (lstm_cell.peephole_weight_operand_id) {
    CHECK(context_properties_.data_type_limits.lstm_cell_bias.Supports(
        GetOperand(lstm_cell.peephole_weight_operand_id.value()).descriptor));
    ASSIGN_OR_RETURN(
        const TensorInfo& peephole_weight_tensor_info,
        SerializeInputTensorInfo(*lstm_cell.peephole_weight_operand_id));
    peephole_weight_tensor_index = peephole_weight_tensor_info.index;
  }
  base::FixedArray<TensorIndex> output_tensor_indices(
      lstm_cell.output_operand_ids.size());
  for (size_t i = 0; i < lstm_cell.output_operand_ids.size(); ++i) {
    output_tensor_indices[i] =
        SerializeOutputTensorInfo(lstm_cell.output_operand_ids[i]).index;
  }

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
    std::optional<OperandId> state_operand_id,
    base::span<const int32_t> state_dimensions)
    -> base::expected<TensorIndex, std::string> {
  TensorIndex state_tensor_index;
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

GraphBuilderTflite::TensorIndex GraphBuilderTflite::ReshapeHiddenAndCellState(
    ::tflite::TensorType input_tensor_type,
    TensorIndex input_tensor_index,
    base::span<const int32_t> new_shape,
    std::optional<TensorIndex> concat_input_tensor_index,
    base::span<const int32_t> concat_output_shape) {
  // Reshape the output hidden or cell state tensor of gru / lstm cell to the
  // 3-D tensor [1, batchSize, hiddenSize].
  const TensorIndex out_tensor_index_of_shape =
      SerializeTemporaryTensor(new_shape, input_tensor_type);
  operators_.emplace_back(SerializeReshapeOperation(
      input_tensor_index, out_tensor_index_of_shape, new_shape));
  if (concat_input_tensor_index) {
    const TensorIndex concat_output_tensor_index =
        SerializeTemporaryTensor(concat_output_shape, input_tensor_type);
    operators_.emplace_back(SerializeConcatOperation(
        std::array<TensorIndex, 2>(
            {*concat_input_tensor_index, out_tensor_index_of_shape}),
        concat_output_tensor_index, /*axis=*/0));
    return concat_output_tensor_index;
  }

  return out_tensor_index_of_shape;
}

auto GraphBuilderTflite::SerializeMatmul(const mojom::Matmul& matmul)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.matmul_input.SupportsAll(
      {GetOperand(matmul.a_operand_id).descriptor,
       GetOperand(matmul.b_operand_id).descriptor}));
  ASSIGN_OR_RETURN(const TensorInfo& a_tensor_info,
                   SerializeInputTensorInfo(matmul.a_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& b_tensor_info,
                   SerializeInputTensorInfo(matmul.b_operand_id));
  const TensorIndex output_tensor_index =
      SerializeOutputTensorInfo(matmul.output_operand_id).index;

  return SerializeMatmulOperation(a_tensor_info.index, b_tensor_info.index,
                                  output_tensor_index);
}

auto GraphBuilderTflite::SerializePad(const mojom::Pad& pad)
    -> base::expected<OperatorOffset, std::string> {
  CHECK_EQ(pad.beginning_padding.size(), pad.ending_padding.size());
  CHECK(context_properties_.data_type_limits.pad_input.Supports(
      GetOperand(pad.input_operand_id).descriptor));

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
  const TensorIndex paddings_index =
      SerializeTensorWithBuffer<int32_t>(paddings, paddings_shape);

  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(pad.input_operand_id));
  std::vector<TensorIndex> op_inputs = {input_tensor_info.index,
                                        paddings_index};

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
      const TensorIndex padding_value_index = SerializeTensorWithBuffer<float>(
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
  }

  const TensorIndex output_tensor_index =
      SerializeOutputTensorInfo(pad.output_operand_id).index;
  const OperatorCodeIndex operator_code_index =
      GetOperatorCodeIndex(operator_code);
  const std::array<TensorIndex, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index,
      builder_.CreateVector<TensorIndex>(op_inputs),
      builder_.CreateVector<TensorIndex>(op_outputs), builtin_options_type,
      builtin_options);
}

auto GraphBuilderTflite::SerializePool2d(const mojom::Pool2d& pool2d)
    -> base::expected<OperatorOffset, std::string> {
  // The dilations are not supported in tflite schema.
  if (pool2d.dilations->height != 1 || pool2d.dilations->width != 1) {
    return base::unexpected("Pool2d in tflite doesn't support dilations.");
  }

  ::tflite::BuiltinOperator operator_code;
  std::optional<TensorInfo> quantized_output;
  const mojom::Operand& input_operand = GetOperand(pool2d.input_operand_id);
  switch (pool2d.kind) {
    case mojom::Pool2d::Kind::kAveragePool2d:
      CHECK(context_properties_.data_type_limits.average_pool2d_input.Supports(
          input_operand.descriptor));
      operator_code = ::tflite::BuiltinOperator_AVERAGE_POOL_2D;
      quantized_output = CanFuseQuantizeAndGetOutput(pool2d);
      break;
    case mojom::Pool2d::Kind::kMaxPool2d:
      CHECK(context_properties_.data_type_limits.max_pool2d_input.Supports(
          input_operand.descriptor));
      operator_code = ::tflite::BuiltinOperator_MAX_POOL_2D;
      quantized_output = CanFuseQuantizeAndGetOutput(pool2d);
      break;
    case mojom::Pool2d::Kind::kL2Pool2d:
      // TODO(crbug.com/361717758): Support L2Pool2d.
      return base::unexpected("L2Pool2d is not supported in tflite.");
  }

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
  ASSIGN_OR_RETURN(
      const TensorInfo& input_tensor_info,
      SerializeInputTensorInfo(
          pool2d.input_operand_id,
          /*quantize_params=*/0,
          /*operation_supports_float16=*/false,
          /*fuse_dequantize_quantize=*/quantized_output.has_value()));
  // Insert a Pad operator before TfLite Pool2d if needed for explicit padding.
  std::optional<TensorIndex> explicit_pad_index;
  if (padding_mode.paddings) {
    ASSIGN_OR_RETURN(
        explicit_pad_index,
        InsertPadOperation(input_tensor_info, padding_mode.paddings.value()));
  }

  const auto pool_2d_options = ::tflite::CreatePool2DOptions(
      builder_, padding_mode.mode, pool2d.strides->width,
      pool2d.strides->height, filter_size2d.width, filter_size2d.height,
      ::tflite::ActivationFunctionType_NONE);

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  TensorIndex output_tensor_index =
      quantized_output
          ? quantized_output->index
          : SerializeOutputTensorInfo(pool2d.output_operand_id).index;
  const OperatorCodeIndex operator_code_index =
      GetOperatorCodeIndex(operator_code);
  const std::array<TensorIndex, 1> op_inputs = {explicit_pad_index
                                                    ? explicit_pad_index.value()
                                                    : input_tensor_info.index};
  const std::array<TensorIndex, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index,
      builder_.CreateVector<TensorIndex>(op_inputs),
      builder_.CreateVector<TensorIndex>(op_outputs),
      ::tflite::BuiltinOptions_Pool2DOptions, pool_2d_options.Union());
}

base::FixedArray<int64_t> GraphBuilderTflite::GetInt64ZeroPointFromInt4(
    OperandId zero_point_operand_id) {
  const mojom::Operand& operand = GetOperand(zero_point_operand_id);
  const size_t size = operand.descriptor.NumberOfElements();
  CHECK_EQ(operand.kind, mojom::Operand::Kind::kConstant);
  CHECK_EQ(operand.descriptor.data_type(), OperandDataType::kInt4);

  auto it = constant_operands_->find(zero_point_operand_id);
  CHECK(it != constant_operands_->end());
  base::span<const uint8_t> byte_span = it->second->ByteSpan();

  base::FixedArray<int64_t> int64_value(size);
  const int64_t int4_sign_mask = 0x08;
  for (size_t i = 0; i < size; ++i) {
    const size_t byte_index = i / 2;
    int64_t value = (byte_span[byte_index] >> ((i % 2) * 4)) & 0xF;
    // Sign-extend if necessary.
    if (value & int4_sign_mask) {
      value |= 0xFFFFFFFFFFFFFFF0;
    }
    int64_value[i] = value;
  }
  return int64_value;
}

base::FixedArray<int64_t> GraphBuilderTflite::GetConstantInt64Value(
    OperandId operand_id) {
  const mojom::Operand& operand = GetOperand(operand_id);
  base::FixedArray<int64_t> typed_value(operand.descriptor.NumberOfElements());
  switch (operand.descriptor.data_type()) {
    case OperandDataType::kInt4: {
      return GetInt64ZeroPointFromInt4(operand_id);
    }
    case OperandDataType::kInt8: {
      std::ranges::copy(GetConstantValue<int8_t>(operand_id),
                        typed_value.begin());
      break;
    }
    case OperandDataType::kUint8: {
      std::ranges::copy(GetConstantValue<uint8_t>(operand_id),
                        typed_value.begin());
      break;
    }
    case OperandDataType::kInt32: {
      std::ranges::copy(GetConstantValue<int32_t>(operand_id),
                        typed_value.begin());
      break;
    }
    case OperandDataType::kUint32: {
      std::ranges::copy(GetConstantValue<uint32_t>(operand_id),
                        typed_value.begin());
      break;
    }
    case OperandDataType::kInt64: {
      std::ranges::copy(GetConstantValue<int64_t>(operand_id),
                        typed_value.begin());
      break;
    }
    case OperandDataType::kFloat32:
    case OperandDataType::kFloat16:
    case OperandDataType::kUint64:
    case OperandDataType::kUint4:
      NOTREACHED() << "This data type is not supported.";
  }
  return typed_value;
}

auto GraphBuilderTflite::SerializeQuantizeParams(
    OperandId zero_point_operand_id,
    OperandId scale_operand_id,
    size_t input_rank) -> std::optional<QuantizateParametersOffset> {
  const mojom::Operand& scale_operand = GetOperand(scale_operand_id);
  const mojom::Operand& zero_point_operand = GetOperand(zero_point_operand_id);
  if (scale_operand.kind != mojom::Operand::Kind::kConstant ||
      zero_point_operand.kind != mojom::Operand::Kind::kConstant) {
    return std::nullopt;
  }

  // The shape of scale is the same as zero point.
  const std::vector<uint32_t>& scale_shape = scale_operand.descriptor.shape();
  // The scale are broadcastable that is validated before calling.
  std::optional<size_t> axis;
  for (size_t i = 0; i < scale_shape.size(); ++i) {
    if (scale_shape[i] != 1) {
      // The scale doesn't support per-channel quantization.
      if (axis) {
        return std::nullopt;
      }
      axis = (input_rank - scale_shape.size()) + i;
    }
  }

  base::FixedArray<int64_t> zero_point_vale =
      GetConstantInt64Value(zero_point_operand_id);
  base::span<const float> scale_value =
      GetConstantValue<float>(scale_operand_id);
  flatbuffers::Offset<flatbuffers::Vector<float>> scale_offset =
      builder_.CreateVector<float>(scale_value);
  flatbuffers::Offset<flatbuffers::Vector<int64_t>> zero_point_offset =
      builder_.CreateVector<int64_t>(zero_point_vale);
  QuantizateParametersOffset quantize_params;
  if (axis) {
    auto checked_axis = base::MakeCheckedNum<int32_t>(*axis);
    if (!checked_axis.IsValid()) {
      return std::nullopt;
    }
    // Per-channel quantization.
    quantize_params = ::tflite::CreateQuantizationParameters(
        builder_, /*min=*/0, /*max=*/0, /*scale=*/scale_offset,
        /*zero point=*/zero_point_offset, ::tflite::QuantizationDetails_NONE, 0,
        checked_axis.ValueOrDie());
  } else {
    // Per-node quantization.
    quantize_params = ::tflite::CreateQuantizationParameters(
        builder_, /*min=*/0, /*max=*/0, scale_offset, zero_point_offset);
  }
  quantize_param_data_.try_emplace(
      quantize_params.o,
      std::make_pair(scale_operand_id, zero_point_operand_id));
  return quantize_params;
}

auto GraphBuilderTflite::SerializeQuantizeLinear(
    const mojom::QuantizeLinear& quantize_linear)
    -> base::expected<OperatorOffset, std::string> {
  const mojom::Operand& input_operand =
      GetOperand(quantize_linear.input_operand_id);
  const mojom::Operand& scale_operand =
      GetOperand(quantize_linear.scale_operand_id);
  const mojom::Operand& zero_point_operand =
      GetOperand(quantize_linear.zero_point_operand_id);
  CHECK(context_properties_.data_type_limits.quantize_linear_input.SupportsAll(
      {input_operand.descriptor, scale_operand.descriptor}));
  CHECK(
      context_properties_.data_type_limits.quantize_linear_zero_point.Supports(
          zero_point_operand.descriptor));

  // TODO(crbug.com/377172670): Add emulation support for block-wise
  // quantizeLinear.
  if (!BroadcastShapes(scale_operand.descriptor.shape(),
                       input_operand.descriptor.shape(),
                       /*bidirectional=*/false)) {
    return base::unexpected("QuantizeLinear can't support block-wise.");
  }

  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(quantize_linear.input_operand_id));
  CHECK_EQ(input_tensor_info.data_type, ::tflite::TensorType_FLOAT32);
  std::optional<QuantizateParametersOffset> quantize_params =
      SerializeQuantizeParams(quantize_linear.zero_point_operand_id,
                              quantize_linear.scale_operand_id,
                              input_tensor_info.dimensions.size());
  if (quantize_params &&
      zero_point_operand.descriptor.data_type() != OperandDataType::kInt32) {
    const TensorIndex output_tensor_index =
        SerializeOutputTensorInfo(quantize_linear.output_operand_id,
                                  *quantize_params)
            .index;
    const OperatorCodeIndex operator_code_index =
        GetOperatorCodeIndex(::tflite::BuiltinOperator_QUANTIZE);
    const std::array<TensorIndex, 1> op_inputs = {input_tensor_info.index};
    const std::array<TensorIndex, 1> op_outputs = {output_tensor_index};
    return ::tflite::CreateOperator(
        builder_, operator_code_index,
        builder_.CreateVector<TensorIndex>(op_inputs),
        builder_.CreateVector<TensorIndex>(op_outputs));
  } else {
    // Emulate the quantize operation whose calculation follows the expression
    // `clamp(tfl.round(input / scale) + zeroPoint, 0, 255)`.
    ASSIGN_OR_RETURN(
        const TensorInfo& scale_tensor_info,
        SerializeInputTensorInfo(quantize_linear.scale_operand_id));
    CHECK_EQ(scale_tensor_info.data_type, ::tflite::TensorType_FLOAT32);
    const TensorIndex div_tensor_index = SerializeTemporaryTensor(
        input_tensor_info.dimensions, ::tflite::TensorType_FLOAT32);
    operators_.emplace_back(SerializeBinaryOperation(
        ::tflite::BuiltinOperator_DIV, input_tensor_info.index,
        scale_tensor_info.index, div_tensor_index));

    const TensorIndex round_tensor_index = SerializeTemporaryTensor(
        input_tensor_info.dimensions, ::tflite::TensorType_FLOAT32);
    operators_.emplace_back(SerializeUnaryOperation(
        ::tflite::BuiltinOperator_ROUND, div_tensor_index, round_tensor_index));

    ASSIGN_OR_RETURN(
        const TensorInfo& zero_point_tensor_info,
        SerializeInputTensorInfo(quantize_linear.zero_point_operand_id));
    const TensorIndex float32_zero_point_tensor_index =
        SerializeTemporaryTensor(zero_point_tensor_info.dimensions,
                                 ::tflite::TensorType_FLOAT32);
    operators_.emplace_back(SerializeCastOperation(
        zero_point_tensor_info.index,
        /*input_tensor_type=*/zero_point_tensor_info.data_type,
        float32_zero_point_tensor_index,
        /*output_tensor_type=*/::tflite::TensorType_FLOAT32));

    const TensorIndex add_zero_point_tensor_index = SerializeTemporaryTensor(
        input_tensor_info.dimensions, ::tflite::TensorType_FLOAT32);
    operators_.emplace_back(SerializeBinaryOperation(
        ::tflite::BuiltinOperator_ADD, round_tensor_index,
        float32_zero_point_tensor_index, add_zero_point_tensor_index));

    const TensorInfo output_tensor_info = SerializeOutputTensorInfo(
        quantize_linear.output_operand_id, quantize_params.value_or(0));
    float min_value, max_value;
    if (output_tensor_info.data_type == ::tflite::TensorType_INT8) {
      min_value = -128.0f;
      max_value = 127.0f;
    } else if (output_tensor_info.data_type == ::tflite::TensorType_UINT8) {
      min_value = 0.0f;
      max_value = 255.0f;
    } else if (output_tensor_info.data_type == ::tflite::TensorType_INT32) {
      min_value = -2147483648.0f;
      max_value = 2147483647.0f;
    } else {
      NOTREACHED() << "This data type is not supported.";
    }
    const TensorIndex clamp_tensor_index = SerializeTemporaryTensor(
        input_tensor_info.dimensions, ::tflite::TensorType_FLOAT32);
    operators_.emplace_back(SerializeSubGraphMaxMin<float>(
        TensorInfo(add_zero_point_tensor_index, ::tflite::TensorType_FLOAT32,
                   input_tensor_info.dimensions),
        clamp_tensor_index, std::array<float, 1>{min_value},
        std::array<float, 1>{max_value}));

    return SerializeCastOperation(
        clamp_tensor_index,
        /*input_tensor_type=*/::tflite::TensorType_FLOAT32,
        output_tensor_info.index,
        /*output_tensor_type=*/output_tensor_info.data_type);
  }
}

auto GraphBuilderTflite::SerializeDequantizeLinear(
    const mojom::DequantizeLinear& dequantize_linear)
    -> base::expected<OperatorOffset, std::string> {
  const mojom::Operand& input_operand =
      GetOperand(dequantize_linear.input_operand_id);
  CHECK(context_properties_.data_type_limits.dequantize_linear_input.Supports(
      input_operand.descriptor));
  const mojom::Operand& scale_operand =
      GetOperand(dequantize_linear.scale_operand_id);
  CHECK(context_properties_.data_type_limits.dequantize_linear_scale.Supports(
      scale_operand.descriptor));
  const mojom::Operand& zero_point_operand =
      GetOperand(dequantize_linear.zero_point_operand_id);
  CHECK(context_properties_.data_type_limits.dequantize_linear_zero_point
            .Supports(zero_point_operand.descriptor));

  // TODO(crbug.com/377172670): Add emulation support for block-wise
  // dequantizeLinear.
  if (!BroadcastShapes(scale_operand.descriptor.shape(),
                       input_operand.descriptor.shape(),
                       /*bidirectional=*/false)) {
    return base::unexpected("DequantizeLinear can't support block-wise.");
  }

  std::optional<QuantizateParametersOffset> quantize_params =
      SerializeQuantizeParams(dequantize_linear.zero_point_operand_id,
                              dequantize_linear.scale_operand_id,
                              input_operand.descriptor.shape().size());

  // TODO(crbug.com/375614289): Support constant input after TFLite runtime fix
  // the issue https://github.com/tensorflow/tensorflow/issues/78748.
  if (quantize_params &&
      !IsSerializedWithMismatchQuantizeParameters(
          dequantize_linear.input_operand_id, *quantize_params) &&
      input_operand.kind != mojom::Operand::Kind::kConstant &&
      input_operand.descriptor.data_type() != OperandDataType::kInt32) {
    ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                     SerializeInputTensorInfo(
                         dequantize_linear.input_operand_id, *quantize_params));
    const TensorIndex output_tensor_index =
        SerializeOutputTensorInfo(dequantize_linear.output_operand_id).index;
    const OperatorCodeIndex operator_code_index =
        GetOperatorCodeIndex(::tflite::BuiltinOperator_DEQUANTIZE);
    const std::array<TensorIndex, 1> op_inputs = {input_tensor_info.index};
    const std::array<TensorIndex, 1> op_outputs = {output_tensor_index};
    return ::tflite::CreateOperator(
        builder_, operator_code_index,
        builder_.CreateVector<TensorIndex>(op_inputs),
        builder_.CreateVector<TensorIndex>(op_outputs));
  } else {
    // Emulate the dequantize operation whose calculation follows the expression
    // `output = (input - zeroPoint) * scale`.
    ASSIGN_OR_RETURN(
        const TensorInfo& input_tensor_info,
        SerializeInputTensorInfo(dequantize_linear.input_operand_id));
    const TensorIndex float32_input_tensor_index = SerializeTemporaryTensor(
        input_tensor_info.dimensions, ::tflite::TensorType_FLOAT32);
    operators_.emplace_back(SerializeCastOperation(
        input_tensor_info.index,
        /*input_tensor_type=*/input_tensor_info.data_type,
        float32_input_tensor_index,
        /*output_tensor_type=*/::tflite::TensorType_FLOAT32));

    ASSIGN_OR_RETURN(
        const TensorInfo& zero_point_tensor_info,
        SerializeInputTensorInfo(dequantize_linear.zero_point_operand_id));
    const TensorIndex float32_zero_point_tensor_index =
        SerializeTemporaryTensor(zero_point_tensor_info.dimensions,
                                 ::tflite::TensorType_FLOAT32);
    operators_.emplace_back(SerializeCastOperation(
        zero_point_tensor_info.index,
        /*input_tensor_type=*/zero_point_tensor_info.data_type,
        float32_zero_point_tensor_index,
        /*output_tensor_type=*/::tflite::TensorType_FLOAT32));

    const TensorIndex output_tensor_index_of_sub = SerializeTemporaryTensor(
        input_tensor_info.dimensions, ::tflite::TensorType_FLOAT32);
    operators_.emplace_back(SerializeBinaryOperation(
        ::tflite::BuiltinOperator_SUB, float32_input_tensor_index,
        float32_zero_point_tensor_index, output_tensor_index_of_sub));

    ASSIGN_OR_RETURN(
        const TensorInfo& scale_tensor_info,
        SerializeInputTensorInfo(dequantize_linear.scale_operand_id));
    const TensorIndex output_tensor_index =
        SerializeOutputTensorInfo(dequantize_linear.output_operand_id,
                                  quantize_params.value_or(0))
            .index;

    return SerializeBinaryOperation(
        ::tflite::BuiltinOperator_MUL, output_tensor_index_of_sub,
        scale_tensor_info.index, output_tensor_index);
  }
}

auto GraphBuilderTflite::SerializePrelu(const mojom::Prelu& prelu)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.prelu_input.Supports(
      GetOperand(prelu.input_operand_id).descriptor));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(prelu.input_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& slope_tensor_info,
                   SerializeInputTensorInfo(prelu.slope_operand_id));
  const TensorIndex output_tensor_index =
      SerializeOutputTensorInfo(prelu.output_operand_id).index;

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

  const OperatorCodeIndex operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_PRELU);
  const std::array<TensorIndex, 2> op_inputs = {input_tensor_info.index,
                                                slope_tensor_info.index};
  const std::array<TensorIndex, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index,
      builder_.CreateVector<TensorIndex>(op_inputs),
      builder_.CreateVector<TensorIndex>(op_outputs));
}

auto GraphBuilderTflite::SerializeReciprocal(
    const TensorInfo& input_tensor_info,
    const TensorInfo& output_tensor_info)
    -> base::expected<OperatorOffset, std::string> {
  // Emulate the reciprocal operation whose calculation follows the expression
  // `1 / x`.
  CHECK_EQ(input_tensor_info.data_type, ::tflite::TensorType_FLOAT32);

  const TensorIndex constant_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{1.0},
      /*dimensions=*/{});

  return SerializeBinaryOperation(
      ::tflite::BuiltinOperator_DIV, constant_tensor_index,
      input_tensor_info.index, output_tensor_info.index);
}

auto GraphBuilderTflite::SerializeReduce(const mojom::Reduce& reduce)
    -> base::expected<OperatorOffset, std::string> {
  std::optional<TensorInfo> quantized_output =
      CanFuseQuantizeAndGetOutput(reduce);
  const bool fuse_dequantize = quantized_output.has_value();
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(
                       reduce.input_operand_id, /*quantize_params=*/0,
                       /*operation_supports_float16=*/false, fuse_dequantize));

  const TensorIndex output_tensor_index =
      fuse_dequantize
          ? quantized_output->index
          : SerializeOutputTensorInfo(reduce.output_operand_id).index;

  // Serialize the axes tensor to reduce input tensor.
  ASSIGN_OR_RETURN(const std::vector<int32_t> signed_axes,
                   ToSignedDimensions(reduce.axes));

  ::tflite::BuiltinOperator operator_code;
  TensorIndex input_tensor_index = input_tensor_info.index;
  const DataTypeLimits& data_type_limits = context_properties_.data_type_limits;
  const OperandDescriptor& input_descriptor =
      GetOperand(reduce.input_operand_id).descriptor;
  switch (reduce.kind) {
    case mojom::Reduce::Kind::kMax:
      CHECK(data_type_limits.reduce_max_input.Supports(input_descriptor));
      operator_code = ::tflite::BuiltinOperator_REDUCE_MAX;
      break;
    case mojom::Reduce::Kind::kMean:
      CHECK(data_type_limits.reduce_mean_input.Supports(input_descriptor));
      operator_code = ::tflite::BuiltinOperator_MEAN;
      break;
    case mojom::Reduce::Kind::kMin:
      CHECK(data_type_limits.reduce_min_input.Supports(input_descriptor));
      operator_code = ::tflite::BuiltinOperator_REDUCE_MIN;
      break;
    case mojom::Reduce::Kind::kProduct:
      CHECK(data_type_limits.reduce_product_input.Supports(input_descriptor));
      operator_code = ::tflite::BuiltinOperator_REDUCE_PROD;
      break;
    case mojom::Reduce::Kind::kSum:
      CHECK(data_type_limits.reduce_sum_input.Supports(input_descriptor));
      operator_code = ::tflite::BuiltinOperator_SUM;
      break;
    case mojom::Reduce::Kind::kLogSum:
      CHECK(data_type_limits.reduce_log_sum_input.Supports(input_descriptor));
      // The reduceLogSum can be emulated with appending log operation after
      // reduceSum.
      operator_code = ::tflite::BuiltinOperator_SUM;
      break;
    case mojom::Reduce::Kind::kLogSumExp: {
      // The reduceLogSumExp can be emulated with adding exp operation before
      // reduceSum and appending log operation after the reduceSum.
      CHECK(
          data_type_limits.reduce_log_sum_exp_input.Supports(input_descriptor));
      const TensorIndex output_tensor_index_of_exp = SerializeTemporaryTensor(
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
      CHECK(data_type_limits.reduce_l2_input.Supports(input_descriptor));
      // The reduceL2 can be emulated with appending sqrt operation after
      // reduceSumSquare.
      const TensorIndex output_tensor_index_of_sum = SerializeTemporaryTensor(
          input_tensor_info.dimensions, input_tensor_info.data_type);
      ASSIGN_OR_RETURN(OperatorOffset operator_offset,
                       SerializeReduceSumSquare(input_tensor_info, signed_axes,
                                                reduce.keep_dimensions,
                                                output_tensor_index_of_sum));
      operators_.emplace_back(operator_offset);
      return SerializeSquareRootOperation(output_tensor_index_of_sum,
                                          input_tensor_info.data_type,
                                          output_tensor_index);
    }
    case mojom::Reduce::Kind::kSumSquare: {
      // The reduceSumSquare can be emulated with adding pow operation before
      // reduceSum.
      CHECK(
          data_type_limits.reduce_sum_square_input.Supports(input_descriptor));
      return SerializeReduceSumSquare(input_tensor_info, signed_axes,
                                      reduce.keep_dimensions,
                                      output_tensor_index);
    }
    case mojom::Reduce::Kind::kL1: {
      CHECK(data_type_limits.reduce_l1_input.Supports(input_descriptor));
      // The reduceL1 can be emulated with adding abs operation before
      // reduceSum.
      const TensorIndex output_tensor_index_of_abs = SerializeTemporaryTensor(
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
    const TensorIndex output_tensor_index_of_sum = SerializeTemporaryTensor(
        input_tensor_info.dimensions, input_tensor_info.data_type);
    operators_.emplace_back(SerializeReduceOperation(
        operator_code, input_tensor_index, output_tensor_index_of_sum,
        signed_axes, reduce.keep_dimensions));
    return SerializeUnaryOperation(::tflite::BuiltinOperator_LOG,
                                   output_tensor_index_of_sum,
                                   output_tensor_index);
  }

  return SerializeReduceOperation(operator_code, input_tensor_index,
                                  output_tensor_index, signed_axes,
                                  reduce.keep_dimensions);
}

auto GraphBuilderTflite::SerializeReduceSumSquare(
    const TensorInfo& input_tensor_info,
    base::span<const int32_t> axes,
    bool keep_dimensions,
    TensorIndex output_tensor_index)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(input_tensor_info.data_type == ::tflite::TensorType_FLOAT32 ||
        input_tensor_info.data_type == ::tflite::TensorType_INT32);
  // The reduceSumSquare can be emulated with adding square operation before
  // reduceSum.
  const TensorIndex output_tensor_index_of_square = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeSquareOperation(
      input_tensor_info.index, input_tensor_info.data_type,
      output_tensor_index_of_square));

  return SerializeReduceOperation(::tflite::BuiltinOperator_SUM,
                                  output_tensor_index_of_square,
                                  output_tensor_index, axes, keep_dimensions);
}

auto GraphBuilderTflite::SerializeRelu(const mojom::Relu& relu)
    -> base::expected<OperatorOffset, std::string> {
  // TODO(crbug.com/354625677): Support 32-bit signed integer with
  // TFL::MaximumOp
  // https://www.tensorflow.org/mlir/tfl_ops#tflmaximum_tflmaximumop.
  CHECK(context_properties_.data_type_limits.relu_input.Supports(
      GetOperand(relu.input_operand_id).descriptor));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(relu.input_operand_id));
  const TensorIndex output_tensor_index =
      SerializeOutputTensorInfo(relu.output_operand_id).index;

  return SerializeUnaryOperation(
      ::tflite::BuiltinOperator::BuiltinOperator_RELU, input_tensor_info.index,
      output_tensor_index);
}

auto GraphBuilderTflite::SerializeResample2d(
    const mojom::Resample2d& resample2d)
    -> base::expected<OperatorOffset, std::string> {
  // TODO: crbug.com/329543543 - `resample2d.scales` is dropped on the floor.
  CHECK(context_properties_.data_type_limits.resample2d_input.Supports(
      GetOperand(resample2d.input_operand_id).descriptor));

  const std::array<uint32_t, 2> supported_axes = {1, 2};
  CHECK(std::ranges::equal(resample2d.axes, supported_axes));

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
  const TensorInfo output_tensor_info =
      SerializeOutputTensorInfo(resample2d.output_operand_id);
  int32_t output_height = output_tensor_info.dimensions[resample2d.axes[0]];
  int32_t output_width = output_tensor_info.dimensions[resample2d.axes[1]];

  const std::array<int32_t, 2> resize_data = {output_height, output_width};
  const std::array<int32_t, 1> resize_shape = {resize_data.size()};
  const TensorIndex resize_tensor_index =
      SerializeTensorWithBuffer<int32_t>(resize_data, resize_shape);

  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(resample2d.input_operand_id));
  const OperatorCodeIndex operator_code_index =
      GetOperatorCodeIndex(operator_code);
  const std::array<TensorIndex, 2> op_inputs = {input_tensor_info.index,
                                                resize_tensor_index};
  const std::array<TensorIndex, 1> op_outputs = {output_tensor_info.index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index,
      builder_.CreateVector<TensorIndex>(op_inputs),
      builder_.CreateVector<TensorIndex>(op_outputs), builtin_options_type,
      builtin_options);
}

auto GraphBuilderTflite::SerializeReshape(const mojom::Reshape& reshape)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.reshape_input.Supports(
      GetOperand(reshape.input_operand_id).descriptor));

  std::optional<TensorInfo> quantized_output =
      CanFuseQuantizeAndGetOutput(reshape);
  const bool fuse_dequantize = quantized_output.has_value();
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(
                       reshape.input_operand_id, /*quantize_params=*/0,
                       /*operation_supports_float16=*/true, fuse_dequantize));

  TensorIndex output_tensor_index;
  std::vector<int32_t> output_tensor_shape;
  if (fuse_dequantize) {
    output_tensor_index = quantized_output->index;
    output_tensor_shape = std::move(quantized_output->dimensions);
  } else {
    TensorInfo output_tensor_info = SerializeOutputTensorInfo(
        reshape.output_operand_id, /*quantize_params=*/0,
        /*operation_supports_float16=*/true, input_tensor_info.data_type);
    output_tensor_index = output_tensor_info.index;
    output_tensor_shape = std::move(output_tensor_info.dimensions);
  }

  return SerializeReshapeOperation(input_tensor_info.index, output_tensor_index,
                                   output_tensor_shape);
}

auto GraphBuilderTflite::SerializeReverse(const mojom::Reverse& reverse)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.reverse_input.Supports(
      GetOperand(reverse.input_operand_id).descriptor));

  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(reverse.input_operand_id));
  const TensorInfo output_tensor_info =
      SerializeOutputTensorInfo(reverse.output_operand_id);
  // Don't reverse if the axes are empty.
  if (reverse.axes.empty()) {
    return SerializeIdentityOperation(input_tensor_info.index,
                                      output_tensor_info.index,
                                      output_tensor_info.dimensions);
  }

  // The TFLite kernel of reverse only supports contiguous axes, so the input
  // tensor need to be reversed slice by slice.
  ASSIGN_OR_RETURN(std::vector<int32_t> signed_axes,
                   ToSignedDimensions(reverse.axes));
  std::ranges::sort(signed_axes);
  std::vector<int32_t> contiguous_axes = {signed_axes[0]};
  std::optional<TensorIndex> previous_reverse_tensor_index;
  for (size_t i = 1; i < signed_axes.size(); ++i) {
    if (signed_axes[i] != signed_axes[i - 1] + 1) {
      const TensorIndex reverse_tensor_index = SerializeTemporaryTensor(
          input_tensor_info.dimensions, input_tensor_info.data_type);
      operators_.emplace_back(SerializeReverseOperation(
          previous_reverse_tensor_index.value_or(input_tensor_info.index),
          contiguous_axes, reverse_tensor_index));

      previous_reverse_tensor_index = reverse_tensor_index;
      contiguous_axes.clear();
    }
    contiguous_axes.push_back(signed_axes[i]);
  }

  return SerializeReverseOperation(
      previous_reverse_tensor_index.value_or(input_tensor_info.index),
      contiguous_axes, output_tensor_info.index);
}

auto GraphBuilderTflite::SerializeSigmoid(const mojom::Sigmoid& sigmoid)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.sigmoid_input.Supports(
      GetOperand(sigmoid.input_operand_id).descriptor));

  std::optional<TensorInfo> quantized_output =
      CanFuseQuantizeAndGetOutput(sigmoid);
  const bool fuse_dequantize = quantized_output.has_value();
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(
                       sigmoid.input_operand_id, /*quantize_params=*/0,
                       /*operation_supports_float16=*/false, fuse_dequantize));

  TensorIndex output_tensor_index =
      fuse_dequantize
          ? quantized_output->index
          : SerializeOutputTensorInfo(sigmoid.output_operand_id).index;

  return SerializeUnaryOperation(::tflite::BuiltinOperator_LOGISTIC,
                                 input_tensor_info.index, output_tensor_index);
}

auto GraphBuilderTflite::SerializeWebNNScatterND(
    const TensorInfo& input_tensor_info,
    const TensorInfo& updates_tensor_info,
    TensorIndex indices_tensor_index,
    TensorIndex output_tensor_index) -> OperatorOffset {
  base::FixedArray<bool> true_updates(
      std::accumulate(updates_tensor_info.dimensions.begin(),
                      updates_tensor_info.dimensions.end(),
                      static_cast<size_t>(1), std::multiplies()),
      true);
  const TensorIndex true_updates_tensor_index = SerializeTensorWithBuffer<bool>(
      /*buffer=*/true_updates,
      /*dimensions=*/updates_tensor_info.dimensions);

  // Scatter the True values into a zero (False) initialized tensor according to
  // indices.
  const TensorIndex scatter_true_tensor_index = SerializeTemporaryTensor(
      input_tensor_info.dimensions, ::tflite::TensorType_BOOL);
  operators_.emplace_back(SerializeTFLiteScatterND(
      input_tensor_info.dimensions, indices_tensor_index,
      true_updates_tensor_index, scatter_true_tensor_index));

  // Scatter the values of updates into another zero-initialized tensor
  // according to indices.
  const TensorIndex scatter_updates_tensor_index = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeTFLiteScatterND(
      input_tensor_info.dimensions, indices_tensor_index,
      updates_tensor_info.index, scatter_updates_tensor_index));

  // Select scattered value or input value based on condition.
  return SerializeWhereOperation(scatter_true_tensor_index,
                                 scatter_updates_tensor_index,
                                 input_tensor_info.index, output_tensor_index);
}

auto GraphBuilderTflite::SerializeScatterElements(
    const mojom::ScatterElements& scatter_elements)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.scatter_elements_input.Supports(
      GetOperand(scatter_elements.input_operand_id).descriptor));
  const mojom::Operand& indices_operand =
      GetOperand(scatter_elements.indices_operand_id);
  CHECK(context_properties_.data_type_limits.scatter_elements_indices.Supports(
      indices_operand.descriptor));
  if (indices_operand.kind != mojom::Operand::Kind::kConstant) {
    // TODO(crbug.com/377615324): Support user input indices.
    return base::unexpected("scatterElements only supports constant indices.");
  }

  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(scatter_elements.input_operand_id));
  ASSIGN_OR_RETURN(
      const TensorIndex indices_tensor_index,
      SerializeElementsCoordinates<int32_t>(
          indices_operand.descriptor.shape(),
          GetConstantValue<int32_t>(scatter_elements.indices_operand_id),
          input_tensor_info.dimensions, scatter_elements.axis));

  // The TFLite kernel of scatter_nd expects updates operand's shape to be one
  // dimension when indices operand's shape is two dimensions here:
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/tflite/src/tensorflow/lite/kernels/scatter_nd.cc;l=64?q=scatter_nd.cc&ss=chromium%2Fchromium%2Fsrc
  //
  // So reshape updates from updates.descriptor.shape() to one dimension
  // (updates.descriptor.NumberOfElements())
  ASSIGN_OR_RETURN(
      const TensorInfo& updates_tensor_info,
      SerializeInputTensorInfo(scatter_elements.updates_operand_id));
  const std::array<int32_t, 1> updates_new_shape = {base::checked_cast<int32_t>(
      GetOperand(scatter_elements.updates_operand_id)
          .descriptor.NumberOfElements())};
  const TensorIndex reshape_updates_tensor_index = SerializeTemporaryTensor(
      updates_new_shape, updates_tensor_info.data_type);
  operators_.emplace_back(SerializeReshapeOperation(
      updates_tensor_info.index, reshape_updates_tensor_index,
      updates_new_shape));

  const TensorIndex output_tensor_index =
      SerializeOutputTensorInfo(scatter_elements.output_operand_id).index;
  return SerializeWebNNScatterND(
      input_tensor_info,
      TensorInfo(reshape_updates_tensor_index, updates_tensor_info.data_type,
                 updates_new_shape),
      indices_tensor_index, output_tensor_index);
}

auto GraphBuilderTflite::SerializeScatterND(const mojom::ScatterND& scatter_nd)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.scatter_nd_input.Supports(
      GetOperand(scatter_nd.input_operand_id).descriptor));
  CHECK(context_properties_.data_type_limits.scatter_nd_indices.Supports(
      GetOperand(scatter_nd.indices_operand_id).descriptor));

  ASSIGN_OR_RETURN(const TensorInfo& updates_tensor_info,
                   SerializeInputTensorInfo(scatter_nd.updates_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(scatter_nd.input_operand_id));
  ASSIGN_OR_RETURN(const TensorInfo& indices_tensor_info,
                   SerializeInputTensorInfo(scatter_nd.indices_operand_id));
  const TensorIndex output_tensor_index =
      SerializeOutputTensorInfo(scatter_nd.output_operand_id).index;
  return SerializeWebNNScatterND(input_tensor_info, updates_tensor_info,
                                 indices_tensor_info.index,
                                 output_tensor_index);
}

auto GraphBuilderTflite::SerializeSlice(const mojom::Slice& slice)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.slice_input.Supports(
      GetOperand(slice.input_operand_id).descriptor));

  // The number of starts, sizes and strides are the same as input rank that is
  // verified in ValidateSliceAndInferOutput() function.
  base::FixedArray<int32_t> slice_starts(slice.ranges.size());
  base::FixedArray<int32_t> slice_ends(slice.ranges.size());
  base::FixedArray<int32_t> slice_strides(slice.ranges.size());
  for (size_t i = 0; i < slice.ranges.size(); ++i) {
    const auto& range = slice.ranges[i];
    auto checked_start = base::MakeCheckedNum<int32_t>(range.start);
    auto checked_end =
        base::MakeCheckedNum<int32_t>(range.size) + checked_start;
    auto checked_stride = base::MakeCheckedNum<int32_t>(range.stride);
    if (!checked_start.IsValid() || !checked_end.IsValid() ||
        !checked_stride.IsValid()) {
      return base::unexpected(
          "The start, end or stride of slice is too large.");
    }
    slice_starts[i] = checked_start.ValueOrDie();
    slice_ends[i] = checked_end.ValueOrDie();
    slice_strides[i] = checked_stride.ValueOrDie();
  }

  std::optional<TensorInfo> quantized_output =
      CanFuseQuantizeAndGetOutput(slice);
  const bool fuse_dequantize = quantized_output.has_value();
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(
                       slice.input_operand_id, /*quantize_params=*/0,
                       /*operation_supports_float16=*/false, fuse_dequantize));

  const TensorIndex output_tensor_index =
      fuse_dequantize
          ? quantized_output->index
          : SerializeOutputTensorInfo(slice.output_operand_id).index;

  auto checked_number = base::MakeCheckedNum<int32_t>(slice.ranges.size());
  if (!checked_number.IsValid()) {
    return base::unexpected("The input rank is too large.");
  }
  // Serialize the starting index of each input dimension.
  const std::array<int32_t, 1> range_shape = {checked_number.ValueOrDie()};
  const TensorIndex starts_tensor_index =
      SerializeTensorWithBuffer<int32_t>(std::move(slice_starts), range_shape);

  // Serialize the ending index of each input dimension.
  const TensorIndex ends_tensor_index =
      SerializeTensorWithBuffer<int32_t>(std::move(slice_ends), range_shape);

  // Serialize the strides of each input dimension.
  const TensorIndex strides_tensor_index =
      SerializeTensorWithBuffer<int32_t>(std::move(slice_strides), range_shape);

  const OperatorCodeIndex operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_STRIDED_SLICE);
  const std::array<TensorIndex, 4> op_inputs = {
      input_tensor_info.index, starts_tensor_index, ends_tensor_index,
      strides_tensor_index};
  const std::array<TensorIndex, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index,
      builder_.CreateVector<TensorIndex>(op_inputs),
      builder_.CreateVector<TensorIndex>(op_outputs));
}

auto GraphBuilderTflite::SerializeSoftmax(const mojom::Softmax& softmax)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.softmax_input.Supports(
      GetOperand(softmax.input_operand_id).descriptor));

  std::optional<TensorInfo> quantized_output =
      CanFuseQuantizeAndGetOutput(softmax);
  const bool fuse_dequantize = quantized_output.has_value();
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(
                       softmax.input_operand_id, /*quantize_params=*/0,
                       /*operation_supports_float16=*/false, fuse_dequantize));
  const TensorIndex output_tensor_index =
      fuse_dequantize
          ? quantized_output->index
          : SerializeOutputTensorInfo(softmax.output_operand_id).index;

  const size_t input_rank = input_tensor_info.dimensions.size();
  const auto softmax_options =
      ::tflite::CreateSoftmaxOptions(builder_, /*beta=*/1.0);
  if (softmax.axis == input_rank - 1) {
    // The axis is the last dimension, so the softmax operation can be directly
    // serialized.
    return SerializeUnaryOperation(::tflite::BuiltinOperator_SOFTMAX,
                                   input_tensor_info.index, output_tensor_index,
                                   ::tflite::BuiltinOptions_SoftmaxOptions,
                                   softmax_options.Union());
  }
  // Transpose the input tensor to make the axis to be the last dimension.
  std::vector<uint32_t> permutation(input_rank);
  std::iota(permutation.begin(), permutation.end(), 0);
  std::swap(permutation[softmax.axis], permutation[input_rank - 1]);
  std::vector<int32_t> transpose_dimensions = input_tensor_info.dimensions;
  std::swap(transpose_dimensions[softmax.axis],
            transpose_dimensions[input_rank - 1]);

  ::tflite::TensorType data_type = input_tensor_info.data_type;
  QuantizateParametersOffset input_quantize_params = 0;
  QuantizateParametersOffset output_quantize_params = 0;
  if (fuse_dequantize) {
    data_type = quantized_output->data_type;
    input_quantize_params = input_tensor_info.quantize_params;
    output_quantize_params = quantized_output->quantize_params;
  }

  const TensorIndex output_tensor_index_of_transpose = SerializeTemporaryTensor(
      transpose_dimensions, data_type, input_quantize_params);
  operators_.emplace_back(SerializeTransposeOperation(
      input_tensor_info.index, output_tensor_index_of_transpose,
      input_tensor_info.dimensions, permutation));

  // Perform softmax.
  const TensorIndex output_tensor_index_of_softmax = SerializeTemporaryTensor(
      transpose_dimensions, data_type, output_quantize_params);
  operators_.emplace_back(SerializeUnaryOperation(
      ::tflite::BuiltinOperator_SOFTMAX, output_tensor_index_of_transpose,
      output_tensor_index_of_softmax, ::tflite::BuiltinOptions_SoftmaxOptions,
      softmax_options.Union()));

  // Transpose the last dimension back to the original axis.
  return SerializeTransposeOperation(output_tensor_index_of_softmax,
                                     output_tensor_index,
                                     input_tensor_info.dimensions, permutation);
}

auto GraphBuilderTflite::SerializeSoftplus(const mojom::Softplus& softplus)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.softplus_input.Supports(
      GetOperand(softplus.input_operand_id).descriptor));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(softplus.input_operand_id));

  // Emulate the softplus operation whose calculation follows the expression
  // `ln(1 + exp(x))`.
  const TensorIndex output_tensor_index_of_exp = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeUnaryOperation(::tflite::BuiltinOperator_EXP,
                                                  input_tensor_info.index,
                                                  output_tensor_index_of_exp));

  // Add constant value `1` to the output tensor of element-wise exp operation.
  // TODO(crbug.com/339654398): Convert the 32-bit floating-point data to 16-bit
  // floating-point data with fp16_ieee_from_fp32_value function if some
  // delegates support 16-bit float inference.
  const TensorIndex constant_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{1},
      /*dimensions=*/{});
  const TensorIndex output_tensor_index_of_add = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_ADD, constant_tensor_index,
      output_tensor_index_of_exp, output_tensor_index_of_add));

  const TensorIndex output_tensor_index =
      SerializeOutputTensorInfo(softplus.output_operand_id).index;
  return SerializeUnaryOperation(::tflite::BuiltinOperator_LOG,
                                 output_tensor_index_of_add,
                                 output_tensor_index);
}

auto GraphBuilderTflite::SerializeSoftsign(const mojom::Softsign& softsign)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.softsign_input.Supports(
      GetOperand(softsign.input_operand_id).descriptor));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(softsign.input_operand_id));

  // Emulate the softsign operation whose calculation follows the expression
  // `x / (1 + |x|)`.
  const TensorIndex output_tensor_index_of_abs = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeUnaryOperation(::tflite::BuiltinOperator_ABS,
                                                  input_tensor_info.index,
                                                  output_tensor_index_of_abs));

  // Add constant value `1` to the output tensor of element-wise abs operation.
  // TODO(crbug.com/339654398): Convert the 32-bit floating-point data to 16-bit
  // floating-point data with fp16_ieee_from_fp32_value function if some
  // delegates support 16-bit float inference.
  CHECK_EQ(input_tensor_info.data_type, ::tflite::TensorType_FLOAT32);
  const TensorIndex constant_tensor_index = SerializeTensorWithBuffer<float>(
      /*buffer=*/std::array<float, 1>{1},
      /*dimensions=*/{});
  const TensorIndex output_tensor_index_of_add = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeBinaryOperation(
      ::tflite::BuiltinOperator_ADD, constant_tensor_index,
      output_tensor_index_of_abs, output_tensor_index_of_add));

  const TensorIndex output_tensor_index =
      SerializeOutputTensorInfo(softsign.output_operand_id).index;
  return SerializeBinaryOperation(
      ::tflite::BuiltinOperator_DIV, input_tensor_info.index,
      output_tensor_index_of_add, output_tensor_index);
}

auto GraphBuilderTflite::SerializeSplit(const mojom::Split& split)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.split_input.Supports(
      GetOperand(split.input_operand_id).descriptor));

  // Serialize the axis tensor to split input tensor along it.
  const auto checked_axis = base::MakeCheckedNum<int32_t>(split.axis);
  if (!checked_axis.IsValid()) {
    return base::unexpected("The axis is too large.");
  }
  const TensorIndex axis_tensor_index = SerializeTensorWithBuffer<int32_t>(
      /*buffer=*/std::array<int32_t, 1>{checked_axis.ValueOrDie()},
      /*dimensions=*/{});

  std::optional<base::FixedArray<TensorInfo>> quantized_outputs =
      CanFuseQuantizeAndGetOutput(split);
  const bool fuse_dequantize = quantized_outputs.has_value();
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(
                       split.input_operand_id, /*quantize_params=*/0,
                       /*operation_supports_float16=*/false, fuse_dequantize));

  // Serialize the split sizes tensor that specifies the sizes of each output
  // tensor along the axis.
  const size_t outputs_size = split.output_operand_ids.size();
  base::FixedArray<int32_t> split_sizes(outputs_size);
  base::FixedArray<int32_t> op_outputs(outputs_size);
  for (size_t i = 0; i < outputs_size; ++i) {
    if (fuse_dequantize) {
      CHECK_LT(split.axis, quantized_outputs->at(i).dimensions.size());
      split_sizes[i] = quantized_outputs->at(i).dimensions[split.axis];
      op_outputs[i] = quantized_outputs->at(i).index;
    } else {
      const TensorInfo output_tensor_info =
          SerializeOutputTensorInfo(split.output_operand_ids[i]);
      CHECK_LT(split.axis, output_tensor_info.dimensions.size());
      split_sizes[i] = output_tensor_info.dimensions[split.axis];
      op_outputs[i] = output_tensor_info.index;
    }
  }
  const auto checked_split_size =
      base::MakeCheckedNum<int32_t>(split_sizes.size());
  if (!checked_split_size.IsValid()) {
    return base::unexpected("The split size is too large.");
  }
  const std::array<int32_t, 1> split_sizes_shape = {
      checked_split_size.ValueOrDie()};
  const TensorIndex sizes_tensor_index =
      SerializeTensorWithBuffer<int32_t>(split_sizes, split_sizes_shape);

  // Create `tflite::SplitOptions` with the split size.
  const auto split_options = ::tflite::CreateSplitOptions(
      builder_, /*num_splits=*/checked_split_size.ValueOrDie());

  const OperatorCodeIndex operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_SPLIT_V);
  // The order of inputs is input, split sizes tensor and then axis tensor as
  // the described https://www.tensorflow.org/mlir/tfl_ops#operands_130.
  const std::array<TensorIndex, 3> op_inputs = {
      input_tensor_info.index, sizes_tensor_index, axis_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index,
      builder_.CreateVector<TensorIndex>(op_inputs),
      builder_.CreateVector<TensorIndex>(op_outputs),
      ::tflite::BuiltinOptions_SplitVOptions, split_options.Union());
}

auto GraphBuilderTflite::SerializeTan(const TensorInfo& input_tensor_info,
                                      const TensorInfo& output_tensor_info)
    -> OperatorOffset {
  // The tangent operation defines the expression `opposite side / adjacent
  // side` to a right triangle as the described here
  // https://www.mathworks.com/help/matlab/ref/tan.html, it can be emulated with
  // `sin(x)/cos(x)` element-wise.
  const TensorIndex output_tensor_index_of_sin = SerializeTemporaryTensor(
      input_tensor_info.dimensions, input_tensor_info.data_type);
  operators_.emplace_back(SerializeUnaryOperation(::tflite::BuiltinOperator_SIN,
                                                  input_tensor_info.index,
                                                  output_tensor_index_of_sin));

  const TensorIndex output_tensor_index_of_cos = SerializeTemporaryTensor(
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
  CHECK(context_properties_.data_type_limits.tanh_input.Supports(
      GetOperand(tanh.input_operand_id).descriptor));

  std::optional<TensorInfo> quantized_output =
      CanFuseQuantizeAndGetOutput(tanh);
  const bool fuse_dequantize = quantized_output.has_value();
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(
                       tanh.input_operand_id, /*quantize_params=*/0,
                       /*operation_supports_float16=*/false, fuse_dequantize));

  TensorIndex output_tensor_index =
      fuse_dequantize ? quantized_output->index
                      : SerializeOutputTensorInfo(tanh.output_operand_id).index;

  return SerializeUnaryOperation(::tflite::BuiltinOperator_TANH,
                                 input_tensor_info.index, output_tensor_index);
}

auto GraphBuilderTflite::SerializeTile(const mojom::Tile& tile)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.tile_input.Supports(
      GetOperand(tile.input_operand_id).descriptor));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(tile.input_operand_id));
  const TensorIndex output_tensor_index =
      SerializeOutputTensorInfo(tile.output_operand_id).index;

  ASSIGN_OR_RETURN(const std::vector<int32_t> signed_repetitions,
                   ToSignedDimensions(tile.repetitions));
  const std::array<int32_t, 1> repetitions_tensor_shape = {
      base::checked_cast<int32_t>(signed_repetitions.size())};
  const TensorIndex repetitions_tensor_index =
      SerializeTensorWithBuffer<int32_t>(signed_repetitions,
                                         repetitions_tensor_shape);

  const OperatorCodeIndex operator_code_index =
      GetOperatorCodeIndex(::tflite::BuiltinOperator_TILE);
  const std::array<TensorIndex, 2> op_inputs = {input_tensor_info.index,
                                                repetitions_tensor_index};
  const std::array<TensorIndex, 1> op_outputs = {output_tensor_index};
  return ::tflite::CreateOperator(
      builder_, operator_code_index,
      builder_.CreateVector<TensorIndex>(op_inputs),
      builder_.CreateVector<TensorIndex>(op_outputs));
}

auto GraphBuilderTflite::SerializeTriangular(
    const mojom::Triangular& triangular)
    -> base::expected<OperatorOffset, std::string> {
  const OperandDescriptor& input_descriptor =
      GetOperand(triangular.input_operand_id).descriptor;
  CHECK(context_properties_.data_type_limits.triangular_input.Supports(
      input_descriptor));
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(triangular.input_operand_id));

  auto input_rank = input_tensor_info.dimensions.size();
  CHECK_GE(input_rank, 2u);
  const int32_t height = input_tensor_info.dimensions[input_rank - 2];
  const int32_t width = input_tensor_info.dimensions[input_rank - 1];
  auto checked_size = base::MakeCheckedNum<int32_t>(height) * width;
  if (!checked_size.IsValid()) {
    return base::unexpected("Triangular mask is too large.");
  }
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
  TensorIndex mask_tensor_index;
  switch (input_descriptor.data_type()) {
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

  const TensorIndex output_tensor_index =
      SerializeOutputTensorInfo(triangular.output_operand_id).index;
  return SerializeBinaryOperation(::tflite::BuiltinOperator_MUL,
                                  input_tensor_info.index, mask_tensor_index,
                                  output_tensor_index);
}

auto GraphBuilderTflite::SerializeTranspose(const mojom::Transpose& transpose)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.transpose_input.Supports(
      GetOperand(transpose.input_operand_id).descriptor));

  std::optional<TensorInfo> quantized_output =
      CanFuseQuantizeAndGetOutput(transpose);
  const bool fuse_dequantize = quantized_output.has_value();
  ASSIGN_OR_RETURN(const TensorInfo& input_tensor_info,
                   SerializeInputTensorInfo(
                       transpose.input_operand_id, /*quantize_params=*/0,
                       /*operation_supports_float16=*/false, fuse_dequantize));

  TensorIndex output_tensor_index =
      fuse_dequantize
          ? quantized_output->index
          : SerializeOutputTensorInfo(transpose.output_operand_id).index;

  return SerializeTransposeOperation(
      input_tensor_info.index, output_tensor_index,
      input_tensor_info.dimensions, transpose.permutation);
}

auto GraphBuilderTflite::SerializeWhere(const mojom::Where& where)
    -> base::expected<OperatorOffset, std::string> {
  CHECK(context_properties_.data_type_limits.where_condition.Supports(
      GetOperand(where.condition_operand_id).descriptor));
  CHECK(context_properties_.data_type_limits.where_value.Supports(
      GetOperand(where.true_value_operand_id).descriptor));
  CHECK(context_properties_.data_type_limits.where_value.Supports(
      GetOperand(where.false_value_operand_id).descriptor));
  ASSIGN_OR_RETURN(const TensorInfo& condition_tensor_info,
                   SerializeInputTensorInfo(where.condition_operand_id));
  // The data type of WebNN condition operand is uint8, but TFLite requires the
  // condition operand to be of type bool, so a cast operation need to be
  // inserted before the operation to convert uint8 to bool for the condition
  // operand.
  const TensorIndex condition_bool_tensor_index = SerializeTemporaryTensor(
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
  const TensorIndex output_tensor_index =
      SerializeOutputTensorInfo(where.output_operand_id).index;
  return SerializeWhereOperation(
      condition_bool_tensor_index, true_value_tensor_info.index,
      false_value_tensor_info.index, output_tensor_index);
}

}  // namespace webnn::tflite
