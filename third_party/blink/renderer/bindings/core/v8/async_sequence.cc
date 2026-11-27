// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/async_sequence.h"

#include "base/check.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink::bindings {

AsyncSequenceBase::AsyncSequenceBase(v8::Isolate* isolate,
                                     v8::Local<v8::Object> object,
                                     v8::Local<v8::Function> method,
                                     ScriptIterator::Kind kind)
    : object_(isolate, object), method_(isolate, method), kind_(kind) {
  DCHECK(kind == ScriptIterator::Kind::kAsync ||
         kind == ScriptIterator::Kind::kAsyncFromSync);
}

v8::Local<v8::Object> AsyncSequenceBase::V8Object(
    ScriptState* script_state) const {
  return object_.Get(script_state);
}

ScriptIterator AsyncSequenceBase::OpenInternal(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  return ScriptIterator::FromIteratorMethod(
      script_state->GetIsolate(), object_.Get(script_state),
      method_.Get(script_state), kind_, exception_state);
}

void AsyncSequenceBase::Trace(Visitor* visitor) const {
  visitor->Trace(object_);
  visitor->Trace(method_);
}

}  // namespace blink::bindings
