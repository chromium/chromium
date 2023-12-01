// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_type_converter.h"

#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_batch_normalization_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_clamp_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_transpose_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_elu_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gather_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gemm_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_leaky_relu_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pad_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pool_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_reduce_options.h"
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
    blink::V8MLOperandDataType::Enum data_type) {
  switch (data_type) {
    case blink::V8MLOperandDataType::Enum::kFloat32:
      return blink_mojom::Operand::DataType::kFloat32;
    case blink::V8MLOperandDataType::Enum::kFloat16:
      return blink_mojom::Operand::DataType::kFloat16;
    case blink::V8MLOperandDataType::Enum::kInt32:
      return blink_mojom::Operand::DataType::kInt32;
    case blink::V8MLOperandDataType::Enum::kUint32:
      return blink_mojom::Operand::DataType::kUint32;
    case blink::V8MLOperandDataType::Enum::kInt64:
      return blink_mojom::Operand::DataType::kInt64;
    case blink::V8MLOperandDataType::Enum::kUint64:
      return blink_mojom::Operand::DataType::kUint64;
    case blink::V8MLOperandDataType::Enum::kInt8:
      return blink_mojom::Operand::DataType::kInt8;
    case blink::V8MLOperandDataType::Enum::kUint8:
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
  mojo_operand->data_type = BlinkOperandTypeToMojo(ml_operand->DataType());
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

using blink_mojom::ActivationPtr;
using blink_mojom::ElementWiseBinary;
using blink_mojom::ElementWiseUnary;
using blink_mojom::Operation;
using blink_mojom::OperationPtr;
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

blink_mojom::ClampPtr CreateClamp(const OperandToIdMap& operand_to_id_map,
                                  const MLOperator* clamp,
                                  bool is_activation) {
  auto clamp_mojo = blink_mojom::Clamp::New();
  // Activation has no input and output operand.
  if (!is_activation) {
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
  return clamp_mojo;
}

blink_mojom::EluPtr CreateElu(const OperandToIdMap& operand_to_id_map,
                              const MLOperator* elu,
                              bool is_activation) {
  auto elu_mojo = blink_mojom::Elu::New();
  // Activation has no input and output operand.
  if (!is_activation) {
    elu_mojo->input_operand_id = GetOperatorInputId(elu, operand_to_id_map);
    elu_mojo->output_operand_id = GetOperatorOutputId(elu, operand_to_id_map);
  }

  const auto* options = static_cast<const MLEluOptions*>(elu->Options());
  CHECK(options);
  elu_mojo->alpha = options->alpha();
  return elu_mojo;
}

OperationPtr CreateExpandOperation(const OperandToIdMap& operand_to_id_map,
                                   const MLOperator* expand) {
  auto expand_mojo = blink_mojom::Expand::New();
  expand_mojo->input_operand_id = GetOperatorInputId(expand, operand_to_id_map);
  expand_mojo->output_operand_id =
      GetOperatorOutputId(expand, operand_to_id_map);
  return blink_mojom::Operation::NewExpand(std::move(expand_mojo));
}

blink_mojom::LeakyReluPtr CreateLeakyRelu(
    const OperandToIdMap& operand_to_id_map,
    const MLOperator* leaky_relu,
    bool is_activation) {
  auto leaky_relu_mojo = blink_mojom::LeakyRelu::New();
  // Activation has no input and output operand.
  if (!is_activation) {
    leaky_relu_mojo->input_operand_id =
        GetOperatorInputId(leaky_relu, operand_to_id_map);
    leaky_relu_mojo->output_operand_id =
        GetOperatorOutputId(leaky_relu, operand_to_id_map);
  }

  const auto* options =
      static_cast<const MLLeakyReluOptions*>(leaky_relu->Options());
  CHECK(options);
  leaky_relu_mojo->alpha = options->alpha();
  return leaky_relu_mojo;
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

base::expected<ActivationPtr, String> CreateActivation(
    const OperandToIdMap& operand_to_id_map,
    const MLOperator* ml_operator) {
  const auto operator_kind = ml_operator->Kind();
  switch (operator_kind) {
    case blink::MLOperator::OperatorKind::kClamp:
      return blink_mojom::Activation::NewClamp(
          CreateClamp(operand_to_id_map, ml_operator, true));
    case blink::MLOperator::OperatorKind::kElu:
      return blink_mojom::Activation::NewElu(
          CreateElu(operand_to_id_map, ml_operator, true));
    case blink::MLOperator::OperatorKind::kLeakyRelu:
      return blink_mojom::Activation::NewLeakyRelu(
          CreateLeakyRelu(operand_to_id_map, ml_operator, true));
    case blink::MLOperator::OperatorKind::kRelu:
      return blink_mojom::Activation::NewRelu(blink_mojom::Relu::New());
    case blink::MLOperator::OperatorKind::kSigmoid:
      return blink_mojom::Activation::NewSigmoid(blink_mojom::Sigmoid::New());
    case blink::MLOperator::OperatorKind::kSoftmax:
      return blink_mojom::Activation::NewSoftmax(blink_mojom::Softmax::New());
    case blink::MLOperator::OperatorKind::kTanh:
      return blink_mojom::Activation::NewTanh(blink_mojom::Tanh::New());
    default:
      return base::unexpected(MLOperator::OperatorKindToString(operator_kind) +
                              " is not converted to mojo as activation.");
  }
}

base::expected<OperationPtr, String> CreateBatchNormalizationOperation(
    const OperandToIdMap& operand_to_id_map,
    const MLOperator* batch_normalization) {
  auto batch_normalization_mojo =
      webnn::mojom::blink::BatchNormalization::New();
  batch_normalization_mojo->input_operand_id =
      GetOperatorInputId(batch_normalization, operand_to_id_map, 0);
  batch_normalization_mojo->mean_operand_id =
      GetOperatorInputId(batch_normalization, operand_to_id_map, 1);
  batch_normalization_mojo->variance_operand_id =
      GetOperatorInputId(batch_normalization, operand_to_id_map, 2);
  batch_normalization_mojo->output_operand_id =
      GetOperatorOutputId(batch_normalization, operand_to_id_map);

  const auto* options = static_cast<const MLBatchNormalizationOptions*>(
      batch_normalization->Options());
  CHECK(options);
  if (options->hasScale()) {
    batch_normalization_mojo->scale_operand_id =
        operand_to_id_map.at(options->scale());
  }
  if (options->hasBias()) {
    batch_normalization_mojo->bias_operand_id =
        operand_to_id_map.at(options->bias());
  }
  batch_normalization_mojo->axis = options->axis();
  batch_normalization_mojo->epsilon = options->epsilon();
  if (options->hasActivation()) {
    auto activation =
        CreateActivation(operand_to_id_map, options->activation()->Operator());
    if (activation.has_value()) {
      batch_normalization_mojo->activation = std::move(activation.value());
    } else {
      return base::unexpected(activation.error());
    }
  }
  return webnn::mojom::blink::Operation::NewBatchNormalization(
      std::move(batch_normalization_mojo));
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

  auto concat_mojo = blink_mojom::Concat::New();
  concat_mojo->input_operand_ids = std::move(input_operand_ids);
  concat_mojo->output_operand_id =
      GetOperatorOutputId(concat, operand_to_id_map);
  const auto* concat_operator = static_cast<const MLConcatOperator*>(concat);

  concat_mojo->axis = concat_operator->Axis();
  return blink_mojom::Operation::NewConcat(std::move(concat_mojo));
}

template <typename MLConv2dOptionsType>
base::expected<OperationPtr, String> CreateConv2dOperation(
    const OperandToIdMap& operand_to_id_map,
    const MLOperator* conv2d) {
  auto conv2d_mojo = blink_mojom::Conv2d::New();
  conv2d_mojo->input_operand_id =
      GetOperatorInputId(conv2d, operand_to_id_map, 0);
  conv2d_mojo->filter_operand_id =
      GetOperatorInputId(conv2d, operand_to_id_map, 1);
  conv2d_mojo->output_operand_id =
      GetOperatorOutputId(conv2d, operand_to_id_map);

  const auto* options =
      static_cast<const MLConv2dOptionsType*>(conv2d->Options());
  CHECK(options);

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

  // Get and validate filter.
  CHECK_GT(conv2d->Inputs().size(), 1u);
  const auto* filter = conv2d->Inputs()[1].Get();
  CHECK(filter);
  const auto filter_shape = filter->Dimensions();
  CHECK_EQ(filter_shape.size(), 4u);

  webnn::Padding2d padding;
  if constexpr (std::is_same<MLConv2dOptionsType, MLConv2dOptions>::value) {
    conv2d_mojo->type = blink_mojom::Conv2d::Type::kDirect;

    if (options->filterLayout().AsEnum() !=
        blink::V8MLConv2dFilterOperandLayout::Enum::kOihw) {
      // The filter layout is being discussed to simplify other variants in
      // WebNN working group
      // https://github.com/webmachinelearning/webnn/issues/324.
      return base::unexpected(
          String::Format("The filter layout %s is not supported.",
                         options->filterLayout().AsCStr()));
    }
    // Get height and width of filter operand for calculating padding.
    auto filter_height = filter_shape[2];
    auto filter_width = filter_shape[3];

    // Calculate the padding given input sizes, filter size, padding, strides
    // and dilations.
    padding = blink::CalculatePadding2D(
        options, input_size.height, input_size.width, filter_height,
        filter_width, conv2d_mojo->strides->height, conv2d_mojo->strides->width,
        conv2d_mojo->dilations->height, conv2d_mojo->dilations->width);
  } else if constexpr (std::is_same<MLConv2dOptionsType,
                                    MLConvTranspose2dOptions>::value) {
    conv2d_mojo->type = blink_mojom::Conv2d::Type::kTransposed;

    if (options->filterLayout().AsEnum() !=
        blink::V8MLConvTranspose2dFilterOperandLayout::Enum::kIohw) {
      // The filter layout is being discussed to simplify other variants in
      // WebNN working group
      // https://github.com/webmachinelearning/webnn/issues/324.
      return base::unexpected(
          String::Format("The filter layout %s is not supported.",
                         options->filterLayout().AsCStr()));
    }
    // Get height and width of filter operand for calculating padding.
    auto filter_height = filter_shape[2];
    auto filter_width = filter_shape[3];

    // Calculate output padding of convTranspose2d for calculating padding.
    const Vector<uint32_t> default_output_padding({0, 0});
    uint32_t output_padding_height, output_padding_width;
    if (options->hasOutputSizes()) {
      const auto calculated_output_sizes = CalculateConvTransposeOutputSize2D(
          options, input_size.height, input_size.width, filter_height,
          filter_width, conv2d_mojo->strides->height,
          conv2d_mojo->strides->width, conv2d_mojo->dilations->height,
          conv2d_mojo->dilations->width,
          // Calculate output size without output padding.
          0u, 0u);

      const auto* output = conv2d->Outputs()[0].Get();
      CHECK(output);
      const auto output_shape = output->Dimensions();
      CHECK_EQ(output_shape.size(), 4u);
      uint32_t output_height, output_width;
      switch (conv2d_mojo->input_layout) {
        case blink_mojom::InputOperandLayout::kChannelsFirst: {
          output_height = output_shape[2];
          output_width = output_shape[3];
          break;
        }
        case blink_mojom::InputOperandLayout::kChannelsLast: {
          output_height = output_shape[1];
          output_width = output_shape[2];
          break;
        }
      }
      CHECK_GE(output_height, calculated_output_sizes.height);
      output_padding_height = output_height - calculated_output_sizes.height;
      CHECK_GE(output_width, calculated_output_sizes.width);
      output_padding_width = output_width - calculated_output_sizes.width;
    } else {
      output_padding_height =
          options->getOutputPaddingOr(default_output_padding)[0];
      output_padding_width =
          options->getOutputPaddingOr(default_output_padding)[1];
    }

    // Calculate the padding given input sizes, filter size, padding, strides,
    // dilations.
    padding = blink::CalculateConvTransposePadding2D(
        options, input_size.height, input_size.width, filter_height,
        filter_width, conv2d_mojo->strides->height, conv2d_mojo->strides->width,
        conv2d_mojo->dilations->height, conv2d_mojo->dilations->width,
        output_padding_height, output_padding_width);
  } else {
    NOTREACHED_NORETURN();
  }

  // The order of sequence array is [beginning_height, ending_height,
  // beginning_width, ending_width].
  conv2d_mojo->padding = blink_mojom::Padding2d::New(
      /*beginning padding*/ Size2d::New(padding.beginning.height,
                                        padding.beginning.width),
      /*ending padding*/ Size2d::New(padding.ending.height,
                                     padding.ending.width));

  // Convert `MLActivition` to `mojo::Operator` if it's configured.
  if (options->hasActivation()) {
    auto activation =
        CreateActivation(operand_to_id_map, options->activation()->Operator());
    if (activation.has_value()) {
      conv2d_mojo->activation = std::move(activation.value());
    } else {
      return base::unexpected(activation.error());
    }
  }
  return blink_mojom::Operation::NewConv2d(std::move(conv2d_mojo));
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

OperationPtr CreateElementWiseUnaryOperator(
    const OperandToIdMap& operand_to_id_map,
    const MLOperator* unary) {
  auto operator_mojo = ElementWiseUnary::New();
  operator_mojo->input_operand_id =
      GetOperatorInputId(unary, operand_to_id_map);
  operator_mojo->output_operand_id =
      GetOperatorOutputId(unary, operand_to_id_map);

  switch (unary->Kind()) {
    case MLOperator::OperatorKind::kAbs:
      operator_mojo->kind = ElementWiseUnary::Kind::kAbs;
      break;
    case MLOperator::OperatorKind::kCeil:
      operator_mojo->kind = ElementWiseUnary::Kind::kCeil;
      break;
    case MLOperator::OperatorKind::kCos:
      operator_mojo->kind = ElementWiseUnary::Kind::kCos;
      break;
    case MLOperator::OperatorKind::kExp:
      operator_mojo->kind = ElementWiseUnary::Kind::kExp;
      break;
    case MLOperator::OperatorKind::kFloor:
      operator_mojo->kind = ElementWiseUnary::Kind::kFloor;
      break;
    case MLOperator::OperatorKind::kLog:
      operator_mojo->kind = ElementWiseUnary::Kind::kLog;
      break;
    case MLOperator::OperatorKind::kNeg:
      operator_mojo->kind = ElementWiseUnary::Kind::kNeg;
      break;
    case MLOperator::OperatorKind::kSin:
      operator_mojo->kind = ElementWiseUnary::Kind::kSin;
      break;
    case MLOperator::OperatorKind::kTan:
      operator_mojo->kind = ElementWiseUnary::Kind::kTan;
      break;
    case MLOperator::OperatorKind::kLogicalNot:
      operator_mojo->kind = ElementWiseUnary::Kind::kLogicalNot;
      break;
    case MLOperator::OperatorKind::kIdentity:
      operator_mojo->kind = ElementWiseUnary::Kind::kIdentity;
      break;
    case MLOperator::OperatorKind::kSqrt:
      operator_mojo->kind = ElementWiseUnary::Kind::kSqrt;
      break;
    case MLOperator::OperatorKind::kErf:
      operator_mojo->kind = ElementWiseUnary::Kind::kErf;
      break;
    case MLOperator::OperatorKind::kReciprocal:
      operator_mojo->kind = ElementWiseUnary::Kind::kReciprocal;
      break;
    case MLOperator::OperatorKind::kCast:
      operator_mojo->kind = ElementWiseUnary::Kind::kCast;
      break;
    default:
      NOTREACHED_NORETURN();
  }
  return webnn::mojom::blink::Operation::NewElementWiseUnary(
      std::move(operator_mojo));
}

OperationPtr CreateGatherOperation(const OperandToIdMap& operand_to_id_map,
                                   const MLOperator* gather) {
  auto gather_mojo = webnn::mojom::blink::Gather::New();
  gather_mojo->input_operand_id =
      GetOperatorInputId(gather, operand_to_id_map, 0);
  gather_mojo->indices_operand_id =
      GetOperatorInputId(gather, operand_to_id_map, 1);
  gather_mojo->output_operand_id =
      GetOperatorOutputId(gather, operand_to_id_map);

  const auto* options = static_cast<const MLGatherOptions*>(gather->Options());
  CHECK(options);
  gather_mojo->axis = options->axis();

  return webnn::mojom::blink::Operation::NewGather(std::move(gather_mojo));
}

OperationPtr CreateGemmOperation(const OperandToIdMap& operand_to_id_map,
                                 const MLOperator* gemm) {
  auto gemm_mojo = webnn::mojom::blink::Gemm::New();
  gemm_mojo->a_operand_id = GetOperatorInputId(gemm, operand_to_id_map, 0);
  gemm_mojo->b_operand_id = GetOperatorInputId(gemm, operand_to_id_map, 1);
  gemm_mojo->output_operand_id = GetOperatorOutputId(gemm, operand_to_id_map);

  const auto* options = static_cast<const MLGemmOptions*>(gemm->Options());
  CHECK(options);
  if (options->hasC()) {
    gemm_mojo->c_operand_id = operand_to_id_map.at(options->c());
  }
  gemm_mojo->alpha = options->alpha();
  gemm_mojo->beta = options->beta();
  gemm_mojo->a_transpose = options->aTranspose();
  gemm_mojo->b_transpose = options->bTranspose();

  return webnn::mojom::blink::Operation::NewGemm(std::move(gemm_mojo));
}

OperationPtr CreateMatmulOperation(const OperandToIdMap& operand_to_id_map,
                                   const MLOperator* matmul) {
  auto matmul_mojo = blink_mojom::Matmul::New();
  matmul_mojo->a_operand_id = GetOperatorInputId(matmul, operand_to_id_map, 0);
  matmul_mojo->b_operand_id = GetOperatorInputId(matmul, operand_to_id_map, 1);
  matmul_mojo->output_operand_id =
      GetOperatorOutputId(matmul, operand_to_id_map);

  return blink_mojom::Operation::NewMatmul(std::move(matmul_mojo));
}

OperationPtr CreatePadOperation(const OperandToIdMap& operand_to_id_map,
                                const MLOperator* op) {
  const auto* pad = static_cast<const blink::MLPadOperator*>(op);
  CHECK(pad);
  auto pad_mojo = blink_mojom::Pad::New();
  pad_mojo->input_operand_id = GetOperatorInputId(pad, operand_to_id_map);
  pad_mojo->output_operand_id = GetOperatorOutputId(pad, operand_to_id_map);
  pad_mojo->beginning_padding = pad->BeginningPadding();
  pad_mojo->ending_padding = pad->EndingPadding();

  const auto* options = static_cast<const blink::MLPadOptions*>(pad->Options());
  CHECK(options);
  switch (options->mode().AsEnum()) {
    case blink::V8MLPaddingMode::Enum::kConstant: {
      auto constant_padding = blink_mojom::ConstantPadding::New();
      constant_padding->value = options->value();
      pad_mojo->mode =
          blink_mojom::PaddingMode::NewConstant(std::move(constant_padding));
      break;
    }
    case blink::V8MLPaddingMode::Enum::kEdge:
      pad_mojo->mode =
          blink_mojom::PaddingMode::NewEdge(blink_mojom::EdgePadding::New());
      break;
    case blink::V8MLPaddingMode::Enum::kReflection:
      pad_mojo->mode = blink_mojom::PaddingMode::NewReflection(
          blink_mojom::ReflectionPadding::New());
      break;
    case blink::V8MLPaddingMode::Enum::kSymmetric:
      pad_mojo->mode = blink_mojom::PaddingMode::NewSymmetric(
          blink_mojom::SymmetricPadding::New());
      break;
  }

  return blink_mojom::Operation::NewPad(std::move(pad_mojo));
}

OperationPtr CreatePool2dOperation(const OperandToIdMap& operand_to_id_map,
                                   const MLOperator* pool2d) {
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
  pool2d_mojo->input_operand_id = GetOperatorInputId(pool2d, operand_to_id_map);
  pool2d_mojo->output_operand_id =
      GetOperatorOutputId(pool2d, operand_to_id_map);

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

OperationPtr CreatePreluOperation(const OperandToIdMap& operand_to_id_map,
                                  const MLOperator* prelu) {
  auto prelu_mojo = blink_mojom::Prelu::New();
  prelu_mojo->input_operand_id =
      GetOperatorInputId(prelu, operand_to_id_map, 0);
  prelu_mojo->slope_operand_id =
      GetOperatorInputId(prelu, operand_to_id_map, 1);
  prelu_mojo->output_operand_id = GetOperatorOutputId(prelu, operand_to_id_map);

  return blink_mojom::Operation::NewPrelu(std::move(prelu_mojo));
}

OperationPtr CreateReduceOperator(const OperandToIdMap& operand_to_id_map,
                                  const MLOperator* reduce) {
  auto reduce_mojo = blink_mojom::Reduce::New();
  switch (reduce->Kind()) {
    case MLOperator::OperatorKind::kReduceL1:
      reduce_mojo->kind = blink_mojom::Reduce::Kind::kL1;
      break;
    case MLOperator::OperatorKind::kReduceL2:
      reduce_mojo->kind = blink_mojom::Reduce::Kind::kL2;
      break;
    case MLOperator::OperatorKind::kReduceLogSum:
      reduce_mojo->kind = blink_mojom::Reduce::Kind::kLogSum;
      break;
    case MLOperator::OperatorKind::kReduceLogSumExp:
      reduce_mojo->kind = blink_mojom::Reduce::Kind::kLogSumExp;
      break;
    case MLOperator::OperatorKind::kReduceMax:
      reduce_mojo->kind = blink_mojom::Reduce::Kind::kMax;
      break;
    case MLOperator::OperatorKind::kReduceMean:
      reduce_mojo->kind = blink_mojom::Reduce::Kind::kMean;
      break;
    case MLOperator::OperatorKind::kReduceMin:
      reduce_mojo->kind = blink_mojom::Reduce::Kind::kMin;
      break;
    case MLOperator::OperatorKind::kReduceProduct:
      reduce_mojo->kind = blink_mojom::Reduce::Kind::kProduct;
      break;
    case MLOperator::OperatorKind::kReduceSum:
      reduce_mojo->kind = blink_mojom::Reduce::Kind::kSum;
      break;
    case MLOperator::OperatorKind::kReduceSumSquare:
      reduce_mojo->kind = blink_mojom::Reduce::Kind::kSumSquare;
      break;
    default:
      NOTREACHED();
  }
  reduce_mojo->input_operand_id = GetOperatorInputId(reduce, operand_to_id_map);
  reduce_mojo->output_operand_id =
      GetOperatorOutputId(reduce, operand_to_id_map);

  const auto* options =
      static_cast<const blink::MLReduceOptions*>(reduce->Options());
  CHECK(options);
  const auto input_rank = reduce->Inputs()[0]->Dimensions().size();
  const auto axes = options->getAxesOr(CreateAllAxes(input_rank));
  CHECK_LE(axes.size(), input_rank);
  reduce_mojo->axes = axes;
  reduce_mojo->keep_dimensions = options->keepDimensions();

  return blink_mojom::Operation::NewReduce(std::move(reduce_mojo));
}

OperationPtr CreateResample2dOperation(const OperandToIdMap& operand_to_id_map,
                                       const MLOperator* resample2d) {
  auto resample2d_mojo = blink_mojom::Resample2d::New();

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
          blink_mojom::Resample2d::InterpolationMode::kNearestNeighbor;
      break;
    case blink::V8MLInterpolationMode::Enum::kLinear:
      resample2d_mojo->mode =
          blink_mojom::Resample2d::InterpolationMode::kLinear;
      break;
  }

  // When the target sizes are specified, the scales argument is ignored.
  if (!options->hasSizes()) {
    // If scales are not present, the values are assumed to be [1.0, 1.0].
    auto scales = options->getScalesOr({1.0, 1.0});
    CHECK_EQ(scales.size(), 2u);
    resample2d_mojo->scales = {scales[0], scales[1]};
  }

  // If axes are not present, the values are assumed to be [2, 3].
  auto axes = options->getAxesOr({2, 3});
  CHECK_EQ(axes.size(), 2u);
  resample2d_mojo->axes = {axes[0], axes[1]};

  return blink_mojom::Operation::NewResample2d(std::move(resample2d_mojo));
}

OperationPtr CreateReluOperation(const OperandToIdMap& operand_to_id_map,
                                 const MLOperator* relu) {
  auto relu_mojo = blink_mojom::Relu::New();
  relu_mojo->input_operand_id = GetOperatorInputId(relu, operand_to_id_map);
  relu_mojo->output_operand_id = GetOperatorOutputId(relu, operand_to_id_map);
  return blink_mojom::Operation::NewRelu(std::move(relu_mojo));
}

OperationPtr CreateReshapeOperation(const OperandToIdMap& operand_to_id_map,
                                    const MLOperator* reshape) {
  auto reshape_mojo = blink_mojom::Reshape::New();
  reshape_mojo->input_operand_id =
      GetOperatorInputId(reshape, operand_to_id_map);
  reshape_mojo->output_operand_id =
      GetOperatorOutputId(reshape, operand_to_id_map);
  return blink_mojom::Operation::NewReshape(std::move(reshape_mojo));
}

OperationPtr CreateSigmoidOperation(const OperandToIdMap& operand_to_id_map,
                                    const MLOperator* sigmoid) {
  auto sigmoid_mojo = blink_mojom::Sigmoid::New();
  sigmoid_mojo->input_operand_id =
      GetOperatorInputId(sigmoid, operand_to_id_map);
  sigmoid_mojo->output_operand_id =
      GetOperatorOutputId(sigmoid, operand_to_id_map);
  return blink_mojom::Operation::NewSigmoid(std::move(sigmoid_mojo));
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
  auto softmax_mojo = blink_mojom::Softmax::New();
  softmax_mojo->input_operand_id =
      GetOperatorInputId(softmax, operand_to_id_map);
  softmax_mojo->output_operand_id =
      GetOperatorOutputId(softmax, operand_to_id_map);
  return blink_mojom::Operation::NewSoftmax(std::move(softmax_mojo));
}

OperationPtr CreateSplitOperation(const OperandToIdMap& operand_to_id_map,
                                  const MLOperator* split) {
  auto split_mojo = blink_mojom::Split::New();
  split_mojo->input_operand_id = GetOperatorInputId(split, operand_to_id_map);
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
  return blink_mojom::Operation::NewSplit(std::move(split_mojo));
}

OperationPtr CreateTanhOperation(const OperandToIdMap& operand_to_id_map,
                                 const MLOperator* tanh) {
  auto tanh_mojo = blink_mojom::Tanh::New();
  tanh_mojo->input_operand_id = GetOperatorInputId(tanh, operand_to_id_map);
  tanh_mojo->output_operand_id = GetOperatorOutputId(tanh, operand_to_id_map);
  return blink_mojom::Operation::NewTanh(std::move(tanh_mojo));
}

OperationPtr CreateTransposeOperation(const OperandToIdMap& operand_to_id_map,
                                      const MLOperator* transpose) {
  auto transpose_mojo = blink_mojom::Transpose::New();
  transpose_mojo->input_operand_id =
      GetOperatorInputId(transpose, operand_to_id_map);
  transpose_mojo->output_operand_id =
      GetOperatorOutputId(transpose, operand_to_id_map);
  const auto* options =
      static_cast<const MLTransposeOptions*>(transpose->Options());
  CHECK(options);

  auto input_rank = transpose->Inputs()[0]->Dimensions().size();
  transpose_mojo->permutation =
      options->getPermutationOr(CreateDefaultPermutation(input_rank));
  CHECK_EQ(transpose_mojo->permutation.size(), input_rank);

  return blink_mojom::Operation::NewTranspose(std::move(transpose_mojo));
}

OperationPtr CreateWhereOperation(const OperandToIdMap& operand_to_id_map,
                                  const MLOperator* where) {
  auto where_mojo = blink_mojom::Where::New();
  where_mojo->condition_operand_id =
      GetOperatorInputId(where, operand_to_id_map, 0);
  where_mojo->true_value_operand_id =
      GetOperatorInputId(where, operand_to_id_map, 1);
  where_mojo->false_value_operand_id =
      GetOperatorInputId(where, operand_to_id_map, 2);
  where_mojo->output_operand_id = GetOperatorOutputId(where, operand_to_id_map);

  return blink_mojom::Operation::NewWhere(std::move(where_mojo));
}

}  // namespace

// TODO(crbug.com/1504405): Use a lookup table to simplifie the switch logic.
base::expected<OperationPtr, String> ConvertToMojoOperation(
    const OperandToIdMap& operand_to_id_map,
    const MLOperator* op) {
  switch (op->Kind()) {
    case MLOperator::OperatorKind::kBatchNormalization:
      return CreateBatchNormalizationOperation(operand_to_id_map, op);
    case MLOperator::OperatorKind::kClamp:
      return blink_mojom::Operation::NewClamp(
          CreateClamp(operand_to_id_map, op, false));
    case MLOperator::OperatorKind::kConcat:
      return CreateConcatOperation(operand_to_id_map, op);
    case MLOperator::OperatorKind::kConv2d:
      return CreateConv2dOperation<MLConv2dOptions>(operand_to_id_map, op);
    case MLOperator::OperatorKind::kConvTranspose2d:
      return CreateConv2dOperation<MLConvTranspose2dOptions>(operand_to_id_map,
                                                             op);
    case MLOperator::OperatorKind::kAdd:
      [[fallthrough]];
    case MLOperator::OperatorKind::kSub:
      [[fallthrough]];
    case MLOperator::OperatorKind::kMul:
      [[fallthrough]];
    case MLOperator::OperatorKind::kDiv:
      [[fallthrough]];
    case MLOperator::OperatorKind::kMin:
      [[fallthrough]];
    case MLOperator::OperatorKind::kMax:
      [[fallthrough]];
    case MLOperator::OperatorKind::kPow:
      return CreateElementWiseBinaryOperator(operand_to_id_map, op);
    case MLOperator::OperatorKind::kAbs:
      [[fallthrough]];
    case MLOperator::OperatorKind::kCeil:
      [[fallthrough]];
    case MLOperator::OperatorKind::kCos:
      [[fallthrough]];
    case MLOperator::OperatorKind::kExp:
      [[fallthrough]];
    case MLOperator::OperatorKind::kFloor:
      [[fallthrough]];
    case MLOperator::OperatorKind::kLog:
      [[fallthrough]];
    case MLOperator::OperatorKind::kNeg:
      [[fallthrough]];
    case MLOperator::OperatorKind::kSin:
      [[fallthrough]];
    case MLOperator::OperatorKind::kTan:
      [[fallthrough]];
    case MLOperator::OperatorKind::kLogicalNot:
      [[fallthrough]];
    case MLOperator::OperatorKind::kIdentity:
      [[fallthrough]];
    case MLOperator::OperatorKind::kSqrt:
      [[fallthrough]];
    case MLOperator::OperatorKind::kErf:
      [[fallthrough]];
    case MLOperator::OperatorKind::kReciprocal:
      [[fallthrough]];
    case MLOperator::OperatorKind::kCast:
      return CreateElementWiseUnaryOperator(operand_to_id_map, op);
    case MLOperator::OperatorKind::kElu:
      return blink_mojom::Operation::NewElu(
          CreateElu(operand_to_id_map, op, false));
    case MLOperator::OperatorKind::kExpand:
      return CreateExpandOperation(operand_to_id_map, op);
    case MLOperator::OperatorKind::kGather:
      return CreateGatherOperation(operand_to_id_map, op);
    case MLOperator::OperatorKind::kGemm:
      return CreateGemmOperation(operand_to_id_map, op);
    case MLOperator::OperatorKind::kLeakyRelu:
      return blink_mojom::Operation::NewLeakyRelu(
          CreateLeakyRelu(operand_to_id_map, op, false));
    case MLOperator::OperatorKind::kMatmul:
      return CreateMatmulOperation(operand_to_id_map, op);
    case MLOperator::OperatorKind::kPad:
      return CreatePadOperation(operand_to_id_map, op);
    case MLOperator::OperatorKind::kAveragePool2d:
      [[fallthrough]];
    case MLOperator::OperatorKind::kMaxPool2d:
      return CreatePool2dOperation(operand_to_id_map, op);
    case MLOperator::OperatorKind::kPRelu:
      return CreatePreluOperation(operand_to_id_map, op);
    case MLOperator::OperatorKind::kReduceL1:
      [[fallthrough]];
    case MLOperator::OperatorKind::kReduceL2:
      [[fallthrough]];
    case MLOperator::OperatorKind::kReduceLogSum:
      [[fallthrough]];
    case MLOperator::OperatorKind::kReduceLogSumExp:
      [[fallthrough]];
    case MLOperator::OperatorKind::kReduceMax:
      [[fallthrough]];
    case MLOperator::OperatorKind::kReduceMean:
      [[fallthrough]];
    case MLOperator::OperatorKind::kReduceMin:
      [[fallthrough]];
    case MLOperator::OperatorKind::kReduceProduct:
      [[fallthrough]];
    case MLOperator::OperatorKind::kReduceSum:
      [[fallthrough]];
    case MLOperator::OperatorKind::kReduceSumSquare:
      return CreateReduceOperator(operand_to_id_map, op);
    case MLOperator::OperatorKind::kResample2d:
      return CreateResample2dOperation(operand_to_id_map, op);
    case MLOperator::OperatorKind::kRelu:
      return CreateReluOperation(operand_to_id_map, op);
    case MLOperator::OperatorKind::kReshape:
      return CreateReshapeOperation(operand_to_id_map, op);
    case MLOperator::OperatorKind::kSigmoid:
      return CreateSigmoidOperation(operand_to_id_map, op);
    case MLOperator::OperatorKind::kSlice:
      return CreateSliceOperation(operand_to_id_map, op);
    case MLOperator::OperatorKind::kSoftmax:
      return CreateSoftmaxOperation(operand_to_id_map, op);
    case MLOperator::OperatorKind::kSplit:
      return CreateSplitOperation(operand_to_id_map, op);
    case MLOperator::OperatorKind::kTanh:
      return CreateTanhOperation(operand_to_id_map, op);
    case MLOperator::OperatorKind::kTranspose:
      return CreateTransposeOperation(operand_to_id_map, op);
    case MLOperator::OperatorKind::kWhere:
      return CreateWhereOperation(operand_to_id_map, op);
    default:
      return base::unexpected(MLOperator::OperatorKindToString(op->Kind()) +
                              " is not implemented.");
  }
  NOTREACHED_NORETURN();
}

}  // namespace blink
