// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter_operation_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"

namespace blink {

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
    } else {
      exception_state.ThrowTypeError(
          "Failed to construct blur filter, 'stdDeviation' must be a number.");
    }
  } else {
    exception_state.ThrowTypeError(
        "Failed to construct blur filter, 'stdDeviation' required.");
  }

  return MakeGarbageCollected<BlurFilterOperation>(std_deviation);
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
      operations.Operations().push_back(
          ResolveBlur(v8_object, script_state, exception_state));
    }
  }

  return operations;
}

}  // namespace blink
