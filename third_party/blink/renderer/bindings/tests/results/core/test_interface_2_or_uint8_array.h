// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_TEST_INTERFACE_2_OR_UINT_8_ARRAY_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_TEST_INTERFACE_2_OR_UINT_8_ARRAY_H_

#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer_view.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_uint8_array.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/flexible_array_buffer_view.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class TestInterface2;

class CORE_EXPORT TestInterface2OrUint8Array final {
  DISALLOW_NEW();
 public:
  TestInterface2OrUint8Array();
  bool IsNull() const { return type_ == SpecificType::kNone; }

  bool IsTestInterface2() const { return type_ == SpecificType::kTestInterface2; }
  TestInterface2* GetAsTestInterface2() const;
  void SetTestInterface2(TestInterface2*);
  static TestInterface2OrUint8Array FromTestInterface2(TestInterface2*);

  bool IsUint8Array() const { return type_ == SpecificType::kUint8Array; }
  NotShared<DOMUint8Array> GetAsUint8Array() const;
  void SetUint8Array(NotShared<DOMUint8Array>);
  static TestInterface2OrUint8Array FromUint8Array(NotShared<DOMUint8Array>);

  TestInterface2OrUint8Array(const TestInterface2OrUint8Array&);
  ~TestInterface2OrUint8Array();
  TestInterface2OrUint8Array& operator=(const TestInterface2OrUint8Array&);
  void Trace(blink::Visitor*);

 private:
  enum class SpecificType {
    kNone,
    kTestInterface2,
    kUint8Array,
  };
  SpecificType type_;

  Member<TestInterface2> test_interface_2_;
  Member<DOMUint8Array> uint8_array_;

  friend CORE_EXPORT v8::Local<v8::Value> ToV8(const TestInterface2OrUint8Array&, v8::Local<v8::Object>, v8::Isolate*);
};

class V8TestInterface2OrUint8Array final {
 public:
  CORE_EXPORT static void ToImpl(v8::Isolate*, v8::Local<v8::Value>, TestInterface2OrUint8Array&, UnionTypeConversionMode, ExceptionState&);
};

CORE_EXPORT v8::Local<v8::Value> ToV8(const TestInterface2OrUint8Array&, v8::Local<v8::Object>, v8::Isolate*);

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, TestInterface2OrUint8Array& impl) {
  V8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, TestInterface2OrUint8Array& impl, v8::Local<v8::Object> creationContext) {
  V8SetReturnValue(callbackInfo, ToV8(impl, creationContext, callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<TestInterface2OrUint8Array> : public NativeValueTraitsBase<TestInterface2OrUint8Array> {
  CORE_EXPORT static TestInterface2OrUint8Array NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
  CORE_EXPORT static TestInterface2OrUint8Array NullValue() { return TestInterface2OrUint8Array(); }
};

template <>
struct V8TypeOf<TestInterface2OrUint8Array> {
  typedef V8TestInterface2OrUint8Array Type;
};

}  // namespace blink

// We need to set canInitializeWithMemset=true because HeapVector supports
// items that can initialize with memset or have a vtable. It is safe to
// set canInitializeWithMemset=true for a union type object in practice.
// See https://codereview.chromium.org/1118993002/#msg5 for more details.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::TestInterface2OrUint8Array);

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_TEST_INTERFACE_2_OR_UINT_8_ARRAY_H_
