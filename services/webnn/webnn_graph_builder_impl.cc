// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_graph_builder_impl.h"

#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/memory/stack_allocated.h"
#include "base/types/pass_key.h"
#include "services/webnn/error.h"
#include "services/webnn/public/cpp/graph_validation_utils.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"
#include "services/webnn/webnn_utils.h"

// Evaluate `condition`, and if it returns false then return false.
#define RETURN_IF_FALSE(condition) \
  do {                             \
    if (!(condition))              \
      return false;                \
  } while (0)

namespace webnn {

namespace {

// Maps the id to its `mojo::Operand`.
using IdToOperandMap = base::flat_map<uint64_t, mojom::OperandPtr>;

using DependentOperationsMap = base::flat_map<uint64_t, base::flat_set<size_t>>;

webnn::InputOperandLayout MojoInputOperandLayoutToComponent(
    webnn::mojom::InputOperandLayout layout) {
  switch (layout) {
    case webnn::mojom::InputOperandLayout::kChannelsFirst:
      return webnn::InputOperandLayout::kNchw;
    case webnn::mojom::InputOperandLayout::kChannelsLast:
      return webnn::InputOperandLayout::kNhwc;
  }
}

webnn::Pool2dKind FromMojoPool2dType(mojom::Pool2d::Kind kind) {
  switch (kind) {
    case mojom::Pool2d::Kind::kAveragePool2d:
      return webnn::Pool2dKind::kAverage;
    case mojom::Pool2d::Kind::kL2Pool2d:
      return webnn::Pool2dKind::kL2;
    case mojom::Pool2d::Kind::kMaxPool2d:
      return webnn::Pool2dKind::kMax;
  }
}

webnn::ReduceKind MojoReduceTypeToComponent(mojom::Reduce::Kind kind) {
  switch (kind) {
    case mojom::Reduce::Kind::kL1:
      return webnn::ReduceKind::kL1;
    case mojom::Reduce::Kind::kL2:
      return webnn::ReduceKind::kL2;
    case mojom::Reduce::Kind::kLogSum:
      return webnn::ReduceKind::kLogSum;
    case mojom::Reduce::Kind::kLogSumExp:
      return webnn::ReduceKind::kLogSumExp;
    case mojom::Reduce::Kind::kMax:
      return webnn::ReduceKind::kMax;
    case mojom::Reduce::Kind::kMean:
      return webnn::ReduceKind::kMean;
    case mojom::Reduce::Kind::kMin:
      return webnn::ReduceKind::kMin;
    case mojom::Reduce::Kind::kProduct:
      return webnn::ReduceKind::kProduct;
    case mojom::Reduce::Kind::kSum:
      return webnn::ReduceKind::kSum;
    case mojom::Reduce::Kind::kSumSquare:
      return webnn::ReduceKind::kSumSquare;
  }
}

webnn::RecurrentNetworkDirection MojoRecurrentNetworkDirectionToComponent(
    mojom::RecurrentNetworkDirection direction) {
  switch (direction) {
    case mojom::RecurrentNetworkDirection::kForward:
      return webnn::RecurrentNetworkDirection::kForward;
    case mojom::RecurrentNetworkDirection::kBackward:
      return webnn::RecurrentNetworkDirection::kBackward;
    case mojom::RecurrentNetworkDirection::kBoth:
      return webnn::RecurrentNetworkDirection::kBoth;
  }
}

bool ValidateClampAttributes(const mojom::Clamp& clamp) {
  if (std::isnan(clamp.min_value) || std::isnan(clamp.max_value)) {
    // The min or max value are nan.
    return false;
  }
  if (clamp.min_value >= clamp.max_value) {
    // The min value must be below the max value.
    return false;
  }
  return true;
}

bool ValidateEluAttributes(const mojom::Elu& elu) {
  if (std::isnan(elu.alpha) || elu.alpha <= 0.0f) {
    // The value of alpha must be greater than 0.
    return false;
  }

  return true;
}

bool ValidateHardSigmoidAttributes(const mojom::HardSigmoid& hard_sigmoid) {
  if (std::isnan(hard_sigmoid.alpha) || std::isnan(hard_sigmoid.beta)) {
    // The value of alpha and beta should not be NAN.
    return false;
  }

  return true;
}

bool ValidateLeakyReluAttributes(const mojom::LeakyRelu& leaky_relu) {
  if (std::isnan(leaky_relu.alpha)) {
    // The value of alpha should not be NAN.
    return false;
  }

  return true;
}

bool ValidateLinearAttributes(const mojom::Linear& linear) {
  if (std::isnan(linear.alpha) || std::isnan(linear.beta)) {
    // The values of alpha and beta should not be NAN.
    return false;
  }

  return true;
}

const mojom::Operand* GetMojoOperand(const IdToOperandMap& id_to_operand_map,
                                     uint64_t operand_id) {
  const auto operand_iterator = id_to_operand_map.find(operand_id);
  if (operand_iterator == id_to_operand_map.end()) {
    // There is no operand for the id.
    return nullptr;
  }
  return operand_iterator->second.get();
}

webnn::BatchNormalizationAttributes ConvertToBatchNormalizationAttributes(
    const IdToOperandMap& id_to_operand_map,
    const mojom::BatchNormalization& batch_normalization) {
  webnn::BatchNormalizationAttributes component_attributes;
  const auto& scale_operand_id = batch_normalization.scale_operand_id;
  if (scale_operand_id) {
    const mojom::Operand& scale_operand =
        *id_to_operand_map.at(scale_operand_id.value());
    component_attributes.scale = scale_operand.descriptor;
  }
  const auto& bias_operand_id = batch_normalization.bias_operand_id;
  if (bias_operand_id) {
    const mojom::Operand& bias_operand =
        *id_to_operand_map.at(bias_operand_id.value());
    component_attributes.bias = bias_operand.descriptor;
  }
  component_attributes.axis = batch_normalization.axis;
  component_attributes.label = batch_normalization.label;

  return component_attributes;
}

template <typename Conv2dAttributesType>
Conv2dAttributesType ConvertToConv2dAttributes(
    const webnn::ContextProperties& context_properties,
    const IdToOperandMap& id_to_operand_map,
    const webnn::mojom::Conv2d& conv2d,
    std::optional<OperandDescriptor> bias_operand) {
  Conv2dAttributesType attributes_base;
  // Convert padding, strides, dilations.
  auto& mojo_padding = conv2d.padding;
  attributes_base.padding = webnn::Padding2d{
      .beginning =
          webnn::Size2d<uint32_t>{.height = mojo_padding->beginning->height,
                                  .width = mojo_padding->beginning->width},
      .ending = webnn::Size2d<uint32_t>{.height = mojo_padding->ending->height,
                                        .width = mojo_padding->ending->width}};
  attributes_base.strides = webnn::Size2d<uint32_t>{
      .height = conv2d.strides->height, .width = conv2d.strides->width};
  attributes_base.dilations = webnn::Size2d<uint32_t>{
      .height = conv2d.dilations->height, .width = conv2d.dilations->width};

  // Convert groups, input layout and bias.
  attributes_base.groups = conv2d.groups;
  attributes_base.input_layout = context_properties.input_operand_layout;
  attributes_base.bias_operand = std::move(bias_operand);

  attributes_base.label = conv2d.label;

  return std::move(attributes_base);
}

webnn::Conv2dAttributes ConvertToConv2dAttributes(
    const webnn::ContextProperties& context_properties,
    const IdToOperandMap& id_to_operand_map,
    const webnn::mojom::Conv2d& conv2d,
    std::optional<OperandDescriptor> bias_operand) {
  auto component_attributes =
      ConvertToConv2dAttributes<webnn::Conv2dAttributes>(
          context_properties, id_to_operand_map, conv2d,
          std::move(bias_operand));
  switch (context_properties.input_operand_layout) {
    case webnn::InputOperandLayout::kNchw:
      // "channelsFirst": [batches, input_channels, height, width]
      component_attributes.filter_layout = Conv2dFilterOperandLayout::kOihw;
      break;
    case webnn::InputOperandLayout::kNhwc:
      // "channelsLast": [batches, height, width, input_channels]
      // For regular conv2d, ohwi filter layout is expected by default.
      // For depthwise conv2d, ihwo filter layout is expected by default.
      const auto* const input =
          GetMojoOperand(id_to_operand_map, conv2d.input_operand_id);
      CHECK(input);
      CHECK_EQ(input->descriptor.Rank(), 4u);
      const uint32_t input_channels = input->descriptor.shape()[3];
      const auto* const output =
          GetMojoOperand(id_to_operand_map, conv2d.output_operand_id);
      CHECK(output);
      CHECK_EQ(output->descriptor.Rank(), 4u);
      const uint32_t output_channels = output->descriptor.shape()[3];
      // Depthwise conv2d is "options.groups == input_channels ==
      // output_channels".
      const bool depthwise = webnn::IsDepthwiseConv2d(
          input_channels, output_channels, conv2d.groups);
      component_attributes.filter_layout =
          depthwise ? Conv2dFilterOperandLayout::kIhwo
                    : Conv2dFilterOperandLayout::kOhwi;
      break;
  }
  return component_attributes;
}

webnn::LstmAttributes ConvertToLstmAttributes(
    const IdToOperandMap& id_to_operand_map,
    const webnn::mojom::Lstm& lstm) {
  webnn::LstmAttributes attributes;
  attributes.return_sequence = lstm.return_sequence;
  attributes.direction =
      MojoRecurrentNetworkDirectionToComponent(lstm.direction);
  attributes.activation_count = lstm.activations.size();

  if (lstm.bias_operand_id.has_value()) {
    const auto* bias =
        GetMojoOperand(id_to_operand_map, lstm.bias_operand_id.value());
    attributes.bias = bias->descriptor;
  }
  if (lstm.recurrent_bias_operand_id.has_value()) {
    const auto* recurrent_bias = GetMojoOperand(
        id_to_operand_map, lstm.recurrent_bias_operand_id.value());
    attributes.recurrent_bias = recurrent_bias->descriptor;
  }
  if (lstm.peephole_weight_operand_id.has_value()) {
    const auto* peephole_weight = GetMojoOperand(
        id_to_operand_map, lstm.peephole_weight_operand_id.value());
    attributes.peephole_weight = peephole_weight->descriptor;
  }
  if (lstm.initial_hidden_state_operand_id.has_value()) {
    const auto* initial_hidden_state = GetMojoOperand(
        id_to_operand_map, lstm.initial_hidden_state_operand_id.value());
    attributes.initial_hidden_state = initial_hidden_state->descriptor;
  }
  if (lstm.initial_cell_state_operand_id.has_value()) {
    const auto* initial_cell_state = GetMojoOperand(
        id_to_operand_map, lstm.initial_cell_state_operand_id.value());
    attributes.initial_cell_state = initial_cell_state->descriptor;
  }
  attributes.label = lstm.label;

  return attributes;
}

webnn::LstmCellAttributes ConvertToLstmCellAttributes(
    const IdToOperandMap& id_to_operand_map,
    const webnn::mojom::LstmCell& lstm_cell) {
  webnn::LstmCellAttributes attributes;
  attributes.activation_count = lstm_cell.activations.size();

  if (lstm_cell.bias_operand_id.has_value()) {
    const auto* bias =
        GetMojoOperand(id_to_operand_map, lstm_cell.bias_operand_id.value());
    attributes.bias = bias->descriptor;
  }
  if (lstm_cell.recurrent_bias_operand_id.has_value()) {
    const auto* recurrent_bias = GetMojoOperand(
        id_to_operand_map, lstm_cell.recurrent_bias_operand_id.value());
    attributes.recurrent_bias = recurrent_bias->descriptor;
  }
  if (lstm_cell.peephole_weight_operand_id.has_value()) {
    const auto* peephole_weight = GetMojoOperand(
        id_to_operand_map, lstm_cell.peephole_weight_operand_id.value());
    attributes.peephole_weight = peephole_weight->descriptor;
  }
  attributes.label = lstm_cell.label;

  return attributes;
}

webnn::ConvTranspose2dAttributes ConvertToConvTranspose2dAttributes(
    const webnn::ContextProperties& context_properties,
    const IdToOperandMap& id_to_operand_map,
    const webnn::mojom::Conv2d& conv2d,
    std::optional<OperandDescriptor> bias_operand) {
  auto component_attributes =
      ConvertToConv2dAttributes<webnn::ConvTranspose2dAttributes>(
          context_properties, id_to_operand_map, conv2d,
          std::move(bias_operand));

  // Convert the output sizes that fetched from dimensions of output operand.
  auto* output = GetMojoOperand(id_to_operand_map, conv2d.output_operand_id);
  CHECK_EQ(output->descriptor.Rank(), 4u);
  webnn::Size2d<uint32_t> output_sizes;
  switch (context_properties.input_operand_layout) {
    case webnn::InputOperandLayout::kNchw:
      // "channelsFirst": [batches, input_channels, height, width]
      output_sizes.height = output->descriptor.shape()[2];
      output_sizes.width = output->descriptor.shape()[3];
      component_attributes.filter_layout =
          ConvTranspose2dFilterOperandLayout::kIohw;
      break;
    case webnn::InputOperandLayout::kNhwc:
      // "channelsLast": [batches, height, width, input_channels]
      output_sizes.height = output->descriptor.shape()[1];
      output_sizes.width = output->descriptor.shape()[2];
      component_attributes.filter_layout =
          ConvTranspose2dFilterOperandLayout::kOhwi;
      break;
  }
  component_attributes.output_sizes = std::move(output_sizes);

  return component_attributes;
}

webnn::LayerNormalizationAttributes ConvertToLayerNormalizationAttributes(
    const IdToOperandMap& id_to_operand_map,
    const mojom::LayerNormalization& layer_normalization) {
  webnn::LayerNormalizationAttributes component_attributes;
  const auto& scale_operand_id = layer_normalization.scale_operand_id;
  if (scale_operand_id.has_value()) {
    const mojom::Operand& scale_operand =
        *id_to_operand_map.at(scale_operand_id.value());
    component_attributes.scale = scale_operand.descriptor;
  }

  const auto& bias_operand_id = layer_normalization.bias_operand_id;
  if (bias_operand_id.has_value()) {
    const mojom::Operand& bias_operand =
        *id_to_operand_map.at(bias_operand_id.value());
    component_attributes.bias = bias_operand.descriptor;
  }
  component_attributes.label = layer_normalization.label;

  return component_attributes;
}

webnn::Pool2dAttributes ConvertToPool2dAttributes(
    const webnn::ContextProperties& context_properties,
    const webnn::mojom::Pool2d& pool2d,
    const mojom::Operand* output) {
  webnn::Pool2dAttributes component_attributes;
  auto& window_dimensions = pool2d.window_dimensions;
  component_attributes.window_dimensions = webnn::Size2d<uint32_t>{
      .height = window_dimensions->height, .width = window_dimensions->width};
  auto& mojo_padding = pool2d.padding;
  component_attributes.padding = webnn::Padding2d{
      .beginning =
          webnn::Size2d<uint32_t>{.height = mojo_padding->beginning->height,
                                  .width = mojo_padding->beginning->width},
      .ending = webnn::Size2d<uint32_t>{.height = mojo_padding->ending->height,
                                        .width = mojo_padding->ending->width}};
  component_attributes.strides = webnn::Size2d<uint32_t>{
      .height = pool2d.strides->height, .width = pool2d.strides->width};
  component_attributes.dilations = webnn::Size2d<uint32_t>{
      .height = pool2d.dilations->height, .width = pool2d.dilations->width};
  component_attributes.layout = context_properties.input_operand_layout;
  CHECK_EQ(output->descriptor.Rank(), 4u);
  switch (component_attributes.layout) {
    case webnn::InputOperandLayout::kNchw:
      component_attributes.output_sizes =
          webnn::Size2d<uint32_t>{.height = output->descriptor.shape()[2],
                                  .width = output->descriptor.shape()[3]};
      break;
    case webnn::InputOperandLayout::kNhwc:
      component_attributes.output_sizes =
          webnn::Size2d<uint32_t>{.height = output->descriptor.shape()[1],
                                  .width = output->descriptor.shape()[2]};
      break;
  }
  component_attributes.label = pool2d.label;

  return component_attributes;
}

webnn::GemmAttributes ConvertToGemmAttributes(
    const IdToOperandMap& id_to_operand_map,
    const mojom::Gemm& gemm) {
  webnn::GemmAttributes component_attributes;
  auto& c_operand_id = gemm.c_operand_id;
  if (c_operand_id) {
    const mojom::Operand& c_operand =
        *id_to_operand_map.at(c_operand_id.value());
    component_attributes.c_operand = c_operand.descriptor;
  }
  component_attributes.alpha = gemm.alpha;
  component_attributes.beta = gemm.beta;
  component_attributes.a_transpose = gemm.a_transpose;
  component_attributes.b_transpose = gemm.b_transpose;
  component_attributes.label = gemm.label;

  return component_attributes;
}

webnn::GruAttributes ConvertToGruAttributes(
    const IdToOperandMap& id_to_operand_map,
    const webnn::mojom::Gru& gru) {
  webnn::GruAttributes component_attributes;
  if (gru.bias_operand_id.has_value()) {
    const auto* bias =
        GetMojoOperand(id_to_operand_map, gru.bias_operand_id.value());
    component_attributes.bias = bias->descriptor;
  }
  if (gru.recurrent_bias_operand_id.has_value()) {
    const auto* recurrent_bias = GetMojoOperand(
        id_to_operand_map, gru.recurrent_bias_operand_id.value());
    component_attributes.recurrent_bias = recurrent_bias->descriptor;
  }
  if (gru.initial_hidden_state_operand_id.has_value()) {
    const auto* initial_hidden_state = GetMojoOperand(
        id_to_operand_map, gru.initial_hidden_state_operand_id.value());
    component_attributes.initial_hidden_state =
        initial_hidden_state->descriptor;
  }

  component_attributes.return_sequence = gru.return_sequence;
  component_attributes.direction =
      MojoRecurrentNetworkDirectionToComponent(gru.direction);
  component_attributes.activation_count = gru.activations.size();
  component_attributes.label = gru.label;

  return component_attributes;
}

webnn::GruCellAttributes ConvertToGruCellAttributes(
    const IdToOperandMap& id_to_operand_map,
    const webnn::mojom::GruCell& gru_cell) {
  webnn::GruCellAttributes component_attributes;
  if (gru_cell.bias_operand_id.has_value()) {
    const auto* bias =
        GetMojoOperand(id_to_operand_map, gru_cell.bias_operand_id.value());
    component_attributes.bias = bias->descriptor;
  }
  if (gru_cell.recurrent_bias_operand_id.has_value()) {
    const auto* recurrent_bias = GetMojoOperand(
        id_to_operand_map, gru_cell.recurrent_bias_operand_id.value());
    component_attributes.recurrent_bias = recurrent_bias->descriptor;
  }
  component_attributes.activation_count = gru_cell.activations.size();
  component_attributes.label = gru_cell.label;

  return component_attributes;
}

webnn::InstanceNormalizationAttributes ConvertToInstanceNormalizationAttributes(
    const IdToOperandMap& id_to_operand_map,
    const mojom::InstanceNormalization& instance_normalization) {
  webnn::InstanceNormalizationAttributes component_attributes;
  const auto& scale_operand_id = instance_normalization.scale_operand_id;
  if (scale_operand_id) {
    const mojom::Operand& scale_operand =
        *id_to_operand_map.at(scale_operand_id.value());
    component_attributes.scale = scale_operand.descriptor;
  }
  const auto& bias_operand_id = instance_normalization.bias_operand_id;
  if (bias_operand_id) {
    const mojom::Operand& bias_operand =
        *id_to_operand_map.at(bias_operand_id.value());
    component_attributes.bias = bias_operand.descriptor;
  }
  component_attributes.layout =
      MojoInputOperandLayoutToComponent(instance_normalization.layout);
  component_attributes.label = instance_normalization.label;

  return component_attributes;
}

webnn::SliceAttributes ConvertToSliceAttributes(
    const webnn::mojom::Slice& slice) {
  webnn::SliceAttributes component_attributes;
  component_attributes.starts.reserve(slice.ranges.size());
  component_attributes.sizes.reserve(slice.ranges.size());
  component_attributes.strides.reserve(slice.ranges.size());
  for (const auto& range : slice.ranges) {
    component_attributes.starts.push_back(range.start);
    component_attributes.sizes.push_back(range.size);
    component_attributes.strides.push_back(range.stride);
  }
  component_attributes.label = slice.label;

  return component_attributes;
}

// Helper class to validate a operations with the members passed to the
// constructor as context.
class OperationValidationContext {
  STACK_ALLOCATED();

 public:
  // If `operations` are valid given the passed members as context, returns a
  // mapping of operands to the operations which depend on it.
  static std::optional<DependentOperationsMap>
  ValidateOperationsAndGetDependencies(
      const std::vector<mojom::OperationPtr>& operations,
      const ContextProperties& context_properties,
      const IdToOperandMap& id_to_operand_map,
      base::flat_set<uint64_t> processed_operands);

 private:
  OperationValidationContext(const ContextProperties& context_properties,
                             const IdToOperandMap& id_to_operand_map,
                             base::flat_set<uint64_t> processed_operands)
      : context_properties_(context_properties),
        id_to_operand_map_(id_to_operand_map),
        processed_operands_(std::move(processed_operands)) {
    operands_to_dependent_operations_.reserve(id_to_operand_map.size());
  }

  const mojom::Operand* GetMojoOperand(uint64_t operand_id);

  void NoteDependency(uint64_t operand_id, size_t operation_id);

  template <typename Operation>
  bool ValidateUnaryOperation(const Operation& operation,
                              const webnn::SupportedDataTypes& input_constraint,
                              size_t operation_id);

  bool ValidateCastOperation(const mojom::ElementWiseUnary& operation,
                             size_t operation_id);
  bool ValidateBatchNormalization(
      const mojom::BatchNormalization& batch_normalization,
      size_t operation_id);
  bool ValidateArgMinMax(const mojom::ArgMinMax& arg_min_max,
                         size_t operation_id);
  bool ValidateClamp(const mojom::Clamp& clamp, size_t operation_id);
  bool ValidateConcat(const mojom::Concat& concat, size_t operation_id);
  bool ValidateConv2d(const mojom::Conv2d& conv2d, size_t operation_id);
  bool ValidateCumulativeSum(const mojom::CumulativeSum& cumulative_sum,
                             size_t operation_id);
  bool ValidateDequantizeLinear(
      const mojom::DequantizeLinear& dequantize_linear,
      size_t operation_id);
  bool ValidateElementWiseBinaryDataTypes(
      const mojom::Operand* lhs,
      const mojom::Operand* rhs,
      const mojom::Operand* output,
      const mojom::ElementWiseBinary& operation);
  bool ValidateElementWiseBinary(const mojom::ElementWiseBinary& operation,
                                 size_t operation_id);
  bool ValidateElu(const mojom::Elu& elu, size_t operation_id);

  bool ValidateElementWiseUnary(const mojom::ElementWiseUnary& operation,
                                size_t operation_id);
  bool ValidateExpand(const mojom::Expand& expand, size_t operation_id);
  bool ValidateGather(const mojom::Gather& gather, size_t operation_id);
  bool ValidateGatherElements(const mojom::GatherElements& gather_elements,
                              size_t operation_id);
  bool ValidateGatherND(const mojom::GatherND& gather_nd, size_t operation_id);
  bool ValidateGemm(const mojom::Gemm& gemm, size_t operation_id);
  bool ValidateGru(const mojom::Gru& gru, size_t operation_id);
  bool ValidateGruCell(const mojom::GruCell& gru_cell, size_t operation_id);
  bool ValidateHardSigmoid(const mojom::HardSigmoid& hard_sigmoid,
                           size_t operation_id);
  bool ValidateLayerNormalization(
      const mojom::LayerNormalization& layer_normalization,
      size_t operation_id);
  bool ValidateLeakyRelu(const mojom::LeakyRelu& leaky_relu,
                         size_t operation_id);
  bool ValidateLinear(const mojom::Linear& linear, size_t operation_id);
  bool ValidateLstm(const mojom::Lstm& lstm, size_t operation_id);
  bool ValidateLstmCell(const mojom::LstmCell& lstm_cell, size_t operation_id);
  bool ValidateInstanceNormalization(
      const mojom::InstanceNormalization& instance_normalization,
      size_t operation_id);
  bool ValidateMatmul(const mojom::Matmul& matmul, size_t operation_id);
  bool ValidatePad(const mojom::Pad& pad, size_t operation_id);
  bool ValidatePool2d(const mojom::Pool2d& pool2d, size_t operation_id);
  bool ValidatePrelu(const mojom::Prelu& prelu, size_t operation_id);
  bool ValidateQuantizeLinear(const mojom::QuantizeLinear& quantize_linear,
                              size_t operation_id);
  bool ValidateResample2d(const mojom::Resample2d& resample2d,
                          size_t operation_id);
  bool ValidateReshape(const mojom::Reshape& reshape, size_t operation_id);
  bool ValidateScatterElements(const mojom::ScatterElements& scatter_elements,
                               size_t operation_id);
  bool ValidateScatterND(const mojom::ScatterND& scatter_nd,
                         size_t operation_id);
  bool ValidateSlice(const mojom::Slice& slice, size_t operation_id);
  bool ValidateSoftmax(const mojom::Softmax& softmax, size_t operation_id);
  bool ValidateSplit(const mojom::Split& split, size_t operation_id);
  bool ValidateTile(const mojom::Tile& tile, size_t operation_id);
  bool ValidateTranspose(const mojom::Transpose& transpose,
                         size_t operation_id);
  bool ValidateTriangular(const mojom::Triangular& triangular,
                          size_t operation_id);
  bool ValidateWhere(const mojom::Where& where, size_t operation_id);
  bool ValidateReduce(const mojom::Reduce& reduce, size_t operation_id);

  bool ValidateOperation(const mojom::Operation& operation,
                         size_t operation_id);

  const base::raw_ref<const ContextProperties> context_properties_;
  const base::raw_ref<const IdToOperandMap> id_to_operand_map_;

  base::flat_set<uint64_t> processed_operands_;

  DependentOperationsMap operands_to_dependent_operations_;
};

const mojom::Operand* OperationValidationContext::GetMojoOperand(
    uint64_t operand_id) {
  return ::webnn::GetMojoOperand(*id_to_operand_map_, operand_id);
}

void OperationValidationContext::NoteDependency(uint64_t operand_id,
                                                size_t operation_id) {
  auto it = operands_to_dependent_operations_.find(operand_id);
  if (it == operands_to_dependent_operations_.end()) {
    operands_to_dependent_operations_.emplace(operand_id,
                                              std::vector({operation_id}));
  } else {
    it->second.insert(operation_id);
  }
}

// static
std::optional<DependentOperationsMap>
OperationValidationContext::ValidateOperationsAndGetDependencies(
    const std::vector<mojom::OperationPtr>& operations,
    const ContextProperties& context_properties,
    const IdToOperandMap& id_to_operand_map,
    base::flat_set<uint64_t> processed_operands) {
  OperationValidationContext context(context_properties, id_to_operand_map,
                                     processed_operands);

  for (size_t i = 0; i < operations.size(); i++) {
    if (!context.ValidateOperation(*operations[i], /*operation_id=*/i)) {
      return std::nullopt;
    }
  }

  return context.operands_to_dependent_operations_;
}

template <typename Operation>
bool OperationValidationContext::ValidateUnaryOperation(
    const Operation& operation,
    const webnn::SupportedDataTypes& input_constraint,
    size_t operation_id) {
  if (!processed_operands_.contains(operation.input_operand_id)) {
    return false;
  }
  NoteDependency(operation.input_operand_id, operation_id);

  RETURN_IF_FALSE(
      processed_operands_.insert(operation.output_operand_id).second);

  const auto* input = GetMojoOperand(operation.input_operand_id);
  const auto* output = GetMojoOperand(operation.output_operand_id);
  if (!input || !output || output == input) {
    // The unary operator is invalid.
    return false;
  }

  const auto input_data_type = input->descriptor.data_type();
  if (!input_constraint.Has(input_data_type)) {
    // The data type is not in the constraint.
    return false;
  }
  return output->descriptor == input->descriptor;
}

bool OperationValidationContext::ValidateCastOperation(
    const mojom::ElementWiseUnary& operation,
    size_t operation_id) {
  if (!processed_operands_.contains(operation.input_operand_id)) {
    return false;
  }
  NoteDependency(operation.input_operand_id, operation_id);

  RETURN_IF_FALSE(
      processed_operands_.insert(operation.output_operand_id).second);

  const auto* input = GetMojoOperand(operation.input_operand_id);
  const auto* output = GetMojoOperand(operation.output_operand_id);
  if (!input || !output || output == input) {
    // The unary operator is invalid.
    return false;
  }
  if (!base::ranges::equal(output->descriptor.shape(),
                           input->descriptor.shape())) {
    // The output shape is not expected.
    return false;
  }

  if (!context_properties_->data_type_limits.cast_input.Has(
          input->descriptor.data_type())) {
    return false;
  }
  if (!context_properties_->data_type_limits.cast_input.Has(
          output->descriptor.data_type())) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateBatchNormalization(
    const mojom::BatchNormalization& batch_normalization,
    size_t operation_id) {
  if (!processed_operands_.contains(batch_normalization.input_operand_id) ||
      !processed_operands_.contains(batch_normalization.mean_operand_id) ||
      !processed_operands_.contains(batch_normalization.variance_operand_id)) {
    return false;
  }
  NoteDependency(batch_normalization.input_operand_id, operation_id);
  NoteDependency(batch_normalization.mean_operand_id, operation_id);
  NoteDependency(batch_normalization.variance_operand_id, operation_id);

  RETURN_IF_FALSE(
      processed_operands_.insert(batch_normalization.output_operand_id).second);

  const auto* input = GetMojoOperand(batch_normalization.input_operand_id);
  const auto* mean = GetMojoOperand(batch_normalization.mean_operand_id);
  const auto* variance =
      GetMojoOperand(batch_normalization.variance_operand_id);
  const auto* output = GetMojoOperand(batch_normalization.output_operand_id);
  if (!input || !mean || !variance || !output || output == input ||
      output == mean || output == variance) {
    // The batchNormalization operator is invalid.
    return false;
  }
  const auto& scale_operand_id = batch_normalization.scale_operand_id;
  if (scale_operand_id) {
    if (!id_to_operand_map_->contains(scale_operand_id.value()) ||
        !processed_operands_.contains(scale_operand_id.value())) {
      // The scale operand is invalid.
      return false;
    }
    NoteDependency(scale_operand_id.value(), operation_id);
  }
  const auto& bias_operand_id = batch_normalization.bias_operand_id;
  if (bias_operand_id) {
    if (!id_to_operand_map_->contains(bias_operand_id.value()) ||
        !processed_operands_.contains(bias_operand_id.value())) {
      // The bias operand is invalid.
      return false;
    }
    NoteDependency(bias_operand_id.value(), operation_id);
  }

  const auto validated_output = ValidateBatchNormalizationAndInferOutput(
      *context_properties_, input->descriptor, mean->descriptor,
      variance->descriptor,
      ConvertToBatchNormalizationAttributes(*id_to_operand_map_,
                                            batch_normalization));
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateArgMinMax(
    const mojom::ArgMinMax& arg_min_max,
    size_t operation_id) {
  if (!processed_operands_.contains(arg_min_max.input_operand_id)) {
    return false;
  }
  NoteDependency(arg_min_max.input_operand_id, operation_id);

  RETURN_IF_FALSE(
      processed_operands_.insert(arg_min_max.output_operand_id).second);

  const auto* input = GetMojoOperand(arg_min_max.input_operand_id);
  const auto* output = GetMojoOperand(arg_min_max.output_operand_id);
  if (!input || !output || output == input) {
    // The argMinMax operator is invalid.
    return false;
  }

  const auto validated_output = ValidateArgMinMaxAndInferOutput(
      *context_properties_, input->descriptor, arg_min_max.label,
      arg_min_max.axis, output->descriptor.data_type(),
      arg_min_max.keep_dimensions);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateClamp(const mojom::Clamp& clamp,
                                               size_t operation_id) {
  if (!ValidateUnaryOperation(clamp,
                              context_properties_->data_type_limits.clamp_input,
                              operation_id)) {
    return false;
  }
  if (!ValidateClampAttributes(clamp)) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateConcat(const mojom::Concat& concat,
                                                size_t operation_id) {
  auto* output = GetMojoOperand(concat.output_operand_id);
  if (!output) {
    // The concat operator is invalid.
    return false;
  }

  std::vector<OperandDescriptor> inputs;
  inputs.reserve(concat.input_operand_ids.size());
  for (const auto& input_operand_id : concat.input_operand_ids) {
    if (!processed_operands_.contains(input_operand_id)) {
      return false;
    }
    NoteDependency(input_operand_id, operation_id);

    auto* input = GetMojoOperand(input_operand_id);
    if (!input || input == output) {
      return false;
    }
    inputs.push_back(input->descriptor);
  }

  auto validated_output = ValidateConcatAndInferOutput(
      *context_properties_, inputs, concat.axis, concat.label);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }
  RETURN_IF_FALSE(processed_operands_.insert(concat.output_operand_id).second);

  return true;
}

bool OperationValidationContext::ValidateConv2d(const mojom::Conv2d& conv2d,
                                                size_t operation_id) {
  if (!processed_operands_.contains(conv2d.input_operand_id) ||
      !processed_operands_.contains(conv2d.filter_operand_id)) {
    return false;
  }
  NoteDependency(conv2d.input_operand_id, operation_id);
  NoteDependency(conv2d.filter_operand_id, operation_id);

  auto* input = GetMojoOperand(conv2d.input_operand_id);
  auto* filter = GetMojoOperand(conv2d.filter_operand_id);
  auto* output = GetMojoOperand(conv2d.output_operand_id);
  if (!input || !filter || !output || output == input || output == filter) {
    // The conv2d operator is invalid.
    return false;
  }

  // The input and output rank need to be validated before converting to
  // `webnn::Conv2dAttributes`.
  if (input->descriptor.Rank() != 4 || output->descriptor.Rank() != 4) {
    // The element of input and output dimensions should be 4.
    return false;
  }

  std::optional<OperandDescriptor> bias_operand;
  auto& bias_operand_id = conv2d.bias_operand_id;
  if (bias_operand_id) {
    if (!processed_operands_.contains(bias_operand_id.value())) {
      return false;
    }
    NoteDependency(bias_operand_id.value(), operation_id);

    const auto bias_operand_iterator =
        id_to_operand_map_->find(bias_operand_id.value());
    if (bias_operand_iterator == id_to_operand_map_->end()) {
      // Invalid bias operand.
      return false;
    }
    bias_operand = bias_operand_iterator->second->descriptor;
  }
  RETURN_IF_FALSE(processed_operands_.insert(conv2d.output_operand_id).second);

  std::optional<base::expected<OperandDescriptor, std::string>>
      validated_output;
  switch (conv2d.kind) {
    case mojom::Conv2d::Kind::kDirect: {
      validated_output = ValidateConv2dAndInferOutput(
          *context_properties_, input->descriptor, filter->descriptor,
          ConvertToConv2dAttributes(*context_properties_, *id_to_operand_map_,
                                    conv2d, std::move(bias_operand)));
      break;
    }

    case mojom::Conv2d::Kind::kTransposed: {
      validated_output = ValidateConvTranspose2dAndInferOutput(
          *context_properties_, input->descriptor, filter->descriptor,
          ConvertToConvTranspose2dAttributes(*context_properties_,
                                             *id_to_operand_map_, conv2d,
                                             std::move(bias_operand)));
      break;
    }
  }
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateCumulativeSum(
    const mojom::CumulativeSum& cumulative_sum,
    size_t operation_id) {
  if (!processed_operands_.contains(cumulative_sum.input_operand_id)) {
    return false;
  }
  NoteDependency(cumulative_sum.input_operand_id, operation_id);

  auto* input = GetMojoOperand(cumulative_sum.input_operand_id);
  auto* output = GetMojoOperand(cumulative_sum.output_operand_id);

  if (!input || !output || output == input) {
    // The cumulative_sum operator is invalid.
    return false;
  }

  auto validated_output = ValidateCumulativeSumAndInferOutput(
      *context_properties_, input->descriptor, cumulative_sum.axis,
      cumulative_sum.label);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateDequantizeLinear(
    const mojom::DequantizeLinear& dequantize_linear,
    size_t operation_id) {
  if (!processed_operands_.contains(dequantize_linear.input_operand_id) ||
      !processed_operands_.contains(dequantize_linear.scale_operand_id) ||
      !processed_operands_.contains(dequantize_linear.zero_point_operand_id)) {
    return false;
  }
  NoteDependency(dequantize_linear.input_operand_id, operation_id);
  NoteDependency(dequantize_linear.scale_operand_id, operation_id);
  NoteDependency(dequantize_linear.zero_point_operand_id, operation_id);

  RETURN_IF_FALSE(
      processed_operands_.insert(dequantize_linear.output_operand_id).second);

  auto* input = GetMojoOperand(dequantize_linear.input_operand_id);
  auto* output = GetMojoOperand(dequantize_linear.output_operand_id);
  auto* scale = GetMojoOperand(dequantize_linear.scale_operand_id);
  auto* zero_point = GetMojoOperand(dequantize_linear.zero_point_operand_id);
  if (!input || !output || !scale || !zero_point || output == input ||
      output == scale || output == zero_point) {
    // The quantize_linear operator is invalid.
    return false;
  }

  auto validated_output = ValidateDequantizeLinearAndInferOutput(
      *context_properties_, input->descriptor, scale->descriptor,
      zero_point->descriptor, dequantize_linear.label);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateElementWiseBinaryDataTypes(
    const mojom::Operand* lhs,
    const mojom::Operand* rhs,
    const mojom::Operand* output,
    const mojom::ElementWiseBinary& operation) {
  if (lhs->descriptor.data_type() != rhs->descriptor.data_type()) {
    // The input types don't match.
    return false;
  }

  if (IsLogicalElementWiseBinary(operation.kind)) {
    if (output->descriptor.data_type() != OperandDataType::kUint8) {
      // For logical operations, the output data type must be uint8.
      return false;
    }
  } else {
    // For all other operations, the input and output data types must match.
    if (output->descriptor.data_type() != lhs->descriptor.data_type()) {
      return false;
    }
  }

  switch (operation.kind) {
    case mojom::ElementWiseBinary::Kind::kAdd:
      return context_properties_->data_type_limits.add_input.Has(
          lhs->descriptor.data_type());
    case mojom::ElementWiseBinary::Kind::kSub:
      return context_properties_->data_type_limits.sub_input.Has(
          lhs->descriptor.data_type());
    case mojom::ElementWiseBinary::Kind::kMul:
      return context_properties_->data_type_limits.mul_input.Has(
          lhs->descriptor.data_type());
    case mojom::ElementWiseBinary::Kind::kDiv:
      return context_properties_->data_type_limits.div_input.Has(
          lhs->descriptor.data_type());
    case mojom::ElementWiseBinary::Kind::kMax:
      return context_properties_->data_type_limits.max_input.Has(
          lhs->descriptor.data_type());
    case mojom::ElementWiseBinary::Kind::kMin:
      return context_properties_->data_type_limits.min_input.Has(
          lhs->descriptor.data_type());
    case mojom::ElementWiseBinary::Kind::kPow:
      return context_properties_->data_type_limits.pow_input.Has(
          lhs->descriptor.data_type());
    case mojom::ElementWiseBinary::Kind::kEqual:
      return context_properties_->data_type_limits.equal_input.Has(
          lhs->descriptor.data_type());
    case mojom::ElementWiseBinary::Kind::kGreater:
      return context_properties_->data_type_limits.greater_input.Has(
          lhs->descriptor.data_type());
    case mojom::ElementWiseBinary::Kind::kGreaterOrEqual:
      return context_properties_->data_type_limits.greater_or_equal_input.Has(
          lhs->descriptor.data_type());
    case mojom::ElementWiseBinary::Kind::kLesser:
      return context_properties_->data_type_limits.lesser_input.Has(
          lhs->descriptor.data_type());
    case mojom::ElementWiseBinary::Kind::kLesserOrEqual:
      return context_properties_->data_type_limits.lesser_or_equal_input.Has(
          lhs->descriptor.data_type());
    case mojom::ElementWiseBinary::Kind::kLogicalAnd:
      return context_properties_->data_type_limits.logical_and_input.Has(
          lhs->descriptor.data_type());
    case mojom::ElementWiseBinary::Kind::kLogicalOr:
      return context_properties_->data_type_limits.logical_or_input.Has(
          lhs->descriptor.data_type());
    case mojom::ElementWiseBinary::Kind::kLogicalXor:
      return context_properties_->data_type_limits.logical_xor_input.Has(
          lhs->descriptor.data_type());
  }
}

bool OperationValidationContext::ValidateElementWiseBinary(
    const mojom::ElementWiseBinary& operation,
    size_t operation_id) {
  if (!processed_operands_.contains(operation.lhs_operand_id) ||
      !processed_operands_.contains(operation.rhs_operand_id)) {
    return false;
  }
  NoteDependency(operation.lhs_operand_id, operation_id);
  NoteDependency(operation.rhs_operand_id, operation_id);

  RETURN_IF_FALSE(
      processed_operands_.insert(operation.output_operand_id).second);

  auto* a = GetMojoOperand(operation.lhs_operand_id);
  auto* b = GetMojoOperand(operation.rhs_operand_id);
  auto* output = GetMojoOperand(operation.output_operand_id);

  if (!a || !b || !output || output == a || output == b) {
    // The elementWise binary operator is invalid.
    return false;
  }

  if (!ValidateElementWiseBinaryDataTypes(a, b, output, operation)) {
    return false;
  }

  auto dims_output =
      BroadcastShapes(a->descriptor.shape(), b->descriptor.shape());
  if (!dims_output) {
    // The input shapes are not broadcastable.
    return false;
  }
  if (!base::ranges::equal(output->descriptor.shape(), dims_output.value())) {
    // The output shape is not expected.
    return false;
  }
  return true;
}

bool OperationValidationContext::ValidateElu(const mojom::Elu& elu,
                                             size_t operation_id) {
  if (!ValidateUnaryOperation(
          elu, context_properties_->data_type_limits.elu_input, operation_id)) {
    return false;
  }
  if (!ValidateEluAttributes(elu)) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateElementWiseUnary(
    const mojom::ElementWiseUnary& operation,
    size_t operation_id) {
  switch (operation.kind) {
    case mojom::ElementWiseUnary::Kind::kAbs:
      return ValidateUnaryOperation(
          operation, context_properties_->data_type_limits.abs_input,
          operation_id);
    case mojom::ElementWiseUnary::Kind::kCast:
      return ValidateCastOperation(operation, operation_id);
    case mojom::ElementWiseUnary::Kind::kCeil:
      return ValidateUnaryOperation(
          operation, context_properties_->data_type_limits.ceil_input,
          operation_id);
    case mojom::ElementWiseUnary::Kind::kCos:
      return ValidateUnaryOperation(
          operation, context_properties_->data_type_limits.cos_input,
          operation_id);
    case mojom::ElementWiseUnary::Kind::kErf:
      return ValidateUnaryOperation(
          operation, context_properties_->data_type_limits.erf_input,
          operation_id);
    case mojom::ElementWiseUnary::Kind::kExp:
      return ValidateUnaryOperation(
          operation, context_properties_->data_type_limits.exp_input,
          operation_id);
    case mojom::ElementWiseUnary::Kind::kFloor:
      return ValidateUnaryOperation(
          operation, context_properties_->data_type_limits.floor_input,
          operation_id);
    case mojom::ElementWiseUnary::Kind::kIdentity:
      return ValidateUnaryOperation(
          operation, context_properties_->data_type_limits.identity_input,
          operation_id);
    case mojom::ElementWiseUnary::Kind::kLog:
      return ValidateUnaryOperation(
          operation, context_properties_->data_type_limits.log_input,
          operation_id);
    case mojom::ElementWiseUnary::Kind::kLogicalNot:
      return ValidateUnaryOperation(
          operation, context_properties_->data_type_limits.logical_not_input,
          operation_id);
    case mojom::ElementWiseUnary::Kind::kNeg:
      return ValidateUnaryOperation(
          operation, context_properties_->data_type_limits.neg_input,
          operation_id);
    case mojom::ElementWiseUnary::Kind::kReciprocal:
      return ValidateUnaryOperation(
          operation, context_properties_->data_type_limits.reciprocal_input,
          operation_id);
    case mojom::ElementWiseUnary::Kind::kSign:
      return ValidateUnaryOperation(
          operation, context_properties_->data_type_limits.sign_input,
          operation_id);
    case mojom::ElementWiseUnary::Kind::kSin:
      return ValidateUnaryOperation(
          operation, context_properties_->data_type_limits.sin_input,
          operation_id);
    case mojom::ElementWiseUnary::Kind::kSqrt:
      return ValidateUnaryOperation(
          operation, context_properties_->data_type_limits.sqrt_input,
          operation_id);
    case mojom::ElementWiseUnary::Kind::kTan:
      return ValidateUnaryOperation(
          operation, context_properties_->data_type_limits.tan_input,
          operation_id);
  }
}

bool OperationValidationContext::ValidateExpand(const mojom::Expand& expand,
                                                size_t operation_id) {
  if (!processed_operands_.contains(expand.input_operand_id)) {
    return false;
  }
  NoteDependency(expand.input_operand_id, operation_id);

  RETURN_IF_FALSE(processed_operands_.insert(expand.output_operand_id).second);

  auto* input = GetMojoOperand(expand.input_operand_id);
  auto* output = GetMojoOperand(expand.output_operand_id);
  if (!input || !output || output == input) {
    // The expand operator is invalid.
    return false;
  }
  if (!context_properties_->data_type_limits.expand_input.Has(
          input->descriptor.data_type())) {
    return false;
  }
  if (output->descriptor.data_type() != input->descriptor.data_type()) {
    // The output data type doesn't match input data type.
    return false;
  }

  auto output_shape = BroadcastShapes(input->descriptor.shape(),
                                      output->descriptor.shape(), false);
  if (!output_shape) {
    // The input shape is not broadcastable to the output shape.
    return false;
  }
  CHECK(base::ranges::equal(output_shape.value(), output->descriptor.shape()));

  return true;
}

bool OperationValidationContext::ValidateGather(const mojom::Gather& gather,
                                                size_t operation_id) {
  if (!processed_operands_.contains(gather.input_operand_id) ||
      !processed_operands_.contains(gather.indices_operand_id)) {
    return false;
  }
  NoteDependency(gather.input_operand_id, operation_id);
  NoteDependency(gather.indices_operand_id, operation_id);

  RETURN_IF_FALSE(processed_operands_.insert(gather.output_operand_id).second);

  auto* input = GetMojoOperand(gather.input_operand_id);
  auto* output = GetMojoOperand(gather.output_operand_id);
  auto* indices = GetMojoOperand(gather.indices_operand_id);
  if (!input || !output || !indices || output == input || output == indices) {
    // The gather operator is invalid.
    return false;
  }

  auto validated_output = ValidateGatherAndInferOutput(
      *context_properties_, input->descriptor, indices->descriptor, gather.axis,
      gather.label);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateGatherElements(
    const mojom::GatherElements& gather_elements,
    size_t operation_id) {
  if (!processed_operands_.contains(gather_elements.input_operand_id) ||
      !processed_operands_.contains(gather_elements.indices_operand_id)) {
    return false;
  }
  NoteDependency(gather_elements.input_operand_id, operation_id);
  NoteDependency(gather_elements.indices_operand_id, operation_id);

  RETURN_IF_FALSE(
      processed_operands_.insert(gather_elements.output_operand_id).second);

  auto* input = GetMojoOperand(gather_elements.input_operand_id);
  auto* output = GetMojoOperand(gather_elements.output_operand_id);
  auto* indices = GetMojoOperand(gather_elements.indices_operand_id);
  if (!input || !output || !indices || output == input || output == indices) {
    return false;
  }

  auto validated_output = ValidateGatherElementsAndInferOutput(
      *context_properties_, input->descriptor, indices->descriptor,
      gather_elements.axis, gather_elements.label);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateGatherND(
    const mojom::GatherND& gather_nd,
    size_t operation_id) {
  if (!processed_operands_.contains(gather_nd.input_operand_id) ||
      !processed_operands_.contains(gather_nd.indices_operand_id)) {
    return false;
  }
  NoteDependency(gather_nd.input_operand_id, operation_id);
  NoteDependency(gather_nd.indices_operand_id, operation_id);

  RETURN_IF_FALSE(
      processed_operands_.insert(gather_nd.output_operand_id).second);

  auto* input = GetMojoOperand(gather_nd.input_operand_id);
  auto* output = GetMojoOperand(gather_nd.output_operand_id);
  auto* indices = GetMojoOperand(gather_nd.indices_operand_id);
  if (!input || !output || !indices || output == input || output == indices) {
    return false;
  }

  auto validated_output =
      ValidateGatherNDAndInferOutput(*context_properties_, input->descriptor,
                                     indices->descriptor, gather_nd.label);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateGemm(const mojom::Gemm& gemm,
                                              size_t operation_id) {
  if (!processed_operands_.contains(gemm.a_operand_id) ||
      !processed_operands_.contains(gemm.b_operand_id)) {
    return false;
  }
  NoteDependency(gemm.a_operand_id, operation_id);
  NoteDependency(gemm.b_operand_id, operation_id);

  RETURN_IF_FALSE(processed_operands_.insert(gemm.output_operand_id).second);

  auto* a = GetMojoOperand(gemm.a_operand_id);
  auto* b = GetMojoOperand(gemm.b_operand_id);
  auto* output = GetMojoOperand(gemm.output_operand_id);
  if (!a || !b || !output || output == a || output == b) {
    // The gemm operator is invalid.
    return false;
  }
  auto& c_operand_id = gemm.c_operand_id;
  if (c_operand_id) {
    if (!id_to_operand_map_->contains(c_operand_id.value()) ||
        !processed_operands_.contains(c_operand_id.value())) {
      // The third operand is invalid.
      return false;
    }
    NoteDependency(c_operand_id.value(), operation_id);
  }

  auto validated_output = ValidateGemmAndInferOutput(
      *context_properties_, a->descriptor, b->descriptor,
      ConvertToGemmAttributes(*id_to_operand_map_, gemm));
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateGru(const mojom::Gru& gru,
                                             size_t operation_id) {
  if (!processed_operands_.contains(gru.input_operand_id) ||
      !processed_operands_.contains(gru.weight_operand_id) ||
      !processed_operands_.contains(gru.recurrent_weight_operand_id)) {
    return false;
  }
  NoteDependency(gru.input_operand_id, operation_id);
  NoteDependency(gru.weight_operand_id, operation_id);
  NoteDependency(gru.recurrent_weight_operand_id, operation_id);

  const auto* input = GetMojoOperand(gru.input_operand_id);
  const auto* weight = GetMojoOperand(gru.weight_operand_id);
  const auto* recurrent_weight =
      GetMojoOperand(gru.recurrent_weight_operand_id);
  if (!input || !weight || !recurrent_weight) {
    return false;
  }

  const auto& bias_operand_id = gru.bias_operand_id;
  if (bias_operand_id.has_value()) {
    if (!id_to_operand_map_->contains(bias_operand_id.value()) ||
        !processed_operands_.contains(gru.bias_operand_id)) {
      return false;
    }
    NoteDependency(bias_operand_id.value(), operation_id);
  }
  const auto& recurrent_bias_operand_id = gru.recurrent_bias_operand_id;
  if (recurrent_bias_operand_id.has_value()) {
    if (!id_to_operand_map_->contains(recurrent_bias_operand_id.value()) ||
        !processed_operands_.contains(gru.recurrent_bias_operand_id)) {
      return false;
    }
    NoteDependency(recurrent_bias_operand_id.value(), operation_id);
  }
  const auto& initial_hidden_state_operand_id =
      gru.initial_hidden_state_operand_id;
  if (initial_hidden_state_operand_id.has_value()) {
    if (!id_to_operand_map_->contains(
            initial_hidden_state_operand_id.value()) ||
        !processed_operands_.contains(gru.initial_hidden_state_operand_id)) {
      return false;
    }
    NoteDependency(initial_hidden_state_operand_id.value(), operation_id);
  }

  for (uint64_t output_operand_id : gru.output_operand_ids) {
    if (output_operand_id == gru.input_operand_id ||
        output_operand_id == gru.weight_operand_id ||
        output_operand_id == gru.recurrent_weight_operand_id) {
      return false;
    }
    if (bias_operand_id == output_operand_id ||
        recurrent_bias_operand_id == output_operand_id ||
        initial_hidden_state_operand_id == output_operand_id) {
      return false;
    }
    RETURN_IF_FALSE(processed_operands_.insert(output_operand_id).second);
  }

  const auto validated_outputs = ValidateGruAndInferOutput(
      *context_properties_, input->descriptor, weight->descriptor,
      recurrent_weight->descriptor, gru.steps, gru.hidden_size,
      ConvertToGruAttributes(*id_to_operand_map_, gru));
  if (!validated_outputs.has_value()) {
    return false;
  }
  if (gru.output_operand_ids.size() != validated_outputs->size()) {
    return false;
  }
  for (size_t i = 0; i < validated_outputs->size(); ++i) {
    const auto* output = GetMojoOperand(gru.output_operand_ids[i]);
    if (!output) {
      return false;
    }
    if (validated_outputs->at(i) != output->descriptor) {
      return false;
    }
  }

  return true;
}

bool OperationValidationContext::ValidateGruCell(const mojom::GruCell& gru_cell,
                                                 size_t operation_id) {
  if (!processed_operands_.contains(gru_cell.input_operand_id) ||
      !processed_operands_.contains(gru_cell.weight_operand_id) ||
      !processed_operands_.contains(gru_cell.recurrent_weight_operand_id) ||
      !processed_operands_.contains(gru_cell.hidden_state_operand_id)) {
    return false;
  }
  NoteDependency(gru_cell.input_operand_id, operation_id);
  NoteDependency(gru_cell.weight_operand_id, operation_id);
  NoteDependency(gru_cell.recurrent_weight_operand_id, operation_id);
  NoteDependency(gru_cell.hidden_state_operand_id, operation_id);

  const mojom::Operand* input = GetMojoOperand(gru_cell.input_operand_id);
  const mojom::Operand* weight = GetMojoOperand(gru_cell.weight_operand_id);
  const mojom::Operand* recurrent_weight =
      GetMojoOperand(gru_cell.recurrent_weight_operand_id);
  const mojom::Operand* hidden_state =
      GetMojoOperand(gru_cell.hidden_state_operand_id);
  if (!input || !weight || !recurrent_weight || !hidden_state) {
    return false;
  }

  const std::optional<uint32_t>& bias_operand_id = gru_cell.bias_operand_id;
  if (bias_operand_id.has_value()) {
    if (!id_to_operand_map_->contains(bias_operand_id.value()) ||
        !processed_operands_.contains(gru_cell.bias_operand_id)) {
      return false;
    }
    NoteDependency(bias_operand_id.value(), operation_id);
  }
  const std::optional<uint32_t>& recurrent_bias_operand_id =
      gru_cell.recurrent_bias_operand_id;
  if (recurrent_bias_operand_id.has_value()) {
    if (!id_to_operand_map_->contains(recurrent_bias_operand_id.value()) ||
        !processed_operands_.contains(gru_cell.recurrent_bias_operand_id)) {
      return false;
    }
    NoteDependency(recurrent_bias_operand_id.value(), operation_id);
  }

  if (gru_cell.output_operand_id == gru_cell.input_operand_id ||
      gru_cell.output_operand_id == gru_cell.weight_operand_id ||
      gru_cell.output_operand_id == gru_cell.recurrent_weight_operand_id ||
      gru_cell.output_operand_id == gru_cell.hidden_state_operand_id ||
      gru_cell.output_operand_id == bias_operand_id ||
      gru_cell.output_operand_id == recurrent_bias_operand_id) {
    return false;
  }
  RETURN_IF_FALSE(
      processed_operands_.insert(gru_cell.output_operand_id).second);

  const base::expected<OperandDescriptor, std::string> validated_output =
      ValidateGruCellAndInferOutput(
          *context_properties_, input->descriptor, weight->descriptor,
          recurrent_weight->descriptor, hidden_state->descriptor,
          gru_cell.hidden_size,
          ConvertToGruCellAttributes(*id_to_operand_map_, gru_cell));
  if (!validated_output.has_value()) {
    return false;
  }

  const mojom::Operand* output = GetMojoOperand(gru_cell.output_operand_id);
  if (!output) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateHardSigmoid(
    const mojom::HardSigmoid& hard_sigmoid,
    size_t operation_id) {
  if (!ValidateUnaryOperation(
          hard_sigmoid,
          context_properties_->data_type_limits.hard_sigmoid_input,
          operation_id)) {
    return false;
  }
  if (!ValidateHardSigmoidAttributes(hard_sigmoid)) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateLayerNormalization(
    const mojom::LayerNormalization& layer_normalization,
    size_t operation_id) {
  if (!processed_operands_.contains(layer_normalization.input_operand_id)) {
    return false;
  }
  NoteDependency(layer_normalization.input_operand_id, operation_id);

  RETURN_IF_FALSE(
      processed_operands_.insert(layer_normalization.output_operand_id).second);

  const auto* input = GetMojoOperand(layer_normalization.input_operand_id);
  const auto* output = GetMojoOperand(layer_normalization.output_operand_id);
  if (!input || !output || output == input) {
    // The layerNormalization operator is invalid.
    return false;
  }

  const auto& scale_operand_id = layer_normalization.scale_operand_id;
  if (scale_operand_id) {
    if (!id_to_operand_map_->contains(scale_operand_id.value()) ||
        !processed_operands_.contains(scale_operand_id.value()) ||
        scale_operand_id.value() == layer_normalization.output_operand_id) {
      // The scale operand is invalid.
      return false;
    }
    NoteDependency(scale_operand_id.value(), operation_id);
  }
  const auto& bias_operand_id = layer_normalization.bias_operand_id;
  if (bias_operand_id) {
    if (!id_to_operand_map_->contains(bias_operand_id.value()) ||
        !processed_operands_.contains(bias_operand_id.value()) ||
        bias_operand_id.value() == layer_normalization.output_operand_id) {
      // The bias operand is invalid.
      return false;
    }
    NoteDependency(bias_operand_id.value(), operation_id);
  }

  const auto validated_output = ValidateLayerNormalizationAndInferOutput(
      *context_properties_, input->descriptor, layer_normalization.axes,
      ConvertToLayerNormalizationAttributes(*id_to_operand_map_,
                                            layer_normalization));
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateLeakyRelu(
    const mojom::LeakyRelu& leaky_relu,
    size_t operation_id) {
  if (!ValidateUnaryOperation(
          leaky_relu, context_properties_->data_type_limits.leaky_relu_input,
          operation_id)) {
    return false;
  }
  if (!ValidateLeakyReluAttributes(leaky_relu)) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateLinear(const mojom::Linear& linear,
                                                size_t operation_id) {
  if (!ValidateUnaryOperation(
          linear, context_properties_->data_type_limits.linear_input,
          operation_id)) {
    return false;
  }
  if (!ValidateLinearAttributes(linear)) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateLstm(const mojom::Lstm& lstm,
                                              size_t operation_id) {
  if (!processed_operands_.contains(lstm.input_operand_id) ||
      !processed_operands_.contains(lstm.weight_operand_id) ||
      !processed_operands_.contains(lstm.recurrent_weight_operand_id)) {
    return false;
  }
  NoteDependency(lstm.input_operand_id, operation_id);
  NoteDependency(lstm.weight_operand_id, operation_id);
  NoteDependency(lstm.recurrent_weight_operand_id, operation_id);

  const auto* input = GetMojoOperand(lstm.input_operand_id);
  const auto* weight = GetMojoOperand(lstm.weight_operand_id);
  const auto* recurrent_weight =
      GetMojoOperand(lstm.recurrent_weight_operand_id);
  if (!input || !weight || !recurrent_weight) {
    return false;
  }

  const auto& bias_operand_id = lstm.bias_operand_id;
  if (bias_operand_id.has_value()) {
    if (!id_to_operand_map_->contains(bias_operand_id.value()) ||
        !processed_operands_.contains(lstm.bias_operand_id)) {
      return false;
    }
    NoteDependency(bias_operand_id.value(), operation_id);
  }
  const auto& recurrent_bias_operand_id = lstm.recurrent_bias_operand_id;
  if (recurrent_bias_operand_id.has_value()) {
    if (!id_to_operand_map_->contains(recurrent_bias_operand_id.value()) ||
        !processed_operands_.contains(lstm.recurrent_bias_operand_id)) {
      return false;
    }
    NoteDependency(recurrent_bias_operand_id.value(), operation_id);
  }
  const auto& peephole_weight_operand_id = lstm.peephole_weight_operand_id;
  if (peephole_weight_operand_id.has_value()) {
    if (!id_to_operand_map_->contains(peephole_weight_operand_id.value()) ||
        !processed_operands_.contains(lstm.peephole_weight_operand_id)) {
      return false;
    }
    NoteDependency(peephole_weight_operand_id.value(), operation_id);
  }
  const auto& initial_hidden_state_operand_id =
      lstm.initial_hidden_state_operand_id;
  if (initial_hidden_state_operand_id.has_value()) {
    if (!id_to_operand_map_->contains(
            initial_hidden_state_operand_id.value()) ||
        !processed_operands_.contains(lstm.initial_hidden_state_operand_id)) {
      return false;
    }
    NoteDependency(initial_hidden_state_operand_id.value(), operation_id);
  }
  const auto& initial_cell_state_operand_id =
      lstm.initial_cell_state_operand_id;
  if (initial_cell_state_operand_id.has_value()) {
    if (!id_to_operand_map_->contains(initial_cell_state_operand_id.value()) ||
        !processed_operands_.contains(lstm.initial_cell_state_operand_id)) {
      return false;
    }
    NoteDependency(initial_cell_state_operand_id.value(), operation_id);
  }

  for (uint64_t output_operand_id : lstm.output_operand_ids) {
    if (output_operand_id == lstm.input_operand_id ||
        output_operand_id == lstm.weight_operand_id ||
        output_operand_id == lstm.recurrent_weight_operand_id) {
      return false;
    }
    if ((initial_hidden_state_operand_id.has_value() &&
         initial_hidden_state_operand_id.value() == output_operand_id) ||
        (initial_cell_state_operand_id.has_value() &&
         initial_cell_state_operand_id.value() == output_operand_id)) {
      return false;
    }
    RETURN_IF_FALSE(processed_operands_.insert(output_operand_id).second);
  }

  const auto validated_outputs = ValidateLstmAndInferOutput(
      *context_properties_, input->descriptor, weight->descriptor,
      recurrent_weight->descriptor, lstm.steps, lstm.hidden_size,
      ConvertToLstmAttributes(*id_to_operand_map_, lstm));
  if (!validated_outputs.has_value()) {
    return false;
  }
  if (lstm.output_operand_ids.size() != validated_outputs->size()) {
    return false;
  }
  for (size_t i = 0; i < validated_outputs->size(); ++i) {
    const auto* output = GetMojoOperand(lstm.output_operand_ids[i]);
    if (!output) {
      return false;
    }
    if (validated_outputs->at(i) != output->descriptor) {
      return false;
    }
  }

  return true;
}

bool OperationValidationContext::ValidateLstmCell(
    const mojom::LstmCell& lstm_cell,
    size_t operation_id) {
  if (!processed_operands_.contains(lstm_cell.input_operand_id) ||
      !processed_operands_.contains(lstm_cell.weight_operand_id) ||
      !processed_operands_.contains(lstm_cell.recurrent_weight_operand_id) ||
      !processed_operands_.contains(lstm_cell.hidden_state_operand_id) ||
      !processed_operands_.contains(lstm_cell.cell_state_operand_id)) {
    return false;
  }
  NoteDependency(lstm_cell.input_operand_id, operation_id);
  NoteDependency(lstm_cell.weight_operand_id, operation_id);
  NoteDependency(lstm_cell.recurrent_weight_operand_id, operation_id);
  NoteDependency(lstm_cell.hidden_state_operand_id, operation_id);
  NoteDependency(lstm_cell.cell_state_operand_id, operation_id);

  const mojom::Operand* input = GetMojoOperand(lstm_cell.input_operand_id);
  const mojom::Operand* weight = GetMojoOperand(lstm_cell.weight_operand_id);
  const mojom::Operand* recurrent_weight =
      GetMojoOperand(lstm_cell.recurrent_weight_operand_id);
  const mojom::Operand* hidden_state =
      GetMojoOperand(lstm_cell.hidden_state_operand_id);
  const mojom::Operand* cell_state =
      GetMojoOperand(lstm_cell.cell_state_operand_id);
  if (!input || !weight || !recurrent_weight || !hidden_state || !cell_state) {
    return false;
  }

  const std::optional<uint64_t> bias_operand_id = lstm_cell.bias_operand_id;
  if (bias_operand_id.has_value()) {
    if (!id_to_operand_map_->contains(bias_operand_id.value()) ||
        !processed_operands_.contains(bias_operand_id.value())) {
      return false;
    }
    NoteDependency(bias_operand_id.value(), operation_id);
  }
  const std::optional<uint64_t> recurrent_bias_operand_id =
      lstm_cell.recurrent_bias_operand_id;
  if (recurrent_bias_operand_id.has_value()) {
    if (!id_to_operand_map_->contains(recurrent_bias_operand_id.value()) ||
        !processed_operands_.contains(recurrent_bias_operand_id.value())) {
      return false;
    }
    NoteDependency(recurrent_bias_operand_id.value(), operation_id);
  }
  const std::optional<uint64_t> peephole_weight_operand_id =
      lstm_cell.peephole_weight_operand_id;
  if (peephole_weight_operand_id.has_value()) {
    if (!id_to_operand_map_->contains(peephole_weight_operand_id.value()) ||
        !processed_operands_.contains(peephole_weight_operand_id.value())) {
      return false;
    }
    NoteDependency(peephole_weight_operand_id.value(), operation_id);
  }

  for (uint64_t output_operand_id : lstm_cell.output_operand_ids) {
    if (output_operand_id == lstm_cell.input_operand_id ||
        output_operand_id == lstm_cell.weight_operand_id ||
        output_operand_id == lstm_cell.recurrent_weight_operand_id ||
        output_operand_id == lstm_cell.hidden_state_operand_id ||
        output_operand_id == lstm_cell.cell_state_operand_id) {
      return false;
    }
    RETURN_IF_FALSE(processed_operands_.insert(output_operand_id).second);
  }

  const base::expected<std::vector<webnn::OperandDescriptor>, std::string>
      validated_outputs = ValidateLstmCellAndInferOutput(
          *context_properties_, input->descriptor, weight->descriptor,
          recurrent_weight->descriptor, hidden_state->descriptor,
          cell_state->descriptor, lstm_cell.hidden_size,
          ConvertToLstmCellAttributes(*id_to_operand_map_, lstm_cell));
  if (!validated_outputs.has_value()) {
    return false;
  }
  if (lstm_cell.output_operand_ids.size() != validated_outputs->size()) {
    return false;
  }
  for (size_t i = 0; i < validated_outputs->size(); ++i) {
    const mojom::Operand* output =
        GetMojoOperand(lstm_cell.output_operand_ids[i]);
    if (!output) {
      return false;
    }
    if (validated_outputs->at(i) != output->descriptor) {
      return false;
    }
  }

  return true;
}

bool OperationValidationContext::ValidateInstanceNormalization(
    const mojom::InstanceNormalization& instance_normalization,
    size_t operation_id) {
  if (!processed_operands_.contains(instance_normalization.input_operand_id)) {
    return false;
  }
  NoteDependency(instance_normalization.input_operand_id, operation_id);

  RETURN_IF_FALSE(
      processed_operands_.insert(instance_normalization.output_operand_id)
          .second);

  const auto* input = GetMojoOperand(instance_normalization.input_operand_id);
  const auto* output = GetMojoOperand(instance_normalization.output_operand_id);
  if (!input || !output || output == input) {
    // The instanceNormalization operator is invalid.
    return false;
  }
  const auto& scale_operand_id = instance_normalization.scale_operand_id;
  if (scale_operand_id) {
    if (!id_to_operand_map_->contains(scale_operand_id.value()) ||
        !processed_operands_.contains(scale_operand_id.value()) ||
        scale_operand_id.value() == instance_normalization.output_operand_id) {
      // The scale operand is invalid.
      return false;
    }
    NoteDependency(scale_operand_id.value(), operation_id);
  }
  const auto& bias_operand_id = instance_normalization.bias_operand_id;
  if (bias_operand_id) {
    if (!id_to_operand_map_->contains(bias_operand_id.value()) ||
        !processed_operands_.contains(bias_operand_id.value()) ||
        bias_operand_id.value() == instance_normalization.output_operand_id) {
      // The bias operand is invalid.
      return false;
    }
    NoteDependency(bias_operand_id.value(), operation_id);
  }

  const auto validated_output = ValidateInstanceNormalizationAndInferOutput(
      *context_properties_, input->descriptor,
      ConvertToInstanceNormalizationAttributes(*id_to_operand_map_,
                                               instance_normalization));
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateMatmul(const mojom::Matmul& matmul,
                                                size_t operation_id) {
  if (!processed_operands_.contains(matmul.a_operand_id) ||
      !processed_operands_.contains(matmul.b_operand_id)) {
    return false;
  }
  NoteDependency(matmul.a_operand_id, operation_id);
  NoteDependency(matmul.b_operand_id, operation_id);

  RETURN_IF_FALSE(processed_operands_.insert(matmul.output_operand_id).second);

  auto* a = GetMojoOperand(matmul.a_operand_id);
  auto* b = GetMojoOperand(matmul.b_operand_id);
  auto* output = GetMojoOperand(matmul.output_operand_id);
  if (!a || !b || !output || output == a || output == b) {
    // The matmul operator is invalid.
    return false;
  }
  auto validated_output = ValidateMatmulAndInferOutput(
      *context_properties_, a->descriptor, b->descriptor, matmul.label);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidatePad(const mojom::Pad& pad,
                                             size_t operation_id) {
  if (!processed_operands_.contains(pad.input_operand_id)) {
    return false;
  }
  NoteDependency(pad.input_operand_id, operation_id);

  RETURN_IF_FALSE(processed_operands_.insert(pad.output_operand_id).second);

  auto* input = GetMojoOperand(pad.input_operand_id);
  auto* output = GetMojoOperand(pad.output_operand_id);
  if (!input || !output || output == input) {
    // The pad operator is invalid.
    return false;
  }

  auto validated_output = ValidatePadAndInferOutput(
      *context_properties_, input->descriptor, pad.beginning_padding,
      pad.ending_padding, pad.label);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidatePool2d(const mojom::Pool2d& pool2d,
                                                size_t operation_id) {
  if (!processed_operands_.contains(pool2d.input_operand_id)) {
    return false;
  }
  NoteDependency(pool2d.input_operand_id, operation_id);

  RETURN_IF_FALSE(processed_operands_.insert(pool2d.output_operand_id).second);

  auto* input = GetMojoOperand(pool2d.input_operand_id);
  auto* output = GetMojoOperand(pool2d.output_operand_id);
  if (!input || !output || output == input) {
    // The pool2d operator is invalid.
    return false;
  }

  if (output->descriptor.Rank() != 4) {
    return false;
  }
  auto validated_output = ValidatePool2dAndInferOutput(
      *context_properties_, input->descriptor,
      ConvertToPool2dAttributes(*context_properties_, pool2d, output),
      FromMojoPool2dType(pool2d.kind));
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidatePrelu(const mojom::Prelu& prelu,
                                               size_t operation_id) {
  if (!processed_operands_.contains(prelu.input_operand_id) ||
      !processed_operands_.contains(prelu.slope_operand_id)) {
    return false;
  }
  NoteDependency(prelu.input_operand_id, operation_id);
  NoteDependency(prelu.slope_operand_id, operation_id);

  RETURN_IF_FALSE(processed_operands_.insert(prelu.output_operand_id).second);

  auto* input = GetMojoOperand(prelu.input_operand_id);
  auto* output = GetMojoOperand(prelu.output_operand_id);
  auto* slope = GetMojoOperand(prelu.slope_operand_id);
  if (!input || !output || !slope || output == input || output == slope) {
    // The prelu operator is invalid.
    return false;
  }

  auto validated_output = ValidatePreluAndInferOutput(
      *context_properties_, input->descriptor, slope->descriptor, prelu.label);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateQuantizeLinear(
    const mojom::QuantizeLinear& quantize_linear,
    size_t operation_id) {
  if (!processed_operands_.contains(quantize_linear.input_operand_id) ||
      !processed_operands_.contains(quantize_linear.scale_operand_id) ||
      !processed_operands_.contains(quantize_linear.zero_point_operand_id)) {
    return false;
  }
  NoteDependency(quantize_linear.input_operand_id, operation_id);
  NoteDependency(quantize_linear.scale_operand_id, operation_id);
  NoteDependency(quantize_linear.zero_point_operand_id, operation_id);

  RETURN_IF_FALSE(
      processed_operands_.insert(quantize_linear.output_operand_id).second);

  auto* input = GetMojoOperand(quantize_linear.input_operand_id);
  auto* output = GetMojoOperand(quantize_linear.output_operand_id);
  auto* scale = GetMojoOperand(quantize_linear.scale_operand_id);
  auto* zero_point = GetMojoOperand(quantize_linear.zero_point_operand_id);
  if (!input || !output || !scale || !zero_point || output == input ||
      output == scale || output == zero_point) {
    // The quantize_linear operator is invalid.
    return false;
  }

  auto validated_output = ValidateQuantizeLinearAndInferOutput(
      *context_properties_, input->descriptor, scale->descriptor,
      zero_point->descriptor, quantize_linear.label);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateResample2d(
    const mojom::Resample2d& resample2d,
    size_t operation_id) {
  if (!processed_operands_.contains(resample2d.input_operand_id)) {
    return false;
  }
  NoteDependency(resample2d.input_operand_id, operation_id);

  RETURN_IF_FALSE(
      processed_operands_.insert(resample2d.output_operand_id).second);

  auto* input = GetMojoOperand(resample2d.input_operand_id);
  auto* output = GetMojoOperand(resample2d.output_operand_id);
  if (!input || !output || output == input) {
    // The resample2d operator is invalid.
    return false;
  }

  // Validate and infer the output for resample2d with given scales or with
  // the sizes from output dimensions along axes.
  absl::variant<base::span<const float>, base::span<const uint32_t>>
      scales_or_sizes;
  const auto& axes = resample2d.axes;
  std::vector<uint32_t> sizes;
  const auto& output_dimensions = output->descriptor.shape();
  if (axes.size() != 2 || axes[0] >= output_dimensions.size() ||
      axes[1] >= output_dimensions.size()) {
    return false;
  }
  const std::array<uint32_t, 2> kResample2dChannelFirstAxes{2u, 3u};
  const std::array<uint32_t, 2> kResample2dChannelLastAxes{1u, 2u};
  switch (context_properties_->resample_2d_axes) {
    case Resample2DAxes::kAny:
      break;
    case Resample2DAxes::kChannelsFirst:
      if (!std::ranges::equal(axes, kResample2dChannelFirstAxes)) {
        return false;
      }
      break;
    case Resample2DAxes::kChannelsLast:
      if (!std::ranges::equal(axes, kResample2dChannelLastAxes)) {
        return false;
      }
      break;
  }
  if (resample2d.scales) {
    scales_or_sizes = resample2d.scales.value();
  } else {
    sizes = {output_dimensions[axes[0]], output_dimensions[axes[1]]};
    scales_or_sizes = sizes;
  }

  auto validated_output =
      ValidateResample2dAndInferOutput(*context_properties_, input->descriptor,
                                       scales_or_sizes, axes, resample2d.label);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateReshape(const mojom::Reshape& reshape,
                                                 size_t operation_id) {
  if (!processed_operands_.contains(reshape.input_operand_id)) {
    return false;
  }
  NoteDependency(reshape.input_operand_id, operation_id);

  RETURN_IF_FALSE(processed_operands_.insert(reshape.output_operand_id).second);

  auto* input = GetMojoOperand(reshape.input_operand_id);
  auto* output = GetMojoOperand(reshape.output_operand_id);
  if (!input || !output || output == input) {
    // The reshape operator is invalid.
    return false;
  }
  if (!context_properties_->data_type_limits.reshape_input.Has(
          input->descriptor.data_type())) {
    return false;
  }
  if (output->descriptor.data_type() != input->descriptor.data_type()) {
    return false;
  }

  if (input->descriptor.NumberOfElements() !=
      output->descriptor.NumberOfElements()) {
    // The output shape is not expected.
    return false;
  }
  return true;
}

bool OperationValidationContext::ValidateScatterElements(
    const mojom::ScatterElements& scatter_elements,
    size_t operation_id) {
  if (!processed_operands_.contains(scatter_elements.input_operand_id) ||
      !processed_operands_.contains(scatter_elements.indices_operand_id) ||
      !processed_operands_.contains(scatter_elements.updates_operand_id)) {
    return false;
  }
  NoteDependency(scatter_elements.input_operand_id, operation_id);
  NoteDependency(scatter_elements.indices_operand_id, operation_id);
  NoteDependency(scatter_elements.updates_operand_id, operation_id);

  RETURN_IF_FALSE(
      processed_operands_.insert(scatter_elements.output_operand_id).second);

  auto* input = GetMojoOperand(scatter_elements.input_operand_id);
  auto* indices = GetMojoOperand(scatter_elements.indices_operand_id);
  auto* updates = GetMojoOperand(scatter_elements.updates_operand_id);
  auto* output = GetMojoOperand(scatter_elements.output_operand_id);
  if (!input || !indices || !updates || !output || output == input ||
      output == indices || output == updates) {
    return false;
  }

  auto validated_output = ValidateScatterElementsAndInferOutput(
      *context_properties_, input->descriptor, indices->descriptor,
      updates->descriptor, scatter_elements.axis, scatter_elements.label);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateScatterND(
    const mojom::ScatterND& scatter_nd,
    size_t operation_id) {
  if (!processed_operands_.contains(scatter_nd.input_operand_id) ||
      !processed_operands_.contains(scatter_nd.indices_operand_id) ||
      !processed_operands_.contains(scatter_nd.updates_operand_id)) {
    return false;
  }
  NoteDependency(scatter_nd.input_operand_id, operation_id);
  NoteDependency(scatter_nd.indices_operand_id, operation_id);
  NoteDependency(scatter_nd.updates_operand_id, operation_id);

  RETURN_IF_FALSE(
      processed_operands_.insert(scatter_nd.output_operand_id).second);

  auto* input = GetMojoOperand(scatter_nd.input_operand_id);
  auto* indices = GetMojoOperand(scatter_nd.indices_operand_id);
  auto* updates = GetMojoOperand(scatter_nd.updates_operand_id);
  auto* output = GetMojoOperand(scatter_nd.output_operand_id);
  if (!input || !indices || !updates || !output || output == input ||
      output == indices || output == updates) {
    return false;
  }

  auto validated_output = ValidateScatterNDAndInferOutput(
      *context_properties_, input->descriptor, indices->descriptor,
      updates->descriptor, scatter_nd.label);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateSlice(const mojom::Slice& slice,
                                               size_t operation_id) {
  if (!processed_operands_.contains(slice.input_operand_id)) {
    return false;
  }
  NoteDependency(slice.input_operand_id, operation_id);

  RETURN_IF_FALSE(processed_operands_.insert(slice.output_operand_id).second);

  auto* input = GetMojoOperand(slice.input_operand_id);
  auto* output = GetMojoOperand(slice.output_operand_id);

  if (!input || !output || output == input) {
    // The slice operator is invalid.
    return false;
  }

  auto validated_output = ValidateSliceAndInferOutput(
      *context_properties_, input->descriptor, ConvertToSliceAttributes(slice));
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateSoftmax(const mojom::Softmax& softmax,
                                                 size_t operation_id) {
  if (!processed_operands_.contains(softmax.input_operand_id)) {
    return false;
  }
  NoteDependency(softmax.input_operand_id, operation_id);

  RETURN_IF_FALSE(processed_operands_.insert(softmax.output_operand_id).second);

  auto* input = GetMojoOperand(softmax.input_operand_id);
  auto* output = GetMojoOperand(softmax.output_operand_id);
  if (!input || !output || output == input) {
    // The softmax operator is invalid.
    return false;
  }
  auto validated_output = ValidateSoftmaxAndInferOutput(
      *context_properties_, input->descriptor, softmax.axis, softmax.label);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateSplit(const mojom::Split& split,
                                               size_t operation_id) {
  if (!processed_operands_.contains(split.input_operand_id)) {
    return false;
  }
  NoteDependency(split.input_operand_id, operation_id);

  auto* input = GetMojoOperand(split.input_operand_id);
  if (!input) {
    // The split operator is invalid.
    return false;
  }
  std::vector<uint32_t> splits;
  splits.reserve(split.output_operand_ids.size());
  for (uint64_t output_id : split.output_operand_ids) {
    auto* output = GetMojoOperand(output_id);
    if (!output || input == output) {
      return false;
    }

    if (split.axis >= output->descriptor.Rank()) {
      return false;
    }
    splits.push_back(output->descriptor.shape()[split.axis]);
    RETURN_IF_FALSE(processed_operands_.insert(output_id).second);
  }

  auto validated_output = ValidateSplitAndInferOutput(
      *context_properties_, input->descriptor,
      {.splits = splits, .axis = split.axis, .label = split.label});
  if (!validated_output.has_value()) {
    return false;
  }

  if (split.output_operand_ids.size() != validated_output->size()) {
    // The number of specified outputs did not match the expected number of
    // outputs.
    return false;
  }

  for (uint32_t i = 0; i < validated_output->size(); ++i) {
    auto* output = GetMojoOperand(split.output_operand_ids[i]);
    if (validated_output->at(i) != output->descriptor) {
      return false;
    }
  }

  return true;
}

bool OperationValidationContext::ValidateTile(const mojom::Tile& tile,
                                              size_t operation_id) {
  if (!processed_operands_.contains(tile.input_operand_id)) {
    return false;
  }
  NoteDependency(tile.input_operand_id, operation_id);

  RETURN_IF_FALSE(processed_operands_.insert(tile.output_operand_id).second);

  auto* input = GetMojoOperand(tile.input_operand_id);
  auto* output = GetMojoOperand(tile.output_operand_id);
  if (!input || !output || output == input) {
    // The tile operator is invalid.
    return false;
  }

  auto validated_output = ValidateTileAndInferOutput(
      *context_properties_, input->descriptor, tile.repetitions, tile.label);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateTranspose(
    const mojom::Transpose& transpose,
    size_t operation_id) {
  if (!processed_operands_.contains(transpose.input_operand_id)) {
    return false;
  }
  NoteDependency(transpose.input_operand_id, operation_id);

  RETURN_IF_FALSE(
      processed_operands_.insert(transpose.output_operand_id).second);

  auto* input = GetMojoOperand(transpose.input_operand_id);
  auto* output = GetMojoOperand(transpose.output_operand_id);
  if (!input || !output || output == input) {
    // The transpose operator is invalid.
    return false;
  }

  auto validated_output =
      ValidateTransposeAndInferOutput(*context_properties_, input->descriptor,
                                      transpose.permutation, transpose.label);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateTriangular(
    const mojom::Triangular& triangular,
    size_t operation_id) {
  if (!processed_operands_.contains(triangular.input_operand_id)) {
    return false;
  }
  NoteDependency(triangular.input_operand_id, operation_id);

  RETURN_IF_FALSE(
      processed_operands_.insert(triangular.output_operand_id).second);

  auto* input = GetMojoOperand(triangular.input_operand_id);
  auto* output = GetMojoOperand(triangular.output_operand_id);
  if (!input || !output || output == input) {
    // The triangular operator is invalid.
    return false;
  }

  base::expected<OperandDescriptor, std::string> validated_output =
      ValidateTriangularAndInferOutput(*context_properties_, input->descriptor,
                                       triangular.label);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateWhere(const mojom::Where& where,
                                               size_t operation_id) {
  if (!processed_operands_.contains(where.condition_operand_id) ||
      !processed_operands_.contains(where.true_value_operand_id) ||
      !processed_operands_.contains(where.false_value_operand_id)) {
    return false;
  }
  NoteDependency(where.condition_operand_id, operation_id);
  NoteDependency(where.true_value_operand_id, operation_id);
  NoteDependency(where.false_value_operand_id, operation_id);

  RETURN_IF_FALSE(processed_operands_.insert(where.output_operand_id).second);

  auto* condition = GetMojoOperand(where.condition_operand_id);
  auto* true_value = GetMojoOperand(where.true_value_operand_id);
  auto* false_value = GetMojoOperand(where.false_value_operand_id);
  auto* output = GetMojoOperand(where.output_operand_id);
  if (!condition || !true_value || !false_value || !output ||
      output == condition || output == true_value || output == false_value) {
    // The where operator is invalid.
    return false;
  }

  auto validated_output_descriptor = ValidateWhereAndInferOutput(
      *context_properties_, condition->descriptor, true_value->descriptor,
      false_value->descriptor, where.label);
  if (!validated_output_descriptor.has_value()) {
    return false;
  }
  if (validated_output_descriptor != output->descriptor) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateReduce(const mojom::Reduce& reduce,
                                                size_t operation_id) {
  if (!processed_operands_.contains(reduce.input_operand_id)) {
    return false;
  }
  NoteDependency(reduce.input_operand_id, operation_id);

  RETURN_IF_FALSE(processed_operands_.insert(reduce.output_operand_id).second);

  auto* input = GetMojoOperand(reduce.input_operand_id);
  auto* output = GetMojoOperand(reduce.output_operand_id);
  if (!input || !output || output == input) {
    // The reduce operator is invalid.
    return false;
  }

  auto validated_output = ValidateReduceAndInferOutput(
      *context_properties_, MojoReduceTypeToComponent(reduce.kind),
      input->descriptor, reduce.label, reduce.axes, reduce.keep_dimensions);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != output->descriptor) {
    return false;
  }

  return true;
}

bool OperationValidationContext::ValidateOperation(
    const mojom::Operation& operation,
    size_t operation_id) {
  switch (operation.which()) {
    case mojom::Operation::Tag::kArgMinMax:
      return ValidateArgMinMax(*operation.get_arg_min_max(), operation_id);
    case mojom::Operation::Tag::kBatchNormalization:
      return ValidateBatchNormalization(*operation.get_batch_normalization(),
                                        operation_id);
    case mojom::Operation::Tag::kClamp:
      return ValidateClamp(*operation.get_clamp(), operation_id);
    case mojom::Operation::Tag::kConcat:
      return ValidateConcat(*operation.get_concat(), operation_id);
    case mojom::Operation::Tag::kConv2d:
      return ValidateConv2d(*operation.get_conv2d(), operation_id);
    case mojom::Operation::Tag::kCumulativeSum:
      return ValidateCumulativeSum(*operation.get_cumulative_sum(),
                                   operation_id);
    case mojom::Operation::Tag::kDequantizeLinear:
      return ValidateDequantizeLinear(*operation.get_dequantize_linear(),
                                      operation_id);
    case mojom::Operation::Tag::kElementWiseBinary:
      return ValidateElementWiseBinary(*operation.get_element_wise_binary(),
                                       operation_id);
    case mojom::Operation::Tag::kElu:
      return ValidateElu(*operation.get_elu(), operation_id);
    case mojom::Operation::Tag::kElementWiseUnary:
      return ValidateElementWiseUnary(*operation.get_element_wise_unary(),
                                      operation_id);
    case mojom::Operation::Tag::kExpand:
      return ValidateExpand(*operation.get_expand(), operation_id);
    case mojom::Operation::Tag::kGather:
      return ValidateGather(*operation.get_gather(), operation_id);
    case mojom::Operation::Tag::kGatherElements:
      return ValidateGatherElements(*operation.get_gather_elements(),
                                    operation_id);
    case mojom::Operation::Tag::kGatherNd:
      return ValidateGatherND(*operation.get_gather_nd(), operation_id);
    case mojom::Operation::Tag::kGelu:
      return ValidateUnaryOperation(
          *operation.get_gelu(),
          context_properties_->data_type_limits.gelu_input, operation_id);
    case mojom::Operation::Tag::kGemm:
      return ValidateGemm(*operation.get_gemm(), operation_id);
    case mojom::Operation::Tag::kGru:
      return ValidateGru(*operation.get_gru(), operation_id);
    case mojom::Operation::Tag::kGruCell:
      return ValidateGruCell(*operation.get_gru_cell(), operation_id);
    case mojom::Operation::Tag::kHardSigmoid:
      return ValidateHardSigmoid(*operation.get_hard_sigmoid(), operation_id);
    case mojom::Operation::Tag::kHardSwish:
      return ValidateUnaryOperation(
          *operation.get_hard_swish(),
          context_properties_->data_type_limits.hard_swish_input, operation_id);
    case mojom::Operation::Tag::kLayerNormalization:
      return ValidateLayerNormalization(*operation.get_layer_normalization(),
                                        operation_id);
    case mojom::Operation::Tag::kInstanceNormalization:
      return ValidateInstanceNormalization(
          *operation.get_instance_normalization(), operation_id);
    case mojom::Operation::Tag::kLeakyRelu:
      return ValidateLeakyRelu(*operation.get_leaky_relu(), operation_id);
    case mojom::Operation::Tag::kLinear:
      return ValidateLinear(*operation.get_linear(), operation_id);
    case mojom::Operation::Tag::kLstm:
      return ValidateLstm(*operation.get_lstm(), operation_id);
    case mojom::Operation::Tag::kLstmCell:
      return ValidateLstmCell(*operation.get_lstm_cell(), operation_id);
    case mojom::Operation::Tag::kMatmul:
      return ValidateMatmul(*operation.get_matmul(), operation_id);
    case mojom::Operation::Tag::kPad:
      return ValidatePad(*operation.get_pad(), operation_id);
    case mojom::Operation::Tag::kPool2d:
      return ValidatePool2d(*operation.get_pool2d(), operation_id);
    case mojom::Operation::Tag::kPrelu:
      return ValidatePrelu(*operation.get_prelu(), operation_id);
    case mojom::Operation::Tag::kQuantizeLinear:
      return ValidateQuantizeLinear(*operation.get_quantize_linear(),
                                    operation_id);
    case mojom::Operation::Tag::kReduce:
      return ValidateReduce(*operation.get_reduce(), operation_id);
    case mojom::Operation::Tag::kResample2d:
      return ValidateResample2d(*operation.get_resample2d(), operation_id);
    case mojom::Operation::Tag::kReshape:
      return ValidateReshape(*operation.get_reshape(), operation_id);
    case mojom::Operation::Tag::kRelu:
      return ValidateUnaryOperation(
          *operation.get_relu(),
          context_properties_->data_type_limits.relu_input, operation_id);
    case mojom::Operation::Tag::kScatterElements:
      return ValidateScatterElements(*operation.get_scatter_elements(),
                                     operation_id);
    case mojom::Operation::Tag::kScatterNd:
      return ValidateScatterND(*operation.get_scatter_nd(), operation_id);
    case mojom::Operation::Tag::kSlice:
      return ValidateSlice(*operation.get_slice(), operation_id);
    case mojom::Operation::Tag::kSigmoid:
      return ValidateUnaryOperation(
          *operation.get_sigmoid(),
          context_properties_->data_type_limits.sigmoid_input, operation_id);
    case mojom::Operation::Tag::kSoftmax:
      return ValidateSoftmax(*operation.get_softmax(), operation_id);
    case mojom::Operation::Tag::kSoftplus:
      return ValidateUnaryOperation(
          *operation.get_softplus(),
          context_properties_->data_type_limits.softplus_input, operation_id);
    case mojom::Operation::Tag::kSoftsign:
      return ValidateUnaryOperation(
          *operation.get_softsign(),
          context_properties_->data_type_limits.softsign_input, operation_id);
    case mojom::Operation::Tag::kSplit:
      return ValidateSplit(*operation.get_split(), operation_id);
    case mojom::Operation::Tag::kTanh:
      return ValidateUnaryOperation(
          *operation.get_tanh(),
          context_properties_->data_type_limits.tanh_input, operation_id);
    case mojom::Operation::Tag::kTile:
      return ValidateTile(*operation.get_tile(), operation_id);
    case mojom::Operation::Tag::kTranspose:
      return ValidateTranspose(*operation.get_transpose(), operation_id);
    case mojom::Operation::Tag::kTriangular:
      return ValidateTriangular(*operation.get_triangular(), operation_id);
    case mojom::Operation::Tag::kWhere:
      return ValidateWhere(*operation.get_where(), operation_id);
  }
}

}  // namespace

WebNNGraphBuilderImpl::WebNNGraphBuilderImpl(WebNNContextImpl& context)
    : context_(context) {}

WebNNGraphBuilderImpl::~WebNNGraphBuilderImpl() = default;

void WebNNGraphBuilderImpl::CreateGraph(mojom::GraphInfoPtr graph_info,
                                        CreateGraphCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (has_built_) {
    context_->ReportBadGraphBuilderMessage(
        kBadMessageOnBuiltGraphBuilder, base::PassKey<WebNNGraphBuilderImpl>());
    return;
  }

  has_built_ = true;

  auto compute_resource_info =
      ValidateGraph(context_->properties(), *graph_info);
  if (!compute_resource_info.has_value()) {
    context_->ReportBadGraphBuilderMessage(
        kBadMessageInvalidGraph, base::PassKey<WebNNGraphBuilderImpl>());
    return;
  }

  base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
      constant_operands = TakeConstants(*graph_info);

  context_->CreateGraphImpl(
      std::move(graph_info), *std::move(compute_resource_info),
      std::move(constant_operands),
      base::BindOnce(&WebNNGraphBuilderImpl::DidCreateGraph,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void WebNNGraphBuilderImpl::SetId(
    mojo::ReceiverId id,
    base::PassKey<WebNNContextImpl> /*pass_key*/) {
  id_ = id;
}

void WebNNGraphBuilderImpl::DidCreateGraph(
    CreateGraphCallback callback,
    base::expected<std::unique_ptr<WebNNGraphImpl>, mojom::ErrorPtr> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Ensure `this` is destroyed.
  base::ScopedClosureRunner destroy_self_closure(base::BindOnce(
      &WebNNGraphBuilderImpl::DestroySelf, weak_factory_.GetWeakPtr()));

  if (!result.has_value()) {
    std::move(callback).Run(
        mojom::CreateGraphResult::NewError(std::move(result.error())));
    return;
  }

  mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver;
  std::move(callback).Run(mojom::CreateGraphResult::NewGraphRemote(
      receiver.InitWithNewEndpointAndPassRemote()));

  context_->TakeGraph(*std::move(result), std::move(receiver),
                      base::PassKey<WebNNGraphBuilderImpl>());
}

// static
std::optional<WebNNGraphImpl::ComputeResourceInfo>
WebNNGraphBuilderImpl::ValidateGraph(
    const ContextProperties& context_properties,
    const mojom::GraphInfo& graph_info) {
  // The input operands of graph can be empty.
  if (graph_info.id_to_operand_map.empty() || graph_info.operations.empty() ||
      graph_info.output_operands.empty()) {
    return std::nullopt;
  }

  // Keeps track of operands as they are visited in order to assert that they
  // are topologically sorted with inputs pointing to predecessor's outputs or
  // graph inputs.
  base::flat_set<uint64_t> processed_operands;

  // Keeps track of input and output names in order to assert they are unique.
  base::flat_map<std::string, OperandDescriptor> inputs;
  base::flat_map<std::string, OperandDescriptor> outputs;
  inputs.reserve(graph_info.input_operands.size());
  outputs.reserve(graph_info.output_operands.size());

  // Validate all operands in the graph for the dimensions and the byte length
  // of operand that can't be out of range, and hold the temporary information
  // of inputs, constants, outputs for further validation.
  std::vector<uint64_t> graph_inputs;
  graph_inputs.reserve(graph_info.input_operands.size());
  std::vector<uint64_t> graph_outputs;
  graph_outputs.reserve(graph_info.output_operands.size());
  base::flat_map<uint64_t, size_t> constant_ids_to_byte_lengths;
  constant_ids_to_byte_lengths.reserve(
      graph_info.constant_id_to_buffer_map.size());

  // The operand id must start from 1.
  uint64_t expected_operand_id = 1;
  for (auto& [id, operand] : graph_info.id_to_operand_map) {
    // Validate that the operand ids are increasing and contiguous.
    if (id != expected_operand_id++) {
      return std::nullopt;
    }
    const std::optional<std::string>& name = operand->name;
    switch (operand->kind) {
      case mojom::Operand::Kind::kInput: {
        if (!name || name.value().empty()) {
          // The name of input is empty.
          return std::nullopt;
        }
        if (!inputs.try_emplace(*name, operand->descriptor).second) {
          // Input names must be unique.
          return std::nullopt;
        }
        if (!context_properties.data_type_limits.input.Has(
                operand->descriptor.data_type())) {
          // Input data type not supported.
          return std::nullopt;
        }
        graph_inputs.push_back(id);
        processed_operands.insert(id);
        break;
      }
      case mojom::Operand::Kind::kOutput: {
        // The intermediate operands have no the name value, only the graph
        // outputs have the name.
        if (name) {
          if (name.value().empty()) {
            // The name of output is empty.
            return std::nullopt;
          }
          if (!outputs.try_emplace(*name, operand->descriptor).second) {
            // Output names must be unique.
            return std::nullopt;
          }
          if (!context_properties.data_type_limits.input.Has(
                  operand->descriptor.data_type())) {
            // Output data type not supported.
            return std::nullopt;
          }
          graph_outputs.push_back(id);
        } else {
          // The intermediate operand that connects with two operators has no
          // the name value.
        }
        break;
      }
      case mojom::Operand::Kind::kConstant: {
        if (name) {
          // Constant operand should not have a name.
          return std::nullopt;
        }

        constant_ids_to_byte_lengths[id] =
            operand->descriptor.PackedByteLength();

        processed_operands.insert(id);
        break;
      }
    }
  }

  // The `id_to_operand_map` is an ordered map, so the `graph_inputs` and
  // `graph_outputs` are also an ordered array for the value id, the
  // `input_operands` and `graph_outputs` are also an ordered array configured
  // in blink side.
  if (graph_info.input_operands != graph_inputs ||
      graph_info.output_operands != graph_outputs) {
    return std::nullopt;
  }

  // Validate the constant weight data are valid.
  if (!base::ranges::equal(graph_info.constant_id_to_buffer_map,
                           constant_ids_to_byte_lengths,
                           [](const auto& iter_a, const auto& iter_b) {
                             // Compare the constant id with the key of map and
                             // the byte length of buffer with value of map.
                             return iter_a.first == iter_b.first &&
                                    iter_a.second.size() == iter_b.second;
                           })) {
    return std::nullopt;
  }

  // Validate the operations which are sorted in the topological order.
  std::optional<DependentOperationsMap> operands_to_dependent_operations =
      OperationValidationContext::ValidateOperationsAndGetDependencies(
          graph_info.operations, context_properties,
          graph_info.id_to_operand_map, std::move(processed_operands));
  if (!operands_to_dependent_operations.has_value()) {
    return std::nullopt;
  }

  return WebNNGraphImpl::ComputeResourceInfo(
      std::move(inputs), std::move(outputs),
      *std::move(operands_to_dependent_operations),
      base::PassKey<WebNNGraphBuilderImpl>());
}

// static
bool WebNNGraphBuilderImpl::IsValidForTesting(
    const ContextProperties& context_properties,
    const mojom::GraphInfo& graph_info) {
  return ValidateGraph(context_properties, graph_info).has_value();
}

// static
base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
WebNNGraphBuilderImpl::TakeConstants(mojom::GraphInfo& graph_info) {
  std::vector<std::pair<uint64_t, std::unique_ptr<WebNNConstantOperand>>>
      constant_operands;
  constant_operands.reserve(graph_info.constant_id_to_buffer_map.size());

  for (auto it = graph_info.constant_id_to_buffer_map.begin();
       it != graph_info.constant_id_to_buffer_map.end();) {
    const auto* operand =
        GetMojoOperand(graph_info.id_to_operand_map, it->first);
    CHECK(operand);
    constant_operands.emplace_back(
        it->first, std::make_unique<WebNNConstantOperand>(operand->descriptor,
                                                          it->second));
    // Destroy the `BigBuffer` immediately after copying it, to avoid ending up
    // holding two copies of the all weights simultaneously by the last
    // iteration of this loop.
    it = graph_info.constant_id_to_buffer_map.erase(it);
  }

  return base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>(
      std::move(constant_operands));
}

void WebNNGraphBuilderImpl::DestroySelf() {
  context_->RemoveGraphBuilder(id_, base::PassKey<WebNNGraphBuilderImpl>());
}

}  // namespace webnn
