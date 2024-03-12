// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_tflite_converter.h"

#include <optional>

#include "base/ranges/algorithm.h"
#include "base/types/expected_macros.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_clamp_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_elu_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gemm_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_leaky_relu_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pad_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pool_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_resample_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_transpose_options.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_activation.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"

namespace blink {

namespace {

// The version number of the Schema. Ideally all changes will be backward
// compatible. If that ever changes, we must ensure that version is the first
// entry in the new tflite root so that we can see that version is not 1.
#define TFLITE_SCHEMA_VERSION (3)

// Maps MLOperand to its index of `tflite::Tensor` array.
using OperandToIndexMap = HeapHashMap<Member<const MLOperand>, int32_t>;
using OperatorCodeOffset = flatbuffers::Offset<tflite::OperatorCode>;
using OperatorOffset = flatbuffers::Offset<tflite::Operator>;
using BufferOffset = flatbuffers::Offset<tflite::Buffer>;
using TensorOffset = flatbuffers::Offset<tflite::Tensor>;

int32_t GetOperatorInputIndex(const MLOperator* op,
                              const OperandToIndexMap& operand_to_index_map,
                              wtf_size_t index = 0) {
  CHECK_NE(op, nullptr);
  CHECK_LE(index, op->Inputs().size());
  const auto* const input = op->Inputs()[index].Get();
  return operand_to_index_map.at(input);
}

int32_t GetOperatorOutputIndex(const MLOperator* op,
                               const OperandToIndexMap& operand_to_index_map,
                               wtf_size_t index = 0) {
  CHECK_NE(op, nullptr);
  CHECK_LE(index, op->Outputs().size());
  const auto* const output = op->Outputs()[index].Get();
  return operand_to_index_map.at(output);
}

base::expected<Vector<int32_t>, String> ConvertDimensions(
    const Vector<uint32_t>& input_dimensions) {
  Vector<int32_t> output_dimensions;
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

tflite::TensorType BlinkOperandTypeToTFLite(
    V8MLOperandDataType::Enum data_type) {
  switch (data_type) {
    case V8MLOperandDataType::Enum::kFloat32:
      return tflite::TensorType_FLOAT32;
    case V8MLOperandDataType::Enum::kFloat16:
      return tflite::TensorType_FLOAT16;
    case V8MLOperandDataType::Enum::kInt32:
      return tflite::TensorType_INT32;
    case V8MLOperandDataType::Enum::kUint32:
      return tflite::TensorType_UINT32;
    case V8MLOperandDataType::Enum::kInt64:
      return tflite::TensorType_INT64;
    case V8MLOperandDataType::Enum::kUint64:
      return tflite::TensorType_UINT64;
    case V8MLOperandDataType::Enum::kInt8:
      return tflite::TensorType_INT8;
    case V8MLOperandDataType::Enum::kUint8:
      return tflite::TensorType_UINT8;
  }
  NOTREACHED_NORETURN();
}

uint32_t GetOperatorCodeIndex(tflite::BuiltinOperator code,
                              flatbuffers::FlatBufferBuilder& builder,
                              Vector<OperatorCodeOffset>& operator_codes) {
  auto operator_code_index =
      base::checked_cast<uint32_t>(operator_codes.size());
  operator_codes.push_back(tflite::CreateOperatorCode(builder, code));
  // The type of operation is determined by the index into the list of the valid
  // OperatorCodes.
  return operator_code_index;
}

struct PaddingSizes {
  uint32_t begin;
  uint32_t end;
};

// Helper to calculate the explicit padding for TFLite SAME padding mode.
std::optional<PaddingSizes> CalculateExplicitPaddingForSamePaddingMode(
    const uint32_t input_size,
    const uint32_t filter_size,
    const uint32_t stride,
    const uint32_t dilation) {
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

// Holds tflite padding mode and the explicit padding if needed.
struct TfLitePadding {
  tflite::Padding mode;
  // The explicit paddings are used to create TfLite Pad operator.
  std::optional<Vector<uint32_t>> paddings;
};

// Helper to get tflite padding mode for convolution 2d or pooling 2d.
template <typename OptionsType>
base::expected<TfLitePadding, String> GetTfLitePaddingMode(
    const OptionsType* options,
    const webnn::Size2d<uint32_t>& input,
    const webnn::Size2d<uint32_t>& filter,
    const webnn::Size2d<uint32_t>& stride,
    const webnn::Size2d<uint32_t>& dilation) {
  CHECK(options);
  // Valid padding means there are no padding to be used as described here
  // https://www.tensorflow.org/api_docs/python/tf/nn#valid_padding.
  const Vector<uint32_t> no_padding = {0, 0, 0, 0};
  const auto explicit_padding = options->getPaddingOr(no_padding);
  CHECK_EQ(explicit_padding.size(), 4u);
  if (explicit_padding == no_padding) {
    return TfLitePadding{.mode = tflite::Padding_VALID};
  } else {
    // Convert the explicit padding to tflite same padding mode, store the
    // padding values for inserting a dedicated pad operator if the calculated
    // padding with kSameUpper are not the same as explicit padding.
    const auto padding_height = CalculateExplicitPaddingForSamePaddingMode(
        input.height, filter.height, stride.height, dilation.height);
    const auto padding_width = CalculateExplicitPaddingForSamePaddingMode(
        input.width, filter.width, stride.width, dilation.width);
    // WebNN explicit padding is in [beginning_height, ending_height,
    // beginning_width, ending_width] sequence.
    const Vector<uint32_t> upper_padding = {
        padding_height->begin, padding_height->end, padding_width->begin,
        padding_width->end};
    if (explicit_padding == upper_padding) {
      return TfLitePadding{.mode = tflite::Padding_SAME};
    } else {
      // Store other padding values for inserting a dedicated pad operator.
      return TfLitePadding{.mode = tflite::Padding_VALID,
                           .paddings = explicit_padding};
    }
  }

  NOTREACHED_NORETURN();
}

enum class ClampRange { kRelu, kRelu1, kRelu6 };

base::expected<ClampRange, String> GetClampRange(const MLOperator* clamp) {
  DCHECK(clamp);
  const auto* options = static_cast<const MLClampOptions*>(clamp->Options());
  DCHECK(options);
  const float min =
      options->getMinValueOr(-std::numeric_limits<float>::infinity());
  const float max =
      options->getMaxValueOr(+std::numeric_limits<float>::infinity());
  // TODO(crbug.com/326156496): Use RELU_0_TO_1 to support min = 0.0f and max
  // = 1.0f.
  if (min == -1.0f && max == 1.0f) {
    return ClampRange::kRelu1;
  } else if (min == 0.0f && max == 6.0f) {
    return ClampRange::kRelu6;
  } else if (min == 0.0f && !options->hasMaxValue()) {
    return ClampRange::kRelu;
  }

  // TODO(crbug.com/326156496): Support other range.
  return base::unexpected(
      "The range of clamp is not supported in tflite schema.");
}

base::expected<tflite::ActivationFunctionType, String>
GetActivationTypeForClamp(const MLOperator* clamp) {
  const auto range_result = GetClampRange(clamp);
  RETURN_IF_ERROR(range_result);
  switch (range_result.value()) {
    case ClampRange::kRelu:
      return tflite::ActivationFunctionType_RELU;
    case ClampRange::kRelu1:
      return tflite::ActivationFunctionType_RELU_N1_TO_1;
    case ClampRange::kRelu6:
      return tflite::ActivationFunctionType_RELU6;
  }
  NOTREACHED_NORETURN();
}

base::expected<tflite::ActivationFunctionType, String>
GetActivationFunctionType(const MLActivation* ml_activation) {
  CHECK(ml_activation);
  switch (ml_activation->Kind()) {
    case webnn::mojom::blink::Activation::Tag::kClamp: {
      const auto activation_result =
          GetActivationTypeForClamp(ml_activation->Operator());
      RETURN_IF_ERROR(activation_result);
      return activation_result.value();
    }
    case webnn::mojom::blink::Activation::Tag::kRelu:
      return tflite::ActivationFunctionType_RELU;
    case webnn::mojom::blink::Activation::Tag::kElu:
    case webnn::mojom::blink::Activation::Tag::kHardSigmoid:
    case webnn::mojom::blink::Activation::Tag::kLeakyRelu:
    case webnn::mojom::blink::Activation::Tag::kLinear:
    case webnn::mojom::blink::Activation::Tag::kSigmoid:
    case webnn::mojom::blink::Activation::Tag::kSoftmax:
    case webnn::mojom::blink::Activation::Tag::kSoftplus:
    case webnn::mojom::blink::Activation::Tag::kSoftsign:
    case webnn::mojom::blink::Activation::Tag::kTanh:
      return base::unexpected(
          MLActivation::ActivationKindToString(ml_activation->Kind()) +
          " activation is not supported.");
  }
}

template <typename T>
struct TensorInfo {
  tflite::TensorType type;
  Vector<int32_t> dimensions;
  Vector<T> values;
};

template <typename DataType>
int32_t SerializeTensorWithBuffer(const TensorInfo<DataType>& tensor_info,
                                  flatbuffers::FlatBufferBuilder& builder,
                                  Vector<BufferOffset>& buffers,
                                  Vector<TensorOffset>& tensors) {
  // Create `tflite::Buffer` for the empty tensors.
  const auto buffer_index = base::checked_cast<uint32_t>(buffers.size());
  const auto& tensor_data = tensor_info.values;
  buffers.emplace_back(tflite::CreateBuffer(
      builder,
      builder.CreateVector(reinterpret_cast<const uint8_t*>(tensor_data.data()),
                           tensor_data.size() * sizeof(DataType))));

  // Create `tflite::Tensor` with the dimensions and the index of buffer.
  const int32_t tensor_index = base::checked_cast<int32_t>(tensors.size());
  const auto dimensions = builder.CreateVector<int32_t>(tensor_info.dimensions);
  tensors.emplace_back(tflite::CreateTensor(builder, std::move(dimensions),
                                            tensor_info.type, buffer_index));

  return tensor_index;
}

base::expected<int32_t, String> SerializeExplicitPad(
    const MLOperand* input_operand,
    const int32_t input_tensor_index,
    const Vector<uint32_t>& paddings,
    flatbuffers::FlatBufferBuilder& builder,
    Vector<OperatorCodeOffset>& operator_codes,
    Vector<OperatorOffset>& operators,
    Vector<BufferOffset>& buffers,
    Vector<TensorOffset>& tensors) {
  // WebNN explicit padding is in [beginning_height, ending_height,
  // beginning_width, ending_width] sequence.
  const auto padding_rank = paddings.size();
  CHECK_EQ(padding_rank, 4u);

  // TfLite padding is an integer tensor array filled with pre and post padding.
  // For NHWC input layout, the sequence will be [[0, 0], [beginning_height,
  // ending_height], [beginning_width, ending_width], [0, 0]].
  Vector<int32_t> tflite_paddings(padding_rank);
  tflite_paddings.InsertAt(tflite_paddings.begin() + 2, paddings.data(),
                           paddings.size());

  // The shape of padding is [n, 2], where n is the rank of input as described
  // here https://www.tensorflow.org/mlir/tfl_ops#tflmirror_pad_tflmirrorpadop.
  const Vector<int32_t> paddings_shape{
      {base::checked_cast<int32_t>(padding_rank), 2}};
  const TensorInfo<int32_t> paddings_info = {.type = tflite::TensorType_INT32,
                                             .dimensions = paddings_shape,
                                             .values = tflite_paddings};
  const auto padding_tensor_index = SerializeTensorWithBuffer<int32_t>(
      paddings_info, builder, buffers, tensors);

  // Create `tflite::Tensor` for the output operand of explicit padding operator
  // with the dimensions and data type.
  const Vector<uint32_t>& input_shape = input_operand->Dimensions();
  CHECK_EQ(input_shape.size(), 4u);
  Vector<int32_t> output_shape;
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

  const tflite::TensorType input_tensor_type =
      BlinkOperandTypeToTFLite(input_operand->DataType());
  const int32_t output_tensor_index =
      base::checked_cast<int32_t>(tensors.size());
  tensors.emplace_back(tflite::CreateTensor(
      builder, builder.CreateVector<int32_t>(output_shape), input_tensor_type));

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  std::array<int32_t, 2> op_inputs = {input_tensor_index, padding_tensor_index};
  const auto operator_code_index = GetOperatorCodeIndex(
      tflite::BuiltinOperator_PAD, builder, operator_codes);
  const std::array<int32_t, 1> op_outputs = {output_tensor_index};
  operators.emplace_back(tflite::CreateOperator(
      builder, operator_code_index, builder.CreateVector<int32_t>(op_inputs),
      builder.CreateVector<int32_t>(op_outputs)));

  return output_tensor_index;
}

base::expected<OperatorOffset, String> SerializeConv2d(
    const OperandToIndexMap& operand_to_index_map,
    const MLOperator* conv2d,
    flatbuffers::FlatBufferBuilder& builder,
    Vector<OperatorCodeOffset>& operator_codes,
    Vector<OperatorOffset>& operators,
    Vector<BufferOffset>& buffers,
    Vector<TensorOffset>& tensors) {
  const int32_t input_index =
      GetOperatorInputIndex(conv2d, operand_to_index_map, 0);
  const int32_t filter_index =
      GetOperatorInputIndex(conv2d, operand_to_index_map, 1);
  const int32_t output_index =
      GetOperatorOutputIndex(conv2d, operand_to_index_map);

  const MLConv2dOptions* options =
      static_cast<const MLConv2dOptions*>(conv2d->Options());
  CHECK(options);
  // TODO(crbug.com/1273291): Transpose input operand to support other layouts
  // because tflite only support nhwc layout.
  if (options->inputLayout().AsEnum() != V8MLInputOperandLayout::Enum::kNhwc) {
    return base::unexpected(
        String::Format("The input layout %s is not supported.",
                       options->inputLayout().AsCStr()));
  }

  // Depthwise conv2d is "options.groups == input_channels == output_channels".
  const auto* const input = conv2d->Inputs()[0].Get();
  CHECK(input);
  const auto& input_shape = input->Dimensions();
  CHECK_EQ(input_shape.size(), 4u);
  const uint32_t input_channels = input_shape[3];
  const auto* const output = conv2d->Outputs()[0].Get();
  CHECK(output);
  const auto& output_shape = output->Dimensions();
  CHECK_EQ(output_shape.size(), 4u);
  const uint32_t output_channels = output_shape[3];
  const uint32_t groups = base::checked_cast<uint32_t>(options->groups());
  const bool depthwise =
      webnn::IsDepthwiseConv2d(input_channels, output_channels, groups);

  // Validate filter layout for nhwc input layout that is being discussed to
  // simplify other variants in WebNN working group
  // https://github.com/webmachinelearning/webnn/issues/324.
  const auto validation_result = ValidateFilterLayout(
      depthwise, options->inputLayout(), options->filterLayout());
  RETURN_IF_ERROR(validation_result);

  // Validate activation operator that is partial support in tflite schema and
  // convert to tflite function type.
  tflite::ActivationFunctionType activation =
      tflite::ActivationFunctionType_NONE;
  if (options->hasActivation()) {
    const auto activation_result =
        GetActivationFunctionType(options->activation());
    RETURN_IF_ERROR(activation_result);
    activation = activation_result.value();
  }

  // Get tflite padding mode with the size2d of input, filter, dilation.
  const webnn::Size2d<uint32_t> input_size2d = {.height = input_shape[1],
                                                .width = input_shape[2]};
  const auto* const filter = conv2d->Inputs()[1].Get();
  CHECK(filter);
  const auto& filter_shape = filter->Dimensions();
  CHECK_EQ(filter_shape.size(), 4u);
  const webnn::Size2d<uint32_t> filter_size2d = {.height = filter_shape[1],
                                                 .width = filter_shape[2]};

  // If strides is not present, the values are assumed to be [1,1].
  const auto strides = options->getStridesOr({1, 1});
  CHECK_EQ(strides.size(), 2u);
  const webnn::Size2d<uint32_t> stride_size2d = {.height = strides[0],
                                                 .width = strides[1]};

  // If dilations is not present, the values are assumed to be [1,1].
  const auto dilations = options->getDilationsOr({1, 1});
  CHECK_EQ(dilations.size(), 2u);
  const webnn::Size2d<uint32_t> dilation_size2d = {.height = dilations[0],
                                                   .width = dilations[1]};
  const auto padding_mode = GetTfLitePaddingMode(
      options, input_size2d, filter_size2d, stride_size2d, dilation_size2d);
  RETURN_IF_ERROR(padding_mode);

  // Insert a Pad operator before TfLite Conv2d if needed for explicit padding.
  std::optional<int32_t> explicit_pad_index;
  const auto& explicit_padding = padding_mode->paddings;
  if (explicit_padding) {
    const auto serialization_result = SerializeExplicitPad(
        input, input_index, explicit_padding.value(), builder, operator_codes,
        operators, buffers, tensors);
    RETURN_IF_ERROR(serialization_result);
    explicit_pad_index = serialization_result.value();
  }

  tflite::BuiltinOperator operator_kind;
  tflite::BuiltinOptions builtin_options_type = tflite::BuiltinOptions_NONE;
  flatbuffers::Offset<void> builtin_options = 0;
  if (depthwise) {
    const uint32_t depth_multiplier = 1;
    operator_kind = tflite::BuiltinOperator_DEPTHWISE_CONV_2D;
    builtin_options = tflite::CreateDepthwiseConv2DOptions(
                          builder, padding_mode->mode, stride_size2d.width,
                          stride_size2d.height, depth_multiplier, activation,
                          dilation_size2d.width, dilation_size2d.height)
                          .Union();
    builtin_options_type = tflite::BuiltinOptions_DepthwiseConv2DOptions;
  } else {
    operator_kind = tflite::BuiltinOperator_CONV_2D;
    builtin_options = tflite::CreateConv2DOptions(
                          builder, padding_mode->mode, stride_size2d.width,
                          stride_size2d.height, activation,
                          dilation_size2d.width, dilation_size2d.height)
                          .Union();
    builtin_options_type = tflite::BuiltinOptions_Conv2DOptions;
  }

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const auto operator_code_index =
      GetOperatorCodeIndex(operator_kind, builder, operator_codes);
  // If there is no bias operand, serialize a empty buffer with the size of
  // output channel.
  int32_t bias_index = 0;
  if (options->hasBias()) {
    bias_index = GetOperatorInputIndex(conv2d, operand_to_index_map, 2);
  } else {
    // TODO(crbug.com/1273291): Support other tensor data type.
    if (input->DataType() != V8MLOperandDataType::Enum::kFloat32) {
      return base::unexpected("The data type of input is not supported.");
    }
    const TensorInfo<float> zero_buffer_info = {
        .type = BlinkOperandTypeToTFLite(input->DataType()),
        .dimensions = {base::checked_cast<int32_t>(output_channels)},
        .values = Vector<float>(output_channels)};
    bias_index = SerializeTensorWithBuffer<float>(zero_buffer_info, builder,
                                                  buffers, tensors);
  }

  const std::vector<int32_t> op_inputs = {
      explicit_pad_index ? explicit_pad_index.value() : input_index,
      filter_index, bias_index};
  const std::vector<int32_t> op_outputs = {output_index};
  return tflite::CreateOperator(builder, operator_code_index,
                                builder.CreateVector<int32_t>(op_inputs),
                                builder.CreateVector<int32_t>(op_outputs),
                                builtin_options_type, builtin_options);
}

OperatorOffset SerializeConcat(const OperandToIndexMap& operand_to_index_map,
                               const MLOperator* ml_operator,
                               flatbuffers::FlatBufferBuilder& builder,
                               Vector<OperatorCodeOffset>& operator_codes) {
  const MLConcatOperator* concat =
      static_cast<const MLConcatOperator*>(ml_operator);
  const auto inputs_size = concat->Inputs().size();
  Vector<int32_t> operator_inputs(inputs_size);
  for (wtf_size_t i = 0; i < inputs_size; ++i) {
    operator_inputs[i] = GetOperatorInputIndex(concat, operand_to_index_map, i);
  }
  const int32_t output_index =
      GetOperatorOutputIndex(concat, operand_to_index_map);

  // Create `tflite::ConcatenationOptions` with axis.
  const auto concat_options =
      tflite::CreateConcatenationOptions(builder, concat->Axis());

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const auto operator_code_index = GetOperatorCodeIndex(
      tflite::BuiltinOperator_CONCATENATION, builder, operator_codes);
  const std::array<int32_t, 1> operator_outputs = {output_index};
  return tflite::CreateOperator(builder, operator_code_index,
                                builder.CreateVector<int32_t>(operator_inputs),
                                builder.CreateVector<int32_t>(operator_outputs),
                                tflite::BuiltinOptions_ConcatenationOptions,
                                concat_options.Union());
}

OperatorOffset SerializeElementWiseBinary(
    const OperandToIndexMap& operand_to_index_map,
    const MLOperator* binary,
    flatbuffers::FlatBufferBuilder& builder,
    Vector<OperatorCodeOffset>& operator_codes) {
  const int32_t lhs_index =
      GetOperatorInputIndex(binary, operand_to_index_map, 0);
  const int32_t rhs_index =
      GetOperatorInputIndex(binary, operand_to_index_map, 1);
  const int32_t output_index =
      GetOperatorOutputIndex(binary, operand_to_index_map);
  tflite::BuiltinOperator operator_kind;
  switch (binary->SubKind<webnn::mojom::blink::ElementWiseBinary::Kind>()) {
    case webnn::mojom::blink::ElementWiseBinary::Kind::kAdd:
      operator_kind = tflite::BuiltinOperator_ADD;
      break;
    case webnn::mojom::blink::ElementWiseBinary::Kind::kSub:
      operator_kind = tflite::BuiltinOperator_SUB;
      break;
    case webnn::mojom::blink::ElementWiseBinary::Kind::kMul:
      operator_kind = tflite::BuiltinOperator_MUL;
      break;
    case webnn::mojom::blink::ElementWiseBinary::Kind::kDiv:
      operator_kind = tflite::BuiltinOperator_DIV;
      break;
    case webnn::mojom::blink::ElementWiseBinary::Kind::kMin:
      operator_kind = tflite::BuiltinOperator_MINIMUM;
      break;
    case webnn::mojom::blink::ElementWiseBinary::Kind::kMax:
      operator_kind = tflite::BuiltinOperator_MAXIMUM;
      break;
    case webnn::mojom::blink::ElementWiseBinary::Kind::kPow:
      operator_kind = tflite::BuiltinOperator_POW;
      break;
    default:
      NOTREACHED_NORETURN() << "The operator is not element-wise binary.";
  }

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const auto operator_code_index =
      GetOperatorCodeIndex(operator_kind, builder, operator_codes);
  const std::vector<int32_t> operator_inputs = {lhs_index, rhs_index};
  const std::vector<int32_t> operator_outputs = {output_index};
  return tflite::CreateOperator(
      builder, operator_code_index,
      builder.CreateVector<int32_t>(operator_inputs),
      builder.CreateVector<int32_t>(operator_outputs));
}

OperatorOffset SerializeTranspose(const int32_t input_index,
                                  const int32_t output_index,
                                  const Vector<int32_t>& permutation,
                                  flatbuffers::FlatBufferBuilder& builder,
                                  Vector<OperatorCodeOffset>& operator_codes,
                                  Vector<BufferOffset>& buffers,
                                  Vector<TensorOffset>& tensors) {
  const Vector<int32_t> permutation_shape{
      base::checked_cast<int32_t>(permutation.size())};
  const TensorInfo<int32_t> permutation_info = {
      .type = tflite::TensorType_INT32,
      .dimensions = permutation_shape,
      .values = permutation};
  const auto permutation_tensor_index = SerializeTensorWithBuffer<int32_t>(
      permutation_info, builder, buffers, tensors);

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const auto operator_code_index = GetOperatorCodeIndex(
      tflite::BuiltinOperator_TRANSPOSE, builder, operator_codes);
  const std::array<int32_t, 2> operator_inputs = {input_index,
                                                  permutation_tensor_index};
  const std::array<int32_t, 1> operator_outputs = {output_index};
  return tflite::CreateOperator(
      builder, operator_code_index,
      builder.CreateVector<int32_t>(operator_inputs),
      builder.CreateVector<int32_t>(operator_outputs));
}

int32_t InsertTransposeOperator(const MLOperand* input_operand,
                                const int32_t input_tensor_index,
                                const Vector<int32_t>& permutation,
                                flatbuffers::FlatBufferBuilder& builder,
                                Vector<OperatorCodeOffset>& operator_codes,
                                Vector<OperatorOffset>& operators,
                                Vector<BufferOffset>& buffers,
                                Vector<TensorOffset>& tensors) {
  // Create `tflite::Tensor` for the output operand of Transpose operator with
  // the dimensions and tensor data type.
  const Vector<uint32_t>& input_shape = input_operand->Dimensions();
  const auto input_rank = input_shape.size();
  CHECK_EQ(permutation.size(), input_rank);
  std::vector<int32_t> output_shape(input_rank);
  for (wtf_size_t i = 0; i < input_rank; ++i) {
    // The input shape has been validated the overflow before creating tensor.
    output_shape[i] = base::checked_cast<int32_t>(input_shape[permutation[i]]);
  }
  const tflite::TensorType input_tensor_type =
      BlinkOperandTypeToTFLite(input_operand->DataType());
  const int32_t output_tensor_index =
      base::checked_cast<int32_t>(tensors.size());
  tensors.emplace_back(tflite::CreateTensor(
      builder, builder.CreateVector<int32_t>(output_shape), input_tensor_type));

  const auto transpose_offset =
      SerializeTranspose(input_tensor_index, output_tensor_index, permutation,
                         builder, operator_codes, buffers, tensors);
  operators.emplace_back(transpose_offset);

  return output_tensor_index;
}

base::expected<OperatorOffset, String> SerializeGemm(
    const OperandToIndexMap& operand_to_index_map,
    const MLOperator* gemm,
    flatbuffers::FlatBufferBuilder& builder,
    Vector<OperatorCodeOffset>& operator_codes,
    Vector<OperatorOffset>& operators,
    Vector<BufferOffset>& buffers,
    Vector<TensorOffset>& tensors) {
  // Get the tensor index of input, filter, bias and output operand.
  const int32_t input_index =
      GetOperatorInputIndex(gemm, operand_to_index_map, 0);
  const int32_t filter_index =
      GetOperatorInputIndex(gemm, operand_to_index_map, 1);
  const int32_t output_index =
      GetOperatorOutputIndex(gemm, operand_to_index_map);

  // TODO(crbug.com/1273291): Support alpha, beta and aTranspose options.
  const MLGemmOptions* options =
      static_cast<const MLGemmOptions*>(gemm->Options());
  const auto output_channels = gemm->Outputs()[0]->Dimensions()[1];
  const auto validation_result = ValidateGemmOptions(options, output_channels);
  RETURN_IF_ERROR(validation_result);

  // The WebNN Gemm follows the expression `alpha * A * B + beta * C`, where A
  // is a 2-D tensor with shape [M, K], B is a 2-D tensor with shape [K, N] by
  // default options, but Tflite Fully Connected's input and filter shapes
  // are [batch, input_channels] and [output_channels, input_channels], so the
  // Transpose operator need to be inserted before Gemm When bTranspose option
  // is false.
  std::optional<int32_t> transpose_index;
  if (!options->bTranspose()) {
    const auto* const filter = gemm->Inputs()[1].Get();
    CHECK_EQ(filter->Dimensions().size(), 2u);
    const Vector<int32_t> permutation = {1, 0};
    transpose_index =
        InsertTransposeOperator(filter, filter_index, permutation, builder,
                                operator_codes, operators, buffers, tensors);
  }
  std::vector<int32_t> operator_inputs = {
      input_index, transpose_index ? transpose_index.value() : filter_index};
  if (gemm->Inputs().size() == 3) {
    operator_inputs.push_back(
        GetOperatorInputIndex(gemm, operand_to_index_map, 2));
  }

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const auto operator_code_index = GetOperatorCodeIndex(
      tflite::BuiltinOperator_FULLY_CONNECTED, builder, operator_codes);
  const std::array<int32_t, 1> operator_outputs = {output_index};
  return tflite::CreateOperator(
      builder, operator_code_index,
      builder.CreateVector<int32_t>(operator_inputs),
      builder.CreateVector<int32_t>(operator_outputs));
}

base::expected<OperatorOffset, String> SerializePad(
    const OperandToIndexMap& operand_to_index_map,
    const MLOperator* pad,
    flatbuffers::FlatBufferBuilder& builder,
    Vector<OperatorCodeOffset>& operator_codes,
    Vector<BufferOffset>& buffers,
    Vector<TensorOffset>& tensors) {
  const MLPadOperator* pad_operator = static_cast<const MLPadOperator*>(pad);
  const int32_t input_index =
      GetOperatorInputIndex(pad_operator, operand_to_index_map);
  const int32_t output_index =
      GetOperatorOutputIndex(pad_operator, operand_to_index_map);

  // Paddings is an integer tensor array filled with pre and post padding.
  const Vector<uint32_t>& pre_paddings = pad_operator->BeginningPadding();
  const Vector<uint32_t>& post_paddings = pad_operator->EndingPadding();
  CHECK_EQ(pre_paddings.size(), post_paddings.size());
  Vector<int32_t> paddings(pre_paddings.size() + post_paddings.size());
  for (size_t i = 0; i < pre_paddings.size(); ++i) {
    auto checked_pre_padding = base::MakeCheckedNum<int32_t>(pre_paddings[i]);
    auto checked_post_padding = base::MakeCheckedNum<int32_t>(post_paddings[i]);
    if (!checked_pre_padding.IsValid() || !checked_post_padding.IsValid()) {
      return base::unexpected("The padding is too large.");
    }
    paddings[i * 2] = checked_pre_padding.ValueOrDie();
    paddings[i * 2 + 1] = checked_post_padding.ValueOrDie();
  }

  // The shape of padding is [n, 2], where n is the rank of input as described
  // here https://www.tensorflow.org/mlir/tfl_ops#tflmirror_pad_tflmirrorpadop.
  const Vector<int32_t> paddings_shape{
      {base::checked_cast<int32_t>(pre_paddings.size()), 2}};
  const TensorInfo<int32_t> paddings_info = {.type = tflite::TensorType_INT32,
                                             .dimensions = paddings_shape,
                                             .values = paddings};
  const auto paddings_index = SerializeTensorWithBuffer<int32_t>(
      paddings_info, builder, buffers, tensors);

  // Create the inputs of operator with the index of input and paddings, the
  // index of padding value will be pushed back into the vector if the padding
  // mode is Constant.
  std::vector<int32_t> op_inputs = {input_index, paddings_index};
  const MLPadOptions* options =
      static_cast<const MLPadOptions*>(pad_operator->Options());
  CHECK(options);
  tflite::BuiltinOptions builtin_options_type = tflite::BuiltinOptions_NONE;
  flatbuffers::Offset<void> builtin_options = 0;
  tflite::BuiltinOperator operator_code;
  switch (options->mode().AsEnum()) {
    case blink::V8MLPaddingMode::Enum::kReflection: {
      operator_code = tflite::BuiltinOperator_MIRROR_PAD;
      builtin_options_type = tflite::BuiltinOptions_MirrorPadOptions;
      builtin_options =
          CreateMirrorPadOptions(builder, tflite::MirrorPadMode_REFLECT)
              .Union();
      break;
    }
    case blink::V8MLPaddingMode::Enum::kSymmetric: {
      operator_code = tflite::BuiltinOperator_MIRROR_PAD;
      builtin_options_type = tflite::BuiltinOptions_MirrorPadOptions;
      builtin_options =
          CreateMirrorPadOptions(builder, tflite::MirrorPadMode_SYMMETRIC)
              .Union();
      break;
    }
    case blink::V8MLPaddingMode::Enum::kConstant: {
      operator_code = tflite::BuiltinOperator_PADV2;
      builtin_options = tflite::CreatePadV2Options(builder).Union();
      float padding_value = options->value();
      const TensorInfo<float> padding_value_info = {
          .type = tflite::TensorType_FLOAT32,
          .dimensions = {1},
          .values = {padding_value}};
      const auto padding_value_index = SerializeTensorWithBuffer<float>(
          padding_value_info, builder, buffers, tensors);
      op_inputs.push_back(padding_value_index);
      break;
    }
    case blink::V8MLPaddingMode::Enum::kEdge:
      return base::unexpected(
          "The edge padding mode is not supported in tflite schema.");
  }

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const auto operator_code_index =
      GetOperatorCodeIndex(operator_code, builder, operator_codes);
  const std::array<int32_t, 1> op_outputs = {output_index};
  return tflite::CreateOperator(builder, operator_code_index,
                                builder.CreateVector<int32_t>(op_inputs),
                                builder.CreateVector<int32_t>(op_outputs),
                                builtin_options_type, builtin_options);
}

base::expected<OperatorOffset, String> SerializePool2d(
    const OperandToIndexMap& operand_to_index_map,
    const MLOperator* pool2d,
    flatbuffers::FlatBufferBuilder& builder,
    Vector<OperatorCodeOffset>& operator_codes,
    Vector<OperatorOffset>& operators,
    Vector<BufferOffset>& buffers,
    Vector<TensorOffset>& tensors) {
  const int32_t input_index =
      GetOperatorInputIndex(pool2d, operand_to_index_map);
  const int32_t output_index =
      GetOperatorOutputIndex(pool2d, operand_to_index_map);

  // TODO(crbug.com/1273291): Transpose input operand to support other layouts
  // because tflite only support nhwc layout.
  const MLPool2dOptions* options =
      static_cast<const MLPool2dOptions*>(pool2d->Options());
  CHECK(options);
  if (options->layout().AsEnum() != V8MLInputOperandLayout::Enum::kNhwc) {
    return base::unexpected(String::Format(
        "The input layout %s is not supported.", options->layout().AsCStr()));
  }

  // If dilations is not present, the values are assumed to be [1,1].
  const Vector<uint32_t> default_strides = {1, 1};
  const auto dilations = options->getDilationsOr(default_strides);
  CHECK_EQ(dilations.size(), 2u);
  if (dilations != default_strides) {
    return base::unexpected("Pool2d in tflite doesn't support dilations.");
  }
  const webnn::Size2d<uint32_t> dilation_size2d = {.height = dilations[0],
                                                   .width = dilations[1]};

  // If strides is not present, the values are assumed to be [1,1].
  const auto strides = options->getStridesOr({1, 1});
  CHECK_EQ(strides.size(), 2u);
  const webnn::Size2d<uint32_t> stride_size2d = {.height = strides[0],
                                                 .width = strides[1]};

  const auto* const input = pool2d->Inputs()[0].Get();
  CHECK(input);
  const auto& input_shape = input->Dimensions();
  CHECK_EQ(input_shape.size(), 4u);
  const auto input_height = input_shape[1];
  const auto input_width = input_shape[2];
  const webnn::Size2d<uint32_t> input_size2d = {.height = input_height,
                                                .width = input_width};

  // According to WebNN pool2d spec:
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-pool2d, if the window
  // dimensions are not present, the window dimensions are assumed to be
  // the height and width dimensions of the input shape that could be
  // mapped to the global pooling operation.
  webnn::Size2d<uint32_t> filter_size2d;
  if (options->hasWindowDimensions()) {
    const auto& window_dimensions = options->windowDimensions();
    CHECK_EQ(window_dimensions.size(), 2u);
    filter_size2d.height = window_dimensions[0];
    filter_size2d.width = window_dimensions[1];
  } else {
    filter_size2d.height = input_height;
    filter_size2d.width = input_width;
  }

  const auto padding_mode = GetTfLitePaddingMode(
      options, input_size2d, filter_size2d, stride_size2d, dilation_size2d);
  RETURN_IF_ERROR(padding_mode);
  // Insert a Pad operator before TfLite Pool2d if needed for explicit padding.
  std::optional<int32_t> explicit_pad_index;
  const auto& explicit_padding = padding_mode->paddings;
  if (explicit_padding) {
    const auto serialization_result = SerializeExplicitPad(
        input, input_index, explicit_padding.value(), builder, operator_codes,
        operators, buffers, tensors);
    RETURN_IF_ERROR(serialization_result);
    explicit_pad_index = serialization_result.value();
  }

  tflite::BuiltinOperator operator_kind;
  switch (pool2d->SubKind<webnn::mojom::blink::Pool2d::Kind>()) {
    case webnn::mojom::blink::Pool2d::Kind::kAveragePool2d:
      operator_kind = tflite::BuiltinOperator_AVERAGE_POOL_2D;
      break;
    case webnn::mojom::blink::Pool2d::Kind::kMaxPool2d:
      operator_kind = tflite::BuiltinOperator_MAX_POOL_2D;
      break;
    case webnn::mojom::blink::Pool2d::Kind::kL2Pool2d:
      return base::unexpected("L2Pool2d is not supported by tflite.");
  }

  const auto pool_2d_options = CreatePool2DOptions(
      builder, padding_mode->mode, stride_size2d.width, stride_size2d.height,
      filter_size2d.width, filter_size2d.height,
      tflite::ActivationFunctionType_NONE);

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const auto operator_code_index =
      GetOperatorCodeIndex(operator_kind, builder, operator_codes);
  const std::vector<int32_t> op_inputs = {
      explicit_pad_index ? explicit_pad_index.value() : input_index};
  const std::vector<int32_t> op_outputs = {output_index};
  return tflite::CreateOperator(
      builder, operator_code_index, builder.CreateVector<int32_t>(op_inputs),
      builder.CreateVector<int32_t>(op_outputs),
      tflite::BuiltinOptions_Pool2DOptions, pool_2d_options.Union());
}

OperatorOffset SerializeUnaryOperator(
    tflite::BuiltinOperator code,
    const OperandToIndexMap& operand_to_index_map,
    const MLOperator* unary,
    flatbuffers::FlatBufferBuilder& builder,
    Vector<OperatorCodeOffset>& operator_codes) {
  const int32_t input_index =
      GetOperatorInputIndex(unary, operand_to_index_map);
  const int32_t output_index =
      GetOperatorOutputIndex(unary, operand_to_index_map);

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const auto operator_code_index =
      GetOperatorCodeIndex(code, builder, operator_codes);
  const std::array<int32_t, 1> op_inputs = {input_index};
  const std::array<int32_t, 1> op_outputs = {output_index};
  return tflite::CreateOperator(builder, operator_code_index,
                                builder.CreateVector<int32_t>(op_inputs),
                                builder.CreateVector<int32_t>(op_outputs));
}

base::expected<OperatorOffset, String> SerializeClamp(
    const OperandToIndexMap& operand_to_index_map,
    const MLOperator* clamp,
    flatbuffers::FlatBufferBuilder& builder,
    Vector<OperatorCodeOffset>& operator_codes) {
  const auto range_result = GetClampRange(clamp);
  RETURN_IF_ERROR(range_result);

  tflite::BuiltinOperator code;
  switch (range_result.value()) {
    case ClampRange::kRelu:
      code = tflite::BuiltinOperator_RELU;
      break;
    case ClampRange::kRelu1:
      code = tflite::BuiltinOperator_RELU_N1_TO_1;
      break;
    case ClampRange::kRelu6:
      code = tflite::BuiltinOperator_RELU6;
      break;
  }

  return SerializeUnaryOperator(code, operand_to_index_map, clamp, builder,
                                operator_codes);
}

base::expected<OperatorOffset, String> SerializeElu(
    const OperandToIndexMap& operand_to_index_map,
    const MLOperator* elu,
    flatbuffers::FlatBufferBuilder& builder,
    Vector<OperatorCodeOffset>& operator_codes) {
  const MLEluOptions* options =
      static_cast<const MLEluOptions*>(elu->Options());
  CHECK(options);
  const float alpha = options->alpha();
  if (alpha != 1.0) {
    return base::unexpected(
        "Setting a custom alpha is not supported in tflite schema.");
  }

  return SerializeUnaryOperator(tflite::BuiltinOperator_ELU,
                                operand_to_index_map, elu, builder,
                                operator_codes);
}

OperatorOffset SerializeLeakyRelu(const OperandToIndexMap& operand_to_index_map,
                                  const MLOperator* leaky_relu,
                                  flatbuffers::FlatBufferBuilder& builder,
                                  Vector<OperatorCodeOffset>& operator_codes) {
  const int32_t input_index =
      GetOperatorInputIndex(leaky_relu, operand_to_index_map);
  const int32_t output_index =
      GetOperatorOutputIndex(leaky_relu, operand_to_index_map);

  // Create `tflite::LeakyReluOptions` with negative slope.
  const MLLeakyReluOptions* options =
      static_cast<const MLLeakyReluOptions*>(leaky_relu->Options());
  CHECK(options);
  const auto leaky_relu_options =
      tflite::CreateLeakyReluOptions(builder, options->alpha());

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const auto operator_code_index = GetOperatorCodeIndex(
      tflite::BuiltinOperator_LEAKY_RELU, builder, operator_codes);
  const std::array<int32_t, 1> operator_inputs = {input_index};
  const std::array<int32_t, 1> operator_outputs = {output_index};
  return tflite::CreateOperator(builder, operator_code_index,
                                builder.CreateVector<int32_t>(operator_inputs),
                                builder.CreateVector<int32_t>(operator_outputs),
                                tflite::BuiltinOptions_LeakyReluOptions,
                                leaky_relu_options.Union());
}

base::expected<OperatorOffset, String> SerializeResample2d(
    const OperandToIndexMap& operand_to_index_map,
    const MLOperator* resample2d,
    flatbuffers::FlatBufferBuilder& builder,
    Vector<OperatorCodeOffset>& operator_codes,
    Vector<BufferOffset>& buffers,
    Vector<TensorOffset>& tensors) {
  const int32_t input_index =
      GetOperatorInputIndex(resample2d, operand_to_index_map);
  const int32_t output_index =
      GetOperatorOutputIndex(resample2d, operand_to_index_map);

  const MLResample2dOptions* options =
      static_cast<const MLResample2dOptions*>(resample2d->Options());
  CHECK(options);
  const Vector<uint32_t> default_axes({2, 3});
  // TfLite resize bilinear node only supports axes = {1, 2}.
  // TODO(crbug.com/1273291): Support axes = {2, 3} by transposing the input
  // tensor.
  if (!(options->getAxesOr(default_axes)[0] == 1 &&
        options->getAxesOr(default_axes)[1] == 2)) {
    return base::unexpected(
        "Resample2d only supports axes = {1, 2} in tflite schema.");
  }

  // Create tflite builtin options for resize mode that is align_corner = false
  // and half_pixel_center = true by default. WebNN will support coordinate
  // transformation modes for Resample2d and it's tracked by the issue:
  // https://github.com/webmachinelearning/webnn/issues/270.
  tflite::BuiltinOperator operator_code;
  tflite::BuiltinOptions builtin_options_type;
  flatbuffers::Offset<void> builtin_options;
  switch (options->mode().AsEnum()) {
    case V8MLInterpolationMode::Enum::kLinear:
      operator_code = tflite::BuiltinOperator_RESIZE_BILINEAR;
      builtin_options_type = tflite::BuiltinOptions_ResizeBilinearOptions;
      builtin_options =
          tflite::CreateResizeBilinearOptions(builder, /*align_corners=*/false,
                                              /*half_pixel_centers=*/true)
              .Union();
      break;
    case V8MLInterpolationMode::Enum::kNearestNeighbor:
      operator_code = tflite::BuiltinOperator_RESIZE_NEAREST_NEIGHBOR;
      builtin_options_type =
          tflite::BuiltinOptions_ResizeNearestNeighborOptions;
      builtin_options = tflite::CreateResizeNearestNeighborOptions(
                            builder, /*align_corners=*/false,
                            /*half_pixel_centers=*/true)
                            .Union();
  }

  // Serialize the target sizes for the dimensions [OutputHeight, OutputWidth].
  const base::expected<Vector<int32_t>, String> output_dimensions =
      ConvertDimensions(resample2d->Outputs()[0]->Dimensions());
  RETURN_IF_ERROR(output_dimensions);
  DCHECK_EQ(output_dimensions->size(), 4U);
  const auto output_height = output_dimensions.value()[1];
  const auto output_width = output_dimensions.value()[2];
  const Vector<int32_t> resize_data = {output_height, output_width};
  const Vector<int32_t> resize_shape = {
      static_cast<int32_t>(resize_data.size())};
  const TensorInfo<int32_t> resize_info = {
      .type = tflite::TensorType_INT32,
      .dimensions = std::move(resize_shape),
      .values = std::move(resize_data)};
  const int32_t resize_tensor_index = SerializeTensorWithBuffer<int32_t>(
      resize_info, builder, buffers, tensors);

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const uint32_t operator_code_index =
      GetOperatorCodeIndex(operator_code, builder, operator_codes);
  const std::array<int32_t, 2> op_inputs = {input_index, resize_tensor_index};
  const std::array<int32_t, 1> op_outputs = {output_index};
  return tflite::CreateOperator(builder, operator_code_index,
                                builder.CreateVector<int32_t>(op_inputs),
                                builder.CreateVector<int32_t>(op_outputs),
                                builtin_options_type, builtin_options);
}

OperatorOffset SerializeReshape(const OperandToIndexMap& operand_to_index_map,
                                const MLOperator* reshape,
                                flatbuffers::FlatBufferBuilder& builder,
                                Vector<OperatorCodeOffset>& operator_codes) {
  const int32_t input_index =
      GetOperatorInputIndex(reshape, operand_to_index_map);
  const int32_t output_index =
      GetOperatorOutputIndex(reshape, operand_to_index_map);

  // Create `tflite::ReshapeOptions` with output dimensions.
  const auto& output = reshape->Outputs()[0];
  const auto output_dimensions = ConvertDimensions(output->Dimensions());
  // The output dimensions has been verified before creating tflite tensor.
  CHECK(output_dimensions.has_value());
  const auto reshape_options = tflite::CreateReshapeOptions(
      builder, builder.CreateVector<int32_t>(output_dimensions.value()));

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const auto operator_code_index = GetOperatorCodeIndex(
      tflite::BuiltinOperator_RESHAPE, builder, operator_codes);
  const std::vector<int32_t> operator_inputs = {input_index};
  const std::vector<int32_t> operator_outputs = {output_index};
  return tflite::CreateOperator(builder, operator_code_index,
                                builder.CreateVector<int32_t>(operator_inputs),
                                builder.CreateVector<int32_t>(operator_outputs),
                                tflite::BuiltinOptions_ReshapeOptions,
                                reshape_options.Union());
}

OperatorOffset SerializeSoftmax(const OperandToIndexMap& operand_to_index_map,
                                const MLOperator* softmax,
                                flatbuffers::FlatBufferBuilder& builder,
                                Vector<OperatorCodeOffset>& operator_codes) {
  const int32_t input_index =
      GetOperatorInputIndex(softmax, operand_to_index_map);
  const int32_t output_index =
      GetOperatorOutputIndex(softmax, operand_to_index_map);

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const auto softmax_options =
      tflite::CreateSoftmaxOptions(builder, /*beta*/ 1.0);
  const auto operator_code_index = GetOperatorCodeIndex(
      tflite::BuiltinOperator_SOFTMAX, builder, operator_codes);
  const std::vector<int32_t> operator_inputs = {input_index};
  const std::vector<int32_t> operator_outputs = {output_index};
  return tflite::CreateOperator(builder, operator_code_index,
                                builder.CreateVector<int32_t>(operator_inputs),
                                builder.CreateVector<int32_t>(operator_outputs),
                                tflite::BuiltinOptions_SoftmaxOptions,
                                softmax_options.Union());
}

base::expected<OperatorOffset, String> SerializeTranspose(
    const OperandToIndexMap& operand_to_index_map,
    const MLOperator* transpose,
    flatbuffers::FlatBufferBuilder& builder,
    Vector<OperatorCodeOffset>& operator_codes,
    Vector<BufferOffset>& buffers,
    Vector<TensorOffset>& tensors) {
  const int32_t input_index =
      GetOperatorInputIndex(transpose, operand_to_index_map);
  const int32_t output_index =
      GetOperatorOutputIndex(transpose, operand_to_index_map);

  const MLTransposeOptions* options =
      static_cast<const MLTransposeOptions*>(transpose->Options());
  const auto* input = transpose->Inputs()[0].Get();
  CHECK(input);
  const auto input_rank = input->Dimensions().size();
  const Vector<uint32_t> permutation =
      options->getPermutationOr(CreateDefaultPermutation(input_rank));
  const auto tflite_permutation = ConvertDimensions(permutation);
  RETURN_IF_ERROR(tflite_permutation);

  return SerializeTranspose(input_index, output_index,
                            tflite_permutation.value(), builder, operator_codes,
                            buffers, tensors);
}

}  // namespace

MLGraphTfLiteConverter::MLGraphTfLiteConverter() {
  // TFLite requires the first entry in FlatBuffer to be an empty buffer.
  buffers_.push_back(tflite::CreateBuffer(builder_, builder_.CreateVector({})));
}

MLGraphTfLiteConverter::~MLGraphTfLiteConverter() = default;

uint32_t MLGraphTfLiteConverter::SerializeBuffer(const MLOperand* constant) {
  auto* const array_buffer_view = constant->ArrayBufferView();
  CHECK_NE(array_buffer_view, nullptr);
  CHECK(!array_buffer_view->IsDetached());
  // Create `tflite::Buffer` with raw data buffers for constant operand.
  const auto buffer_data =
      builder_.CreateVector(reinterpret_cast<const uint8_t*>(
                                array_buffer_view->BaseAddressMaybeShared()),
                            array_buffer_view->byteLength());
  const auto buffer_index = base::checked_cast<uint32_t>(buffers_.size());
  buffers_.emplace_back(tflite::CreateBuffer(builder_, buffer_data));
  // The index of buffer is referenced by tensors.
  return buffer_index;
}

base::expected<int32_t, String> MLGraphTfLiteConverter::SerializeTensor(
    const MLOperand* operand,
    std::optional<String> graph_output_name) {
  // The buffer index 0 represents input and output operand because there is no
  // data buffer associated.
  uint32_t buffer_index = 0;
  // The index of `tflite::Tensor` array, each `MLOperand` (input, constant,
  // output) will be converted and pushed back into the array, so it's increased
  // by one after each serialization in flat buffer.
  int32_t tensor_index = base::checked_cast<int32_t>(tensors_.size());
  CHECK_GE(tensor_index, int32_t(0));
  // The name identifies the tensor for inference, so only inputs and outputs of
  // graph have this attribute.
  std::optional<String> name;
  switch (operand->Kind()) {
    case webnn::mojom::blink::Operand::Kind::kInput: {
      name = operand->Name();
      // Fill the graph inputs with the index of input tensor.
      graph_input_ids_.push_back(tensor_index);
      break;
    }
    case webnn::mojom::blink::Operand::Kind::kConstant: {
      // Serialize buffer and return buffer index which starts from 1, it is
      // used to create the constant's tensor.
      buffer_index = SerializeBuffer(operand);
      break;
    }
    case webnn::mojom::blink::Operand::Kind::kOutput: {
      // The `kOutput` represents not only the intermediate operands of
      // operation, but also the outputs of graph.
      // It's a graph output if the argument `graph_output_name` has value.
      if (graph_output_name) {
        name = graph_output_name.value();
        // Fill the graph outputs with the index of output tensor.
        graph_outputs_ids_.push_back(tensor_index);
      }
      break;
    }
  }
  // Create `Tensor` with operand shape, the index of buffer and the name.
  const auto dimensions_result = ConvertDimensions(operand->Dimensions());
  RETURN_IF_ERROR(dimensions_result);
  const auto dimensions =
      builder_.CreateVector<int32_t>(dimensions_result.value());
  const auto operand_type = BlinkOperandTypeToTFLite(operand->DataType());
  const auto operand_name =
      name.has_value() ? builder_.CreateString(name->Utf8()) : 0;
  tensors_.emplace_back(tflite::CreateTensor(builder_, std::move(dimensions),
                                             operand_type, buffer_index,
                                             operand_name));
  return tensor_index;
}

base::expected<void, String> MLGraphTfLiteConverter::SerializeOperation(
    const OperandToIndexMap& operand_to_index_map,
    const MLOperator* op) {
  OperatorOffset operator_offset;
  switch (op->Kind()) {
    case webnn::mojom::blink::Operation::Tag::kClamp: {
      const auto clamp_result =
          SerializeClamp(operand_to_index_map, op, builder_, operator_codes_);
      RETURN_IF_ERROR(clamp_result);
      operator_offset = clamp_result.value();
      break;
    }
    case webnn::mojom::blink::Operation::Tag::kConcat:
      operator_offset =
          SerializeConcat(operand_to_index_map, op, builder_, operator_codes_);
      break;
    case webnn::mojom::blink::Operation::Tag::kConv2d: {
      const auto conv2d_result =
          SerializeConv2d(operand_to_index_map, op, builder_, operator_codes_,
                          operators_, buffers_, tensors_);
      RETURN_IF_ERROR(conv2d_result);
      operator_offset = conv2d_result.value();
      break;
    }
    case webnn::mojom::blink::Operation::Tag::kElementWiseBinary:
      operator_offset = SerializeElementWiseBinary(operand_to_index_map, op,
                                                   builder_, operator_codes_);
      break;
    case webnn::mojom::blink::Operation::Tag::kElementWiseUnary: {
      switch (op->SubKind<webnn::mojom::blink::ElementWiseUnary::Kind>()) {
        case webnn::mojom::blink::ElementWiseUnary::Kind::kAbs:
          operator_offset = SerializeUnaryOperator(tflite::BuiltinOperator_ABS,
                                                   operand_to_index_map, op,
                                                   builder_, operator_codes_);
          break;
        case webnn::mojom::blink::ElementWiseUnary::Kind::kCeil:
          operator_offset = SerializeUnaryOperator(tflite::BuiltinOperator_CEIL,
                                                   operand_to_index_map, op,
                                                   builder_, operator_codes_);
          break;
        case webnn::mojom::blink::ElementWiseUnary::Kind::kFloor:
          operator_offset = SerializeUnaryOperator(
              tflite::BuiltinOperator_FLOOR, operand_to_index_map, op, builder_,
              operator_codes_);
          break;
        case webnn::mojom::blink::ElementWiseUnary::Kind::kNeg:
          operator_offset = SerializeUnaryOperator(tflite::BuiltinOperator_NEG,
                                                   operand_to_index_map, op,
                                                   builder_, operator_codes_);
          break;
        case webnn::mojom::blink::ElementWiseUnary::Kind::kCos:
          operator_offset = SerializeUnaryOperator(tflite::BuiltinOperator_COS,
                                                   operand_to_index_map, op,
                                                   builder_, operator_codes_);
          break;
        case webnn::mojom::blink::ElementWiseUnary::Kind::kExp:
          operator_offset = SerializeUnaryOperator(tflite::BuiltinOperator_EXP,
                                                   operand_to_index_map, op,
                                                   builder_, operator_codes_);
          break;
        case webnn::mojom::blink::ElementWiseUnary::Kind::kLog:
          operator_offset = SerializeUnaryOperator(tflite::BuiltinOperator_LOG,
                                                   operand_to_index_map, op,
                                                   builder_, operator_codes_);
          break;
        case webnn::mojom::blink::ElementWiseUnary::Kind::kSin:
          operator_offset = SerializeUnaryOperator(tflite::BuiltinOperator_SIN,
                                                   operand_to_index_map, op,
                                                   builder_, operator_codes_);
          break;
        case webnn::mojom::blink::ElementWiseUnary::Kind::kSqrt:
          operator_offset = SerializeUnaryOperator(tflite::BuiltinOperator_SQRT,
                                                   operand_to_index_map, op,
                                                   builder_, operator_codes_);
          break;
        case webnn::mojom::blink::ElementWiseUnary::Kind::kCast:
          operator_offset = SerializeUnaryOperator(tflite::BuiltinOperator_CAST,
                                                   operand_to_index_map, op,
                                                   builder_, operator_codes_);
          break;
        case webnn::mojom::blink::ElementWiseUnary::Kind::kErf:
        case webnn::mojom::blink::ElementWiseUnary::Kind::kIdentity:
        case webnn::mojom::blink::ElementWiseUnary::Kind::kLogicalNot:
        case webnn::mojom::blink::ElementWiseUnary::Kind::kReciprocal:
        case webnn::mojom::blink::ElementWiseUnary::Kind::kTan:
          return base::unexpected(
              MLOperator::OperatorKindToString(
                  op->Kind(),
                  op->SubKind<webnn::mojom::blink::ElementWiseUnary::Kind>()) +
              " is not implemented.");
      }
      break;
    }
    case webnn::mojom::blink::Operation::Tag::kElu: {
      const auto elu_result =
          SerializeElu(operand_to_index_map, op, builder_, operator_codes_);
      // The scalar multiplier is not supported in tflite schema.
      RETURN_IF_ERROR(elu_result);
      operator_offset = elu_result.value();
      break;
    }
    case webnn::mojom::blink::Operation::Tag::kGemm: {
      const auto gemm_result =
          SerializeGemm(operand_to_index_map, op, builder_, operator_codes_,
                        operators_, buffers_, tensors_);
      // The alpha, beta and transpose options are not supported in tflite
      // schema.
      RETURN_IF_ERROR(gemm_result);
      operator_offset = gemm_result.value();
      break;
    }
    case webnn::mojom::blink::Operation::Tag::kHardSwish:
      operator_offset = SerializeUnaryOperator(
          tflite::BuiltinOperator_HARD_SWISH, operand_to_index_map, op,
          builder_, operator_codes_);
      break;
    case webnn::mojom::blink::Operation::Tag::kLeakyRelu:
      operator_offset = SerializeLeakyRelu(operand_to_index_map, op, builder_,
                                           operator_codes_);
      break;
    case webnn::mojom::blink::Operation::Tag::kPad: {
      const auto pad_result = SerializePad(operand_to_index_map, op, builder_,
                                           operator_codes_, buffers_, tensors_);
      // The Edge padding model is not supported in tflite schema.
      RETURN_IF_ERROR(pad_result);
      operator_offset = pad_result.value();
      break;
    }
    case webnn::mojom::blink::Operation::Tag::kPool2d: {
      const auto pool2d_result =
          SerializePool2d(operand_to_index_map, op, builder_, operator_codes_,
                          operators_, buffers_, tensors_);
      // Some pool2d attributes are not supported in tflite schema.
      RETURN_IF_ERROR(pool2d_result);
      operator_offset = pool2d_result.value();
      break;
    }
    case webnn::mojom::blink::Operation::Tag::kRelu:
      operator_offset = SerializeUnaryOperator(tflite::BuiltinOperator_RELU,
                                               operand_to_index_map, op,
                                               builder_, operator_codes_);
      break;
    case webnn::mojom::blink::Operation::Tag::kReshape:
      operator_offset =
          SerializeReshape(operand_to_index_map, op, builder_, operator_codes_);
      break;
    case webnn::mojom::blink::Operation::Tag::kResample2d: {
      const auto resize_result =
          SerializeResample2d(operand_to_index_map, op, builder_,
                              operator_codes_, buffers_, tensors_);
      // Resize operator in tflite schema only supports axes = {1, 2}.
      RETURN_IF_ERROR(resize_result);
      operator_offset = resize_result.value();
      break;
    }
    case webnn::mojom::blink::Operation::Tag::kSigmoid:
      operator_offset = SerializeUnaryOperator(tflite::BuiltinOperator_LOGISTIC,
                                               operand_to_index_map, op,
                                               builder_, operator_codes_);
      break;
    case webnn::mojom::blink::Operation::Tag::kSoftmax:
      operator_offset =
          SerializeSoftmax(operand_to_index_map, op, builder_, operator_codes_);
      break;
    case webnn::mojom::blink::Operation::Tag::kTranspose: {
      const auto transpose_result =
          SerializeTranspose(operand_to_index_map, op, builder_,
                             operator_codes_, buffers_, tensors_);
      // Fails to convert the data type of permutation from uint32 to int32.
      RETURN_IF_ERROR(transpose_result);
      operator_offset = transpose_result.value();
      break;
    }
    case webnn::mojom::blink::Operation::Tag::kArgMinMax:
    case webnn::mojom::blink::Operation::Tag::kBatchNormalization:
    case webnn::mojom::blink::Operation::Tag::kExpand:
    case webnn::mojom::blink::Operation::Tag::kGather:
    case webnn::mojom::blink::Operation::Tag::kGru:
    case webnn::mojom::blink::Operation::Tag::kHardSigmoid:
    case webnn::mojom::blink::Operation::Tag::kLayerNormalization:
    case webnn::mojom::blink::Operation::Tag::kInstanceNormalization:
    case webnn::mojom::blink::Operation::Tag::kLinear:
    case webnn::mojom::blink::Operation::Tag::kLstm:
    case webnn::mojom::blink::Operation::Tag::kMatmul:
    case webnn::mojom::blink::Operation::Tag::kPrelu:
    case webnn::mojom::blink::Operation::Tag::kReduce:
    case webnn::mojom::blink::Operation::Tag::kSlice:
    case webnn::mojom::blink::Operation::Tag::kSoftplus:
    case webnn::mojom::blink::Operation::Tag::kSoftsign:
    case webnn::mojom::blink::Operation::Tag::kSplit:
    case webnn::mojom::blink::Operation::Tag::kTanh:
    case webnn::mojom::blink::Operation::Tag::kTriangular:
    case webnn::mojom::blink::Operation::Tag::kWhere:
      return base::unexpected(MLOperator::OperatorKindToString(op->Kind()) +
                              " is not implemented.");
  }
  operators_.emplace_back(operator_offset);

  return base::ok();
}

flatbuffers::DetachedBuffer MLGraphTfLiteConverter::FinishAndTakeFlatBuffer() {
  CHECK(!is_created_model_);

  // Create `tflite::SubGraph`, which typically represents an entire model.
  // The inputs of subgraph are the list of non-static tensors that feed into
  // the subgraph for inference. The outputs of subgraph are considered the
  // product of the subgraph's inference. The operators are in execution order.
  flatbuffers::Offset<tflite::SubGraph> subgraph = tflite::CreateSubGraph(
      builder_, builder_.CreateVector(tensors_.data(), tensors_.size()),
      builder_.CreateVector<int32_t>(graph_input_ids_),
      builder_.CreateVector<int32_t>(graph_outputs_ids_),
      builder_.CreateVector(operators_.data(), operators_.size()));

  flatbuffers::Offset<flatbuffers::String> description =
      builder_.CreateString("TF-Lite model converted from WebNN Graph");

  // The operator codes used in this model are kept in order because operators
  // carry an index into this vector.
  // There is only one subgraph in the model. The buffers of the model must be
  // initialized an empty buffer.
  flatbuffers::Offset<tflite::Model> model_buffer = tflite::CreateModel(
      builder_, TFLITE_SCHEMA_VERSION,
      builder_.CreateVector(operator_codes_.data(), operator_codes_.size()),
      builder_.CreateVector(&subgraph, 1), description,
      builder_.CreateVector(buffers_.data(), buffers_.size()));

  tflite::FinishModelBuffer(builder_, model_buffer);
  is_created_model_ = true;

  return builder_.Release();
}

}  // namespace blink
