// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_tflite_converter.h"

#include "base/ranges/algorithm.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_clamp_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pool_2d_options.h"
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

Vector<int32_t> ConvertDimensions(const Vector<uint32_t>& input_dimensions) {
  Vector<int32_t> output_dimensions;
  output_dimensions.reserve(input_dimensions.size());
  base::ranges::transform(input_dimensions,
                          std::back_inserter(output_dimensions),
                          [](const auto& dimension) {
                            return base::checked_cast<int32_t>(dimension);
                          });
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

// Helper to get tflite padding mode for convolution 2d or pooling 2d.
template <typename OptionsType>
base::expected<tflite::Padding, String> GetTfLitePaddingMode(
    const OptionsType* options,
    const webnn::Size2d<uint32_t>& input,
    const webnn::Size2d<uint32_t>& filter,
    const webnn::Size2d<uint32_t>& stride,
    const webnn::Size2d<uint32_t>& dilation) {
  CHECK(options);
  switch (options->autoPad().AsEnum()) {
    case V8MLAutoPad::Enum::kExplicit: {
      // Valid padding means there are no padding to be used as described here
      // https://www.tensorflow.org/api_docs/python/tf/nn#valid_padding.
      const Vector<uint32_t> no_padding = {0, 0, 0, 0};
      const auto explicit_padding = options->getPaddingOr(no_padding);
      CHECK_EQ(explicit_padding.size(), 4u);
      if (explicit_padding == no_padding) {
        return tflite::Padding_VALID;
      } else {
        // Convert the explicit padding to tflite same padding mode, throw
        // exception if the calculated padding with kSameUpper are not the same
        // as explicit padding.
        const auto padding_height = webnn::CalculateConv2dPadding(
            webnn::AutoPad::kSameUpper, input.height, filter.height,
            stride.height, dilation.height);
        const auto padding_width = webnn::CalculateConv2dPadding(
            webnn::AutoPad::kSameUpper, input.width, filter.width, stride.width,
            dilation.width);
        // WebNN explicit padding is in [beginning_height, ending_height,
        // beginning_width, ending_width] sequence.
        const Vector<uint32_t> upper_padding = {
            padding_height->begin, padding_height->end, padding_width->begin,
            padding_width->end};
        if (explicit_padding == upper_padding) {
          return tflite::Padding_SAME;
        } else {
          return base::unexpected(
              "The explicit padding are not supported in tflite.");
        }
      }
    }
    case V8MLAutoPad::Enum::kSameUpper:
      // Tflite same padding is the additional ending padding of the spatial
      // input dimensions by default.
      // https://www.tensorflow.org/api_docs/python/tf/nn#same_padding
      return tflite::Padding_SAME;
    case V8MLAutoPad::Enum::kSameLower:
      // The values in the padding array are ignored, so we don't need to
      // calculate if it's tflite same padding.
      return base::unexpected(
          "Same lower padding mode is not supported in tflite schema.");
  }

  NOTREACHED_NORETURN();
}

base::expected<tflite::ActivationFunctionType, String>
GetActivationFunctionType(const MLActivation* ml_activation) {
  CHECK(ml_activation);
  const MLOperator* op = ml_activation->Operator();
  CHECK(op);
  tflite::ActivationFunctionType activation =
      tflite::ActivationFunctionType_NONE;
  switch (op->Kind()) {
    case MLOperator::OperatorKind::kClamp: {
      const MLClampOptions* clamp_options =
          static_cast<const MLClampOptions*>(op->Options());
      CHECK(clamp_options);
      const float min =
          clamp_options->getMinValueOr(-std::numeric_limits<float>::infinity());
      const float max =
          clamp_options->getMaxValueOr(+std::numeric_limits<float>::infinity());
      if (min == 0.0f && max == 6.0f) {
        activation = tflite::ActivationFunctionType_RELU6;
      } else {
        return base::unexpected("Clamp activation is not supported.");
      }
      break;
    }
    case MLOperator::OperatorKind::kRelu:
      activation = tflite::ActivationFunctionType_RELU;
      break;
    default:
      return base::unexpected(MLOperator::OperatorKindToString(op->Kind()) +
                              " activation is not supported.");
  }

  return activation;
}

template <typename DataType>
uint32_t SerializeZeroBiasBuffer(V8MLOperandDataType::Enum data_type,
                                 uint32_t output_channels,
                                 flatbuffers::FlatBufferBuilder& builder,
                                 Vector<BufferOffset>& buffers,
                                 Vector<TensorOffset>& tensors) {
  // Create `tflite::Buffer` for the empty tensors.
  const auto buffer_index = base::checked_cast<uint32_t>(buffers.size());
  const std::vector<DataType> empty_data(output_channels);
  buffers.emplace_back(tflite::CreateBuffer(
      builder,
      builder.CreateVector(reinterpret_cast<const uint8_t*>(empty_data.data()),
                           empty_data.size() * sizeof(DataType))));

  // Create `tflite::Tensor` with the output channels and the index of buffer.
  const int32_t tensor_index = base::checked_cast<int32_t>(tensors.size());
  const auto dimensions = builder.CreateVector<int32_t>(
      {base::checked_cast<int32_t>(output_channels)});
  const auto operand_type = BlinkOperandTypeToTFLite(data_type);
  tensors.emplace_back(tflite::CreateTensor(builder, std::move(dimensions),
                                            operand_type, buffer_index));

  return tensor_index;
}

base::expected<OperatorOffset, String> SerializeConv2d(
    const OperandToIndexMap& operand_to_index_map,
    const MLOperator* conv2d,
    flatbuffers::FlatBufferBuilder& builder,
    Vector<OperatorCodeOffset>& operator_codes,
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
      IsDepthwiseConv2d(input_channels, output_channels, groups);

  // Validate filter layout for nhwc input layout that is being discussed to
  // simplify other variants in WebNN working group
  // https://github.com/webmachinelearning/webnn/issues/324.
  const auto validation_result = ValidateFilterLayout(
      depthwise, options->inputLayout(), options->filterLayout());
  if (!validation_result.has_value()) {
    return base::unexpected(validation_result.error());
  }

  // Validate activation operator that is partial support in tflite schema and
  // convert to tflite function type.
  tflite::ActivationFunctionType activation =
      tflite::ActivationFunctionType_NONE;
  if (options->hasActivation()) {
    const auto activation_result =
        GetActivationFunctionType(options->activation());
    if (!activation_result.has_value()) {
      return base::unexpected(validation_result.error());
    }
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
  if (!padding_mode.has_value()) {
    return base::unexpected(padding_mode.error());
  }

  tflite::BuiltinOperator operator_kind;
  tflite::BuiltinOptions builtin_options_type = tflite::BuiltinOptions_NONE;
  flatbuffers::Offset<void> builtin_options = 0;
  if (depthwise) {
    const uint32_t depth_multiplier = 1;
    operator_kind = tflite::BuiltinOperator_DEPTHWISE_CONV_2D;
    builtin_options = tflite::CreateDepthwiseConv2DOptions(
                          builder, padding_mode.value(), stride_size2d.width,
                          stride_size2d.height, depth_multiplier, activation,
                          dilation_size2d.width, dilation_size2d.height)
                          .Union();
    builtin_options_type = tflite::BuiltinOptions_DepthwiseConv2DOptions;
  } else {
    operator_kind = tflite::BuiltinOperator_CONV_2D;
    builtin_options = tflite::CreateConv2DOptions(
                          builder, padding_mode.value(), stride_size2d.width,
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
    bias_index = SerializeZeroBiasBuffer<float>(
        input->DataType(), output_channels, builder, buffers, tensors);
  }
  const std::vector<int32_t> op_inputs = {input_index, filter_index,
                                          bias_index};
  const std::vector<int32_t> op_outputs = {output_index};
  return tflite::CreateOperator(builder, operator_code_index,
                                builder.CreateVector<int32_t>(op_inputs),
                                builder.CreateVector<int32_t>(op_outputs),
                                builtin_options_type, builtin_options);
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
  switch (binary->Kind()) {
    case MLOperator::OperatorKind::kAdd:
      operator_kind = tflite::BuiltinOperator_ADD;
      break;
    case MLOperator::OperatorKind::kSub:
      operator_kind = tflite::BuiltinOperator_SUB;
      break;
    case MLOperator::OperatorKind::kMul:
      operator_kind = tflite::BuiltinOperator_MUL;
      break;
    case MLOperator::OperatorKind::kDiv:
      operator_kind = tflite::BuiltinOperator_DIV;
      break;
    case MLOperator::OperatorKind::kMin:
      operator_kind = tflite::BuiltinOperator_MINIMUM;
      break;
    case MLOperator::OperatorKind::kMax:
      operator_kind = tflite::BuiltinOperator_MAXIMUM;
      break;
    case MLOperator::OperatorKind::kPow:
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

base::expected<OperatorOffset, String> SerializePool2d(
    const OperandToIndexMap& operand_to_index_map,
    const MLOperator* pool2d,
    flatbuffers::FlatBufferBuilder& builder,
    Vector<OperatorCodeOffset>& operator_codes) {
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
  if (!padding_mode.has_value()) {
    return base::unexpected(padding_mode.error());
  }

  tflite::BuiltinOperator operator_kind;
  switch (pool2d->Kind()) {
    case MLOperator::OperatorKind::kAveragePool2d:
      operator_kind = tflite::BuiltinOperator_AVERAGE_POOL_2D;
      break;
    case MLOperator::OperatorKind::kMaxPool2d:
      operator_kind = tflite::BuiltinOperator_MAX_POOL_2D;
      break;
    default:
      NOTREACHED_NORETURN() << "The operator is not pool2d.";
  }

  const auto pool_2d_options = CreatePool2DOptions(
      builder, padding_mode.value(), stride_size2d.width, stride_size2d.height,
      filter_size2d.width, filter_size2d.height,
      tflite::ActivationFunctionType_NONE);

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const auto operator_code_index =
      GetOperatorCodeIndex(operator_kind, builder, operator_codes);
  const std::vector<int32_t> op_inputs = {input_index};
  const std::vector<int32_t> op_outputs = {output_index};
  return tflite::CreateOperator(
      builder, operator_code_index, builder.CreateVector<int32_t>(op_inputs),
      builder.CreateVector<int32_t>(op_outputs),
      tflite::BuiltinOptions_Pool2DOptions, pool_2d_options.Union());
}

OperatorOffset SerializeRelu(const OperandToIndexMap& operand_to_index_map,
                             const MLOperator* relu,
                             flatbuffers::FlatBufferBuilder& builder,
                             Vector<OperatorCodeOffset>& operator_codes) {
  const int32_t input_index = GetOperatorInputIndex(relu, operand_to_index_map);
  const int32_t output_index =
      GetOperatorOutputIndex(relu, operand_to_index_map);

  // Create `tflite::Operator` with the tensor index of inputs and outputs
  // operand. The type of operation is determined by the index of the operator
  // code.
  const auto operator_code_index = GetOperatorCodeIndex(
      tflite::BuiltinOperator_RELU, builder, operator_codes);
  const std::vector<int32_t> op_inputs = {input_index};
  const std::vector<int32_t> op_outputs = {output_index};
  return tflite::CreateOperator(builder, operator_code_index,
                                builder.CreateVector<int32_t>(op_inputs),
                                builder.CreateVector<int32_t>(op_outputs));
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
  const auto reshape_options = tflite::CreateReshapeOptions(
      builder,
      builder.CreateVector<int32_t>(ConvertDimensions(output->Dimensions())));

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

int32_t MLGraphTfLiteConverter::SerializeTensor(
    const MLOperand* operand,
    absl::optional<String> graph_output_name) {
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
  absl::optional<String> name;
  switch (operand->Kind()) {
    case MLOperand::OperandKind::kInput: {
      name = operand->Name();
      // Fill the graph inputs with the index of input tensor.
      graph_input_ids_.push_back(tensor_index);
      break;
    }
    case MLOperand::OperandKind::kConstant: {
      // Serialize buffer and return buffer index which starts from 1, it is
      // used to create the constant's tensor.
      buffer_index = SerializeBuffer(operand);
      break;
    }
    case MLOperand::OperandKind::kOutput: {
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
  const auto dimensions =
      builder_.CreateVector<int32_t>(ConvertDimensions(operand->Dimensions()));
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
    case MLOperator::OperatorKind::kConv2d: {
      const auto conv2d_result =
          SerializeConv2d(operand_to_index_map, op, builder_, operator_codes_,
                          buffers_, tensors_);
      // Some conv2d attributes are not supported in tflite schema.
      if (!conv2d_result.has_value()) {
        return base::unexpected(conv2d_result.error());
      }
      operator_offset = conv2d_result.value();
      break;
    }
    case MLOperator::OperatorKind::kAdd:
    case MLOperator::OperatorKind::kSub:
    case MLOperator::OperatorKind::kMul:
    case MLOperator::OperatorKind::kDiv:
    case MLOperator::OperatorKind::kMin:
    case MLOperator::OperatorKind::kMax:
    case MLOperator::OperatorKind::kPow:
      operator_offset = SerializeElementWiseBinary(operand_to_index_map, op,
                                                   builder_, operator_codes_);
      break;
    case MLOperator::OperatorKind::kAveragePool2d:
      [[fallthrough]];
    case MLOperator::OperatorKind::kMaxPool2d: {
      const auto pool2d_result =
          SerializePool2d(operand_to_index_map, op, builder_, operator_codes_);
      // Some pool2d attributes are not supported in tflite schema.
      if (!pool2d_result.has_value()) {
        return base::unexpected(pool2d_result.error());
      }
      operator_offset = pool2d_result.value();
      break;
    }
    case MLOperator::OperatorKind::kRelu:
      operator_offset =
          SerializeRelu(operand_to_index_map, op, builder_, operator_codes_);
      break;
    case MLOperator::OperatorKind::kReshape:
      operator_offset =
          SerializeReshape(operand_to_index_map, op, builder_, operator_codes_);
      break;
    case MLOperator::OperatorKind::kSoftmax:
      operator_offset =
          SerializeSoftmax(operand_to_index_map, op, builder_, operator_codes_);
      break;
    default:
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
