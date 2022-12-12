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
}  // namespace blink
