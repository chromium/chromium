// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_graph_impl.h"

#include <math.h>
#include <utility>
#include <vector>

#include "base/ranges/algorithm.h"
#include "base/types/expected.h"
#include "components/ml/webnn/graph_validation_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_WIN)
#include "services/webnn/dml/graph_impl.h"
#endif

namespace webnn {

namespace {

// Maps the id to its `mojo::Operand`.
using IdToOperandMap = base::flat_map<uint64_t, mojom::OperandPtr>;

size_t GetBytesPerElement(mojom::Operand::DataType operand_type) {
  switch (operand_type) {
    case mojom::Operand::DataType::kFloat32:
      return sizeof(float);
    case mojom::Operand::DataType::kFloat16:
      return sizeof(uint16_t);
    case mojom::Operand::DataType::kInt32:
      return sizeof(int32_t);
    case mojom::Operand::DataType::kUint32:
      return sizeof(uint32_t);
    case mojom::Operand::DataType::kInt8:
      return sizeof(int8_t);
    case mojom::Operand::DataType::kUint8:
      return sizeof(uint8_t);
  }
  NOTREACHED();
}

webnn::Operand::DataType MojoOperandTypeToComponent(
    mojom::Operand::DataType data_type) {
  switch (data_type) {
    case mojom::Operand::DataType::kFloat32:
      return webnn::Operand::DataType::kFloat32;
    case mojom::Operand::DataType::kFloat16:
      return webnn::Operand::DataType::kFloat16;
    case mojom::Operand::DataType::kInt32:
      return webnn::Operand::DataType::kInt32;
    case mojom::Operand::DataType::kUint32:
      return webnn::Operand::DataType::kUint32;
    case mojom::Operand::DataType::kInt8:
      return webnn::Operand::DataType::kInt8;
    case mojom::Operand::DataType::kUint8:
      return webnn::Operand::DataType::kUint8;
  }
  NOTREACHED_NORETURN();
}

webnn::Operand ConvertToComponentOperand(const mojom::Operand* mojo_operand) {
  return webnn::Operand(MojoOperandTypeToComponent(mojo_operand->data_type),
                        mojo_operand->dimensions);
}

webnn::InputOperandLayout MojoInputOperandLayoutToComponent(
    webnn::mojom::InputOperandLayout layout) {
  switch (layout) {
    case webnn::mojom::InputOperandLayout::kChannelsFirst:
      return webnn::InputOperandLayout::kNchw;
    case webnn::mojom::InputOperandLayout::kChannelsLast:
      return webnn::InputOperandLayout::kNhwc;
  }
  NOTREACHED_NORETURN();
}

bool ValidateClampAttributes(
    const webnn::mojom::OperatorAttributesPtr& attributes) {
  if (!attributes || !attributes->is_clamp()) {
    // The type of attribute is not clamp.
    return false;
  }
  auto& clamp_attributes = attributes->get_clamp();
  if (!clamp_attributes) {
    // The attributes of clamp were not configured.
    return false;
  }
  if (std::isnan(clamp_attributes->min_value) ||
      std::isnan(clamp_attributes->max_value)) {
    // The min or max value are nan.
    return false;
  }
  if (clamp_attributes->min_value >= clamp_attributes->max_value) {
    // The min value must be below the max value.
    return false;
  }
  return true;
}

bool ValidateActivation(const mojom::OperatorPtr& activation) {
  switch (activation->kind) {
    case mojom::Operator::Kind::kClamp:
      return ValidateClampAttributes(activation->attributes);
    case mojom::Operator::Kind::kRelu:
      return true;
    default:
      // The activation is not supported.
      return false;
  }
}

absl::optional<webnn::Conv2dAttributes> ConvertToConv2dAttributes(
    const IdToOperandMap& id_to_operand_map,
    const webnn::mojom::OperatorAttributesPtr& attributes) {
  if (!attributes->is_conv2d()) {
    // The type of attribute is not conv2d.
    return absl::nullopt;
  }
  auto& mojo_attributes = attributes->get_conv2d();
  if (!mojo_attributes) {
    // The attributes of conv2d were not configured.
    return absl::nullopt;
  }

  webnn::Conv2dAttributes component_attributes;
  // Convert padding, strides, dilations.
  auto& mojo_padding = mojo_attributes->padding;
  component_attributes.padding = webnn::Padding2d{
      .beginning = webnn::Size2d{.height = mojo_padding->beginning->height,
                                 .width = mojo_padding->beginning->width},
      .ending = webnn::Size2d{.height = mojo_padding->ending->height,
                              .width = mojo_padding->ending->width}};
  component_attributes.strides =
      webnn::Size2d{.height = mojo_attributes->strides->height,
                    .width = mojo_attributes->strides->width};
  component_attributes.dilations =
      webnn::Size2d{.height = mojo_attributes->dilations->height,
                    .width = mojo_attributes->dilations->width};

  // Convert groups, input and filter layout.
  component_attributes.groups = mojo_attributes->groups;
  component_attributes.input_layout =
      MojoInputOperandLayoutToComponent(mojo_attributes->input_layout);
  // The filter only supports default `Oihw` layout in mojo definition, other
  // variants are being discussed in WebNN working group:
  // https://github.com/webmachinelearning/webnn/issues/324.
  component_attributes.filter_layout = webnn::Conv2dFilterOperandLayout::kOihw;

  // Convert to componment operand type with bias id.
  auto& bias_operand_id = mojo_attributes->bias_operand_id;
  if (bias_operand_id) {
    if (!id_to_operand_map.contains(bias_operand_id.value())) {
      // Invalid bias operand.
      return absl::nullopt;
    }
    const mojom::OperandPtr& bias_operand =
        id_to_operand_map.at(bias_operand_id.value());
    component_attributes.bias_operand =
        ConvertToComponentOperand(bias_operand.get());
  }

  // Validate the activation if the option is configured.
  auto& activation = mojo_attributes->activation;
  if (activation && !ValidateActivation(activation)) {
    // The activation is invalid.
    return absl::nullopt;
  }

  return component_attributes;
}

absl::optional<webnn::Pool2dAttributes> ConvertToPool2dAttributes(
    const webnn::mojom::OperatorAttributesPtr& attributes,
    const mojom::Operand* output) {
  if (!attributes->is_pool2d()) {
    // The type of attribute is not pool2d.
    return absl::nullopt;
  }
  auto& mojo_attributes = attributes->get_pool2d();
  if (!mojo_attributes) {
    // The attributes of pool2d were not configured.
    return absl::nullopt;
  }
  if (output->dimensions.size() != 4) {
    // The element of output dimensions should be 4.
    return absl::nullopt;
  }

  webnn::Pool2dAttributes component_attributes;
  auto& window_dimensions = mojo_attributes->window_dimensions;
  component_attributes.window_dimensions = webnn::Size2d{
      .height = window_dimensions->height, .width = window_dimensions->width};
  auto& mojo_padding = mojo_attributes->padding;
  component_attributes.padding = webnn::Padding2d{
      .beginning = webnn::Size2d{.height = mojo_padding->beginning->height,
                                 .width = mojo_padding->beginning->width},
      .ending = webnn::Size2d{.height = mojo_padding->ending->height,
                              .width = mojo_padding->ending->width}};
  component_attributes.strides =
      webnn::Size2d{.height = mojo_attributes->strides->height,
                    .width = mojo_attributes->strides->width};
  component_attributes.dilations =
      webnn::Size2d{.height = mojo_attributes->dilations->height,
                    .width = mojo_attributes->dilations->width};
  component_attributes.layout =
      MojoInputOperandLayoutToComponent(mojo_attributes->layout);
  switch (component_attributes.layout) {
    case webnn::InputOperandLayout::kNchw:
      component_attributes.output_sizes = webnn::Size2d{
          .height = output->dimensions[2], .width = output->dimensions[3]};
      break;
    case webnn::InputOperandLayout::kNhwc:
      component_attributes.output_sizes = webnn::Size2d{
          .height = output->dimensions[1], .width = output->dimensions[2]};
      break;
  }
  return component_attributes;
}

absl::optional<webnn::GemmAttributes> ConvertToGemmAttributes(
    const IdToOperandMap& id_to_operand_map,
    const webnn::mojom::OperatorAttributesPtr& attributes) {
  if (!attributes->is_gemm()) {
    // The type of attribute is not gemm.
    return absl::nullopt;
  }
  auto& mojo_attributes = attributes->get_gemm();
  if (!mojo_attributes) {
    // The attributes of gemm were not configured.
    return absl::nullopt;
  }
  webnn::GemmAttributes component_attributes;
  auto& c_operand_id = mojo_attributes->c_operand_id;
  if (c_operand_id) {
    if (!id_to_operand_map.contains(c_operand_id.value())) {
      // The third operand is invalid.
      return absl::nullopt;
    }
    const mojom::OperandPtr& c_operand =
        id_to_operand_map.at(c_operand_id.value());
    component_attributes.c_operand = ConvertToComponentOperand(c_operand.get());
  }
  component_attributes.alpha = mojo_attributes->alpha;
  component_attributes.beta = mojo_attributes->beta;
  component_attributes.a_transpose = mojo_attributes->a_transpose;
  component_attributes.b_transpose = mojo_attributes->b_transpose;
  return component_attributes;
}

const mojom::Operand* GetMojoOperand(
    const IdToOperandMap& id_to_operand_map,
    const std::vector<uint64_t>& operand_id_array,
    size_t index = 0) {
  if (index >= operand_id_array.size()) {
    // Index out of range.
    return nullptr;
  }
  uint64_t operand_id = operand_id_array[index];
  if (!id_to_operand_map.contains(operand_id)) {
    // There is no operand for the id.
    return nullptr;
  }
  return id_to_operand_map.at(operand_id).get();
}

bool ValidateClamp(const IdToOperandMap& id_to_operand_map,
                   const mojom::OperatorPtr& operation) {
  auto* input = GetMojoOperand(id_to_operand_map, operation->input_operands);
  auto* output = GetMojoOperand(id_to_operand_map, operation->output_operands);
  if (!input || !output || !operation->attributes) {
    // The clamp operator is invalid.
    return false;
  }
  if (!ValidateClampAttributes(operation->attributes)) {
    // The attributes of clamp are invalid.
    return false;
  }
  if (output->data_type != input->data_type) {
    // The output data type doesn't match input data type.
    return false;
  }

  if (output->dimensions != input->dimensions) {
    // The output shape is not expected.
    return false;
  }

  return true;
}

bool ValidateConv2d(const IdToOperandMap& id_to_operand_map,
                    const mojom::OperatorPtr& operation) {
  auto* input = GetMojoOperand(id_to_operand_map, operation->input_operands, 0);
  auto* filter =
      GetMojoOperand(id_to_operand_map, operation->input_operands, 1);
  auto* output = GetMojoOperand(id_to_operand_map, operation->output_operands);
  if (!input || !filter || !output || !operation->attributes) {
    // The conv2d operator is invalid.
    return false;
  }
  auto component_attributes =
      ConvertToConv2dAttributes(id_to_operand_map, operation->attributes);
  if (!component_attributes) {
    // Failed to convert the attributes of conv2d.
    return false;
  }
  auto validated_output = ValidateConv2dAndInferOutput(
      ConvertToComponentOperand(input), ConvertToComponentOperand(filter),
      std::move(component_attributes.value()));
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != ConvertToComponentOperand(output)) {
    return false;
  }

  return true;
}

bool ValidateElementWiseBinary(const IdToOperandMap& id_to_operand_map,
                               const mojom::OperatorPtr& operation) {
  auto* a = GetMojoOperand(id_to_operand_map, operation->input_operands, 0);
  auto* b = GetMojoOperand(id_to_operand_map, operation->input_operands, 1);
  auto* output = GetMojoOperand(id_to_operand_map, operation->output_operands);
  if (!a || !b || !output) {
    // The elementWise binary operator is invalid.
    return false;
  }
  if (a->data_type != b->data_type || output->data_type != a->data_type) {
    // The input types don't match.
    return false;
  }

  auto dims_output = BroadcastShapes(a->dimensions, b->dimensions);
  if (!dims_output) {
    // The input shapes are not broadcastable.
    return false;
  }
  if (output->dimensions != dims_output.value()) {
    // The output shape is not expected.
    return false;
  }
  return true;
}

bool ValidateGemm(const IdToOperandMap& id_to_operand_map,
                  const mojom::OperatorPtr& operation) {
  auto* a = GetMojoOperand(id_to_operand_map, operation->input_operands, 0);
  auto* b = GetMojoOperand(id_to_operand_map, operation->input_operands, 1);
  auto* output = GetMojoOperand(id_to_operand_map, operation->output_operands);
  if (!a || !b || !output || !operation->attributes) {
    // The gemm operator is invalid.
    return false;
  }
  auto component_attributes =
      ConvertToGemmAttributes(id_to_operand_map, operation->attributes);
  if (!component_attributes) {
    return false;
  }
  auto validated_output = ValidateGemmAndInferOutput(
      ConvertToComponentOperand(a), ConvertToComponentOperand(b),
      component_attributes.value());
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != ConvertToComponentOperand(output)) {
    return false;
  }

  return true;
}

bool ValidatePool2d(const IdToOperandMap& id_to_operand_map,
                    const mojom::OperatorPtr& operation) {
  auto* input = GetMojoOperand(id_to_operand_map, operation->input_operands);
  auto* output = GetMojoOperand(id_to_operand_map, operation->output_operands);
  if (!input || !output || !operation->attributes) {
    // The pool2d operator is invalid.
    return false;
  }
  auto component_attributes =
      ConvertToPool2dAttributes(operation->attributes, output);
  if (!component_attributes) {
    // Failed to convert the attributes of pool2d.
    return false;
  }
  auto validated_output = ValidatePool2dAndInferOutput(
      ConvertToComponentOperand(input), component_attributes.value());
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != ConvertToComponentOperand(output)) {
    return false;
  }

  return true;
}

bool ValidateRelu(const IdToOperandMap& id_to_operand_map,
                  const mojom::OperatorPtr& operation) {
  auto* input = GetMojoOperand(id_to_operand_map, operation->input_operands);
  auto* output = GetMojoOperand(id_to_operand_map, operation->output_operands);
  if (!input || !output) {
    // The relu operator is invalid.
    return false;
  }
  if (output->data_type != input->data_type) {
    // The output data type doesn't match input data type.
    return false;
  }

  if (output->dimensions != input->dimensions) {
    // The output shape is not expected.
    return false;
  }
  return true;
}

bool ValidateReshape(const IdToOperandMap& id_to_operand_map,
                     const mojom::OperatorPtr& operation) {
  auto* input = GetMojoOperand(id_to_operand_map, operation->input_operands);
  auto* output = GetMojoOperand(id_to_operand_map, operation->output_operands);
  if (!input || !output) {
    // The reshape operator is invalid.
    return false;
  }
  if (output->data_type != input->data_type) {
    // The output data type doesn't match input data type.
    return false;
  }

  base::expected<size_t, std::string> output_number_of_elements =
      ValidateAndCalculateElementsNumber(output->dimensions);
  // The dimensions of input and output operand are valid which were already
  // validated before calling this function.
  CHECK(output_number_of_elements.has_value());
  base::expected<size_t, std::string> input_number_of_elements =
      ValidateAndCalculateElementsNumber(input->dimensions);
  CHECK(input_number_of_elements.has_value());
  if (output_number_of_elements.value() != input_number_of_elements.value()) {
    // The output shape is not expected.
    return false;
  }
  return true;
}

bool ValidateSoftmax(const IdToOperandMap& id_to_operand_map,
                     const mojom::OperatorPtr& operation) {
  auto* input = GetMojoOperand(id_to_operand_map, operation->input_operands);
  auto* output = GetMojoOperand(id_to_operand_map, operation->output_operands);
  if (!input || !output) {
    // The softmax operator is invalid.
    return false;
  }
  auto validated_output =
      ValidateSoftmaxAndInferOutput(ConvertToComponentOperand(input));
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != ConvertToComponentOperand(output)) {
    return false;
  }

  return true;
}

bool ValidateOperator(const IdToOperandMap& id_to_operand_map,
                      const mojom::OperatorPtr& operation) {
  switch (operation->kind) {
    case mojom::Operator::Kind::kClamp:
      return ValidateClamp(id_to_operand_map, operation);
    case mojom::Operator::Kind::kConv2d:
      return ValidateConv2d(id_to_operand_map, operation);
    case mojom::Operator::Kind::kAdd:
    case mojom::Operator::Kind::kSub:
    case mojom::Operator::Kind::kMul:
    case mojom::Operator::Kind::kDiv:
    case mojom::Operator::Kind::kMax:
    case mojom::Operator::Kind::kMin:
      return ValidateElementWiseBinary(id_to_operand_map, operation);
    case mojom::Operator::Kind::kGemm:
      return ValidateGemm(id_to_operand_map, operation);
    case mojom::Operator::Kind::kAveragePool2d:
    case mojom::Operator::Kind::kMaxPool2d:
      return ValidatePool2d(id_to_operand_map, operation);
    case mojom::Operator::Kind::kRelu:
      return ValidateRelu(id_to_operand_map, operation);
    case mojom::Operator::Kind::kReshape:
      return ValidateReshape(id_to_operand_map, operation);
    case mojom::Operator::Kind::kSoftmax:
      return ValidateSoftmax(id_to_operand_map, operation);
  }
  NOTREACHED_NORETURN();
}

}  // namespace

WebNNGraphImpl::ComputeResourceInfo::ComputeResourceInfo(
    const mojom::GraphInfoPtr& graph_info) {
  // Calculate the byte length of inputs for validating before computing.
  for (auto& input_id : graph_info->input_operands) {
    const mojom::OperandPtr& operand =
        graph_info->id_to_operand_map.at(input_id);
    // The `operand` is valid and the byte length of it was already verified in
    // `ValidateGraph` function.
    CHECK(operand);
    auto byte_length = ValidateAndCalculateByteLength(
        GetBytesPerElement(operand->data_type), operand->dimensions);
    CHECK(byte_length.has_value());
    input_name_to_byte_length_map[operand->name.value()] = byte_length.value();
  }
}

WebNNGraphImpl::ComputeResourceInfo::~ComputeResourceInfo() = default;

WebNNGraphImpl::WebNNGraphImpl(
    std::unique_ptr<ComputeResourceInfo> compute_resource_info)
    : compute_resource_info_(std::move(compute_resource_info)) {}

WebNNGraphImpl::~WebNNGraphImpl() = default;

bool WebNNGraphImpl::ValidateGraph(const mojom::GraphInfoPtr& graph_info) {
  // The input operands of graph can be empty.
  if (graph_info->id_to_operand_map.empty() || graph_info->operators.empty() ||
      graph_info->output_operands.empty()) {
    return false;
  }

  // Validate all operands in the graph for the dimensions and the byte length
  // of operand that can't be out of range, and hold the temporary information
  // of inputs, constants, outputs for further validation.
  std::vector<uint64_t> graph_inputs;
  graph_inputs.reserve(graph_info->input_operands.size());
  std::vector<uint64_t> graph_outputs;
  graph_outputs.reserve(graph_info->output_operands.size());
  base::flat_map<uint64_t, size_t> constant_id_to_byte_length_map;
  for (auto& [id, operand] : graph_info->id_to_operand_map) {
    base::expected<size_t, std::string> byte_length =
        ValidateAndCalculateByteLength(GetBytesPerElement(operand->data_type),
                                       operand->dimensions);
    if (!byte_length.has_value()) {
      return false;
    }

    const absl::optional<std::string>& name = operand->name;
    switch (operand->kind) {
      case mojom::Operand::Kind::kInput: {
        if (!name || name.value().empty()) {
          // The name of input is empty.
          return false;
        }
        graph_inputs.push_back(id);
        break;
      }
      case mojom::Operand::Kind::kOutput: {
        // The intermediate operands have no the name value, only the graph
        // outputs have the name.
        if (name) {
          if (name.value().empty()) {
            // The name of output is empty.
            return false;
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
          return false;
        }
        constant_id_to_byte_length_map[id] = byte_length.value();
        break;
      }
    }
  }

  // The `id_to_operand_map` is an ordered map, so the `graph_inputs` and
  // `graph_outputs` are also an ordered array for the value id, the
  // `input_operands` and `graph_outputs` are also an ordered array configured
  // in blink side.
  if (graph_info->input_operands != graph_inputs ||
      graph_info->output_operands != graph_outputs) {
    return false;
  }

  // Validate the constant weight data are valid.
  if (!base::ranges::equal(graph_info->constant_id_to_buffer_map,
                           constant_id_to_byte_length_map,
                           [](const auto& iter_a, const auto& iter_b) {
                             // Compare the constant id with the key of map and
                             // the byte length of buffer with value of map.
                             return iter_a.first == iter_b.first &&
                                    iter_a.second.size() == iter_b.second;
                           })) {
    return false;
  }

  // Validate the operators which are sorted in the topological order.
  for (auto& operation : graph_info->operators) {
    if (!ValidateOperator(graph_info->id_to_operand_map, operation)) {
      return false;
    }
  }

  return true;
}

void WebNNGraphImpl::Compute(
    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs,
    mojom::WebNNGraph::ComputeCallback callback) {
  // Validate the inputs for computation match the built graph's expected.
  if (!base::ranges::equal(
          named_inputs, compute_resource_info_->input_name_to_byte_length_map,
          [](const auto& iter_a, const auto& iter_b) {
            // Compare the input name with the key of map and the byte length of
            // buffer with value of map.
            return iter_a.first == iter_b.first &&
                   iter_a.second.size() == iter_b.second;
          })) {
    std::move(callback).Run(mojom::ComputeResult::kInvalidInputs,
                            absl::nullopt);
    return;
  }

  // Call ComputeImpl() implemented by an `mojom::WebNNGraph` backend.
  ComputeImpl(std::move(named_inputs), std::move(callback));
}

}  // namespace webnn
