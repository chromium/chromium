// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/async_iterator_base.h"

#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink::bindings {

ScriptPromise<IDLAny> AsyncIteratorBase::next(ScriptState* script_state) {
  return iteration_source_->Next(script_state);
}

ScriptPromise<IDLAny> AsyncIteratorBase::returnForBinding(
    ScriptState* script_state) {
  v8::Isolate* isolate = script_state->GetIsolate();
  return iteration_source_->Return(
      script_state, ScriptValue(isolate, v8::Undefined(isolate)));
}

ScriptPromise<IDLAny> AsyncIteratorBase::returnForBinding(
    ScriptState* script_state,
    ScriptValue value) {
  return iteration_source_->Return(script_state, value);
}

void AsyncIteratorBase::Trace(Visitor* visitor) const {
  visitor->Trace(iteration_source_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink::bindings
