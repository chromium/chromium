// Copyright 2016 The Chromium Authors. All rights reserved.
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
  if (controller_)
    controller_->NoteHasBeenCanceled();
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
  if (controller_) {
    controller_->NoteHasBeenCanceled();
    controller_.Clear();
  }
}

void UnderlyingSourceBase::Trace(Visitor* visitor) const {
  visitor->Trace(controller_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
