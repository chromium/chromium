// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"

#include <algorithm>

#include "base/containers/span.h"
#include "base/notimplemented.h"
#include "base/numerics/checked_math.h"
#include "base/ranges/algorithm.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/webnn_errors.h"
#include "services/webnn/public/mojom/features.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_arg_min_max_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_batch_normalization_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_clamp_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_transpose_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_cumulative_sum_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_elu_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gather_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gemm_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gru_cell_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gru_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_hard_sigmoid_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_instance_normalization_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_layer_normalization_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_leaky_relu_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_linear_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_lstm_cell_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_lstm_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operator_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pad_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pool_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_recurrent_network_activation.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_reduce_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_resample_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_split_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_transpose_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_triangular_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_constant_operand.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_error.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_type_converter.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

#define THROW_AND_RETURN_TYPE_IF_ERROR(func, return_value) \
  RETURN_IF_ERROR(func, [&exception_state](String error) { \
    exception_state.ThrowTypeError(error);                 \
    return return_value;                                   \
  });

#define THROW_AND_RETURN_IF_ERROR(func, return_value)                       \
  RETURN_IF_ERROR(func, [&exception_state](String error) {                  \
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError, \
                                      error);                               \
    return return_value;                                                    \
  });

#define ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(lhs, rexpr)                \
  ASSIGN_OR_RETURN(lhs, rexpr, [&exception_state](std::string error) { \
    exception_state.ThrowTypeError(String::FromUTF8(error));           \
    return nullptr;                                                    \
  });

constexpr char kGraphAlreadyBuiltError[] =
    "This MLGraphBuilder has already built a graph.";

webnn::InputOperandLayout BlinkInputOperandLayoutToComponent(
    blink::V8MLInputOperandLayout::Enum type) {
  switch (type) {
    case blink::V8MLInputOperandLayout::Enum::kNchw:
      return webnn::InputOperandLayout::kNchw;
    case blink::V8MLInputOperandLayout::Enum::kNhwc:
      return webnn::InputOperandLayout::kNhwc;
  }
}

webnn::Conv2dFilterOperandLayout BlinkConv2dFilterLayoutToComponent(
    blink::V8MLConv2dFilterOperandLayout::Enum type) {
  switch (type) {
    case blink::V8MLConv2dFilterOperandLayout::Enum::kOihw:
      return webnn::Conv2dFilterOperandLayout::kOihw;
    case blink::V8MLConv2dFilterOperandLayout::Enum::kHwio:
      return webnn::Conv2dFilterOperandLayout::kHwio;
    case blink::V8MLConv2dFilterOperandLayout::Enum::kOhwi:
      return webnn::Conv2dFilterOperandLayout::kOhwi;
    case blink::V8MLConv2dFilterOperandLayout::Enum::kIhwo:
      return webnn::Conv2dFilterOperandLayout::kIhwo;
  }
}

webnn::ConvTranspose2dFilterOperandLayout
BlinkConvTranspose2dFilterLayoutToComponent(
    blink::V8MLConvTranspose2dFilterOperandLayout::Enum type) {
  switch (type) {
    case blink::V8MLConvTranspose2dFilterOperandLayout::Enum::kIohw:
      return webnn::ConvTranspose2dFilterOperandLayout::kIohw;
    case blink::V8MLConvTranspose2dFilterOperandLayout::Enum::kHwoi:
      return webnn::ConvTranspose2dFilterOperandLayout::kHwoi;
    case blink::V8MLConvTranspose2dFilterOperandLayout::Enum::kOhwi:
      return webnn::ConvTranspose2dFilterOperandLayout::kOhwi;
  }
}

webnn::RoundingType BlinkRoundingTypeToComponent(
    blink::V8MLRoundingType::Enum type) {
  switch (type) {
    case blink::V8MLRoundingType::Enum::kFloor:
      return webnn::RoundingType::kFloor;
    case blink::V8MLRoundingType::Enum::kCeil:
      return webnn::RoundingType::kCeil;
  }
}

webnn::Pool2dKind FromMojoPool2dKind(webnn::mojom::blink::Pool2d::Kind kind) {
  switch (kind) {
    case webnn::mojom::blink::Pool2d::Kind::kAveragePool2d:
      return webnn::Pool2dKind::kAverage;
    case webnn::mojom::blink::Pool2d::Kind::kL2Pool2d:
      return webnn::Pool2dKind::kL2;
    case webnn::mojom::blink::Pool2d::Kind::kMaxPool2d:
      return webnn::Pool2dKind::kMax;
  }
}

webnn::ReduceKind MojoReduceKindToComponent(
    webnn::mojom::blink::Reduce::Kind kind) {
  switch (kind) {
    case webnn::mojom::blink::Reduce::Kind::kL1:
      return webnn::ReduceKind::kL1;
    case webnn::mojom::blink::Reduce::Kind::kL2:
      return webnn::ReduceKind::kL2;
    case webnn::mojom::blink::Reduce::Kind::kLogSum:
      return webnn::ReduceKind::kLogSum;
    case webnn::mojom::blink::Reduce::Kind::kLogSumExp:
      return webnn::ReduceKind::kLogSumExp;
    case webnn::mojom::blink::Reduce::Kind::kMax:
      return webnn::ReduceKind::kMax;
    case webnn::mojom::blink::Reduce::Kind::kMean:
      return webnn::ReduceKind::kMean;
    case webnn::mojom::blink::Reduce::Kind::kMin:
      return webnn::ReduceKind::kMin;
    case webnn::mojom::blink::Reduce::Kind::kProduct:
      return webnn::ReduceKind::kProduct;
    case webnn::mojom::blink::Reduce::Kind::kSum:
      return webnn::ReduceKind::kSum;
    case webnn::mojom::blink::Reduce::Kind::kSumSquare:
      return webnn::ReduceKind::kSumSquare;
  }
}

webnn::RecurrentNetworkDirection BlinkRecurrentNetworkDirectionToComponent(
    blink::V8MLRecurrentNetworkDirection::Enum direction) {
  switch (direction) {
    case blink::V8MLRecurrentNetworkDirection::Enum::kForward:
      return webnn::RecurrentNetworkDirection::kForward;
    case blink::V8MLRecurrentNetworkDirection::Enum::kBackward:
      return webnn::RecurrentNetworkDirection::kBackward;
    case blink::V8MLRecurrentNetworkDirection::Enum::kBoth:
      return webnn::RecurrentNetworkDirection::kBoth;
  }
}

webnn::BatchNormalizationAttributes ConvertToBatchNormalizationAttributes(
    const blink::MLBatchNormalizationOptions* options) {
  CHECK(options);
  webnn::BatchNormalizationAttributes attributes;
  if (options->hasScale()) {
    attributes.scale = options->scale()->Descriptor();
  }
  if (options->hasBias()) {
    attributes.bias = options->bias()->Descriptor();
  }
  attributes.label = options->label().Utf8();
  attributes.axis = options->axis();
  return attributes;
}

template <typename MLConv2dOptionsType, typename Conv2dAttributesType>
base::expected<Conv2dAttributesType, String> ConvertToConv2dAttributesBase(
    const MLConv2dOptionsType* options) {
  Conv2dAttributesType attributes;
  CHECK(options);
  const std::string label = options->label().Utf8();
  // If padding is not present, the values are assumed to be [0,0,0,0].
  auto padding = options->getPaddingOr({0, 0, 0, 0});
  if (padding.size() != 4) {
    return base::unexpected(
        String::FromUTF8(webnn::GetErrorLabelPrefix(label)) +
        "The length of padding should be 4.");
  }
  // The order of padding array is [beginning_height, ending_height,
  // beginning_width, ending_width].
  attributes.padding = webnn::Padding2d{
      .beginning =
          webnn::Size2d<uint32_t>{.height = padding[0], .width = padding[2]},
      .ending =
          webnn::Size2d<uint32_t>{.height = padding[1], .width = padding[3]}};

  // If strides is not present, the values are assumed to be [1,1].
  auto strides = options->getStridesOr({1, 1});
  if (strides.size() != 2) {
    return base::unexpected(
        String::FromUTF8(webnn::GetErrorLabelPrefix(label)) +
        "The length of strides should be 2.");
  }
  attributes.strides =
      webnn::Size2d<uint32_t>{.height = strides[0], .width = strides[1]};

  // If dilations is not present, the values are assumed to be [1,1].
  auto dilations = options->getDilationsOr({1, 1});
  if (dilations.size() != 2) {
    return base::unexpected(
        String::FromUTF8(webnn::GetErrorLabelPrefix(label)) +
        +"The length of dilations should be 2.");
  }
  attributes.dilations =
      webnn::Size2d<uint32_t>{.height = dilations[0], .width = dilations[1]};
  attributes.groups = options->groups();
  attributes.input_layout =
      BlinkInputOperandLayoutToComponent(options->inputLayout().AsEnum());
  if (options->hasBias()) {
    attributes.bias_operand = options->bias()->Descriptor();
  }
  attributes.label = label;

  return std::move(attributes);
}

base::expected<webnn::Conv2dAttributes, String> ConvertToConv2dAttributes(
    const blink::MLConv2dOptions* options) {
  auto attributes =
      ConvertToConv2dAttributesBase<blink::MLConv2dOptions,
                                    webnn::Conv2dAttributes>(options);
  if (!attributes.has_value()) {
    return base::unexpected(attributes.error());
  }
  attributes.value().filter_layout =
      BlinkConv2dFilterLayoutToComponent(options->filterLayout().AsEnum());

  return attributes;
}

base::expected<webnn::ConvTranspose2dAttributes, String>
ConvertToConvTranspose2dAttributes(
    const blink::MLConvTranspose2dOptions* options) {
  auto attributes =
      ConvertToConv2dAttributesBase<blink::MLConvTranspose2dOptions,
                                    webnn::ConvTranspose2dAttributes>(options);
  if (!attributes.has_value()) {
    return base::unexpected(attributes.error());
  }

  const std::string& label = attributes.value().label;
  // If output padding is not present, the values are assumed to be [0,0].
  const auto output_padding = options->getOutputPaddingOr({0, 0});
  if (output_padding.size() != 2) {
    return base::unexpected(
        String::FromUTF8(webnn::GetErrorLabelPrefix(label)) +
        "The length of output padding should be 2.");
  }
  attributes.value().output_padding = webnn::Size2d<uint32_t>{
      .height = output_padding[0], .width = output_padding[1]};

  if (options->hasOutputSizes()) {
    auto output_sizes = options->getOutputSizesOr({});
    if (output_sizes.size() != 2) {
      return base::unexpected(
          String::FromUTF8(webnn::GetErrorLabelPrefix(label)) +
          "The length of output sizes should be 2.");
    }
    attributes.value().output_sizes = webnn::Size2d<uint32_t>{
        .height = output_sizes[0], .width = output_sizes[1]};
  }

  attributes.value().filter_layout =
      BlinkConvTranspose2dFilterLayoutToComponent(
          options->filterLayout().AsEnum());

  return attributes;
}

base::expected<webnn::Pool2dAttributes, std::string> ConvertToPool2dAttributes(
    const blink::MLPool2dOptions* options) {
  CHECK(options);
  const std::string label = options->label().Utf8();
  webnn::Pool2dAttributes attributes;
  if (options->hasWindowDimensions()) {
    auto& window_dimensions = options->windowDimensions();
    if (window_dimensions.size() != 2) {
      return base::unexpected(webnn::GetErrorLabelPrefix(label) +
                              "The length of window dimensions should be 2.");
    }
    attributes.window_dimensions = webnn::Size2d<uint32_t>{
        .height = window_dimensions[0], .width = window_dimensions[1]};
  }

  // If padding is not present, the values are assumed to be [0,0,0,0].
  auto padding = options->getPaddingOr({0, 0, 0, 0});
  if (padding.size() != 4) {
    return base::unexpected(webnn::GetErrorLabelPrefix(label) +
                            "The length of padding should be 4.");
  }
  attributes.padding = webnn::Padding2d{
      .beginning =
          webnn::Size2d<uint32_t>{.height = padding[0], .width = padding[2]},
      .ending =
          webnn::Size2d<uint32_t>{.height = padding[1], .width = padding[3]}};

  // If strides is not present, the values are assumed to be [1,1].
  auto strides = options->getStridesOr({1, 1});
  if (strides.size() != 2) {
    return base::unexpected(webnn::GetErrorLabelPrefix(label) +
                            "The length of strides should be 2.");
  }
  attributes.strides =
      webnn::Size2d<uint32_t>{.height = strides[0], .width = strides[1]};

  // If dilations is not present, the values are assumed to be [1,1].
  auto dilations = options->getDilationsOr({1, 1});
  if (dilations.size() != 2) {
    return base::unexpected(webnn::GetErrorLabelPrefix(label) +
                            "The length of dilations should be 2.");
  }
  attributes.dilations =
      webnn::Size2d<uint32_t>{.height = dilations[0], .width = dilations[1]};
  attributes.layout =
      BlinkInputOperandLayoutToComponent(options->layout().AsEnum());
  attributes.rounding_type =
      BlinkRoundingTypeToComponent(options->roundingType().AsEnum());
  if (options->hasOutputSizes()) {
    // TODO(ningxin.hu@intel.com): report a DevTools warning message if rounding
    // type is provided but ignored.
    auto& output_size = options->outputSizes();
    if (output_size.size() != 2) {
      return base::unexpected(webnn::GetErrorLabelPrefix(label) +
                              "The length of output sizes should be 2.");
    }
    attributes.output_sizes = webnn::Size2d<uint32_t>{.height = output_size[0],
                                                      .width = output_size[1]};
  }
  attributes.label = label;
  return attributes;
}

webnn::GemmAttributes ConvertToGemmAttributes(
    const blink::MLGemmOptions* options) {
  CHECK(options);
  webnn::GemmAttributes attributes;
  if (options->hasC()) {
    attributes.c_operand = options->c()->Descriptor();
  }
  attributes.alpha = options->alpha();
  attributes.beta = options->beta();
  attributes.a_transpose = options->aTranspose();
  attributes.b_transpose = options->bTranspose();
  attributes.label = options->label().Utf8();
  return attributes;
}

webnn::GruAttributes ConvertToGruAttributes(MLGraphBuilder* builder,
                                            blink::MLGruOptions* options) {
  CHECK(options);
  webnn::GruAttributes attributes;

  if (options->hasBias()) {
    attributes.bias = options->bias()->Descriptor();
  }
  if (options->hasRecurrentBias()) {
    attributes.recurrent_bias = options->recurrentBias()->Descriptor();
  }
  if (options->hasInitialHiddenState()) {
    attributes.initial_hidden_state =
        options->initialHiddenState()->Descriptor();
  }
  attributes.return_sequence = options->returnSequence();
  attributes.direction =
      BlinkRecurrentNetworkDirectionToComponent(options->direction().AsEnum());
  if (!options->hasActivations()) {
    // Create a default activation sequence as defined in the spec.
    options->setActivations(
        {V8MLRecurrentNetworkActivation(
             V8MLRecurrentNetworkActivation::Enum::kSigmoid),
         V8MLRecurrentNetworkActivation(
             V8MLRecurrentNetworkActivation::Enum::kTanh)});
  }
  attributes.activation_count = options->activations().size();
  attributes.label = options->label().Utf8();
  return attributes;
}

webnn::GruCellAttributes ConvertToGruCellAttributes(
    MLGraphBuilder* builder,
    blink::MLGruCellOptions* options) {
  CHECK(options);
  webnn::GruCellAttributes attributes;

  if (options->hasBias()) {
    attributes.bias = options->bias()->Descriptor();
  }
  if (options->hasRecurrentBias()) {
    attributes.recurrent_bias = options->recurrentBias()->Descriptor();
  }
  if (!options->hasActivations()) {
    // Create a default activation sequence as defined in the spec.
    options->setActivations(
        {V8MLRecurrentNetworkActivation(
             V8MLRecurrentNetworkActivation::Enum::kSigmoid),
         V8MLRecurrentNetworkActivation(
             V8MLRecurrentNetworkActivation::Enum::kTanh)});
  }
  attributes.activation_count = options->activations().size();
  attributes.label = options->label().Utf8();
  return attributes;
}

webnn::InstanceNormalizationAttributes ConvertToInstanceNormalizationAttributes(
    const blink::MLInstanceNormalizationOptions* options) {
  CHECK(options);
  webnn::InstanceNormalizationAttributes attributes;
  if (options->hasScale()) {
    attributes.scale = options->scale()->Descriptor();
  }
  if (options->hasBias()) {
    attributes.bias = options->bias()->Descriptor();
  }
  attributes.layout =
      BlinkInputOperandLayoutToComponent(options->layout().AsEnum());
  attributes.label = options->label().Utf8();
  return attributes;
}

webnn::LayerNormalizationAttributes ConvertToLayerNormalizationAttributes(
    const blink::MLLayerNormalizationOptions* options) {
  CHECK(options);
  webnn::LayerNormalizationAttributes attributes;
  if (options->hasScale()) {
    attributes.scale = options->scale()->Descriptor();
  }
  if (options->hasBias()) {
    attributes.bias = options->bias()->Descriptor();
  }
  attributes.label = options->label().Utf8();
  return attributes;
}

webnn::LstmAttributes ConvertToLstmAttributes(
    const blink::MLLstmOptions* options) {
  CHECK(options);
  webnn::LstmAttributes attributes;

  if (options->hasBias()) {
    attributes.bias = options->bias()->Descriptor();
  }
  if (options->hasRecurrentBias()) {
    attributes.recurrent_bias = options->recurrentBias()->Descriptor();
  }
  if (options->hasPeepholeWeight()) {
    attributes.peephole_weight = options->peepholeWeight()->Descriptor();
  }
  if (options->hasInitialHiddenState()) {
    attributes.initial_hidden_state =
        options->initialHiddenState()->Descriptor();
  }
  if (options->hasInitialCellState()) {
    attributes.initial_cell_state = options->initialCellState()->Descriptor();
  }
  attributes.activation_count = options->activations().size();
  attributes.return_sequence = options->returnSequence();
  attributes.direction =
      BlinkRecurrentNetworkDirectionToComponent(options->direction().AsEnum());
  attributes.label = options->label().Utf8();
  return attributes;
}

webnn::LstmCellAttributes ConvertToLstmCellAttributes(
    const blink::MLLstmCellOptions* options) {
  CHECK(options);
  webnn::LstmCellAttributes attributes;

  if (options->hasBias()) {
    attributes.bias = options->bias()->Descriptor();
  }
  if (options->hasRecurrentBias()) {
    attributes.recurrent_bias = options->recurrentBias()->Descriptor();
  }
  if (options->hasPeepholeWeight()) {
    attributes.peephole_weight = options->peepholeWeight()->Descriptor();
  }
  attributes.activation_count = options->activations().size();
  attributes.label = options->label().Utf8();
  return attributes;
}

bool ValidateClampOptions(const MLClampOptions* options,
                          ExceptionState& exception_state) {
  // The generated code of MLClampOptions uses blink::ToRestrictedFloat to
  // convert the min/max value to a single precision float. It will throw on
  // non-finite values.
  const std::string label = options->label().Utf8();
  if (options->hasMinValue() && options->hasMaxValue()) {
    if (options->minValue() > options->maxValue()) {
      exception_state.ThrowTypeError(
          String::FromUTF8(webnn::GetErrorLabelPrefix(label)) +
          String::Format("The min value (%f) should be less than or equal to "
                         "the max value (%f).",
                         options->minValue(), options->maxValue()));
      return false;
    }
  }
  return true;
}

MLOperand* BuildArgMinMax(MLGraphBuilder* builder,
                          webnn::mojom::blink::ArgMinMax::Kind sub_kind,
                          const MLOperand* input,
                          const uint32_t axis,
                          const MLArgMinMaxOptions* options,
                          ExceptionState& exception_state) {
  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateArgMinMaxAndInferOutput(
          builder->GetContext()->GetProperties(), input->Descriptor(),
          options->label().Utf8(), axis,
          FromBlinkDataType(options->outputDataType().AsEnum()),
          options->keepDimensions()));

  auto* arg_min_max = MakeGarbageCollected<MLArgMinMaxOperator>(
      builder, sub_kind, axis, options);
  MLOperand* output = MLOperand::CreateOutput(
      builder, std::move(output_descriptor), arg_min_max);
  arg_min_max->Connect({input}, {output});

  return output;
}

MLOperand* BuildElementWiseBinary(
    MLGraphBuilder* builder,
    webnn::mojom::blink::ElementWiseBinary::Kind kind,
    const webnn::SupportedDataTypes& data_type_constraint,
    const MLOperand* a,
    const MLOperand* b,
    const MLOperatorOptions* options,
    ExceptionState& exception_state) {
  const std::string label = options->label().Utf8();
  if (!data_type_constraint.Has(a->DataType())) {
    exception_state.ThrowTypeError(
        String::FromUTF8(webnn::GetErrorLabelPrefix(label)) +
        String(NotSupportedArgumentTypeError("a", a->DataType(),
                                             data_type_constraint)));
    return nullptr;
  }
  if (!data_type_constraint.Has(b->DataType())) {
    exception_state.ThrowTypeError(
        String::FromUTF8(webnn::GetErrorLabelPrefix(label)) +
        String(NotSupportedArgumentTypeError("b", b->DataType(),
                                             data_type_constraint)));
    return nullptr;
  }

  if (a->DataType() != b->DataType()) {
    exception_state.ThrowTypeError(
        String::FromUTF8(webnn::GetErrorLabelPrefix(label)) +
        "The input operand data types don't match.");
    return nullptr;
  }
  auto output_shape = webnn::BroadcastShapes(a->Shape(), b->Shape());
  if (!output_shape) {
    exception_state.ThrowTypeError(
        String::FromUTF8(webnn::GetErrorLabelPrefix(label)) +
        "The input shapes are not broadcastable.");
    return nullptr;
  }

  // Logical operator outputs are bools, otherwise output operators are the same
  // type as input operators.
  webnn::OperandDataType data_type = IsLogicalBinaryOperator(kind)
                                         ? webnn::OperandDataType::kUint8
                                         : a->DataType();

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::OperandDescriptor::Create(data_type, *output_shape));

  auto* binary = MakeGarbageCollected<MLOperator>(
      builder, /*kind=*/webnn::mojom::blink::Operation::Tag::kElementWiseBinary,
      options, /*sub_kind=*/kind);
  MLOperand* output =
      MLOperand::CreateOutput(builder, std::move(output_descriptor), binary);

  binary->Connect({a, b}, {output});
  return output;
}

MLOperand* BuildUnaryOperator(
    MLGraphBuilder* builder,
    ExceptionState& exception_state,
    webnn::mojom::blink::Operation::Tag kind,
    const webnn::SupportedDataTypes& data_type_constraint,
    const MLOperand* input,
    const MLOperatorOptions* options) {
  // The output tensor of unary operator has the same data type and dimensions
  // as its input tensor.
  if (!data_type_constraint.Has(input->DataType())) {
    exception_state.ThrowTypeError(
        String::FromUTF8(webnn::GetErrorLabelPrefix(options->label().Utf8())) +
        String(NotSupportedInputArgumentTypeError(input->DataType(),
                                                  data_type_constraint)));
    return nullptr;
  }

  auto* unary = MakeGarbageCollected<MLOperator>(builder, kind, options);

  MLOperand* output =
      MLOperand::CreateOutput(builder, input->Descriptor(), unary);
  unary->Connect({input}, {output});
  return output;
}

MLOperand* BuildElementWiseUnaryOperator(
    MLGraphBuilder* builder,
    ExceptionState& exception_state,
    webnn::mojom::blink::ElementWiseUnary::Kind kind,
    const webnn::SupportedDataTypes& data_type_constraint,
    const MLOperand* input,
    const MLOperatorOptions* options) {
  const std::string label = options->label().Utf8();
  if (!data_type_constraint.Has(input->DataType())) {
    exception_state.ThrowTypeError(
        String::FromUTF8(webnn::GetErrorLabelPrefix(options->label().Utf8())) +
        String(NotSupportedInputArgumentTypeError(input->DataType(),
                                                  data_type_constraint)));
    return nullptr;
  }

  auto* unary = MakeGarbageCollected<MLOperator>(
      builder, /*kind=*/webnn::mojom::blink::Operation::Tag::kElementWiseUnary,
      options, /*sub_kind=*/kind);
  MLOperand* output =
      MLOperand::CreateOutput(builder, input->Descriptor(), unary);
  unary->Connect({input}, {output});
  return output;
}

MLOperand* BuildReduce(MLGraphBuilder* builder,
                       webnn::mojom::blink::Reduce::Kind kind,
                       const webnn::ContextProperties& context_properties,
                       const MLOperand* input,
                       const MLReduceOptions* options,
                       ExceptionState& exception_state) {
  const auto axes = options->getAxesOr(CreateAllAxes(input->Rank()));
  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateReduceAndInferOutput(
          context_properties, MojoReduceKindToComponent(kind),
          input->Descriptor(), options->label().Utf8(), axes,
          options->keepDimensions()));

  auto* reduce = MakeGarbageCollected<MLOperator>(
      builder, /*kind=*/webnn::mojom::blink::Operation::Tag::kReduce, options,
      /*sub_kind=*/kind);
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-reduce, the output
  // tensor of reduce has the same data type as its input.
  MLOperand* output =
      MLOperand::CreateOutput(builder, std::move(output_descriptor), reduce);
  reduce->Connect({input}, {output});
  return output;
}

MLOperand* BuildPool2d(MLGraphBuilder* builder,
                       webnn::mojom::blink::Pool2d::Kind kind,
                       const webnn::ContextProperties& context_properties,
                       const MLOperand* input,
                       const MLPool2dOptions* options,
                       ExceptionState& exception_state) {
  auto pool2d_attributes = ConvertToPool2dAttributes(options);
  if (!pool2d_attributes.has_value()) {
    exception_state.ThrowTypeError(String::FromUTF8(pool2d_attributes.error()));
    return nullptr;
  }

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidatePool2dAndInferOutput(
          context_properties, input->Descriptor(),
          std::move(pool2d_attributes.value()), FromMojoPool2dKind(kind)));

  // Create pool2d operator and its output operand. Connect the pool2d operator
  // to its input and output operands.
  auto* pool2d = MakeGarbageCollected<MLOperator>(
      builder, /*kind=*/webnn::mojom::blink::Operation::Tag::kPool2d, options,
      /*sub_kind=*/kind);
  MLOperand* output =
      MLOperand::CreateOutput(builder, std::move(output_descriptor), pool2d);
  pool2d->Connect({input}, {output});
  return output;
}

// Determines the input and output resources required for this computational
// graph by traversing the graph from `named_outputs` to its inputs.
// This may fail if the graph is not valid.
base::expected<std::pair<MLGraph::NamedOperandDescriptors,
                         MLGraph::NamedOperandDescriptors>,
               String>
DetermineGraphConstraintsFromOutputs(const MLNamedOperands& named_outputs) {
  // The outputs should not be empty.
  if (named_outputs.empty()) {
    return base::unexpected("At least one output needs to be provided.");
  }

  // The queue and visited set of operators that help implement the
  // breadth-first graph traversal:
  // https://en.wikipedia.org/wiki/Breadth-first_search
  HeapDeque<Member<const MLOperator>> operators_queue;
  HeapHashSet<Member<const MLOperator>> visited_operators;

  MLGraph::NamedOperandDescriptors input_constraints;
  MLGraph::NamedOperandDescriptors output_constraints;

  // Validate the named outputs, setup corresponding output resource info and
  // initialize the queue and visited set with their dependent operators.
  for (const auto& output : named_outputs) {
    const auto& name = output.first;
    const auto& operand = output.second;
    // Validate whether it is an output operand.
    if (operand->Kind() != webnn::mojom::blink::Operand::Kind::kOutput) {
      return base::unexpected(String::Format(
          "The operand with name \"%s\" is not an output operand.",
          name.Utf8().c_str()));
    }
    // Setup resource info for this output operand.
    output_constraints.insert(name, operand->Descriptor());
    // Mark its dependent operator is visited.
    visited_operators.insert(operand->Operator());
    // Enqueue its dependent operator.
    operators_queue.push_back(operand->Operator());
  }

  // An input MLOperand may be used by more than one MLOperators. This set
  // ensures an input MLOperand won't be validated multiple times.
  HeapHashSet<Member<const MLOperand>> visited_input_operands;
  while (operators_queue.size() > 0) {
    // If the queue is not empty, dequeue an operator from the queue.
    const auto current_operator = operators_queue.TakeFirst();
    // Enumerate the current operator's input operands.
    for (const auto& operand : current_operator->Inputs()) {
      switch (operand->Kind()) {
        case webnn::mojom::blink::Operand::Kind::kOutput:
          DCHECK(operand->Operator());
          // If the operand is an output operand and its dependent operator is
          // not visited, mark the dependent operator is visited and enqueue
          // it.
          if (!visited_operators.Contains(operand->Operator())) {
            visited_operators.insert(operand->Operator());
            operators_queue.push_back(operand->Operator());
          }
          break;
        case webnn::mojom::blink::Operand::Kind::kInput:
          // If the operand has been validated, it doesn't need to be verified
          // multiple times.
          if (visited_input_operands.Contains(operand)) {
            continue;
          }
          visited_input_operands.insert(operand);
          // If the operand is an input operand, validate whether its name is
          // unique.
          if (input_constraints.Contains(operand->Name())) {
            return base::unexpected(
                String::Format("The input name \"%s\" is duplicated.",
                               operand->Name().Utf8().c_str()));
          }
          // Setup resource info for this input operand.
          input_constraints.insert(operand->Name(), operand->Descriptor());
          break;
        case webnn::mojom::blink::Operand::Kind::kConstant:
          // If the operand has been validated, it doesn't need to be verified
          // multiple times.
          if (visited_input_operands.Contains(operand)) {
            continue;
          }
          visited_input_operands.insert(operand);
          break;
      }
    }
  }
  return std::make_pair(std::move(input_constraints),
                        std::move(output_constraints));
}

base::expected<webnn::mojom::blink::GraphInfoPtr, String> BuildWebNNGraphInfo(
    const MLNamedOperands& named_outputs,
    const webnn::ContextProperties& context_properties) {
  // The `GraphInfo` represents an entire information of WebNN graph.
  auto graph_info = webnn::mojom::blink::GraphInfo::New();

  HeapHashMap<Member<const MLOperand>, uint64_t> operand_to_id_map;
  for (const auto& [name, operand] : named_outputs) {
    // Create `mojo::Operand` for output operands of graph with the name.
    auto output_operand =
        mojo::ConvertTo<webnn::mojom::blink::OperandPtr>(operand.Get());
    output_operand->name = name;
    uint64_t operand_id = NextOperandId(*graph_info);
    graph_info->id_to_operand_map.insert(operand_id, std::move(output_operand));
    graph_info->output_operands.push_back(operand_id);
    operand_to_id_map.insert(operand, operand_id);
  }

  HeapVector<Member<const MLOperator>>* topologically_sorted_operators =
      GetOperatorsInTopologicalOrder(named_outputs);

  // Optimize away redundant constant reshapes by removing the reshape operator
  // and change constant operand's descriptor to the reshape output operand's
  // descriptor.
  // The algorithm walks down all constants and its dependent operators to
  // identify redundant reshapes, skips serialization for reshape operator and
  // points the reshape output operand in `operand_to_id_map` to constant's id.
  HeapHashMap<Member<const MLOperand>, HeapHashSet<Member<const MLOperator>>>
      operand_dependencies;
  HeapHashSet<Member<const MLOperand>> constant_operands;
  for (const auto& current_operator : *topologically_sorted_operators) {
    for (const auto& operand : current_operator->Inputs()) {
      if (operand->Kind() == webnn::mojom::blink::Operand::Kind::kConstant) {
        constant_operands.insert(operand);
      }
      auto it = operand_dependencies.find(operand);
      auto& operators =
          it != operand_dependencies.end()
              ? it->value
              : operand_dependencies
                    .Set(operand, HeapHashSet<Member<const MLOperator>>())
                    .stored_value->value;

      operators.insert(current_operator);
    }
  }

  // Hash map of redundant reshape output operand from constant that can be
  // removed from the graph.
  HeapHashMap<Member<const MLOperand>, Member<const MLOperand>>
      reshaped_to_constant_mapping;
  HeapHashMap<Member<const MLOperand>, Member<const MLOperand>>
      constant_to_reshaped_mapping;

  for (const auto& constant_operand : constant_operands) {
    Member<const MLOperand> next_operand = constant_operand;
    // For each constant operand, keep walking down the dependencies until no
    // reshape is found.
    while (true) {
      auto dependent_operators = operand_dependencies.at(next_operand);
      // If reshape is the only dependent of the constant, then this reshape
      // operation can be removed from the graph.
      if (dependent_operators.size() != 1) {
        break;
      }
      auto dependent_operator = *dependent_operators.begin();
      if (dependent_operator->Kind() !=
          webnn::mojom::blink::Operation::Tag::kReshape) {
        break;
      }
      Member<const MLOperand> reshape_output = dependent_operator->Outputs()[0];
      reshaped_to_constant_mapping.Set(reshape_output, constant_operand);
      constant_to_reshaped_mapping.Set(constant_operand, reshape_output);
      next_operand = reshape_output;
    }
  }
  // Visit the operators in topological order. For each operator,
  // 1, Create `mojo::Operand` for its input and output operands if needed.
  // 2, Create `mojo::Operator` with the id of input and output operands.
  //
  // Skips the redundant constant reshapes.
  for (const auto& current_operator : *topologically_sorted_operators) {
    for (const auto& operand : current_operator->Inputs()) {
      if (operand_to_id_map.Contains(operand.Get())) {
        // The `mojo::Operand` is already converted with the MLOperand, skip it.
        continue;
      }
      switch (operand->Kind()) {
        case webnn::mojom::blink::Operand::Kind::kInput: {
          // Create `mojo::Operand` for the input MLOperand.
          uint64_t operand_id = NextOperandId(*graph_info);
          graph_info->id_to_operand_map.insert(
              operand_id,
              mojo::ConvertTo<webnn::mojom::blink::OperandPtr>(operand.Get()));
          //  Build the array of input operands for this graph with the id.
          graph_info->input_operands.push_back(operand_id);
          operand_to_id_map.insert(operand, operand_id);
          break;
        }
        case webnn::mojom::blink::Operand::Kind::kConstant: {
          // Convert `mojo::Operand` for constant operand.
          uint64_t operand_id = NextOperandId(*graph_info);
          auto mojo_operand =
              mojo::ConvertTo<webnn::mojom::blink::OperandPtr>(operand.Get());
          // Set constant's descriptor to the redundant reshape's output's
          // descriptor.
          if (constant_to_reshaped_mapping.Contains(operand)) {
            mojo_operand->descriptor =
                mojo::ConvertTo<webnn::mojom::blink::OperandPtr>(
                    constant_to_reshaped_mapping.at(operand))
                    ->descriptor;
          }
          graph_info->id_to_operand_map.insert(operand_id,
                                               std::move(mojo_operand));
          // Build the map of constant operands for this graph with the id.
          graph_info->constant_id_to_buffer_map.insert(
              operand_id, operand->AsConstantOperand()->Bytes());
          operand_to_id_map.insert(operand, operand_id);
          break;
        }
        case webnn::mojom::blink::Operand::Kind::kOutput:
          // Because the operators are visited in topological order, if this
          // operand is an intermediate operand, it should already be defined as
          // an output operand of the dependent operator.
          NOTREACHED();
      }
    }
    bool is_redundant_reshape = false;
    for (const auto& operand : current_operator->Outputs()) {
      if (operand_to_id_map.Contains(operand.Get())) {
        // The `mojo::Operand` is already converted with the MLOperand, skip it.
        continue;
      }

      if (reshaped_to_constant_mapping.Contains(operand)) {
        is_redundant_reshape = true;
        // Point redundant reshape's output operand to its corresponding
        // constant operand.
        operand_to_id_map.insert(
            operand,
            operand_to_id_map.at(reshaped_to_constant_mapping.at(operand)));
        continue;
      }
      // Because the graph's output operands are already converted before, this
      // operand should be an intermediate operand that connects with two
      // operators. Create `mojo::Operand` for this operand.
      uint64_t operand_id = NextOperandId(*graph_info);
      graph_info->id_to_operand_map.insert(
          operand_id,
          mojo::ConvertTo<webnn::mojom::blink::OperandPtr>(operand.Get()));
      operand_to_id_map.insert(operand, operand_id);
    }
    if (is_redundant_reshape) {
      continue;
    }
    // Create `mojo::Operation` with the id of the input and output operands.
    std::optional<String> error =
        SerializeMojoOperation(operand_to_id_map, context_properties,
                               current_operator.Get(), graph_info.get());
    if (error.has_value()) {
      // Return here if the operator is not implemented.
      return base::unexpected(*error);
    }
  }

  return graph_info;
}

}  // namespace

// static
MLGraphBuilder* MLGraphBuilder::Create(ScriptState* script_state,
                                       MLContext* context,
                                       ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return nullptr;
  }

  return context->CreateWebNNGraphBuilder(script_state, exception_state);
}

MLGraphBuilder::MLGraphBuilder(
    ExecutionContext* execution_context,
    MLContext* context,
    mojo::PendingAssociatedRemote<webnn::mojom::blink::WebNNGraphBuilder>
        pending_remote)
    : ml_context_(context), remote_(execution_context) {
  CHECK(base::FeatureList::IsEnabled(
      webnn::mojom::features::kWebMachineLearningNeuralNetwork));

  remote_.Bind(std::move(pending_remote),
               execution_context->GetTaskRunner(TaskType::kMachineLearning));
  remote_.set_disconnect_handler(WTF::BindOnce(
      &MLGraphBuilder::OnConnectionError, WrapWeakPersistent(this)));
}

MLGraphBuilder::~MLGraphBuilder() = default;

void MLGraphBuilder::Trace(Visitor* visitor) const {
  visitor->Trace(ml_context_);
  visitor->Trace(remote_);
  visitor->Trace(constant_operands_);
  visitor->Trace(pending_resolver_);
  ScriptWrappable::Trace(visitor);
}

MLContext* MLGraphBuilder::GetContext() const {
  return ml_context_.Get();
}

MLOperand* MLGraphBuilder::input(ScriptState* script_state,
                                 String name,
                                 const MLOperandDescriptor* desc,
                                 ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      Vector<uint32_t> shape, GetShapeFromDescriptor(script_state, *desc));

  auto input_operand = MLOperand::ValidateAndCreateInput(
      this, desc->dataType().AsEnum(), std::move(shape), std::move(name));
  if (!input_operand.has_value()) {
    exception_state.ThrowTypeError(input_operand.error());
    return nullptr;
  }

  if (!ml_context_->GetProperties().data_type_limits.input.Has(
          input_operand.value()->DataType())) {
    exception_state.ThrowTypeError(String(webnn::NotSupportedInputTypeError(
        input_operand.value()->Name().Utf8(), input_operand.value()->DataType(),
        ml_context_->GetProperties().data_type_limits.input)));
    return nullptr;
  }

  return input_operand.value();
}

MLOperand* MLGraphBuilder::constant(ScriptState* script_state,
                                    const MLOperandDescriptor* desc,
                                    NotShared<DOMArrayBufferView> buffer_view,
                                    ExceptionState& exception_state) {
  CHECK(buffer_view);

  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      Vector<uint32_t> shape, GetShapeFromDescriptor(script_state, *desc));

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor descriptor,
      webnn::OperandDescriptor::Create(
          FromBlinkDataType(desc->dataType().AsEnum()), shape));

  if (GetArrayBufferViewType(descriptor.data_type()) !=
      buffer_view->GetType()) {
    exception_state.ThrowTypeError(
        "The buffer view type doesn't match the operand data type.");
    return nullptr;
  }

  if (descriptor.PackedByteLength() != buffer_view->byteLength()) {
    exception_state.ThrowTypeError(String::Format(
        "The buffer view byte length (%zu) doesn't match the "
        "expected byte length (%zu).",
        buffer_view->byteLength(), descriptor.PackedByteLength()));
    return nullptr;
  }

  if (!ml_context_->GetProperties().data_type_limits.constant.Has(
          descriptor.data_type())) {
    exception_state.ThrowTypeError(String(webnn::NotSupportedConstantTypeError(
        descriptor.data_type(),
        ml_context_->GetProperties().data_type_limits.constant)));
    return nullptr;
  }

  auto* constant_operand = MakeGarbageCollected<MLConstantOperand>(this, std::move(descriptor),
                                                 buffer_view->ByteSpan());
  constant_operands_.push_back(constant_operand);
  return constant_operand;
}

MLOperand* MLGraphBuilder::argMin(const MLOperand* input,
                                  const uint32_t axis,
                                  const MLArgMinMaxOptions* options,
                                  ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);
  return BuildArgMinMax(this, webnn::mojom::blink::ArgMinMax::Kind::kMin, input,
                        axis, options, exception_state);
}

MLOperand* MLGraphBuilder::argMax(const MLOperand* input,
                                  const uint32_t axis,
                                  const MLArgMinMaxOptions* options,
                                  ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);
  return BuildArgMinMax(this, webnn::mojom::blink::ArgMinMax::Kind::kMax, input,
                        axis, options, exception_state);
}

MLOperand* MLGraphBuilder::batchNormalization(
    const MLOperand* input,
    const MLOperand* mean,
    const MLOperand* variance,
    const MLBatchNormalizationOptions* options,
    ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  HeapVector<Member<const MLOperand>> inputs = {input, mean, variance};
  // Adding the optional operands into inputs ensures the graph traversal
  // algorithm GetOperatorsInTopologicalOrder() works. For backends, the
  // optional operands should be retrieved from the options instead of inputs.
  if (options->hasScale()) {
    inputs.push_back(options->scale());
  }
  if (options->hasBias()) {
    inputs.push_back(options->bias());
  }
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateBatchNormalizationAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(), mean->Descriptor(),
          variance->Descriptor(),
          ConvertToBatchNormalizationAttributes(options)));

  // Create batchNormalization operator and its output operand. Connect the
  // batchNormalization operator to its input and output operands.
  auto* batch_normalization = MakeGarbageCollected<MLOperator>(
      this, webnn::mojom::blink::Operation::Tag::kBatchNormalization, options);
  MLOperand* output = MLOperand::CreateOutput(
      this, std::move(output_descriptor), batch_normalization);
  batch_normalization->Connect(std::move(inputs), {output});
  return output;
}

MLOperand* MLGraphBuilder::concat(const HeapVector<Member<MLOperand>>& inputs,
                                  const uint32_t axis,
                                  const MLOperatorOptions* options,
                                  ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(
      ValidateInputs(static_cast<HeapVector<Member<const MLOperand>>>(inputs)),
      nullptr);

  std::vector<webnn::OperandDescriptor> input_component_operands;
  input_component_operands.reserve(inputs.size());
  base::ranges::transform(
      inputs, std::back_inserter(input_component_operands),
      [](const auto& input) { return input->Descriptor(); });

  const std::string label = options->label().Utf8();
  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateConcatAndInferOutput(
          ml_context_->GetProperties(), input_component_operands, axis, label));

  auto* concat = MakeGarbageCollected<MLConcatOperator>(this, axis, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), concat);

  concat->Connect(static_cast<HeapVector<Member<const MLOperand>>>(inputs),
                  {output});
  return output;
}

MLOperand* MLGraphBuilder::clamp(const MLOperand* input,
                                 const MLClampOptions* options,
                                 ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  if (!ValidateClampOptions(options, exception_state)) {
    return nullptr;
  }

  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-clamp, the output tensor of
  // clamp has the same data type and dimensions as its input.
  return BuildUnaryOperator(
      this, exception_state, webnn::mojom::blink::Operation::Tag::kClamp,
      ml_context_->GetProperties().data_type_limits.clamp_input, input,
      options);
}

MLOperand* MLGraphBuilder::conv2d(const MLOperand* input,
                                  const MLOperand* filter,
                                  const MLConv2dOptions* options,
                                  ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  HeapVector<Member<const MLOperand>> inputs = {input, filter};
  if (options->hasBias()) {
    inputs.push_back(options->bias());
  }
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  auto conv2d_attributes = ConvertToConv2dAttributes(options);
  if (!conv2d_attributes.has_value()) {
    exception_state.ThrowTypeError(conv2d_attributes.error());
    return nullptr;
  }

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateConv2dAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(),
          filter->Descriptor(), std::move(conv2d_attributes.value())));

  // Create conv2d operator and its output operand. Connect the conv2d operator
  // to its input and output operands.
  auto* conv2d = MakeGarbageCollected<MLOperator>(
      this, webnn::mojom::blink::Operation::Tag::kConv2d, options,
      /*sub_type=*/webnn::mojom::blink::Conv2d::Kind::kDirect);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), conv2d);
  conv2d->Connect(std::move(inputs), {output});
  return output;
}

MLOperand* MLGraphBuilder::convTranspose2d(
    const MLOperand* input,
    const MLOperand* filter,
    const MLConvTranspose2dOptions* options,
    ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  HeapVector<Member<const MLOperand>> inputs = {input, filter};
  if (options->hasBias()) {
    inputs.push_back(options->bias());
  }
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  auto convTranspose2d_attributes = ConvertToConvTranspose2dAttributes(options);
  if (!convTranspose2d_attributes.has_value()) {
    exception_state.ThrowTypeError(convTranspose2d_attributes.error());
    return nullptr;
  }

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateConvTranspose2dAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(),
          filter->Descriptor(), std::move(convTranspose2d_attributes.value())));

  // Create convTranspose2d operator and its output operand. Connect the
  // convTranspose2d operator to its input and output operands.
  auto* convTranspose2d = MakeGarbageCollected<MLOperator>(
      this, webnn::mojom::blink::Operation::Tag::kConv2d, options,
      /*sub_type=*/webnn::mojom::blink::Conv2d::Kind::kTransposed);
  MLOperand* output = MLOperand::CreateOutput(
      this, std::move(output_descriptor), convTranspose2d);
  convTranspose2d->Connect(std::move(inputs), {output});
  return output;
}

MLOperand* MLGraphBuilder::cumulativeSum(const MLOperand* input,
                                         const uint32_t axis,
                                         const MLCumulativeSumOptions* options,
                                         ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateCumulativeSumAndInferOutput(ml_context_->GetProperties(),
                                                 input->Descriptor(), axis,
                                                 options->label().Utf8()));

  // Create cumulativeSum operator and its output operand. Connect the
  // cumulativeSum operator to its input and output operands.
  auto* cumulativeSum =
      MakeGarbageCollected<MLCumulativeSumOperator>(this, axis, options);
  MLOperand* output = MLOperand::CreateOutput(
      this, std::move(output_descriptor), cumulativeSum);
  cumulativeSum->Connect({input}, {output});

  return output;
}

// Macro to define the function for an elementwise binary op. `op_camel` is the
// name of the op in camel case. `op_snake` is the name of the op in snake case.
// `op_kind` is the corresponding `ElementWiseBinary::Kind` enum. We need to
// separately specify the camel case and the snake case name because the
// function name is in camel case, while the corresponding `DataTypeLimits`
// field is in snake case.
#define BUILD_ELEMENTWISE_BINARY_OP(op_camel, op_snake, op_kind)              \
  MLOperand* MLGraphBuilder::op_camel(const MLOperand* a, const MLOperand* b, \
                                      const MLOperatorOptions* options,       \
                                      ExceptionState& exception_state) {      \
    THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);          \
    THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs({a, b}), nullptr);          \
    return BuildElementWiseBinary(                                            \
        this, webnn::mojom::blink::ElementWiseBinary::Kind::op_kind,          \
        ml_context_->GetProperties().data_type_limits.op_snake##_input, a, b, \
        options, exception_state);                                            \
  }

BUILD_ELEMENTWISE_BINARY_OP(add, add, kAdd)
BUILD_ELEMENTWISE_BINARY_OP(sub, sub, kSub)
BUILD_ELEMENTWISE_BINARY_OP(mul, mul, kMul)
BUILD_ELEMENTWISE_BINARY_OP(div, div, kDiv)
BUILD_ELEMENTWISE_BINARY_OP(min, min, kMin)
BUILD_ELEMENTWISE_BINARY_OP(max, max, kMax)
BUILD_ELEMENTWISE_BINARY_OP(pow, pow, kPow)
BUILD_ELEMENTWISE_BINARY_OP(equal, equal, kEqual)
BUILD_ELEMENTWISE_BINARY_OP(greater, greater, kGreater)
BUILD_ELEMENTWISE_BINARY_OP(lesser, lesser, kLesser)
BUILD_ELEMENTWISE_BINARY_OP(greaterOrEqual, greater_or_equal, kGreaterOrEqual)
BUILD_ELEMENTWISE_BINARY_OP(lesserOrEqual, lesser_or_equal, kLesserOrEqual)
BUILD_ELEMENTWISE_BINARY_OP(logicalAnd, logical_and, kLogicalAnd)
BUILD_ELEMENTWISE_BINARY_OP(logicalOr, logical_or, kLogicalOr)
BUILD_ELEMENTWISE_BINARY_OP(logicalXor, logical_xor, kLogicalXor)

#define BUILD_ELEMENTWISE_UNARY_OP(op, op_kind)                          \
  MLOperand* MLGraphBuilder::op(const MLOperand* input,                  \
                                const MLOperatorOptions* options,        \
                                ExceptionState& exception_state) {       \
    THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);     \
    THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);       \
    return BuildElementWiseUnaryOperator(                                \
        this, exception_state,                                           \
        webnn::mojom::blink::ElementWiseUnary::Kind::op_kind,            \
        ml_context_->GetProperties().data_type_limits.op##_input, input, \
        options);                                                        \
  }

BUILD_ELEMENTWISE_UNARY_OP(abs, kAbs)
BUILD_ELEMENTWISE_UNARY_OP(ceil, kCeil)
BUILD_ELEMENTWISE_UNARY_OP(cos, kCos)
BUILD_ELEMENTWISE_UNARY_OP(exp, kExp)
BUILD_ELEMENTWISE_UNARY_OP(floor, kFloor)
BUILD_ELEMENTWISE_UNARY_OP(log, kLog)
BUILD_ELEMENTWISE_UNARY_OP(neg, kNeg)
BUILD_ELEMENTWISE_UNARY_OP(sign, kSign)
BUILD_ELEMENTWISE_UNARY_OP(sin, kSin)
BUILD_ELEMENTWISE_UNARY_OP(tan, kTan)
BUILD_ELEMENTWISE_UNARY_OP(erf, kErf)
BUILD_ELEMENTWISE_UNARY_OP(identity, kIdentity)
BUILD_ELEMENTWISE_UNARY_OP(reciprocal, kReciprocal)
BUILD_ELEMENTWISE_UNARY_OP(sqrt, kSqrt)

MLOperand* MLGraphBuilder::logicalNot(const MLOperand* input,
                                      const MLOperatorOptions* options,
                                      ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);
  return BuildElementWiseUnaryOperator(
      this, exception_state,
      webnn::mojom::blink::ElementWiseUnary::Kind::kLogicalNot,
      ml_context_->GetProperties().data_type_limits.logical_not_input, input,
      options);
}

MLOperand* MLGraphBuilder::cast(const MLOperand* input,
                                const V8MLOperandDataType output_data_type,
                                const MLOperatorOptions* options,
                                ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  const std::string label = options->label().Utf8();

  if (!ml_context_->GetProperties().data_type_limits.cast_input.Has(
          input->DataType())) {
    exception_state.ThrowTypeError(
        String::FromUTF8(webnn::GetErrorLabelPrefix(label)) +
        String(NotSupportedInputArgumentTypeError(
            input->DataType(),
            ml_context_->GetProperties().data_type_limits.cast_input)));
    return nullptr;
  }

  const webnn::OperandDataType cast_data_type =
      FromBlinkDataType(output_data_type.AsEnum());

  if (!ml_context_->GetProperties().data_type_limits.cast_input.Has(
          cast_data_type)) {
    exception_state.ThrowTypeError(
        String::FromUTF8(webnn::GetErrorLabelPrefix(label)) +
        String(NotSupportedOpOutputTypeError(
            cast_data_type,
            ml_context_->GetProperties().data_type_limits.cast_input)));
    return nullptr;
  }

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::OperandDescriptor::Create(cast_data_type, input->Shape()));

  auto* cast = MakeGarbageCollected<MLOperator>(
      this, webnn::mojom::blink::Operation::Tag::kElementWiseUnary, options,
      /*sub_kind=*/webnn::mojom::blink::ElementWiseUnary::Kind::kCast);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), cast);

  cast->Connect({input}, {output});
  return output;
}

MLOperand* MLGraphBuilder::dequantizeLinear(const MLOperand* input,
                                            const MLOperand* scale,
                                            const MLOperand* zeroPoint,
                                            const MLOperatorOptions* options,
                                            ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  HeapVector<Member<const MLOperand>> inputs = {input, scale, zeroPoint};
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateDequantizeLinearAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(),
          scale->Descriptor(), zeroPoint->Descriptor(),
          options->label().Utf8()));

  auto* dequantize_linear = MakeGarbageCollected<MLOperator>(
      this, webnn::mojom::blink::Operation::Tag::kDequantizeLinear, options);
  MLOperand* output = MLOperand::CreateOutput(
      this, std::move(output_descriptor), dequantize_linear);
  dequantize_linear->Connect(std::move(inputs), {output});
  return output;
}

#define BUILD_REDUCE_OP(op, op_kind)                                     \
  MLOperand* MLGraphBuilder::op(const MLOperand* input,                  \
                                const MLReduceOptions* options,          \
                                ExceptionState& exception_state) {       \
    THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);     \
    THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);       \
    return BuildReduce(this, webnn::mojom::blink::Reduce::Kind::op_kind, \
                       ml_context_->GetProperties(), input, options,     \
                       exception_state);                                 \
  }

BUILD_REDUCE_OP(reduceL1, kL1)
BUILD_REDUCE_OP(reduceL2, kL2)
BUILD_REDUCE_OP(reduceLogSum, kLogSum)
BUILD_REDUCE_OP(reduceLogSumExp, kLogSumExp)
BUILD_REDUCE_OP(reduceMax, kMax)
BUILD_REDUCE_OP(reduceMean, kMean)
BUILD_REDUCE_OP(reduceMin, kMin)
BUILD_REDUCE_OP(reduceProduct, kProduct)
BUILD_REDUCE_OP(reduceSum, kSum)
BUILD_REDUCE_OP(reduceSumSquare, kSumSquare)

MLOperand* MLGraphBuilder::elu(const MLOperand* input,
                               const MLEluOptions* options,
                               ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);
  const std::string label = options->label().Utf8();
  // The current spec doesn't restrict the value of alpha. An issue has been
  // filed to track it: https://github.com/webmachinelearning/webnn/issues/383
  if (options->alpha() <= 0.0f) {
    exception_state.ThrowTypeError(
        String::FromUTF8(webnn::GetErrorLabelPrefix(label)) +
        "The value of alpha must be greater than 0.");
    return nullptr;
  }

  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-elu, the output tensor of
  // elu has the same data type and dimensions as its input.
  return BuildUnaryOperator(
      this, exception_state, webnn::mojom::blink::Operation::Tag::kElu,
      ml_context_->GetProperties().data_type_limits.elu_input, input, options);
}

MLOperand* MLGraphBuilder::expand(const MLOperand* input,
                                  const Vector<uint32_t>& new_shape,
                                  const MLOperatorOptions* options,
                                  ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  const std::string label = options->label().Utf8();

  const webnn::SupportedDataTypes& data_type_constraint =
      ml_context_->GetProperties().data_type_limits.expand_input;
  if (!data_type_constraint.Has(input->DataType())) {
    exception_state.ThrowTypeError(
        String::FromUTF8(webnn::GetErrorLabelPrefix(options->label().Utf8())) +
        String(NotSupportedInputArgumentTypeError(input->DataType(),
                                                  data_type_constraint)));
    return nullptr;
  }

  auto output_shape = webnn::BroadcastShapes(input->Shape(), new_shape,
                                             /*bidirectional=*/false);
  if (!output_shape) {
    exception_state.ThrowTypeError(
        String::FromUTF8(webnn::GetErrorLabelPrefix(label)) +
        "The input shape is not broadcastable to the new shape.");
    return nullptr;
  }
  CHECK(base::ranges::equal(*output_shape, new_shape));

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::OperandDescriptor::Create(input->DataType(), *output_shape));

  auto* expand = MakeGarbageCollected<MLOperator>(
      this, webnn::mojom::blink::Operation::Tag::kExpand, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), expand);

  expand->Connect({input}, {output});
  return output;
}

MLOperand* MLGraphBuilder::gather(const MLOperand* input,
                                  const MLOperand* indices,
                                  const MLGatherOptions* options,
                                  ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  HeapVector<Member<const MLOperand>> inputs = {input, indices};
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateGatherAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(),
          indices->Descriptor(), options->axis(), options->label().Utf8()));

  auto* gather = MakeGarbageCollected<MLOperator>(
      this, webnn::mojom::blink::Operation::Tag::kGather, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), gather);

  gather->Connect(std::move(inputs), {output});
  return output;
}

MLOperand* MLGraphBuilder::gatherElements(const MLOperand* input,
                                          const MLOperand* indices,
                                          const MLGatherOptions* options,
                                          ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  HeapVector<Member<const MLOperand>> inputs = {input, indices};
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateGatherElementsAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(),
          indices->Descriptor(), options->axis(), options->label().Utf8()));

  auto* gather_elements = MakeGarbageCollected<MLOperator>(
      this, webnn::mojom::blink::Operation::Tag::kGatherElements, options);
  MLOperand* output = MLOperand::CreateOutput(
      this, std::move(output_descriptor), gather_elements);

  gather_elements->Connect(std::move(inputs), {output});
  return output;
}

MLOperand* MLGraphBuilder::gatherND(const MLOperand* input,
                                    const MLOperand* indices,
                                    const MLOperatorOptions* options,
                                    ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  HeapVector<Member<const MLOperand>> inputs = {input, indices};
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateGatherNDAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(),
          indices->Descriptor(), options->label().Utf8()));

  auto* gather_nd = MakeGarbageCollected<MLOperator>(
      this, webnn::mojom::blink::Operation::Tag::kGatherNd, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), gather_nd);

  gather_nd->Connect(std::move(inputs), {output});
  return output;
}

MLOperand* MLGraphBuilder::gelu(const MLOperand* input,
                                const MLOperatorOptions* options,
                                ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-gelu, the output tensor of
  // gelu has the same data type and dimensions as its input. And the input data
  // type must be one of the floating point types.
  return BuildUnaryOperator(
      this, exception_state, webnn::mojom::blink::Operation::Tag::kGelu,
      ml_context_->GetProperties().data_type_limits.gelu_input, input, options);
}

MLOperand* MLGraphBuilder::gemm(const MLOperand* a,
                                const MLOperand* b,
                                const MLGemmOptions* options,
                                ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  HeapVector<Member<const MLOperand>> inputs = {a, b};
  if (options->hasC()) {
    inputs.push_back(options->c());
  }
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateGemmAndInferOutput(ml_context_->GetProperties(),
                                        a->Descriptor(), b->Descriptor(),
                                        ConvertToGemmAttributes(options)));

  auto* gemm = MakeGarbageCollected<MLOperator>(
      this, webnn::mojom::blink::Operation::Tag::kGemm, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), gemm);

  gemm->Connect(std::move(inputs), {output});
  return output;
}

HeapVector<Member<const MLOperand>> MLGraphBuilder::gru(
    const MLOperand* input,
    const MLOperand* weight,
    const MLOperand* recurrent_weight,
    const uint32_t steps,
    const uint32_t hidden_size,
    MLGruOptions* options,
    ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(),
                            HeapVector<Member<const MLOperand>>());

  HeapVector<Member<const MLOperand>> inputs = {input, weight,
                                                recurrent_weight};
  if (options->hasBias()) {
    inputs.push_back(options->bias());
  }
  if (options->hasRecurrentBias()) {
    inputs.push_back(options->recurrentBias());
  }
  if (options->hasInitialHiddenState()) {
    inputs.push_back(options->initialHiddenState());
  }
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs),
                                 HeapVector<Member<const MLOperand>>());

  auto validated_outputs = webnn::ValidateGruAndInferOutput(
      ml_context_->GetProperties(), input->Descriptor(), weight->Descriptor(),
      recurrent_weight->Descriptor(), steps, hidden_size,
      ConvertToGruAttributes(this, options));
  if (!validated_outputs.has_value()) {
    exception_state.ThrowTypeError(String::FromUTF8(validated_outputs.error()));
    return {};
  }
  auto* gru =
      MakeGarbageCollected<MLGruOperator>(this, steps, hidden_size, options);

  HeapVector<Member<const MLOperand>> outputs;
  for (const auto& validated_output : validated_outputs.value()) {
    outputs.push_back(MLOperand::CreateOutput(this, validated_output, gru));
  }

  gru->Connect(std::move(inputs), outputs);
  return outputs;
}

MLOperand* MLGraphBuilder::gruCell(const MLOperand* input,
                                   const MLOperand* weight,
                                   const MLOperand* recurrent_weight,
                                   const MLOperand* hidden_state,
                                   const uint32_t hidden_size,
                                   MLGruCellOptions* options,
                                   ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  HeapVector<Member<const MLOperand>> inputs = {input, weight, recurrent_weight,
                                                hidden_state};
  if (options->hasBias()) {
    inputs.push_back(options->bias());
  }
  if (options->hasRecurrentBias()) {
    inputs.push_back(options->recurrentBias());
  }
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  auto validated_output = webnn::ValidateGruCellAndInferOutput(
      ml_context_->GetProperties(), input->Descriptor(), weight->Descriptor(),
      recurrent_weight->Descriptor(), hidden_state->Descriptor(), hidden_size,
      ConvertToGruCellAttributes(this, options));
  if (!validated_output.has_value()) {
    exception_state.ThrowTypeError(String::FromUTF8(validated_output.error()));
    return {};
  }
  auto* gru_cell =
      MakeGarbageCollected<MLGruCellOperator>(this, hidden_size, options);

  MLOperand* output =
      MLOperand::CreateOutput(this, *std::move(validated_output), gru_cell);

  gru_cell->Connect(std::move(inputs), {output});
  return output;
}

MLOperand* MLGraphBuilder::hardSigmoid(const MLOperand* input,
                                       const MLHardSigmoidOptions* options,
                                       ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-hardsigmoid, the output
  // tensor of softplus has the same type and dimensions as its input.
  return BuildUnaryOperator(
      this, exception_state, webnn::mojom::blink::Operation::Tag::kHardSigmoid,
      ml_context_->GetProperties().data_type_limits.hard_sigmoid_input, input,
      options);
}

MLOperand* MLGraphBuilder::hardSwish(const MLOperand* input,
                                     const MLOperatorOptions* options,
                                     ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-hard-swish, the output
  // tensor of hard-swish has the same data type and dimensions as its input.
  return BuildUnaryOperator(
      this, exception_state, webnn::mojom::blink::Operation::Tag::kHardSwish,
      ml_context_->GetProperties().data_type_limits.hard_swish_input, input,
      options);
}

MLOperand* MLGraphBuilder::instanceNormalization(
    const MLOperand* input,
    const MLInstanceNormalizationOptions* options,
    ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  HeapVector<Member<const MLOperand>> inputs = {input};
  // Adding the optional operands into inputs ensures the graph traversal
  // algorithm GetOperatorsInTopologicalOrder() works. For backends, the
  // optional operands should be retrieved from the options instead of inputs.
  if (options->hasScale()) {
    inputs.push_back(options->scale());
  }
  if (options->hasBias()) {
    inputs.push_back(options->bias());
  }
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateInstanceNormalizationAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(),
          ConvertToInstanceNormalizationAttributes(options)));

  auto* instance_normalization = MakeGarbageCollected<MLOperator>(
      this, webnn::mojom::blink::Operation::Tag::kInstanceNormalization,
      options);

  MLOperand* output = MLOperand::CreateOutput(
      this, std::move(output_descriptor), instance_normalization);

  instance_normalization->Connect(std::move(inputs), {output});
  return output;
}

MLOperand* MLGraphBuilder::layerNormalization(
    const MLOperand* input,
    const MLLayerNormalizationOptions* options,
    ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  HeapVector<Member<const MLOperand>> inputs = {input};
  // Adding the optional operands into inputs ensures the graph traversal
  // algorithm GetOperatorsInTopologicalOrder() works. For backends, the
  // optional operands should be retrieved from the options instead of inputs.
  if (options->hasScale()) {
    inputs.push_back(options->scale());
  }
  if (options->hasBias()) {
    inputs.push_back(options->bias());
  }
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  // TODO(crbug.com/1273291): Figure out whether the `axes` should be required,
  // tracked by issue: https://github.com/webmachinelearning/webnn/issues/487
  const Vector<uint32_t> axes =
      options->getAxesOr(CreateLayerNormalizationDefaultAxes(input->Rank()));

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateLayerNormalizationAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(), axes,
          ConvertToLayerNormalizationAttributes(options)));

  auto* layer_normalization = MakeGarbageCollected<MLOperator>(
      this, webnn::mojom::blink::Operation::Tag::kLayerNormalization, options);

  MLOperand* output = MLOperand::CreateOutput(
      this, std::move(output_descriptor), layer_normalization);

  layer_normalization->Connect(std::move(inputs), {output});
  return output;
}

MLOperand* MLGraphBuilder::leakyRelu(const MLOperand* input,
                                     const MLLeakyReluOptions* options,
                                     ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-leakyrelu, the output
  // tensor of leaky relu has the same type and dimensions as its input.
  return BuildUnaryOperator(
      this, exception_state, webnn::mojom::blink::Operation::Tag::kLeakyRelu,
      ml_context_->GetProperties().data_type_limits.leaky_relu_input, input,
      options);
}

MLOperand* MLGraphBuilder::linear(const MLOperand* input,
                                  const MLLinearOptions* options,
                                  ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  // The current spec doesn't specify the operand data type constraints of
  // linear. An issue has been filed to track it:
  // https://github.com/webmachinelearning/webnn/issues/283.
  //
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-linear, the output tensor
  // of linear has the same type and dimensions as its input.
  return BuildUnaryOperator(
      this, exception_state, webnn::mojom::blink::Operation::Tag::kLinear,
      ml_context_->GetProperties().data_type_limits.linear_input, input,
      options);
}

HeapVector<Member<const MLOperand>> MLGraphBuilder::lstm(
    const MLOperand* input,
    const MLOperand* weight,
    const MLOperand* recurrent_weight,
    const uint32_t steps,
    const uint32_t hidden_size,
    MLLstmOptions* options,
    ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(),
                            HeapVector<Member<const MLOperand>>());

  HeapVector<Member<const MLOperand>> inputs = {input, weight,
                                                recurrent_weight};
  if (options->hasBias()) {
    inputs.push_back(options->bias());
  }
  if (options->hasRecurrentBias()) {
    inputs.push_back(options->recurrentBias());
  }
  if (options->hasPeepholeWeight()) {
    inputs.push_back(options->peepholeWeight());
  }
  if (options->hasInitialHiddenState()) {
    inputs.push_back(options->initialHiddenState());
  }
  if (options->hasInitialCellState()) {
    inputs.push_back(options->initialCellState());
  }
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs),
                                 HeapVector<Member<const MLOperand>>());

  if (!options->hasActivations()) {
    // Create a default activation sequence as defined in the spec.
    options->setActivations(
        {V8MLRecurrentNetworkActivation(
             V8MLRecurrentNetworkActivation::Enum::kSigmoid),
         V8MLRecurrentNetworkActivation(
             V8MLRecurrentNetworkActivation::Enum::kTanh),
         V8MLRecurrentNetworkActivation(
             V8MLRecurrentNetworkActivation::Enum::kTanh)});
  }

  auto validated_outputs = webnn::ValidateLstmAndInferOutput(
      ml_context_->GetProperties(), input->Descriptor(), weight->Descriptor(),
      recurrent_weight->Descriptor(), steps, hidden_size,
      ConvertToLstmAttributes(options));
  if (!validated_outputs.has_value()) {
    exception_state.ThrowTypeError(String::FromUTF8(validated_outputs.error()));
    return {};
  }

  auto* lstm =
      MakeGarbageCollected<MLLstmOperator>(this, steps, hidden_size, options);

  HeapVector<Member<const MLOperand>> outputs;
  for (const auto& validated_output : validated_outputs.value()) {
    outputs.push_back(MLOperand::CreateOutput(this, validated_output, lstm));
  }

  lstm->Connect(std::move(inputs), outputs);
  return outputs;
}

HeapVector<Member<const MLOperand>> MLGraphBuilder::lstmCell(
    const MLOperand* input,
    const MLOperand* weight,
    const MLOperand* recurrent_weight,
    const MLOperand* hidden_state,
    const MLOperand* cell_state,
    const uint32_t hidden_size,
    MLLstmCellOptions* options,
    ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(),
                            HeapVector<Member<const MLOperand>>());

  HeapVector<Member<const MLOperand>> inputs = {input, weight, recurrent_weight,
                                                hidden_state, cell_state};
  if (options->hasBias()) {
    inputs.push_back(options->bias());
  }
  if (options->hasRecurrentBias()) {
    inputs.push_back(options->recurrentBias());
  }
  if (options->hasPeepholeWeight()) {
    inputs.push_back(options->peepholeWeight());
  }
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs),
                                 HeapVector<Member<const MLOperand>>());

  if (!options->hasActivations()) {
    // Create a default activation sequence as defined in the spec.
    options->setActivations(
        {V8MLRecurrentNetworkActivation(
             V8MLRecurrentNetworkActivation::Enum::kSigmoid),
         V8MLRecurrentNetworkActivation(
             V8MLRecurrentNetworkActivation::Enum::kTanh),
         V8MLRecurrentNetworkActivation(
             V8MLRecurrentNetworkActivation::Enum::kTanh)});
  }

  auto validated_outputs = webnn::ValidateLstmCellAndInferOutput(
      ml_context_->GetProperties(), input->Descriptor(), weight->Descriptor(),
      recurrent_weight->Descriptor(), hidden_state->Descriptor(),
      cell_state->Descriptor(), hidden_size,
      ConvertToLstmCellAttributes(options));
  if (!validated_outputs.has_value()) {
    exception_state.ThrowTypeError(String::FromUTF8(validated_outputs.error()));
    return {};
  }

  auto* lstm_cell =
      MakeGarbageCollected<MLLstmCellOperator>(this, hidden_size, options);

  HeapVector<Member<const MLOperand>> outputs;
  CHECK_EQ(validated_outputs->size(), 2u);
  outputs.reserve(2);
  for (const auto& validated_output : validated_outputs.value()) {
    outputs.push_back(
        MLOperand::CreateOutput(this, validated_output, lstm_cell));
  }

  lstm_cell->Connect(std::move(inputs), outputs);
  return outputs;
}

MLOperand* MLGraphBuilder::matmul(const MLOperand* a,
                                  const MLOperand* b,
                                  const MLOperatorOptions* options,
                                  ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  HeapVector<Member<const MLOperand>> inputs = {a, b};
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateMatmulAndInferOutput(ml_context_->GetProperties(),
                                          a->Descriptor(), b->Descriptor(),
                                          options->label().Utf8()));

  // Create matmul operator and its output operand. Connect the matmul operator
  // to its input and output operands.
  auto* matmul = MakeGarbageCollected<MLOperator>(
      this, webnn::mojom::blink::Operation::Tag::kMatmul, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), matmul);

  matmul->Connect(std::move(inputs), {output});
  return output;
}

MLOperand* MLGraphBuilder::pad(ScriptState* script_state,
                               const MLOperand* input,
                               const Vector<uint32_t>& beginning_padding,
                               const Vector<uint32_t>& ending_padding,
                               const MLPadOptions* options,
                               ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  const std::string label = options->label().Utf8();
  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidatePadAndInferOutput(ml_context_->GetProperties(),
                                       input->Descriptor(), beginning_padding,
                                       ending_padding, label));

  if (options->mode().AsEnum() != V8MLPaddingMode::Enum::kConstant &&
      fabs(options->value() - 0.0f) > std::numeric_limits<float>::epsilon()) {
    LogConsoleWarning(
        script_state,
        String::FromUTF8(webnn::GetErrorLabelPrefix(label)) +
            String::Format(
                "The pad value is ignored unless the options.mode is set to "
                "constant."));
  }

  auto* pad = MakeGarbageCollected<MLPadOperator>(this, beginning_padding,
                                                  ending_padding, options);
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-pad, the output
  // tensor of pad has the same data type as its input.
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), pad);

  pad->Connect({input}, {output});
  return output;
}

MLOperand* MLGraphBuilder::averagePool2d(const MLOperand* input,
                                         const MLPool2dOptions* options,
                                         ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  const std::string label = options->label().Utf8();
  if (!(input->DataType() == webnn::OperandDataType::kFloat32 ||
        input->DataType() == webnn::OperandDataType::kFloat16)) {
    exception_state.ThrowTypeError(
        String::FromUTF8(webnn::GetErrorLabelPrefix(label)) +
        "The input data type must be a floating point type.");
    return nullptr;
  }

  return BuildPool2d(this, webnn::mojom::blink::Pool2d::Kind::kAveragePool2d,
                     ml_context_->GetProperties(), input, options,
                     exception_state);
}

MLOperand* MLGraphBuilder::l2Pool2d(const MLOperand* input,
                                    const MLPool2dOptions* options,
                                    ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  const std::string label = options->label().Utf8();
  if (!(input->DataType() == webnn::OperandDataType::kFloat32 ||
        input->DataType() == webnn::OperandDataType::kFloat16)) {
    exception_state.ThrowTypeError(
        String::FromUTF8(webnn::GetErrorLabelPrefix(label)) +
        "The input data type must be a floating point type.");
    return nullptr;
  }

  return BuildPool2d(this, webnn::mojom::blink::Pool2d::Kind::kL2Pool2d,
                     ml_context_->GetProperties(), input, options,
                     exception_state);
}

MLOperand* MLGraphBuilder::maxPool2d(const MLOperand* input,
                                     const MLPool2dOptions* options,
                                     ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  return BuildPool2d(this, webnn::mojom::blink::Pool2d::Kind::kMaxPool2d,
                     ml_context_->GetProperties(), input, options,
                     exception_state);
}

MLOperand* MLGraphBuilder::prelu(const MLOperand* input,
                                 const MLOperand* slope,
                                 const MLOperatorOptions* options,
                                 ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  HeapVector<Member<const MLOperand>> inputs = {input, slope};
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidatePreluAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(),
          slope->Descriptor(), options->label().Utf8()));

  auto* prelu = MakeGarbageCollected<MLOperator>(
      this, webnn::mojom::blink::Operation::Tag::kPrelu, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), prelu);

  prelu->Connect(std::move(inputs), {output});
  return output;
}

MLOperand* MLGraphBuilder::quantizeLinear(const MLOperand* input,
                                          const MLOperand* scale,
                                          const MLOperand* zeroPoint,
                                          const MLOperatorOptions* options,
                                          ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  HeapVector<Member<const MLOperand>> inputs = {input, scale, zeroPoint};
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateQuantizeLinearAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(),
          scale->Descriptor(), zeroPoint->Descriptor(),
          options->label().Utf8()));

  auto* quantize_linear = MakeGarbageCollected<MLOperator>(
      this, webnn::mojom::blink::Operation::Tag::kQuantizeLinear, options);
  MLOperand* output = MLOperand::CreateOutput(
      this, std::move(output_descriptor), quantize_linear);
  quantize_linear->Connect(std::move(inputs), {output});
  return output;
}

MLOperand* MLGraphBuilder::relu(const MLOperand* input,
                                const MLOperatorOptions* options,
                                ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-relu, the output tensor of
  // relu has the same data type and dimensions as its input.
  return BuildUnaryOperator(
      this, exception_state, webnn::mojom::blink::Operation::Tag::kRelu,
      ml_context_->GetProperties().data_type_limits.relu_input, input, options);
}

MLOperand* MLGraphBuilder::reshape(const MLOperand* input,
                                   const Vector<uint32_t>& new_shape,
                                   const MLOperatorOptions* options,
                                   ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  const std::string label = options->label().Utf8();

  if (!ml_context_->GetProperties().data_type_limits.reshape_input.Has(
          input->DataType())) {
    exception_state.ThrowTypeError(
        String::FromUTF8(webnn::GetErrorLabelPrefix(label)) +
        String(NotSupportedInputArgumentTypeError(
            input->DataType(),
            ml_context_->GetProperties().data_type_limits.reshape_input)));
    return nullptr;
  }

  // Setting the initial number of elements to 1 would cover the 0-D scalar with
  // empty dimensions.
  base::CheckedNumeric<size_t> checked_newshape_number_of_elements = 1;
  Vector<uint32_t> output_shape(new_shape.size());
  for (wtf_size_t i = 0; i < new_shape.size(); ++i) {
    auto dim = new_shape[i];
    if (dim == 0) {
      exception_state.ThrowTypeError(
          String::FromUTF8(webnn::GetErrorLabelPrefix(label)) +
          "The value of new shape should not be 0.");
      return nullptr;
    }
    checked_newshape_number_of_elements *= dim;
    output_shape[i] = dim;
  }
  size_t newshape_number_of_elements;
  if (!checked_newshape_number_of_elements.AssignIfValid(
          &newshape_number_of_elements)) {
    exception_state.ThrowTypeError(
        String::FromUTF8(webnn::GetErrorLabelPrefix(label)) +
        "The number of elements implied by new shape is too large.");
    return nullptr;
  }
  DCHECK_NE(newshape_number_of_elements, size_t(0));
  // The number of elements implied by new shape must be the same as the
  // number of elements in the input tensor.
  if (input->NumberOfElements() != newshape_number_of_elements) {
    exception_state.ThrowTypeError(
        String::FromUTF8(webnn::GetErrorLabelPrefix(label)) +
        String::Format(
            "The number of elements (%zu) implied by new shape doesn't match "
            "the number of elements (%zu) in the input tensor.",
            newshape_number_of_elements, input->NumberOfElements()));
    return nullptr;
  }

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::OperandDescriptor::Create(input->DataType(), output_shape));

  auto* reshape = MakeGarbageCollected<MLOperator>(
      this, webnn::mojom::blink::Operation::Tag::kReshape, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), reshape);

  reshape->Connect({input}, {output});
  return output;
}

MLOperand* MLGraphBuilder::resample2d(ScriptState* script_state,
                                      const MLOperand* input,
                                      const MLResample2dOptions* options,
                                      ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  const std::string label = options->label().Utf8();
  absl::variant<base::span<const float>, base::span<const uint32_t>>
      scales_or_sizes;
  Vector<float> default_scales = {1.0, 1.0};
  if (options->hasSizes()) {
    if (options->hasScales()) {
      LogConsoleWarning(script_state,
                        String::FromUTF8(webnn::GetErrorLabelPrefix(label)) +
                            "When sizes and scales are both "
                            "specified, scales argument is "
                            "ignored.");
    }
    scales_or_sizes = options->sizes();
  } else {
    scales_or_sizes = options->hasScales() ? options->scales() : default_scales;
  }

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateResample2dAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(), scales_or_sizes,
          options->getAxesOr({2, 3}), label));

  // Create resample2d operator and its output operand. Connect the resample2d
  // operator to its input and output operands.
  auto* resample2d = MakeGarbageCollected<MLOperator>(
      this, webnn::mojom::blink::Operation::Tag::kResample2d, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), resample2d);

  resample2d->Connect({input}, {output});
  return output;
}

MLOperand* MLGraphBuilder::scatterND(const MLOperand* input,
                                     const MLOperand* indices,
                                     const MLOperand* updates,
                                     const MLOperatorOptions* options,
                                     ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  HeapVector<Member<const MLOperand>> inputs = {input, indices, updates};
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateScatterNDAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(),
          indices->Descriptor(), updates->Descriptor(),
          options->label().Utf8()));

  auto* scatter_nd = MakeGarbageCollected<MLOperator>(
      this, webnn::mojom::blink::Operation::Tag::kScatterNd, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), scatter_nd);

  scatter_nd->Connect(std::move(inputs), {output});
  return output;
}

MLOperand* MLGraphBuilder::sigmoid(const MLOperand* input,
                                   const MLOperatorOptions* options,
                                   ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  // According to WebNN spec
  // https://webmachinelearning.github.io/webnn/#api-mlgraphbuilder-sigmoid, the
  // output tensor of sigmoid has the same data type and dimensions as its
  // input. And the input data type must be one of the floating point types.
  return BuildUnaryOperator(
      this, exception_state, webnn::mojom::blink::Operation::Tag::kSigmoid,
      ml_context_->GetProperties().data_type_limits.sigmoid_input, input,
      options);
}

MLOperand* MLGraphBuilder::slice(const MLOperand* input,
                                 const Vector<uint32_t>& starts,
                                 const Vector<uint32_t>& sizes,
                                 const MLOperatorOptions* options,
                                 ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  webnn::SliceAttributes attributes;
  attributes.sizes.assign(sizes.begin(), sizes.end());
  attributes.starts.assign(starts.begin(), starts.end());
  attributes.label = options->label().Utf8();

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateSliceAndInferOutput(ml_context_->GetProperties(),
                                         input->Descriptor(), attributes));

  auto* slice =
      MakeGarbageCollected<MLSliceOperator>(this, starts, sizes, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), slice);

  slice->Connect({input}, {output});
  return output;
}

MLOperand* MLGraphBuilder::softmax(const MLOperand* input,
                                   uint32_t axis,
                                   const MLOperatorOptions* options,
                                   ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateSoftmaxAndInferOutput(ml_context_->GetProperties(),
                                           input->Descriptor(), axis,
                                           options->label().Utf8()));

  auto* softmax = MakeGarbageCollected<MLSoftmaxOperator>(this, axis, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), softmax);

  softmax->Connect({input}, {output});
  return output;
}

MLOperand* MLGraphBuilder::softmax(const MLOperand* input,
                                   const MLOperatorOptions* options,
                                   ExceptionState& exception_state) {
  // This is to emulate the deprecated 2-D softmax until all Chrome channels
  // support the latest version.
  if (input->Rank() != 2) {
    exception_state.ThrowTypeError(
        String::FromUTF8(webnn::GetErrorLabelPrefix(options->label().Utf8())) +
        "The input must be a 2-D tensor.");
    return nullptr;
  }
  return softmax(input, /*axis=*/1, options, exception_state);
}

MLOperand* MLGraphBuilder::softplus(const MLOperand* input,
                                    const MLOperatorOptions* options,
                                    ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-softplus, the output
  // tensor of softplus has the same type and dimensions as its input.
  return BuildUnaryOperator(
      this, exception_state, webnn::mojom::blink::Operation::Tag::kSoftplus,
      ml_context_->GetProperties().data_type_limits.softplus_input, input,
      options);
}

MLOperand* MLGraphBuilder::softsign(const MLOperand* input,
                                    const MLOperatorOptions* options,
                                    ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-softsign, the output tensor
  // of softsign has the same data type and dimensions as its input.
  return BuildUnaryOperator(
      this, exception_state, webnn::mojom::blink::Operation::Tag::kSoftsign,
      ml_context_->GetProperties().data_type_limits.softsign_input, input,
      options);
}

HeapVector<Member<const MLOperand>> MLGraphBuilder::split(
    const MLOperand* input,
    const uint32_t splits,
    const MLSplitOptions* options,
    ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(),
                            HeapVector<Member<const MLOperand>>());
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input),
                                 HeapVector<Member<const MLOperand>>());

  auto validated_outputs = webnn::ValidateSplitAndInferOutput(
      ml_context_->GetProperties(), input->Descriptor(),
      {.splits = splits,
       .axis = options->axis(),
       .label = options->label().Utf8()});
  if (!validated_outputs.has_value()) {
    exception_state.ThrowTypeError(String::FromUTF8(validated_outputs.error()));
    return {};
  }

  auto* split = MakeGarbageCollected<MLSplitOperator>(this, splits, options);
  HeapVector<Member<const MLOperand>> outputs;
  for (const auto& validated_output : validated_outputs.value()) {
    outputs.push_back(MLOperand::CreateOutput(this, validated_output, split));
  }
  split->Connect({input}, outputs);
  return outputs;
}

HeapVector<Member<const MLOperand>> MLGraphBuilder::split(
    const MLOperand* input,
    const Vector<uint32_t>& splits,
    const MLSplitOptions* options,
    ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(),
                            HeapVector<Member<const MLOperand>>());
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input),
                                 HeapVector<Member<const MLOperand>>());

  auto validated_outputs = webnn::ValidateSplitAndInferOutput(
      ml_context_->GetProperties(), input->Descriptor(),
      {.splits = splits,
       .axis = options->axis(),
       .label = options->label().Utf8()});
  if (!validated_outputs.has_value()) {
    exception_state.ThrowTypeError(String::FromUTF8(validated_outputs.error()));
    return {};
  }

  auto* split = MakeGarbageCollected<MLSplitOperator>(this, splits, options);
  HeapVector<Member<const MLOperand>> outputs;
  for (const auto& validated_output : validated_outputs.value()) {
    outputs.push_back(MLOperand::CreateOutput(this, validated_output, split));
  }
  split->Connect({input}, outputs);
  return outputs;
}

MLOperand* MLGraphBuilder::tanh(const MLOperand* input,
                                const MLOperatorOptions* options,
                                ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  // The input data type must be one of the floating point types.
  // The current spec doesn't specify the operand data type constraints of tanh,
  // an issue has been filed to track it-
  // https://github.com/webmachinelearning/webnn/issues/283.
  //
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-tanh, the output tensor of
  // tanh has the same data type and dimensions as its input.
  return BuildUnaryOperator(
      this, exception_state, webnn::mojom::blink::Operation::Tag::kTanh,
      ml_context_->GetProperties().data_type_limits.tanh_input, input, options);
}

MLOperand* MLGraphBuilder::tile(const MLOperand* input,
                                const Vector<uint32_t>& repetitions,
                                const MLOperatorOptions* options,
                                ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateTileAndInferOutput(ml_context_->GetProperties(),
                                        input->Descriptor(), repetitions,
                                        options->label().Utf8()));

  auto* tile = MakeGarbageCollected<MLTileOperator>(this, repetitions, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), tile);

  tile->Connect({input}, {output});
  return output;
}

MLOperand* MLGraphBuilder::transpose(const MLOperand* input,
                                     const MLTransposeOptions* options,
                                     ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  // According to WebNN spec:
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-transpose,
  // When permutation is not specified, it’s set to [N-1, ..., 0], where N is
  // the rank of the input tensor.
  const Vector<uint32_t> permutation =
      options->getPermutationOr(CreateDefaultPermutation(input->Rank()));
  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateTransposeAndInferOutput(ml_context_->GetProperties(),
                                             input->Descriptor(), permutation,
                                             options->label().Utf8()));

  auto* transpose = MakeGarbageCollected<MLOperator>(
      this, webnn::mojom::blink::Operation::Tag::kTranspose, options);
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-transpose, the output
  // tensor of transpose has the same data type as its input.
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), transpose);

  transpose->Connect({input}, {output});
  return output;
}

MLOperand* MLGraphBuilder::triangular(const MLOperand* input,
                                      const MLTriangularOptions* options,
                                      ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateTriangularAndInferOutput(ml_context_->GetProperties(),
                                              input->Descriptor(),
                                              options->label().Utf8()));

  auto* triangular = MakeGarbageCollected<MLOperator>(
      this, webnn::mojom::blink::Operation::Tag::kTriangular, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), triangular);

  triangular->Connect({input}, {output});
  return output;
}

MLOperand* MLGraphBuilder::where(const MLOperand* condition,
                                 const MLOperand* true_value,
                                 const MLOperand* false_value,
                                 const MLOperatorOptions* options,
                                 ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  HeapVector<Member<const MLOperand>> inputs = {condition, true_value,
                                                false_value};
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateWhereAndInferOutput(
          ml_context_->GetProperties(), condition->Descriptor(),
          true_value->Descriptor(), false_value->Descriptor(),
          options->label().Utf8()));

  auto* where = MakeGarbageCollected<MLOperator>(
      this, webnn::mojom::blink::Operation::Tag::kWhere, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), where);
  where->Connect(std::move(inputs), {output});
  return output;
}

ScriptPromise<MLGraph> MLGraphBuilder::build(
    ScriptState* script_state,
    const MLNamedOperands& named_outputs,
    ExceptionState& exception_state) {
  base::expected<void, String> validation_result = ValidateGraphBuilderState();
  if (!validation_result.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      std::move(validation_result.error()));
    return EmptyPromise();
  }

  HeapVector<Member<const MLOperand>> outputs(named_outputs.size());
  base::ranges::transform(
      named_outputs, outputs.begin(),
      [](const auto& named_output) { return named_output.second; });
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(outputs),
                                 ScriptPromise<MLGraph>());

  for (const auto& named_output : named_outputs) {
    if (!ml_context_->GetProperties().data_type_limits.output().Has(
            named_output.second->DataType())) {
      exception_state.ThrowTypeError(String(webnn::NotSupportedOutputTypeError(
          named_output.first.Utf8(), named_output.second->DataType(),
          ml_context_->GetProperties().data_type_limits.output())));
      return EmptyPromise();
    }
  }

  ScopedMLTrace scoped_trace("MLGraphBuilder::build");
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return EmptyPromise();
  }

  auto graph_constraints = DetermineGraphConstraintsFromOutputs(named_outputs);
  if (!graph_constraints.has_value()) {
    exception_state.ThrowTypeError(graph_constraints.error());
    return EmptyPromise();
  }

  auto graph_info =
      BuildWebNNGraphInfo(named_outputs, ml_context_->GetProperties());
  if (!graph_info.has_value()) {
    // TODO(crbug.com/345271830): Move the platform-specific checks into the
    // respective synchronous operator builder methods, such that
    // `BuildWebNNGraphInfo` always succeeds.
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Failed to build graph: " + graph_info.error());
    return EmptyPromise();
  }

  // Set `has_built_` after all inputs have been validated.
  has_built_ = true;

  // Release constant data held by the renderer now that it has been copied to
  // the remote graph.
  ReleaseConstantData();

  pending_resolver_ = MakeGarbageCollected<ScriptPromiseResolver<MLGraph>>(
      script_state, exception_state.GetContext());

  remote_->CreateGraph(
      *std::move(graph_info),
      WTF::BindOnce(&MLGraphBuilder::DidCreateWebNNGraph, WrapPersistent(this),
                    WrapPersistent(pending_resolver_.Get()),
                    *std::move(graph_constraints)));
  return pending_resolver_->Promise();
}

void MLGraphBuilder::DidCreateWebNNGraph(
    ScriptPromiseResolver<blink::MLGraph>* resolver,
    std::pair<MLGraph::NamedOperandDescriptors,
              MLGraph::NamedOperandDescriptors> input_and_output_constraints,
    webnn::mojom::blink::CreateGraphResultPtr result) {
  CHECK(has_built_);

  pending_resolver_.Clear();

  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid()) {
    return;
  }

  if (result->is_error()) {
    const auto& create_graph_error = result->get_error();
    resolver->RejectWithDOMException(
        WebNNErrorCodeToDOMExceptionCode(create_graph_error->code),
        create_graph_error->message);
    return;
  }

  auto* graph = MakeGarbageCollected<MLGraph>(
      resolver->GetExecutionContext(), ml_context_,
      std::move(result->get_graph_remote()),
      std::move(input_and_output_constraints.first),
      std::move(input_and_output_constraints.second),
      base::PassKey<MLGraphBuilder>());
  ml_context_->OnGraphCreated(graph);

  resolver->Resolve(graph);
}

void MLGraphBuilder::OnConnectionError() {
  remote_.reset();

  ReleaseConstantData();

  if (pending_resolver_) {
    pending_resolver_->RejectWithDOMException(
        DOMExceptionCode::kInvalidStateError, "Context is lost.");
    pending_resolver_.Clear();
  }
}

base::expected<void, String> MLGraphBuilder::ValidateGraphBuilderState() const {
  if (has_built_) {
    return base::unexpected(kGraphAlreadyBuiltError);
  }
  if (!remote_.is_bound()) {
    return base::unexpected("Context is lost.");
  }
  return base::ok();
}

// As specified in https://www.w3.org/TR/webnn/#mlgraphbuilder-validate-operand.
base::expected<void, String> MLGraphBuilder::ValidateInput(
    const MLOperand* input) {
  CHECK(input);
  if (input->Builder() != this) {
    return base::unexpected("Invalid input: Created from another builder.");
  }
  return base::ok();
}

base::expected<void, String> MLGraphBuilder::ValidateInputs(
    const HeapVector<Member<const MLOperand>>& inputs) {
  for (const MLOperand* input_to_validate : inputs) {
    RETURN_IF_ERROR(ValidateInput(input_to_validate));
  }
  return base::ok();
}

void MLGraphBuilder::ReleaseConstantData() {
  base::ranges::for_each(constant_operands_, [](auto& constant_operand) {
    constant_operand->ReleaseBytes();
  });
  constant_operands_.clear();
}

}  // namespace blink
