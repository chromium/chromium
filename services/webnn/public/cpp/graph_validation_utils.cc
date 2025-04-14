// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/graph_validation_utils.h"

#include <algorithm>
#include <numeric>
#include <set>
#include <variant>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/types/fixed_array.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/cpp/supported_tensors.h"
#include "services/webnn/public/cpp/webnn_errors.h"

namespace webnn {

namespace {

// The error message labels for corresponding operands.
static constexpr char kBiasParam[] = "bias";
static constexpr char kCellStateParam[] = "cellState";
static constexpr char kConditionParam[] = "condition";
static constexpr char kFalseValueParam[] = "falseValue";
static constexpr char kFilterParam[] = "filter";
static constexpr char kGemmAParam[] = "gemmA";
static constexpr char kGemmBParam[] = "gemmB";
static constexpr char kGemmCParam[] = "gemmC";
static constexpr char kHiddenStateParam[] = "hiddenState";
static constexpr char kIndicesParam[] = "indices";
static constexpr char kInitialCellStateParam[] = "initialCellState";
static constexpr char kInitialHiddenStateParam[] = "initialHiddenState";
static constexpr char kMeanParam[] = "mean";
static constexpr char kPeepholeWeightParam[] = "peepholeWeight";
static constexpr char kRecurrentBiasParam[] = "recurrentBias";
static constexpr char kRecurrentWeightParam[] = "recurrentWeight";
static constexpr char kScaleParam[] = "scale";
static constexpr char kSlopeParam[] = "slope";
static constexpr char kTrueValueParam[] = "trueValue";
static constexpr char kUpdatesParam[] = "updates";
static constexpr char kVarianceParam[] = "variance";
static constexpr char kWeightParam[] = "weight";
static constexpr char kZeroPointParam[] = "zeroPoint";

// Calculate the output size for conv2d based on WebNN spec:
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-conv2d
// Return the calculated output size if no error.
base::expected<double, std::string> CalculateConv2dOutputSize(
    const uint32_t input_size,
    const uint32_t filter_size,
    const uint32_t beginning_padding,
    const uint32_t ending_padding,
    const uint32_t stride,
    const uint32_t dilation,
    std::string_view label) {
  // Calculate the dilated filter sizes.
  auto checked_effective_filter_size =
      (base::MakeCheckedNum<uint32_t>(filter_size) - 1) * dilation + 1;
  if (!checked_effective_filter_size.IsValid()) {
    return base::unexpected(
        ErrorWithLabel(label, "The effective filter size is too large."));
  }

  // Calculate the output size in double precision floating point number that
  // ensures all dimension values of type uint32_t can be exactly represented.
  // https://en.wikipedia.org/wiki/Double-precision_floating-point_format#Precision_limitations_on_integer_values
  // The max value of checked_output_size should be 3 * UINT_MAX + 1,
  // which is smaller than the max safe integer value for double type.
  auto checked_output_size =
      (base::MakeCheckedNum<double>(input_size) -
       checked_effective_filter_size + beginning_padding + ending_padding) /
          stride +
      1;

  if (checked_output_size.ValueOrDie() <= 0) {
    return base::unexpected(ErrorWithLabel(
        label, "The input size is too small to fill the window."));
  }

  // Check if the value is valid for rounding to uint32_t type.
  if (!checked_output_size.IsValid<uint32_t>()) {
    return base::unexpected(
        ErrorWithLabel(label, "The output size is too large."));
  }

  return checked_output_size.ValueOrDie();
}

// Validate and calculate the output spatial dimensions of conv2d given
// input sizes, filter sizes, padding, strides and dilations.
// Return the calculated output sizes in double precision floating point number
// if no errors.
base::expected<Size2d<double>, std::string>
ValidateAndCalculateConv2dOutputSizes(const uint32_t input_height,
                                      const uint32_t input_width,
                                      const uint32_t filter_height,
                                      const uint32_t filter_width,
                                      const Padding2d& padding,
                                      const Size2d<uint32_t>& strides,
                                      const Size2d<uint32_t>& dilations,
                                      std::string_view label) {
  if (strides.height == 0 || strides.width == 0) {
    return base::unexpected(
        ErrorWithLabel(label, "All strides should be greater than 0."));
  }
  if (dilations.height == 0 || dilations.width == 0) {
    return base::unexpected(
        ErrorWithLabel(label, "All dilations should be greater than 0."));
  }

  const auto float_output_height = CalculateConv2dOutputSize(
      input_height, filter_height, padding.beginning.height,
      padding.ending.height, strides.height, dilations.height, label);
  if (!float_output_height.has_value()) {
    return base::unexpected(
        ErrorWithLabel(label, "Failed to calculate the output height: " +
                                  float_output_height.error()));
  }

  const auto float_output_width = CalculateConv2dOutputSize(
      input_width, filter_width, padding.beginning.width, padding.ending.width,
      strides.width, dilations.width, label);
  if (!float_output_width.has_value()) {
    return base::unexpected(ErrorWithLabel(
        label,
        "Failed to calculate the output width: " + float_output_width.error()));
  }

  return Size2d<double>{.height = float_output_height.value(),
                        .width = float_output_width.value()};
}

// Validate and calculate the output spatial dimensions of convTranspose2d given
// input sizes, filter sizes, padding, strides, dilations and output padding.
base::expected<Size2d<uint32_t>, std::string>
ValidateAndCalculateConvTranspose2dOutputSizes(
    const uint32_t input_height,
    const uint32_t input_width,
    const uint32_t filter_height,
    const uint32_t filter_width,
    const Padding2d& padding,
    const Size2d<uint32_t>& strides,
    const Size2d<uint32_t>& dilations,
    const Size2d<uint32_t>& output_padding,
    std::string_view label) {
  if (strides.height == 0 || strides.width == 0) {
    return base::unexpected(
        ErrorWithLabel(label, "All strides should be greater than 0."));
  }
  if (dilations.height == 0 || dilations.width == 0) {
    return base::unexpected(
        ErrorWithLabel(label, "All dilations should be greater than 0."));
  }
  if (output_padding.height >= strides.height ||
      output_padding.width >= strides.width) {
    return base::unexpected(ErrorWithLabel(
        label,
        "The output padding must be smaller than the stride along the same "
        "dimension."));
  }

  const auto output_height = CalculateConvTranspose2dOutputSize(
      input_height, filter_height, padding.beginning.height,
      padding.ending.height, strides.height, dilations.height,
      output_padding.height);
  if (!output_height.has_value()) {
    return base::unexpected(ErrorWithLabel(
        label,
        "Failed to calculate the output height: " + output_height.error()));
  }

  const auto output_width = CalculateConvTranspose2dOutputSize(
      input_width, filter_width, padding.beginning.width, padding.ending.width,
      strides.width, dilations.width, output_padding.width);
  if (!output_width.has_value()) {
    return base::unexpected(ErrorWithLabel(
        label,
        "Failed to calculate the output width: " + output_width.error()));
  }

  return Size2d<uint32_t>{.height = output_height.value(),
                          .width = output_width.value()};
}

struct Conv2dInputOutputInfo {
  uint32_t batches;
  uint32_t channels;
  uint32_t height;
  uint32_t width;
};

// Get the input info of 2-D direct and transposed convolution
// operation given input operand and attributes.
Conv2dInputOutputInfo GetConv2dInputInfo(
    const std::string& label,
    const OperandDescriptor& input,
    const Conv2dAttributesBase& attributes) {
  const std::vector<uint32_t>& input_shape = input.shape();
  // The input layout option specifies the layout format of the input tensor.
  uint32_t batches, channels, height, width;
  switch (attributes.input_layout) {
    case InputOperandLayout::kNchw:
      // "nchw": [batches, input_channels, height, width]
      batches = input_shape[0];
      channels = input_shape[1];
      height = input_shape[2];
      width = input_shape[3];
      break;
    case InputOperandLayout::kNhwc:
      // "nhwc": [batches, height, width, input_channels]
      batches = input_shape[0];
      height = input_shape[1];
      width = input_shape[2];
      channels = input_shape[3];
      break;
  }

  return Conv2dInputOutputInfo{.batches = batches,
                               .channels = channels,
                               .height = height,
                               .width = width};
}

// Validate the bias of 2-D direct and transposed convolution operation and
// create output operand given input operand, attributes and output info.
base::expected<OperandDescriptor, std::string>
ValidateConv2dBiasAndCreateOutputOperand(
    const ContextProperties& context_properties,
    const OperandDescriptor& input,
    const Conv2dAttributesBase& attributes,
    const Conv2dInputOutputInfo& output_info) {
  const std::string& label = attributes.label;
  // Validate bias operand if it is present.
  if (attributes.bias_operand) {
    if (attributes.bias_operand->shape()[0] != output_info.channels) {
      return base::unexpected(ErrorWithLabel(
          label, base::StringPrintf("The bias shape should be [%u].",
                                    output_info.channels)));
    }
    if (attributes.bias_operand->data_type() != input.data_type()) {
      return base::unexpected(ErrorWithLabel(
          label, "The bias data type doesn't match input data type."));
    }
  }

  // The input layout option specifies the layout format of the output tensor.
  std::array<uint32_t, 4> output_shape;
  switch (attributes.input_layout) {
    case InputOperandLayout::kNchw:
      // "nchw": [batches, output_channels, height, width]
      output_shape = {output_info.batches, output_info.channels,
                      output_info.height, output_info.width};
      break;
    case InputOperandLayout::kNhwc:
      // "nhwc": [batches, height, width, output_channels]
      output_shape = {output_info.batches, output_info.height,
                      output_info.width, output_info.channels};
      break;
  }

  return OperandDescriptor::Create(context_properties, input.data_type(),
                                   output_shape, label);
}

// Validate the axes and infer output for reduce operations.
base::expected<std::vector<uint32_t>, std::string>
ValidateReduceAxesAndInferOutput(base::span<const uint32_t> input_dimensions,
                                 base::span<const uint32_t> axes,
                                 bool keep_dimensions,
                                 std::string_view label) {
  auto input_rank = static_cast<uint32_t>(input_dimensions.size());
  RETURN_IF_ERROR(ValidateAxes(axes, input_rank, label));

  std::vector<uint32_t> output_shape;
  if (keep_dimensions) {
    output_shape.assign(input_dimensions.begin(), input_dimensions.end());
    for (auto axis : axes) {
      output_shape[axis] = 1;
    }
  } else {
    for (size_t i = 0; i < input_rank; i++) {
      if (!base::Contains(axes, i)) {
        output_shape.push_back(input_dimensions[i]);
      }
    }
  }
  return output_shape;
}

// Validate the operand of recurrent network.
base::expected<void, std::string> ValidateRecurrentNetworkOperand(
    const OperandDescriptor& operand,
    const char* operand_name,
    base::span<const uint32_t> expected_shape,
    OperandDataType input_data_type,
    std::string_view label) {
  if (!std::ranges::equal(operand.shape(), expected_shape)) {
    return base::unexpected(ErrorWithLabel(
        label,
        base::StringPrintf("The %s operand shape is invalid.", operand_name)));
  }
  if (operand.data_type() != input_data_type) {
    return base::unexpected(ErrorWithLabel(
        label,
        base::StringPrintf(
            "The %s operand data type doesn't match the input data type.",
            operand_name)));
  }
  return base::ok();
}

// This helper method is intended to validate mean, variance, scale and bias
// operands of batchNormalization and instanceNormalization against the input
// operand. These operands share the same constraint.
base::expected<void, std::string>
ValidateNormalizationOperandIsCompatibleWithInput(
    const OperandDescriptor& operand,
    const OperandDataType input_data_type,
    size_t input_size_on_axis,
    std::string_view label,
    std::string_view argument_name) {
  if (operand.data_type() != input_data_type) {
    return base::unexpected(ErrorWithLabel(
        label,
        base::StrCat(
            {"For ", argument_name,
             " operand: the data type doesn't match the input data type."})));
  }

  if (operand.shape()[0] != input_size_on_axis) {
    return base::unexpected(ErrorWithLabel(
        label,
        base::StrCat({"For ", argument_name,
                      " operand: the size of operand must be equal to the size "
                      "of the feature dimension of the input."})));
  }

  return base::ok();
}

}  // namespace

BatchNormalizationAttributes::BatchNormalizationAttributes() = default;
BatchNormalizationAttributes::~BatchNormalizationAttributes() = default;
BatchNormalizationAttributes::BatchNormalizationAttributes(
    BatchNormalizationAttributes&& other) = default;
BatchNormalizationAttributes& BatchNormalizationAttributes::operator=(
    BatchNormalizationAttributes&& other) = default;

Conv2dAttributesBase::Conv2dAttributesBase() = default;
Conv2dAttributesBase::~Conv2dAttributesBase() = default;
Conv2dAttributesBase::Conv2dAttributesBase(Conv2dAttributesBase&& other) =
    default;
Conv2dAttributesBase& Conv2dAttributesBase::operator=(
    Conv2dAttributesBase&& other) = default;

Conv2dAttributes::Conv2dAttributes() = default;
Conv2dAttributes::~Conv2dAttributes() = default;
Conv2dAttributes::Conv2dAttributes(Conv2dAttributes&& other) = default;
Conv2dAttributes& Conv2dAttributes::operator=(Conv2dAttributes&& other) =
    default;

ConvTranspose2dAttributes::ConvTranspose2dAttributes() = default;
ConvTranspose2dAttributes::~ConvTranspose2dAttributes() = default;
ConvTranspose2dAttributes::ConvTranspose2dAttributes(
    ConvTranspose2dAttributes&& other) = default;
ConvTranspose2dAttributes& ConvTranspose2dAttributes::operator=(
    ConvTranspose2dAttributes&& other) = default;

GemmAttributes::GemmAttributes() = default;
GemmAttributes::~GemmAttributes() = default;
GemmAttributes::GemmAttributes(GemmAttributes&& other) = default;
GemmAttributes& GemmAttributes::operator=(GemmAttributes&& other) = default;

GruAttributes::GruAttributes() = default;
GruAttributes::~GruAttributes() = default;
GruAttributes::GruAttributes(GruAttributes&& other) = default;
GruAttributes& GruAttributes::operator=(GruAttributes&& other) = default;

GruCellAttributes::GruCellAttributes() = default;
GruCellAttributes::~GruCellAttributes() = default;
GruCellAttributes::GruCellAttributes(GruCellAttributes&& other) = default;
GruCellAttributes& GruCellAttributes::operator=(GruCellAttributes&& other) =
    default;

InstanceNormalizationAttributes::InstanceNormalizationAttributes() = default;
InstanceNormalizationAttributes::~InstanceNormalizationAttributes() = default;
InstanceNormalizationAttributes::InstanceNormalizationAttributes(
    InstanceNormalizationAttributes&& other) = default;
InstanceNormalizationAttributes& InstanceNormalizationAttributes::operator=(
    InstanceNormalizationAttributes&& other) = default;

LayerNormalizationAttributes::LayerNormalizationAttributes() = default;
LayerNormalizationAttributes::~LayerNormalizationAttributes() = default;
LayerNormalizationAttributes::LayerNormalizationAttributes(
    LayerNormalizationAttributes&& other) = default;
LayerNormalizationAttributes& LayerNormalizationAttributes::operator=(
    LayerNormalizationAttributes&& other) = default;

LstmAttributes::LstmAttributes() = default;
LstmAttributes::~LstmAttributes() = default;
LstmAttributes::LstmAttributes(LstmAttributes&& other) = default;
LstmAttributes& LstmAttributes::operator=(LstmAttributes&& other) = default;

LstmCellAttributes::LstmCellAttributes() = default;
LstmCellAttributes::~LstmCellAttributes() = default;
LstmCellAttributes::LstmCellAttributes(LstmCellAttributes&& other) = default;
LstmCellAttributes& LstmCellAttributes::operator=(LstmCellAttributes&& other) =
    default;

Pool2dAttributes::Pool2dAttributes() = default;
Pool2dAttributes::~Pool2dAttributes() = default;
Pool2dAttributes::Pool2dAttributes(Pool2dAttributes&& other) = default;
Pool2dAttributes& Pool2dAttributes::operator=(Pool2dAttributes&& other) =
    default;

SliceAttributes::SliceAttributes() = default;
SliceAttributes::~SliceAttributes() = default;
SliceAttributes::SliceAttributes(SliceAttributes&& other) = default;
SliceAttributes& SliceAttributes::operator=(SliceAttributes&& other) = default;

base::expected<OperandDescriptor, std::string> ValidateArgMinMaxAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& input,
    std::string_view label,
    uint32_t axis,
    OperandDataType output_data_type,
    bool keep_dimensions) {
  if (!context_properties.data_type_limits.arg_min_max_input.Supports(input)) {
    return base::unexpected(ErrorWithLabel(
        label,
        NotSupportedInputArgumentError(
            input, context_properties.data_type_limits.arg_min_max_input)));
  }

  if (!context_properties.data_type_limits.arg_min_max_output.Has(
          output_data_type)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedOpOutputTypeError(
                   output_data_type,
                   context_properties.data_type_limits.arg_min_max_output)));
  }

  ASSIGN_OR_RETURN(std::vector<uint32_t> output_shape,
                   ValidateReduceAxesAndInferOutput(
                       input.shape(), std::array<uint32_t, 1>{axis},
                       keep_dimensions, label));

  return OperandDescriptor::Create(context_properties, output_data_type,
                                   output_shape, label);
}

base::expected<OperandDescriptor, std::string>
ValidateBatchNormalizationAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& input,
    const OperandDescriptor& mean,
    const OperandDescriptor& variance,
    const BatchNormalizationAttributes& attributes) {
  // Validate input operand.
  const std::string& label = attributes.label;
  if (!context_properties.data_type_limits.batch_normalization_input.Supports(
          input)) {
    return base::unexpected(ErrorWithLabel(
        label,
        NotSupportedInputArgumentError(
            input,
            context_properties.data_type_limits.batch_normalization_input)));
  }

  if (attributes.axis >= input.Rank()) {
    return base::unexpected(ErrorWithLabel(
        label,
        "The value of axis must be in the range [0, N-1] where N is the rank "
        "of the input tensor."));
  }

  uint32_t input_size_on_axis = input.shape()[attributes.axis];
  // Validate mean operand.
  if (!context_properties.data_type_limits.batch_normalization_mean.Supports(
          mean)) {
    return base::unexpected(ErrorWithLabel(
        label,
        NotSupportedArgumentError(
            kMeanParam, mean,
            context_properties.data_type_limits.batch_normalization_mean)));
  }
  RETURN_IF_ERROR(ValidateNormalizationOperandIsCompatibleWithInput(
      mean, input.data_type(), input_size_on_axis, label, kMeanParam));

  // Validate variance operand.
  if (!context_properties.data_type_limits.batch_normalization_mean.Supports(
          variance)) {
    return base::unexpected(ErrorWithLabel(
        label,
        NotSupportedArgumentError(
            kVarianceParam, variance,
            context_properties.data_type_limits.batch_normalization_mean)));
  }
  RETURN_IF_ERROR(ValidateNormalizationOperandIsCompatibleWithInput(
      variance, input.data_type(), input_size_on_axis, label, kVarianceParam));

  // Validate scale operand.
  if (attributes.scale) {
    if (!context_properties.data_type_limits.batch_normalization_mean.Supports(
            attributes.scale.value())) {
      return base::unexpected(ErrorWithLabel(
          label,
          NotSupportedArgumentError(
              kScaleParam, attributes.scale.value(),
              context_properties.data_type_limits.batch_normalization_mean)));
    }
    RETURN_IF_ERROR(ValidateNormalizationOperandIsCompatibleWithInput(
        attributes.scale.value(), input.data_type(), input_size_on_axis, label,
        kScaleParam));
  }

  // Validate bias operand.
  if (attributes.bias) {
    if (!context_properties.data_type_limits.batch_normalization_mean.Supports(
            attributes.bias.value())) {
      return base::unexpected(ErrorWithLabel(
          label,
          NotSupportedArgumentError(
              kBiasParam, attributes.bias.value(),
              context_properties.data_type_limits.batch_normalization_mean)));
    }
    RETURN_IF_ERROR(ValidateNormalizationOperandIsCompatibleWithInput(
        attributes.bias.value(), input.data_type(), input_size_on_axis, label,
        kBiasParam));
  }

  // The output tensor of batchNormalization is the same shape as the input
  // tensor.
  return input;
}

base::expected<OperandDescriptor, std::string> ValidateCastAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& input,
    OperandDataType output_data_type,
    std::string_view label) {
  // Validate input operand.
  if (!context_properties.data_type_limits.cast_input.Supports(input)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedInputArgumentError(
                   input, context_properties.data_type_limits.cast_input)));
  }

  // Validate output data type.
  if (!context_properties.data_type_limits.cast_input.data_types.Has(
          output_data_type)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedOpOutputTypeError(
                   output_data_type,
                   context_properties.data_type_limits.cast_input.data_types)));
  }

  return OperandDescriptor::Create(context_properties, output_data_type,
                                   input.shape(), label);
}

base::expected<OperandDescriptor, std::string> ValidateConcatAndInferOutput(
    const ContextProperties& context_properties,
    const std::vector<OperandDescriptor>& inputs,
    const uint32_t axis,
    std::string_view label) {
  if (inputs.empty()) {
    return base::unexpected(
        ErrorWithLabel(label, "The inputs should not be empty."));
  }

  for (const auto& input : inputs) {
    if (!context_properties.data_type_limits.concat_inputs.Supports(input)) {
      return base::unexpected(ErrorWithLabel(
          label,
          NotSupportedInputArgumentError(
              input, context_properties.data_type_limits.concat_inputs)));
    }
  }

  const auto first_input_rank = inputs[0].Rank();
  // According to WebNN spec:
  // https://www.w3.org/TR/webnn/#dom-mlgraphbuilder-concat-inputs-axis-axis,
  // the axis that the inputs concatenate along, with the value in the interval
  // [0, N-1] where N is the rank of input tensors. We just check the first
  // input rank here because we will check all inputs have same rank in the
  // following loop.
  if (axis >= first_input_rank) {
    return base::unexpected(ErrorWithLabel(
        label,
        "The axis must be in the range [0, N-1] where N is the rank of input "
        "tensor."));
  }

  const std::vector<uint32_t>& first_input_shape = inputs[0].shape();
  const auto output_type = inputs[0].data_type();
  // The loop skips the first input to avoid repeated checks.
  for (size_t i = 1; i < inputs.size(); ++i) {
    if (inputs[i].data_type() != output_type) {
      return base::unexpected(
          ErrorWithLabel(label, "The input data types don't match."));
    }
    // According to WebNN spec:
    // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-concat, all input tensors
    // must have the same dimension.
    if (inputs[i].Rank() != first_input_rank) {
      return base::unexpected(ErrorWithLabel(
          label, "All input tensors must have the same dimension."));
    }
    // According to WebNN spec:
    // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-concat, all input tensors
    // must have the same shape, except for the size of the dimension to
    // concatenate on.
    for (size_t dim = 0; dim < first_input_rank; ++dim) {
      if (dim == axis || inputs[i].shape()[dim] == first_input_shape[dim]) {
        continue;
      }
      return base::unexpected(ErrorWithLabel(
          label,
          "All input tensors must have the same shape, except for the size of "
          "the dimension to concatenate on."));
    }
  }
  // Calculate the output shape according to WebNN spec:
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-concat, the output tensor
  // has the same shape except on the dimension that all the inputs concatenated
  // along. The size of that dimension is computed as the sum of all the input
  // sizes of the same dimension.
  auto axis_size = base::MakeCheckedNum<uint32_t>(0);
  for (auto& input : inputs) {
    axis_size += input.shape()[axis];
  }
  std::vector<uint32_t> output_shape = first_input_shape;
  if (!axis_size.AssignIfValid(&output_shape[axis])) {
    return base::unexpected(
        ErrorWithLabel(label, "The concatenated dimension size is too large."));
  }

  return OperandDescriptor::Create(context_properties, output_type,
                                   output_shape, label);
}

base::expected<OperandDescriptor, std::string> ValidateConv2dAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& input,
    const OperandDescriptor& filter,
    const Conv2dAttributes& attributes) {
  const std::string& label = attributes.label;
  // Validate input operand.
  if (!context_properties.data_type_limits.conv2d_input.Supports(input)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedInputArgumentError(
                   input, context_properties.data_type_limits.conv2d_input)));
  }
  Conv2dInputOutputInfo input_info =
      GetConv2dInputInfo(label, input, attributes);

  // Validate filter operand.
  if (!context_properties.data_type_limits.conv2d_input.Supports(filter)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedArgumentError(
                   kFilterParam, filter,
                   context_properties.data_type_limits.conv2d_input)));
  }
  if (filter.data_type() != input.data_type()) {
    return base::unexpected(ErrorWithLabel(
        label, "The filter data type doesn't match the input data type."));
  }

  // Validate bias operand if it is present.
  if (attributes.bias_operand) {
    if (!context_properties.data_type_limits.conv2d_bias.Supports(
            attributes.bias_operand.value())) {
      return base::unexpected(ErrorWithLabel(
          label, NotSupportedArgumentError(
                     kBiasParam, attributes.bias_operand.value(),
                     context_properties.data_type_limits.conv2d_bias)));
    }
  }

  const std::vector<uint32_t>& filter_shape = filter.shape();
  uint32_t filter_height, filter_width, output_channels, filter_input_channels;
  // The conv2d filter layout specifies the filter layout format.
  switch (attributes.filter_layout) {
    case Conv2dFilterOperandLayout::kHwio:
      // "hwio": [height, width, input_channels/groups, output_channels]
      filter_height = filter_shape[0];
      filter_width = filter_shape[1];
      filter_input_channels = filter_shape[2];
      output_channels = filter_shape[3];
      break;
    case Conv2dFilterOperandLayout::kOhwi:
      // "ohwi": [output_channels, height, width, input_channels/groups]
      output_channels = filter_shape[0];
      filter_height = filter_shape[1];
      filter_width = filter_shape[2];
      filter_input_channels = filter_shape[3];
      break;
    case Conv2dFilterOperandLayout::kIhwo:
      // "ihwo": [input_channels/groups, height, width, output_channels]
      filter_input_channels = filter_shape[0];
      filter_height = filter_shape[1];
      filter_width = filter_shape[2];
      output_channels = filter_shape[3];
      break;
    case Conv2dFilterOperandLayout::kOihw:
      // "oihw": [output_channels, input_channels/groups, height, width]
      output_channels = filter_shape[0];
      filter_input_channels = filter_shape[1];
      filter_height = filter_shape[2];
      filter_width = filter_shape[3];
      break;
  }

  // Validate groups and input channels.
  if (attributes.groups == 0) {
    return base::unexpected(
        ErrorWithLabel(label, "The groups should be greater than 0."));
  }
  if (input_info.channels % attributes.groups != 0 ||
      filter_input_channels != input_info.channels / attributes.groups) {
    return base::unexpected(ErrorWithLabel(
        label,
        "The groups must evenly divide the input channels to filter input "
        "channels."));
  }

  // Validate and calculate output sizes.
  ASSIGN_OR_RETURN(
      Size2d<double> output_sizes,
      ValidateAndCalculateConv2dOutputSizes(
          input_info.height, input_info.width, filter_height, filter_width,
          attributes.padding, attributes.strides, attributes.dilations, label));

  uint32_t output_height = base::ClampFloor<uint32_t>(output_sizes.height);
  uint32_t output_width = base::ClampFloor<uint32_t>(output_sizes.width);

  Conv2dInputOutputInfo output_info{.batches = input_info.batches,
                                    .channels = output_channels,
                                    .height = output_height,
                                    .width = output_width};
  return ValidateConv2dBiasAndCreateOutputOperand(context_properties, input,
                                                  attributes, output_info);
}

base::expected<OperandDescriptor, std::string>
ValidateConvTranspose2dAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& input,
    const OperandDescriptor& filter,
    const ConvTranspose2dAttributes& attributes) {
  // Validate input operand.
  const std::string& label = attributes.label;
  if (!context_properties.data_type_limits.conv_transpose2d_input.Supports(
          input)) {
    return base::unexpected(ErrorWithLabel(
        label,
        NotSupportedInputArgumentError(
            input,
            context_properties.data_type_limits.conv_transpose2d_input)));
  }
  const auto input_info = GetConv2dInputInfo(label, input, attributes);

  // Validate filter operand.
  if (!context_properties.data_type_limits.conv_transpose2d_input.Supports(
          filter)) {
    return base::unexpected(ErrorWithLabel(
        label,
        NotSupportedArgumentError(
            kFilterParam, filter,
            context_properties.data_type_limits.conv_transpose2d_input)));
  }
  if (filter.data_type() != input.data_type()) {
    return base::unexpected(ErrorWithLabel(
        label, "The filter data type doesn't match the input data type."));
  }

  // Validate bias operand if it is present.
  if (attributes.bias_operand) {
    if (!context_properties.data_type_limits.conv_transpose2d_bias.Supports(
            attributes.bias_operand.value())) {
      return base::unexpected(ErrorWithLabel(
          label,
          NotSupportedArgumentError(
              kBiasParam, attributes.bias_operand.value(),
              context_properties.data_type_limits.conv_transpose2d_bias)));
    }
  }

  const std::vector<uint32_t>& filter_shape = filter.shape();
  uint32_t input_channels, filter_height, filter_width, filter_output_channels;
  // The conv2d filter layout specifies the filter layout format.
  switch (attributes.filter_layout) {
    case ConvTranspose2dFilterOperandLayout::kIohw:
      // "iohw": [input_channels, output_channels/groups, height, width]
      input_channels = filter_shape[0];
      filter_output_channels = filter_shape[1];
      filter_height = filter_shape[2];
      filter_width = filter_shape[3];
      break;
    case ConvTranspose2dFilterOperandLayout::kHwoi:
      // "hwoi": [height, width, output_channels/groups, input_channels]
      filter_height = filter_shape[0];
      filter_width = filter_shape[1];
      filter_output_channels = filter_shape[2];
      input_channels = filter_shape[3];
      break;
    case ConvTranspose2dFilterOperandLayout::kOhwi:
      // "ohwi": [output_channels/groups, height, width, input_channels]
      filter_output_channels = filter_shape[0];
      filter_height = filter_shape[1];
      filter_width = filter_shape[2];
      input_channels = filter_shape[3];
      break;
  }
  // Validate groups, input channels and calculate output channels.
  if (attributes.groups == 0) {
    return base::unexpected(
        ErrorWithLabel(label, "The groups should be greater than 0."));
  }
  if (input_info.channels != input_channels) {
    return base::unexpected(ErrorWithLabel(
        label, "The input channels should equal to filter input channels."));
  }
  const auto checked_output_channels =
      base::MakeCheckedNum<uint32_t>(filter_output_channels) *
      attributes.groups;
  if (!checked_output_channels.IsValid()) {
    return base::unexpected(
        ErrorWithLabel(label, "The output channels is too large."));
  }
  const uint32_t output_channels = checked_output_channels.ValueOrDie();

  // Validate and calculate output sizes.
  uint32_t output_height, output_width;
  if (attributes.output_sizes) {
    const auto& output_sizes = attributes.output_sizes;
    output_height = output_sizes->height;
    output_width = output_sizes->width;
    if (output_height <= 0 || output_width <= 0) {
      return base::unexpected(
          ErrorWithLabel(label, "All output sizes should be greater than 0."));
    }
    const auto strides = attributes.strides;
    ASSIGN_OR_RETURN(
        Size2d<uint32_t> calculated_output_sizes,
        ValidateAndCalculateConvTranspose2dOutputSizes(
            input_info.height, input_info.width, filter_height, filter_width,
            attributes.padding, strides, attributes.dilations,
            // According to WebNN spec:
            // https://webmachinelearning.github.io/webnn/#dom-mlconvtranspose2doptions-outputsizes
            // When the output sizes are explicitly specified, the output
            // padding values in outputPadding are ignored.
            {0, 0}, label));
    const auto calculated_output_height = calculated_output_sizes.height;
    const auto max_output_height =
        base::MakeCheckedNum<uint32_t>(calculated_output_height) +
        strides.height;
    if (!max_output_height.IsValid()) {
      return base::unexpected(ErrorWithLabel(
          label, "The checked maximum output height is too large"));
    }
    if (output_height < calculated_output_height ||
        output_height >= max_output_height.ValueOrDie()) {
      return base::unexpected(
          ErrorWithLabel(label, "The height of output sizes is invalid."));
    }
    const auto calculated_output_width = calculated_output_sizes.width;
    const auto max_output_width =
        base::MakeCheckedNum<uint32_t>(calculated_output_width) + strides.width;
    if (!max_output_width.IsValid()) {
      return base::unexpected(ErrorWithLabel(
          label, "The checked maximum output width is too large"));
    }
    if (output_width < calculated_output_width ||
        output_width >= max_output_width.ValueOrDie()) {
      return base::unexpected(
          ErrorWithLabel(label, "The width of output sizes is invalid."));
    }
  } else {
    ASSIGN_OR_RETURN(
        Size2d<uint32_t> output_sizes,
        ValidateAndCalculateConvTranspose2dOutputSizes(
            input_info.height, input_info.width, filter_height, filter_width,
            attributes.padding, attributes.strides, attributes.dilations,
            attributes.output_padding, label));
    output_height = output_sizes.height;
    output_width = output_sizes.width;
  }

  Conv2dInputOutputInfo output_info{.batches = input_info.batches,
                                    .channels = output_channels,
                                    .height = output_height,
                                    .width = output_width};
  return ValidateConv2dBiasAndCreateOutputOperand(context_properties, input,
                                                  attributes, output_info);
}

base::expected<OperandDescriptor, std::string>
ValidateCumulativeSumAndInferOutput(const ContextProperties& context_properties,
                                    const OperandDescriptor& input,
                                    const uint32_t axis,
                                    std::string_view label) {
  if (input.Rank() <= axis) {
    return base::unexpected(ErrorWithLabel(
        label, base::StringPrintf("The axis (%u) must be in the range [0, N-1] "
                                  "where N (%u) is the rank of input "
                                  "tensor.",
                                  axis, input.Rank())));
  }

  if (!context_properties.data_type_limits.cumulative_sum_input.Supports(
          input)) {
    return base::unexpected(ErrorWithLabel(
        label,
        NotSupportedInputArgumentError(
            input, context_properties.data_type_limits.cumulative_sum_input)));
  }

  // The data type and shape of input determine the output.
  return input;
}

// This helper method is intended to validate scale and zero_point
// operands of quantizeLinear and dequantizeLinear against the input
// operand.
// TODO(crbug.com/396176047): Make scale and zero_point's rank match with
// input.
base::expected<void, std::string>
ValidateScaleZeroPointOperandShapeIsCompatibleWithInput(
    base::span<const uint32_t> input_shape,
    base::span<const uint32_t> scale_shape,
    base::span<const uint32_t> zero_point_shape,
    std::string_view label) {
  // Check whether `scale_shape` is a subsample of `input_shape`.
  if (scale_shape.size() > input_shape.size()) {
    return base::unexpected(ErrorWithLabel(
        label, "The rank of scale is larger than the rank of input."));
  }

  for (size_t i = 0; i < scale_shape.size(); ++i) {
    auto scale_dim = scale_shape[scale_shape.size() - i - 1];
    auto input_dim = input_shape[input_shape.size() - i - 1];
    // The block_size should be an integer where block_size = dim_input /
    // dim_scale along the axis.
    if (input_dim % scale_dim != 0) {
      return base::unexpected(ErrorWithLabel(
          label,
          "The shape of scale is not a subsample of the shape of input."));
    }
  }

  if (!std::ranges::equal(scale_shape, zero_point_shape)) {
    return base::unexpected(ErrorWithLabel(
        label, "The shape of scale and zero point must be the same."));
  }
  return base::ok();
}

base::expected<OperandDescriptor, std::string>
ValidateDequantizeLinearAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& input,
    const OperandDescriptor& scale,
    const OperandDescriptor& zero_point,
    std::string_view label) {
  // Validate scale and zero_point operands.
  RETURN_IF_ERROR(ValidateScaleZeroPointOperandShapeIsCompatibleWithInput(
      input.shape(), scale.shape(), zero_point.shape(), label));

  if (!context_properties.data_type_limits.dequantize_linear_input.Supports(
          input)) {
    return base::unexpected(ErrorWithLabel(
        label,
        NotSupportedInputArgumentError(
            input,
            context_properties.data_type_limits.dequantize_linear_input)));
  }

  if (!context_properties.data_type_limits.dequantize_linear_zero_point
           .Supports(zero_point)) {
    return base::unexpected(ErrorWithLabel(
        label,
        NotSupportedArgumentError(
            kZeroPointParam, zero_point,
            context_properties.data_type_limits.dequantize_linear_zero_point)));
  }

  if (input.data_type() != zero_point.data_type()) {
    return base::unexpected(ErrorWithLabel(
        label, "The data type of input and zero point must be the same."));
  }

  if (!context_properties.data_type_limits.dequantize_linear_scale.Supports(
          scale)) {
    return base::unexpected(ErrorWithLabel(
        label,
        NotSupportedArgumentError(
            kScaleParam, scale,
            context_properties.data_type_limits.dequantize_linear_scale)));
  }

  // The data type of scale determines the output type.
  return OperandDescriptor::Create(context_properties, scale.data_type(),
                                   input.shape(), label);
}

base::expected<OperandDescriptor, std::string>
ValidateQuantizeLinearAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& input,
    const OperandDescriptor& scale,
    const OperandDescriptor& zero_point,
    std::string_view label) {
  // Validate scale and zero_point operands.
  RETURN_IF_ERROR(ValidateScaleZeroPointOperandShapeIsCompatibleWithInput(
      input.shape(), scale.shape(), zero_point.shape(), label));

  if (!context_properties.data_type_limits.quantize_linear_input.Supports(
          input)) {
    return base::unexpected(ErrorWithLabel(
        label,
        NotSupportedInputArgumentError(
            input, context_properties.data_type_limits.quantize_linear_input)));
  }

  if (!context_properties.data_type_limits.quantize_linear_input.Supports(
          scale)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedArgumentError(
                   kScaleParam, scale,
                   context_properties.data_type_limits.quantize_linear_input)));
  }

  if (input.data_type() != scale.data_type()) {
    return base::unexpected(ErrorWithLabel(
        label, "The data type of input and scale must be the same."));
  }

  if (!context_properties.data_type_limits.quantize_linear_zero_point.Supports(
          zero_point)) {
    return base::unexpected(ErrorWithLabel(
        label,
        NotSupportedArgumentError(
            kZeroPointParam, zero_point,
            context_properties.data_type_limits.quantize_linear_zero_point)));
  }
  // The data type of zero_point determines the output type.
  return OperandDescriptor::Create(context_properties, zero_point.data_type(),
                                   input.shape(), label);
}

base::expected<OperandDescriptor, std::string> ValidateExpandAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& input,
    base::span<const uint32_t> new_shape,
    std::string_view label) {
  // Validate input operand.
  if (!context_properties.data_type_limits.expand_input.Supports(input)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedInputArgumentError(
                   input, context_properties.data_type_limits.expand_input)));
  }

  std::optional<std::vector<uint32_t>> output_shape =
      BroadcastShapes(input.shape(), new_shape,
                      /*bidirectional=*/false);
  if (!output_shape) {
    return base::unexpected(ErrorWithLabel(
        label, "The input shape is not broadcastable to the new shape."));
  }
  CHECK_EQ(new_shape, base::span<const uint32_t>(*output_shape));

  return OperandDescriptor::Create(context_properties, input.data_type(),
                                   *output_shape, label);
}

base::expected<OperandDescriptor, std::string> ValidateGatherAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& input,
    const OperandDescriptor& indices,
    const uint32_t axis,
    std::string_view label) {
  // Validate input operand.
  if (!context_properties.data_type_limits.gather_input.Supports(input)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedInputArgumentError(
                   input, context_properties.data_type_limits.gather_input)));
  }

  if (input.Rank() <= axis) {
    return base::unexpected(ErrorWithLabel(
        label, base::StringPrintf("The axis (%u) must be in the range [0, N-1] "
                                  "where N=%u is the rank of input tensor.",
                                  axis, input.Rank())));
  }

  // Validate indices operand.
  if (!context_properties.data_type_limits.gather_indices.Supports(indices)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedArgumentError(
                   kIndicesParam, indices,
                   context_properties.data_type_limits.gather_indices)));
  }

  auto checked_output_rank =
      base::MakeCheckedNum<uint32_t>(input.Rank()) - 1 + indices.Rank();
  if (!checked_output_rank.IsValid()) {
    return base::unexpected(
        ErrorWithLabel(label, "The output rank is too large."));
  }

  std::vector<uint32_t> output_shape;
  output_shape.reserve(checked_output_rank.ValueOrDie());
  for (uint32_t i = 0; i < input.Rank(); ++i) {
    if (i == axis) {
      std::ranges::copy(indices.shape(), std::back_inserter(output_shape));
    } else {
      output_shape.push_back(input.shape()[i]);
    }
  }

  return OperandDescriptor::Create(context_properties, input.data_type(),
                                   output_shape, label);
}

base::expected<OperandDescriptor, std::string>
ValidateGatherElementsAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& input,
    const OperandDescriptor& indices,
    const uint32_t axis,
    std::string_view label) {
  // Validate input operand.
  if (!context_properties.data_type_limits.gather_elements_input.Supports(
          input)) {
    return base::unexpected(ErrorWithLabel(
        label,
        NotSupportedInputArgumentError(
            input, context_properties.data_type_limits.gather_elements_input)));
  }

  if (input.Rank() <= axis) {
    return base::unexpected(ErrorWithLabel(
        label, base::StringPrintf("The axis (%u) must be in the range [0, N-1] "
                                  "where N=%u is the rank of input "
                                  "tensor.",
                                  axis, input.Rank())));
  }

  // Validate indices operand.
  if (!context_properties.data_type_limits.gather_elements_indices.Supports(
          indices)) {
    return base::unexpected(ErrorWithLabel(
        label,
        NotSupportedArgumentError(
            kIndicesParam, indices,
            context_properties.data_type_limits.gather_elements_indices)));
  }

  if (input.Rank() != indices.Rank()) {
    return base::unexpected(ErrorWithLabel(
        label,
        base::StringPrintf(
            "The input rank (%u) must be equal to the indices rank (%u).",
            input.Rank(), indices.Rank())));
  }

  for (uint32_t i = 0; i < input.Rank(); ++i) {
    if (i == axis) {
      continue;
    }
    if (input.shape()[i] != indices.shape()[i]) {
      return base::unexpected(
          ErrorWithLabel(label,
                         "Except on the axis dimension, the input and indices "
                         "tensor must have the same dimension size."));
    }
  }

  return OperandDescriptor::Create(context_properties, input.data_type(),
                                   indices.shape(), label);
}

base::expected<OperandDescriptor, std::string> ValidateGatherNDAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& input,
    const OperandDescriptor& indices,
    std::string_view label) {
  // Validate input operand.
  if (!context_properties.data_type_limits.gather_nd_input.Supports(input)) {
    return base::unexpected(ErrorWithLabel(
        label,
        NotSupportedInputArgumentError(
            input, context_properties.data_type_limits.gather_nd_input)));
  }

  // Validate indices operand.
  if (!context_properties.data_type_limits.gather_nd_indices.Supports(
          indices)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedArgumentError(
                   kIndicesParam, indices,
                   context_properties.data_type_limits.gather_nd_indices)));
  }

  uint32_t indices_last_dimension_size = indices.shape()[indices.Rank() - 1];
  if (indices_last_dimension_size > input.Rank()) {
    return base::unexpected(ErrorWithLabel(
        label, base::StringPrintf(
                   "The last dimension size of indices (%u) must be less than "
                   "or equal to the input rank (%u).",
                   indices_last_dimension_size, input.Rank())));
  }

  auto checked_output_rank = base::MakeCheckedNum(indices.Rank()) - 1 +
                             input.Rank() - indices_last_dimension_size;
  if (!checked_output_rank.IsValid()) {
    return base::unexpected(
        ErrorWithLabel(label, "The output rank is too large."));
  }

  std::vector<uint32_t> output_shape;
  output_shape.reserve(checked_output_rank.ValueOrDie());
  std::ranges::copy(indices.shape().begin(), indices.shape().end() - 1,
                    std::back_inserter(output_shape));
  std::ranges::copy(input.shape().begin() + indices_last_dimension_size,
                    input.shape().end(), std::back_inserter(output_shape));

  return OperandDescriptor::Create(context_properties, input.data_type(),
                                   output_shape, label);
}

base::expected<OperandDescriptor, std::string> ValidateGemmAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& a,
    const OperandDescriptor& b,
    const GemmAttributes& attributes) {
  const std::string& label = attributes.label;
  // Validate a and b operand.
  if (!context_properties.data_type_limits.gemm_a.Supports(a)) {
    return base::unexpected(ErrorWithLabel(
        label,
        NotSupportedArgumentError(kGemmAParam, a,
                                  context_properties.data_type_limits.gemm_a)));
  }

  if (!context_properties.data_type_limits.gemm_a.Supports(b)) {
    return base::unexpected(ErrorWithLabel(
        label,
        NotSupportedArgumentError(kGemmBParam, b,
                                  context_properties.data_type_limits.gemm_a)));
  }

  if (a.data_type() != b.data_type()) {
    return base::unexpected(ErrorWithLabel(
        label, "The data types of first two inputs don't match."));
  }

  std::vector<uint32_t> shape_a = a.shape();
  if (attributes.a_transpose) {
    std::ranges::reverse(shape_a);
  }
  // The second input 2-D tensor with shape [K, N] if bTranspose is false, or
  // [N, K] if bTranspose is true.
  std::vector<uint32_t> shape_b = b.shape();
  if (attributes.b_transpose) {
    std::ranges::reverse(shape_b);
  }
  // The number of columns in the first matrix must be equal to the number of
  // rows in the second matrix.
  if (shape_a[1] != shape_b[0]) {
    return base::unexpected(ErrorWithLabel(
        label,
        base::StringPrintf(
            "The number of columns (%u) in the %sfirst matrix isn't equal to "
            "the number of rows (%u) in the %ssecond matrix.",
            shape_a[1], attributes.a_transpose ? "transposed " : "", shape_b[0],
            attributes.b_transpose ? "transposed " : "")));
  };
  // The output is 2-D tensor of shape [M, N].
  std::vector<uint32_t> output_shape = {shape_a[0], shape_b[1]};
  // The third input tensor c is either a scalar, or of the shape that is
  // unidirectionally broadcastable to the output shape [M, N].
  if (attributes.c_operand) {
    if (!context_properties.data_type_limits.gemm_c.Supports(
            attributes.c_operand.value())) {
      return base::unexpected(ErrorWithLabel(
          label, NotSupportedArgumentError(
                     kGemmCParam, attributes.c_operand.value(),
                     context_properties.data_type_limits.gemm_c)));
    }

    if (attributes.c_operand->data_type() != a.data_type()) {
      return base::unexpected(ErrorWithLabel(
          label,
          "The third input data type doesn't match other inputs' data type."));
    }

    if (!BroadcastShapes(attributes.c_operand->shape(), output_shape,
                         /*bidirectional=*/false)) {
      return base::unexpected(ErrorWithLabel(
          label,
          "The third input tensor isn't unidirectionally broadcastable to the "
          "output tensor."));
    }
  }

  return OperandDescriptor::Create(context_properties, a.data_type(),
                                   output_shape, label);
}

base::expected<std::vector<OperandDescriptor>, std::string>
ValidateGruAndInferOutput(const ContextProperties& context_properties,
                          const OperandDescriptor& input,
                          const OperandDescriptor& weight,
                          const OperandDescriptor& recurrent_weight,
                          uint32_t steps,
                          uint32_t hidden_size,
                          const GruAttributes& attributes) {
  const std::string& label = attributes.label;
  if (steps <= 0) {
    return base::unexpected(
        ErrorWithLabel(label, "The steps must be greater than 0."));
  }
  if (hidden_size <= 0) {
    return base::unexpected(
        ErrorWithLabel(label, "The hidden size must be greater than 0."));
  }

  // Validate the input operand.
  if (!context_properties.data_type_limits.gru_input.Supports(input)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedInputArgumentError(
                   input, context_properties.data_type_limits.gru_input)));
  }

  const std::vector<uint32_t>& input_dimensions = input.shape();
  if (input_dimensions[0] != steps) {
    return base::unexpected(ErrorWithLabel(
        label, "The input dimension[0] must be equal to the steps."));
  }
  const auto batch_size = input_dimensions[1];
  const auto input_size = input_dimensions[2];
  auto checked_three_times_hidden_size = base::MakeCheckedNum(hidden_size) * 3;
  uint32_t three_times_hidden_size;
  if (!checked_three_times_hidden_size.AssignIfValid(
          &three_times_hidden_size)) {
    return base::unexpected(
        ErrorWithLabel(label, "The hidden size is too large."));
  }
  const uint32_t num_directions =
      attributes.direction == RecurrentNetworkDirection::kBoth ? 2 : 1;

  // Validate the weight operand.
  if (!context_properties.data_type_limits.gru_input.Supports(weight)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedArgumentError(
                   kWeightParam, weight,
                   context_properties.data_type_limits.gru_input)));
  }
  std::array<uint32_t, 3> expected_weight_shape = {
      num_directions, three_times_hidden_size, input_size};
  RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
      weight, kWeightParam, expected_weight_shape, input.data_type(), label));

  // Validate the recurrent weight operand.
  if (!context_properties.data_type_limits.gru_input.Supports(
          recurrent_weight)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedArgumentError(
                   kRecurrentWeightParam, recurrent_weight,
                   context_properties.data_type_limits.gru_input)));
  }
  std::array<uint32_t, 3> expected_recurrent_weight_shape = {
      num_directions, three_times_hidden_size, hidden_size};
  RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
      recurrent_weight, kRecurrentWeightParam, expected_recurrent_weight_shape,
      input.data_type(), label));

  // Validate the bias operand.
  if (attributes.bias) {
    if (!context_properties.data_type_limits.gru_bias.Supports(
            attributes.bias.value())) {
      return base::unexpected(ErrorWithLabel(
          label, NotSupportedArgumentError(
                     kBiasParam, attributes.bias.value(),
                     context_properties.data_type_limits.gru_bias)));
    }
    std::array<uint32_t, 2> expected_bias_shape = {num_directions,
                                                   three_times_hidden_size};
    RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
        *attributes.bias, kBiasParam, expected_bias_shape, input.data_type(),
        label));
  }

  // Validate the recurrent bias operand.
  if (attributes.recurrent_bias) {
    if (!context_properties.data_type_limits.gru_bias.Supports(
            attributes.recurrent_bias.value())) {
      return base::unexpected(ErrorWithLabel(
          label, NotSupportedArgumentError(
                     kRecurrentBiasParam, attributes.recurrent_bias.value(),
                     context_properties.data_type_limits.gru_bias)));
    }
    std::array<uint32_t, 2> expected_recurrent_bias_shape = {
        num_directions, three_times_hidden_size};
    RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
        *attributes.recurrent_bias, kRecurrentBiasParam,
        expected_recurrent_bias_shape, input.data_type(), label));
  }

  // Validate the initial hidden state operand.
  if (attributes.initial_hidden_state) {
    if (!context_properties.data_type_limits.gru_input.Supports(
            attributes.initial_hidden_state.value())) {
      return base::unexpected(ErrorWithLabel(
          label,
          NotSupportedArgumentError(
              kInitialHiddenStateParam, attributes.initial_hidden_state.value(),
              context_properties.data_type_limits.gru_input)));
    }
    std::array<uint32_t, 3> expected_initial_hidden_state_shape = {
        num_directions, batch_size, hidden_size};
    RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
        *attributes.initial_hidden_state, kInitialHiddenStateParam,
        expected_initial_hidden_state_shape, input.data_type(), label));
  }

  if (attributes.activation_count != 2) {
    return base::unexpected(
        ErrorWithLabel(label, "The number of activations must be 2."));
  }

  std::vector<OperandDescriptor> outputs;
  ASSIGN_OR_RETURN(
      OperandDescriptor output,
      OperandDescriptor::Create(
          context_properties, input.data_type(),
          std::array{num_directions, batch_size, hidden_size}, label));
  outputs.push_back(std::move(output));
  if (attributes.return_sequence) {
    ASSIGN_OR_RETURN(
        OperandDescriptor return_sequence_output,
        OperandDescriptor::Create(
            context_properties, input.data_type(),
            std::array{steps, num_directions, batch_size, hidden_size}, label));
    outputs.push_back(std::move(return_sequence_output));
  }

  return outputs;
}

base::expected<OperandDescriptor, std::string> ValidateGruCellAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& input,
    const OperandDescriptor& weight,
    const OperandDescriptor& recurrent_weight,
    const OperandDescriptor& hidden_state,
    uint32_t hidden_size,
    const GruCellAttributes& attributes) {
  const std::string& label = attributes.label;
  if (hidden_size <= 0) {
    return base::unexpected(
        ErrorWithLabel(label, "The hidden size must be greater than 0."));
  }

  // Validate the input operand.
  if (!context_properties.data_type_limits.gru_cell_input.Supports(input)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedInputArgumentError(
                   input, context_properties.data_type_limits.gru_cell_input)));
  }

  const uint32_t batch_size = input.shape()[0];
  const uint32_t input_size = input.shape()[1];
  auto checked_three_times_hidden_size = base::MakeCheckedNum(hidden_size) * 3;
  uint32_t three_times_hidden_size;
  if (!checked_three_times_hidden_size.AssignIfValid(
          &three_times_hidden_size)) {
    return base::unexpected(
        ErrorWithLabel(label, "The hidden size is too large."));
  }

  // Validate the weight operand.
  if (!context_properties.data_type_limits.gru_cell_input.Supports(weight)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedArgumentError(
                   kWeightParam, weight,
                   context_properties.data_type_limits.gru_cell_input)));
  }
  std::array<uint32_t, 2> expected_weight_shape = {three_times_hidden_size,
                                                   input_size};
  RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
      weight, kWeightParam, expected_weight_shape, input.data_type(), label));

  // Validate the recurrent weight operand.
  if (!context_properties.data_type_limits.gru_cell_input.Supports(
          recurrent_weight)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedArgumentError(
                   kRecurrentWeightParam, recurrent_weight,
                   context_properties.data_type_limits.gru_cell_input)));
  }
  std::array<uint32_t, 2> expected_recurrent_weight_shape = {
      three_times_hidden_size, hidden_size};
  RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
      recurrent_weight, kRecurrentWeightParam, expected_recurrent_weight_shape,
      input.data_type(), label));

  // Validate the hidden state operand.
  if (!context_properties.data_type_limits.gru_cell_input.Supports(
          hidden_state)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedArgumentError(
                   kHiddenStateParam, hidden_state,
                   context_properties.data_type_limits.gru_cell_input)));
  }
  std::array<uint32_t, 2> expected_hidden_state_shape = {batch_size,
                                                         hidden_size};
  RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
      hidden_state, kHiddenStateParam, expected_hidden_state_shape,
      input.data_type(), label));

  // Validate the bias operand.
  if (attributes.bias) {
    if (!context_properties.data_type_limits.gru_cell_bias.Supports(
            attributes.bias.value())) {
      return base::unexpected(ErrorWithLabel(
          label, NotSupportedArgumentError(
                     kBiasParam, attributes.bias.value(),
                     context_properties.data_type_limits.gru_cell_bias)));
    }
    std::array<uint32_t, 1> expected_bias_shape = {three_times_hidden_size};
    RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
        *attributes.bias, kBiasParam, expected_bias_shape, input.data_type(),
        label));
  }

  // Validate the recurrent bias operand.
  if (attributes.recurrent_bias) {
    if (!context_properties.data_type_limits.gru_cell_bias.Supports(
            attributes.recurrent_bias.value())) {
      return base::unexpected(ErrorWithLabel(
          label, NotSupportedArgumentError(
                     kRecurrentBiasParam, attributes.recurrent_bias.value(),
                     context_properties.data_type_limits.gru_cell_bias)));
    }
    std::array<uint32_t, 1> expected_recurrent_bias_shape = {
        three_times_hidden_size};
    RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
        *attributes.recurrent_bias, kRecurrentBiasParam,
        expected_recurrent_bias_shape, input.data_type(), label));
  }

  if (attributes.activation_count != 2) {
    return base::unexpected(
        ErrorWithLabel(label, "The number of activations must be 2."));
  }

  std::array<uint32_t, 2> output_shape{batch_size, hidden_size};
  return OperandDescriptor::Create(context_properties, input.data_type(),
                                   output_shape, label);
}

base::expected<OperandDescriptor, std::string>
ValidateInstanceNormalizationAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& input,
    const InstanceNormalizationAttributes& attributes) {
  const std::string& label = attributes.label;
  // Validate the input operand.
  if (!context_properties.data_type_limits.instance_normalization_input
           .Supports(input)) {
    return base::unexpected(ErrorWithLabel(
        label,
        NotSupportedInputArgumentError(
            input,
            context_properties.data_type_limits.instance_normalization_input)));
  }

  uint32_t axis;
  switch (attributes.layout) {
    case InputOperandLayout::kNchw:
      axis = 1;
      break;
    case InputOperandLayout::kNhwc:
      axis = 3;
      break;
  }

  // Validate scale operand.
  if (attributes.scale.has_value()) {
    if (!context_properties.data_type_limits.instance_normalization_scale
             .Supports(attributes.scale.value())) {
      return base::unexpected(ErrorWithLabel(
          label,
          NotSupportedArgumentError(kScaleParam, attributes.scale.value(),
                                    context_properties.data_type_limits
                                        .instance_normalization_scale)));
    }
    RETURN_IF_ERROR(ValidateNormalizationOperandIsCompatibleWithInput(
        attributes.scale.value(), input.data_type(), input.shape()[axis], label,
        kScaleParam));
  }

  // Validate the bias operand.
  if (attributes.bias.has_value()) {
    if (!context_properties.data_type_limits.instance_normalization_scale
             .Supports(attributes.bias.value())) {
      return base::unexpected(ErrorWithLabel(
          label, NotSupportedArgumentError(kBiasParam, attributes.bias.value(),
                                           context_properties.data_type_limits
                                               .instance_normalization_scale)));
    }
    RETURN_IF_ERROR(ValidateNormalizationOperandIsCompatibleWithInput(
        attributes.bias.value(), input.data_type(), input.shape()[axis], label,
        kBiasParam));
  }

  return input;
}

base::expected<OperandDescriptor, std::string>
ValidateLayerNormalizationAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& input,
    base::span<const uint32_t> axes,
    const LayerNormalizationAttributes& attributes) {
  const std::string& label = attributes.label;
  // Validate the input operand.
  if (!context_properties.data_type_limits.layer_normalization_input.Supports(
          input)) {
    return base::unexpected(ErrorWithLabel(
        label,
        NotSupportedInputArgumentError(
            input,
            context_properties.data_type_limits.layer_normalization_input)));
  }

  // Ensure that the axes are all less than the input rank and have no
  // duplication.
  RETURN_IF_ERROR(ValidateAxes(axes, input.Rank(), label));

  const std::vector<uint32_t>& input_dimensions = input.shape();

  // The dimensions for layerNormalization to reduce along.
  std::vector<uint32_t> reduction_dimensions;
  reduction_dimensions.reserve(axes.size());
  std::ranges::transform(
      axes, std::back_inserter(reduction_dimensions),
      [&input_dimensions](uint32_t axis) { return input_dimensions[axis]; });

  // Validate the scale operand.
  if (attributes.scale.has_value()) {
    if (attributes.scale->data_type() != input.data_type()) {
      return base::unexpected(ErrorWithLabel(
          label,
          "For scale operand: the data type doesn't match the input data "
          "type."));
    }
    if (attributes.scale->shape() != reduction_dimensions) {
      return base::unexpected(ErrorWithLabel(
          label,
          "For scale operand: the shape doesn't match the axis dimensions of "
          "the input."));
    }
  }

  // Validate the bias operand.
  if (attributes.bias.has_value()) {
    if (attributes.bias->data_type() != input.data_type()) {
      return base::unexpected(
          ErrorWithLabel(label,
                         "For bias operand: the data type doesn't match the "
                         "input data type."));
    }
    if (attributes.bias->shape() != reduction_dimensions) {
      return base::unexpected(ErrorWithLabel(
          label,
          "For bias operand: the shape doesn't match the axis dimensions of "
          "the input."));
    }
  }

  return input;
}

base::expected<std::vector<OperandDescriptor>, std::string>
ValidateLstmAndInferOutput(const ContextProperties& context_properties,
                           const OperandDescriptor& input,
                           const OperandDescriptor& weight,
                           const OperandDescriptor& recurrent_weight,
                           const uint32_t steps,
                           const uint32_t hidden_size,
                           const LstmAttributes& attributes) {
  const std::string& label = attributes.label;
  if (steps <= 0) {
    return base::unexpected(
        ErrorWithLabel(label, "The steps must be greater than 0."));
  }
  if (hidden_size <= 0) {
    return base::unexpected(
        ErrorWithLabel(label, "The hidden size must be greater than 0."));
  }

  uint32_t four_times_hidden_size;
  auto checked_four_times_hidden_size = base::MakeCheckedNum(hidden_size) * 4;
  if (!checked_four_times_hidden_size.AssignIfValid(&four_times_hidden_size)) {
    return base::unexpected(
        ErrorWithLabel(label, "The hidden size is too large."));
  }

  // Validate the input operand.
  if (!context_properties.data_type_limits.lstm_input.Supports(input)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedInputArgumentError(
                   input, context_properties.data_type_limits.lstm_input)));
  }

  const auto& input_dimensions = input.shape();
  if (input_dimensions[0] != steps) {
    return base::unexpected(ErrorWithLabel(
        label, "The input dimensions[0] must be equal to the steps."));
  }

  const uint32_t batch_size = input_dimensions[1];
  const uint32_t input_size = input_dimensions[2];
  const uint32_t direction_count =
      attributes.direction == RecurrentNetworkDirection::kBoth ? 2 : 1;

  // Validate the weight operand.
  if (!context_properties.data_type_limits.lstm_input.Supports(weight)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedArgumentError(
                   kWeightParam, weight,
                   context_properties.data_type_limits.lstm_input)));
  }
  uint32_t expected_weight_shape[3] = {direction_count, four_times_hidden_size,
                                       input_size};
  RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
      weight, kWeightParam, expected_weight_shape, input.data_type(), label));

  // Validate the recurrent weight operand.
  if (!context_properties.data_type_limits.lstm_input.Supports(
          recurrent_weight)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedArgumentError(
                   kRecurrentWeightParam, recurrent_weight,
                   context_properties.data_type_limits.lstm_input)));
  }
  uint32_t expected_recurrent_weight_shape[3] = {
      direction_count, four_times_hidden_size, hidden_size};
  RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
      recurrent_weight, kRecurrentWeightParam, expected_recurrent_weight_shape,
      input.data_type(), label));

  // Validate the bias operand.
  if (attributes.bias) {
    if (!context_properties.data_type_limits.lstm_bias.Supports(
            attributes.bias.value())) {
      return base::unexpected(ErrorWithLabel(
          label, NotSupportedArgumentError(
                     kBiasParam, attributes.bias.value(),
                     context_properties.data_type_limits.lstm_bias)));
    }
    uint32_t expected_bias_shape[2] = {direction_count, four_times_hidden_size};
    RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
        attributes.bias.value(), kBiasParam, expected_bias_shape,
        input.data_type(), label));
  }

  // Validate the recurrent bias operand.
  if (attributes.recurrent_bias) {
    if (!context_properties.data_type_limits.lstm_bias.Supports(
            attributes.recurrent_bias.value())) {
      return base::unexpected(ErrorWithLabel(
          label, NotSupportedArgumentError(
                     kRecurrentBiasParam, attributes.recurrent_bias.value(),
                     context_properties.data_type_limits.lstm_bias)));
    }
    uint32_t expected_recurrent_bias_shape[2] = {direction_count,
                                                 four_times_hidden_size};
    RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
        attributes.recurrent_bias.value(), kRecurrentBiasParam,
        expected_recurrent_bias_shape, input.data_type(), label));
  }

  // Validate the peephole weight operand.
  if (attributes.peephole_weight) {
    if (!context_properties.data_type_limits.lstm_bias.Supports(
            attributes.peephole_weight.value())) {
      return base::unexpected(ErrorWithLabel(
          label, NotSupportedArgumentError(
                     kPeepholeWeightParam, attributes.peephole_weight.value(),
                     context_properties.data_type_limits.lstm_bias)));
    }
    // Here `3 * hidden_size` will not overflow because `4 * hidden_size` has
    // already been checked.
    uint32_t expected_peephole_weight_shape[2] = {direction_count,
                                                  3 * hidden_size};
    RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
        attributes.peephole_weight.value(), kPeepholeWeightParam,
        expected_peephole_weight_shape, input.data_type(), label));
  }

  // Validate the initial hidden state operand.
  if (attributes.initial_hidden_state) {
    if (!context_properties.data_type_limits.lstm_input.Supports(
            attributes.initial_hidden_state.value())) {
      return base::unexpected(ErrorWithLabel(
          label,
          NotSupportedArgumentError(
              kInitialHiddenStateParam, attributes.initial_hidden_state.value(),
              context_properties.data_type_limits.lstm_input)));
    }
    uint32_t expected_initial_hidden_state_shape[3] = {direction_count,
                                                       batch_size, hidden_size};
    RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
        attributes.initial_hidden_state.value(), kInitialHiddenStateParam,
        expected_initial_hidden_state_shape, input.data_type(), label));
  }

  // Validate the initial cell state operand.
  if (attributes.initial_cell_state) {
    if (!context_properties.data_type_limits.lstm_input.Supports(
            attributes.initial_cell_state.value())) {
      return base::unexpected(ErrorWithLabel(
          label,
          NotSupportedArgumentError(
              kInitialCellStateParam, attributes.initial_cell_state.value(),
              context_properties.data_type_limits.lstm_input)));
    }
    uint32_t expected_initial_cell_state_shape[3] = {direction_count,
                                                     batch_size, hidden_size};
    RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
        attributes.initial_cell_state.value(), kInitialCellStateParam,
        expected_initial_cell_state_shape, input.data_type(), label));
  }

  if (attributes.activation_count != 3) {
    return base::unexpected(ErrorWithLabel(
        label, "The activations should be a sequence of length 3."));
  }

  std::vector<OperandDescriptor> outputs;
  ASSIGN_OR_RETURN(
      OperandDescriptor output,
      OperandDescriptor::Create(
          context_properties, input.data_type(),
          std::array{direction_count, batch_size, hidden_size}, label));
  outputs.push_back(output);
  outputs.push_back(std::move(output));
  if (attributes.return_sequence) {
    ASSIGN_OR_RETURN(
        OperandDescriptor return_sequence_output,
        OperandDescriptor::Create(
            context_properties, input.data_type(),
            std::array{steps, direction_count, batch_size, hidden_size},
            label));
    outputs.push_back(std::move(return_sequence_output));
  }

  return outputs;
}

base::expected<std::vector<OperandDescriptor>, std::string>
ValidateLstmCellAndInferOutput(const ContextProperties& context_properties,
                               const OperandDescriptor& input,
                               const OperandDescriptor& weight,
                               const OperandDescriptor& recurrent_weight,
                               const OperandDescriptor& hidden_state,
                               const OperandDescriptor& cell_state,
                               const uint32_t hidden_size,
                               const LstmCellAttributes& attributes) {
  const std::string& label = attributes.label;
  if (hidden_size <= 0) {
    return base::unexpected(
        ErrorWithLabel(label, "The hidden size must be greater than 0."));
  }

  uint32_t four_times_hidden_size;
  auto checked_four_times_hidden_size = base::MakeCheckedNum(hidden_size) * 4;
  if (!checked_four_times_hidden_size.AssignIfValid(&four_times_hidden_size)) {
    return base::unexpected(
        ErrorWithLabel(label, "The hidden size is too large."));
  }

  // Validate the input operand.
  if (!context_properties.data_type_limits.lstm_cell_input.Supports(input)) {
    return base::unexpected(ErrorWithLabel(
        label,
        NotSupportedInputArgumentError(
            input, context_properties.data_type_limits.lstm_cell_input)));
  }

  const uint32_t batch_size = input.shape()[0];
  const uint32_t input_size = input.shape()[1];

  // Validate the weight operand.
  if (!context_properties.data_type_limits.lstm_cell_input.Supports(weight)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedArgumentError(
                   kWeightParam, weight,
                   context_properties.data_type_limits.lstm_cell_input)));
  }
  std::array<uint32_t, 2> expected_weight_shape = {four_times_hidden_size,
                                                   input_size};
  RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
      weight, kWeightParam, expected_weight_shape, input.data_type(), label));

  // Validate the hidden state operand.
  if (!context_properties.data_type_limits.lstm_cell_input.Supports(
          hidden_state)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedArgumentError(
                   kHiddenStateParam, hidden_state,
                   context_properties.data_type_limits.lstm_cell_input)));
  }
  std::array<uint32_t, 2> expected_hidden_state_shape = {batch_size,
                                                         hidden_size};
  RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
      hidden_state, kHiddenStateParam, expected_hidden_state_shape,
      input.data_type(), label));

  // Validate the cell state operand.
  if (!context_properties.data_type_limits.lstm_cell_input.Supports(
          cell_state)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedArgumentError(
                   kCellStateParam, cell_state,
                   context_properties.data_type_limits.lstm_cell_input)));
  }
  std::array<uint32_t, 2> expected_cell_state_shape = {batch_size, hidden_size};
  RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(cell_state, kCellStateParam,
                                                  expected_cell_state_shape,
                                                  input.data_type(), label));

  // Validate the recurrent weight operand.
  if (!context_properties.data_type_limits.lstm_cell_input.Supports(
          recurrent_weight)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedArgumentError(
                   kRecurrentWeightParam, recurrent_weight,
                   context_properties.data_type_limits.lstm_cell_input)));
  }
  std::array<uint32_t, 2> expected_recurrent_weight_shape = {
      four_times_hidden_size, hidden_size};
  RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
      recurrent_weight, kRecurrentWeightParam, expected_recurrent_weight_shape,
      input.data_type(), label));

  // Validate the bias operand.
  if (attributes.bias) {
    if (!context_properties.data_type_limits.lstm_cell_bias.Supports(
            attributes.bias.value())) {
      return base::unexpected(ErrorWithLabel(
          label, NotSupportedArgumentError(
                     kBiasParam, attributes.bias.value(),
                     context_properties.data_type_limits.lstm_cell_bias)));
    }
    std::array<uint32_t, 1> expected_bias_shape = {four_times_hidden_size};
    RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
        attributes.bias.value(), kBiasParam, expected_bias_shape,
        input.data_type(), label));
  }

  // Validate the recurrent bias operand.
  if (attributes.recurrent_bias) {
    if (!context_properties.data_type_limits.lstm_cell_bias.Supports(
            attributes.recurrent_bias.value())) {
      return base::unexpected(ErrorWithLabel(
          label, NotSupportedArgumentError(
                     kRecurrentBiasParam, attributes.recurrent_bias.value(),
                     context_properties.data_type_limits.lstm_cell_bias)));
    }
    std::array<uint32_t, 1> expected_recurrent_bias_shape = {
        four_times_hidden_size};
    RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
        attributes.recurrent_bias.value(), kRecurrentBiasParam,
        expected_recurrent_bias_shape, input.data_type(), label));
  }

  // Validate the peephole weight operand.
  if (attributes.peephole_weight) {
    if (!context_properties.data_type_limits.lstm_cell_bias.Supports(
            attributes.peephole_weight.value())) {
      return base::unexpected(ErrorWithLabel(
          label, NotSupportedArgumentError(
                     kPeepholeWeightParam, attributes.peephole_weight.value(),
                     context_properties.data_type_limits.lstm_cell_bias)));
    }
    // Here `3 * hidden_size` will not overflow because `4 * hidden_size` has
    // already been checked.
    std::array<uint32_t, 1> expected_peephole_weight_shape = {3 * hidden_size};
    RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
        attributes.peephole_weight.value(), kPeepholeWeightParam,
        expected_peephole_weight_shape, input.data_type(), label));
  }

  if (attributes.activation_count != 3) {
    return base::unexpected(ErrorWithLabel(
        label, "The activations should be a sequence of length 3."));
  }

  std::vector<OperandDescriptor> outputs;
  outputs.reserve(2);

  ASSIGN_OR_RETURN(
      OperandDescriptor output,
      OperandDescriptor::Create(context_properties, input.data_type(),
                                std::array{batch_size, hidden_size}, label));
  outputs.push_back(output);
  outputs.push_back(std::move(output));

  return outputs;
}

base::expected<OperandDescriptor, std::string> ValidateMatmulAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& a,
    const OperandDescriptor& b,
    std::string_view label) {
  if (!context_properties.data_type_limits.matmul_input.Supports(a)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedArgumentError(
                   "a", a, context_properties.data_type_limits.matmul_input)));
  }

  if (!context_properties.data_type_limits.matmul_input.Supports(b)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedArgumentError(
                   "b", b, context_properties.data_type_limits.matmul_input)));
  }

  if (a.data_type() != b.data_type()) {
    return base::unexpected(ErrorWithLabel(
        label, "The data types of first two inputs don't match."));
  }

  std::vector<uint32_t> a_dimensions = a.shape();
  CHECK_GE(a_dimensions.size(), 2u);
  std::vector<uint32_t> b_dimensions = b.shape();
  CHECK_GE(b_dimensions.size(), 2u);

  // The number of columns in the first matrix must be equal to the number of
  // rows in the second matrix.
  const uint32_t a_cols = a_dimensions[a_dimensions.size() - 1];
  const uint32_t a_rows = a_dimensions[a_dimensions.size() - 2];
  const uint32_t b_cols = b_dimensions[b_dimensions.size() - 1];
  const uint32_t b_rows = b_dimensions[b_dimensions.size() - 2];
  if (a_cols != b_rows) {
    return base::unexpected(ErrorWithLabel(
        label,
        base::StringPrintf(
            "The number of columns (%u) in the first matrix isn't equal to "
            "the number of rows (%u) in the second matrix.",
            a_cols, b_rows)));
  }

  size_t output_rank = std::max(a_dimensions.size(), b_dimensions.size());
  std::vector<uint32_t> output_dimensions;
  // Figure out the output shape by broadcasting all the dimensions except the
  // last two. The output is 2-D tensor of shape [M, N].
  if (a.Rank() > 2 && b.Rank() > 2) {
    std::vector<uint32_t> sliced_a_dimensions(a_dimensions.begin(),
                                              a_dimensions.end() - 2);
    std::vector<uint32_t> sliced_b_dimensions(b_dimensions.begin(),
                                              b_dimensions.end() - 2);
    std::optional<std::vector<uint32_t>> optional_output_dimensions =
        BroadcastShapes(sliced_a_dimensions, sliced_b_dimensions, true);
    if (!optional_output_dimensions) {
      return base::unexpected(ErrorWithLabel(
          label, "The matmul input shapes are not broadcastable."));
    }
    output_dimensions = *optional_output_dimensions;
    output_dimensions.push_back(a_rows);
    output_dimensions.push_back(b_cols);
  } else if (a.Rank() == 2 && b.Rank() == 2) {
    output_dimensions.push_back(a_rows);
    output_dimensions.push_back(b_cols);
  } else {
    output_dimensions =
        a_dimensions.size() > b_dimensions.size() ? a_dimensions : b_dimensions;
    output_dimensions[output_rank - 2] = a_rows;
    output_dimensions[output_rank - 1] = b_cols;
  }
  CHECK_EQ(output_rank, output_dimensions.size());

  return OperandDescriptor::Create(context_properties, a.data_type(),
                                   output_dimensions, label);
}

base::expected<OperandDescriptor, std::string> ValidatePadAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& input,
    base::span<const uint32_t> beginning_padding,
    base::span<const uint32_t> ending_padding,
    std::string_view label) {
  if (!context_properties.data_type_limits.pad_input.Supports(input)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedInputArgumentError(
                   input, context_properties.data_type_limits.pad_input)));
  }

  // Validate the beginning_padding and ending_padding.
  if (beginning_padding.size() != input.Rank()) {
    return base::unexpected(
        ErrorWithLabel(label,
                       "The length of beginningPadding must be "
                       "equal to the rank of the input tensor."));
  }
  if (ending_padding.size() != input.Rank()) {
    return base::unexpected(
        ErrorWithLabel(label,
                       "The length of endingPadding must be "
                       "equal to the rank of the input tensor."));
  }

  // Infer the output.
  // Each dimension of the output tensor can be calculated as follow:
  // input_size = input_shape[i];
  // output_size = beginning_padding + input_size + ending_padding.
  std::vector<uint32_t> output_shape(input.Rank());
  for (size_t i = 0; i < input.Rank(); ++i) {
    auto checked_output_size =
        base::MakeCheckedNum<uint32_t>(input.shape()[i]) +
        beginning_padding[i] + ending_padding[i];
    if (!checked_output_size.AssignIfValid(&output_shape[i])) {
      return base::unexpected(ErrorWithLabel(
          label, base::StringPrintf(
                     "The padding of dimension (%zu) is too large.", i)));
    }
  }

  return OperandDescriptor::Create(context_properties, input.data_type(),
                                   output_shape, label);
}

base::expected<OperandDescriptor, std::string> ValidatePool2dAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& input,
    const Pool2dAttributes& attributes,
    Pool2dKind kind) {
  const std::string& label = attributes.label;
  // Validate input operand and set its sizes.
  const SupportedTensors& tensor_constraint = [&](Pool2dKind kind) {
    switch (kind) {
      case Pool2dKind::kAverage:
        return context_properties.data_type_limits.average_pool2d_input;
      case Pool2dKind::kL2:
        return context_properties.data_type_limits.l2_pool2d_input;
      case Pool2dKind::kMax:
        return context_properties.data_type_limits.max_pool2d_input;
    }
  }(kind);

  if (!tensor_constraint.Supports(input)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedInputArgumentError(input, tensor_constraint)));
  }

  const std::vector<uint32_t>& input_shape = input.shape();
  CHECK_EQ(input_shape.size(), 4u);
  // The layout option specifies the layout format of the input tensor.
  uint32_t input_batches, input_channels, input_height, input_width;
  switch (attributes.layout) {
    case InputOperandLayout::kNchw:
      // "nchw": [batches, channels, height, width]
      input_batches = input_shape[0];
      input_channels = input_shape[1];
      input_height = input_shape[2];
      input_width = input_shape[3];
      break;
    case InputOperandLayout::kNhwc:
      // "nhwc": [batches, height, width, channels]
      input_batches = input_shape[0];
      input_height = input_shape[1];
      input_width = input_shape[2];
      input_channels = input_shape[3];
      break;
  }

  // Validate windowDimensions and get its values. If not present, the window
  // dimensions are assumed to be the height and width dimensions of the input
  // shape.
  uint32_t window_height = input_height;
  uint32_t window_width = input_width;
  if (attributes.window_dimensions) {
    if (attributes.window_dimensions->height == 0 ||
        attributes.window_dimensions->width == 0) {
      return base::unexpected(ErrorWithLabel(
          label, "All window dimensions should be greater than 0."));
    }
    window_height = attributes.window_dimensions->height;
    window_width = attributes.window_dimensions->width;
  }

  // Reuse ValidateAndCalculateConv2dOutputSizes to calculate pool2d output
  // sizes.
  ASSIGN_OR_RETURN(
      Size2d<double> output_sizes,
      ValidateAndCalculateConv2dOutputSizes(
          input_height, input_width, window_height, window_width,
          attributes.padding, attributes.strides, attributes.dilations, label));

  const uint32_t floor_output_height =
      base::ClampFloor<uint32_t>(output_sizes.height);
  const uint32_t ceil_output_height =
      base::ClampCeil<uint32_t>(output_sizes.height);
  const uint32_t floor_output_width =
      base::ClampFloor<uint32_t>(output_sizes.width);
  const uint32_t ceil_output_width =
      base::ClampCeil<uint32_t>(output_sizes.width);

  uint32_t output_height, output_width;
  if (attributes.output_sizes) {
    auto& output_size = attributes.output_sizes.value();
    if (output_size.height == 0 || output_size.width == 0) {
      return base::unexpected(
          ErrorWithLabel(label, "All output sizes should be greater than 0."));
    }
    uint32_t user_output_height = output_size.height;
    uint32_t user_output_width = output_size.width;

    // Check whether the user supplied output sizes is either floor or ceil
    // rounding of the calculated output sizes. The backend implementation
    // should check whether the indicated rounding type is supported.
    if ((user_output_height == floor_output_height &&
         user_output_width == floor_output_width) ||
        (user_output_height == ceil_output_height &&
         user_output_width == ceil_output_width)) {
      output_height = user_output_height;
      output_width = user_output_width;
    } else {
      return base::unexpected(ErrorWithLabel(
          label,
          (floor_output_height == ceil_output_height &&
           floor_output_width == ceil_output_width)
              ? base::StringPrintf("The output sizes should be [%u, %u].",
                                   floor_output_height, floor_output_width)
              : base::StringPrintf(
                    "The output sizes should be either [%u, %u] or [%u, %u].",
                    floor_output_height, floor_output_width, ceil_output_height,
                    ceil_output_width)));
    }
  } else {
    switch (attributes.rounding_type) {
      case RoundingType::kFloor:
        output_height = floor_output_height;
        output_width = floor_output_width;
        break;
      case RoundingType::kCeil:
        output_height = ceil_output_height;
        output_width = ceil_output_width;
        break;
    }
  }
  // The layout option specifies the layout format of the output tensor.
  std::vector<uint32_t> output_shape;
  switch (attributes.layout) {
    case InputOperandLayout::kNchw:
      // "nchw": [batches, channels, height, width]
      output_shape = {input_batches, input_channels, output_height,
                      output_width};
      break;
    case InputOperandLayout::kNhwc:
      // "nhwc": [batches, height, width, channels]
      output_shape = {input_batches, output_height, output_width,
                      input_channels};
      break;
  }

  return OperandDescriptor::Create(context_properties, input.data_type(),
                                   output_shape, label);
}

base::expected<OperandDescriptor, std::string> ValidatePreluAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& input,
    const OperandDescriptor& slope,
    std::string_view label) {
  if (!context_properties.data_type_limits.prelu_input.Supports(input)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedInputArgumentError(
                   input, context_properties.data_type_limits.prelu_input)));
  }

  if (!context_properties.data_type_limits.prelu_input.Supports(slope)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedArgumentError(
                   kSlopeParam, slope,
                   context_properties.data_type_limits.prelu_input)));
  }

  if (input.data_type() != slope.data_type()) {
    return base::unexpected(ErrorWithLabel(
        label, "The data type of slope doesn't match the data type of input."));
  }
  // TODO(crbug.com/387892103): Use bidirectional broadcasting.
  // BroadcastShape unidirectionally broadcasts slope.dimensions to
  // input.dimensions.
  if (!BroadcastShapes(slope.shape(), input.shape(), /*bidirectional=*/false)) {
    return base::unexpected(ErrorWithLabel(
        label,
        "The shape of slope is not broadcastable to the shape of input."));
  }

  return input;
}

base::expected<OperandDescriptor, std::string> ValidateReduceAndInferOutput(
    const ContextProperties& context_properties,
    ReduceKind kind,
    const OperandDescriptor& input,
    std::string_view label,
    base::span<const uint32_t> axes,
    bool keep_dimensions) {
  const SupportedTensors& tensor_constraint = [&](ReduceKind kind) {
    switch (kind) {
      case ReduceKind::kL1:
        return context_properties.data_type_limits.reduce_l1_input;
      case ReduceKind::kL2:
        return context_properties.data_type_limits.reduce_l2_input;
      case ReduceKind::kLogSum:
        return context_properties.data_type_limits.reduce_log_sum_input;
      case ReduceKind::kLogSumExp:
        return context_properties.data_type_limits.reduce_log_sum_exp_input;
      case ReduceKind::kMax:
        return context_properties.data_type_limits.reduce_max_input;
      case ReduceKind::kMean:
        return context_properties.data_type_limits.reduce_mean_input;
      case ReduceKind::kMin:
        return context_properties.data_type_limits.reduce_min_input;
      case ReduceKind::kProduct:
        return context_properties.data_type_limits.reduce_product_input;
      case ReduceKind::kSum:
        return context_properties.data_type_limits.reduce_sum_input;
      case ReduceKind::kSumSquare:
        return context_properties.data_type_limits.reduce_sum_square_input;
    }
  }(kind);

  if (!tensor_constraint.Supports(input)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedInputArgumentError(input, tensor_constraint)));
  }

  ASSIGN_OR_RETURN(std::vector<uint32_t> output_shape,
                   ValidateReduceAxesAndInferOutput(input.shape(), axes,
                                                    keep_dimensions, label));

  return OperandDescriptor::Create(context_properties, input.data_type(),
                                   output_shape, label);
}

// The current WebNN spec doesn't define the calculation formula of the output
// size for resample2d. An issue has been filed to track it -
// https://github.com/webmachinelearning/webnn/issues/360.
base::expected<uint32_t, std::string> CalculateResample2dOutputSize(
    const uint32_t input_size,
    const float scale,
    std::string_view label) {
  // Calculate the output size in double precision floating point number that
  // ensures values of type uint32_t can be exactly represented.
  // https://en.wikipedia.org/wiki/Double-precision_floating-point_format#Precision_limitations_on_integer_values
  auto checked_output_size = base::MakeCheckedNum<double>(input_size) * scale;

  // Check if the value is valid for rounding to uint32_t type.
  if (!checked_output_size.IsValid<uint32_t>()) {
    return base::unexpected(ErrorWithLabel(label, "The scale is too large."));
  }
  const uint32_t output_size = base::ClampFloor<uint32_t>(
      static_cast<double>(checked_output_size.ValueOrDie()));
  if (output_size == 0) {
    return base::unexpected(ErrorWithLabel(label, "The scale is too small."));
  }
  return output_size;
}

base::expected<OperandDescriptor, std::string> ValidateResample2dAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& input,
    const std::variant<base::span<const float>, base::span<const uint32_t>>&
        scales_or_sizes,
    base::span<const uint32_t> axes,
    std::string_view label) {
  if (!context_properties.data_type_limits.resample2d_input.Supports(input)) {
    return base::unexpected(ErrorWithLabel(
        label,
        NotSupportedInputArgumentError(
            input, context_properties.data_type_limits.resample2d_input)));
  }

  if (axes.size() != 2) {
    return base::unexpected(
        ErrorWithLabel(label, "The length of axes should be 2."));
  }
  RETURN_IF_ERROR(ValidateAxes(axes, input.Rank(), label));

  // Validate scales or sizes and infer the output.
  std::vector<uint32_t> output_shape(input.shape());
  if (std::holds_alternative<base::span<const float>>(scales_or_sizes)) {
    const auto& scales = std::get<base::span<const float>>(scales_or_sizes);
    if (scales.size() != 2) {
      return base::unexpected(
          ErrorWithLabel(label, "The length of scales should be 2."));
    }
    if (scales[0] <= 0 || scales[1] <= 0) {
      return base::unexpected(
          ErrorWithLabel(label, "All scales should be greater than 0."));
    }

    auto output_first_axis =
        CalculateResample2dOutputSize(input.shape()[axes[0]], scales[0], label);
    if (!output_first_axis.has_value()) {
      return base::unexpected(ErrorWithLabel(
          label, "Failed to calculate the output shape for first axis : " +
                     output_first_axis.error()));
    }
    output_shape[axes[0]] = output_first_axis.value();

    auto output_second_axis =
        CalculateResample2dOutputSize(input.shape()[axes[1]], scales[1], label);
    if (!output_second_axis.has_value()) {
      return base::unexpected(ErrorWithLabel(
          label, "Failed to calculate the output shape for second axis: " +
                     output_second_axis.error()));
    }
    output_shape[axes[1]] = output_second_axis.value();
  } else if (std::holds_alternative<base::span<const uint32_t>>(
                 scales_or_sizes)) {
    const auto& sizes = std::get<base::span<const uint32_t>>(scales_or_sizes);
    if (sizes.size() != 2) {
      return base::unexpected(
          ErrorWithLabel(label, "The length of sizes should be 2."));
    }
    if (sizes[0] == 0 || sizes[1] == 0) {
      return base::unexpected(
          ErrorWithLabel(label, "All sizes should be greater than 0."));
    }

    output_shape[axes[0]] = sizes[0];
    output_shape[axes[1]] = sizes[1];
  } else {
    NOTREACHED();
  }

  return OperandDescriptor::Create(context_properties, input.data_type(),
                                   output_shape, label);
}

base::expected<OperandDescriptor, std::string> ValidateReverseAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& input,
    base::span<const uint32_t> axes,
    std::string_view label) {
  RETURN_IF_ERROR(ValidateAxes(axes, input.Rank(), label));

  if (!context_properties.data_type_limits.reverse_input.Supports(input)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedInputArgumentError(
                   input, context_properties.data_type_limits.reverse_input)));
  }

  return input;
}

base::expected<OperandDescriptor, std::string>
ValidateScatterElementsAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& input,
    const OperandDescriptor& indices,
    const OperandDescriptor& updates,
    const uint32_t axis,
    std::string_view label) {
  if (!context_properties.data_type_limits.scatter_elements_input.Supports(
          input)) {
    return base::unexpected(ErrorWithLabel(
        label,
        NotSupportedInputArgumentError(
            input,
            context_properties.data_type_limits.scatter_elements_input)));
  }

  if (!context_properties.data_type_limits.scatter_elements_indices.Supports(
          indices)) {
    return base::unexpected(ErrorWithLabel(
        label,
        NotSupportedArgumentError(
            kIndicesParam, indices,
            context_properties.data_type_limits.scatter_elements_indices)));
  }

  if (input.data_type() != updates.data_type()) {
    return base::unexpected(
        ErrorWithLabel(label,
                       "The updates tensor data type should be the same as "
                       "input data type."));
  }

  if (input.Rank() <= axis) {
    return base::unexpected(ErrorWithLabel(
        label,
        "The axis must be in the range [0, N-1] where N is the rank of input "
        "tensor."));
  }

  if (indices.Rank() != input.Rank()) {
    return base::unexpected(ErrorWithLabel(
        label, "The indices and input tensors should have the same rank."));
  }

  for (uint32_t i = 0; i < input.Rank(); ++i) {
    if (i == axis) {
      continue;
    }
    if (input.shape()[i] != indices.shape()[i]) {
      return base::unexpected(
          ErrorWithLabel(label,
                         "Except on the axis dimension, the input and indices "
                         "tensor must have the same dimension size."));
    }
  }

  if (indices.shape() != updates.shape()) {
    return base::unexpected(ErrorWithLabel(
        label, "The updates and indices tensors should have the same shape."));
  }

  // The output tensor has the same data type and shape as input's.
  return input;
}

base::expected<OperandDescriptor, std::string> ValidateScatterNDAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& input,
    const OperandDescriptor& indices,
    const OperandDescriptor& updates,
    std::string_view label) {
  if (!context_properties.data_type_limits.scatter_nd_input.Supports(input)) {
    return base::unexpected(ErrorWithLabel(
        label,
        NotSupportedInputArgumentError(
            input, context_properties.data_type_limits.scatter_nd_input)));
  }

  if (!context_properties.data_type_limits.scatter_nd_indices.Supports(
          indices)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedArgumentError(
                   kIndicesParam, indices,
                   context_properties.data_type_limits.scatter_nd_indices)));
  }

  if (!context_properties.data_type_limits.scatter_nd_updates.Supports(
          updates)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedArgumentError(
                   kUpdatesParam, updates,
                   context_properties.data_type_limits.scatter_nd_updates)));
  }

  // Updates tensor's data type should be the same as input's.
  if (input.data_type() != updates.data_type()) {
    return base::unexpected(
        ErrorWithLabel(label,
                       "The updates tensor data type should be the same as "
                       "input data type."));
  }

  const uint32_t indices_last_dim_size = indices.shape()[indices.Rank() - 1];
  if (indices_last_dim_size > input.Rank()) {
    return base::unexpected(ErrorWithLabel(
        label, base::StringPrintf(
                   "The size of the last dimension of indices tensor (%u)"
                   "should not be greater than input rank (%u).",
                   indices_last_dim_size, input.Rank())));
  }

  // Validate `updates.shape` =
  // `indices.shape[:-1]` + `input.shape[indices.shape[-1]:]`, where `+` denotes
  // the concatenation of shapes.
  auto checked_updates_rank = base::MakeCheckedNum<uint32_t>(indices.Rank()) -
                              1 + input.Rank() - indices_last_dim_size;
  if (!checked_updates_rank.IsValid()) {
    return base::unexpected(
        ErrorWithLabel(label, "The expected updates rank is too large."));
  }

  std::vector<uint32_t> expected_updates_shape;
  expected_updates_shape.reserve(checked_updates_rank.ValueOrDie());
  std::ranges::copy(indices.shape().begin(), indices.shape().end() - 1,
                    std::back_inserter(expected_updates_shape));
  std::ranges::copy(input.shape().begin() + indices_last_dim_size,
                    input.shape().end(),
                    std::back_inserter(expected_updates_shape));

  if (expected_updates_shape != updates.shape()) {
    return base::unexpected(
        ErrorWithLabel(label, "The updates tensor shape is invalid."));
  }

  // The output tensor has the same data type and shape as input's.
  return input;
}

base::expected<OperandDescriptor, std::string> ValidateSliceAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& input,
    const SliceAttributes& attributes) {
  const std::string& label = attributes.label;
  const auto input_rank = input.Rank();

  if (!context_properties.data_type_limits.slice_input.Supports(input)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedInputArgumentError(
                   input, context_properties.data_type_limits.slice_input)));
  }

  if (attributes.starts.size() != input_rank) {
    return base::unexpected(ErrorWithLabel(
        label,
        "The length of starts must be equal to the rank of the input tensor."));
  }

  if (attributes.sizes.size() != input_rank) {
    return base::unexpected(ErrorWithLabel(
        label,
        "The length of sizes must be equal to the rank of the input tensor."));
  }

  if (attributes.strides.size() != input_rank) {
    return base::unexpected(
        ErrorWithLabel(label,
                       "The length of strides must be equal to the rank of the "
                       "input tensor."));
  }

  std::vector<uint32_t> output_shape;
  output_shape.reserve(input_rank);

  for (uint32_t i = 0; i < input_rank; ++i) {
    if (attributes.starts[i] >= input.shape()[i]) {
      return base::unexpected(ErrorWithLabel(
          label, base::StringPrintf(
                     "For dimension (%u): the starting index to slice must "
                     "be less than input size (%u).",
                     i, input.shape()[i])));
    }

    // WebNN plans to allow 0 size dimensions and an issue has been filed to
    // track it: https://github.com/webmachinelearning/webnn/issues/391.
    if (attributes.sizes[i] == 0) {
      return base::unexpected(ErrorWithLabel(
          label, base::StringPrintf(
                     "For dimension (%u): the number of elements to slice "
                     "must not be 0.",
                     i)));
    }

    if (attributes.strides[i] < 1) {
      return base::unexpected(ErrorWithLabel(
          label,
          base::StringPrintf(
              "For dimension (%u): the stride (%u) must not be less than 1.", i,
              attributes.strides[i])));
    }

    auto checked_ending_index =
        base::MakeCheckedNum<uint32_t>(attributes.starts[i]) +
        attributes.sizes[i];
    if (!checked_ending_index.IsValid<uint32_t>()) {
      return base::unexpected(ErrorWithLabel(
          label,
          base::StringPrintf(
              "For dimension (%u): the ending index to slice is too large.",
              i)));
    }

    if (checked_ending_index.ValueOrDie() > input.shape()[i]) {
      return base::unexpected(ErrorWithLabel(
          label,
          base::StringPrintf("For dimension (%u): the ending index to slice "
                             "must not be greater than input size (%u).",
                             i, input.shape()[i])));
    }

    uint32_t output_size = attributes.sizes[i] / attributes.strides[i] +
                           (attributes.sizes[i] % attributes.strides[i] != 0);
    output_shape.push_back(output_size);
  }

  return OperandDescriptor::Create(context_properties, input.data_type(),
                                   output_shape, label);
}

base::expected<OperandDescriptor, std::string> ValidateSoftmaxAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& input,
    uint32_t axis,
    std::string_view label) {
  if (!context_properties.data_type_limits.softmax_input.Supports(input)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedInputArgumentError(
                   input, context_properties.data_type_limits.softmax_input)));
  }
  if (axis >= input.Rank()) {
    return base::unexpected(
        ErrorWithLabel(label, "Axis must be a valid dimension."));
  }
  // The output tensor of softmax is the same shape as the input tensor.
  return input;
}

base::expected<std::vector<OperandDescriptor>, std::string>
ValidateSplitAndInferOutput(const ContextProperties& context_properties,
                            const OperandDescriptor& input,
                            const SplitAttribute& attributes) {
  const std::string& label = attributes.label;
  if (!context_properties.data_type_limits.split_input.Supports(input)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedInputArgumentError(
                   input, context_properties.data_type_limits.split_input)));
  }

  if (attributes.axis >= input.Rank()) {
    return base::unexpected(ErrorWithLabel(
        label,
        "The axis must be in the range [0, N-1] where N is the rank of the "
        "input tensor."));
  }

  static_assert(std::variant_size<decltype(attributes.splits)>() == 2,
                "When adding new variants update the branches below.");

  std::vector<OperandDescriptor> outputs;
  if (std::holds_alternative<uint32_t>(attributes.splits)) {
    uint32_t splits = std::get<uint32_t>(attributes.splits);
    if (splits == 0) {
      return base::unexpected(
          ErrorWithLabel(label, "The splits must be greater than zero."));
    }

    if (input.shape()[attributes.axis] % splits != 0) {
      return base::unexpected(
          ErrorWithLabel(label,
                         "The dimension size of the input tensor along "
                         "options.axis must be divisible by splits."));
    }

    outputs.reserve(splits);
    for (uint32_t i = 0; i < splits; ++i) {
      // When splits is of type uint32_t, we create splits number of Operands.
      // Each Operand will have the same new_dimensions shape.
      std::vector<uint32_t> new_dimensions = input.shape();
      new_dimensions[attributes.axis] /= splits;
      auto split_descriptor = OperandDescriptor::Create(
          context_properties, input.data_type(), new_dimensions, label);
      // `split_descriptor` should always be valid, since it's a subset of the
      // input.
      CHECK(split_descriptor.has_value());
      outputs.push_back(*std::move(split_descriptor));
    }
  } else if (std::holds_alternative<base::span<const uint32_t>>(
                 attributes.splits)) {
    const auto& splits =
        std::get<base::span<const uint32_t>>(attributes.splits);
    if (std::ranges::any_of(splits,
                            [](uint32_t split) { return split == 0; })) {
      return base::unexpected(
          ErrorWithLabel(label, "All splits must be greater than zero."));
    }

    base::CheckedNumeric<uint32_t> sum = std::accumulate(
        splits.begin(), splits.end(), base::MakeCheckedNum<uint32_t>(0));
    if (!sum.IsValid() || sum.ValueOrDie() != input.shape()[attributes.axis]) {
      return base::unexpected(ErrorWithLabel(
          label,
          "The sum of all sizes in splits must be equal to the dimension size "
          "of the input tensor specified by options.axis."));
    }

    outputs.reserve(splits.size());
    for (uint32_t split : splits) {
      std::vector<uint32_t> new_dimensions = input.shape();
      new_dimensions[attributes.axis] = split;
      auto split_descriptor = OperandDescriptor::Create(
          context_properties, input.data_type(), new_dimensions, label);
      // `split_descriptor` should always be valid, since it's a subset of the
      // input.
      CHECK(split_descriptor.has_value());
      outputs.push_back(*std::move(split_descriptor));
    }
  } else {
    NOTREACHED();
  }

  return outputs;
}

base::expected<OperandDescriptor, std::string> ValidateTileAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& input,
    base::span<const uint32_t> repetitions,
    std::string_view label) {
  if (!context_properties.data_type_limits.tile_input.Supports(input)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedInputArgumentError(
                   input, context_properties.data_type_limits.tile_input)));
  }

  if (repetitions.size() != input.Rank()) {
    return base::unexpected(ErrorWithLabel(
        label,
        "The number of values in repetitions must be the same as the rank of "
        "the input tensor."));
  }

  std::vector<uint32_t> output_shape(input.Rank());
  for (size_t i = 0; i < input.Rank(); ++i) {
    if (repetitions[i] == 0) {
      return base::unexpected(
          ErrorWithLabel(label, "Any value in repetitions must not be 0."));
    }
    auto tiled_dim =
        base::MakeCheckedNum<uint32_t>(repetitions[i]) * input.shape()[i];
    if (!tiled_dim.AssignIfValid(&output_shape[i])) {
      return base::unexpected(
          ErrorWithLabel(label, "The tiled dimension size is too large."));
    }
  }

  return OperandDescriptor::Create(context_properties, input.data_type(),
                                   output_shape, label);
}

base::expected<OperandDescriptor, std::string> ValidateTransposeAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& input,
    base::span<const uint32_t> permutation,
    std::string_view label) {
  if (!context_properties.data_type_limits.transpose_input.Supports(input)) {
    return base::unexpected(ErrorWithLabel(
        label,
        NotSupportedInputArgumentError(
            input, context_properties.data_type_limits.transpose_input)));
  }

  if (permutation.size() != static_cast<size_t>(input.Rank())) {
    return base::unexpected(ErrorWithLabel(
        label,
        "The number of values in permutation must be the same as the rank of "
        "the input tensor."));
  }
  RETURN_IF_ERROR(ValidateAxes(permutation, input.Rank(), label));

  std::vector<uint32_t> output_shape(input.Rank());
  for (uint32_t i = 0; i < input.Rank(); ++i) {
    output_shape[i] = input.shape()[permutation[i]];
  }
  return OperandDescriptor::Create(context_properties, input.data_type(),
                                   output_shape, label);
}

base::expected<OperandDescriptor, std::string> ValidateTriangularAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& input,
    std::string_view label) {
  if (!context_properties.data_type_limits.triangular_input.Supports(input)) {
    return base::unexpected(ErrorWithLabel(
        label,
        NotSupportedInputArgumentError(
            input, context_properties.data_type_limits.triangular_input)));
  }

  // The output tensor of triangular is the same shape and the same type as the
  // input tensor.
  return input;
}

base::expected<OperandDescriptor, std::string> ValidateWhereAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& condition,
    const OperandDescriptor& true_value,
    const OperandDescriptor& false_value,
    std::string_view label) {
  if (!context_properties.data_type_limits.where_condition.Supports(
          condition)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedArgumentError(
                   kConditionParam, condition,
                   context_properties.data_type_limits.where_condition)));
  }

  if (!context_properties.data_type_limits.where_value.Supports(true_value)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedArgumentError(
                   kTrueValueParam, true_value,
                   context_properties.data_type_limits.where_value)));
  }

  if (!context_properties.data_type_limits.where_value.Supports(false_value)) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedArgumentError(
                   kFalseValueParam, false_value,
                   context_properties.data_type_limits.where_value)));
  }

  if (true_value.data_type() != false_value.data_type()) {
    return base::unexpected(ErrorWithLabel(
        label, "The data types of trueValue and falseValue don't match."));
  }

  const std::optional<std::vector<uint32_t>> value_shape = BroadcastShapes(
      true_value.shape(), false_value.shape(), /*bidirectional=*/true);
  if (!value_shape) {
    return base::unexpected(ErrorWithLabel(
        label,
        "The shapes of trueValue and falseValue are not broadcastable."));
  }

  std::optional<std::vector<uint32_t>> output_shape = BroadcastShapes(
      condition.shape(), value_shape.value(), /*bidirectional=*/true);
  if (!output_shape) {
    return base::unexpected(ErrorWithLabel(
        label,
        "The condition shape is not broadcastable to the shape broadcasted "
        "from trueValue and falseValue."));
  }
  return OperandDescriptor::Create(context_properties, true_value.data_type(),
                                   *output_shape, label);
}

base::expected<void, std::string> ValidateAxes(base::span<const uint32_t> axes,
                                               uint32_t rank,
                                               std::string_view label) {
  if (std::ranges::any_of(axes, [rank](uint32_t axis) {
        return base::MakeStrictNum(axis) >= rank;
      })) {
    return base::unexpected(ErrorWithLabel(
        label, base::StringPrintf(
                   "The values in axes must be in the range [0, %u).", rank)));
  }

  if (axes.size() != std::set<uint32_t>(axes.begin(), axes.end()).size()) {
    return base::unexpected(ErrorWithLabel(
        label, "Two or more values are same in the axes sequence."));
  }

  return base::ok();
}

base::expected<void, std::string> ValidateTensor(
    const ContextProperties& context_properties,
    OperandDescriptor descriptor) {
  // TODO(crbug.com/343638938): Consider adding more constraints to MLTensor
  // creation, such as whether an MLTensor...
  // - may be empty
  // - may have a max size (in addition to `OperandDescriptor` restrictions)

  // TODO(crbug.com/356905054): Consider adding `DataTypeLimits` specific to
  // `MLTensor` rather than using `input`.
  if (!context_properties.data_type_limits.input.Has(descriptor.data_type())) {
    return base::unexpected(NotSupportedMLTensorTypeError(
        descriptor.data_type(), context_properties.data_type_limits.input));
  }

  const size_t byte_length = descriptor.PackedByteLength();
  if (byte_length > context_properties.tensor_byte_length_limit) {
    return base::unexpected(NotSupportedTensorSizeError(
        byte_length, context_properties.tensor_byte_length_limit));
  }

  return base::ok();
}

std::optional<std::vector<uint32_t>> BroadcastShapes(
    base::span<const uint32_t> dims_lhs,
    base::span<const uint32_t> dims_rhs,
    bool bidirectional) {
  // If bidirectional is true, the rank of the output shape is the maximum rank
  // of the input shapes. Otherwise it is as the same as the rhs' rank.
  auto rank_lhs = dims_lhs.size(), rank_rhs = dims_rhs.size();
  if (!bidirectional && rank_lhs > rank_rhs) {
    return std::nullopt;
  }

  auto rank_output = bidirectional ? std::max(rank_lhs, rank_rhs) : rank_rhs;
  std::vector<uint32_t> dims_output(rank_output);
  for (size_t i = 0; i < rank_output; ++i) {
    auto dim_lhs = i < rank_lhs ? dims_lhs[rank_lhs - i - 1] : 1;
    DCHECK_GT(dim_lhs, static_cast<uint32_t>(0));
    auto dim_rhs = i < rank_rhs ? dims_rhs[rank_rhs - i - 1] : 1;
    DCHECK_GT(dim_rhs, static_cast<uint32_t>(0));
    // If bidirectional is true, two dimensions are compatible when they are
    // equal, or one of them is 1. Otherwise, two dimensions are compatible when
    // they are equal, or the lhs dimension is 1.
    if (bidirectional) {
      if (dim_lhs != dim_rhs && dim_lhs != 1 && dim_rhs != 1) {
        return std::nullopt;
      }
    } else if (dim_lhs != dim_rhs && dim_lhs != 1) {
      return std::nullopt;
    }
    // If bidirectional is true, for each dimension of the output tensor, its
    // size is the maximum size along that dimension of the input shapes.
    // Otherwise, its size is the same as the rhs.
    dims_output[rank_output - i - 1] =
        bidirectional ? std::max(dim_lhs, dim_rhs) : dim_rhs;
  }
  return dims_output;
}

base::expected<uint32_t, std::string> CalculateConvTranspose2dOutputSize(
    const uint32_t input_size,
    const uint32_t filter_size,
    const uint32_t beginning_padding,
    const uint32_t ending_padding,
    const uint32_t stride,
    const uint32_t dilation,
    const uint32_t output_padding) {
  // Calculate the dilated filter sizes.
  auto checked_effective_filter_size =
      (base::MakeCheckedNum<uint32_t>(filter_size) - 1) * dilation + 1;
  if (!checked_effective_filter_size.IsValid()) {
    return base::unexpected("The effective filter size is too large.");
  }
  auto checked_output_size =
      (base::MakeCheckedNum<uint32_t>(input_size) - 1) * stride +
      checked_effective_filter_size - beginning_padding - ending_padding +
      output_padding;
  if (!checked_output_size.IsValid()) {
    return base::unexpected(
        "The stride is too large or the input size is too small for padding.");
  }

  return checked_output_size.ValueOrDie();
}

bool IsDepthwiseConv2d(uint32_t input_channels,
                       uint32_t output_channels,
                       uint32_t groups) {
  return groups == input_channels && groups == output_channels && groups != 1;
}

}  // namespace webnn
