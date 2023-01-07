// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_iterator_result_value.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"

namespace blink {

v8::Local<v8::Object> V8IteratorResultValue(ScriptState* script_state,
                                            bool done,
                                            v8::Local<v8::Value> value) {
  if (value.IsEmpty())
    value = v8::Undefined(script_state->GetIsolate());
  return V8ObjectBuilder(script_state)
      .Add("done", done)
      .Add("value", value)
      .V8Value();
}

v8::MaybeLocal<v8::Value> V8UnpackIteratorResult(ScriptState* script_state,
                                                 v8::Local<v8::Object> result,
                                                 bool* done) {
  v8::MaybeLocal<v8::Value> maybe_value =
      result->Get(script_state->GetContext(),
                  V8AtomicString(script_state->GetIsolate(), "value"));
  if (maybe_value.IsEmpty())
    return maybe_value;
  v8::Local<v8::Value> done_value;
  if (!result
           ->Get(script_state->GetContext(),
                 V8AtomicString(script_state->GetIsolate(), "done"))
           .ToLocal(&done_value)) {
    return v8::MaybeLocal<v8::Value>();
  }
  *done = done_value->BooleanValue(script_state->GetIsolate());
  return maybe_value;
}

}  // namespace blink
