// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"

#include <tuple>

#include "base/check.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_microtasks_scope.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "v8/include/v8-exception.h"

namespace blink::bindings {

v8::Local<v8::Promise> PromiseResolve(ScriptState* script_state,
                                      v8::Local<v8::Value> value) {
  DCHECK(!value.IsEmpty());
  DCHECK(!value->IsPromise());
  v8::Isolate* isolate = script_state->GetIsolate();
  if (isolate->IsExecutionTerminating()) [[unlikely]] {
    return v8::Local<v8::Promise>();
  }

  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Context> context = script_state->GetContext();
  V8DoNotRunMicrotasksScope microtasks_scope(script_state);
  v8::Local<v8::Promise::Resolver> resolver;
  if (!v8::Promise::Resolver::New(context).ToLocal(&resolver)) [[unlikely]] {
    return v8::Local<v8::Promise>();
  }
  std::ignore = resolver->Resolve(context, value);
  return resolver->GetPromise();
}

v8::Local<v8::Promise> PromiseReject(ScriptState* script_state,
                                     v8::Local<v8::Value> value) {
  DCHECK(!value.IsEmpty());
  v8::Isolate* isolate = script_state->GetIsolate();
  if (isolate->IsExecutionTerminating()) [[unlikely]] {
    return v8::Local<v8::Promise>();
  }

  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Context> context = script_state->GetContext();
  V8DoNotRunMicrotasksScope microtasks_scope(script_state);
  v8::Local<v8::Promise::Resolver> resolver;
  if (!v8::Promise::Resolver::New(context).ToLocal(&resolver)) [[unlikely]] {
    return v8::Local<v8::Promise>();
  }
  std::ignore = resolver->Reject(context, value);
  return resolver->GetPromise();
}

v8::Local<v8::Promise> PromiseThen(ScriptState* script_state,
                                   v8::Local<v8::Promise> promise,
                                   ScriptFunction* on_fulfilled,
                                   ScriptFunction* on_rejected) {
  DCHECK(!promise.IsEmpty());
  CHECK(on_fulfilled);
  v8::Isolate* isolate = script_state->GetIsolate();
  if (isolate->IsExecutionTerminating()) [[unlikely]] {
    return v8::Local<v8::Promise>();
  }

  // `v8::Promise::Then` enters `context` and invokes V8's PerformPromiseThen
  // builtin, which can fail with a catchable exception created in `context`
  // (e.g. RangeError near the stack limit, or an exception from a PromiseHook).
  // `ScriptPromise::Then` does not take an ExceptionState and must not leave
  // non-terminating exceptions pending on the isolate; `v8::TryCatch`'s
  // destructor resets non-terminating exceptions while propagating termination
  // to any outer TryCatch or active V8 frame.
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Context> context = script_state->GetContext();
  v8::Local<v8::Function> v8_on_fulfilled =
      on_fulfilled->ToV8Function(script_state);
  if (on_rejected) {
    v8::Local<v8::Function> v8_on_rejected =
        on_rejected->ToV8Function(script_state);
    return promise->Then(context, v8_on_fulfilled, v8_on_rejected)
        .FromMaybe(v8::Local<v8::Promise>());
  }
  return promise->Then(context, v8_on_fulfilled)
      .FromMaybe(v8::Local<v8::Promise>());
}

v8::Local<v8::Promise> PromiseCatch(ScriptState* script_state,
                                    v8::Local<v8::Promise> promise,
                                    ScriptFunction* on_rejected) {
  DCHECK(!promise.IsEmpty());
  CHECK(on_rejected);
  v8::Isolate* isolate = script_state->GetIsolate();
  if (isolate->IsExecutionTerminating()) [[unlikely]] {
    return v8::Local<v8::Promise>();
  }

  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Context> context = script_state->GetContext();
  v8::Local<v8::Function> v8_on_rejected =
      on_rejected->ToV8Function(script_state);
  return promise->Catch(context, v8_on_rejected)
      .FromMaybe(v8::Local<v8::Promise>());
}

}  // namespace blink::bindings
