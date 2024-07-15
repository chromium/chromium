// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/graph_validation_utils.h"

#include <algorithm>
#include <numeric>
#include <set>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/cpp/webnn_errors.h"

namespace webnn {

namespace {

// Calculate the output size for conv2d based on WebNN spec:
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-conv2d
// Return the calculated output size if no error.
base::expected<double, std::string> CalculateConv2dOutputSize(
    const uint32_t input_size,
    const uint32_t filter_size,
    const uint32_t beginning_padding,
    const uint32_t ending_padding,
    const uint32_t stride,
    const uint32_t dilation) {
  // Calculate the dilated filter sizes.
  auto checked_effective_filter_size =
      (base::MakeCheckedNum<uint32_t>(filter_size) - 1) * dilation + 1;
  if (!checked_effective_filter_size.IsValid()) {
    return base::unexpected("The effective filter size is too large.");
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

  if (checked_output_size.ValueOrDie() < 0) {
    return base::unexpected("The input size is too small to fill the window.");
  }

  // Check if the value is valid for rounding to uint32_t type.
  if (!checked_output_size.IsValid<uint32_t>()) {
    return base::unexpected("The output size is too large.");
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
                                      const Size2d<uint32_t>& dilations) {
  if (strides.height == 0 || strides.width == 0) {
    return base::unexpected("All strides should be greater than 0.");
  }
  if (dilations.height == 0 || dilations.width == 0) {
    return base::unexpected("All dilations should be greater than 0.");
  }

  const auto float_output_height = CalculateConv2dOutputSize(
      input_height, filter_height, padding.beginning.height,
      padding.ending.height, strides.height, dilations.height);
  if (!float_output_height.has_value()) {
    return base::unexpected("Failed to calculate the output height: " +
                            float_output_height.error());
  }

  const auto float_output_width = CalculateConv2dOutputSize(
      input_width, filter_width, padding.beginning.width, padding.ending.width,
      strides.width, dilations.width);
  if (!float_output_width.has_value()) {
    return base::unexpected("Failed to calculate the output width: " +
                            float_output_width.error());
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
    const Size2d<uint32_t>& output_padding) {
  if (strides.height == 0 || strides.width == 0) {
    return base::unexpected("All strides should be greater than 0.");
  }
  if (dilations.height == 0 || dilations.width == 0) {
    return base::unexpected("All dilations should be greater than 0.");
  }
  if (output_padding.height >= strides.height ||
      output_padding.width >= strides.width) {
    return base::unexpected(
        "The output padding must be smaller than the stride along the same "
        "dimension.");
  }

  const auto output_height = CalculateConvTranspose2dOutputSize(
      input_height, filter_height, padding.beginning.height,
      padding.ending.height, strides.height, dilations.height,
      output_padding.height);
  if (!output_height.has_value()) {
    return base::unexpected("Failed to calculate the output height: " +
                            output_height.error());
  }

  const auto output_width = CalculateConvTranspose2dOutputSize(
      input_width, filter_width, padding.beginning.width, padding.ending.width,
      strides.width, dilations.width, output_padding.width);
  if (!output_width.has_value()) {
    return base::unexpected("Failed to calculate the output width: " +
                            output_width.error());
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

// Validate and get the input info of 2-D direct and transposed convolution
// operation given input operand and attributes.
base::expected<Conv2dInputOutputInfo, std::string>
ValidateAndGetConv2dInputInfo(const OperandDescriptor& input,
                              const Conv2dAttributesBase& attributes) {
  // Validate input operand.
  if (!IsFloatingPointType(input.data_type())) {
    return base::unexpected(
        "The input data type must be a floating point type.");
  }

  if (input.Rank() != 4) {
    return base::unexpected("The input should be a 4-D tensor.");
  }

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
    const OperandDescriptor& input,
    const Conv2dAttributesBase& attributes,
    const Conv2dInputOutputInfo& output_info) {
  // Validate bias operand if it is present.
  if (attributes.bias_operand) {
    if (attributes.bias_operand->Rank() != 1) {
      return base::unexpected("The bias should be a 1-D tensor.");
    }
    if (attributes.bias_operand->shape()[0] != output_info.channels) {
      return base::unexpected(base::StringPrintf(
          "The bias shape should be [%u].", output_info.channels));
    }
    if (attributes.bias_operand->data_type() != input.data_type()) {
      return base::unexpected(
          "The bias data type doesn't match input data type.");
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

  return OperandDescriptor::Create(input.data_type(), output_shape);
}

// Validate the axes and infer output for reduce operations.
base::expected<std::vector<uint32_t>, std::string>
ValidateReduceAxesAndInferOutput(base::span<const uint32_t> input_dimensions,
                                 base::span<const uint32_t> axes,
                                 bool keep_dimensions) {
  auto input_rank = input_dimensions.size();
  RETURN_IF_ERROR(ValidateAxes(axes, input_rank));

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
    OperandDataType input_data_type) {
  if (operand.Rank() != expected_shape.size()) {
    return base::unexpected(
        base::StringPrintf("The %s operand should be a %zu-D tensor.",
                           operand_name, expected_shape.size()));
  }
  if (!base::ranges::equal(operand.shape(), expected_shape)) {
    return base::unexpected(
        base::StringPrintf("The %s operand shape is invalid.", operand_name));
  }
  if (operand.data_type() != input_data_type) {
    return base::unexpected(base::StringPrintf(
        "The %s operand data type doesn't match the input data type.",
        operand_name));
  }
  return base::ok();
}

std::string ErrorWithLabel(std::string_view error_message,
                           std::string_view label) {
  return base::StrCat({GetLabelErrorSuffix(label), error_message});
}

}  // namespace

std::string DataTypeConstraintToString(
    const SupportedDataTypes& constraint_set) {
  std::vector<std::string> data_types;
  data_types.reserve(constraint_set.size());
  for (auto data_type : constraint_set) {
    std::string data_type_as_string;
    switch (data_type) {
      case OperandDataType::kFloat32:
        data_type_as_string = "float32";
        break;
      case OperandDataType::kFloat16:
        data_type_as_string = "float16";
        break;
      case OperandDataType::kInt32:
        data_type_as_string = "int32";
        break;
      case OperandDataType::kUint32:
        data_type_as_string = "uint32";
        break;
      case OperandDataType::kInt64:
        data_type_as_string = "int64";
        break;
      case OperandDataType::kUint64:
        data_type_as_string = "uint64";
        break;
      case OperandDataType::kInt8:
        data_type_as_string = "int8";
        break;
      case OperandDataType::kUint8:
        data_type_as_string = "uint8";
        break;
    }
    data_types.push_back(std::move(data_type_as_string));
  }
  return base::JoinString(data_types, /*separator=*/",");
}

base::expected<OperandDescriptor, std::string> ValidateSoftmaxAndInferOutput(
    const OperandDescriptor& input,
    uint32_t axis) {
  // The input data type must be one of the floating point types.
  if (!IsFloatingPointType(input.data_type())) {
    return base::unexpected(
        "The input data type must be one of the floating point types.");
  }
  if (axis >= input.Rank()) {
    return base::unexpected("Axis must be a valid dimension.");
  }
  // The output tensor of softmax is the same shape as the input tensor.
  return input;
}

base::expected<OperandDescriptor, std::string> ValidateArgMinMaxAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& input,
    base::span<const uint32_t> axes,
    OperandDataType output_data_type,
    bool keep_dimensions) {
  if (!context_properties.data_type_limits.arg_min_max_input.Has(
          input.data_type())) {
    return base::unexpected(NotSupportedInputArgumentTypeError(
        input.data_type(),
        context_properties.data_type_limits.arg_min_max_input));
  }

  if (!context_properties.data_type_limits.arg_min_max_output.Has(
          output_data_type)) {
    return base::unexpected(NotSupportedOpOutputTypeError(
        output_data_type,
        context_properties.data_type_limits.arg_min_max_output));
  }

  ASSIGN_OR_RETURN(
      std::vector<uint32_t> output_shape,
      ValidateReduceAxesAndInferOutput(input.shape(), axes, keep_dimensions));
  return OperandDescriptor::Create(output_data_type, output_shape);
}

base::expected<std::vector<OperandDescriptor>, std::string>
ValidateSplitAndInferOutput(const OperandDescriptor& input,
                            const SplitAttribute& attributes) {
  std::vector<OperandDescriptor> outputs;

  if (attributes.axis >= input.Rank()) {
    return base::unexpected(
        "The axis must be in the range [0, N-1] where N is the rank of the "
        "input tensor.");
  }

  static_assert(absl::variant_size<decltype(attributes.splits)>() == 2,
                "When adding new variants update the branches below.");
  if (absl::holds_alternative<uint32_t>(attributes.splits)) {
    uint32_t splits = absl::get<uint32_t>(attributes.splits);
    if (splits == 0) {
      return base::unexpected("The splits must be greater than zero.");
    }

    if (input.shape()[attributes.axis] % splits != 0) {
      return base::unexpected(
          "The dimension size of the input tensor along "
          "options.axis must be divisible by splits.");
    }

    outputs.reserve(splits);
    for (uint32_t i = 0; i < splits; ++i) {
      // When splits is of type uint32_t, we create splits number of Operands.
      // Each Operand will have the same new_dimensions shape.
      std::vector<uint32_t> new_dimensions = input.shape();
      new_dimensions[attributes.axis] /= splits;
      auto split_descriptor =
          OperandDescriptor::Create(input.data_type(), new_dimensions);
      // `split_descriptor` should always be valid, since it's a subset of the
      // input.
      CHECK(split_descriptor.has_value());
      outputs.push_back(*std::move(split_descriptor));
    }
  } else if (absl::holds_alternative<base::span<const uint32_t>>(
                 attributes.splits)) {
    const auto& splits =
        absl::get<base::span<const uint32_t>>(attributes.splits);
    if (base::ranges::any_of(splits,
                             [](uint32_t split) { return split == 0; })) {
      return base::unexpected("All splits must be greater than zero.");
    }

    base::CheckedNumeric<uint32_t> sum = std::accumulate(
        splits.begin(), splits.end(), base::MakeCheckedNum<uint32_t>(0));
    if (!sum.IsValid() || sum.ValueOrDie() != input.shape()[attributes.axis]) {
      return base::unexpected(
          "The sum of all sizes in splits must be equal to the dimension size "
          "of the input tensor specified by options.axis.");
    }

    outputs.reserve(splits.size());
    for (uint32_t split : splits) {
      std::vector<uint32_t> new_dimensions = input.shape();
      new_dimensions[attributes.axis] = split;
      auto split_descriptor =
          OperandDescriptor::Create(input.data_type(), new_dimensions);
      // `split_descriptor` should always be valid, since it's a subset of the
      // input.
      CHECK(split_descriptor.has_value());
      outputs.push_back(*std::move(split_descriptor));
    }
  } else {
    NOTREACHED_NORETURN();
  }

  return outputs;
}

// This helper method is intended to validate mean, variance, scale and bias
// operands of batchNormalization and instanceNormalization against the input
// operand. These operands share the same constraint.
base::expected<void, std::string>
ValidateNormalizationOperandIsCompatibleWithInput(
    const OperandDescriptor& operand,
    const OperandDataType input_data_type,
    size_t input_size_on_axis) {
  if (operand.data_type() != input_data_type) {
    return base::unexpected("the data type doesn't match the input data type.");
  }
  if (operand.Rank() != 1) {
    return base::unexpected("the operand should be a 1-D tensor.");
  }

  if (operand.shape()[0] != input_size_on_axis) {
    return base::unexpected(
        "the size of operand must be equal to the size of the feature "
        "dimension of the input.");
  }

  return base::ok();
}

BatchNormalizationAttributes::BatchNormalizationAttributes() = default;
BatchNormalizationAttributes::~BatchNormalizationAttributes() = default;

BatchNormalizationAttributes::BatchNormalizationAttributes(
    BatchNormalizationAttributes&& other) = default;
BatchNormalizationAttributes& BatchNormalizationAttributes::operator=(
    BatchNormalizationAttributes&& other) = default;

base::expected<OperandDescriptor, std::string>
ValidateBatchNormalizationAndInferOutput(
    const OperandDescriptor& input,
    const OperandDescriptor& mean,
    const OperandDescriptor& variance,
    const BatchNormalizationAttributes& attributes) {
  // Validate input type.
  const auto label = attributes.label;
  if (!IsFloatingPointType(input.data_type())) {
    return base::unexpected(ErrorWithLabel(
        label, "The input type must be one of the floating point types."));
  }
  if (attributes.axis >= input.Rank()) {
    return base::unexpected(ErrorWithLabel(
        label,
        "The value of axis must be in the range [0, N-1] where N is the rank "
        "of the input tensor."));
  }

  uint32_t input_size_on_axis = input.shape()[attributes.axis];
  // Validate mean operand.
  const auto validation_mean =
      ValidateNormalizationOperandIsCompatibleWithInput(mean, input.data_type(),
                                                        input_size_on_axis);
  if (!validation_mean.has_value()) {
    return base::unexpected(
        ErrorWithLabel(label, "For mean operand: " + validation_mean.error()));
  }

  // Validate variance operand.
  const auto validation_variance =
      ValidateNormalizationOperandIsCompatibleWithInput(
          variance, input.data_type(), input_size_on_axis);
  if (!validation_variance.has_value()) {
    return base::unexpected(ErrorWithLabel(
        label, "For variance operand: " + validation_variance.error()));
  }

  // Validate scale operand.
  if (attributes.scale) {
    const auto validation_scale =
        ValidateNormalizationOperandIsCompatibleWithInput(
            attributes.scale.value(), input.data_type(), input_size_on_axis);
    if (!validation_scale.has_value()) {
      return base::unexpected(ErrorWithLabel(
          label, "For scale operand: " + validation_scale.error()));
    }
  }

  // Validate bias operand.
  if (attributes.bias) {
    const auto validation_bias =
        ValidateNormalizationOperandIsCompatibleWithInput(
            attributes.bias.value(), input.data_type(), input_size_on_axis);
    if (!validation_bias.has_value()) {
      return base::unexpected(ErrorWithLabel(
          label, "For bias operand: " + validation_bias.error()));
    }
  }

  // The output tensor of batchNormalization is the same shape as the input
  // tensor.
  return input;
}

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

base::expected<OperandDescriptor, std::string> ValidateConv2dAndInferOutput(
    const OperandDescriptor& input,
    const OperandDescriptor& filter,
    const Conv2dAttributes& attributes) {
  // Validate input operand.
  ASSIGN_OR_RETURN(Conv2dInputOutputInfo input_info,
                   ValidateAndGetConv2dInputInfo(input, attributes));
  const auto label = attributes.label;
  // Validate filter operand.
  if (filter.data_type() != input.data_type()) {
    return base::unexpected(ErrorWithLabel(
        label, "The filter data type doesn't match the input data type."));
  }

  if (filter.Rank() != 4) {
    return base::unexpected(
        ErrorWithLabel(label, "The filter should be a 4-D tensor."));
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
          attributes.padding, attributes.strides, attributes.dilations));

  uint32_t output_height = base::ClampFloor<uint32_t>(output_sizes.height);
  uint32_t output_width = base::ClampFloor<uint32_t>(output_sizes.width);

  Conv2dInputOutputInfo output_info{.batches = input_info.batches,
                                    .channels = output_channels,
                                    .height = output_height,
                                    .width = output_width};
  return ValidateConv2dBiasAndCreateOutputOperand(input, attributes,
                                                  output_info);
}

ConvTranspose2dAttributes::ConvTranspose2dAttributes() = default;
ConvTranspose2dAttributes::~ConvTranspose2dAttributes() = default;

ConvTranspose2dAttributes::ConvTranspose2dAttributes(
    ConvTranspose2dAttributes&& other) = default;
ConvTranspose2dAttributes& ConvTranspose2dAttributes::operator=(
    ConvTranspose2dAttributes&& other) = default;

base::expected<OperandDescriptor, std::string>
ValidateConvTranspose2dAndInferOutput(
    const OperandDescriptor& input,
    const OperandDescriptor& filter,
    const ConvTranspose2dAttributes& attributes) {
  // Validate input operand.
  const auto label = attributes.label;
  const auto input_info = ValidateAndGetConv2dInputInfo(input, attributes);
  if (!input_info.has_value()) {
    return base::unexpected(ErrorWithLabel(label, input_info.error()));
  }
  // Validate filter operand.
  if (filter.data_type() != input.data_type()) {
    return base::unexpected(ErrorWithLabel(
        label, "The filter data type doesn't match the input data type."));
  }

  if (filter.Rank() != 4) {
    return base::unexpected(
        ErrorWithLabel(label, "The filter should be a 4-D tensor."));
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
  if (input_info->channels != input_channels) {
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
            input_info->height, input_info->width, filter_height, filter_width,
            attributes.padding, strides, attributes.dilations,
            // According to WebNN spec:
            // https://webmachinelearning.github.io/webnn/#dom-mlconvtranspose2doptions-outputsizes
            // When the output sizes are explicitly specified, the output
            // padding values in outputPadding are ignored.
            {0, 0}));
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
    ASSIGN_OR_RETURN(Size2d<uint32_t> output_sizes,
                     ValidateAndCalculateConvTranspose2dOutputSizes(
                         input_info->height, input_info->width, filter_height,
                         filter_width, attributes.padding, attributes.strides,
                         attributes.dilations, attributes.output_padding));
    output_height = output_sizes.height;
    output_width = output_sizes.width;
  }

  Conv2dInputOutputInfo output_info{.batches = input_info->batches,
                                    .channels = output_channels,
                                    .height = output_height,
                                    .width = output_width};
  return ValidateConv2dBiasAndCreateOutputOperand(input, attributes,
                                                  output_info);
}

base::expected<OperandDescriptor, std::string> ValidatePadAndInferOutput(
    const OperandDescriptor& input,
    base::span<const uint32_t> beginning_padding,
    base::span<const uint32_t> ending_padding) {
  if (input.Rank() == 0) {
    return base::unexpected("The input should not be a scalar.");
  }

  // Validate the beginning_padding and ending_padding.
  if (beginning_padding.size() != input.Rank()) {
    return base::unexpected(
        "The length of beginningPadding must be "
        "equal to the rank of the input tensor.");
  }
  if (ending_padding.size() != input.Rank()) {
    return base::unexpected(
        "The length of endingPadding must be "
        "equal to the rank of the input tensor.");
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
      return base::unexpected(base::StringPrintf(
          "The padding of dimension (%zu) is too large.", i));
    }
  }

  return OperandDescriptor::Create(input.data_type(), output_shape);
}

base::expected<OperandDescriptor, std::string> ValidateMatmulAndInferOutput(
    const OperandDescriptor& a,
    const OperandDescriptor& b) {
  if (!IsFloatingPointType(a.data_type())) {
    return base::unexpected(
        "The data type of inputs must be one of the floating point types.");
  }

  if (a.data_type() != b.data_type()) {
    return base::unexpected("The data types of first two inputs don't match.");
  }

  // Based on the WG discussion:
  // https://github.com/webmachinelearning/webnn/issues/470, prototype the
  // matmul without 1-D input tensors support.
  if (a.Rank() < 2 || b.Rank() < 2) {
    return base::unexpected(
        "The rank of input must be larger than or equal to 2.");
  }

  std::vector<uint32_t> a_dimensions = a.shape();
  std::vector<uint32_t> b_dimensions = b.shape();

  // The number of columns in the first matrix must be equal to the number of
  // rows in the second matrix.
  const uint32_t a_cols = a_dimensions[a_dimensions.size() - 1];
  const uint32_t a_rows = a_dimensions[a_dimensions.size() - 2];
  const uint32_t b_cols = b_dimensions[b_dimensions.size() - 1];
  const uint32_t b_rows = b_dimensions[b_dimensions.size() - 2];
  if (a_cols != b_rows) {
    return base::unexpected(base::StringPrintf(
        "The number of columns (%u) in the first matrix isn't equal to "
        "the number of rows (%u) in the second matrix.",
        a_cols, b_rows));
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
      return base::unexpected("The matmul input shapes are not broadcastable.");
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
  return OperandDescriptor::Create(a.data_type(), output_dimensions);
}

base::expected<OperandDescriptor, std::string> ValidatePool2dAndInferOutput(
    const OperandDescriptor& input,
    const Pool2dAttributes& attributes) {
  // Validate input operand and set its sizes.
  if (input.Rank() != 4) {
    return base::unexpected("The input should be a 4-D tensor.");
  }

  const std::vector<uint32_t>& input_shape = input.shape();
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
      return base::unexpected(
          "All window dimensions should be greater than 0.");
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
          attributes.padding, attributes.strides, attributes.dilations));

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
      return base::unexpected("All output sizes should be greater than 0.");
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
      return base::unexpected(
          (floor_output_height == ceil_output_height &&
           floor_output_width == ceil_output_width)
              ? base::StringPrintf("The output sizes should be [%u, %u].",
                                   floor_output_height, floor_output_width)
              : base::StringPrintf(
                    "The output sizes should be either [%u, %u] or [%u, %u].",
                    floor_output_height, floor_output_width, ceil_output_height,
                    ceil_output_width));
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
  return OperandDescriptor::Create(input.data_type(), output_shape);
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
  const uint32_t output_size =
      base::ClampFloor<uint32_t>(double(checked_output_size.ValueOrDie()));
  if (output_size == 0) {
    return base::unexpected(ErrorWithLabel(label, "The scale is too small."));
  }
  return output_size;
}

base::expected<OperandDescriptor, std::string> ValidateResample2dAndInferOutput(
    const OperandDescriptor& input,
    const absl::variant<base::span<const float>, base::span<const uint32_t>>&
        scales_or_sizes,
    base::span<const uint32_t> axes,
    std::string_view label) {
  // Validate the input.
  if (!IsFloatingPointType(input.data_type())) {
    return base::unexpected(ErrorWithLabel(label,
                                           "The data type of the input must be "
                                           "one of the floating point types."));
  }

  if (input.Rank() != 4) {
    return base::unexpected(
        ErrorWithLabel(label, "The input must be a 4-D tensor."));
  }

  // Validate axes.
  // According to WebNN spec:
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-resample2d,
  // the valid values in the sequence are [0, 1], [1, 2] or [2, 3].
  if (axes.size() != 2) {
    return base::unexpected(
        ErrorWithLabel(label, "The length of axes should be 2."));
  }
  if (!((axes[0] == 0 && axes[1] == 1) || (axes[0] == 1 && axes[1] == 2) ||
        (axes[0] == 2 && axes[1] == 3))) {
    return base::unexpected(
        ErrorWithLabel(label, "The values of axes are invalid."));
  }

  // Validate scales or sizes and infer the output.
  std::vector<uint32_t> output_shape(input.shape());
  if (absl::holds_alternative<base::span<const float>>(scales_or_sizes)) {
    const auto& scales = absl::get<base::span<const float>>(scales_or_sizes);
    if (scales.size() != 2) {
      return base::unexpected(
          ErrorWithLabel(label, "The length of scales should be 2."));
    }
    if (scales[0] <= 0 || scales[1] <= 0) {
      return base::unexpected(
          ErrorWithLabel(label, "All scales should be greater than 0."));
    }

    auto output_height =
        CalculateResample2dOutputSize(input.shape()[axes[0]], scales[0], label);
    if (!output_height.has_value()) {
      return base::unexpected(ErrorWithLabel(
          label,
          "Failed to calculate the output height: " + output_height.error()));
    }
    output_shape[axes[0]] = output_height.value();

    auto output_width =
        CalculateResample2dOutputSize(input.shape()[axes[1]], scales[1], label);
    if (!output_width.has_value()) {
      return base::unexpected(ErrorWithLabel(
          label,
          "Failed to calculate the output width: " + output_width.error()));
    }
    output_shape[axes[1]] = output_width.value();
  } else if (absl::holds_alternative<base::span<const uint32_t>>(
                 scales_or_sizes)) {
    const auto& sizes = absl::get<base::span<const uint32_t>>(scales_or_sizes);
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
    NOTREACHED_NORETURN();
  }

  return OperandDescriptor::Create(input.data_type(), output_shape);
}

base::expected<OperandDescriptor, std::string> ValidateGatherAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& input,
    const OperandDescriptor& indices,
    const uint32_t axis) {
  if (input.Rank() == 0) {
    return base::unexpected("The input should not be a scalar.");
  }

  if (input.Rank() <= axis) {
    return base::unexpected(
        "The axis must be in the range [0, N-1] where N is the rank of input "
        "tensor.");
  }

  if (!context_properties.data_type_limits.gather_input.Has(
          input.data_type())) {
    return base::unexpected(NotSupportedInputArgumentTypeError(
        input.data_type(), context_properties.data_type_limits.gather_input));
  }

  static constexpr char kIndicesParam[] = "indices";
  if (!context_properties.data_type_limits.gather_indices.Has(
          indices.data_type())) {
    return base::unexpected(NotSupportedArgumentTypeError(
        kIndicesParam, indices.data_type(),
        context_properties.data_type_limits.gather_indices));
  }

  // TODO(crbug.com/325598628): Remove this checked math once input ranks are
  // capped.
  auto checked_output_rank =
      base::MakeCheckedNum<size_t>(input.Rank()) - 1 + indices.Rank();
  if (!checked_output_rank.IsValid()) {
    return base::unexpected("The output rank is too large.");
  }

  std::vector<uint32_t> output_shape;
  output_shape.reserve(checked_output_rank.ValueOrDie());
  for (size_t i = 0; i < input.Rank(); ++i) {
    if (i == axis) {
      base::ranges::copy(indices.shape(), std::back_inserter(output_shape));
    } else {
      output_shape.push_back(input.shape()[i]);
    }
  }

  return OperandDescriptor::Create(input.data_type(), output_shape);
}

GemmAttributes::GemmAttributes() = default;
GemmAttributes::~GemmAttributes() = default;

GemmAttributes::GemmAttributes(GemmAttributes&& other) = default;
GemmAttributes& GemmAttributes::operator=(GemmAttributes&& other) = default;

base::expected<OperandDescriptor, std::string> ValidateGemmAndInferOutput(
    const OperandDescriptor& a,
    const OperandDescriptor& b,
    const GemmAttributes& attributes) {
  if (!IsFloatingPointType(a.data_type())) {
    return base::unexpected(
        "The data type of inputs must be one of the floating point types.");
  }

  if (a.data_type() != b.data_type()) {
    return base::unexpected("The data types of first two inputs don't match.");
  }
  // According to WebNN spec:
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-gemm, the first input 2-D
  // tensor with shape [M, K] if aTranspose is false, or [K, M] if aTranspose is
  // true.
  if (a.Rank() != 2) {
    return base::unexpected("The first input must be a 2-D tensor.");
  }
  if (b.Rank() != 2) {
    return base::unexpected("The second input must be a 2-D tensor.");
  }

  std::vector<uint32_t> shape_a = a.shape();
  if (attributes.a_transpose) {
    base::ranges::reverse(shape_a);
  }
  // The second input 2-D tensor with shape [K, N] if bTranspose is false, or
  // [N, K] if bTranspose is true.
  std::vector<uint32_t> shape_b = b.shape();
  if (attributes.b_transpose) {
    base::ranges::reverse(shape_b);
  }
  // The number of columns in the first matrix must be equal to the number of
  // rows in the second matrix.
  if (shape_a[1] != shape_b[0]) {
    return base::unexpected(base::StringPrintf(
        "The number of columns (%u) in the %sfirst matrix isn't equal to "
        "the number of rows (%u) in the %ssecond matrix.",
        shape_a[1], attributes.a_transpose ? "transposed " : "", shape_b[0],
        attributes.b_transpose ? "transposed " : ""));
  };
  // The output is 2-D tensor of shape [M, N].
  std::vector<uint32_t> output_shape = {shape_a[0], shape_b[1]};
  // The third input tensor c is either a scalar, or of the shape that is
  // unidirectionally broadcastable to the output shape [M, N].
  if (attributes.c_operand) {
    if (attributes.c_operand->data_type() != a.data_type()) {
      return base::unexpected(
          "The third input data type doesn't match other inputs' data type.");
    }
    if (attributes.c_operand->Rank() > 2) {
      return base::unexpected(
          "The third input tensor should be either a scalar or a 2-D tensor.");
    }

    if (!BroadcastShapes(attributes.c_operand->shape(), output_shape,
                         /*bidirectional=*/false)) {
      return base::unexpected(
          "The third input tensor isn't unidirectionally broadcastable to the "
          "output tensor.");
    }
  }
  return OperandDescriptor::Create(a.data_type(), output_shape);
}

GruAttributes::GruAttributes() = default;
GruAttributes::~GruAttributes() = default;

GruAttributes::GruAttributes(GruAttributes&& other) = default;
GruAttributes& GruAttributes::operator=(GruAttributes&& other) = default;

base::expected<std::vector<OperandDescriptor>, std::string>
ValidateGruAndInferOutput(const OperandDescriptor& input,
                          const OperandDescriptor& weight,
                          const OperandDescriptor& recurrent_weight,
                          uint32_t steps,
                          uint32_t hidden_size,
                          const GruAttributes& attributes) {
  if (steps <= 0) {
    return base::unexpected("The steps must be greater than 0.");
  }
  if (hidden_size <= 0) {
    return base::unexpected("The hidden size must be greater than 0.");
  }

  // Validate the weight operand.
  // The current spec doesn't specify the operand data type constraints of
  // gru. An issue has been filed to track it:
  // https://github.com/webmachinelearning/webnn/issues/283.
  if (!IsFloatingPointType(input.data_type())) {
    return base::unexpected(
        "The data type of input must be one of the floating point types.");
  }
  if (input.Rank() != 3) {
    return base::unexpected("The input must be a 3-D tensor.");
  }

  const std::vector<uint32_t>& input_dimensions = input.shape();
  if (input_dimensions[0] != steps) {
    return base::unexpected(
        "The input dimension[0] must be equal to the steps.");
  }
  const auto batch_size = input_dimensions[1];
  const auto input_size = input_dimensions[2];
  auto checked_three_times_hidden_size = base::MakeCheckedNum(hidden_size) * 3;
  uint32_t three_times_hidden_size;
  if (!checked_three_times_hidden_size.AssignIfValid(
          &three_times_hidden_size)) {
    return base::unexpected("The hidden size is too large.");
  }
  const uint32_t num_directions =
      attributes.direction == RecurrentNetworkDirection::kBoth ? 2 : 1;

  // Validate the weight operand.
  std::array<uint32_t, 3> expected_weight_shape = {
      num_directions, three_times_hidden_size, input_size};
  RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
      weight, "weight", expected_weight_shape, input.data_type()));

  // Validate the recurrent weight operand.
  std::array<uint32_t, 3> expected_recurrent_weight_shape = {
      num_directions, three_times_hidden_size, hidden_size};
  RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
      recurrent_weight, "recurrent weight", expected_recurrent_weight_shape,
      input.data_type()));

  // Validate the bias operand.
  std::array<uint32_t, 2> expected_bias_shape = {num_directions,
                                                 three_times_hidden_size};
  if (attributes.bias) {
    RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
        *attributes.bias, "bias", expected_bias_shape, input.data_type()));
  }

  // Validate the recurrent bias operand.
  std::array<uint32_t, 2> expected_recurrent_bias_shape = {
      num_directions, three_times_hidden_size};
  if (attributes.recurrent_bias) {
    RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
        *attributes.recurrent_bias, "recurrent bias",
        expected_recurrent_bias_shape, input.data_type()));
  }

  // Validate the initial hidden state operand.
  std::array<uint32_t, 3> expected_initial_hidden_state_shape = {
      num_directions, batch_size, hidden_size};
  if (attributes.initial_hidden_state) {
    RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
        *attributes.initial_hidden_state, "initial hidden state",
        expected_initial_hidden_state_shape, input.data_type()));
  }

  if (attributes.activation_count != 2) {
    return base::unexpected("The number of activations must be 2.");
  }

  std::vector<OperandDescriptor> outputs;
  ASSIGN_OR_RETURN(OperandDescriptor output,
                   OperandDescriptor::Create(
                       input.data_type(),
                       std::array{num_directions, batch_size, hidden_size}));
  outputs.push_back(std::move(output));
  if (attributes.return_sequence) {
    ASSIGN_OR_RETURN(
        OperandDescriptor return_sequence_output,
        OperandDescriptor::Create(
            input.data_type(),
            std::array{steps, num_directions, batch_size, hidden_size}));
    outputs.push_back(std::move(return_sequence_output));
  }

  return outputs;
}

GruCellAttributes::GruCellAttributes() = default;
GruCellAttributes::~GruCellAttributes() = default;

GruCellAttributes::GruCellAttributes(GruCellAttributes&& other) = default;
GruCellAttributes& GruCellAttributes::operator=(GruCellAttributes&& other) =
    default;

base::expected<OperandDescriptor, std::string> ValidateGruCellAndInferOutput(
    const OperandDescriptor& input,
    const OperandDescriptor& weight,
    const OperandDescriptor& recurrent_weight,
    const OperandDescriptor& hidden_state,
    uint32_t hidden_size,
    const GruCellAttributes& attributes) {
  if (hidden_size <= 0) {
    return base::unexpected("The hidden size must be greater than 0.");
  }

  // Validate the weight operand.
  // TODO(crbug.com/331055053): Specify the operand data type constraints of
  // operation.
  if (!IsFloatingPointType(input.data_type())) {
    return base::unexpected(
        "The data type of input must be one of the floating point types.");
  }
  if (input.Rank() != 2) {
    return base::unexpected("The input must be a 2-D tensor.");
  }

  const uint32_t batch_size = input.shape()[0];
  const uint32_t input_size = input.shape()[1];
  auto checked_three_times_hidden_size = base::MakeCheckedNum(hidden_size) * 3;
  uint32_t three_times_hidden_size;
  if (!checked_three_times_hidden_size.AssignIfValid(
          &three_times_hidden_size)) {
    return base::unexpected("The hidden size is too large.");
  }

  // Validate the weight operand.
  std::array<uint32_t, 2> expected_weight_shape = {three_times_hidden_size,
                                                   input_size};
  RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
      weight, "weight", expected_weight_shape, input.data_type()));

  // Validate the recurrent weight operand.
  std::array<uint32_t, 2> expected_recurrent_weight_shape = {
      three_times_hidden_size, hidden_size};
  RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
      recurrent_weight, "recurrent weight", expected_recurrent_weight_shape,
      input.data_type()));

  // Validate the hidden state operand.
  std::array<uint32_t, 2> expected_hidden_state_shape = {batch_size,
                                                         hidden_size};
  RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(hidden_state, "hidden state",
                                                  expected_hidden_state_shape,
                                                  input.data_type()));

  // Validate the bias operand.
  std::array<uint32_t, 1> expected_bias_shape = {three_times_hidden_size};
  if (attributes.bias) {
    RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
        *attributes.bias, "bias", expected_bias_shape, input.data_type()));
  }

  // Validate the recurrent bias operand.
  std::array<uint32_t, 1> expected_recurrent_bias_shape = {
      three_times_hidden_size};
  if (attributes.recurrent_bias) {
    RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
        *attributes.recurrent_bias, "recurrent bias",
        expected_recurrent_bias_shape, input.data_type()));
  }

  if (attributes.activation_count != 2) {
    return base::unexpected("The number of activations must be 2.");
  }

  std::array<uint32_t, 2> output_shape{batch_size, hidden_size};
  return OperandDescriptor::Create(input.data_type(), output_shape);
}

InstanceNormalizationAttributes::InstanceNormalizationAttributes() = default;
InstanceNormalizationAttributes::~InstanceNormalizationAttributes() = default;

InstanceNormalizationAttributes::InstanceNormalizationAttributes(
    InstanceNormalizationAttributes&& other) = default;
InstanceNormalizationAttributes& InstanceNormalizationAttributes::operator=(
    InstanceNormalizationAttributes&& other) = default;

base::expected<OperandDescriptor, std::string>
ValidateInstanceNormalizationAndInferOutput(
    const OperandDescriptor& input,
    const InstanceNormalizationAttributes& attributes) {
  // Validate the input operand.
  if (!IsFloatingPointType(input.data_type())) {
    return base::unexpected(
        "The input type must be one of the floating point types.");
  }
  if (input.Rank() != 4) {
    return base::unexpected("The input should be a 4-D tensor.");
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
    const auto validation_scale =
        ValidateNormalizationOperandIsCompatibleWithInput(
            attributes.scale.value(), input.data_type(), input.shape()[axis]);
    if (!validation_scale.has_value()) {
      return base::unexpected("For scale operand: " + validation_scale.error());
    }
  }

  // Validate the bias operand.
  if (attributes.bias.has_value()) {
    const auto validation_bias =
        ValidateNormalizationOperandIsCompatibleWithInput(
            attributes.bias.value(), input.data_type(), input.shape()[axis]);
    if (!validation_bias.has_value()) {
      return base::unexpected("For bias operand: " + validation_bias.error());
    }
  }

  return input;
}

LayerNormalizationAttributes::LayerNormalizationAttributes() = default;
LayerNormalizationAttributes::~LayerNormalizationAttributes() = default;

LayerNormalizationAttributes::LayerNormalizationAttributes(
    LayerNormalizationAttributes&& other) = default;
LayerNormalizationAttributes& LayerNormalizationAttributes::operator=(
    LayerNormalizationAttributes&& other) = default;

base::expected<OperandDescriptor, std::string>
ValidateLayerNormalizationAndInferOutput(
    const OperandDescriptor& input,
    base::span<const uint32_t> axes,
    const LayerNormalizationAttributes& attributes) {
  // Validate the input operand.
  if (!IsFloatingPointType(input.data_type())) {
    return base::unexpected(
        "The input type must be one of the floating point types.");
  }

  // Ensure that the axes are all less than the input rank and have no
  // duplication.
  RETURN_IF_ERROR(ValidateAxes(axes, input.Rank()));

  const std::vector<uint32_t>& input_dimensions = input.shape();

  // The dimensions for layerNormalization to reduce along.
  std::vector<uint32_t> reduction_dimensions;
  reduction_dimensions.reserve(axes.size());
  base::ranges::transform(
      axes, std::back_inserter(reduction_dimensions),
      [&input_dimensions](uint32_t axis) { return input_dimensions[axis]; });

  // Validate the scale operand.
  if (attributes.scale.has_value()) {
    if (attributes.scale->data_type() != input.data_type()) {
      return base::unexpected(
          "For scale operand: the data type doesn't match the input data "
          "type.");
    }
    if (attributes.scale->shape() != reduction_dimensions) {
      return base::unexpected(
          "For scale operand: the shape doesn't match the axis dimensions of "
          "the input.");
    }
  }

  // Validate the bias operand.
  if (attributes.bias.has_value()) {
    if (attributes.bias->data_type() != input.data_type()) {
      return base::unexpected(
          "For bias operand: the data type doesn't match the input data type.");
    }
    if (attributes.bias->shape() != reduction_dimensions) {
      return base::unexpected(
          "For bias operand: the shape doesn't match the axis dimensions of "
          "the input.");
    }
  }

  return input;
}

LstmAttributes::LstmAttributes() = default;
LstmAttributes::~LstmAttributes() = default;

LstmAttributes::LstmAttributes(LstmAttributes&& other) = default;
LstmAttributes& LstmAttributes::operator=(LstmAttributes&& other) = default;

base::expected<std::vector<OperandDescriptor>, std::string>
ValidateLstmAndInferOutput(const OperandDescriptor& input,
                           const OperandDescriptor& weight,
                           const OperandDescriptor& recurrent_weight,
                           const uint32_t steps,
                           const uint32_t hidden_size,
                           const LstmAttributes& attributes) {
  if (steps <= 0) {
    return base::unexpected("The steps must be greater than 0.");
  }
  if (hidden_size <= 0) {
    return base::unexpected("The hidden size must be greater than 0.");
  }

  uint32_t four_times_hidden_size;
  auto checked_four_times_hidden_size = base::MakeCheckedNum(hidden_size) * 4;
  if (!checked_four_times_hidden_size.AssignIfValid(&four_times_hidden_size)) {
    return base::unexpected("The hidden size is too large.");
  }

  if (input.Rank() != 3) {
    return base::unexpected("The input should be a 3-D tensor.");
  }

  const auto& input_dimensions = input.shape();
  if (input_dimensions[0] != steps) {
    return base::unexpected(
        "The input dimensions[0] must be equal to the steps.");
  }
  // The current spec doesn't specify the operand data type constraints of
  // lstm. An issue has been filed to track it:
  // https://github.com/webmachinelearning/webnn/issues/283.
  if (!IsFloatingPointType(input.data_type())) {
    return base::unexpected(
        "The data type of input must be one of the floating point types.");
  }

  const uint32_t batch_size = input_dimensions[1];
  const uint32_t input_size = input_dimensions[2];
  const uint32_t direction_count =
      attributes.direction == RecurrentNetworkDirection::kBoth ? 2 : 1;

  // Validate the weight operand.
  uint32_t expected_weight_shape[3] = {direction_count, four_times_hidden_size,
                                       input_size};
  RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
      weight, "weight", expected_weight_shape, input.data_type()));

  // Validate the recurrent weight operand.
  uint32_t expected_recurrent_weight_shape[3] = {
      direction_count, four_times_hidden_size, hidden_size};
  RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
      recurrent_weight, "recurrent weight", expected_recurrent_weight_shape,
      input.data_type()));

  // Validate the bias operand.
  if (attributes.bias) {
    uint32_t expected_bias_shape[2] = {direction_count, four_times_hidden_size};
    RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(attributes.bias.value(),
                                                    "bias", expected_bias_shape,
                                                    input.data_type()));
  }

  // Validate the recurrent bias operand.
  if (attributes.recurrent_bias) {
    uint32_t expected_recurrent_bias_shape[2] = {direction_count,
                                                 four_times_hidden_size};
    RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
        attributes.recurrent_bias.value(), "recurrent bias",
        expected_recurrent_bias_shape, input.data_type()));
  }

  // Validate the peephole weight operand.
  if (attributes.peephole_weight) {
    // Here `3 * hidden_size` will not overflow because `4 * hidden_size` has
    // already been checked.
    uint32_t expected_peephole_weight_shape[2] = {direction_count,
                                                  3 * hidden_size};
    RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
        attributes.peephole_weight.value(), "peephole weight",
        expected_peephole_weight_shape, input.data_type()));
  }

  // Validate the initial hidden state operand.
  if (attributes.initial_hidden_state) {
    uint32_t expected_initial_hidden_state_shape[3] = {direction_count,
                                                       batch_size, hidden_size};
    RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
        attributes.initial_hidden_state.value(), "initial hidden state",
        expected_initial_hidden_state_shape, input.data_type()));
  }

  // Validate the initial cell state operand.
  if (attributes.initial_cell_state) {
    uint32_t expected_initial_cell_state_shape[3] = {direction_count,
                                                     batch_size, hidden_size};
    RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
        attributes.initial_cell_state.value(), "initial cell state",
        expected_initial_cell_state_shape, input.data_type()));
  }

  if (attributes.activation_count != 3) {
    return base::unexpected(
        "The activations should be a sequence of length 3.");
  }

  std::vector<OperandDescriptor> outputs;
  ASSIGN_OR_RETURN(OperandDescriptor output,
                   OperandDescriptor::Create(
                       input.data_type(),
                       std::array{direction_count, batch_size, hidden_size}));
  outputs.push_back(output);
  outputs.push_back(std::move(output));
  if (attributes.return_sequence) {
    ASSIGN_OR_RETURN(
        OperandDescriptor return_sequence_output,
        OperandDescriptor::Create(
            input.data_type(),
            std::array{steps, direction_count, batch_size, hidden_size}));
    outputs.push_back(std::move(return_sequence_output));
  }

  return outputs;
}

LstmCellAttributes::LstmCellAttributes() = default;
LstmCellAttributes::~LstmCellAttributes() = default;

LstmCellAttributes::LstmCellAttributes(LstmCellAttributes&& other) = default;
LstmCellAttributes& LstmCellAttributes::operator=(LstmCellAttributes&& other) =
    default;

base::expected<std::vector<OperandDescriptor>, std::string>
ValidateLstmCellAndInferOutput(const OperandDescriptor& input,
                               const OperandDescriptor& weight,
                               const OperandDescriptor& recurrent_weight,
                               const OperandDescriptor& hidden_state,
                               const OperandDescriptor& cell_state,
                               const uint32_t hidden_size,
                               const LstmCellAttributes& attributes) {
  if (hidden_size <= 0) {
    return base::unexpected("The hidden size must be greater than 0.");
  }

  uint32_t four_times_hidden_size;
  auto checked_four_times_hidden_size = base::MakeCheckedNum(hidden_size) * 4;
  if (!checked_four_times_hidden_size.AssignIfValid(&four_times_hidden_size)) {
    return base::unexpected("The hidden size is too large.");
  }

  if (input.Rank() != 2) {
    return base::unexpected("The input should be a 2-D tensor.");
  }

  // TODO(crbug.com/331055053): The current spec doesn't specify the operand
  // data type constraints of lstm.
  if (!IsFloatingPointType(input.data_type())) {
    return base::unexpected(
        "The data type of input must be one of the floating point types.");
  }

  const uint32_t batch_size = input.shape()[0];
  const uint32_t input_size = input.shape()[1];

  // Validate the weight operand.
  std::array<uint32_t, 2> expected_weight_shape = {four_times_hidden_size,
                                                   input_size};
  RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
      weight, "weight", expected_weight_shape, input.data_type()));

  // Validate the hidden state operand.
  std::array<uint32_t, 2> expected_hidden_state_shape = {batch_size,
                                                         hidden_size};
  RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(hidden_state, "hidden state",
                                                  expected_hidden_state_shape,
                                                  input.data_type()));

  // Validate the cell state operand.
  std::array<uint32_t, 2> expected_cell_state_shape = {batch_size, hidden_size};
  RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
      cell_state, "cell state", expected_cell_state_shape, input.data_type()));

  // Validate the recurrent weight operand.
  std::array<uint32_t, 2> expected_recurrent_weight_shape = {
      four_times_hidden_size, hidden_size};
  RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
      recurrent_weight, "recurrent weight", expected_recurrent_weight_shape,
      input.data_type()));

  // Validate the bias operand.
  if (attributes.bias) {
    std::array<uint32_t, 1> expected_bias_shape = {four_times_hidden_size};
    RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(attributes.bias.value(),
                                                    "bias", expected_bias_shape,
                                                    input.data_type()));
  }

  // Validate the recurrent bias operand.
  if (attributes.recurrent_bias) {
    std::array<uint32_t, 1> expected_recurrent_bias_shape = {
        four_times_hidden_size};
    RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
        attributes.recurrent_bias.value(), "recurrent bias",
        expected_recurrent_bias_shape, input.data_type()));
  }

  // Validate the peephole weight operand.
  if (attributes.peephole_weight) {
    // Here `3 * hidden_size` will not overflow because `4 * hidden_size` has
    // already been checked.
    std::array<uint32_t, 1> expected_peephole_weight_shape = {3 * hidden_size};
    RETURN_IF_ERROR(ValidateRecurrentNetworkOperand(
        attributes.peephole_weight.value(), "peephole weight",
        expected_peephole_weight_shape, input.data_type()));
  }

  if (attributes.activation_count != 3) {
    return base::unexpected(
        "The activations should be a sequence of length 3.");
  }

  std::vector<OperandDescriptor> outputs;
  outputs.reserve(2);

  ASSIGN_OR_RETURN(OperandDescriptor output,
                   OperandDescriptor::Create(
                       input.data_type(), std::array{batch_size, hidden_size}));
  outputs.push_back(output);
  outputs.push_back(std::move(output));

  return outputs;
}

base::expected<OperandDescriptor, std::string> ValidateConcatAndInferOutput(
    const ContextProperties& context_properties,
    const std::vector<OperandDescriptor>& inputs,
    const uint32_t axis) {
  if (inputs.empty()) {
    return base::unexpected("The inputs should not be empty.");
  }
  const std::vector<uint32_t>& first_input_shape = inputs[0].shape();
  const auto first_input_rank = inputs[0].Rank();
  // According to WebNN spec:
  // https://www.w3.org/TR/webnn/#dom-mlgraphbuilder-concat-inputs-axis-axis,
  // the axis that the inputs concatenate along, with the value in the interval
  // [0, N-1] where N is the rank of input tensors. We just check the first
  // input rank here because we will check all inputs have same rank in the
  // following loop.
  if (axis >= first_input_rank) {
    return base::unexpected(
        "The axis must be in the range [0, N-1] where N is the rank of input "
        "tensor.");
  }

  const auto output_type = inputs[0].data_type();

  static constexpr char kInputsParam[] = "inputs";
  if (!context_properties.data_type_limits.concat_inputs.Has(output_type)) {
    return base::unexpected(NotSupportedArgumentTypeError(
        kInputsParam, output_type,
        context_properties.data_type_limits.concat_inputs));
  }

  // The loop skips the first input to avoid repeated checks.
  for (size_t i = 1; i < inputs.size(); ++i) {
    if (inputs[i].data_type() != output_type) {
      return base::unexpected("The input data types don't match.");
    }
    // According to WebNN spec:
    // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-concat, all input tensors
    // must have the same dimension.
    if (inputs[i].Rank() != first_input_rank) {
      return base::unexpected(
          "All input tensors must have the same dimension.");
    }
    // According to WebNN spec:
    // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-concat, all input tensors
    // must have the same shape, except for the size of the dimension to
    // concatenate on.
    for (size_t dim = 0; dim < first_input_rank; ++dim) {
      if (dim == axis || inputs[i].shape()[dim] == first_input_shape[dim]) {
        continue;
      }
      return base::unexpected(
          "All input tensors must have the same shape, except for the size of "
          "the dimension to concatenate on.");
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
    return base::unexpected("The concatenated dimension size is too large.");
  }

  return OperandDescriptor::Create(output_type, output_shape);
}

base::expected<OperandDescriptor, std::string> ValidatePreluAndInferOutput(
    const OperandDescriptor& input,
    const OperandDescriptor& slope,
    std::string_view label) {
  if (!IsFloatingPointType(input.data_type()) &&
      input.data_type() != OperandDataType::kInt8 &&
      input.data_type() != OperandDataType::kInt32) {
    return base::unexpected(ErrorWithLabel(
        label,
        "The data type of input and slope must be one of {float32, float16, "
        "int32, int8}."));
  }
  if (input.data_type() != slope.data_type()) {
    return base::unexpected(ErrorWithLabel(
        label, "The data type of slope doesn't match the data type of input."));
  }
  // BroadcastShape unidirectionally broadcasts slope.dimensions to
  // input.dimensions.
  if (!BroadcastShapes(slope.shape(), input.shape(), /*bidirectional=*/false)) {
    return base::unexpected(ErrorWithLabel(
        label,
        "The shape of slope is not broadcastable to the shape of input."));
  }

  return input;
}

base::expected<OperandDescriptor, std::string> ValidateTransposeAndInferOutput(
    const OperandDescriptor& input,
    base::span<const uint32_t> permutation) {
  if (permutation.size() != input.Rank()) {
    return base::unexpected(
        "The number of values in permutation must be the same as the rank of "
        "the input tensor.");
  }
  RETURN_IF_ERROR(ValidateAxes(permutation, input.Rank()));

  std::vector<uint32_t> output_shape(input.Rank());
  for (size_t i = 0; i < input.Rank(); ++i) {
    output_shape[i] = input.shape()[permutation[i]];
  }
  return OperandDescriptor::Create(input.data_type(), std::move(output_shape));
}

SliceAttributes::SliceAttributes() = default;
SliceAttributes::~SliceAttributes() = default;

SliceAttributes::SliceAttributes(SliceAttributes&& other) = default;
SliceAttributes& SliceAttributes::operator=(SliceAttributes&& other) = default;

base::expected<OperandDescriptor, std::string> ValidateSliceAndInferOutput(
    const OperandDescriptor& input,
    const SliceAttributes& attributes) {
  const auto input_rank = input.Rank();
  if (input_rank == 0) {
    return base::unexpected("The input should not be a scalar.");
  }

  if (attributes.starts.size() != input_rank) {
    return base::unexpected(
        "The length of starts must be equal to the rank of the input tensor.");
  }

  if (attributes.sizes.size() != input_rank) {
    return base::unexpected(
        "The length of sizes must be equal to the rank of the input tensor.");
  }

  for (uint32_t i = 0; i < input_rank; ++i) {
    if (attributes.starts[i] >= input.shape()[i]) {
      return base::unexpected(base::StringPrintf(
          "For dimension (%u): the starting index to slice must "
          "be less than input size (%u).",
          i, input.shape()[i]));
    }

    // WebNN plans to allow 0 size dimensions and an issue has been filed to
    // track it: https://github.com/webmachinelearning/webnn/issues/391.
    if (attributes.sizes[i] == 0) {
      return base::unexpected(base::StringPrintf(
          "For dimension (%u): the number of elements to slice "
          "must not be 0.",
          i));
    }

    auto checked_ending_index =
        base::MakeCheckedNum<uint32_t>(attributes.starts[i]) +
        attributes.sizes[i];
    if (!checked_ending_index.IsValid<uint32_t>()) {
      return base::unexpected(base::StringPrintf(
          "For dimension (%u): the ending index to slice is too large.", i));
    }

    if (checked_ending_index.ValueOrDie() > input.shape()[i]) {
      return base::unexpected(
          base::StringPrintf("For dimension (%u): the ending index to slice "
                             "must not be greater than input size (%u).",
                             i, input.shape()[i]));
    }
  }

  // The output is a tensor the same as the specified slice sizes.
  std::vector<uint32_t> output_shape;
  output_shape.assign(attributes.sizes.begin(), attributes.sizes.end());
  return OperandDescriptor::Create(input.data_type(), std::move(output_shape));
}

base::expected<OperandDescriptor, std::string> ValidateReduceAndInferOutput(
    ReduceKind kind,
    const OperandDescriptor& input,
    base::span<const uint32_t> axes,
    bool keep_dimensions) {
  switch (kind) {
    case ReduceKind::kL2:
    case ReduceKind::kMean:
    case ReduceKind::kLogSum:
    case ReduceKind::kLogSumExp: {
      if (!IsFloatingPointType(input.data_type())) {
        return base::unexpected(
            "The input data type must be one of the floating point types.");
      }
      break;
    }
    case ReduceKind::kL1:
    case ReduceKind::kProduct:
    case ReduceKind::kSum:
    case ReduceKind::kSumSquare: {
      if (!IsFloatingPointType(input.data_type()) &&
          input.data_type() != OperandDataType::kInt32 &&
          input.data_type() != OperandDataType::kUint32 &&
          input.data_type() != OperandDataType::kInt64 &&
          input.data_type() != OperandDataType::kUint64) {
        return base::unexpected(
            "The input data type must be one of {float32, float16, int32, "
            "uint32, int64, uint64}.");
      }
      break;
    }
    case ReduceKind::kMax:
    case ReduceKind::kMin:
      break;
  }

  ASSIGN_OR_RETURN(
      std::vector<uint32_t> output_shape,
      ValidateReduceAxesAndInferOutput(input.shape(), axes, keep_dimensions));

  return OperandDescriptor::Create(input.data_type(), output_shape);
}

base::expected<OperandDescriptor, std::string> ValidateTriangularAndInferOutput(
    const OperandDescriptor& input) {
  // According to WebNN spec:
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-triangular, the input
  // tensor which is at least 2-D.
  if (input.Rank() < 2) {
    return base::unexpected(
        "The input rank must be larger than or equal to 2.");
  }

  // The output tensor of triangular is the same shape and the same type as the
  // input tensor.
  return input;
}

base::expected<OperandDescriptor, std::string> ValidateWhereAndInferOutput(
    const ContextProperties& context_properties,
    const OperandDescriptor& condition,
    const OperandDescriptor& true_value,
    const OperandDescriptor& false_value) {
  static constexpr char kConditionParam[] = "condition";
  if (!context_properties.data_type_limits.where_condition.Has(
          condition.data_type())) {
    return base::unexpected(NotSupportedArgumentTypeError(
        kConditionParam, condition.data_type(),
        context_properties.data_type_limits.where_condition));
  }

  static constexpr char kTrueValueParam[] = "trueValue";
  if (!context_properties.data_type_limits.where_true_value.Has(
          true_value.data_type())) {
    return base::unexpected(NotSupportedArgumentTypeError(
        kTrueValueParam, true_value.data_type(),
        context_properties.data_type_limits.where_true_value));
  }

  static constexpr char kFalseValueParam[] = "falseValue";
  if (!context_properties.data_type_limits.where_false_value.Has(
          false_value.data_type())) {
    return base::unexpected(NotSupportedArgumentTypeError(
        kFalseValueParam, false_value.data_type(),
        context_properties.data_type_limits.where_false_value));
  }

  if (true_value.data_type() != false_value.data_type()) {
    return base::unexpected(
        "The data types of trueValue and falseValue don't match.");
  }

  const std::optional<std::vector<uint32_t>> value_shape = BroadcastShapes(
      true_value.shape(), false_value.shape(), /*bidirectional=*/true);
  if (!value_shape) {
    return base::unexpected(
        "The shapes of trueValue and falseValue are not broadcastable.");
  }

  std::optional<std::vector<uint32_t>> output_shape = BroadcastShapes(
      condition.shape(), value_shape.value(), /*bidirectional=*/true);
  if (!output_shape) {
    return base::unexpected(
        "The condition shape is not broadcastable to the shape broadcasted "
        "from trueValue and falseValue.");
  }
  return OperandDescriptor::Create(true_value.data_type(),
                                   *std::move(output_shape));
}

base::expected<void, std::string> ValidateAxes(base::span<const uint32_t> axes,
                                               const size_t rank) {
  if (base::ranges::any_of(axes, [rank](uint32_t axis) {
        return base::MakeStrictNum(axis) >= rank;
      })) {
    return base::unexpected(base::StringPrintf(
        "The values in axes must be in the range [0, %zu).", rank));
  }

  if (axes.size() != std::set<uint32_t>(axes.begin(), axes.end()).size()) {
    return base::unexpected(
        "Two or more values are same in the axes sequence.");
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

bool IsFloatingPointType(OperandDataType data_type) {
  return DataTypeConstraint::kFloat.Has(data_type);
}

bool IsDepthwiseConv2d(uint32_t input_channels,
                       uint32_t output_channels,
                       uint32_t groups) {
  return groups == input_channels && groups == output_channels && groups != 1;
}

}  // namespace webnn
