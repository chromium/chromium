// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_PROMISE_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_PROMISE_HANDLER_H_

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "v8/include/v8.h"

namespace blink {

// Common subclass for PromiseHandlers.
class CORE_EXPORT PromiseHandlerBase : public ScriptFunction {
 public:
  explicit PromiseHandlerBase(ScriptState* script_state)
      : ScriptFunction(script_state) {}

  // Exposed for use by StreamThenPromise.
  using ScriptFunction::BindToV8Function;
};

// A variant of ScriptFunction that avoids the conversion to and from a
// ScriptValue. Use this when the reaction doesn't need to return a value other
// than undefined.
class CORE_EXPORT PromiseHandler : public PromiseHandlerBase {
 public:
  explicit PromiseHandler(ScriptState* script_state);

  virtual void CallWithLocal(v8::Local<v8::Value>) = 0;

 private:
  void CallRaw(const v8::FunctionCallbackInfo<v8::Value>&) final;
};

// A variant of PromiseHandler for when the reaction does need to return a
// value.
class CORE_EXPORT PromiseHandlerWithValue : public PromiseHandlerBase {
 public:
  explicit PromiseHandlerWithValue(ScriptState* script_state);

  virtual v8::Local<v8::Value> CallWithLocal(v8::Local<v8::Value>) = 0;

 private:
  void CallRaw(const v8::FunctionCallbackInfo<v8::Value>&) final;
};

// A convenience wrapper for promise->Then() for when all paths are
// PromiseHandlers. It avoids having to call BindToV8Function()
// explicitly. If |on_rejected| is null then behaves like single-argument
// Then(). If |on_fulfilled| is null then it behaves like Catch().
v8::Local<v8::Promise> StreamThenPromise(
    v8::Local<v8::Context>,
    v8::Local<v8::Promise>,
    PromiseHandlerBase* on_fulfilled,
    PromiseHandlerBase* on_rejected = nullptr);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_PROMISE_HANDLER_H_
