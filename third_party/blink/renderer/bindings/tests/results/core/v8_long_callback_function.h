// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/callback_function.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_V8_LONG_CALLBACK_FUNCTION_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_V8_LONG_CALLBACK_FUNCTION_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/callback_function_base.h"

namespace blink {

class ScriptWrappable;

class CORE_EXPORT V8LongCallbackFunction final : public CallbackFunctionBase {
 public:
  static V8LongCallbackFunction* Create(v8::Local<v8::Function> callback_function) {
    return new V8LongCallbackFunction(callback_function);
  }

  ~V8LongCallbackFunction() override = default;

  // NameClient overrides:
  const char* NameInHeapSnapshot() const override;

  // Performs "invoke".
  // https://heycam.github.io/webidl/#es-invoking-callback-functions
  v8::Maybe<int32_t> Invoke(ScriptWrappable* callback_this_value, int32_t num1, int32_t num2) WARN_UNUSED_RESULT;

 private:
  explicit V8LongCallbackFunction(v8::Local<v8::Function> callback_function)
      : CallbackFunctionBase(callback_function) {}
};

template <>
class V8PersistentCallbackFunction<V8LongCallbackFunction> final : public V8PersistentCallbackFunctionBase {
  using V8CallbackFunction = V8LongCallbackFunction;

 public:
  ~V8PersistentCallbackFunction() override = default;

  // Returns a wrapper-tracing version of this callback function.
  V8CallbackFunction* ToNonV8Persistent() { return Proxy(); }

  v8::Maybe<int32_t> Invoke(ScriptWrappable* callback_this_value, int32_t num1, int32_t num2) WARN_UNUSED_RESULT;

 private:
  explicit V8PersistentCallbackFunction(V8CallbackFunction* callback_function)
      : V8PersistentCallbackFunctionBase(callback_function) {}

  V8CallbackFunction* Proxy() {
    return As<V8CallbackFunction>();
  }

  template <typename V8CallbackFunction>
  friend V8PersistentCallbackFunction<V8CallbackFunction>*
  ToV8PersistentCallbackFunction(V8CallbackFunction*);
};

// V8LongCallbackFunction is designed to be used with wrapper-tracing.
// As blink::Persistent does not perform wrapper-tracing, use of
// |WrapPersistent| for callback functions is likely (if not always) misuse.
// Thus, this code prohibits such a use case. The call sites should explicitly
// use WrapPersistent(V8PersistentCallbackFunction<T>*).
Persistent<V8LongCallbackFunction> WrapPersistent(V8LongCallbackFunction*) = delete;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_V8_LONG_CALLBACK_FUNCTION_H_
