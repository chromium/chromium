// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"

#include <algorithm>

#include "base/numerics/checked_math.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_clamp_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gemm_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pool_2d_options.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

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

DOMArrayBufferView::ViewType GetArrayBufferViewType(
    V8MLOperandType::Enum operand_type) {
  switch (operand_type) {
    case V8MLOperandType::Enum::kFloat32:
      return DOMArrayBufferView::ViewType::kTypeFloat32;
    case V8MLOperandType::Enum::kFloat16:
      // Using Uint16Array for float16 is a workaround of WebNN spec issue:
      // https://github.com/webmachinelearning/webnn/issues/127
      return DOMArrayBufferView::ViewType::kTypeUint16;
    case V8MLOperandType::Enum::kInt32:
      return DOMArrayBufferView::ViewType::kTypeInt32;
    case V8MLOperandType::Enum::kUint32:
      return DOMArrayBufferView::ViewType::kTypeUint32;
    case V8MLOperandType::Enum::kInt8:
      return DOMArrayBufferView::ViewType::kTypeInt8;
    case V8MLOperandType::Enum::kUint8:
      return DOMArrayBufferView::ViewType::kTypeUint8;
  }
}

size_t GetBytesPerElement(V8MLOperandType::Enum operand_type) {
  switch (operand_type) {
    case V8MLOperandType::Enum::kFloat32:
      return sizeof(float);
    case V8MLOperandType::Enum::kFloat16:
      // Using Uint16Array for float16 is a workaround of WebNN spec issue:
      // https://github.com/webmachinelearning/webnn/issues/127
      return sizeof(uint16_t);
    case V8MLOperandType::Enum::kInt32:
      return sizeof(int32_t);
    case V8MLOperandType::Enum::kUint32:
      return sizeof(uint32_t);
    case V8MLOperandType::Enum::kInt8:
      return sizeof(int8_t);
    case V8MLOperandType::Enum::kUint8:
      return sizeof(uint8_t);
  }
}

absl::optional<size_t> ValidateAndCalculateElementsNumber(
    const Vector<uint32_t>& dimensions,
    String& error_message) {
  if (dimensions.IsEmpty()) {
    error_message = "The dimensions is empty.";
    return absl::nullopt;
  }
  base::CheckedNumeric<size_t> checked_elements_number = 1;
  for (auto& d : dimensions) {
    if (d == 0) {
      error_message = "All dimensions should be positive";
      return absl::nullopt;
    }
    checked_elements_number *= d;
  }
  if (!checked_elements_number.IsValid()) {
    error_message = "The elements number of the dimensions is too large.";
    return absl::nullopt;
  }
  return checked_elements_number.ValueOrDie();
}

absl::optional<size_t> ValidateAndCalculateByteLength(
    V8MLOperandType::Enum type,
    const Vector<uint32_t>& dimensions,
    String& error_message) {
  absl::optional<size_t> elements_num =
      ValidateAndCalculateElementsNumber(dimensions, error_message);
  if (!elements_num) {
    return absl::nullopt;
  }
  base::CheckedNumeric<size_t> checked_byte_length =
      elements_num.value() * GetBytesPerElement(type);
  if (!checked_byte_length.IsValid()) {
    error_message = "The byte length of the dimensions is too large.";
    return absl::nullopt;
  }
  return checked_byte_length.ValueOrDie();
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
  auto* output =
      MLOperand::CreateOutput(builder, a->Type(), dims_output.value(), binary);
  binary->Connect({a, b}, {output});
  return output;
}

bool CalculatePaddingForAutoPad(V8MLAutoPad::Enum autoPad,
                                const uint32_t input_size,
                                const uint32_t filter_size,
                                const int32_t stride,
                                const int32_t dilation,
                                uint32_t& padding_begin,
                                uint32_t& padding_end) {
  base::CheckedNumeric<uint32_t> checked_input_size(input_size);
  auto checked_output_size = (checked_input_size + stride - 1) / stride;
  base::CheckedNumeric<uint32_t> checked_filter_size(filter_size);
  auto checked_dilated_filter_size = (checked_filter_size - 1) * dilation + 1;
  auto checked_needed_input_size =
      (checked_output_size - 1) * stride + checked_dilated_filter_size;
  if (!checked_needed_input_size.IsValid() || !checked_input_size.IsValid()) {
    return false;
  }
  auto checked_total_padding =
      checked_needed_input_size.ValueOrDie() > checked_input_size.ValueOrDie()
          ? checked_needed_input_size - checked_input_size
          : base::MakeCheckedNum<uint32_t>(0);
  base::CheckedNumeric<uint32_t> checked_padding_begin, checked_padding_end;
  switch (autoPad) {
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
  if (!checked_padding_begin.AssignIfValid(&padding_begin) ||
      !checked_padding_end.AssignIfValid(&padding_end)) {
    return false;
  }
  return true;
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

MLOperand* MLGraphBuilder::input(String name,
                                 const MLOperandDescriptor* desc,
                                 ExceptionState& exception_state) {
  if (name.IsEmpty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The name is empty.");
    return nullptr;
  }
  V8MLOperandType::Enum type = desc->type().AsEnum();
  // If no dimensions, it represents a scalar. Set dimensions to {1}.
  Vector<uint32_t> dimensions = desc->getDimensionsOr({1});
  String error_message;
  if (!ValidateAndCalculateByteLength(type, dimensions, error_message)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "Invalid operand descriptor: " + error_message);
    return nullptr;
  }
  return MLOperand::CreateInput(this, type, std::move(dimensions),
                                std::move(name));
}

MLOperand* MLGraphBuilder::constant(const MLOperandDescriptor* desc,
                                    NotShared<DOMArrayBufferView> buffer_view,
                                    ExceptionState& exception_state) {
  if (GetArrayBufferViewType(desc->type().AsEnum()) != buffer_view->GetType()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The buffer view type doesn't match the operand type.");
    return nullptr;
  }
  V8MLOperandType::Enum type = desc->type().AsEnum();
  // If no dimensions, it represents a scalar. Set dimensions to {1}.
  Vector<uint32_t> dimensions = desc->getDimensionsOr({1});
  String error_message;
  absl::optional<size_t> expected_byte_length =
      ValidateAndCalculateByteLength(type, dimensions, error_message);
  if (!expected_byte_length) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "Invalid operand descriptor: " + error_message);
    return nullptr;
  }
  if (expected_byte_length.value() != buffer_view->byteLength()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        String::Format("The buffer view byte length (%zu) doesn't match the "
                       "expected byte length (%zu).",
                       buffer_view->byteLength(),
                       expected_byte_length.value()));
    return nullptr;
  }
  return MLOperand::CreateConstant(this, type, std::move(dimensions),
                                   buffer_view.Get());
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
  auto* output =
      MLOperand::CreateOutput(this, input->Type(), input->Dimensions(), clamp);
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
  // The input layout specifies the input layout format as follows:
  // "nchw": [batches, input_channels, height, width]
  // "nhwc": [batches, height, width, input_channels]
  bool nchw = options->inputLayout() == V8MLInputOperandLayout::Enum::kNchw;
  const uint32_t input_batches = input_shape[0];
  const uint32_t input_channels = nchw ? input_shape[1] : input_shape[3];
  const uint32_t input_height = nchw ? input_shape[2] : input_shape[1];
  const uint32_t input_width = nchw ? input_shape[3] : input_shape[2];
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
  if (options->groups() < 1) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The groups should be greater than or equal to 1.");
    return nullptr;
  }
  if (input_channels % options->groups() != 0 ||
      filter_input_channels != input_channels / options->groups()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The groups must evenly divide the input "
                                      "channels to filter input channels.");
    return nullptr;
  }
  // Validate options.padding. If not present, the values are assumed to be
  // [0,0,0,0].
  // The current WebNN spec defines the paddings as signed integer:
  // https://www.w3.org/TR/webnn/#dom-mlconv2doptions-padding
  // However, there is a proposal of using unsigned integer:
  // https://github.com/webmachinelearning/webnn/pull/294.
  // Before the change merged, the signed integers are checked_cast to
  // unsigned integers for output shape calculation.
  uint32_t padding_beginning_height = 0, padding_ending_height = 0,
           padding_beginning_width = 0, padding_ending_width = 0;
  if (options->hasPadding()) {
    if (options->padding().size() != 4) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                        "The length of padding should be 4.");
      return nullptr;
    }
    if (std::any_of(options->padding().begin(), options->padding().end(),
                    [](int32_t x) { return x < 0; })) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "All paddings should be greater than or equal to 0.");
      return nullptr;
    }
    padding_beginning_height =
        base::checked_cast<uint32_t>(options->padding()[0]);
    padding_ending_height = base::checked_cast<uint32_t>(options->padding()[1]);
    padding_beginning_width =
        base::checked_cast<uint32_t>(options->padding()[2]);
    padding_ending_width = base::checked_cast<uint32_t>(options->padding()[3]);
  }
  // Validate options.strides. If not present, the values are assumed to be
  // [1,1].
  // The current WebNN spec defines the strides as signed integer:
  // https://www.w3.org/TR/webnn/#dom-mlconv2doptions-strides
  // However, there is a proposal of using unsigned integer:
  // https://github.com/webmachinelearning/webnn/pull/294
  // Before the change merged, the signed integers are checked_cast to
  // unsigned integers for output shape calculation.
  uint32_t stride_height = 1, stride_width = 1;
  if (options->hasStrides()) {
    if (options->strides().size() != 2) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                        "The length of strides should be 2.");
      return nullptr;
    }
    if (std::any_of(options->strides().begin(), options->strides().end(),
                    [](int32_t x) { return x < 1; })) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "All strides should be greater than or equal to 1.");
      return nullptr;
    }
    stride_height = base::checked_cast<uint32_t>(options->strides()[0]);
    stride_width = base::checked_cast<uint32_t>(options->strides()[1]);
  }
  // Validate options.dilations. If not present, the values are assumed to be
  // [1,1].
  // The current WebNN spec defines the dilations as signed integer:
  // https://www.w3.org/TR/webnn/#dom-mlconv2doptions-dilations
  // However, there is a proposal of using unsigned integer:
  // https://github.com/webmachinelearning/webnn/pull/294
  // Before the change merged, the signed integers are checked_cast to
  // unsigned integers for output shape calculation.
  uint32_t dilation_height = 1, dilation_width = 1;
  if (options->hasDilations()) {
    if (options->dilations().size() != 2) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                        "The length of dilations should be 2.");
      return nullptr;
    }
    if (std::any_of(options->dilations().begin(), options->dilations().end(),
                    [](int32_t x) { return x < 1; })) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "All dilations should be greater than or equal to 1.");
      return nullptr;
    }
    dilation_height = base::checked_cast<uint32_t>(options->dilations()[0]);
    dilation_width = base::checked_cast<uint32_t>(options->dilations()[1]);
  }
  // When the options.autoPad is other than "explicit", the values in the
  // options.padding array are ignored and the explicit padding values need to
  // be calculated.
  if (options->autoPad().AsEnum() != V8MLAutoPad::Enum::kExplicit) {
    if (!CalculatePaddingForAutoPad(options->autoPad().AsEnum(), input_height,
                                    filter_height, stride_height,
                                    dilation_height, padding_beginning_height,
                                    padding_ending_height)) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "Overflow occurred when calculating "
          "the padding along the height dimension.");
      return nullptr;
    }
    if (!CalculatePaddingForAutoPad(options->autoPad().AsEnum(), input_width,
                                    filter_width, stride_width, dilation_width,
                                    padding_beginning_width,
                                    padding_ending_width)) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "Overflow occurred when calculating "
          "the padding along the width dimension.");
      return nullptr;
    }
  }
  // Calculate the output shape.
  base::CheckedNumeric<uint32_t> checked_filter_height(filter_height),
      checked_filter_width(filter_width);
  auto dilated_filter_height =
      (checked_filter_height - 1) * dilation_height + 1;
  auto dilated_filter_width = (checked_filter_width - 1) * dilation_width + 1;
  if (!dilated_filter_height.IsValid()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "Overflow occurred when calculating the dilated filter height.");
    return nullptr;
  }
  if (!dilated_filter_width.IsValid()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "Overflow occurred when calculating the dilated filter width.");
    return nullptr;
  }
  base::CheckedNumeric<uint32_t> checked_input_height(input_height),
      checked_input_width(input_width);
  auto checked_output_height =
      (checked_input_height - dilated_filter_height + padding_beginning_height +
       padding_ending_height) /
          stride_height +
      1;
  auto checked_output_width = (checked_input_width - dilated_filter_width +
                               padding_beginning_width + padding_ending_width) /
                                  stride_width +
                              1;
  uint32_t output_height, output_width;
  if (!checked_output_height.AssignIfValid(&output_height)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "Overflow occurred when calculating the output height.");
    return nullptr;
  }
  if (!checked_output_width.AssignIfValid(&output_width)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "Overflow occurred when calculating the output width.");
    return nullptr;
  }
  // The input layout specifies the output layout format as follows:
  // "nchw": [batches, output_channels, height, width]
  // "nhwc": [batches, height, width, output_channels]
  Vector<uint32_t> output_shape;
  if (nchw) {
    output_shape = {input_batches, output_channels, output_height,
                    output_width};
  } else {
    output_shape = {input_batches, output_height, output_width,
                    output_channels};
  }
  // Create conv2d operator and its output operand. Connect the conv2d operator
  // to its input and output operands.
  auto* conv2d = MakeGarbageCollected<MLOperator>(
      this, MLOperator::OperatorKind::kConv2d, options);
  HeapVector<Member<const MLOperand>> inputs = {input, filter};
  if (options->hasBias()) {
    inputs.push_back(options->bias());
  }
  auto* output = MLOperand::CreateOutput(this, input->Type(),
                                         std::move(output_shape), conv2d);
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
  auto* gemm =
      MakeGarbageCollected<MLOperator>(this, MLOperator::OperatorKind::kGemm);
  HeapVector<Member<const MLOperand>> inputs = {a, b};
  if (options->hasC()) {
    inputs.push_back(options->c());
  }
  auto* output =
      MLOperand::CreateOutput(this, a->Type(), std::move(output_shape), gemm);
  gemm->Connect(std::move(inputs), {output});
  return output;
}

MLOperand* MLGraphBuilder::averagePool2d(const MLOperand* input,
                                         const MLPool2dOptions* options,
                                         ExceptionState& exception_state) {
  // TODO(crbug.com/1273291): Implement this on operating systems to access
  // hardware acceleration.
  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Not implemented");
  return nullptr;
}

MLOperand* MLGraphBuilder::reshape(const MLOperand* input,
                                   const Vector<int32_t>& new_shape,
                                   ExceptionState& exception_state) {
  String error_message;
  absl::optional<size_t> input_elements_num =
      ValidateAndCalculateElementsNumber(input->Dimensions(), error_message);
  if (!input_elements_num) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "Invalid input operand: " + error_message);
    return nullptr;
  }
  bool has_minus1 = false;
  wtf_size_t minus1_dim_index;
  base::CheckedNumeric<size_t> checked_newshape_elements_num = 1;
  Vector<uint32_t> output_shape;
  if (new_shape.size() == 0) {
    // The empty new shape means reshaping to scalar, set output shape to {1}.
    output_shape = {1};
  } else {
    output_shape.resize(new_shape.size());
    // According to WebNN spec:
    // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-reshape, only one
    // component of new shape can be the special value of -1.
    for (wtf_size_t i = 0; i < new_shape.size(); ++i) {
      auto d = new_shape[i];
      if (d < -1 || d == 0) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kDataError,
            "The value of new shape should be positive or -1.");
        return nullptr;
      } else if (d == -1) {
        if (has_minus1) {
          exception_state.ThrowDOMException(
              DOMExceptionCode::kDataError,
              "Only one component of new shape can be -1.");
          return nullptr;
        }
        has_minus1 = true;
        minus1_dim_index = i;
      } else {
        checked_newshape_elements_num *= d;
        output_shape[i] = d;
      }
    }
  }
  size_t newshape_elements_num;
  if (!checked_newshape_elements_num.AssignIfValid(&newshape_elements_num)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The number of elements implied by new shape is too large.");
    return nullptr;
  }
  DCHECK_NE(newshape_elements_num, size_t(0));
  if (has_minus1) {
    // The size of the dimension with the value -1 is computed so that the total
    // size remains constant.
    if (input_elements_num.value() % newshape_elements_num != size_t(0)) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          String::Format(
              "The number of elements (%zu) in the input tensor can't be "
              "divided evenly by the number of elements (%zu) implied by new "
              "shape.",
              input_elements_num.value(), newshape_elements_num));
      return nullptr;
    }
    // Check whether the quotient of type size_t is in the range of dimension of
    // type uint32_t.
    if (!base::CheckDiv(input_elements_num.value(), newshape_elements_num)
             .AssignIfValid(&output_shape[minus1_dim_index])) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "The size of dimension with the value -1 is too large.");
      return nullptr;
    }
  } else {
    // The number of elements implied by new shape must be the same as the
    // number of elements in the input tensor.
    if (input_elements_num.value() != newshape_elements_num) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          String::Format(
              "The number of elements (%zu) implied by new shape doesn't match "
              "the number of elements (%zu) in the input tensor.",
              newshape_elements_num, input_elements_num.value()));
      return nullptr;
    }
  }
  auto* reshape = MakeGarbageCollected<MLOperator>(
      this, MLOperator::OperatorKind::kReshape);
  auto* output = MLOperand::CreateOutput(this, input->Type(),
                                         std::move(output_shape), reshape);
  reshape->Connect({input}, {output});
  return output;
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
  auto* output = MLOperand::CreateOutput(this, input->Type(),
                                         input->Dimensions(), softmax);
  softmax->Connect({input}, {output});
  return output;
}

}  // namespace blink
