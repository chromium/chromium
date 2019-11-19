// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/callback_interface.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_V8_TEST_CALLBACK_INTERFACE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_V8_TEST_CALLBACK_INTERFACE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/callback_interface_base.h"
#include "third_party/blink/renderer/platform/bindings/v8_value_or_script_wrappable_adapter.h"

namespace blink {

class TestInterfaceEmpty;

class CORE_EXPORT V8TestCallbackInterface final : public CallbackInterfaceBase {
 public:
  static V8TestCallbackInterface* Create(v8::Local<v8::Object> callback_object) {
    return MakeGarbageCollected<V8TestCallbackInterface>(callback_object);
  }

  explicit V8TestCallbackInterface(v8::Local<v8::Object> callback_object)
      : CallbackInterfaceBase(callback_object,
                              kNotSingleOperation) {}
  ~V8TestCallbackInterface() override = default;

  // NameClient overrides:
  const char* NameInHeapSnapshot() const override;

  // Performs "call a user object's operation".
  // https://heycam.github.io/webidl/#call-a-user-objects-operation
  v8::Maybe<void> voidMethod(bindings::V8ValueOrScriptWrappableAdapter callback_this_value) WARN_UNUSED_RESULT;

  // Performs "call a user object's operation".
  // https://heycam.github.io/webidl/#call-a-user-objects-operation
  v8::Maybe<bool> booleanMethod(bindings::V8ValueOrScriptWrappableAdapter callback_this_value) WARN_UNUSED_RESULT;

  // Performs "call a user object's operation".
  // https://heycam.github.io/webidl/#call-a-user-objects-operation
  v8::Maybe<void> voidMethodBooleanArg(bindings::V8ValueOrScriptWrappableAdapter callback_this_value, bool boolArg) WARN_UNUSED_RESULT;

  // Performs "call a user object's operation".
  // https://heycam.github.io/webidl/#call-a-user-objects-operation
  v8::Maybe<void> voidMethodSequenceArg(bindings::V8ValueOrScriptWrappableAdapter callback_this_value, const HeapVector<Member<TestInterfaceEmpty>>& sequenceArg) WARN_UNUSED_RESULT;

  // Performs "call a user object's operation".
  // https://heycam.github.io/webidl/#call-a-user-objects-operation
  v8::Maybe<void> voidMethodFloatArg(bindings::V8ValueOrScriptWrappableAdapter callback_this_value, float floatArg) WARN_UNUSED_RESULT;

  // Performs "call a user object's operation".
  // https://heycam.github.io/webidl/#call-a-user-objects-operation
  v8::Maybe<void> voidMethodTestInterfaceEmptyArg(bindings::V8ValueOrScriptWrappableAdapter callback_this_value, TestInterfaceEmpty* testInterfaceEmptyArg) WARN_UNUSED_RESULT;

  // Performs "call a user object's operation".
  // https://heycam.github.io/webidl/#call-a-user-objects-operation
  v8::Maybe<void> voidMethodTestInterfaceEmptyStringArg(bindings::V8ValueOrScriptWrappableAdapter callback_this_value, TestInterfaceEmpty* testInterfaceEmptyArg, const String& stringArg) WARN_UNUSED_RESULT;

  // Performs "call a user object's operation".
  // https://heycam.github.io/webidl/#call-a-user-objects-operation
  v8::Maybe<void> callbackWithThisValueVoidMethodStringArg(bindings::V8ValueOrScriptWrappableAdapter callback_this_value, const String& stringArg) WARN_UNUSED_RESULT;

  // Performs "call a user object's operation".
  // https://heycam.github.io/webidl/#call-a-user-objects-operation
  v8::Maybe<void> customVoidMethodTestInterfaceEmptyArg(bindings::V8ValueOrScriptWrappableAdapter callback_this_value, TestInterfaceEmpty* testInterfaceEmptyArg) WARN_UNUSED_RESULT;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_V8_TEST_CALLBACK_INTERFACE_H_
