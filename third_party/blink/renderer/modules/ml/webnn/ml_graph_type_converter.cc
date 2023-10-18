// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_type_converter.h"

#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_clamp_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gemm_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pool_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_resample_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_split_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_transpose_options.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_activation.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"

namespace blink_mojom = webnn::mojom::blink;

namespace mojo {

blink_mojom::Operand::DataType BlinkOperandTypeToMojo(
    blink::V8MLOperandType::Enum type) {
  switch (type) {
    case blink::V8MLOperandType::Enum::kFloat32:
      return blink_mojom::Operand::DataType::kFloat32;
    case blink::V8MLOperandType::Enum::kFloat16:
      return blink_mojom::Operand::DataType::kFloat16;
    case blink::V8MLOperandType::Enum::kInt32:
      return blink_mojom::Operand::DataType::kInt32;
    case blink::V8MLOperandType::Enum::kUint32:
      return blink_mojom::Operand::DataType::kUint32;
    case blink::V8MLOperandType::Enum::kInt8:
      return blink_mojom::Operand::DataType::kInt8;
    case blink::V8MLOperandType::Enum::kUint8:
      return blink_mojom::Operand::DataType::kUint8;
  }
  NOTREACHED_NORETURN();
}

// Converters from IDL to Mojo.
blink_mojom::OperandPtr
TypeConverter<blink_mojom::OperandPtr, blink::MLOperand*>::Convert(
    const blink::MLOperand* ml_operand) {
  if (!ml_operand) {
    return nullptr;
  }
  auto mojo_operand = blink_mojom::Operand::New();
  switch (ml_operand->Kind()) {
    case blink::MLOperand::OperandKind::kInput:
      mojo_operand->kind = blink_mojom::Operand::Kind::kInput;
      mojo_operand->name = ml_operand->Name();
      break;
    case blink::MLOperand::OperandKind::kConstant:
      mojo_operand->kind = blink_mojom::Operand::Kind::kConstant;
      break;
    case blink::MLOperand::OperandKind::kOutput:
      mojo_operand->kind = blink_mojom::Operand::Kind::kOutput;
      break;
  }
  mojo_operand->data_type = BlinkOperandTypeToMojo(ml_operand->Type());
  mojo_operand->dimensions = ml_operand->Dimensions();
  return mojo_operand;
}

// Get height and width of input operand.
webnn::Size2d<uint32_t> GetInputOperandSize2d(
    const blink::MLOperand* input,
    blink::V8MLInputOperandLayout::Enum type) {
  CHECK(input);
  const auto input_shape = input->Dimensions();
  CHECK_EQ(input_shape.size(), 4u);
  uint32_t input_height, input_width;
  switch (type) {
    case blink::V8MLInputOperandLayout::Enum::kNchw:
      // "nchw": [batches, channels, height, width]
      input_height = input_shape[2];
      input_width = input_shape[3];
      break;
    case blink::V8MLInputOperandLayout::Enum::kNhwc:
      // "nhwc": [batches, height, width, channels]
      input_height = input_shape[1];
      input_width = input_shape[2];
      break;
  }
  return {.height = input_height, .width = input_width};
}

}  // namespace mojo

namespace blink {

namespace {

using blink_mojom::ElementWiseBinary;
using blink_mojom::Operation;
using blink_mojom::OperationPtr;
using blink_mojom::Operator;
using blink_mojom::OperatorPtr;
using blink_mojom::Size2d;

// Maps MLOperand to its id which is used to identify the `mojo::Operand` across
// processes.
using OperandToIdMap = HeapHashMap<Member<const MLOperand>, uint64_t>;

uint64_t GetOperatorInputId(const MLOperator* op,
                            const OperandToIdMap& operand_to_id_map,
                            wtf_size_t index = 0) {
  CHECK_NE(op, nullptr);
  CHECK_LE(index, op->Inputs().size());
  const auto* input = op->Inputs()[index].Get();
  return operand_to_id_map.at(input);
}

uint64_t GetOperatorOutputId(const MLOperator* op,
                             const OperandToIdMap& operand_to_id_map,
                             wtf_size_t index = 0) {
  CHECK_NE(op, nullptr);
  CHECK_LE(index, op->Outputs().size());
  const auto* output = op->Outputs()[index].Get();
  return operand_to_id_map.at(output);
}

OperationPtr CreateClampOperation(const OperandToIdMap& operand_to_id_map,
                                  const MLOperator* clamp,
                                  bool activation = false) {
  auto clamp_mojo = webnn::mojom::blink::Clamp::New();
  // Activation has no input and output operand.
  if (!activation) {
    clamp_mojo->input_operand_id = GetOperatorInputId(clamp, operand_to_id_map);
    clamp_mojo->output_operand_id =
        GetOperatorOutputId(clamp, operand_to_id_map);
  }
  const auto* options = static_cast<const MLClampOptions*>(clamp->Options());
  CHECK(options);
  clamp_mojo->min_value =
      options->getMinValueOr(-std::numeric_limits<float>::infinity());
  clamp_mojo->max_value =
      options->getMaxValueOr(+std::numeric_limits<float>::infinity());
  return webnn::mojom::blink::Operation::NewClamp(std::move(clamp_mojo));
}

blink_mojom::InputOperandLayout BlinkInputOperandLayoutToMojo(
    blink::V8MLInputOperandLayout::Enum type) {
  switch (type) {
    case blink::V8MLInputOperandLayout::Enum::kNchw:
      return blink_mojom::InputOperandLayout::kChannelsFirst;
    case blink::V8MLInputOperandLayout::Enum::kNhwc:
      return blink_mojom::InputOperandLayout::kChannelsLast;
  }
  NOTREACHED_NORETURN();
}

OperationPtr CreateConcatOperation(const OperandToIdMap& operand_to_id_map,
                                   const MLOperator* concat) {
  const auto& inputs = concat->Inputs();

  Vector<uint64_t> input_operand_ids;
  input_operand_ids.reserve(inputs.size());
  base::ranges::transform(inputs, std::back_inserter(input_operand_ids),
                          [operand_to_id_map](const auto& input) {
                            return operand_to_id_map.at(input);
                          });

  auto concat_mojo = webnn::mojom::blink::Concat::New();
  concat_mojo->input_operand_ids = std::move(input_operand_ids);
  concat_mojo->output_operand_id =
      GetOperatorOutputId(concat, operand_to_id_map);
  const auto* concat_operator = static_cast<const MLConcatOperator*>(concat);

  concat_mojo->axis = concat_operator->Axis();
  return webnn::mojom::blink::Operation::NewConcat(std::move(concat_mojo));
}

base::expected<OperationPtr, String> CreateConv2dOperation(
    const OperandToIdMap& operand_to_id_map,
    const MLOperator* conv2d) {
  auto conv2d_mojo = webnn::mojom::blink::Conv2d::New();
  conv2d_mojo->input_operand_id =
      GetOperatorInputId(conv2d, operand_to_id_map, 0);
  conv2d_mojo->filter_operand_id =
      GetOperatorInputId(conv2d, operand_to_id_map, 1);
  conv2d_mojo->output_operand_id =
      GetOperatorOutputId(conv2d, operand_to_id_map);

  const auto* options = static_cast<const MLConv2dOptions*>(conv2d->Options());
  CHECK(options);
  if (options->filterLayout().AsEnum() !=
      blink::V8MLConv2dFilterOperandLayout::Enum::kOihw) {
    // The filter layout is being discussed to simplify other variants in WebNN
    // working group https://github.com/webmachinelearning/webnn/issues/324.
    return base::unexpected(
        String::Format("The filter layout %s is not supported.",
                       options->filterLayout().AsCStr()));
  }
  // If strides is not present, the values are assumed to be [1,1].
  auto strides = options->getStridesOr({1, 1});
  CHECK_EQ(strides.size(), 2u);
  conv2d_mojo->strides = Size2d::New(strides[0], strides[1]);

  // If dilations is not present, the values are assumed to be [1, 1].
  auto dilations = options->getDilationsOr({1, 1});
  CHECK_EQ(dilations.size(), 2u);
  conv2d_mojo->dilations = Size2d::New(dilations[0], dilations[1]);
  conv2d_mojo->groups = options->groups();
  conv2d_mojo->input_layout =
      BlinkInputOperandLayoutToMojo(options->inputLayout().AsEnum());
  if (options->hasBias()) {
    conv2d_mojo->bias_operand_id = operand_to_id_map.at(options->bias());
  }

  // Get height and width of input for calculating padding.
  auto input_size = mojo::GetInputOperandSize2d(
      conv2d->Inputs()[0].Get(), options->inputLayout().AsEnum());
  // Get height and width of filter operand for calculating padding.
  const auto* filter = conv2d->Inputs()[1].Get();
  CHECK(filter);
  const auto filter_shape = filter->Dimensions();
  CHECK_EQ(filter_shape.size(), 4u);
  uint32_t filter_height, filter_width;
  switch (options->filterLayout().AsEnum()) {
    case V8MLConv2dFilterOperandLayout::Enum::kOihw:
      // "oihw": [output_channels, input_channels/groups, height, width]
      filter_height = filter_shape[2];
      filter_width = filter_shape[3];
      break;
    case V8MLConv2dFilterOperandLayout::Enum::kHwio:
      // "hwio": [height, width, input_channels/groups, output_channels]
      filter_height = filter_shape[0];
      filter_width = filter_shape[1];
      break;
    case V8MLConv2dFilterOperandLayout::Enum::kOhwi:
    case V8MLConv2dFilterOperandLayout::Enum::kIhwo:
      // "ohwi": [output_channels, height, width, input_channels/groups]
      // "ihwo": [input_channels/groups, height, width, output_channels]
      filter_height = filter_shape[1];
      filter_width = filter_shape[2];
      break;
  }

  // Calculate the padding given input sizes, filter size, padding, strides and
  // dilations.
  auto padding = blink::CalculatePadding2D(
      options, input_size.height, input_size.width, filter_height, filter_width,
      conv2d_mojo->strides->height, conv2d_mojo->strides->width,
      conv2d_mojo->dilations->height, conv2d_mojo->dilations->width);
  // The order of sequence array is [beginning_height, ending_height,
  // beginning_width, ending_width].
  conv2d_mojo->padding = webnn::mojom::blink::Padding2d::New(
      /*beginning padding*/ Size2d::New(padding.beginning.height,
                                        padding.beginning.width),
      /*ending padding*/ Size2d::New(padding.ending.height,
                                     padding.ending.width));

  // Convert `MLActivition` to `mojo::Operator` if it's configured.
  if (options->hasActivation()) {
    auto operator_kind = options->activation()->Operator()->Kind();
    switch (operator_kind) {
      case blink::MLOperator::OperatorKind::kClamp: {
        conv2d_mojo->activation = CreateClampOperation(
            operand_to_id_map, options->activation()->Operator(), true);
        break;
      }
      case blink::MLOperator::OperatorKind::kRelu:
        conv2d_mojo->activation = webnn::mojom::blink::Operation::NewRelu(
            webnn::mojom::blink::Relu::New());
        break;
      default:
        return base::unexpected(
            MLOperator::OperatorKindToString(operator_kind) +
            " is not converted to mojo as activation.");
    }
  }
  return webnn::mojom::blink::Operation::NewConv2d(std::move(conv2d_mojo));
}

OperationPtr CreateElementWiseBinaryOperator(
    const OperandToIdMap& operand_to_id_map,
    const MLOperator* binary) {
  const uint64_t lhs_operand_id =
      GetOperatorInputId(binary, operand_to_id_map, 0);
  const uint64_t rhs_operand_id =
      GetOperatorInputId(binary, operand_to_id_map, 1);
  const uint64_t output_operand_id =
      GetOperatorOutputId(binary, operand_to_id_map);

  auto operator_mojo = ElementWiseBinary::New();
  switch (binary->Kind()) {
    case MLOperator::OperatorKind::kAdd:
      operator_mojo->kind = ElementWiseBinary::Kind::kAdd;
      break;
    case MLOperator::OperatorKind::kSub:
      operator_mojo->kind = ElementWiseBinary::Kind::kSub;
      break;
    case MLOperator::OperatorKind::kMul:
      operator_mojo->kind = ElementWiseBinary::Kind::kMul;
      break;
    case MLOperator::OperatorKind::kDiv:
      operator_mojo->kind = ElementWiseBinary::Kind::kDiv;
      break;
    case MLOperator::OperatorKind::kMax:
      operator_mojo->kind = ElementWiseBinary::Kind::kMax;
      break;
    case MLOperator::OperatorKind::kMin:
      operator_mojo->kind = ElementWiseBinary::Kind::kMin;
      break;
    case MLOperator::OperatorKind::kPow:
      operator_mojo->kind = ElementWiseBinary::Kind::kPow;
      break;
    default:
      NOTREACHED();
  }
  operator_mojo->lhs_operand = lhs_operand_id;
  operator_mojo->rhs_operand = rhs_operand_id;
  operator_mojo->output_operand = output_operand_id;
  return webnn::mojom::blink::Operation::NewElementWiseBinary(
      std::move(operator_mojo));
}

blink_mojom::GemmAttributesPtr ConvertToGemmAttributes(
    const OperandToIdMap& operand_to_id_map,
    const blink::MLGemmOptions* options) {
  CHECK(options);
  auto attributes = blink_mojom::GemmAttributes::New();
  if (options->hasC()) {
    attributes->c_operand_id = operand_to_id_map.at(options->c());
  }
  attributes->alpha = options->alpha();
  attributes->beta = options->beta();
  attributes->a_transpose = options->aTranspose();
  attributes->b_transpose = options->bTranspose();
  return attributes;
}

OperationPtr CreateGemmOperator(const OperandToIdMap& operand_to_id_map,
                                const MLOperator* gemm) {
  const uint64_t a_operand_id = GetOperatorInputId(gemm, operand_to_id_map, 0);
  const uint64_t b_operand_id = GetOperatorInputId(gemm, operand_to_id_map, 1);
  const uint64_t output_operand_id =
      GetOperatorOutputId(gemm, operand_to_id_map);

  auto operator_mojo = blink_mojom::Operator::New();
  operator_mojo->kind = Operator::Kind::kGemm;
  operator_mojo->input_operands = {a_operand_id, b_operand_id};
  operator_mojo->output_operands = {output_operand_id};
  const auto* options = static_cast<const MLGemmOptions*>(gemm->Options());
  CHECK(options);
  operator_mojo->attributes = blink_mojom::OperatorAttributes::NewGemm(
      ConvertToGemmAttributes(operand_to_id_map, options));
  return blink_mojom::Operation::NewGenericOperator(std::move(operator_mojo));
}

OperationPtr CreatePool2dOperation(const OperandToIdMap& operand_to_id_map,
                                   const MLOperator* pool2d) {
  const uint64_t input_operand_id =
      GetOperatorInputId(pool2d, operand_to_id_map);
  const uint64_t output_operand_id =
      GetOperatorOutputId(pool2d, operand_to_id_map);

  auto pool2d_mojo = blink_mojom::Pool2d::New();
  switch (pool2d->Kind()) {
    case MLOperator::OperatorKind::kAveragePool2d:
      pool2d_mojo->kind = blink_mojom::Pool2d::Kind::kAveragePool2d;
      break;
    case MLOperator::OperatorKind::kMaxPool2d:
      pool2d_mojo->kind = blink_mojom::Pool2d::Kind::kMaxPool2d;
      break;
    default:
      NOTREACHED();
  }
  pool2d_mojo->input_operand_id = input_operand_id;
  pool2d_mojo->output_operand_id = output_operand_id;

  const auto* options =
      static_cast<const blink::MLPool2dOptions*>(pool2d->Options());
  CHECK(options);
  // If strides is not present, the values are assumed to be [1,1].
  auto strides = options->getStridesOr({1, 1});
  CHECK_EQ(strides.size(), 2u);
  pool2d_mojo->strides = Size2d::New(strides[0], strides[1]);

  // If dilations is not present, the values are assumed to be [1, 1].
  auto dilations = options->getDilationsOr({1, 1});
  CHECK_EQ(dilations.size(), 2u);
  pool2d_mojo->dilations = Size2d::New(dilations[0], dilations[1]);
  pool2d_mojo->layout =
      BlinkInputOperandLayoutToMojo(options->layout().AsEnum());

  // Get height and width of input for calculating padding.
  auto input_size = mojo::GetInputOperandSize2d(pool2d->Inputs()[0].Get(),
                                                options->layout().AsEnum());
  // The dimensions of the sliding window are the height and width of input
  // operand if they are not supplied by user.
  uint32_t window_height = input_size.height;
  uint32_t window_width = input_size.width;
  if (options->hasWindowDimensions()) {
    auto& window_dimensions = options->windowDimensions();
    CHECK_EQ(window_dimensions.size(), 2u);
    window_height = window_dimensions[0];
    window_width = window_dimensions[1];
  }
  pool2d_mojo->window_dimensions = Size2d::New(window_height, window_width);

  // Calculate the padding given input sizes, window dimensions, padding,
  // strides and dilations.
  auto padding = blink::CalculatePadding2D(
      options, input_size.height, input_size.width, window_height, window_width,
      pool2d_mojo->strides->height, pool2d_mojo->strides->width,
      pool2d_mojo->dilations->height, pool2d_mojo->dilations->width);
  // The order of sequence array is [beginning_height, ending_height,
  // beginning_width, ending_width].
  pool2d_mojo->padding = blink_mojom::Padding2d::New(
      /*beginning padding*/ Size2d::New(padding.beginning.height,
                                        padding.beginning.width),
      /*ending padding*/ Size2d::New(padding.ending.height,
                                     padding.ending.width));

  return blink_mojom::Operation::NewPool2d(std::move(pool2d_mojo));
}

OperationPtr CreateResample2dOperation(const OperandToIdMap& operand_to_id_map,
                                       const MLOperator* resample2d) {
  auto resample2d_mojo = webnn::mojom::blink::Resample2d::New();

  resample2d_mojo->input_operand_id =
      GetOperatorInputId(resample2d, operand_to_id_map);
  resample2d_mojo->output_operand_id =
      GetOperatorOutputId(resample2d, operand_to_id_map);

  const auto* options =
      static_cast<const blink::MLResample2dOptions*>(resample2d->Options());
  CHECK(options);
  switch (options->mode().AsEnum()) {
    case blink::V8MLInterpolationMode::Enum::kNearestNeighbor:
      resample2d_mojo->mode =
          webnn::mojom::blink::Resample2d::InterpolationMode::kNearestNeighbor;
      break;
    case blink::V8MLInterpolationMode::Enum::kLinear:
      resample2d_mojo->mode =
          webnn::mojom::blink::Resample2d::InterpolationMode::kLinear;
      break;
  }

  return webnn::mojom::blink::Operation::NewResample2d(
      std::move(resample2d_mojo));
}

OperationPtr CreateReluOperation(const OperandToIdMap& operand_to_id_map,
                                 const MLOperator* relu) {
  const uint64_t input_operand_id = GetOperatorInputId(relu, operand_to_id_map);
  const uint64_t output_operand_id =
      GetOperatorOutputId(relu, operand_to_id_map);

  auto relu_mojo = webnn::mojom::blink::Relu::New();
  relu_mojo->input_operand_id = input_operand_id;
  relu_mojo->output_operand_id = output_operand_id;
  return webnn::mojom::blink::Operation::NewRelu(std::move(relu_mojo));
}

OperationPtr CreateReshapeOperator(const OperandToIdMap& operand_to_id_map,
                                   const MLOperator* reshape) {
  const uint64_t input_operand_id =
      GetOperatorInputId(reshape, operand_to_id_map);
  const uint64_t output_operand_id =
      GetOperatorOutputId(reshape, operand_to_id_map);

  auto operator_mojo = blink_mojom::Operator::New();
  operator_mojo->kind = Operator::Kind::kReshape;
  operator_mojo->input_operands = {input_operand_id};
  operator_mojo->output_operands = {output_operand_id};
  return blink_mojom::Operation::NewGenericOperator(std::move(operator_mojo));
}

OperationPtr CreateSliceOperation(const OperandToIdMap& operand_to_id_map,
                                  const MLOperator* slice) {
  auto slice_mojo = webnn::mojom::blink::Slice::New();
  slice_mojo->input_operand_id = GetOperatorInputId(slice, operand_to_id_map);
  slice_mojo->output_operand_id = GetOperatorOutputId(slice, operand_to_id_map);
  const MLSliceOperator* slice_operator =
      static_cast<const MLSliceOperator*>(slice);
  CHECK_EQ(slice_operator->Sizes().size(), slice_operator->Starts().size());
  slice_mojo->starts_and_sizes.reserve(slice_operator->Starts().size());
  for (uint32_t i = 0; i < slice_operator->Starts().size(); ++i) {
    webnn::mojom::blink::StartAndSizePtr start_and_size =
        webnn::mojom::blink::StartAndSize::New();
    start_and_size->start = slice_operator->Starts()[i];
    start_and_size->size = slice_operator->Sizes()[i];
    slice_mojo->starts_and_sizes.push_back(std::move(start_and_size));
  }
  return webnn::mojom::blink::Operation::NewSlice(std::move(slice_mojo));
}

OperationPtr CreateSoftmaxOperation(const OperandToIdMap& operand_to_id_map,
                                    const MLOperator* softmax) {
  const uint64_t input_operand_id =
      GetOperatorInputId(softmax, operand_to_id_map);
  const uint64_t output_operand_id =
      GetOperatorOutputId(softmax, operand_to_id_map);

  auto softmax_mojo = webnn::mojom::blink::Softmax::New();
  softmax_mojo->input_operand_id = input_operand_id;
  softmax_mojo->output_operand_id = output_operand_id;
  return webnn::mojom::blink::Operation::NewSoftmax(std::move(softmax_mojo));
}

OperationPtr CreateSplitOperation(const OperandToIdMap& operand_to_id_map,
                                  const MLOperator* split) {
  const uint64_t input_operand_id =
      GetOperatorInputId(split, operand_to_id_map);

  auto split_mojo = webnn::mojom::blink::Split::New();
  split_mojo->input_operand_id = input_operand_id;
  const wtf_size_t number_of_splits = split->Outputs().size();
  split_mojo->output_operand_ids.reserve(number_of_splits);
  for (uint32_t i = 0; i < number_of_splits; ++i) {
    split_mojo->output_operand_ids.push_back(
        GetOperatorOutputId(split, operand_to_id_map, i));
  }
  const auto* options =
      static_cast<const blink::MLSplitOptions*>(split->Options());
  CHECK(options);
  if (options->hasAxis()) {
    split_mojo->axis = options->axis();
  }
  return webnn::mojom::blink::Operation::NewSplit(std::move(split_mojo));
}

OperationPtr CreateTransposeOperation(const OperandToIdMap& operand_to_id_map,
                                      const MLOperator* transpose) {
  const uint64_t input_operand_id =
      GetOperatorInputId(transpose, operand_to_id_map);
  const uint64_t output_operand_id =
      GetOperatorOutputId(transpose, operand_to_id_map);

  auto transpose_mojo = blink_mojom::Transpose::New();
  transpose_mojo->input_operand_id = input_operand_id;
  transpose_mojo->output_operand_id = output_operand_id;
  const auto* options =
      static_cast<const MLTransposeOptions*>(transpose->Options());
  CHECK(options);

  auto input_rank = transpose->Inputs()[0]->Dimensions().size();
  transpose_mojo->permutation =
      options->getPermutationOr(CreateDefaultPermutation(input_rank));
  CHECK_EQ(transpose_mojo->permutation.size(), input_rank);

  return blink_mojom::Operation::NewTranspose(std::move(transpose_mojo));
}

}  // namespace

base::expected<OperationPtr, String> ConvertToMojoOperation(
    const OperandToIdMap& operand_to_id_map,
    const MLOperator* op) {
  switch (op->Kind()) {
    case MLOperator::OperatorKind::kClamp:
      return CreateClampOperation(operand_to_id_map, op);
    case MLOperator::OperatorKind::kConcat:
      return CreateConcatOperation(operand_to_id_map, op);
    case MLOperator::OperatorKind::kConv2d:
      return CreateConv2dOperation(operand_to_id_map, op);
    case MLOperator::OperatorKind::kAdd:
    case MLOperator::OperatorKind::kSub:
    case MLOperator::OperatorKind::kMul:
    case MLOperator::OperatorKind::kDiv:
    case MLOperator::OperatorKind::kMin:
    case MLOperator::OperatorKind::kMax:
    case MLOperator::OperatorKind::kPow:
      return CreateElementWiseBinaryOperator(operand_to_id_map, op);
    case MLOperator::OperatorKind::kAveragePool2d:
    case MLOperator::OperatorKind::kMaxPool2d:
      return CreatePool2dOperation(operand_to_id_map, op);
    case MLOperator::OperatorKind::kGemm:
      return CreateGemmOperator(operand_to_id_map, op);
    case MLOperator::OperatorKind::kResample2d:
      return CreateResample2dOperation(operand_to_id_map, op);
    case MLOperator::OperatorKind::kRelu:
      return CreateReluOperation(operand_to_id_map, op);
    case MLOperator::OperatorKind::kReshape:
      return CreateReshapeOperator(operand_to_id_map, op);
    case MLOperator::OperatorKind::kSlice:
      return CreateSliceOperation(operand_to_id_map, op);
    case MLOperator::OperatorKind::kSoftmax:
      return CreateSoftmaxOperation(operand_to_id_map, op);
    case MLOperator::OperatorKind::kSplit:
      return CreateSplitOperation(operand_to_id_map, op);
    case MLOperator::OperatorKind::kTranspose:
      return CreateTransposeOperation(operand_to_id_map, op);
    case MLOperator::OperatorKind::kHardSwish:
    case MLOperator::OperatorKind::kReduceMean:
    case MLOperator::OperatorKind::kReduceSum:
    case MLOperator::OperatorKind::kSigmoid:
    case MLOperator::OperatorKind::kLeakyRelu:
    case MLOperator::OperatorKind::kConvTranspose2d:
    case MLOperator::OperatorKind::kPRelu:
    case MLOperator::OperatorKind::kPad:
    case MLOperator::OperatorKind::kElu:
    case MLOperator::OperatorKind::kAbs:
    case MLOperator::OperatorKind::kCeil:
    case MLOperator::OperatorKind::kFloor:
    case MLOperator::OperatorKind::kNeg:
    case MLOperator::OperatorKind::kTanh:
      return base::unexpected(MLOperator::OperatorKindToString(op->Kind()) +
                              " is not implemented.");
  }
  NOTREACHED_NORETURN();
}

}  // namespace blink
