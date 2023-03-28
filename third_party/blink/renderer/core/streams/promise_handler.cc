// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/promise_handler.h"


namespace blink {

namespace {

void NoopFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>&) {}

// Creating a new v8::Promise::Resolver to create a new promise can fail. If
// JavaScript will no longer execute, then we can safely return the original
// promise. Otherwise we have no choice but to crash.
v8::Local<v8::Promise> AttemptToReturnDummyPromise(
    v8::Local<v8::Context> context,
    v8::Local<v8::Promise> original_promise) {
  v8::Local<v8::Promise::Resolver> resolver;
  if (!v8::Promise::Resolver::New(context).ToLocal(&resolver)) {
    if (!context->GetIsolate()->IsExecutionTerminating()) {
      // It's not safe to leak |original_promise| unless we have a guarantee
      // that no further JavaScript will run.
      LOG(FATAL) << "Cannot recover from failure to create a new "
                    "v8::Promise::Resolver object (OOM?)";
    }

    // We are probably in the process of worker termination.
    return original_promise;
  }

  return resolver->GetPromise();
}

}  // namespace

PromiseHandler::PromiseHandler() = default;

void PromiseHandler::CallRaw(ScriptState* script_state,
                             const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK_EQ(args.Length(), 1);
  CallWithLocal(script_state, args[0]);
}

PromiseHandlerWithValue::PromiseHandlerWithValue() = default;

void PromiseHandlerWithValue::CallRaw(
    ScriptState* script_state,
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK_EQ(args.Length(), 1);
  auto ret = CallWithLocal(script_state, args[0]);
  args.GetReturnValue().Set(ret);
}

v8::Local<v8::Promise> StreamThenPromise(v8::Local<v8::Context> context,
                                         v8::Local<v8::Promise> promise,
                                         ScriptFunction* on_fulfilled,
                                         ScriptFunction* on_rejected) {
  v8::Context::Scope v8_context_scope(context);
  v8::MaybeLocal<v8::Promise> result_maybe;
  if (!on_fulfilled) {
    DCHECK(on_rejected);
    // v8::Promise::Catch is not safe as it calls promise.then() which can be
    // tampered with by JavaScript. v8::Promise::Then won't accept an undefined
    // value for on_fulfilled, it has to be a function. So we pass a no-op
    // function, which gives us approximately the semantics we need.
    // TODO(ricea): Add a safe variant of v8::Promise::Catch to V8.
    v8::Local<v8::Function> noop;
    if (!v8::Function::New(context, NoopFunctionCallback).ToLocal(&noop)) {
      DVLOG(3) << "Assuming that the failure of v8::Function::New() is caused "
               << "by shutdown and ignoring it";
      return AttemptToReturnDummyPromise(context, promise);
    }
    result_maybe = promise->Then(context, noop, on_rejected->V8Function());
  } else if (on_rejected) {
    result_maybe = promise->Then(context, on_fulfilled->V8Function(),
                                 on_rejected->V8Function());
  } else {
    result_maybe = promise->Then(context, on_fulfilled->V8Function());
  }

  v8::Local<v8::Promise> result;
  if (!result_maybe.ToLocal(&result)) {
    DVLOG(3)
        << "assuming that failure of promise->Then() is caused by shutdown and"
           "ignoring it";
    result = AttemptToReturnDummyPromise(context, promise);
  }
  return result;
}

}  // namespace blink
