// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_promise_rejection_event.h"

#include "third_party/blink/renderer/bindings/core/v8/generated_code_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/events/promise_rejection_event.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

void V8PromiseRejectionEvent::PromiseAttributeGetterCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();

  // This attribute returns a Promise.
  // Per https://webidl.spec.whatwg.org/#dfn-attribute-getter, all exceptions
  // must be turned into a Promise rejection. Returning a Promise type requires
  // us to disable some of V8's type checks, so we have to manually check that
  // info.Holder() really points to an instance of the type.
  PromiseRejectionEvent* event =
      V8PromiseRejectionEvent::ToWrappable(isolate, info.Holder());
  if (!event) {
    ExceptionState exception_state(isolate, ExceptionState::kGetterContext,
                                   "PromiseRejectionEvent", "promise");
    ExceptionToRejectPromiseScope rejectPromiseScope(info, exception_state);
    exception_state.ThrowTypeError("Illegal invocation");
    return;
  }

  ScriptPromise promise = event->promise(ScriptState::Current(isolate));
  if (promise.IsEmpty()) {
    V8SetReturnValueNull(info);
    return;
  }

  V8SetReturnValue(info, promise.V8Value());
}

}  // namespace blink
