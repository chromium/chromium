// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter_operation_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"

namespace blink {

namespace {
double GetDoubleValueOrZero(v8::Local<v8::Object> v8_object,
                            WTF::String key,
                            ScriptState* script_state) {
  v8::Local<v8::Value> v8_value;
  if (!v8_object
           ->Get(script_state->GetContext(),
                 V8String(script_state->GetIsolate(), key))
           .ToLocal(&v8_value) ||
      !v8_value->IsNumber()) {
    return 0;
  }

  double result = v8_value.As<v8::Number>()->Value();
  if (!std::isfinite(result))
    return 0;
  return result;
}

String GetStringValue(v8::Local<v8::Object> v8_object,
                      WTF::String key,
                      ScriptState* script_state) {
  v8::Local<v8::Value> v8_type;
  if (!v8_object
           ->Get(script_state->GetContext(),
                 V8String(script_state->GetIsolate(), "type"))
           .ToLocal(&v8_type))
    return String();
  return ToCoreStringWithUndefinedOrNullCheck(v8_type);
}
}  // namespace

BlurFilterOperation* ResolveBlur(v8::Local<v8::Object> v8_filter_object,
                                 ScriptState* script_state,
                                 ExceptionState& exception_state) {
  Length std_deviation = Length::Fixed(0);
  v8::Local<v8::Value> v8_std_deviation;
  if (v8_filter_object
          ->Get(script_state->GetContext(),
                V8String(script_state->GetIsolate(), "stdDeviation"))
          .ToLocal(&v8_std_deviation)) {
    if (v8_std_deviation->IsNumber()) {
      std_deviation = Length::Fixed(v8_std_deviation.As<v8::Number>()->Value());
    } else if (v8_std_deviation->IsUndefined()) {
      exception_state.ThrowTypeError(
          "Failed to construct blur filter, 'stdDeviation' required.");
      return nullptr;
    } else {
      exception_state.ThrowTypeError(
          "Failed to construct blur filter, 'stdDeviation' must be a number.");
      return nullptr;
    }
  } else {
    exception_state.ThrowTypeError(
        "Failed to construct blur filter, 'stdDeviation' required.");
    return nullptr;
  }

  return MakeGarbageCollected<BlurFilterOperation>(std_deviation);
}

ColorMatrixFilterOperation* ResolveColorMatrix(
    v8::Local<v8::Object> v8_filter_object,
    ScriptState* script_state,
    ExceptionState& exception_state) {
  v8::Local<v8::Value> v8_value;
  v8::Local<v8::Array> v8_array;
  if (v8_filter_object
          ->Get(script_state->GetContext(),
                V8String(script_state->GetIsolate(), "values"))
          .ToLocal(&v8_value)) {
    if (!v8_value->IsArray()) {
      exception_state.ThrowTypeError(
          "Failed to construct color matrix filter, 'values' must be an array "
          "of 20 numbers.");
      return nullptr;
    }
    v8_array = v8_value.As<v8::Array>();
  }

  // Color matrices are 4x5, so the input must be of length 20.
  // https://developer.mozilla.org/en-US/docs/Web/SVG/Element/feColorMatrix
  const int length = 20;
  if (v8_array->Length() != length) {
    exception_state.ThrowTypeError(
        "Failed to construct color matrix filter, 'values' array must have 20 "
        "numbers.");
    return nullptr;
  }

  Vector<float> values;
  values.ReserveInitialCapacity(length);
  for (uint32_t i = 0; i < length; ++i) {
    if (!v8_array->Get(script_state->GetContext(), i).ToLocal(&v8_value) ||
        !v8_value->IsNumber()) {
      exception_state.ThrowTypeError(
          "Failed to construct color matrix filter, 'values' array must be "
          "numbers.");
      return nullptr;
    }
    const float value = v8_value.As<v8::Number>()->Value();
    if (!std::isfinite(value)) {
      exception_state.ThrowTypeError(
          "Failed to construct color matrix filter, 'values' array must have "
          "finite values.");
      return nullptr;
    }
    values.push_back(value);
  }

  return MakeGarbageCollected<ColorMatrixFilterOperation>(
      values, FilterOperation::COLOR_MATRIX);
}

FilterOperations CanvasFilterOperationResolver::CreateFilterOperations(
    ScriptState* script_state,
    HeapVector<Member<CanvasFilterDictionary>> filters,
    ExceptionState& exception_state) {
  FilterOperations operations;
  v8::Local<v8::Context> context = script_state->GetContext();

  for (auto filter : filters) {
    v8::Local<v8::Object> v8_object;
    if (filter->hasBlur() &&
        filter->blur().V8Value()->ToObject(context).ToLocal(&v8_object)) {
      if (auto* blur_operation =
              ResolveBlur(v8_object, script_state, exception_state)) {
        operations.Operations().push_back(blur_operation);
      }
    }
    if (filter->hasColorMatrix() &&
        filter->colorMatrix().V8Value()->ToObject(context).ToLocal(
            &v8_object)) {
      String type = GetStringValue(v8_object, "type", script_state);
      if (type == "hueRotate") {
        double amount = GetDoubleValueOrZero(v8_object, "values", script_state);
        operations.Operations().push_back(
            MakeGarbageCollected<BasicColorMatrixFilterOperation>(
                amount, FilterOperation::HUE_ROTATE));
      } else if (type == "saturate") {
        double amount = GetDoubleValueOrZero(v8_object, "values", script_state);
        operations.Operations().push_back(
            MakeGarbageCollected<BasicColorMatrixFilterOperation>(
                amount, FilterOperation::SATURATE));
      } else if (type == "luminanceToAlpha") {
        operations.Operations().push_back(
            MakeGarbageCollected<BasicColorMatrixFilterOperation>(
                0, FilterOperation::LUMINANCE_TO_ALPHA));
      } else if (auto* color_matrix_operation = ResolveColorMatrix(
                     v8_object, script_state, exception_state)) {
        operations.Operations().push_back(color_matrix_operation);
      }
    }
  }

  return operations;
}

}  // namespace blink
