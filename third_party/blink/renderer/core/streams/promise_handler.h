// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_PROMISE_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_PROMISE_HANDLER_H_

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "v8/include/v8.h"

namespace blink {

// A variant of ScriptFunction that avoids the conversion to and from a
// ScriptValue. Use this when the reaction doesn't need to return a value other
// than undefined.
class CORE_EXPORT PromiseHandler : public ScriptFunction::Callable {
 public:
  PromiseHandler();

  virtual void CallWithLocal(ScriptState*, v8::Local<v8::Value>) = 0;

  void CallRaw(ScriptState*, const v8::FunctionCallbackInfo<v8::Value>&) final;
};

// A variant of PromiseHandler for when the reaction does need to return a
// value.
class CORE_EXPORT PromiseHandlerWithValue : public ScriptFunction::Callable {
 public:
  PromiseHandlerWithValue();

  virtual v8::Local<v8::Value> CallWithLocal(ScriptState*,
                                             v8::Local<v8::Value>) = 0;

  void CallRaw(ScriptState*, const v8::FunctionCallbackInfo<v8::Value>&) final;
};

// A convenience wrapper for promise->Then() for when all paths are
// PromiseHandlers. It avoids having to call BindToV8Function()
// explicitly. If |on_rejected| is null then behaves like single-argument
// Then(). If |on_fulfilled| is null then it behaves like Catch().
CORE_EXPORT v8::Local<v8::Promise> StreamThenPromise(
    v8::Local<v8::Context>,
    v8::Local<v8::Promise>,
    ScriptFunction* on_fulfilled,
    ScriptFunction* on_rejected = nullptr);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_PROMISE_HANDLER_H_
