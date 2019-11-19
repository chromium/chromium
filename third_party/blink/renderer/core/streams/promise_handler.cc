// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/promise_handler.h"

#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

namespace {

void NoopFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>&) {}

}  // namespace

PromiseHandler::PromiseHandler(ScriptState* script_state)
    : PromiseHandlerBase(script_state) {}

void PromiseHandler::CallRaw(const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK_EQ(args.Length(), 1);
  CallWithLocal(args[0]);
}

PromiseHandlerWithValue::PromiseHandlerWithValue(ScriptState* script_state)
    : PromiseHandlerBase(script_state) {}

void PromiseHandlerWithValue::CallRaw(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK_EQ(args.Length(), 1);
  auto ret = CallWithLocal(args[0]);
  args.GetReturnValue().Set(ret);
}

v8::Local<v8::Promise> StreamThenPromise(v8::Local<v8::Context> context,
                                         v8::Local<v8::Promise> promise,
                                         PromiseHandlerBase* on_fulfilled,
                                         PromiseHandlerBase* on_rejected) {
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
      return v8::Promise::Resolver::New(context).ToLocalChecked()->GetPromise();
    }
    result_maybe =
        promise->Then(context, noop, on_rejected->BindToV8Function());
  } else if (on_rejected) {
    result_maybe = promise->Then(context, on_fulfilled->BindToV8Function(),
                                 on_rejected->BindToV8Function());
  } else {
    result_maybe = promise->Then(context, on_fulfilled->BindToV8Function());
  }

  v8::Local<v8::Promise> result;
  if (!result_maybe.ToLocal(&result)) {
    DVLOG(3)
        << "assuming that failure of promise->Then() is caused by shutdown and"
           "ignoring it";
    // Try to create a dummy promise so that the calling code can continue. If
    // we can't create one, then we can't return to the calling context so we
    // have to crash. This shouldn't happen except on OOM.
    result = v8::Promise::Resolver::New(context).ToLocalChecked()->GetPromise();
  }
  return result;
}

}  // namespace blink
