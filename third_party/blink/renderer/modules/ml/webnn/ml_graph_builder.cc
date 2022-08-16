// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"

#include "base/numerics/checked_math.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_clamp_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gemm_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pool_2d_options.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

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

absl::optional<size_t> ValidateAndCalculateByteLength(
    V8MLOperandType::Enum type,
    const Vector<int32_t>& dimensions,
    ExceptionState& exception_state) {
  if (dimensions.IsEmpty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The dimensions is empty.");
    return absl::nullopt;
  }
  base::CheckedNumeric<size_t> elements_num = 1;
  for (auto& d : dimensions) {
    if (d <= 0) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                        "All dimensions should be positive");
      return absl::nullopt;
    }
    elements_num *= d;
  }
  base::CheckedNumeric<size_t> checked_byte_length =
      elements_num * GetBytesPerElement(type);
  if (!checked_byte_length.IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The dimensions is too large.");
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
  Vector<int32_t> dimensions = desc->getDimensionsOr({1});
  if (!ValidateAndCalculateByteLength(type, dimensions, exception_state)) {
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
  Vector<int32_t> dimensions = desc->getDimensionsOr({1});
  absl::optional<size_t> expected_byte_length =
      ValidateAndCalculateByteLength(type, dimensions, exception_state);
  if (!expected_byte_length) {
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
  auto* output = MLOperand::CreateOutput(this, input->Type(),
                                         std::move(input->Dimensions()), clamp);
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
  // TODO(crbug.com/1273291): Implement this on operating systems to access
  // hardware acceleration.
  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Not implemented");
  return nullptr;
}

MLOperand* MLGraphBuilder::add(const MLOperand* a,
                               const MLOperand* b,
                               ExceptionState& exception_state) {
  // TODO(crbug.com/1273291): Implement this on operating systems to access
  // hardware acceleration.
  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Not implemented");
  return nullptr;
}

MLOperand* MLGraphBuilder::gemm(const MLOperand* a,
                                const MLOperand* b,
                                const MLGemmOptions* options,
                                ExceptionState& exception_state) {
  // TODO(crbug.com/1273291): Implement this on operating systems to access
  // hardware acceleration.
  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Not implemented");
  return nullptr;
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
  // TODO(crbug.com/1273291): Implement this on operating systems to access
  // hardware acceleration.
  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Not implemented");
  return nullptr;
}

MLOperand* MLGraphBuilder::softmax(const MLOperand* input,
                                   ExceptionState& exception_state) {
  // TODO(crbug.com/1273291): Implement this on operating systems to access
  // hardware acceleration.
  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Not implemented");
  return nullptr;
}

}  // namespace blink
