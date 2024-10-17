/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "v8/include/v8.h"

namespace blink {

ScriptPromiseUntyped::ScriptPromiseUntyped(v8::Isolate* isolate,
                                           v8::Local<v8::Promise> promise)
    : promise_(isolate, promise) {}

ScriptPromiseUntyped::ScriptPromiseUntyped(const ScriptPromiseUntyped& other) {
  promise_ = other.promise_;
}

ScriptPromise<IDLAny> ScriptPromiseUntyped::Then(ScriptFunction* on_fulfilled,
                                                 ScriptFunction* on_rejected) {
  DCHECK(on_fulfilled || on_rejected);
  ScriptState* script_state = on_fulfilled ? on_fulfilled->GetScriptState()
                                           : on_rejected->GetScriptState();
  return ScriptPromise<IDLAny>::FromV8Promise(
      script_state->GetIsolate(),
      ThenRaw(script_state, on_fulfilled, on_rejected));
}

v8::Local<v8::Promise> ScriptPromiseUntyped::ThenRaw(
    ScriptState* script_state,
    ScriptFunction* on_fulfilled,
    ScriptFunction* on_rejected) const {
  CHECK(on_fulfilled || on_rejected);
  CHECK(!on_fulfilled || !on_rejected ||
        on_fulfilled->GetScriptState() == on_rejected->GetScriptState());
  if (promise_.IsEmpty()) {
    return v8::Local<v8::Promise>();
  }

  v8::Local<v8::Promise> promise = V8Promise();
  v8::Local<v8::Promise> result_promise;
  if (!on_rejected) {
    if (!promise->Then(script_state->GetContext(), on_fulfilled->V8Function())
             .ToLocal(&result_promise)) {
      return v8::Local<v8::Promise>();
    }
  } else if (!on_fulfilled) {
    if (!promise->Catch(script_state->GetContext(), on_rejected->V8Function())
             .ToLocal(&result_promise)) {
      return v8::Local<v8::Promise>();
    }
  } else {
    if (!promise
             ->Then(script_state->GetContext(), on_fulfilled->V8Function(),
                    on_rejected->V8Function())
             .ToLocal(&result_promise)) {
      return v8::Local<v8::Promise>();
    }
  }
  return result_promise;
}

ScriptPromiseUntyped ScriptPromiseUntyped::Reject(ScriptState* script_state,
                                                  const ScriptValue& value) {
  return ScriptPromiseUntyped::Reject(script_state, value.V8Value());
}

ScriptPromiseUntyped ScriptPromiseUntyped::Reject(ScriptState* script_state,
                                                  v8::Local<v8::Value> value) {
  return ScriptPromiseUntyped(script_state->GetIsolate(),
                              RejectRaw(script_state, value));
}

v8::Local<v8::Promise> ScriptPromiseUntyped::ResolveRaw(
    ScriptState* script_state,
    v8::Local<v8::Value> value) {
  v8::MicrotasksScope microtasks_scope(
      script_state->GetIsolate(), ToMicrotaskQueue(script_state),
      v8::MicrotasksScope::kDoNotRunMicrotasks);
  auto resolver =
      v8::Promise::Resolver::New(script_state->GetContext()).ToLocalChecked();
  std::ignore = resolver->Resolve(script_state->GetContext(), value);
  return resolver->GetPromise();
}

v8::Local<v8::Promise> ScriptPromiseUntyped::RejectRaw(
    ScriptState* script_state,
    v8::Local<v8::Value> value) {
  v8::MicrotasksScope microtasks_scope(
      script_state->GetIsolate(), ToMicrotaskQueue(script_state),
      v8::MicrotasksScope::kDoNotRunMicrotasks);
  auto resolver =
      v8::Promise::Resolver::New(script_state->GetContext()).ToLocalChecked();
  std::ignore = resolver->Reject(script_state->GetContext(), value);
  return resolver->GetPromise();
}

void ScriptPromiseUntyped::MarkAsHandled() {
  if (promise_.IsEmpty())
    return;
  promise_.V8Value().As<v8::Promise>()->MarkAsHandled();
}

ScriptPromise<IDLUndefined> ToResolvedUndefinedPromise(
    ScriptState* script_state) {
  return ToResolvedPromise<IDLUndefined>(script_state,
                                         ToV8UndefinedGenerator());
}

}  // namespace blink
