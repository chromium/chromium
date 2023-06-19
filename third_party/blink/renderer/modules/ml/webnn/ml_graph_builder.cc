// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"

#include <algorithm>

#include "base/numerics/checked_math.h"
#include "components/ml/webnn/graph_validation_utils.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_clamp_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_transpose_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_elu_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gemm_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_leaky_relu_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pad_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pool_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_resample_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_split_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_transpose_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/ml/buildflags.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_activation.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
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

bool IsFloatingPointType(V8MLOperandType::Enum operand_type) {
  switch (operand_type) {
    case V8MLOperandType::Enum::kFloat32:
    case V8MLOperandType::Enum::kFloat16:
      return true;
    case V8MLOperandType::Enum::kInt32:
    case V8MLOperandType::Enum::kUint32:
    case V8MLOperandType::Enum::kInt8:
    case V8MLOperandType::Enum::kUint8:
      return false;
  }
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
  if (a->Type() != b->Type()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The input types don't match.");
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
  auto output = MLOperand::ValidateAndCreateOutput(builder, a->Type(),
                                                   dims_output.value(), binary);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  binary->Connect({a, b}, {output.value()});
  return output.value();
}

MLOperand* BuildElementWiseUnary(MLGraphBuilder* builder,
                                 MLOperator::OperatorKind kind,
                                 const MLOperand* input,
                                 ExceptionState& exception_state) {
  // The input type must be one of the floating point types. Although this
  // constraint is not specified in current WebNN spec, there is a feature
  // request for that: https://github.com/webmachinelearning/webnn/issues/283
  if (!IsFloatingPointType(input->Type())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The input type must be one of the floating point types.");
    return nullptr;
  }
  // According to WebNN spec:
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-unary, the shape of the
  // output tensor is the same as the shape of input tensor.
  Vector<uint32_t> dims_output = input->Dimensions();
  auto* unary = MakeGarbageCollected<MLOperator>(builder, kind);
  auto output = MLOperand::ValidateAndCreateOutput(builder, input->Type(),
                                                   dims_output, unary);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  unary->Connect({input}, {output.value()});
  return output.value();
}

// Calculate the output size for conv2d based on WebNN spec:
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-conv2d
// Return the calculated output size if no error.
base::expected<double, String> CalculateConv2dOutputSize(
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

struct FloatSize2D {
  double height;
  double width;
};

// Validate and calculate the output spatial dimensions of conv2d given
// input sizes, filter sizes, padding, strides and dilations.
// Return the calculated output sizes in double precision floating point number
// if no errors.
base::expected<FloatSize2D, String> ValidateAndCalculateConv2dOutputSizes(
    const uint32_t input_height,
    const uint32_t input_width,
    const uint32_t filter_height,
    const uint32_t filter_width,
    const Vector<uint32_t>& padding,
    const Vector<uint32_t>& strides,
    const Vector<uint32_t>& dilations,
    const V8MLAutoPad auto_pad,
    ExceptionState& exception_state) {
  // Validate padding and get its values.
  if (padding.size() != 4) {
    return base::unexpected("The length of padding should be 4.");
  }
  uint32_t padding_beginning_height = padding[0];
  uint32_t padding_ending_height = padding[1];
  uint32_t padding_beginning_width = padding[2];
  uint32_t padding_ending_width = padding[3];

  // Validate strides and get its values.
  if (strides.size() != 2) {
    return base::unexpected("The length of strides should be 2.");
  }
  if (base::ranges::any_of(strides, [](uint32_t x) { return x == 0; })) {
    return base::unexpected("All strides should be greater than 0.");
  }
  const uint32_t stride_height = strides[0];
  const uint32_t stride_width = strides[1];

  // Validate dilations and get its values.
  if (dilations.size() != 2) {
    return base::unexpected("The length of dilations should be 2.");
  }
  if (base::ranges::any_of(dilations, [](uint32_t x) { return x == 0; })) {
    return base::unexpected("All dilations should be greater than 0.");
  }
  const uint32_t dilation_height = dilations[0];
  const uint32_t dilation_width = dilations[1];

  // When the autoPad is other than "explicit", the values in the
  // options.padding array are ignored and the explicit padding values need to
  // be calculated.
  if (auto_pad != V8MLAutoPad::Enum::kExplicit) {
    auto padding_sizes_height = MLGraphBuilder::CalculateConv2dPadding(
        auto_pad.AsEnum(), input_height, filter_height, stride_height,
        dilation_height);
    if (!padding_sizes_height) {
      return base::unexpected(
          "Overflow occurred when calculating the padding along the height "
          "dimension.");
    }
    padding_beginning_height = padding_sizes_height->begin;
    padding_ending_height = padding_sizes_height->end;
    auto padding_sizes_width = MLGraphBuilder::CalculateConv2dPadding(
        auto_pad.AsEnum(), input_width, filter_width, stride_width,
        dilation_width);
    if (!padding_sizes_width) {
      return base::unexpected(
          "Overflow occurred when calculating the padding along the width "
          "dimension.");
    }
    padding_beginning_width = padding_sizes_width->begin;
    padding_ending_width = padding_sizes_width->end;
  }

  auto float_output_height = CalculateConv2dOutputSize(
      input_height, filter_height, padding_beginning_height,
      padding_ending_height, stride_height, dilation_height);
  if (!float_output_height.has_value()) {
    return base::unexpected("Failed to calculate the output height: " +
                            float_output_height.error());
  }

  auto float_output_width = CalculateConv2dOutputSize(
      input_width, filter_width, padding_beginning_width, padding_ending_width,
      stride_width, dilation_width);
  if (!float_output_width.has_value()) {
    return base::unexpected("Failed to calculate the output width: " +
                            float_output_width.error());
  }

  return FloatSize2D({.height = float_output_height.value(),
                      .width = float_output_width.value()});
}

MLOperand* BuildPool2d(MLGraphBuilder* builder,
                       MLOperator::OperatorKind kind,
                       const MLOperand* input,
                       const MLPool2dOptions* options,
                       ExceptionState& exception_state) {
  // Validate input operand and set its sizes.
  const auto input_shape = input->Dimensions();
  if (input_shape.size() != 4) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The input should be a 4-D tensor.");
    return nullptr;
  }
  // The layout option specifies the layout format of the input tensor.
  uint32_t input_batches, input_channels, input_height, input_width;
  switch (options->layout().AsEnum()) {
    case V8MLInputOperandLayout::Enum::kNchw:
      // "nchw": [batches, channels, height, width]
      input_batches = input_shape[0];
      input_channels = input_shape[1];
      input_height = input_shape[2];
      input_width = input_shape[3];
      break;
    case V8MLInputOperandLayout::Enum::kNhwc:
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
  if (options->hasWindowDimensions()) {
    if (options->windowDimensions().size() != 2) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "The length of window dimensions should be 2.");
      return nullptr;
    }
    if (std::any_of(options->windowDimensions().begin(),
                    options->windowDimensions().end(),
                    [](uint32_t x) { return x == 0; })) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "All window dimensions should be greater than 0.");
      return nullptr;
    }
    window_height = options->windowDimensions()[0];
    window_width = options->windowDimensions()[1];
  }

  // Reuse ValidateAndCalculateConv2dOutputSizes to calculate pool2d output
  // sizes.
  const auto output_sizes = ValidateAndCalculateConv2dOutputSizes(
      input_height, input_width, window_height, window_width,
      // If padding is not present, the values are assumed to be [0,0,0,0].
      options->getPaddingOr({0, 0, 0, 0}),
      // If strides is not present, the values are assumed to be [1,1].
      options->getStridesOr({1, 1}),
      // If dilations is not present, the values are assumed to be [1, 1].
      options->getDilationsOr({1, 1}), options->autoPad(), exception_state);
  if (!output_sizes.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output_sizes.error());
    return nullptr;
  }
  const uint32_t floor_output_height =
      base::ClampFloor<uint32_t>(output_sizes->height);
  const uint32_t ceil_output_height =
      base::ClampCeil<uint32_t>(output_sizes->height);
  const uint32_t floor_output_width =
      base::ClampFloor<uint32_t>(output_sizes->width);
  const uint32_t ceil_output_width =
      base::ClampCeil<uint32_t>(output_sizes->width);

  uint32_t output_height, output_width;
  if (options->hasOutputSizes()) {
    // TODO(ningxin.hu@intel.com): report a DevTools warning message if rounding
    // type is provided but ignored.
    if (options->outputSizes().size() != 2) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "The length of output sizes should be 2.");
      return nullptr;
    }
    if (base::ranges::any_of(options->outputSizes(),
                             [](uint32_t x) { return x == 0; })) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "All output sizes should be greater than 0.");
      return nullptr;
    }
    uint32_t user_output_height = options->outputSizes()[0];
    uint32_t user_output_width = options->outputSizes()[1];

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
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          (floor_output_height == ceil_output_height &&
           floor_output_width == ceil_output_width)
              ? String::Format("The output sizes should be [%u, %u].",
                               floor_output_height, floor_output_width)
              : String::Format(
                    "The output sizes should be either [%u, %u] or [%u, %u].",
                    floor_output_height, floor_output_width, ceil_output_height,
                    ceil_output_width));
      return nullptr;
    }
  } else {
    switch (options->roundingType().AsEnum()) {
      case V8MLRoundingType::Enum::kFloor:
        output_height = floor_output_height;
        output_width = floor_output_width;
        break;
      case V8MLRoundingType::Enum::kCeil:
        output_height = ceil_output_height;
        output_width = ceil_output_width;
        break;
    }
  }
  // The layout option specifies the layout format of the output tensor.
  Vector<uint32_t> output_shape;
  switch (options->layout().AsEnum()) {
    case V8MLInputOperandLayout::Enum::kNchw:
      // "nchw": [batches, channels, height, width]
      output_shape = {input_batches, input_channels, output_height,
                      output_width};
      break;
    case V8MLInputOperandLayout::Enum::kNhwc:
      // "nhwc": [batches, height, width, channels]
      output_shape = {input_batches, output_height, output_width,
                      input_channels};
      break;
  }
  // Create pool2d operator and its output operand. Connect the pool2d operator
  // to its input and output operands.
  auto* pool2d = MakeGarbageCollected<MLOperator>(builder, kind, options);
  auto output = MLOperand::ValidateAndCreateOutput(
      builder, input->Type(), std::move(output_shape), pool2d);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  pool2d->Connect({input}, {output.value()});
  return output.value();
}

// The current WebNN spec doesn't define the calculation formula of the output
// size for resample2d. An issue has been filed to track it -
// https://github.com/webmachinelearning/webnn/issues/360.
base::expected<uint32_t, String> CalculateResample2dOutputSize(
    const uint32_t input_size,
    const float scale) {
  // Calculate the output size in double precision floating point number that
  // ensures values of type uint32_t can be exactly represented.
  // https://en.wikipedia.org/wiki/Double-precision_floating-point_format#Precision_limitations_on_integer_values
  // The max value of checked_output_size should be 3 * UINT_MAX + 1,
  // which is smaller than the max safe integer value for double type.
  auto checked_output_size = base::MakeCheckedNum<double>(input_size) * scale;

  // Check if the value is valid for rounding to uint32_t type.
  if (!checked_output_size.IsValid<uint32_t>()) {
    return base::unexpected("The scale is too large.");
  }
  const uint32_t output_size =
      base::ClampFloor<uint32_t>(double(checked_output_size.ValueOrDie()));
  if (output_size == 0) {
    return base::unexpected("The scale is too small.");
  }
  return output_size;
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
  return ml_context_;
}

// static
absl::optional<MLGraphBuilder::PaddingSizes>
MLGraphBuilder::CalculateConv2dPadding(V8MLAutoPad::Enum auto_pad,
                                       const uint32_t input_size,
                                       const uint32_t filter_size,
                                       const uint32_t stride,
                                       const uint32_t dilation) {
  auto checked_output_size =
      (base::MakeCheckedNum<uint32_t>(input_size) + stride - 1) / stride;
  auto checked_dilated_filter_size =
      (base::MakeCheckedNum<uint32_t>(filter_size) - 1) * dilation + 1;
  auto checked_needed_input_size =
      (checked_output_size - 1) * stride + checked_dilated_filter_size;
  if (!checked_needed_input_size.IsValid()) {
    return absl::nullopt;
  }
  auto checked_total_padding =
      checked_needed_input_size.ValueOrDie() > input_size
          ? checked_needed_input_size - input_size
          : base::MakeCheckedNum<uint32_t>(0);
  base::CheckedNumeric<uint32_t> checked_padding_begin, checked_padding_end;
  switch (auto_pad) {
    case V8MLAutoPad::Enum::kSameUpper:
      checked_padding_begin = checked_total_padding / 2;
      checked_padding_end = (checked_total_padding + 1) / 2;
      break;
    case V8MLAutoPad::Enum::kSameLower:
      checked_padding_begin = (checked_total_padding + 1) / 2;
      checked_padding_end = checked_total_padding / 2;
      break;
    case V8MLAutoPad::Enum::kExplicit:
      // The case has been ruled out before the function be called.
      NOTREACHED_NORETURN()
          << "Invalid auto pad value when calculating conv2d padding.";
  }
  uint32_t padding_begin, padding_end;
  if (!checked_padding_begin.AssignIfValid(&padding_begin) ||
      !checked_padding_end.AssignIfValid(&padding_end)) {
    return absl::nullopt;
  }
  return PaddingSizes({.begin = padding_begin, .end = padding_end});
}

// static
absl::optional<MLGraphBuilder::PaddingSizes>
MLGraphBuilder::CalculateConvTransposed2dPadding(
    V8MLAutoPad::Enum auto_pad,
    const uint32_t input_size,
    const uint32_t filter_size,
    const uint32_t stride,
    const uint32_t dilation,
    const uint32_t output_padding) {
  auto checked_output_size =
      base::MakeCheckedNum<uint32_t>(input_size) * stride;
  auto checked_effective_filter_size =
      (base::MakeCheckedNum<uint32_t>(filter_size) - 1) * dilation + 1;
  auto checked_total_padding = stride * (input_size - 1) +
                               checked_effective_filter_size + output_padding -
                               checked_output_size;
  base::CheckedNumeric<uint32_t> checked_padding_begin, checked_padding_end;
  switch (auto_pad) {
    case V8MLAutoPad::Enum::kSameUpper:
      checked_padding_begin = checked_total_padding / 2;
      checked_padding_end = (checked_total_padding + 1) / 2;
      break;
    case V8MLAutoPad::Enum::kSameLower:
      checked_padding_begin = (checked_total_padding + 1) / 2;
      checked_padding_end = checked_total_padding / 2;
      break;
    case V8MLAutoPad::Enum::kExplicit:
      // The case has been ruled out before the function be called.
      NOTREACHED_NORETURN()
          << "Invalid auto pad value when calculating convTranspose2d padding.";
  }
  uint32_t padding_begin, padding_end;
  if (!checked_padding_begin.AssignIfValid(&padding_begin) ||
      !checked_padding_end.AssignIfValid(&padding_end)) {
    return absl::nullopt;
  }
  return PaddingSizes({.begin = padding_begin, .end = padding_end});
}

// Calculate the output size for convTranspose2d based on WebNN spec:
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-convtranspose2d
// Return the calculated output size if no error.
base::expected<uint32_t, String> CalculateConvTranspose2dOutputSize(
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
  // Check if the checked_output_size is valid.
  if (!checked_output_size.IsValid()) {
    return base::unexpected(
        "The stride is too large or the input size is to small for padding.");
  }

  return checked_output_size.ValueOrDie();
}

// static
base::expected<MLGraphBuilder::Size2D, String>
MLGraphBuilder::ValidateAndCalculateConvTranspose2dOutputSizes(
    const uint32_t input_height,
    const uint32_t input_width,
    const uint32_t filter_height,
    const uint32_t filter_width,
    const Vector<uint32_t>& padding,
    const Vector<uint32_t>& strides,
    const Vector<uint32_t>& dilations,
    const Vector<uint32_t>& output_padding,
    const V8MLAutoPad auto_pad) {
  // Validate padding and get its values.
  if (padding.size() != 4) {
    return base::unexpected("The length of padding should be 4.");
  }
  uint32_t padding_beginning_height = padding[0];
  uint32_t padding_ending_height = padding[1];
  uint32_t padding_beginning_width = padding[2];
  uint32_t padding_ending_width = padding[3];

  // Validate strides and get its values.
  if (strides.size() != 2) {
    return base::unexpected("The length of strides should be 2.");
  }
  if (base::ranges::any_of(strides, [](uint32_t x) { return x == 0; })) {
    return base::unexpected("All strides should be greater than 0.");
  }
  const uint32_t stride_height = strides[0];
  const uint32_t stride_width = strides[1];

  // Validate dilations and get its values.
  if (dilations.size() != 2) {
    return base::unexpected("The length of dilations should be 2.");
  }
  if (base::ranges::any_of(dilations, [](uint32_t x) { return x == 0; })) {
    return base::unexpected("All dilations should be greater than 0.");
  }
  const uint32_t dilation_height = dilations[0];
  const uint32_t dilation_width = dilations[1];

  // Validate output padding and get its values.
  if (output_padding.size() != 2) {
    return base::unexpected("The length of outputPadding should be 2.");
  }
  const uint32_t outputPadding_height = output_padding[0];
  const uint32_t outputPadding_width = output_padding[1];
  if (outputPadding_height >= stride_height ||
      outputPadding_width >= stride_width) {
    return base::unexpected(
        "The output padding must be smaller than the stride along the same "
        "dimension.");
  }

  // When the autoPad is other than "explicit", the values in the
  // options.padding array are ignored and the explicit padding values need to
  // be calculated.
  if (auto_pad != V8MLAutoPad::Enum::kExplicit) {
    auto padding_sizes_height =
        MLGraphBuilder::CalculateConvTransposed2dPadding(
            auto_pad.AsEnum(), input_height, filter_height, stride_height,
            dilation_height, outputPadding_height);
    if (!padding_sizes_height) {
      return base::unexpected(
          "Overflow occurred when calculating the padding along the height "
          "dimension.");
    }
    padding_beginning_height = padding_sizes_height->begin;
    padding_ending_height = padding_sizes_height->end;
    auto padding_sizes_width = MLGraphBuilder::CalculateConvTransposed2dPadding(
        auto_pad.AsEnum(), input_width, filter_width, stride_width,
        dilation_width, outputPadding_width);
    if (!padding_sizes_width) {
      return base::unexpected(
          "Overflow occurred when calculating the padding along the width "
          "dimension.");
    }
    padding_beginning_width = padding_sizes_width->begin;
    padding_ending_width = padding_sizes_width->end;
  }

  auto output_height = CalculateConvTranspose2dOutputSize(
      input_height, filter_height, padding_beginning_height,
      padding_ending_height, stride_height, dilation_height,
      outputPadding_height);
  if (!output_height.has_value()) {
    return base::unexpected("Failed to calculate the output height: " +
                            output_height.error());
  }

  auto output_width = CalculateConvTranspose2dOutputSize(
      input_width, filter_width, padding_beginning_width, padding_ending_width,
      stride_width, dilation_width, outputPadding_width);
  if (!output_width.has_value()) {
    return base::unexpected("Failed to calculate the output width: " +
                            output_width.error());
  }

  return Size2D(
      {.height = output_height.value(), .width = output_width.value()});
}

MLOperand* MLGraphBuilder::input(String name,
                                 const MLOperandDescriptor* desc,
                                 ExceptionState& exception_state) {
  // If no dimensions, it represents a scalar. Set dimensions to {1}.
  Vector<uint32_t> dimensions = desc->getDimensionsOr({1});
  auto input_operand = MLOperand::ValidateAndCreateInput(
      this, desc->type().AsEnum(), std::move(dimensions), std::move(name));
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
  // If no dimensions, it represents a scalar. Set dimensions to {1}.
  Vector<uint32_t> dimensions = desc->getDimensionsOr({1});
  auto constant_operand = MLOperand::ValidateAndCreateConstant(
      this, desc->type().AsEnum(), std::move(dimensions), buffer_view.Get());
  if (!constant_operand.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      constant_operand.error());
    return nullptr;
  }
  return constant_operand.value();
}

MLOperand* MLGraphBuilder::concat(const HeapVector<Member<MLOperand>>& inputs,
                                  const uint32_t axis,
                                  ExceptionState& exception_state) {
  auto* concat =
      MakeGarbageCollected<MLOperator>(this, MLOperator::OperatorKind::kConcat);
  if (inputs.empty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The inputs should not be empty.");
    return nullptr;
  }
  const auto& first_input_shape = inputs[0]->Dimensions();
  const auto first_input_rank = first_input_shape.size();
  // According to WebNN spec:
  // https://www.w3.org/TR/webnn/#dom-mlgraphbuilder-concat-inputs-axis-axis,
  // the axis that the inputs concatenate along, with the value in the interval
  // [0, N-1] where N is the rank of input tensors. We just check the first
  // input rank here because we will check all inputs have same rank in the
  // following loop.
  if (axis >= first_input_rank) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The value of axis should be in the interval [0, N-1] where N is the "
        "rank of input tensors.");
    return nullptr;
  }
  const auto output_type = inputs[0]->Type();
  // The loop skips the first input to avoid repeated checks.
  for (wtf_size_t i = 1; i < inputs.size(); ++i) {
    if (inputs[i]->Type() != output_type) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                        "The input types don't match.");
      return nullptr;
    }
    // According to WebNN spec:
    // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-concat, all input tensors
    // must have the same dimension.
    if (inputs[i]->Dimensions().size() != first_input_rank) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "All input tensors must have the same dimension.");
      return nullptr;
    }
    // According to WebNN spec:
    // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-concat, all input tensors
    // must have the same shape, except for the size of the dimension to
    // concatenate on.
    for (wtf_size_t dim = 0; dim < first_input_rank; ++dim) {
      if (dim == axis ||
          inputs[i]->Dimensions()[dim] == first_input_shape[dim]) {
        continue;
      }
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "All input tensors must have the same shape, except for the size of "
          "the dimension to concatenate on.");
      return nullptr;
    }
  }
  // Calculate the output shape according to WebNN spec:
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-concat, the output tensor
  // has the same shape except on the dimension that all the inputs concatenated
  // along. The size of that dimension is computed as the sum of all the input
  // sizes of the same dimension.
  auto axis_size = base::MakeCheckedNum<uint32_t>(0);
  for (auto& input : inputs) {
    axis_size += input->Dimensions()[axis];
  }
  auto output_shape = first_input_shape;
  if (!axis_size.AssignIfValid(&output_shape[axis])) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The concatenated dimension size is too large.");
    return nullptr;
  }
  auto output = MLOperand::ValidateAndCreateOutput(this, output_type,
                                                   output_shape, concat);
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
  auto* clamp = MakeGarbageCollected<MLOperator>(
      this, MLOperator::OperatorKind::kClamp, options);
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-clamp, the output tensor of
  // clamp has the same type and dimensions as its input.
  auto output = MLOperand::ValidateAndCreateOutput(this, input->Type(),
                                                   input->Dimensions(), clamp);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  clamp->Connect({input}, {output.value()});
  return output.value();
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
  // Validate input operand and set its sizes.
  const auto input_shape = input->Dimensions();
  if (input_shape.size() != 4) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The input should be a 4-D tensor.");
    return nullptr;
  }
  // The input layout option specifies the layout format of the input tensor.
  uint32_t input_batches, input_channels, input_height, input_width;
  switch (options->inputLayout().AsEnum()) {
    case V8MLInputOperandLayout::Enum::kNchw:
      // "nchw": [batches, input_channels, height, width]
      input_batches = input_shape[0];
      input_channels = input_shape[1];
      input_height = input_shape[2];
      input_width = input_shape[3];
      break;
    case V8MLInputOperandLayout::Enum::kNhwc:
      // "nhwc": [batches, height, width, input_channels]
      input_batches = input_shape[0];
      input_height = input_shape[1];
      input_width = input_shape[2];
      input_channels = input_shape[3];
      break;
  }

  // Validate filter operand and set its sizes.
  if (filter->Type() != input->Type()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The filter type doesn't match the input type.");
    return nullptr;
  }
  const auto filter_shape = filter->Dimensions();
  if (filter_shape.size() != 4) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The filter should be a 4-D tensor.");
    return nullptr;
  }
  // The filter layout specifies the filter layout format.
  uint32_t filter_height, filter_width, output_channels, filter_input_channels;
  switch (options->filterLayout().AsEnum()) {
    case V8MLConv2dFilterOperandLayout::Enum::kHwio:
      // "hwio": [height, width, input_channels/groups, output_channels]
      filter_height = filter_shape[0];
      filter_width = filter_shape[1];
      filter_input_channels = filter_shape[2];
      output_channels = filter_shape[3];
      break;
    case V8MLConv2dFilterOperandLayout::Enum::kOhwi:
      // "ohwi": [output_channels, height, width, input_channels/groups]
      output_channels = filter_shape[0];
      filter_height = filter_shape[1];
      filter_width = filter_shape[2];
      filter_input_channels = filter_shape[3];
      break;
    case V8MLConv2dFilterOperandLayout::Enum::kIhwo:
      // "ihwo": [input_channels/groups, height, width, output_channels]
      filter_input_channels = filter_shape[0];
      filter_height = filter_shape[1];
      filter_width = filter_shape[2];
      output_channels = filter_shape[3];
      break;
    case V8MLConv2dFilterOperandLayout::Enum::kOihw:
      // "oihw": [output_channels, input_channels/groups, height, width]
      output_channels = filter_shape[0];
      filter_input_channels = filter_shape[1];
      filter_height = filter_shape[2];
      filter_width = filter_shape[3];
      break;
  }
  // Validate bias operand if it is present.
  if (options->hasBias()) {
    const auto bias_shape = options->bias()->Dimensions();
    if (bias_shape.size() != 1) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                        "The bias should be a 1-D tensor.");
      return nullptr;
    }
    if (bias_shape[0] != output_channels) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          String::Format("The bias shape should be [%u].", output_channels));
      return nullptr;
    }
    if (options->bias()->Type() != input->Type()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "The bias type doesn't match input type.");
      return nullptr;
    }
  }
  // Validate groups.
  if (options->groups() == 0) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The groups should be greater than 0.");
    return nullptr;
  }
  if (input_channels % options->groups() != 0 ||
      filter_input_channels != input_channels / options->groups()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The groups must evenly divide the input "
                                      "channels to filter input channels.");
    return nullptr;
  }

  const auto output_sizes = ValidateAndCalculateConv2dOutputSizes(
      input_height, input_width, filter_height, filter_width,
      // If padding is not present, the values are assumed to be [0,0,0,0].
      options->getPaddingOr({0, 0, 0, 0}),
      // If strides is not present, the values are assumed to be [1,1].
      options->getStridesOr({1, 1}),
      // If dilations is not present, the values are assumed to be [1, 1].
      options->getDilationsOr({1, 1}), options->autoPad(), exception_state);
  if (!output_sizes.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output_sizes.error());
    return nullptr;
  }
  const uint32_t output_height =
      base::ClampFloor<uint32_t>(output_sizes->height);
  const uint32_t output_width = base::ClampFloor<uint32_t>(output_sizes->width);
  // The input layout option specifies the layout format of the output tensor.
  Vector<uint32_t> output_shape;
  switch (options->inputLayout().AsEnum()) {
    case V8MLInputOperandLayout::Enum::kNchw:
      // "nchw": [batches, output_channels, height, width]
      output_shape = {input_batches, output_channels, output_height,
                      output_width};
      break;
    case V8MLInputOperandLayout::Enum::kNhwc:
      // "nhwc": [batches, height, width, output_channels]
      output_shape = {input_batches, output_height, output_width,
                      output_channels};
      break;
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
      this, input->Type(), std::move(output_shape), conv2d);
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
  // Validate input operand and set its sizes.
  const auto input_shape = input->Dimensions();
  if (input_shape.size() != 4) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The input should be a 4-D tensor.");
    return nullptr;
  }
  // The input layout option specifies the layout format of the input tensor.
  uint32_t input_batches, input_channels, input_height, input_width;
  switch (options->inputLayout().AsEnum()) {
    case V8MLInputOperandLayout::Enum::kNchw:
      // "nchw": [batches, input_channels, height, width]
      input_batches = input_shape[0];
      input_channels = input_shape[1];
      input_height = input_shape[2];
      input_width = input_shape[3];
      break;
    case V8MLInputOperandLayout::Enum::kNhwc:
      // "nhwc": [batches, height, width, input_channels]
      input_batches = input_shape[0];
      input_height = input_shape[1];
      input_width = input_shape[2];
      input_channels = input_shape[3];
      break;
  }

  // Validate filter operand and set its sizes.
  if (filter->Type() != input->Type()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The filter type doesn't match the input type.");
    return nullptr;
  }
  const auto filter_shape = filter->Dimensions();
  if (filter_shape.size() != 4) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The filter should be a 4-D tensor.");
    return nullptr;
  }
  // The filter layout specifies the filter layout format.
  uint32_t filter_height, filter_width, output_channels, filter_input_channels;
  switch (options->filterLayout().AsEnum()) {
    case V8MLConvTranspose2dFilterOperandLayout::Enum::kHwoi:
      // "hwoi": [height, width, output_channels, input_channels/groups]
      filter_height = filter_shape[0];
      filter_width = filter_shape[1];
      output_channels = filter_shape[2];
      filter_input_channels = filter_shape[3];
      break;
    case V8MLConvTranspose2dFilterOperandLayout::Enum::kOhwi:
      // "ohwi": [output_channels, height, width, input_channels/groups]
      output_channels = filter_shape[0];
      filter_height = filter_shape[1];
      filter_width = filter_shape[2];
      filter_input_channels = filter_shape[3];
      break;
    case V8MLConvTranspose2dFilterOperandLayout::Enum::kIohw:
      // "iohw": [input_channels/groups, output_channels, height, width]
      filter_input_channels = filter_shape[0];
      output_channels = filter_shape[1];
      filter_height = filter_shape[2];
      filter_width = filter_shape[3];
      break;
  }
  // Validate bias operand if it is present.
  if (options->hasBias()) {
    const auto bias_shape = options->bias()->Dimensions();
    if (bias_shape.size() != 1) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                        "The bias should be a 1-D tensor.");
      return nullptr;
    }
    if (bias_shape[0] != output_channels) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          String::Format("The bias shape should be [%u].", output_channels));
      return nullptr;
    }
    if (options->bias()->Type() != input->Type()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "The bias type doesn't match input type.");
      return nullptr;
    }
  }
  // Validate groups.
  if (options->groups() == 0) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The groups should be greater than 0.");
    return nullptr;
  }
  if (input_channels % options->groups() != 0 ||
      filter_input_channels != input_channels / options->groups()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The groups must evenly divide the input "
                                      "channels to filter input channels.");
    return nullptr;
  }

  // Validate and calculate output sizes.
  uint32_t output_height, output_width;
  if (options->hasOutputSizes()) {
    const auto output_sizes = options->getOutputSizesOr({});
    if (output_sizes.size() != 2) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "The length of outputSizes should be 2.");
      return nullptr;
    }
    output_height = output_sizes[0];
    output_width = output_sizes[1];
    if (output_height == 0 || output_width == 0) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "All output sizes should be greater than 0.");
      return nullptr;
    }
    // If strides is not present, the values are assumed to be [1,1].
    const auto strides = options->getStridesOr({1, 1});
    const auto calculated_output_sizes =
        ValidateAndCalculateConvTranspose2dOutputSizes(
            input_height, input_width, filter_height, filter_width,
            // If padding is not present, the values are assumed to be
            // [0,0,0,0].
            options->getPaddingOr({0, 0, 0, 0}), strides,
            // If dilations is not present, the values are assumed to be [1, 1].
            options->getDilationsOr({1, 1}),
            // Calculate the output sizes without the output padding.
            {0, 0}, options->autoPad());
    if (!calculated_output_sizes.has_value()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                        calculated_output_sizes.error());
      return nullptr;
    }
    auto calculated_output_height = calculated_output_sizes->height;
    auto calculated_output_width = calculated_output_sizes->width;
    if (output_height < calculated_output_height ||
        output_height >= calculated_output_height + strides[0]) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "The height of output sizes is invalid.");
      return nullptr;
    }
    if (output_width < calculated_output_width ||
        output_width >= calculated_output_width + strides[1]) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "The width of output sizes is invalid.");
      return nullptr;
    }
    ml_context_->LogConsoleWarning(
        "When output sizes are specified, output padding argument is ignored");
  } else {
    const auto output_sizes = ValidateAndCalculateConvTranspose2dOutputSizes(
        input_height, input_width, filter_height, filter_width,
        // If padding is not present, the values are assumed to be [0,0,0,0].
        options->getPaddingOr({0, 0, 0, 0}),
        // If strides is not present, the values are assumed to be [1,1].
        options->getStridesOr({1, 1}),
        // If dilations is not present, the values are assumed to be [1, 1].
        options->getDilationsOr({1, 1}),
        // If outputPadding is not present, the values are assumed to be [0, 0].
        options->getOutputPaddingOr({0, 0}), options->autoPad());
    if (!output_sizes.has_value()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                        output_sizes.error());
      return nullptr;
    }
    output_height = output_sizes->height;
    output_width = output_sizes->width;
  }
  // The input layout option specifies the layout format of the output tensor.
  Vector<uint32_t> output_shape;
  switch (options->inputLayout().AsEnum()) {
    case V8MLInputOperandLayout::Enum::kNchw:
      // "nchw": [batches, output_channels, height, width]
      output_shape = {input_batches, output_channels, output_height,
                      output_width};
      break;
    case V8MLInputOperandLayout::Enum::kNhwc:
      // "nhwc": [batches, height, width, output_channels]
      output_shape = {input_batches, output_height, output_width,
                      output_channels};
      break;
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
      this, input->Type(), std::move(output_shape), convTranspose2d);
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

#define BUILD_ELEMENTWISE_UNARY_OP(op, op_kind)                           \
  MLOperand* MLGraphBuilder::op(const MLOperand* input,                   \
                                ExceptionState& exception_state) {        \
    return BuildElementWiseUnary(this, MLOperator::OperatorKind::op_kind, \
                                 input, exception_state);                 \
  }

BUILD_ELEMENTWISE_UNARY_OP(abs, kAbs)
BUILD_ELEMENTWISE_UNARY_OP(ceil, kCeil)
BUILD_ELEMENTWISE_UNARY_OP(floor, kFloor)
BUILD_ELEMENTWISE_UNARY_OP(neg, kNeg)

MLOperand* MLGraphBuilder::elu(const MLOperand* input,
                               const MLEluOptions* options,
                               ExceptionState& exception_state) {
  // The current spec doesn't specify the operand type constraints of elu. An
  // issue has been filed to track it:
  // https://github.com/webmachinelearning/webnn/issues/283.
  if (!IsFloatingPointType(input->Type())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The type of input must be one of the floating point types.");
    return nullptr;
  }
  // The current spec doesn't restrict the value of alpha. An issue has been
  // filed to track it: https://github.com/webmachinelearning/webnn/issues/383
  if (options->alpha() <= 0.0f) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The value of alpha must be greater than 0.");
    return nullptr;
  }
  auto* elu = MakeGarbageCollected<MLOperator>(
      this, MLOperator::OperatorKind::kElu, options);
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-elu, the output tensor of
  // elu has the same type and dimensions as its input.
  auto output = MLOperand::ValidateAndCreateOutput(this, input->Type(),
                                                   input->Dimensions(), elu);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  elu->Connect({input}, {output.value()});
  return output.value();
}

MLActivation* MLGraphBuilder::elu(const MLEluOptions* options,
                                  ExceptionState& exception_state) {
  // Create the elu operator that would be used as an activation
  // function.
  return MakeGarbageCollected<MLActivation>(
      this, MLOperator::OperatorKind::kElu, options);
}

MLOperand* MLGraphBuilder::gemm(const MLOperand* a,
                                const MLOperand* b,
                                const MLGemmOptions* options,
                                ExceptionState& exception_state) {
  if (a->Type() != b->Type()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The types of first two inputs don't match.");
    return nullptr;
  }
  // According to WebNN spec:
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-gemm, the first input 2-D
  // tensor with shape [M, K] if aTranspose is false, or [K, M] if aTranspose is
  // true.
  auto shape_a = a->Dimensions();
  if (shape_a.size() != 2) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The first input must be a 2-D tensor.");
    return nullptr;
  }
  if (options->aTranspose()) {
    shape_a.Reverse();
  }
  // The second input 2-D tensor with shape [K, N] if bTranspose is false, or
  // [N, K] if bTranspose is true.
  auto shape_b = b->Dimensions();
  if (shape_b.size() != 2) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The second input must be a 2-D tensor.");
    return nullptr;
  }
  if (options->bTranspose()) {
    shape_b.Reverse();
  }
  // The number of columns in the first matrix must be equal to the number of
  // rows in the second matrix.
  if (shape_a[1] != shape_b[0]) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        String::Format(
            "The number of columns (%u) in the %sfirst matrix isn't equal to "
            "the number of rows (%u) in the %ssecond matrix.",
            shape_a[1], options->aTranspose() ? "transposed " : "", shape_b[0],
            options->bTranspose() ? "transposed " : ""));
    return nullptr;
  };
  // The output is 2-D tensor of shape [M, N].
  Vector<uint32_t> output_shape = {shape_a[0], shape_b[1]};
  // The third input tensor c is either a scalar, or of the shape that is
  // unidirectionally broadcastable to the output shape [M, N].
  if (options->hasC()) {
    const auto* c = options->c();
    if (c->Type() != a->Type()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "The third input type doesn't match other inputs' type.");
      return nullptr;
    }
    const auto shape_c = options->c()->Dimensions();
    if (shape_c.size() > 2) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "The third input tensor should be either a scalar or a 2-D tensor.");
      return nullptr;
    }
    if (!BroadcastShapes(shape_c, output_shape, false)) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "The third input tensor isn't unidirectionally broadcastable to the "
          "output tensor.");
      return nullptr;
    }
  }
  auto* gemm = MakeGarbageCollected<MLOperator>(
      this, MLOperator::OperatorKind::kGemm, options);
  HeapVector<Member<const MLOperand>> inputs = {a, b};
  if (options->hasC()) {
    inputs.push_back(options->c());
  }
  auto output = MLOperand::ValidateAndCreateOutput(
      this, a->Type(), std::move(output_shape), gemm);
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
  // The input type must be one of the floating point types. Although this
  // constraint is not specified in current WebNN spec, there is a feature
  // request for that: https://github.com/webmachinelearning/webnn/issues/283
  if (!IsFloatingPointType(input->Type())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The input type must be one of the floating point types.");
    return nullptr;
  }
  auto* hard_swish = MakeGarbageCollected<MLOperator>(
      this, MLOperator::OperatorKind::kHardSwish);
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-hard-swish, the output
  // tensor of hard-swish has the same type and dimensions as its input.
  auto output = MLOperand::ValidateAndCreateOutput(
      this, input->Type(), input->Dimensions(), hard_swish);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  hard_swish->Connect({input}, {output.value()});
  return output.value();
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
  auto* leaky_relu = MakeGarbageCollected<MLOperator>(
      this, MLOperator::OperatorKind::kLeakyRelu, options);
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-relu, the output tensor of
  // relu has the same type and dimensions as its input.
  auto output = MLOperand::ValidateAndCreateOutput(
      this, input->Type(), input->Dimensions(), leaky_relu);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  leaky_relu->Connect({input}, {output.value()});
  return output.value();
}

MLActivation* MLGraphBuilder::leakyRelu(const MLLeakyReluOptions* options,
                                        ExceptionState& exception_state) {
  // Create the leakyRelu operator that would be used as an activation
  // function.
  return MakeGarbageCollected<MLActivation>(
      this, MLOperator::OperatorKind::kLeakyRelu, options);
}

MLOperand* MLGraphBuilder::pad(const MLOperand* input,
                               const Vector<uint32_t>& beginning_padding,
                               const Vector<uint32_t>& ending_padding,
                               const MLPadOptions* options,
                               ExceptionState& exception_state) {
  const auto input_rank = input->Dimensions().size();
  if (beginning_padding.size() != input_rank) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The length of beginningPadding must be "
                                      "equal to the rank of the input tensor.");
    return nullptr;
  }
  if (ending_padding.size() != input_rank) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The length of endingPadding must be "
                                      "equal to the rank of the input tensor.");
    return nullptr;
  }

  if (options->mode().AsEnum() != V8MLPaddingMode::Enum::kConstant &&
      fabs(options->value() - 0.0f) > std::numeric_limits<float>::epsilon()) {
    ml_context_->LogConsoleWarning(
        "The pad value is ignored unless the options.mode is set to "
        "constant.");
  }

  // Each dimension of the output tensor can be calculated as follow:
  // output_size = beginning_padding + input_size + ending_padding.
  Vector<uint32_t> output_shape(input_rank);
  for (wtf_size_t i = 0; i < input_rank; i++) {
    auto checked_output_size =
        base::MakeCheckedNum<uint32_t>(input->Dimensions()[i]) +
        beginning_padding[i] + ending_padding[i];
    if (!checked_output_size.AssignIfValid(&output_shape[i])) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          String::Format("The padding of dimension (%u) is too large.", i));
      return nullptr;
    }
  }

  auto* pad = MakeGarbageCollected<MLPadOperator>(this, beginning_padding,
                                                  ending_padding, options);
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-pad, the output
  // tensor of pad has the same type as its input.
  auto output = MLOperand::ValidateAndCreateOutput(
      this, input->Type(), std::move(output_shape), pad);
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
  if (input->Type() != slope->Type()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The type of slope doesn't match the type of input.");
    return nullptr;
  }
  if (!IsFloatingPointType(input->Type())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The type of input and slope must be one of the floating point types.");
    return nullptr;
  }
  // BroadcastShape unidirectionally broadcasts the slope->Dimensions() to the
  // input->Dimensions().
  if (!BroadcastShapes(slope->Dimensions(), input->Dimensions(), false)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The shape of slope is not broadcastable to the shape of input.");
    return nullptr;
  }
  auto* prelu =
      MakeGarbageCollected<MLOperator>(this, MLOperator::OperatorKind::kPRelu);
  auto output = MLOperand::ValidateAndCreateOutput(this, input->Type(),
                                                   input->Dimensions(), prelu);
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
  auto* relu =
      MakeGarbageCollected<MLOperator>(this, MLOperator::OperatorKind::kRelu);
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-relu, the output tensor of
  // relu has the same type and dimensions as its input.
  auto output = MLOperand::ValidateAndCreateOutput(this, input->Type(),
                                                   input->Dimensions(), relu);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  relu->Connect({input}, {output.value()});
  return output.value();
}

MLActivation* MLGraphBuilder::relu(ExceptionState& exception_state) {
  // Create the relu operator that would be used as an activation function.
  return MakeGarbageCollected<MLActivation>(this,
                                            MLOperator::OperatorKind::kRelu);
}

MLOperand* MLGraphBuilder::reshape(
    const MLOperand* input,
    const Vector<absl::optional<uint32_t>>& new_shape,
    ExceptionState& exception_state) {
  absl::optional<wtf_size_t> null_dim_index = absl::nullopt;
  base::CheckedNumeric<size_t> checked_newshape_number_of_elements = 1;
  Vector<uint32_t> output_shape;
  if (new_shape.size() == 0) {
    // The empty new shape means reshaping to scalar, set output shape to {1}.
    output_shape = {1};
  } else {
    output_shape.resize(new_shape.size());
    // According to WebNN spec:
    // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-reshape, only one
    // component of new shape can be the special value of null.
    for (wtf_size_t i = 0; i < new_shape.size(); ++i) {
      auto dim = new_shape[i];
      if (!dim) {
        if (null_dim_index) {
          exception_state.ThrowDOMException(
              DOMExceptionCode::kDataError,
              "Only one component of new shape can be null.");
          return nullptr;
        }
        null_dim_index = i;
      } else {
        if (dim.value() == 0) {
          exception_state.ThrowDOMException(
              DOMExceptionCode::kDataError,
              "The value of new shape should not be 0.");
          return nullptr;
        }
        checked_newshape_number_of_elements *= dim.value();
        output_shape[i] = dim.value();
      }
    }
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
  if (null_dim_index) {
    // The size of the dimension with the value of null is computed so that the
    // total size remains constant.
    if (input->NumberOfElements() % newshape_number_of_elements != size_t(0)) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          String::Format(
              "The number of elements (%zu) in the input tensor can't be "
              "divided evenly by the number of elements (%zu) implied by new "
              "shape.",
              input->NumberOfElements(), newshape_number_of_elements));
      return nullptr;
    }
    // Check whether the quotient of type size_t is in the range of dimension of
    // type uint32_t.
    if (!base::CheckDiv(input->NumberOfElements(), newshape_number_of_elements)
             .AssignIfValid(&output_shape[null_dim_index.value()])) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "The size of dimension with the value null is too large.");
      return nullptr;
    }
  } else {
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
  }
  auto* reshape = MakeGarbageCollected<MLOperator>(
      this, MLOperator::OperatorKind::kReshape);
  auto output = MLOperand::ValidateAndCreateOutput(
      this, input->Type(), std::move(output_shape), reshape);
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
  // According to WebNN spec:
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-resample2d, the input
  // must be a 4-D tensor.
  const auto input_shape = input->Dimensions();
  if (input_shape.size() != 4) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The input must be a 4-D tensor.");
    return nullptr;
  }

  const auto axes = options->getAxesOr({2, 3});
  if (axes.size() != 2) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The length of axes should be 2.");
    return nullptr;
  } else if (!((axes[0] == 0 && axes[1] == 1) ||
               (axes[0] == 1 && axes[1] == 2) ||
               (axes[0] == 2 && axes[1] == 3))) {
    // According to WebNN spec:
    // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-resample2d,
    // the valid values in the sequence are [0, 1], [1, 2] or [2, 3].
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The values of axes are invalid.");
    return nullptr;
  }

  Vector<uint32_t> output_shape(input_shape);
  if (options->hasSizes()) {
    if (options->hasScales()) {
      ml_context_->LogConsoleWarning(
          "When sizes and scales are both specified, scales argument is "
          "ignored.");
    }
    if (options->sizes().size() != 2) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                        "The length of sizes should be 2.");
      return nullptr;
    } else if (std::any_of(options->sizes().begin(), options->sizes().end(),
                           [](uint32_t x) { return x == 0; })) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                        "All sizes should be greater than 0.");
      return nullptr;
    }
    output_shape[axes[0]] = options->sizes()[0];
    output_shape[axes[1]] = options->sizes()[1];
  } else {
    const auto scales = options->getScalesOr({1.0f, 1.0f});
    if (scales.size() != 2) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                        "The length of scales should be 2.");
      return nullptr;
    } else if (std::any_of(scales.begin(), scales.end(),
                           [](float x) { return x <= 0.0f; })) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                        "All scales should be greater than 0.");
      return nullptr;
    }
    auto output_height =
        CalculateResample2dOutputSize(input_shape[axes[0]], scales[0]);
    if (!output_height.has_value()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "Failed to calculate the output height: " + output_height.error());
      return nullptr;
    }
    output_shape[axes[0]] = output_height.value();

    auto output_width =
        CalculateResample2dOutputSize(input_shape[axes[1]], scales[1]);
    if (!output_width.has_value()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "Failed to calculate the output width: " + output_width.error());
      return nullptr;
    }
    output_shape[axes[1]] = output_width.value();
  }
  auto* resample2d = MakeGarbageCollected<MLOperator>(
      this, MLOperator::OperatorKind::kResample2d, options);
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-resample2d, the output
  // tensor of resample2d has the same type as its input.
  auto output = MLOperand::ValidateAndCreateOutput(
      this, input->Type(), std::move(output_shape), resample2d);
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
  auto* sigmoid = MakeGarbageCollected<MLOperator>(
      this, MLOperator::OperatorKind::kSigmoid);
  // According to WebNN spec
  // https://webmachinelearning.github.io/webnn/#api-mlgraphbuilder-sigmoid, the
  // output tensor of sigmoid has the same type and dimensions as its input.
  // And the input type must be one of the floating point types.
  if (!IsFloatingPointType(input->Type())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The input type must be one of the floating point types.");
    return nullptr;
  }
  auto output = MLOperand::ValidateAndCreateOutput(
      this, input->Type(), input->Dimensions(), sigmoid);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  sigmoid->Connect({input}, {output.value()});
  return output.value();
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
  const auto input_rank = input->Dimensions().size();
  if (starts.size() != input_rank) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The length of starts must be "
                                      "equal to the rank of the input tensor.");
    return nullptr;
  }
  if (sizes.size() != input_rank) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The length of sizes must be "
                                      "equal to the rank of the input tensor.");
    return nullptr;
  }

  for (wtf_size_t i = 0; i < input_rank; i++) {
    if (starts[i] >= input->Dimensions()[i]) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          String::Format("For dimension (%u): the starting index to slice must "
                         "be less than input size (%u).",
                         i, input->Dimensions()[i]));
      return nullptr;
    }
    // WebNN plans to allow 0 size dimensions and an issue has been filed to
    // track it: https://github.com/webmachinelearning/webnn/issues/391.
    if (sizes[i] == 0) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          String::Format("For dimension (%u): the number of elements to slice "
                         "must not be 0.",
                         i));
      return nullptr;
    }
    auto checked_ending_index =
        base::MakeCheckedNum<uint32_t>(starts[i]) + sizes[i];
    if (!checked_ending_index.IsValid<uint32_t>()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          String::Format(
              "For dimension (%u): the ending index to slice is too large.",
              i));
      return nullptr;
    }
    if (checked_ending_index.ValueOrDie() > input->Dimensions()[i]) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          String::Format("For dimension (%u): the ending index to slice must "
                         "not be greater "
                         "than input size (%u).",
                         i, input->Dimensions()[i]));
      return nullptr;
    }
  }

  auto* slice = MakeGarbageCollected<MLSliceOperator>(this, starts, sizes);
  auto output =
      MLOperand::ValidateAndCreateOutput(this, input->Type(), sizes, slice);
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
  // According to WebNN spec:
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-softmax, The input must be
  // a 2-D tensor.
  if (input->Dimensions().size() != 2) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The input must be a 2-D tensor.");
    return nullptr;
  }
  // The input type must be one of the floating point types.
  if (!IsFloatingPointType(input->Type())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The input type must be one of the floating point types.");
    return nullptr;
  }
  auto* softmax = MakeGarbageCollected<MLOperator>(
      this, MLOperator::OperatorKind::kSoftmax);
  // The output tensor has the same shape as the input tensor.
  auto output = MLOperand::ValidateAndCreateOutput(
      this, input->Type(), input->Dimensions(), softmax);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  softmax->Connect({input}, {output.value()});
  return output.value();
}

HeapVector<Member<const MLOperand>> MLGraphBuilder::split(
    const MLOperand* input,
    const uint32_t splits,
    const MLSplitOptions* options,
    ExceptionState& exception_state) {
  const auto& input_shape = input->Dimensions();
  const auto input_rank = input_shape.size();
  const auto axis = options->axis();
  // According to WebNN spec:
  // https://www.w3.org/TR/webnn/#dom-mlsplitoptions-axis, the axis must be in
  // the range [0, N-1] where N is the rank of input tensor.
  //
  // TODO(crbug.com/1273291): Consider adding helpers for ValidateAxis and
  // ValidateAxes functions to optimize the code.
  if (axis > input_rank - 1) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The axis must be in the range [0, N-1] "
                                      "where N is the rank of input tensor.");
    return {};
  }
  if (splits == 0) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The splits must be greater than 0.");
    return {};
  }
  if (input_shape[axis] % splits != 0) {
    // According to WebNN spec:
    // https://www.w3.org/TR/webnn/#dom-mlgraphbuilder-split-input-splits-options-splits,
    // the splits specifies the number of output tensors along the axis. The
    // number must evenly divide the dimension size of input along options.axis.
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The splits must evenly divide the dimension size of input along "
        "options.axis.");
    return {};
  }

  auto output_shape = input_shape;
  output_shape[axis] = input_shape[axis] / splits;
  auto* split = MakeGarbageCollected<MLSplitOperator>(this, splits, options);
  HeapVector<Member<const MLOperand>> outputs;
  for (uint32_t i = 0; i < splits; ++i) {
    auto output = MLOperand::ValidateAndCreateOutput(this, input->Type(),
                                                     output_shape, split);
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
  const auto& input_shape = input->Dimensions();
  const auto input_rank = input_shape.size();
  const auto axis = options->axis();
  // According to WebNN spec:
  // https://www.w3.org/TR/webnn/#dom-mlsplitoptions-axis, the axis must be in
  // the range [0, N-1] where N is the rank of input tensor.
  //
  // TODO(crbug.com/1273291): Consider adding helpers for ValidateAxis and
  // ValidateAxes functions to optimize the code.
  if (axis > input_rank - 1) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The axis must be in the range [0, N-1] "
                                      "where N is the rank of input tensor.");
    return {};
  }
  auto checked_splits_sum = base::MakeCheckedNum<uint32_t>(0);
  for (auto split_size : splits) {
    checked_splits_sum += split_size;
  }
  if (!checked_splits_sum.IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The values of splits are too large.");
    return {};
  }
  if (checked_splits_sum.ValueOrDie() != input_shape[axis]) {
    // According to WebNN spec:
    // https://www.w3.org/TR/webnn/#dom-mlgraphbuilder-split-input-splits-options-splits,
    // the splits parameter specifies the sizes of each output tensor along the
    // options.axis. The sum of sizes must equal to the dimension size of input
    // along options.axis.
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The sum of split sizes must equal to the dimension size of input "
        "along options.axis.");
    return {};
  }

  auto* split = MakeGarbageCollected<MLSplitOperator>(this, splits, options);
  HeapVector<Member<const MLOperand>> outputs;
  for (auto split_size : splits) {
    auto output_shape = input_shape;
    output_shape[axis] = split_size;
    auto output = MLOperand::ValidateAndCreateOutput(this, input->Type(),
                                                     output_shape, split);
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
  // The input type must be one of the floating point types.
  // The current spec doesn't specify the operand type constraints of tanh, an
  // issue has been filed to track it-
  // https://github.com/webmachinelearning/webnn/issues/283.
  if (!IsFloatingPointType(input->Type())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The input type must be one of the floating point types.");
    return nullptr;
  }
  auto* tanh =
      MakeGarbageCollected<MLOperator>(this, MLOperator::OperatorKind::kTanh);
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-tanh, the output tensor of
  // tanh has the same type and dimensions as its input.
  auto output = MLOperand::ValidateAndCreateOutput(this, input->Type(),
                                                   input->Dimensions(), tanh);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  tanh->Connect({input}, {output.value()});
  return output.value();
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
  Vector<uint32_t> default_permutation(input_rank);
  for (wtf_size_t i = 0; i < input_rank - 1; i++) {
    default_permutation[i] = input_rank - 1 - i;
  }
  const Vector<uint32_t> permutation =
      options->getPermutationOr(std::move(default_permutation));
  if (permutation.size() != input_rank) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The number of values in permutation must be the same as the rank "
        "of the input tensor.");
    return nullptr;
  }

  if (base::ranges::any_of(permutation, [input_rank](uint32_t axis) {
        return base::MakeStrictNum(axis) >= input_rank;
      })) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        String::Format(
            "The values in permutation must be within the range from 0 "
            "to (%u).",
            input_rank - 1));
    return nullptr;
  }

  if (permutation.size() !=
      std::set<uint32_t>(permutation.begin(), permutation.end()).size()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "Two or more values are same in the permutation sequence.");
    return nullptr;
  }

  Vector<uint32_t> output_shape(input_rank);
  for (wtf_size_t i = 0; i < input_rank; ++i) {
    output_shape[i] = input->Dimensions()[permutation[i]];
  }
  auto* transpose = MakeGarbageCollected<MLOperator>(
      this, MLOperator::OperatorKind::kTranspose, options);
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-transpose, the output
  // tensor of transpose has the same type as its input.
  auto output = MLOperand::ValidateAndCreateOutput(
      this, input->Type(), std::move(output_shape), transpose);
  if (!output.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      output.error());
    return nullptr;
  }
  transpose->Connect({input}, {output.value()});
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
  if (ml_context_->GetDevicePreference() == V8MLDevicePreference::Enum::kAuto ||
      ml_context_->GetDevicePreference() == V8MLDevicePreference::Enum::kCpu) {
    MLGraphXnnpack::ValidateAndBuildAsync(ml_context_, named_outputs, resolver);
    return promise;
  }
#endif

#if BUILDFLAG(BUILD_WEBNN_ON_CROS)
  // On ChromeOS, ML model inferencing is off-loaded to ModelLoader service.
  if (ml_context_->GetDevicePreference() == V8MLDevicePreference::Enum::kAuto ||
      ml_context_->GetDevicePreference() == V8MLDevicePreference::Enum::kCpu) {
    MLGraphCrOS::ValidateAndBuildAsync(ml_context_, named_outputs, resolver);
    return promise;
  }
#endif

#if !BUILDFLAG(IS_CHROMEOS)
  // The runtime enable feature is used to disable the cross process hardware
  // acceleration by default.
  if (base::FeatureList::IsEnabled(
          blink::features::kEnableMachineLearningNeuralNetworkService)) {
    // Reject unsupported error on unimplemented platform when getting
    // `WebNNContext` mojo interface with BrowserInterfaceBroker's
    // GetInterface() method before creating `WebNNGraph` message pipe.
    MLGraphMojo::ValidateAndBuildAsync(ml_context_, named_outputs, resolver);
    return promise;
  }
#endif

  resolver->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNotSupportedError, "Not implemented"));
  return promise;
}

MLGraph* MLGraphBuilder::buildSync(const MLNamedOperands& named_outputs,
                                   ExceptionState& exception_state) {
  if (g_backend_for_testing) {
    return g_backend_for_testing->BuildGraphSyncImpl(ml_context_, named_outputs,
                                                     exception_state);
  }

#if BUILDFLAG(BUILD_WEBNN_WITH_XNNPACK)
  if (ml_context_->GetDevicePreference() == V8MLDevicePreference::Enum::kAuto ||
      ml_context_->GetDevicePreference() == V8MLDevicePreference::Enum::kCpu) {
    return MLGraphXnnpack::ValidateAndBuildSync(ml_context_, named_outputs,
                                                exception_state);
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
