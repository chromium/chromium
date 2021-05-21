// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/long_or_test_dictionary.h"

#include "base/cxx17_backports.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"

namespace blink {

LongOrTestDictionary::LongOrTestDictionary() : type_(SpecificType::kNone) {}

int32_t LongOrTestDictionary::GetAsLong() const {
  DCHECK(IsLong());
  return long_;
}

void LongOrTestDictionary::SetLong(int32_t value) {
  DCHECK(IsNull());
  long_ = value;
  type_ = SpecificType::kLong;
}

LongOrTestDictionary LongOrTestDictionary::FromLong(int32_t value) {
  LongOrTestDictionary container;
  container.SetLong(value);
  return container;
}

TestDictionary* LongOrTestDictionary::GetAsTestDictionary() const {
  DCHECK(IsTestDictionary());
  return test_dictionary_;
}

void LongOrTestDictionary::SetTestDictionary(TestDictionary* value) {
  DCHECK(IsNull());
  test_dictionary_ = value;
  type_ = SpecificType::kTestDictionary;
}

LongOrTestDictionary LongOrTestDictionary::FromTestDictionary(TestDictionary* value) {
  LongOrTestDictionary container;
  container.SetTestDictionary(value);
  return container;
}

LongOrTestDictionary::LongOrTestDictionary(const LongOrTestDictionary&) = default;
LongOrTestDictionary::~LongOrTestDictionary() = default;
LongOrTestDictionary& LongOrTestDictionary::operator=(const LongOrTestDictionary&) = default;

void LongOrTestDictionary::Trace(Visitor* visitor) const {
  visitor->Trace(test_dictionary_);
}

void V8LongOrTestDictionary::ToImpl(
    v8::Isolate* isolate,
    v8::Local<v8::Value> v8_value,
    LongOrTestDictionary& impl,
    UnionTypeConversionMode conversion_mode,
    ExceptionState& exception_state) {
  if (v8_value.IsEmpty())
    return;

  if (conversion_mode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8_value))
    return;

  if (IsUndefinedOrNull(v8_value)) {
    TestDictionary* cpp_value{ NativeValueTraits<TestDictionary>::NativeValue(isolate, v8_value, exception_state) };
    if (exception_state.HadException())
      return;
    impl.SetTestDictionary(cpp_value);
    return;
  }

  if (v8_value->IsObject()) {
    TestDictionary* cpp_value{ NativeValueTraits<TestDictionary>::NativeValue(isolate, v8_value, exception_state) };
    if (exception_state.HadException())
      return;
    impl.SetTestDictionary(cpp_value);
    return;
  }

  if (v8_value->IsNumber()) {
    int32_t cpp_value{ NativeValueTraits<IDLLong>::NativeValue(isolate, v8_value, exception_state) };
    if (exception_state.HadException())
      return;
    impl.SetLong(cpp_value);
    return;
  }

  {
    int32_t cpp_value{ NativeValueTraits<IDLLong>::NativeValue(isolate, v8_value, exception_state) };
    if (exception_state.HadException())
      return;
    impl.SetLong(cpp_value);
    return;
  }
}

v8::Local<v8::Value> ToV8(const LongOrTestDictionary& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case LongOrTestDictionary::SpecificType::kNone:
      return v8::Null(isolate);
    case LongOrTestDictionary::SpecificType::kLong:
      return v8::Integer::New(isolate, impl.GetAsLong());
    case LongOrTestDictionary::SpecificType::kTestDictionary:
      return ToV8(impl.GetAsTestDictionary(), creationContext, isolate);
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

LongOrTestDictionary NativeValueTraits<LongOrTestDictionary>::NativeValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  LongOrTestDictionary impl;
  V8LongOrTestDictionary::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exception_state);
  return impl;
}

}  // namespace blink

