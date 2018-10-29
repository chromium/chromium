// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_TEST_INTERFACE_OR_LONG_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_TEST_INTERFACE_OR_LONG_H_

#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class TestInterfaceImplementation;

class CORE_EXPORT TestInterfaceOrLong final {
  DISALLOW_NEW();
 public:
  TestInterfaceOrLong();
  bool IsNull() const { return type_ == SpecificType::kNone; }

  bool IsLong() const { return type_ == SpecificType::kLong; }
  int32_t GetAsLong() const;
  void SetLong(int32_t);
  static TestInterfaceOrLong FromLong(int32_t);

  bool IsTestInterface() const { return type_ == SpecificType::kTestInterface; }
  TestInterfaceImplementation* GetAsTestInterface() const;
  void SetTestInterface(TestInterfaceImplementation*);
  static TestInterfaceOrLong FromTestInterface(TestInterfaceImplementation*);

  TestInterfaceOrLong(const TestInterfaceOrLong&);
  ~TestInterfaceOrLong();
  TestInterfaceOrLong& operator=(const TestInterfaceOrLong&);
  void Trace(blink::Visitor*);

 private:
  enum class SpecificType {
    kNone,
    kLong,
    kTestInterface,
  };
  SpecificType type_;

  int32_t long_;
  Member<TestInterfaceImplementation> test_interface_;

  friend CORE_EXPORT v8::Local<v8::Value> ToV8(const TestInterfaceOrLong&, v8::Local<v8::Object>, v8::Isolate*);
};

class V8TestInterfaceOrLong final {
 public:
  CORE_EXPORT static void ToImpl(v8::Isolate*, v8::Local<v8::Value>, TestInterfaceOrLong&, UnionTypeConversionMode, ExceptionState&);
};

CORE_EXPORT v8::Local<v8::Value> ToV8(const TestInterfaceOrLong&, v8::Local<v8::Object>, v8::Isolate*);

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, TestInterfaceOrLong& impl) {
  V8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, TestInterfaceOrLong& impl, v8::Local<v8::Object> creationContext) {
  V8SetReturnValue(callbackInfo, ToV8(impl, creationContext, callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<TestInterfaceOrLong> : public NativeValueTraitsBase<TestInterfaceOrLong> {
  CORE_EXPORT static TestInterfaceOrLong NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
  CORE_EXPORT static TestInterfaceOrLong NullValue() { return TestInterfaceOrLong(); }
};

template <>
struct V8TypeOf<TestInterfaceOrLong> {
  typedef V8TestInterfaceOrLong Type;
};

}  // namespace blink

// We need to set canInitializeWithMemset=true because HeapVector supports
// items that can initialize with memset or have a vtable. It is safe to
// set canInitializeWithMemset=true for a union type object in practice.
// See https://codereview.chromium.org/1118993002/#msg5 for more details.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::TestInterfaceOrLong);

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_TEST_INTERFACE_OR_LONG_H_
