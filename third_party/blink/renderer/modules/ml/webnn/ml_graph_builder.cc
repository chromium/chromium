// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"

#include <algorithm>

#include "base/numerics/checked_math.h"
#include "components/ml/webnn/features.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_batch_normalization_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_clamp_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_transpose_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_elu_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gather_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gemm_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_leaky_relu_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pad_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pool_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_reduce_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_resample_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_split_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_transpose_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/ml/buildflags.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_activation.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

#if BUILDFLAG(BUILD_WEBNN_WITH_XNNPACK)
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_xnnpack.h"
#endif

#if BUILDFLAG(BUILD_WEBNN_ON_CROS)
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_cros.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS)
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_mojo.h"
#endif

namespace blink {

namespace {

MLGraphBuilder::BackendForTesting* g_backend_for_testing = nullptr;

blink::V8MLOperandDataType::Enum ComponentOperandTypeToBlink(
    webnn::Operand::DataType data_type) {
  switch (data_type) {
    case webnn::Operand::DataType::kFloat32:
      return blink::V8MLOperandDataType::Enum::kFloat32;
    case webnn::Operand::DataType::kFloat16:
      return blink::V8MLOperandDataType::Enum::kFloat16;
    case webnn::Operand::DataType::kInt32:
      return blink::V8MLOperandDataType::Enum::kInt32;
    case webnn::Operand::DataType::kUint32:
      return blink::V8MLOperandDataType::Enum::kUint32;
    case webnn::Operand::DataType::kInt64:
      return blink::V8MLOperandDataType::Enum::kInt64;
    case webnn::Operand::DataType::kUint64:
      return blink::V8MLOperandDataType::Enum::kUint64;
    case webnn::Operand::DataType::kInt8:
      return blink::V8MLOperandDataType::Enum::kInt8;
    case webnn::Operand::DataType::kUint8:
      return blink::V8MLOperandDataType::Enum::kUint8;
  }
  NOTREACHED_NORETURN();
}

webnn::Operand::DataType BlinkOperandTypeToComponent(
    blink::V8MLOperandDataType::Enum data_type) {
  switch (data_type) {
    case blink::V8MLOperandDataType::Enum::kFloat32:
      return webnn::Operand::DataType::kFloat32;
    case blink::V8MLOperandDataType::Enum::kFloat16:
      return webnn::Operand::DataType::kFloat16;
    case blink::V8MLOperandDataType::Enum::kInt32:
      return webnn::Operand::DataType::kInt32;
    case blink::V8MLOperandDataType::Enum::kUint32:
      return webnn::Operand::DataType::kUint32;
    case blink::V8MLOperandDataType::Enum::kInt64:
      return webnn::Operand::DataType::kInt64;
    case blink::V8MLOperandDataType::Enum::kUint64:
      return webnn::Operand::DataType::kUint64;
    case blink::V8MLOperandDataType::Enum::kInt8:
      return webnn::Operand::DataType::kInt8;
    case blink::V8MLOperandDataType::Enum::kUint8:
      return webnn::Operand::DataType::kUint8;
  }
  NOTREACHED_NORETURN();
}

webnn::Operand ConvertToComponentOperand(const blink::MLOperand* ml_operand) {
  return webnn::Operand(BlinkOperandTypeToComponent(ml_operand->DataType()),
                        ml_operand->Dimensions());
}

webnn::InputOperandLayout BlinkInputOperandLayoutToComponent(
    blink::V8MLInputOperandLayout::Enum type) {
  switch (type) {
    case blink::V8MLInputOperandLayout::Enum::kNchw:
      return webnn::InputOperandLayout::kNchw;
    case blink::V8MLInputOperandLayout::Enum::kNhwc:
      return webnn::InputOperandLayout::kNhwc;
  }
  NOTREACHED_NORETURN();
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
  NOTREACHED_NORETURN();
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
  NOTREACHED_NORETURN();
}

webnn::RoundingType BlinkRoundingTypeToComponent(
    blink::V8MLRoundingType::Enum type) {
  switch (type) {
    case blink::V8MLRoundingType::Enum::kFloor:
      return webnn::RoundingType::kFloor;
    case blink::V8MLRoundingType::Enum::kCeil:
      return webnn::RoundingType::kCeil;
  }
  NOTREACHED_NORETURN();
}

webnn::ReduceKind BlinkReduceKindToComponent(
    blink::MLOperator::OperatorKind kind) {
  switch (kind) {
    case blink::MLOperator::OperatorKind::kReduceL1:
      return webnn::ReduceKind::kL1;
    case blink::MLOperator::OperatorKind::kReduceL2:
      return webnn::ReduceKind::kL2;
    case blink::MLOperator::OperatorKind::kReduceLogSum:
      return webnn::ReduceKind::kLogSum;
    case blink::MLOperator::OperatorKind::kReduceLogSumExp:
      return webnn::ReduceKind::kLogSumExp;
    case blink::MLOperator::OperatorKind::kReduceMax:
      return webnn::ReduceKind::kMax;
    case blink::MLOperator::OperatorKind::kReduceMean:
      return webnn::ReduceKind::kMean;
    case blink::MLOperator::OperatorKind::kReduceMin:
      return webnn::ReduceKind::kMin;
    case blink::MLOperator::OperatorKind::kReduceProduct:
      return webnn::ReduceKind::kProduct;
    case blink::MLOperator::OperatorKind::kReduceSum:
      return webnn::ReduceKind::kSum;
    case blink::MLOperator::OperatorKind::kReduceSumSquare:
      return webnn::ReduceKind::kSumSquare;
    default:
      NOTREACHED_NORETURN();
  }
}

webnn::BatchNormalizationAttributes ConvertToBatchNormalizationAttributes(
    const blink::MLBatchNormalizationOptions* options) {
  CHECK(options);
  webnn::BatchNormalizationAttributes attributes;
  if (options->hasScale()) {
    attributes.scale = ConvertToComponentOperand(options->scale());
  }
  if (options->hasBias()) {
    attributes.bias = ConvertToComponentOperand(options->bias());
  }
  attributes.axis = options->axis();
  return attributes;
}

template <typename MLConv2dOptionsType, typename Conv2dAttributesType>
base::expected<Conv2dAttributesType, String> ConvertToConv2dAttributesBase(
    const MLConv2dOptionsType* options) {
  Conv2dAttributesType attributes;
  CHECK(options);
  // If padding is not present, the values are assumed to be [0,0,0,0].
  auto padding = options->getPaddingOr({0, 0, 0, 0});
  if (padding.size() != 4) {
    return base::unexpected("The length of padding should be 4.");
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
    return base::unexpected("The length of strides should be 2.");
  }
  attributes.strides =
      webnn::Size2d<uint32_t>{.height = strides[0], .width = strides[1]};

  // If dilations is not present, the values are assumed to be [1,1].
  auto dilations = options->getDilationsOr({1, 1});
  if (dilations.size() != 2) {
    return base::unexpected("The length of dilations should be 2.");
  }
  attributes.dilations =
      webnn::Size2d<uint32_t>{.height = dilations[0], .width = dilations[1]};
  attributes.auto_pad = BlinkAutoPadToComponent(options->autoPad().AsEnum());
  attributes.groups = options->groups();
  attributes.input_layout =
      BlinkInputOperandLayoutToComponent(options->inputLayout().AsEnum());
  if (options->hasBias()) {
    attributes.bias_operand = ConvertToComponentOperand(options->bias());
  }

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

  // If output padding is not present, the values are assumed to be [0,0].
  const auto output_padding = options->getOutputPaddingOr({0, 0});
  if (output_padding.size() != 2) {
    return base::unexpected("The length of output padding should be 2.");
  }
  attributes.value().output_padding = webnn::Size2d<uint32_t>{
      .height = output_padding[0], .width = output_padding[1]};

  if (options->hasOutputSizes()) {
    auto output_sizes = options->getOutputSizesOr({});
    if (output_sizes.size() != 2) {
      return base::unexpected("The length of output sizes should be 2.");
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
  webnn::Pool2dAttributes attributes;
  if (options->hasWindowDimensions()) {
    auto& window_dimensions = options->windowDimensions();
    if (window_dimensions.size() != 2) {
      return base::unexpected("The length of window dimensions should be 2.");
    }
    attributes.window_dimensions = webnn::Size2d<uint32_t>{
        .height = window_dimensions[0], .width = window_dimensions[1]};
  }

  // If padding is not present, the values are assumed to be [0,0,0,0].
  auto padding = options->getPaddingOr({0, 0, 0, 0});
  if (padding.size() != 4) {
    return base::unexpected("The length of padding should be 4.");
  }
  attributes.padding = webnn::Padding2d{
      .beginning =
          webnn::Size2d<uint32_t>{.height = padding[0], .width = padding[2]},
      .ending =
          webnn::Size2d<uint32_t>{.height = padding[1], .width = padding[3]}};

  // If strides is not present, the values are assumed to be [1,1].
  auto strides = options->getStridesOr({1, 1});
  if (strides.size() != 2) {
    return base::unexpected("The length of strides should be 2.");
  }
  attributes.strides =
      webnn::Size2d<uint32_t>{.height = strides[0], .width = strides[1]};

  // If dilations is not present, the values are assumed to be [1,1].
  auto dilations = options->getDilationsOr({1, 1});
  if (dilations.size() != 2) {
    return base::unexpected("The length of dilations should be 2.");
  }
  attributes.dilations =
      webnn::Size2d<uint32_t>{.height = dilations[0], .width = dilations[1]};
  attributes.auto_pad = BlinkAutoPadToComponent(options->autoPad().AsEnum());
  attributes.layout =
      BlinkInputOperandLayoutToComponent(options->layout().AsEnum());
  attributes.rounding_type =
      BlinkRoundingTypeToComponent(options->roundingType().AsEnum());
  if (options->hasOutputSizes()) {
    // TODO(ningxin.hu@intel.com): report a DevTools warning message if rounding
    // type is provided but ignored.
    auto& output_size = options->outputSizes();
    if (output_size.size() != 2) {
      return base::unexpected("The length of output sizes should be 2.");
    }
    attributes.output_sizes = webnn::Size2d<uint32_t>{.height = output_size[0],
                                                      .width = output_size[1]};
  }
  return attributes;
}

webnn::GemmAttributes ConvertToGemmAttributes(
    const blink::MLGemmOptions* options) {
  CHECK(options);
  webnn::GemmAttributes attributes;
  if (options->hasC()) {
    attributes.c_operand = ConvertToComponentOperand(options->c());
  }
  attributes.alpha = options->alpha();
  attributes.beta = options->beta();
  attributes.a_transpose = options->aTranspose();
  attributes.b_transpose = options->bTranspose();
  return attributes;
}

bool ValidateClampOptions(const MLClampOptions* options,
                          ExceptionState& exception_state) {
  // The generated code of MLClampOptions uses blink::ToRestrictedFloat to
  // convert the min/max value to a single precision float. It will throw on
  // non-finite values.
  if (options->hasMinValue() && options->hasMaxValue()) {
    if (options->minValue() > options->maxValue()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          String::Format("The min value (%f) should be less than or equal to "
                         "the max value (%f).",
                         options->minValue(), options->maxValue()));
      return false;
    }
  }
  return true;
}

absl::optional<Vector<uint32_t>> BroadcastShapes(
    const Vector<uint32_t>& dims_lhs,
    const Vector<uint32_t>& dims_rhs,
    bool bidirectional = true) {
  auto output_shape = webnn::BroadcastShapes(
      base::make_span(dims_lhs), base::make_span(dims_rhs), bidirectional);
  if (!output_shape) {
    return absl::nullopt;
  }
  return Vector<uint32_t>(output_shape.value());
}

MLOperand* BuildElementWiseBinary(MLGraphBuilder* builder,
                                  MLOperator::OperatorKind kind,
                                  const MLOperand* a,
                                  const MLOperand* b,
                                  ExceptionState& exception_state) {
  if (a->DataType() != b->DataType()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The input operand data types don't match.");
    return nullptr;
  }
  absl::optional<Vector<uint32_t>> dims_output =
      BroadcastShapes(a->Dimensions(), b->Dimensions());
  if (!dims_output) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The input shapes are not broadcastable.");
    return nullptr;
  }
  auto* binary = MakeGarbageCollected<MLOperator>(builder, kind);
  auto output = MLOperand::ValidateAndCreateOutput(builder, a->DataType(),
                                                   dims_output.value(), binary);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  binary->Connect({a, b}, {output.value()});
  return output.value();
}

MLOperand* BuildUnaryOperator(
    MLGraphBuilder* builder,
    ExceptionState& exception_state,
    MLOperator::OperatorKind kind,
    const webnn::DataTypeConstraintSet& data_type_constraint,
    const MLOperand* input,
    const bindings::DictionaryBase* options = nullptr) {
  // The output tensor of unary operator has the same data type and dimensions
  // as its input tensor.
  if (!data_type_constraint.Has(
          BlinkOperandTypeToComponent(input->DataType()))) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        String::Format(
            "The input data type must be one of the %s types.",
            webnn::DataTypeConstraintToString(data_type_constraint).c_str()));
    return nullptr;
  }

  auto* unary = MakeGarbageCollected<MLOperator>(builder, kind, options);
  auto output = MLOperand::ValidateAndCreateOutput(builder, input->DataType(),
                                                   input->Dimensions(), unary);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  unary->Connect({input}, {output.value()});
  return output.value();
}

MLOperand* BuildReduce(MLGraphBuilder* builder,
                       MLOperator::OperatorKind kind,
                       const MLOperand* input,
                       const MLReduceOptions* options,
                       ExceptionState& exception_state) {
  const auto input_rank = input->Dimensions().size();
  const auto axes = options->getAxesOr(CreateAllAxes(input_rank));
  auto validated_output = webnn::ValidateReduceAndInferOutput(
      BlinkReduceKindToComponent(kind), ConvertToComponentOperand(input), axes,
      options->keepDimensions());
  if (!validated_output.has_value()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        String::FromUTF8(validated_output.error()));
    return nullptr;
  }

  auto* reduce = MakeGarbageCollected<MLOperator>(builder, kind, options);
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-reduce, the output
  // tensor of reduce has the same data type as its input.
  auto output = MLOperand::ValidateAndCreateOutput(
      builder, ComponentOperandTypeToBlink(validated_output->data_type),
      Vector<uint32_t>(validated_output->dimensions), reduce);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  reduce->Connect({input}, {output.value()});
  return output.value();
}

MLOperand* BuildPool2d(MLGraphBuilder* builder,
                       MLOperator::OperatorKind kind,
                       const MLOperand* input,
                       const MLPool2dOptions* options,
                       ExceptionState& exception_state) {
  auto pool2d_attributes = ConvertToPool2dAttributes(options);
  if (!pool2d_attributes.has_value()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        WTF::String::FromUTF8(pool2d_attributes.error()));
    return nullptr;
  }

  auto validated_output = webnn::ValidatePool2dAndInferOutput(
      ConvertToComponentOperand(input), std::move(pool2d_attributes.value()));
  if (!validated_output.has_value()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        WTF::String::FromUTF8(validated_output.error()));
    return nullptr;
  }
  // Create pool2d operator and its output operand. Connect the pool2d operator
  // to its input and output operands.
  auto* pool2d = MakeGarbageCollected<MLOperator>(builder, kind, options);
  auto output = MLOperand::ValidateAndCreateOutput(
      builder, input->DataType(),
      Vector<uint32_t>(validated_output->dimensions), pool2d);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  pool2d->Connect({input}, {output.value()});
  return output.value();
}

}  // namespace

// static
MLGraphBuilder* MLGraphBuilder::Create(MLContext* context) {
  return MakeGarbageCollected<MLGraphBuilder>(context);
}

MLGraphBuilder::MLGraphBuilder(MLContext* context) : ml_context_(context) {}

MLGraphBuilder::~MLGraphBuilder() = default;

void MLGraphBuilder::Trace(Visitor* visitor) const {
  visitor->Trace(ml_context_);
  ScriptWrappable::Trace(visitor);
}

MLContext* MLGraphBuilder::GetContext() const {
  return ml_context_.Get();
}

MLOperand* MLGraphBuilder::input(String name,
                                 const MLOperandDescriptor* desc,
                                 ExceptionState& exception_state) {
  // If no dimensions, it represents a scalar. Set dimensions to empty.
  Vector<uint32_t> dimensions = desc->getDimensionsOr({});
  auto input_operand = MLOperand::ValidateAndCreateInput(
      this, desc->dataType().AsEnum(), std::move(dimensions), std::move(name));
  if (!input_operand.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      input_operand.error());
    return nullptr;
  }
  return input_operand.value();
}

MLOperand* MLGraphBuilder::constant(const MLOperandDescriptor* desc,
                                    NotShared<DOMArrayBufferView> buffer_view,
                                    ExceptionState& exception_state) {
  String error_message;
  // If no dimensions, it represents a scalar. Set dimensions to empty.
  Vector<uint32_t> dimensions = desc->getDimensionsOr({});
  auto constant_operand = MLOperand::ValidateAndCreateConstant(
      this, desc->dataType().AsEnum(), std::move(dimensions),
      buffer_view.Get());
  if (!constant_operand.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      constant_operand.error());
    return nullptr;
  }
  return constant_operand.value();
}

MLOperand* MLGraphBuilder::batchNormalization(
    const MLOperand* input,
    const MLOperand* mean,
    const MLOperand* variance,
    const MLBatchNormalizationOptions* options,
    ExceptionState& exception_state) {
  const auto validated_output = webnn::ValidateBatchNormalizationAndInferOutput(
      ConvertToComponentOperand(input), ConvertToComponentOperand(mean),
      ConvertToComponentOperand(variance),
      ConvertToBatchNormalizationAttributes(options));
  if (!validated_output.has_value()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        WTF::String::FromUTF8(validated_output.error()));
    return nullptr;
  }

  // Create batchNormalization operator and its output operand. Connect the
  // batchNormalization operator to its input and output operands.
  auto* batch_normalization = MakeGarbageCollected<MLOperator>(
      this, MLOperator::OperatorKind::kBatchNormalization, options);
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
  auto output = MLOperand::ValidateAndCreateOutput(
      this, ComponentOperandTypeToBlink(validated_output->data_type),
      Vector<uint32_t>(validated_output->dimensions), batch_normalization);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  batch_normalization->Connect(std::move(inputs), {output.value()});
  return output.value();
}

MLOperand* MLGraphBuilder::concat(const HeapVector<Member<MLOperand>>& inputs,
                                  const uint32_t axis,
                                  ExceptionState& exception_state) {
  std::vector<webnn::Operand> input_component_operands;
  input_component_operands.reserve(inputs.size());
  base::ranges::transform(
      inputs, std::back_inserter(input_component_operands),
      [](const auto& input) { return ConvertToComponentOperand(input); });

  auto validated_output =
      webnn::ValidateConcatAndInferOutput(input_component_operands, axis);
  if (!validated_output.has_value()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        String::FromUTF8(validated_output.error()));
    return nullptr;
  }

  auto* concat = MakeGarbageCollected<MLConcatOperator>(this, axis);
  auto output = MLOperand::ValidateAndCreateOutput(
      this, ComponentOperandTypeToBlink(validated_output->data_type),
      Vector<uint32_t>(validated_output->dimensions), concat);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  concat->Connect((HeapVector<Member<const MLOperand>>)inputs,
                  {output.value()});
  return output.value();
}

MLOperand* MLGraphBuilder::clamp(const MLOperand* input,
                                 const MLClampOptions* options,
                                 ExceptionState& exception_state) {
  if (!ValidateClampOptions(options, exception_state)) {
    return nullptr;
  }
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-clamp, the output tensor of
  // clamp has the same data type and dimensions as its input.
  return BuildUnaryOperator(
      this, exception_state, MLOperator::OperatorKind::kClamp,
      webnn::DataTypeConstraintSet::All(), input, options);
}

MLActivation* MLGraphBuilder::clamp(const MLClampOptions* options,
                                    ExceptionState& exception_state) {
  if (!ValidateClampOptions(options, exception_state)) {
    return nullptr;
  }
  // Create the clamp operator that would be used as an activation function.
  return MakeGarbageCollected<MLActivation>(
      this, MLOperator::OperatorKind::kClamp, options);
}

MLOperand* MLGraphBuilder::conv2d(const MLOperand* input,
                                  const MLOperand* filter,
                                  const MLConv2dOptions* options,
                                  ExceptionState& exception_state) {
  auto conv2d_attributes = ConvertToConv2dAttributes(options);
  if (!conv2d_attributes.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      conv2d_attributes.error());
    return nullptr;
  }

  auto validated_output = webnn::ValidateConv2dAndInferOutput(
      ConvertToComponentOperand(input), ConvertToComponentOperand(filter),
      std::move(conv2d_attributes.value()));
  if (!validated_output.has_value()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        WTF::String::FromUTF8(validated_output.error()));
    return nullptr;
  }
  // Create conv2d operator and its output operand. Connect the conv2d operator
  // to its input and output operands.
  auto* conv2d = MakeGarbageCollected<MLOperator>(
      this, MLOperator::OperatorKind::kConv2d, options);
  HeapVector<Member<const MLOperand>> inputs = {input, filter};
  if (options->hasBias()) {
    inputs.push_back(options->bias());
  }
  auto output = MLOperand::ValidateAndCreateOutput(
      this, ComponentOperandTypeToBlink(validated_output.value().data_type),
      Vector<uint32_t>(validated_output.value().dimensions), conv2d);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  conv2d->Connect(std::move(inputs), {output.value()});
  return output.value();
}

MLOperand* MLGraphBuilder::convTranspose2d(
    const MLOperand* input,
    const MLOperand* filter,
    const MLConvTranspose2dOptions* options,
    ExceptionState& exception_state) {
  auto convTranspose2d_attributes = ConvertToConvTranspose2dAttributes(options);
  if (!convTranspose2d_attributes.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      convTranspose2d_attributes.error());
    return nullptr;
  }

  auto validated_output = webnn::ValidateConvTranspose2dAndInferOutput(
      ConvertToComponentOperand(input), ConvertToComponentOperand(filter),
      std::move(convTranspose2d_attributes.value()));
  if (!validated_output.has_value()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        WTF::String::FromUTF8(validated_output.error()));
    return nullptr;
  }
  // Create convTranspose2d operator and its output operand. Connect the
  // convTranspose2d operator to its input and output operands.
  auto* convTranspose2d = MakeGarbageCollected<MLOperator>(
      this, MLOperator::OperatorKind::kConvTranspose2d, options);
  HeapVector<Member<const MLOperand>> inputs = {input, filter};
  if (options->hasBias()) {
    inputs.push_back(options->bias());
  }
  auto output = MLOperand::ValidateAndCreateOutput(
      this, ComponentOperandTypeToBlink(validated_output.value().data_type),
      Vector<uint32_t>(validated_output.value().dimensions), convTranspose2d);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  convTranspose2d->Connect(std::move(inputs), {output.value()});
  return output.value();
}

#define BUILD_ELEMENTWISE_BINARY_OP(op, op_kind)                              \
  MLOperand* MLGraphBuilder::op(const MLOperand* a, const MLOperand* b,       \
                                ExceptionState& exception_state) {            \
    return BuildElementWiseBinary(this, MLOperator::OperatorKind::op_kind, a, \
                                  b, exception_state);                        \
  }

BUILD_ELEMENTWISE_BINARY_OP(add, kAdd)
BUILD_ELEMENTWISE_BINARY_OP(sub, kSub)
BUILD_ELEMENTWISE_BINARY_OP(mul, kMul)
BUILD_ELEMENTWISE_BINARY_OP(div, kDiv)
BUILD_ELEMENTWISE_BINARY_OP(min, kMin)
BUILD_ELEMENTWISE_BINARY_OP(max, kMax)
BUILD_ELEMENTWISE_BINARY_OP(pow, kPow)

#define BUILD_ELEMENTWISE_UNARY_OP(op, op_kind, data_type_constraint) \
  MLOperand* MLGraphBuilder::op(const MLOperand* input,               \
                                ExceptionState& exception_state) {    \
    return BuildUnaryOperator(this, exception_state,                  \
                              MLOperator::OperatorKind::op_kind,      \
                              data_type_constraint, input);           \
  }

BUILD_ELEMENTWISE_UNARY_OP(abs,
                           kAbs,
                           Union(webnn::DataTypeConstraint::kFloat,
                                 webnn::DataTypeConstraint::kSignedInteger))
BUILD_ELEMENTWISE_UNARY_OP(ceil, kCeil, webnn::DataTypeConstraint::kFloat)
BUILD_ELEMENTWISE_UNARY_OP(cos, kCos, webnn::DataTypeConstraint::kFloat)
BUILD_ELEMENTWISE_UNARY_OP(exp, kExp, webnn::DataTypeConstraint::kFloat)
BUILD_ELEMENTWISE_UNARY_OP(floor, kFloor, webnn::DataTypeConstraint::kFloat)
BUILD_ELEMENTWISE_UNARY_OP(log, kLog, webnn::DataTypeConstraint::kFloat)
BUILD_ELEMENTWISE_UNARY_OP(neg,
                           kNeg,
                           Union(webnn::DataTypeConstraint::kFloat,
                                 webnn::DataTypeConstraint::kSignedInteger))
BUILD_ELEMENTWISE_UNARY_OP(sin, kSin, webnn::DataTypeConstraint::kFloat)
BUILD_ELEMENTWISE_UNARY_OP(tan, kTan, webnn::DataTypeConstraint::kFloat)
BUILD_ELEMENTWISE_UNARY_OP(erf, kErf, webnn::DataTypeConstraint::kFloat)
BUILD_ELEMENTWISE_UNARY_OP(identity,
                           kIdentity,
                           webnn::DataTypeConstraintSet::All())
BUILD_ELEMENTWISE_UNARY_OP(logicalNot,
                           kLogicalNot,
                           {webnn::Operand::DataType::kUint8})
BUILD_ELEMENTWISE_UNARY_OP(reciprocal,
                           kReciprocal,
                           webnn::DataTypeConstraint::kFloat)
BUILD_ELEMENTWISE_UNARY_OP(sqrt, kSqrt, webnn::DataTypeConstraint::kFloat)

MLOperand* MLGraphBuilder::cast(const MLOperand* input,
                                const V8MLOperandDataType output_data_type,
                                ExceptionState& exception_state) {
  auto* cast =
      MakeGarbageCollected<MLOperator>(this, MLOperator::OperatorKind::kCast);
  auto output = MLOperand::ValidateAndCreateOutput(
      this, output_data_type.AsEnum(), input->Dimensions(), cast);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  cast->Connect({input}, {output.value()});
  return output.value();
}

#define BUILD_REDUCE_OP(op, op_kind)                                   \
  MLOperand* MLGraphBuilder::op(const MLOperand* input,                \
                                const MLReduceOptions* options,        \
                                ExceptionState& exception_state) {     \
    return BuildReduce(this, MLOperator::OperatorKind::op_kind, input, \
                       options, exception_state);                      \
  }

BUILD_REDUCE_OP(reduceL1, kReduceL1)
BUILD_REDUCE_OP(reduceL2, kReduceL2)
BUILD_REDUCE_OP(reduceLogSum, kReduceLogSum)
BUILD_REDUCE_OP(reduceLogSumExp, kReduceLogSumExp)
BUILD_REDUCE_OP(reduceMax, kReduceMax)
BUILD_REDUCE_OP(reduceMean, kReduceMean)
BUILD_REDUCE_OP(reduceMin, kReduceMin)
BUILD_REDUCE_OP(reduceProduct, kReduceProduct)
BUILD_REDUCE_OP(reduceSum, kReduceSum)
BUILD_REDUCE_OP(reduceSumSquare, kReduceSumSquare)

MLOperand* MLGraphBuilder::elu(const MLOperand* input,
                               const MLEluOptions* options,
                               ExceptionState& exception_state) {
  // The current spec doesn't restrict the value of alpha. An issue has been
  // filed to track it: https://github.com/webmachinelearning/webnn/issues/383
  if (options->alpha() <= 0.0f) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The value of alpha must be greater than 0.");
    return nullptr;
  }
  // The current spec doesn't specify the operand data type constraints of elu.
  // An issue has been filed to track it:
  // https://github.com/webmachinelearning/webnn/issues/283.
  //
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-elu, the output tensor of
  // elu has the same data type and dimensions as its input.
  return BuildUnaryOperator(this, exception_state,
                            MLOperator::OperatorKind::kElu,
                            webnn::DataTypeConstraint::kFloat, input, options);
}

MLActivation* MLGraphBuilder::elu(const MLEluOptions* options,
                                  ExceptionState& exception_state) {
  // The current spec doesn't restrict the value of alpha. An issue has been
  // filed to track it: https://github.com/webmachinelearning/webnn/issues/383
  if (options->alpha() <= 0.0f) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The value of alpha must be greater than 0.");
    return nullptr;
  }
  // Create the elu operator that would be used as an activation
  // function.
  return MakeGarbageCollected<MLActivation>(
      this, MLOperator::OperatorKind::kElu, options);
}

MLOperand* MLGraphBuilder::expand(const MLOperand* input,
                                  const Vector<uint32_t>& new_shape,
                                  ExceptionState& exception_state) {
  auto output_shape = BroadcastShapes(input->Dimensions(), new_shape, false);
  if (!output_shape) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The input shape is not broadcastable to the new shape.");
    return nullptr;
  }
  CHECK(output_shape.value() == new_shape);

  auto* expand =
      MakeGarbageCollected<MLOperator>(this, MLOperator::OperatorKind::kExpand);
  auto output = MLOperand::ValidateAndCreateOutput(
      this, input->DataType(), output_shape.value(), expand);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  expand->Connect({input}, {output.value()});
  return output.value();
}

MLOperand* MLGraphBuilder::gather(const MLOperand* input,
                                  const MLOperand* indices,
                                  const MLGatherOptions* options,
                                  ExceptionState& exception_state) {
  auto validated_output = webnn::ValidateGatherAndInferOutput(
      ConvertToComponentOperand(input), ConvertToComponentOperand(indices),
      options->axis());
  if (!validated_output.has_value()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        String::FromUTF8(validated_output.error()));
    return nullptr;
  }

  auto* gather = MakeGarbageCollected<MLOperator>(
      this, MLOperator::OperatorKind::kGather, options);
  HeapVector<Member<const MLOperand>> inputs = {input, indices};
  auto output = MLOperand::ValidateAndCreateOutput(
      this, ComponentOperandTypeToBlink(validated_output->data_type),
      Vector<uint32_t>(validated_output->dimensions), gather);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }

  gather->Connect(std::move(inputs), {output.value()});
  return output.value();
}

MLOperand* MLGraphBuilder::gemm(const MLOperand* a,
                                const MLOperand* b,
                                const MLGemmOptions* options,
                                ExceptionState& exception_state) {
  auto validated_output = webnn::ValidateGemmAndInferOutput(
      ConvertToComponentOperand(a), ConvertToComponentOperand(b),
      ConvertToGemmAttributes(options));
  if (!validated_output.has_value()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        WTF::String::FromUTF8(validated_output.error()));
    return nullptr;
  }
  auto* gemm = MakeGarbageCollected<MLOperator>(
      this, MLOperator::OperatorKind::kGemm, options);
  HeapVector<Member<const MLOperand>> inputs = {a, b};
  if (options->hasC()) {
    inputs.push_back(options->c());
  }
  auto output = MLOperand::ValidateAndCreateOutput(
      this, ComponentOperandTypeToBlink(validated_output.value().data_type),
      Vector<uint32_t>(validated_output.value().dimensions), gemm);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  gemm->Connect(std::move(inputs), {output.value()});
  return output.value();
}

MLOperand* MLGraphBuilder::hardSwish(const MLOperand* input,
                                     ExceptionState& exception_state) {
  // The input data type must be one of the floating point types. Although this
  // constraint is not specified in current WebNN spec, there is a feature
  // request for that: https://github.com/webmachinelearning/webnn/issues/283
  //
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-hard-swish, the output
  // tensor of hard-swish has the same data type and dimensions as its input.
  return BuildUnaryOperator(this, exception_state,
                            MLOperator::OperatorKind::kHardSwish,
                            webnn::DataTypeConstraint::kFloat, input);
}

MLActivation* MLGraphBuilder::hardSwish(ExceptionState& exception_state) {
  // Create the hard-swish operator that would be used as an activation
  // function.
  return MakeGarbageCollected<MLActivation>(
      this, MLOperator::OperatorKind::kHardSwish);
}

MLOperand* MLGraphBuilder::leakyRelu(const MLOperand* input,
                                     const MLLeakyReluOptions* options,
                                     ExceptionState& exception_state) {
  // The current spec doesn't specify the operand data type constraints of
  // leakyRelu. An issue has been filed to track it:
  // https://github.com/webmachinelearning/webnn/issues/283.
  //
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-relu, the output tensor of
  // relu has the same data type and dimensions as its input.
  return BuildUnaryOperator(this, exception_state,
                            MLOperator::OperatorKind::kLeakyRelu,
                            webnn::DataTypeConstraint::kFloat, input, options);
}

MLActivation* MLGraphBuilder::leakyRelu(const MLLeakyReluOptions* options,
                                        ExceptionState& exception_state) {
  // Create the leakyRelu operator that would be used as an activation
  // function.
  return MakeGarbageCollected<MLActivation>(
      this, MLOperator::OperatorKind::kLeakyRelu, options);
}

MLOperand* MLGraphBuilder::matmul(const MLOperand* a,
                                  const MLOperand* b,
                                  ExceptionState& exception_state) {
  auto validated_output = webnn::ValidateMatmulAndInferOutput(
      ConvertToComponentOperand(a), ConvertToComponentOperand(b));
  if (!validated_output.has_value()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        WTF::String::FromUTF8(validated_output.error()));
    return nullptr;
  }
  // Create matmul operator and its output operand. Connect the matmul operator
  // to its input and output operands.
  auto* matmul =
      MakeGarbageCollected<MLOperator>(this, MLOperator::OperatorKind::kMatmul);
  auto output = MLOperand::ValidateAndCreateOutput(
      this, ComponentOperandTypeToBlink(validated_output.value().data_type),
      Vector<uint32_t>(validated_output.value().dimensions), matmul);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  HeapVector<Member<const MLOperand>> inputs = {a, b};
  matmul->Connect(std::move(inputs), {output.value()});
  return output.value();
}

MLOperand* MLGraphBuilder::pad(const MLOperand* input,
                               const Vector<uint32_t>& beginning_padding,
                               const Vector<uint32_t>& ending_padding,
                               const MLPadOptions* options,
                               ExceptionState& exception_state) {
  auto validated_output = webnn::ValidatePadAndInferOutput(
      ConvertToComponentOperand(input), beginning_padding, ending_padding);
  if (!validated_output.has_value()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        String::FromUTF8(validated_output.error()));
    return nullptr;
  }

  if (options->mode().AsEnum() != V8MLPaddingMode::Enum::kConstant &&
      fabs(options->value() - 0.0f) > std::numeric_limits<float>::epsilon()) {
    ml_context_->LogConsoleWarning(
        "The pad value is ignored unless the options.mode is set to "
        "constant.");
  }

  auto* pad = MakeGarbageCollected<MLPadOperator>(this, beginning_padding,
                                                  ending_padding, options);
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-pad, the output
  // tensor of pad has the same data type as its input.
  auto output = MLOperand::ValidateAndCreateOutput(
      this, input->DataType(), Vector<uint32_t>(validated_output->dimensions),
      pad);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  pad->Connect({input}, {output.value()});
  return output.value();
}

MLOperand* MLGraphBuilder::averagePool2d(const MLOperand* input,
                                         const MLPool2dOptions* options,
                                         ExceptionState& exception_state) {
  return BuildPool2d(this, MLOperator::OperatorKind::kAveragePool2d, input,
                     options, exception_state);
}

MLOperand* MLGraphBuilder::maxPool2d(const MLOperand* input,
                                     const MLPool2dOptions* options,
                                     ExceptionState& exception_state) {
  return BuildPool2d(this, MLOperator::OperatorKind::kMaxPool2d, input, options,
                     exception_state);
}

MLOperand* MLGraphBuilder::prelu(const MLOperand* input,
                                 const MLOperand* slope,
                                 ExceptionState& exception_state) {
  auto validated_output = webnn::ValidatePreluAndInferOutput(
      ConvertToComponentOperand(input), ConvertToComponentOperand(slope));
  if (!validated_output.has_value()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        String::FromUTF8(validated_output.error()));
    return nullptr;
  }

  auto* prelu =
      MakeGarbageCollected<MLOperator>(this, MLOperator::OperatorKind::kPRelu);
  auto output = MLOperand::ValidateAndCreateOutput(
      this, ComponentOperandTypeToBlink(validated_output->data_type),
      Vector<uint32_t>(validated_output->dimensions), prelu);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  prelu->Connect({input, slope}, {output.value()});
  return output.value();
}

MLOperand* MLGraphBuilder::relu(const MLOperand* input,
                                ExceptionState& exception_state) {
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-relu, the output tensor of
  // relu has the same data type and dimensions as its input.
  return BuildUnaryOperator(this, exception_state,
                            MLOperator::OperatorKind::kRelu,
                            webnn::DataTypeConstraintSet::All(), input);
}

MLActivation* MLGraphBuilder::relu(ExceptionState& exception_state) {
  // Create the relu operator that would be used as an activation function.
  return MakeGarbageCollected<MLActivation>(this,
                                            MLOperator::OperatorKind::kRelu);
}

MLOperand* MLGraphBuilder::reshape(const MLOperand* input,
                                   const Vector<uint32_t>& new_shape,
                                   ExceptionState& exception_state) {
  // Setting the initial number of elements to 1 would cover the 0-D scalar with
  // empty dimensions.
  base::CheckedNumeric<size_t> checked_newshape_number_of_elements = 1;
  Vector<uint32_t> output_shape(new_shape.size());
  for (wtf_size_t i = 0; i < new_shape.size(); ++i) {
    auto dim = new_shape[i];
    if (dim == 0) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "The value of new shape should not be 0.");
      return nullptr;
    }
    checked_newshape_number_of_elements *= dim;
    output_shape[i] = dim;
  }
  size_t newshape_number_of_elements;
  if (!checked_newshape_number_of_elements.AssignIfValid(
          &newshape_number_of_elements)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The number of elements implied by new shape is too large.");
    return nullptr;
  }
  DCHECK_NE(newshape_number_of_elements, size_t(0));
  // The number of elements implied by new shape must be the same as the
  // number of elements in the input tensor.
  if (input->NumberOfElements() != newshape_number_of_elements) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        String::Format(
            "The number of elements (%zu) implied by new shape doesn't match "
            "the number of elements (%zu) in the input tensor.",
            newshape_number_of_elements, input->NumberOfElements()));
    return nullptr;
  }
  auto* reshape = MakeGarbageCollected<MLOperator>(
      this, MLOperator::OperatorKind::kReshape);
  auto output = MLOperand::ValidateAndCreateOutput(
      this, input->DataType(), std::move(output_shape), reshape);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  reshape->Connect({input}, {output.value()});
  return output.value();
}

MLOperand* MLGraphBuilder::resample2d(const MLOperand* input,
                                      const MLResample2dOptions* options,
                                      ExceptionState& exception_state) {
  absl::variant<base::span<const float>, base::span<const uint32_t>>
      scales_or_sizes;
  Vector<float> default_scales = {1.0, 1.0};
  if (options->hasSizes()) {
    if (options->hasScales()) {
      ml_context_->LogConsoleWarning(
          "When sizes and scales are both specified, scales argument is "
          "ignored.");
    }
    scales_or_sizes = options->sizes();
  } else {
    scales_or_sizes = options->hasScales() ? options->scales() : default_scales;
  }

  auto validated_output = webnn::ValidateResample2dAndInferOutput(
      ConvertToComponentOperand(input), scales_or_sizes,
      options->getAxesOr({2, 3}));
  if (!validated_output.has_value()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        String::FromUTF8(validated_output.error()));
    return nullptr;
  }
  // Create resample2d operator and its output operand. Connect the resample2d
  // operator to its input and output operands.
  auto* resample2d = MakeGarbageCollected<MLOperator>(
      this, MLOperator::OperatorKind::kResample2d, options);
  auto output = MLOperand::ValidateAndCreateOutput(
      this, ComponentOperandTypeToBlink(validated_output->data_type),
      Vector<uint32_t>(validated_output->dimensions), resample2d);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  resample2d->Connect({input}, {output.value()});
  return output.value();
}

MLOperand* MLGraphBuilder::sigmoid(const MLOperand* input,
                                   ExceptionState& exception_state) {
  // According to WebNN spec
  // https://webmachinelearning.github.io/webnn/#api-mlgraphbuilder-sigmoid, the
  // output tensor of sigmoid has the same data type and dimensions as its
  // input. And the input data type must be one of the floating point types.
  return BuildUnaryOperator(this, exception_state,
                            MLOperator::OperatorKind::kSigmoid,
                            webnn::DataTypeConstraint::kFloat, input);
}

MLActivation* MLGraphBuilder::sigmoid(ExceptionState& exception_state) {
  // Create the sigmoid operator that would be used as an activation function.
  return MakeGarbageCollected<MLActivation>(this,
                                            MLOperator::OperatorKind::kSigmoid);
}

MLOperand* MLGraphBuilder::slice(const MLOperand* input,
                                 const Vector<uint32_t>& starts,
                                 const Vector<uint32_t>& sizes,
                                 ExceptionState& exception_state) {
  webnn::SliceAttributes attributes;
  attributes.sizes.assign(sizes.begin(), sizes.end());
  attributes.starts.assign(starts.begin(), starts.end());
  auto validated_output = webnn::ValidateSliceAndInferOutput(
      ConvertToComponentOperand(input), attributes);
  if (!validated_output.has_value()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        WTF::String::FromUTF8(validated_output.error()));
    return nullptr;
  }

  auto* slice = MakeGarbageCollected<MLSliceOperator>(this, starts, sizes);
  auto output = MLOperand::ValidateAndCreateOutput(
      this, ComponentOperandTypeToBlink(validated_output->data_type),
      Vector<uint32_t>(validated_output->dimensions), slice);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  slice->Connect({input}, {output.value()});
  return output.value();
}

MLOperand* MLGraphBuilder::softmax(const MLOperand* input,
                                   ExceptionState& exception_state) {
  auto validated_output =
      webnn::ValidateSoftmaxAndInferOutput(ConvertToComponentOperand(input));
  if (!validated_output.has_value()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        WTF::String::FromUTF8(validated_output.error()));
    return nullptr;
  }
  auto* softmax = MakeGarbageCollected<MLOperator>(
      this, MLOperator::OperatorKind::kSoftmax);
  auto output = MLOperand::ValidateAndCreateOutput(
      this, ComponentOperandTypeToBlink(validated_output.value().data_type),
      Vector<uint32_t>(validated_output.value().dimensions), softmax);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  softmax->Connect({input}, {output.value()});
  return output.value();
}

MLActivation* MLGraphBuilder::softmax(ExceptionState& exception_state) {
  // Create the softmax operator that would be used as an activation function.
  return MakeGarbageCollected<MLActivation>(this,
                                            MLOperator::OperatorKind::kSoftmax);
}

MLOperand* MLGraphBuilder::softsign(const MLOperand* input,
                                    ExceptionState& exception_state) {
  // The input data type must be one of the floating point types.
  // The current spec doesn't specify the operand data type constraints of
  // softsign, an issue has been filed to track it-
  // https://github.com/webmachinelearning/webnn/issues/283.
  //
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-softsign, the output tensor
  // of softsign has the same data type and dimensions as its input.
  return BuildUnaryOperator(this, exception_state,
                            MLOperator::OperatorKind::kSoftsign,
                            webnn::DataTypeConstraint::kFloat, input);
}

MLActivation* MLGraphBuilder::softsign(ExceptionState& exception_state) {
  // Create the softsign operator that would be used as an activation function.
  return MakeGarbageCollected<MLActivation>(
      this, MLOperator::OperatorKind::kSoftsign);
}

HeapVector<Member<const MLOperand>> MLGraphBuilder::split(
    const MLOperand* input,
    const uint32_t splits,
    const MLSplitOptions* options,
    ExceptionState& exception_state) {
  auto validated_outputs = webnn::ValidateSplitAndInferOutput(
      ConvertToComponentOperand(input), {
                                            .splits = splits,
                                            .axis = options->axis(),
                                        });
  if (!validated_outputs.has_value()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        WTF::String::FromUTF8(validated_outputs.error()));
    return {};
  }

  auto* split = MakeGarbageCollected<MLSplitOperator>(this, splits, options);
  HeapVector<Member<const MLOperand>> outputs;
  for (const auto& validated_output : validated_outputs.value()) {
    auto output = MLOperand::ValidateAndCreateOutput(
        this, ComponentOperandTypeToBlink(validated_output.data_type),
        Vector<uint32_t>(validated_output.dimensions), split);
    if (!output.has_value()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                        output.error());
      return {};
    }
    outputs.push_back(output.value());
  }
  split->Connect({input}, outputs);
  return outputs;
}

// There are some backends don't support "split into sizes" variant, e.g.
// XNNPACK, and there is an ongoing discussion in WG:
// https://github.com/webmachinelearning/webnn/issues/392
HeapVector<Member<const MLOperand>> MLGraphBuilder::split(
    const MLOperand* input,
    const Vector<uint32_t>& splits,
    const MLSplitOptions* options,
    ExceptionState& exception_state) {
  auto validated_outputs = webnn::ValidateSplitAndInferOutput(
      ConvertToComponentOperand(input), {
                                            .splits = splits,
                                            .axis = options->axis(),
                                        });
  if (!validated_outputs.has_value()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        WTF::String::FromUTF8(validated_outputs.error()));
    return {};
  }

  auto* split = MakeGarbageCollected<MLSplitOperator>(this, splits, options);
  HeapVector<Member<const MLOperand>> outputs;
  for (const auto& validated_output : validated_outputs.value()) {
    auto output = MLOperand::ValidateAndCreateOutput(
        this, ComponentOperandTypeToBlink(validated_output.data_type),
        Vector<uint32_t>(validated_output.dimensions), split);
    if (!output.has_value()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                        output.error());
      return {};
    }
    outputs.push_back(output.value());
  }
  split->Connect({input}, outputs);
  return outputs;
}

MLOperand* MLGraphBuilder::tanh(const MLOperand* input,
                                ExceptionState& exception_state) {
  // The input data type must be one of the floating point types.
  // The current spec doesn't specify the operand data type constraints of tanh,
  // an issue has been filed to track it-
  // https://github.com/webmachinelearning/webnn/issues/283.
  //
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-tanh, the output tensor of
  // tanh has the same data type and dimensions as its input.
  return BuildUnaryOperator(this, exception_state,
                            MLOperator::OperatorKind::kTanh,
                            webnn::DataTypeConstraint::kFloat, input);
}

MLActivation* MLGraphBuilder::tanh(ExceptionState& exception_state) {
  // Create the tanh operator that would be used as an activation function.
  return MakeGarbageCollected<MLActivation>(this,
                                            MLOperator::OperatorKind::kTanh);
}

MLOperand* MLGraphBuilder::transpose(const MLOperand* input,
                                     const MLTransposeOptions* options,
                                     ExceptionState& exception_state) {
  // According to WebNN spec:
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-transpose,
  // When permutation is not specified, it’s set to [N-1, ..., 0], where N is
  // the rank of the input tensor.
  auto input_rank = input->Dimensions().size();
  const Vector<uint32_t> permutation =
      options->getPermutationOr(CreateDefaultPermutation(input_rank));
  auto validated_output = webnn::ValidateTransposeAndInferOutput(
      ConvertToComponentOperand(input), permutation);
  if (!validated_output.has_value()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        String::FromUTF8(validated_output.error()));
    return nullptr;
  }

  auto* transpose = MakeGarbageCollected<MLOperator>(
      this, MLOperator::OperatorKind::kTranspose, options);
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-transpose, the output
  // tensor of transpose has the same data type as its input.
  auto output = MLOperand::ValidateAndCreateOutput(
      this, ComponentOperandTypeToBlink(validated_output->data_type),
      Vector<uint32_t>(validated_output->dimensions), transpose);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  transpose->Connect({input}, {output.value()});
  return output.value();
}

MLOperand* MLGraphBuilder::where(const MLOperand* condition,
                                 const MLOperand* true_value,
                                 const MLOperand* false_value,
                                 ExceptionState& exception_state) {
  const auto validated_output = webnn::ValidateWhereAndInferOutput(
      ConvertToComponentOperand(condition),
      ConvertToComponentOperand(true_value),
      ConvertToComponentOperand(false_value));
  if (!validated_output.has_value()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        String::FromUTF8(validated_output.error()));
    return nullptr;
  }

  auto* where =
      MakeGarbageCollected<MLOperator>(this, MLOperator::OperatorKind::kWhere);
  const auto output = MLOperand::ValidateAndCreateOutput(
      this, ComponentOperandTypeToBlink(validated_output->data_type),
      Vector<uint32_t>(validated_output->dimensions), where);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  where->Connect({condition, true_value, false_value}, {output.value()});
  return output.value();
}

ScriptPromise MLGraphBuilder::build(ScriptState* script_state,
                                    const MLNamedOperands& named_outputs,
                                    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  if (g_backend_for_testing) {
    g_backend_for_testing->BuildGraphAsyncImpl(ml_context_, named_outputs,
                                               resolver);
    return promise;
  }

#if BUILDFLAG(BUILD_WEBNN_WITH_XNNPACK)
  if (ml_context_->GetDeviceType() == V8MLDeviceType::Enum::kCpu) {
    MLGraphXnnpack::ValidateAndBuildAsync(ml_context_, named_outputs, resolver);
    return promise;
  }
#endif

#if BUILDFLAG(BUILD_WEBNN_ON_CROS)
  // On ChromeOS, ML model inferencing is off-loaded to ModelLoader service.
  if (ml_context_->GetDeviceType() == V8MLDeviceType::Enum::kCpu) {
    MLGraphCrOS::ValidateAndBuildAsync(ml_context_, named_outputs, resolver);
    return promise;
  }
#endif

#if !BUILDFLAG(IS_CHROMEOS)
  // The runtime enable feature is used to disable the cross process hardware
  // acceleration by default.
  if (base::FeatureList::IsEnabled(
          webnn::features::kEnableMachineLearningNeuralNetworkService) &&
      ml_context_->GetDeviceType() == V8MLDeviceType::Enum::kGpu) {
    // Reject unsupported error on unimplemented platform when getting
    // `WebNNContext` mojo interface with BrowserInterfaceBroker's
    // GetInterface() method before creating `WebNNGraph` message pipe.
    MLContextMojo* ml_context_mojo =
        static_cast<MLContextMojo*>(ml_context_.Get());
    MLGraphMojo::ValidateAndBuildAsync(ml_context_mojo, named_outputs,
                                       resolver);
    return promise;
  }
#endif

  resolver->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNotSupportedError, "Not implemented"));
  return promise;
}

MLGraph* MLGraphBuilder::buildSync(ScriptState* script_state,
                                   const MLNamedOperands& named_outputs,
                                   ExceptionState& exception_state) {
  if (g_backend_for_testing) {
    return g_backend_for_testing->BuildGraphSyncImpl(
        script_state, ml_context_, named_outputs, exception_state);
  }

#if BUILDFLAG(BUILD_WEBNN_WITH_XNNPACK)
  if (ml_context_->GetDeviceType() == V8MLDeviceType::Enum::kCpu) {
    return MLGraphXnnpack::ValidateAndBuildSync(script_state, ml_context_,
                                                named_outputs, exception_state);
  }
#endif

#if !BUILDFLAG(IS_CHROMEOS)
  // GPU support requires a cross-process WebNN acceleration service. This
  // services is gated behind the EnableMachineLearningNeuralNetworkService
  // runtime feature.
  if (ml_context_->GetDeviceType() == V8MLDeviceType::Enum::kGpu &&
      base::FeatureList::IsEnabled(
          webnn::features::kEnableMachineLearningNeuralNetworkService)) {
    MLContextMojo* ml_context_mojo =
        static_cast<MLContextMojo*>(ml_context_.Get());
    return MLGraphMojo::ValidateAndBuildSync(script_state, ml_context_mojo,
                                             named_outputs, exception_state);
  }
#endif

  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Not implemented");
  return nullptr;
}

// static
void MLGraphBuilder::SetBackendForTesting(
    MLGraphBuilder::BackendForTesting* backend_for_testing) {
  g_backend_for_testing = backend_for_testing;
}

}  // namespace blink
