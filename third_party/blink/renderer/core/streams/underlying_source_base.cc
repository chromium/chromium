// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/underlying_source_base.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_interface.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "v8/include/v8.h"

namespace blink {

ScriptPromise UnderlyingSourceBase::startWrapper(ScriptState* script_state,
                                                 ScriptValue js_controller) {
  // Cannot call start twice (e.g., cannot use the same UnderlyingSourceBase to
  // construct multiple streams).
  DCHECK(!controller_);

  controller_ = ReadableStreamDefaultControllerInterface::Create(script_state,
                                                                 js_controller);

  return Start(script_state);
}

ScriptPromise UnderlyingSourceBase::Start(ScriptState* script_state) {
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise UnderlyingSourceBase::pull(ScriptState* script_state) {
  return ScriptPromise::CastUndefined(script_state);
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

void UnderlyingSourceBase::ContextDestroyed(ExecutionContext*) {
  if (controller_) {
    controller_->NoteHasBeenCanceled();
    controller_.Clear();
  }
}

void UnderlyingSourceBase::Trace(blink::Visitor* visitor) {
  visitor->Trace(controller_);
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
