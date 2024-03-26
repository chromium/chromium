// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"

#include "third_party/blink/renderer/core/streams/writable_stream_default_controller.h"
#include "v8/include/v8.h"

namespace blink {

ScriptPromise<IDLUndefined> UnderlyingSinkBase::start(
    ScriptState* script_state,
    ScriptValue controller,
    ExceptionState& exception_state) {
  controller_ = WritableStreamDefaultController::From(script_state, controller);
  return start(script_state, controller_, exception_state);
}

ScriptValue UnderlyingSinkBase::type(ScriptState* script_state) const {
  auto* isolate = script_state->GetIsolate();
  return ScriptValue(isolate, v8::Undefined(isolate));
}

void UnderlyingSinkBase::Trace(Visitor* visitor) const {
  visitor->Trace(controller_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
