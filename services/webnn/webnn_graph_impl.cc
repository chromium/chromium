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

bool ValidateClampAttributes(const mojom::ClampPtr& clamp) {
  if (std::isnan(clamp->min_value) || std::isnan(clamp->max_value)) {
    // The min or max value are nan.
    return false;
  }
  if (clamp->min_value >= clamp->max_value) {
    // The min value must be below the max value.
    return false;
  }
  return true;
}

bool ValidateActivation(const mojom::ActivationPtr& activation) {
  switch (activation->which()) {
    case mojom::Activation::Tag::kClamp:
      return ValidateClampAttributes(activation->get_clamp());
    case mojom::Activation::Tag::kRelu:
    case mojom::Activation::Tag::kSigmoid:
    case mojom::Activation::Tag::kSoftmax:
    case mojom::Activation::Tag::kTanh:
      return true;
  }
  NOTREACHED_NORETURN();
}

webnn::Conv2dAttributes ConvertToConv2dAttributes(
    const IdToOperandMap& id_to_operand_map,
    const webnn::mojom::Conv2dPtr& conv2d) {
  webnn::Conv2dAttributes component_attributes;
  // Convert padding, strides, dilations.
  auto& mojo_padding = conv2d->padding;
  component_attributes.padding = webnn::Padding2d{
      .beginning =
          webnn::Size2d<uint32_t>{.height = mojo_padding->beginning->height,
                                  .width = mojo_padding->beginning->width},
      .ending = webnn::Size2d<uint32_t>{.height = mojo_padding->ending->height,
                                        .width = mojo_padding->ending->width}};
  component_attributes.strides = webnn::Size2d<uint32_t>{
      .height = conv2d->strides->height, .width = conv2d->strides->width};
  component_attributes.dilations = webnn::Size2d<uint32_t>{
      .height = conv2d->dilations->height, .width = conv2d->dilations->width};

  // Convert groups, input and filter layout.
  component_attributes.groups = conv2d->groups;
  component_attributes.input_layout =
      MojoInputOperandLayoutToComponent(conv2d->input_layout);
  // The filter only supports default `Oihw` layout in mojo definition, other
  // variants are being discussed in WebNN working group:
  // https://github.com/webmachinelearning/webnn/issues/324.
  component_attributes.filter_layout = webnn::Conv2dFilterOperandLayout::kOihw;

  // Convert to componment operand type with bias id.
  auto& bias_operand_id = conv2d->bias_operand_id;
  if (bias_operand_id) {
    const auto bias_operand_iterator =
        id_to_operand_map.find(bias_operand_id.value());
    CHECK(bias_operand_iterator != id_to_operand_map.end());
    component_attributes.bias_operand =
        ConvertToComponentOperand(bias_operand_iterator->second.get());
  }

  return component_attributes;
}

webnn::Pool2dAttributes ConvertToPool2dAttributes(
    const webnn::mojom::Pool2dPtr& pool2d,
    const mojom::Operand* output) {
  webnn::Pool2dAttributes component_attributes;
  auto& window_dimensions = pool2d->window_dimensions;
  component_attributes.window_dimensions = webnn::Size2d<uint32_t>{
      .height = window_dimensions->height, .width = window_dimensions->width};
  auto& mojo_padding = pool2d->padding;
  component_attributes.padding = webnn::Padding2d{
      .beginning =
          webnn::Size2d<uint32_t>{.height = mojo_padding->beginning->height,
                                  .width = mojo_padding->beginning->width},
      .ending = webnn::Size2d<uint32_t>{.height = mojo_padding->ending->height,
                                        .width = mojo_padding->ending->width}};
  component_attributes.strides = webnn::Size2d<uint32_t>{
      .height = pool2d->strides->height, .width = pool2d->strides->width};
  component_attributes.dilations = webnn::Size2d<uint32_t>{
      .height = pool2d->dilations->height, .width = pool2d->dilations->width};
  component_attributes.layout =
      MojoInputOperandLayoutToComponent(pool2d->layout);
  CHECK_EQ(output->dimensions.size(), 4u);
  switch (component_attributes.layout) {
    case webnn::InputOperandLayout::kNchw:
      component_attributes.output_sizes = webnn::Size2d<uint32_t>{
          .height = output->dimensions[2], .width = output->dimensions[3]};
      break;
    case webnn::InputOperandLayout::kNhwc:
      component_attributes.output_sizes = webnn::Size2d<uint32_t>{
          .height = output->dimensions[1], .width = output->dimensions[2]};
      break;
  }
  return component_attributes;
}

webnn::GemmAttributes ConvertToGemmAttributes(
    const IdToOperandMap& id_to_operand_map,
    const mojom::GemmPtr& gemm) {
  webnn::GemmAttributes component_attributes;
  auto& c_operand_id = gemm->c_operand_id;
  if (c_operand_id) {
    const mojom::OperandPtr& c_operand =
        id_to_operand_map.at(c_operand_id.value());
    component_attributes.c_operand = ConvertToComponentOperand(c_operand.get());
  }
  component_attributes.alpha = gemm->alpha;
  component_attributes.beta = gemm->beta;
  component_attributes.a_transpose = gemm->a_transpose;
  component_attributes.b_transpose = gemm->b_transpose;
  return component_attributes;
}

webnn::SliceAttributes ConvertToSliceAttributes(
    const webnn::mojom::SlicePtr& slice) {
  webnn::SliceAttributes component_attributes;
  component_attributes.starts.reserve(slice->starts_and_sizes.size());
  component_attributes.sizes.reserve(slice->starts_and_sizes.size());
  for (const auto& start_and_size : slice->starts_and_sizes) {
    component_attributes.starts.push_back(start_and_size->start);
    component_attributes.sizes.push_back(start_and_size->size);
  }
  return component_attributes;
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

bool ValidateUnaryOperation(const mojom::Operand* input,
                            const mojom::Operand* output) {
  if (!input || !output || output == input) {
    // The unary operator is invalid.
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

template <typename Operation>
bool ValidateUnaryOperation(const IdToOperandMap& id_to_operand_map,
                            const Operation& operation) {
  auto* input = GetMojoOperand(id_to_operand_map, operation->input_operand_id);
  auto* output =
      GetMojoOperand(id_to_operand_map, operation->output_operand_id);
  return ValidateUnaryOperation(input, output);
}

template <typename Operation>
bool ValidateFloatingPointUnaryOperation(
    const IdToOperandMap& id_to_operand_map,
    const Operation& operation) {
  auto* input = GetMojoOperand(id_to_operand_map, operation->input_operand_id);
  auto* output =
      GetMojoOperand(id_to_operand_map, operation->output_operand_id);
  if (!ValidateUnaryOperation(input, output)) {
    return false;
  }

  if (!IsFloatingPointType(MojoOperandTypeToComponent(input->data_type))) {
    return false;
  }

  return true;
}

bool ValidateClamp(const IdToOperandMap& id_to_operand_map,
                   const mojom::ClampPtr& clamp) {
  if (!ValidateUnaryOperation(id_to_operand_map, clamp)) {
    return false;
  }
  if (!ValidateClampAttributes(clamp)) {
    return false;
  }

  return true;
}

bool ValidateConcat(const IdToOperandMap& id_to_operand_map,
                    const mojom::ConcatPtr& concat) {
  auto* output = GetMojoOperand(id_to_operand_map, concat->output_operand_id);
  if (!output) {
    // The concat operator is invalid.
    return false;
  }

  std::vector<Operand> inputs;
  inputs.reserve(concat->input_operand_ids.size());
  for (const auto& input_operand_id : concat->input_operand_ids) {
    auto* input = GetMojoOperand(id_to_operand_map, input_operand_id);
    if (!input || input == output) {
      return false;
    }
    inputs.push_back(ConvertToComponentOperand(input));
  }

  auto validated_output = ValidateConcatAndInferOutput(inputs, concat->axis);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != ConvertToComponentOperand(output)) {
    return false;
  }

  return true;
}

bool ValidateConv2d(const IdToOperandMap& id_to_operand_map,
                    const mojom::Conv2dPtr& conv2d) {
  auto* input = GetMojoOperand(id_to_operand_map, conv2d->input_operand_id);
  auto* filter = GetMojoOperand(id_to_operand_map, conv2d->filter_operand_id);
  auto* output = GetMojoOperand(id_to_operand_map, conv2d->output_operand_id);
  if (!input || !filter || !output || output == input || output == filter) {
    // The conv2d operator is invalid.
    return false;
  }
  auto& bias_operand_id = conv2d->bias_operand_id;
  if (bias_operand_id && !id_to_operand_map.contains(bias_operand_id.value())) {
    // Invalid bias operand.
    return false;
  }

  // Validate the activation if the option is configured.
  auto& activation = conv2d->activation;
  if (activation && !ValidateActivation(activation)) {
    // The activation is invalid.
    return false;
  }
  auto validated_output = ValidateConv2dAndInferOutput(
      ConvertToComponentOperand(input), ConvertToComponentOperand(filter),
      ConvertToConv2dAttributes(id_to_operand_map, conv2d));
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != ConvertToComponentOperand(output)) {
    return false;
  }

  return true;
}

bool ValidateElementWiseBinary(const IdToOperandMap& id_to_operand_map,
                               const mojom::ElementWiseBinaryPtr& operation) {
  auto* a = GetMojoOperand(id_to_operand_map, operation->lhs_operand);
  auto* b = GetMojoOperand(id_to_operand_map, operation->rhs_operand);
  auto* output = GetMojoOperand(id_to_operand_map, operation->output_operand);
  if (!a || !b || !output || output == a || output == b) {
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
                  const mojom::GemmPtr& gemm) {
  auto* a = GetMojoOperand(id_to_operand_map, gemm->a_operand_id);
  auto* b = GetMojoOperand(id_to_operand_map, gemm->b_operand_id);
  auto* output = GetMojoOperand(id_to_operand_map, gemm->output_operand_id);
  if (!a || !b || !output || output == a || output == b) {
    // The gemm operator is invalid.
    return false;
  }
  auto& c_operand_id = gemm->c_operand_id;
  if (c_operand_id && !id_to_operand_map.contains(c_operand_id.value())) {
    // The third operand is invalid.
    return false;
  }
  auto validated_output = ValidateGemmAndInferOutput(
      ConvertToComponentOperand(a), ConvertToComponentOperand(b),
      ConvertToGemmAttributes(id_to_operand_map, gemm));
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != ConvertToComponentOperand(output)) {
    return false;
  }

  return true;
}

bool ValidatePad(const IdToOperandMap& id_to_operand_map,
                 const mojom::PadPtr& pad) {
  auto* input = GetMojoOperand(id_to_operand_map, pad->input_operand_id);
  auto* output = GetMojoOperand(id_to_operand_map, pad->output_operand_id);
  if (!input || !output || output == input) {
    // The pad operator is invalid.
    return false;
  }

  auto validated_output =
      ValidatePadAndInferOutput(ConvertToComponentOperand(input),
                                pad->beginning_padding, pad->ending_padding);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != ConvertToComponentOperand(output)) {
    return false;
  }

  return true;
}

bool ValidatePool2d(const IdToOperandMap& id_to_operand_map,
                    const mojom::Pool2dPtr& pool2d) {
  auto* input = GetMojoOperand(id_to_operand_map, pool2d->input_operand_id);
  auto* output = GetMojoOperand(id_to_operand_map, pool2d->output_operand_id);
  if (!input || !output || output == input) {
    // The pool2d operator is invalid.
    return false;
  }

  if (output->dimensions.size() != 4) {
    // The element of output dimensions should be 4.
    return false;
  }
  auto validated_output =
      ValidatePool2dAndInferOutput(ConvertToComponentOperand(input),
                                   ConvertToPool2dAttributes(pool2d, output));
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != ConvertToComponentOperand(output)) {
    return false;
  }

  return true;
}

bool ValidatePrelu(const IdToOperandMap& id_to_operand_map,
                   const mojom::PreluPtr& prelu) {
  auto* input = GetMojoOperand(id_to_operand_map, prelu->input_operand_id);
  auto* output = GetMojoOperand(id_to_operand_map, prelu->output_operand_id);
  auto* slope = GetMojoOperand(id_to_operand_map, prelu->slope_operand_id);
  if (!input || !output || !slope || output == input || output == slope) {
    // The prelu operator is invalid.
    return false;
  }

  auto validated_output = ValidatePreluAndInferOutput(
      ConvertToComponentOperand(input), ConvertToComponentOperand(slope));
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != ConvertToComponentOperand(output)) {
    return false;
  }

  return true;
}

bool ValidateResample2d(const IdToOperandMap& id_to_operand_map,
                        const mojom::Resample2dPtr& resample2d) {
  auto* input = GetMojoOperand(id_to_operand_map, resample2d->input_operand_id);
  auto* output =
      GetMojoOperand(id_to_operand_map, resample2d->output_operand_id);
  if (!input || !output || output == input) {
    // The resample2d operator is invalid.
    return false;
  }
  if (output->data_type != input->data_type) {
    // The output data type doesn't match input data type.
    return false;
  }

  return true;
}

bool ValidateReshape(const IdToOperandMap& id_to_operand_map,
                     const mojom::ReshapePtr& reshape) {
  auto* input = GetMojoOperand(id_to_operand_map, reshape->input_operand_id);
  auto* output = GetMojoOperand(id_to_operand_map, reshape->output_operand_id);
  if (!input || !output || output == input) {
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

bool ValidateSlice(const IdToOperandMap& id_to_operand_map,
                   const mojom::SlicePtr& slice) {
  auto* input = GetMojoOperand(id_to_operand_map, slice->input_operand_id);
  auto* output = GetMojoOperand(id_to_operand_map, slice->output_operand_id);

  if (!input || !output || output == input) {
    // The slice operator is invalid.
    return false;
  }

  auto validated_output = ValidateSliceAndInferOutput(
      ConvertToComponentOperand(input), ConvertToSliceAttributes(slice));
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != ConvertToComponentOperand(output)) {
    return false;
  }

  return true;
}

bool ValidateSoftmax(const IdToOperandMap& id_to_operand_map,
                     const mojom::SoftmaxPtr& softmax) {
  auto* input = GetMojoOperand(id_to_operand_map, softmax->input_operand_id);
  auto* output = GetMojoOperand(id_to_operand_map, softmax->output_operand_id);
  if (!input || !output || output == input) {
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

bool ValidateSplit(const IdToOperandMap& id_to_operand_map,
                   const mojom::SplitPtr& split) {
  auto* input = GetMojoOperand(id_to_operand_map, split->input_operand_id);
  if (!input) {
    // The split operator is invalid.
    return false;
  }
  std::vector<uint32_t> splits;
  splits.reserve(split->output_operand_ids.size());
  for (uint64_t output_id : split->output_operand_ids) {
    auto* output = GetMojoOperand(id_to_operand_map, output_id);
    if (!output || input == output) {
      return false;
    }

    if (split->axis >= output->dimensions.size()) {
      return false;
    }
    splits.push_back(output->dimensions[split->axis]);
  }

  auto validated_output =
      ValidateSplitAndInferOutput(ConvertToComponentOperand(input),
                                  {.splits = splits, .axis = split->axis});
  if (!validated_output.has_value()) {
    return false;
  }

  if (split->output_operand_ids.size() != validated_output->size()) {
    // The number of specified outputs did not match the expected number of
    // outputs.
    return false;
  }

  for (uint32_t i = 0; i < validated_output->size(); ++i) {
    auto* output =
        GetMojoOperand(id_to_operand_map, split->output_operand_ids[i]);
    if (validated_output->at(i) != ConvertToComponentOperand(output)) {
      return false;
    }
  }

  return true;
}

bool ValidateTranspose(const IdToOperandMap& id_to_operand_map,
                       const mojom::TransposePtr& transpose) {
  auto* input = GetMojoOperand(id_to_operand_map, transpose->input_operand_id);
  auto* output =
      GetMojoOperand(id_to_operand_map, transpose->output_operand_id);
  if (!input || !output || output == input) {
    // The transpose operator is invalid.
    return false;
  }

  auto validated_output = ValidateTransposeAndInferOutput(
      ConvertToComponentOperand(input), transpose->permutation);
  if (!validated_output.has_value()) {
    return false;
  }
  if (validated_output != ConvertToComponentOperand(output)) {
    return false;
  }

  return true;
}

base::flat_map<std::string, size_t> CreateByteLengthMap(
    const std::vector<uint64_t>& operand_ids,
    const base::flat_map<uint64_t, mojom::OperandPtr>& id_to_operand_map) {
  base::flat_map<std::string, size_t> name_to_byte_length_map;
  name_to_byte_length_map.reserve(operand_ids.size());
  for (auto& operand_id : operand_ids) {
    const mojom::OperandPtr& operand = id_to_operand_map.at(operand_id);
    // The `operand` is valid and the byte length of it was already verified in
    // `ValidateGraph` function.
    CHECK(operand);

    auto byte_length = ValidateAndCalculateByteLength(
        GetBytesPerElement(operand->data_type), operand->dimensions);
    CHECK(byte_length.has_value());
    CHECK(operand->name.has_value());
    name_to_byte_length_map[operand->name.value()] = byte_length.value();
  }
  return name_to_byte_length_map;
}

bool ValidateOperation(const IdToOperandMap& id_to_operand_map,
                       const mojom::OperationPtr& operation) {
  switch (operation->which()) {
    case mojom::Operation::Tag::kClamp:
      return ValidateClamp(id_to_operand_map, operation->get_clamp());
    case mojom::Operation::Tag::kConcat:
      return ValidateConcat(id_to_operand_map, operation->get_concat());
    case mojom::Operation::Tag::kConv2d:
      return ValidateConv2d(id_to_operand_map, operation->get_conv2d());
    case mojom::Operation::Tag::kElementWiseBinary:
      return ValidateElementWiseBinary(id_to_operand_map,
                                       operation->get_element_wise_binary());
    case mojom::Operation::Tag::kGemm:
      return ValidateGemm(id_to_operand_map, operation->get_gemm());
    case mojom::Operation::Tag::kPad:
      return ValidatePad(id_to_operand_map, operation->get_pad());
    case mojom::Operation::Tag::kPool2d:
      return ValidatePool2d(id_to_operand_map, operation->get_pool2d());
    case mojom::Operation::Tag::kPrelu:
      return ValidatePrelu(id_to_operand_map, operation->get_prelu());
    case mojom::Operation::Tag::kResample2d:
      return ValidateResample2d(id_to_operand_map, operation->get_resample2d());
    case mojom::Operation::Tag::kReshape:
      return ValidateReshape(id_to_operand_map, operation->get_reshape());
    case mojom::Operation::Tag::kRelu:
      return ValidateUnaryOperation(id_to_operand_map, operation->get_relu());
    case mojom::Operation::Tag::kSlice:
      return ValidateSlice(id_to_operand_map, operation->get_slice());
    case mojom::Operation::Tag::kSigmoid:
      return ValidateFloatingPointUnaryOperation(id_to_operand_map,
                                                 operation->get_sigmoid());
    case mojom::Operation::Tag::kSoftmax:
      return ValidateSoftmax(id_to_operand_map, operation->get_softmax());
    case mojom::Operation::Tag::kSplit:
      return ValidateSplit(id_to_operand_map, operation->get_split());
    case mojom::Operation::Tag::kTanh:
      return ValidateFloatingPointUnaryOperation(id_to_operand_map,
                                                 operation->get_tanh());
    case mojom::Operation::Tag::kTranspose:
      return ValidateTranspose(id_to_operand_map, operation->get_transpose());
  }
  NOTREACHED_NORETURN();
}

}  // namespace

WebNNGraphImpl::ComputeResourceInfo::ComputeResourceInfo(
    const mojom::GraphInfoPtr& graph_info) {
  input_name_to_byte_length_map = CreateByteLengthMap(
      graph_info->input_operands, graph_info->id_to_operand_map);
  output_name_to_byte_length_map = CreateByteLengthMap(
      graph_info->output_operands, graph_info->id_to_operand_map);
}

WebNNGraphImpl::ComputeResourceInfo::ComputeResourceInfo(
    ComputeResourceInfo&&) = default;
WebNNGraphImpl::ComputeResourceInfo&
WebNNGraphImpl::ComputeResourceInfo::operator=(ComputeResourceInfo&&) = default;

WebNNGraphImpl::ComputeResourceInfo::~ComputeResourceInfo() = default;

WebNNGraphImpl::WebNNGraphImpl(ComputeResourceInfo compute_resource_info)
    : compute_resource_info_(std::move(compute_resource_info)) {}

WebNNGraphImpl::~WebNNGraphImpl() = default;

bool WebNNGraphImpl::ValidateGraph(const mojom::GraphInfoPtr& graph_info) {
  // The input operands of graph can be empty.
  if (graph_info->id_to_operand_map.empty() || graph_info->operations.empty() ||
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

  // Validate the operations which are sorted in the topological order.
  for (auto& operation : graph_info->operations) {
    if (!ValidateOperation(graph_info->id_to_operand_map, operation)) {
      return false;
    }
  }

  return true;
}

void WebNNGraphImpl::Compute(
    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs,
    mojom::WebNNGraph::ComputeCallback callback) {
  // Validate the inputs for computation match the built graph's expectation.
  if (!base::ranges::equal(named_inputs,
                           compute_resource_info_.input_name_to_byte_length_map,
                           [](const auto& iter_a, const auto& iter_b) {
                             // Compare the input name with the key of map and
                             // the byte length of buffer with value of map.
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
