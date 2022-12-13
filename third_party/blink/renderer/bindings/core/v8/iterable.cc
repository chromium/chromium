// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/iterable.h"

#include <array>

#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"

namespace blink {
namespace bindings {

v8::Local<v8::Object> ESCreateIterResultObject(ScriptState* script_state,
                                               bool done,
                                               v8::Local<v8::Value> value) {
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> result = v8::Object::New(isolate);
  result
      ->CreateDataProperty(context, V8String(isolate, "done"),
                           v8::Boolean::New(isolate, done))
      .ToChecked();
  result->CreateDataProperty(context, V8String(isolate, "value"), value)
      .ToChecked();
  return result;
}

v8::Local<v8::Object> ESCreateIterResultObject(ScriptState* script_state,
                                               bool done,
                                               v8::Local<v8::Value> item1,
                                               v8::Local<v8::Value> item2) {
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Value> items[] = {item1, item2};
  v8::Local<v8::Array> value = v8::Array::New(isolate, items, std::size(items));
  return ESCreateIterResultObject(script_state, done, value);
}

}  // namespace bindings

bool V8UnpackIterationResult(ScriptState* script_state,
                             v8::Local<v8::Object> sync_iteration_result,
                             v8::Local<v8::Value>* out_value,
                             bool* out_done) {
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Context> context = script_state->GetContext();
  v8::TryCatch try_block(isolate);

  if (!sync_iteration_result->Get(context, V8AtomicString(isolate, "value"))
           .ToLocal(out_value)) {
    return false;
  }
  v8::Local<v8::Value> done_value;
  if (!sync_iteration_result->Get(context, V8AtomicString(isolate, "done"))
           .ToLocal(&done_value)) {
    return false;
  }
  *out_done = done_value->BooleanValue(isolate);
  return true;
}

}  // namespace blink
