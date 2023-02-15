// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"

#include <algorithm>

#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_clamp_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gemm_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pool_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_resample_2d_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/ml/buildflags.h"
#include "third_party/blink/renderer/modules/ml/ml.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

#if BUILDFLAG(BUILD_WEBNN_WITH_XNNPACK)
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_xnnpack.h"
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

// Broadcast the input shapes and return the output shape.
// If bidirectional is true, its behavior follows the numpy-broadcasting-rule:
// https://numpy.org/doc/stable/user/basics.broadcasting.html#general-broadcasting-rules.
// Otherwise, it unidirectionally broadcasts the lhs to the rhs.
absl::optional<Vector<uint32_t>> BroadcastShapes(
    const Vector<uint32_t>& dims_lhs,
    const Vector<uint32_t>& dims_rhs,
    bool bidirectional = true) {
  // If bidirectional is true, the rank of the output shape is the maximum rank
  // of the input shapes. Otherwise it is as the same as the rhs' rank.
  auto rank_lhs = dims_lhs.size(), rank_rhs = dims_rhs.size();
  auto rank_output = bidirectional ? std::max(rank_lhs, rank_rhs) : rank_rhs;
  Vector<uint32_t> dims_output(rank_output);
  for (wtf_size_t i = 0; i < rank_output; ++i) {
    auto dim_lhs = i < rank_lhs ? dims_lhs[rank_lhs - i - 1] : 1;
    DCHECK_GT(dim_lhs, uint32_t(0));
    auto dim_rhs = i < rank_rhs ? dims_rhs[rank_rhs - i - 1] : 1;
    DCHECK_GT(dim_rhs, uint32_t(0));
    // If bidirectional is true, two dimensions are compatible when they are
    // equal, or one of them is 1. Otherwise, two dimensions are compatible when
    // they are equal, or the lhs dimension is 1.
    if (bidirectional) {
      if (dim_lhs != dim_rhs && dim_lhs != 1 && dim_rhs != 1) {
        return absl::nullopt;
      }
    } else if (dim_lhs != dim_rhs && dim_lhs != 1) {
      return absl::nullopt;
    }
    // If bidirectional is true, for each dimension of the output tensor, its
    // size is the maximum size along that dimension of the input shapes.
    // Otherwise, its size is the same as the rhs.
    dims_output[rank_output - i - 1] =
        bidirectional ? std::max(dim_lhs, dim_rhs) : dim_rhs;
  }
  return dims_output;
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
  String error_message;
  auto* output = MLOperand::ValidateAndCreateOutput(
      builder, a->Type(), dims_output.value(), binary, error_message);
  if (!output) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      error_message);
    return nullptr;
  }
  binary->Connect({a, b}, {output});
  return output;
}

// Calculate the output size for conv2d based on WebNN spec:
// https://www.w3.org/TR/webnn/#api-mlgraphbuilder-conv2d
// Return the calculated output size if no error.
absl::optional<double> CalculateConv2dOutputSize(
    const uint32_t input_size,
    const uint32_t filter_size,
    const uint32_t beginning_padding,
    const uint32_t ending_padding,
    const uint32_t stride,
    const uint32_t dilation,
    String& error_message) {
  // Calculate the dilated filter sizes.
  auto checked_effective_filter_size =
      (base::MakeCheckedNum<uint32_t>(filter_size) - 1) * dilation + 1;
  if (!checked_effective_filter_size.IsValid()) {
    error_message = "The effective filter size is too large.";
    return absl::nullopt;
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
    error_message = "The input size is too small to fill the window.";
    return absl::nullopt;
  }

  // Check if the value is valid for rounding to uint32_t type.
  if (!checked_output_size.IsValid<uint32_t>()) {
    error_message = "The output size is too large.";
    return absl::nullopt;
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
absl::optional<FloatSize2D> ValidateAndCalculateConv2dOutputSizes(
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
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The length of padding should be 4.");
    return absl::nullopt;
  }
  uint32_t padding_beginning_height = padding[0];
  uint32_t padding_ending_height = padding[1];
  uint32_t padding_beginning_width = padding[2];
  uint32_t padding_ending_width = padding[3];

  // Validate strides and get its values.
  if (strides.size() != 2) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The length of strides should be 2.");
    return absl::nullopt;
  }
  if (std::any_of(strides.begin(), strides.end(),
                  [](uint32_t x) { return x == 0; })) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "All strides should be greater than 0.");
    return absl::nullopt;
  }
  const uint32_t stride_height = strides[0];
  const uint32_t stride_width = strides[1];

  // Validate dilations and get its values.
  if (dilations.size() != 2) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The length of dilations should be 2.");
    return absl::nullopt;
  }
  if (std::any_of(dilations.begin(), dilations.end(),
                  [](uint32_t x) { return x == 0; })) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "All dilations should be greater than 0.");
    return absl::nullopt;
  }
  const uint32_t dilation_height = dilations[0];
  const uint32_t dilation_width = dilations[1];

  // When the autoPad is other than "explicit", the values in the
  // options.padding array are ignored and the explicit padding values need to
  // be calculated.
  if (auto_pad != V8MLAutoPad::Enum::kExplicit) {
    auto padding_sizes_height = MLGraphBuilder::CalculatePaddingForAutoPad(
        auto_pad.AsEnum(), input_height, filter_height, stride_height,
        dilation_height);
    if (!padding_sizes_height) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "Overflow occurred when calculating "
          "the padding along the height dimension.");
      return absl::nullopt;
    }
    padding_beginning_height = padding_sizes_height.value().begin;
    padding_ending_height = padding_sizes_height.value().end;
    auto padding_sizes_width = MLGraphBuilder::CalculatePaddingForAutoPad(
        auto_pad.AsEnum(), input_width, filter_width, stride_width,
        dilation_width);
    if (!padding_sizes_width) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "Overflow occurred when calculating "
          "the padding along the width dimension.");
      return absl::nullopt;
    }
    padding_beginning_width = padding_sizes_width.value().begin;
    padding_ending_width = padding_sizes_width.value().end;
  }

  String error_message;
  auto float_output_height = CalculateConv2dOutputSize(
      input_height, filter_height, padding_beginning_height,
      padding_ending_height, stride_height, dilation_height, error_message);
  if (!float_output_height) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "Failed to calculate the output height: " + error_message);
    return absl::nullopt;
  }

  auto float_output_width = CalculateConv2dOutputSize(
      input_width, filter_width, padding_beginning_width, padding_ending_width,
      stride_width, dilation_width, error_message);
  if (!float_output_width) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "Failed to calculate the output width: " + error_message);
    return absl::nullopt;
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
  if (!output_sizes) {
    return nullptr;
  }
  const uint32_t floor_output_height =
      base::ClampFloor<uint32_t>(output_sizes.value().height);
  const uint32_t ceil_output_height =
      base::ClampCeil<uint32_t>(output_sizes.value().height);
  const uint32_t floor_output_width =
      base::ClampFloor<uint32_t>(output_sizes.value().width);
  const uint32_t ceil_output_width =
      base::ClampCeil<uint32_t>(output_sizes.value().width);

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
    if (std::any_of(options->outputSizes().begin(),
                    options->outputSizes().end(),
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
  String error_message;
  auto* output = MLOperand::ValidateAndCreateOutput(
      builder, input->Type(), std::move(output_shape), pool2d, error_message);
  if (!output) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      error_message);
    return nullptr;
  }
  pool2d->Connect({input}, {output});
  return output;
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
MLGraphBuilder::CalculatePaddingForAutoPad(V8MLAutoPad::Enum auto_pad,
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
    default:
      NOTREACHED();
  }
  uint32_t padding_begin, padding_end;
  if (!checked_padding_begin.AssignIfValid(&padding_begin) ||
      !checked_padding_end.AssignIfValid(&padding_end)) {
    return absl::nullopt;
  }
  return PaddingSizes({.begin = padding_begin, .end = padding_end});
}

MLOperand* MLGraphBuilder::input(String name,
                                 const MLOperandDescriptor* desc,
                                 ExceptionState& exception_state) {
  String error_message;
  // If no dimensions, it represents a scalar. Set dimensions to {1}.
  Vector<uint32_t> dimensions = desc->getDimensionsOr({1});
  auto* input_operand = MLOperand::ValidateAndCreateInput(
      this, desc->type().AsEnum(), std::move(dimensions), std::move(name),
      error_message);
  if (!input_operand) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      error_message);
    return nullptr;
  }
  return input_operand;
}

MLOperand* MLGraphBuilder::constant(const MLOperandDescriptor* desc,
                                    NotShared<DOMArrayBufferView> buffer_view,
                                    ExceptionState& exception_state) {
  String error_message;
  // If no dimensions, it represents a scalar. Set dimensions to {1}.
  Vector<uint32_t> dimensions = desc->getDimensionsOr({1});
  auto* constant_operand = MLOperand::ValidateAndCreateConstant(
      this, desc->type().AsEnum(), std::move(dimensions), buffer_view.Get(),
      error_message);
  if (!constant_operand) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      error_message);
    return nullptr;
  }
  return constant_operand;
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
  String error_message;
  auto* output = MLOperand::ValidateAndCreateOutput(
      this, input->Type(), input->Dimensions(), clamp, error_message);
  if (!output) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      error_message);
    return nullptr;
  }
  clamp->Connect({input}, {output});
  return output;
}

MLOperator* MLGraphBuilder::clamp(const MLClampOptions* options,
                                  ExceptionState& exception_state) {
  if (!ValidateClampOptions(options, exception_state)) {
    return nullptr;
  }
  // Create the clamp operator that would be used as an activation function.
  return MakeGarbageCollected<MLOperator>(
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
  if (!output_sizes) {
    return nullptr;
  }
  const uint32_t output_height =
      base::ClampFloor<uint32_t>(output_sizes.value().height);
  const uint32_t output_width =
      base::ClampFloor<uint32_t>(output_sizes.value().width);
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
  String error_message;
  auto* output = MLOperand::ValidateAndCreateOutput(
      this, input->Type(), std::move(output_shape), conv2d, error_message);
  if (!output) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      error_message);
    return nullptr;
  }
  conv2d->Connect(std::move(inputs), {output});
  return output;
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
  String error_message;
  auto* output = MLOperand::ValidateAndCreateOutput(
      this, a->Type(), std::move(output_shape), gemm, error_message);
  if (!output) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      error_message);
    return nullptr;
  }
  gemm->Connect(std::move(inputs), {output});
  return output;
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
  String error_message;
  auto* output = MLOperand::ValidateAndCreateOutput(
      this, input->Type(), input->Dimensions(), hard_swish, error_message);
  if (!output) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      error_message);
    return nullptr;
  }
  hard_swish->Connect({input}, {output});
  return output;
}

MLOperator* MLGraphBuilder::hardSwish(ExceptionState& exception_state) {
  // Create the hard-swish operator that would be used as an activation
  // function.
  return MakeGarbageCollected<MLOperator>(this,
                                          MLOperator::OperatorKind::kHardSwish);
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

MLOperand* MLGraphBuilder::relu(const MLOperand* input,
                                ExceptionState& exception_state) {
  auto* relu =
      MakeGarbageCollected<MLOperator>(this, MLOperator::OperatorKind::kRelu);
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-relu, the output tensor of
  // relu has the same type and dimensions as its input.
  String error_message;
  auto* output = MLOperand::ValidateAndCreateOutput(
      this, input->Type(), input->Dimensions(), relu, error_message);
  if (!output) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      error_message);
    return nullptr;
  }
  relu->Connect({input}, {output});
  return output;
}

MLOperator* MLGraphBuilder::relu(ExceptionState& exception_state) {
  // Create the relu operator that would be used as an activation function.
  return MakeGarbageCollected<MLOperator>(this,
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
  String error_message;
  auto* output = MLOperand::ValidateAndCreateOutput(
      this, input->Type(), std::move(output_shape), reshape, error_message);
  if (!output) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      error_message);
    return nullptr;
  }
  reshape->Connect({input}, {output});
  return output;
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
      auto* execution_context = GetContext()->GetML()->GetExecutionContext();
      if (!execution_context) {
        exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                          "Execution context is invalid.");
        return nullptr;
      }
      execution_context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kJavaScript,
          mojom::blink::ConsoleMessageLevel::kWarning,
          "When sizes and scales are both specified, scales argument is "
          "ignored."));
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
    base::CheckedNumeric<uint32_t> checked_output_height =
        input_shape[axes[0]] * scales[0];
    if (!checked_output_height.AssignIfValid(&output_shape[axes[0]])) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                        "The scale height is too large.");
      return nullptr;
    }
    base::CheckedNumeric<uint32_t> checked_output_width =
        input_shape[axes[1]] * scales[1];
    if (!checked_output_width.AssignIfValid(&output_shape[axes[1]])) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                        "The scale width is too large.");
      return nullptr;
    }
  }

  auto* resample2d = MakeGarbageCollected<MLOperator>(
      this, MLOperator::OperatorKind::kResample2d, options);
  String error_message;
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-resample2d, the output
  // tensor of resample2d has the same type as its input.
  auto* output = MLOperand::ValidateAndCreateOutput(
      this, input->Type(), std::move(output_shape), resample2d, error_message);
  if (!output) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      error_message);
    return nullptr;
  }
  resample2d->Connect({input}, {output});
  return output;
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
  String error_message;
  auto* output = MLOperand::ValidateAndCreateOutput(
      this, input->Type(), input->Dimensions(), sigmoid, error_message);
  if (!output) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      error_message);
    return nullptr;
  }
  sigmoid->Connect({input}, {output});
  return output;
}

MLOperator* MLGraphBuilder::sigmoid(ExceptionState& exception_state) {
  // Create the sigmoid operator that would be used as an activation function.
  return MakeGarbageCollected<MLOperator>(this,
                                          MLOperator::OperatorKind::kSigmoid);
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
  String error_message;
  auto* output = MLOperand::ValidateAndCreateOutput(
      this, input->Type(), input->Dimensions(), softmax, error_message);
  if (!output) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      error_message);
    return nullptr;
  }
  softmax->Connect({input}, {output});
  return output;
}

ScriptPromise MLGraphBuilder::build(ScriptState* script_state,
                                    const MLNamedOperands& named_outputs,
                                    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
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
