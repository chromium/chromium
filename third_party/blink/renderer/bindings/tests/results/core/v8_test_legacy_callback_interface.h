// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/callback_interface.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_V8_TEST_LEGACY_CALLBACK_INTERFACE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_V8_TEST_LEGACY_CALLBACK_INTERFACE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/callback_interface_base.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/v8_value_or_script_wrappable_adapter.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"

namespace blink {

class Node;

CORE_EXPORT extern const WrapperTypeInfo _wrapper_type_info;

class CORE_EXPORT V8TestLegacyCallbackInterface final : public CallbackInterfaceBase {
 public:
  // Support of "legacy callback interface"
  static v8::Local<v8::FunctionTemplate> DomTemplate(v8::Isolate*, const DOMWrapperWorld&);
  static constexpr const WrapperTypeInfo* GetWrapperTypeInfo() {
    return &_wrapper_type_info;
  }

  // Constants
  static constexpr uint16_t CONST_VALUE_USHORT_42 = 42;

  static V8TestLegacyCallbackInterface* Create(v8::Local<v8::Object> callback_object) {
    return MakeGarbageCollected<V8TestLegacyCallbackInterface>(callback_object);
  }

  explicit V8TestLegacyCallbackInterface(v8::Local<v8::Object> callback_object)
      : CallbackInterfaceBase(callback_object,
                              kSingleOperation) {}
  ~V8TestLegacyCallbackInterface() override = default;

  // NameClient overrides:
  const char* NameInHeapSnapshot() const override;

  // Performs "call a user object's operation".
  // https://heycam.github.io/webidl/#call-a-user-objects-operation
  v8::Maybe<uint16_t> acceptNode(bindings::V8ValueOrScriptWrappableAdapter callback_this_value, Node* node) WARN_UNUSED_RESULT;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_V8_TEST_LEGACY_CALLBACK_INTERFACE_H_
