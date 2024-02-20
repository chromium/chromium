// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/async_iterator_base.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink::bindings {

v8::Local<v8::Promise> AsyncIteratorBase::next(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return iteration_source_->Next(script_state, exception_state);
}

v8::Local<v8::Promise> AsyncIteratorBase::returnForBinding(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return iteration_source_->Return(
      script_state, v8::Undefined(script_state->GetIsolate()), exception_state);
}

v8::Local<v8::Promise> AsyncIteratorBase::returnForBinding(
    ScriptState* script_state,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return iteration_source_->Return(script_state, value, exception_state);
}

void AsyncIteratorBase::Trace(Visitor* visitor) const {
  visitor->Trace(iteration_source_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink::bindings
