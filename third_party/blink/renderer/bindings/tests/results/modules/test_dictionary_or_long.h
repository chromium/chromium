// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_MODULES_TEST_DICTIONARY_OR_LONG_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_MODULES_TEST_DICTIONARY_OR_LONG_H_

#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_dictionary.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class MODULES_EXPORT TestDictionaryOrLong final {
  DISALLOW_NEW();
 public:
  TestDictionaryOrLong();
  bool IsNull() const { return type_ == SpecificType::kNone; }

  bool IsLong() const { return type_ == SpecificType::kLong; }
  int32_t GetAsLong() const;
  void SetLong(int32_t);
  static TestDictionaryOrLong FromLong(int32_t);

  bool IsTestDictionary() const { return type_ == SpecificType::kTestDictionary; }
  TestDictionary* GetAsTestDictionary() const;
  void SetTestDictionary(TestDictionary*);
  static TestDictionaryOrLong FromTestDictionary(TestDictionary*);

  TestDictionaryOrLong(const TestDictionaryOrLong&);
  ~TestDictionaryOrLong();
  TestDictionaryOrLong& operator=(const TestDictionaryOrLong&);
  void Trace(blink::Visitor*);

 private:
  enum class SpecificType {
    kNone,
    kLong,
    kTestDictionary,
  };
  SpecificType type_;

  int32_t long_;
  Member<TestDictionary> test_dictionary_;

  friend MODULES_EXPORT v8::Local<v8::Value> ToV8(const TestDictionaryOrLong&, v8::Local<v8::Object>, v8::Isolate*);
};

class V8TestDictionaryOrLong final {
 public:
  MODULES_EXPORT static void ToImpl(v8::Isolate*, v8::Local<v8::Value>, TestDictionaryOrLong&, UnionTypeConversionMode, ExceptionState&);
};

MODULES_EXPORT v8::Local<v8::Value> ToV8(const TestDictionaryOrLong&, v8::Local<v8::Object>, v8::Isolate*);

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, TestDictionaryOrLong& impl) {
  V8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, TestDictionaryOrLong& impl, v8::Local<v8::Object> creationContext) {
  V8SetReturnValue(callbackInfo, ToV8(impl, creationContext, callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<TestDictionaryOrLong> : public NativeValueTraitsBase<TestDictionaryOrLong> {
  MODULES_EXPORT static TestDictionaryOrLong NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
  MODULES_EXPORT static TestDictionaryOrLong NullValue() { return TestDictionaryOrLong(); }
};

template <>
struct V8TypeOf<TestDictionaryOrLong> {
  typedef V8TestDictionaryOrLong Type;
};

}  // namespace blink

// We need to set canInitializeWithMemset=true because HeapVector supports
// items that can initialize with memset or have a vtable. It is safe to
// set canInitializeWithMemset=true for a union type object in practice.
// See https://codereview.chromium.org/1118993002/#msg5 for more details.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::TestDictionaryOrLong)

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_MODULES_TEST_DICTIONARY_OR_LONG_H_
