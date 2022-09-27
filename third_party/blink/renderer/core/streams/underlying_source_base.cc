// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/underlying_source_base.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "v8/include/v8.h"

namespace blink {

ScriptPromise UnderlyingSourceBase::startWrapper(ScriptState* script_state,
                                                 ScriptValue js_controller) {
  // Cannot call start twice (e.g., cannot use the same UnderlyingSourceBase to
  // construct multiple streams).
  DCHECK(!controller_);

  controller_ =
      MakeGarbageCollected<ReadableStreamDefaultControllerWithScriptScope>(
          script_state, js_controller);

  return Start(script_state);
}

ScriptPromise UnderlyingSourceBase::Start(ScriptState* script_state) {
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise UnderlyingSourceBase::pull(ScriptState* script_state) {
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise UnderlyingSourceBase::cancelWrapper(ScriptState* script_state) {
  v8::Isolate* isolate = script_state->GetIsolate();
  return cancelWrapper(script_state,
                       ScriptValue(isolate, v8::Undefined(isolate)));
}

ScriptPromise UnderlyingSourceBase::cancelWrapper(ScriptState* script_state,
                                                  ScriptValue reason) {
  DCHECK(controller_);  // startWrapper() must have been called
  controller_->Deactivate();
  return Cancel(script_state, reason);
}

ScriptPromise UnderlyingSourceBase::Cancel(ScriptState* script_state,
                                           ScriptValue reason) {
  return ScriptPromise::CastUndefined(script_state);
}

ScriptValue UnderlyingSourceBase::type(ScriptState* script_state) const {
  return ScriptValue(script_state->GetIsolate(),
                     v8::Undefined(script_state->GetIsolate()));
}

void UnderlyingSourceBase::ContextDestroyed() {
  // `controller_` can be unset in two cases:
  // 1. The UnderlyingSourceBase is never used to create a ReadableStream. For
  //    example, BodyStreamBuffer inherits from UnderlyingSourceBase but if an
  //    existing stream is passed to the constructor it won't create a new one.
  // 2. ContextDestroyed() is called re-entrantly during construction. This can
  //    happen when a worker is terminated.
  if (controller_)
    controller_->Deactivate();
}

void UnderlyingSourceBase::Trace(Visitor* visitor) const {
  visitor->Trace(controller_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
