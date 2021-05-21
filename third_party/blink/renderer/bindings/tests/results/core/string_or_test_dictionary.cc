// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/string_or_test_dictionary.h"

#include "base/cxx17_backports.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"

namespace blink {

StringOrTestDictionary::StringOrTestDictionary() : type_(SpecificType::kNone) {}

const String& StringOrTestDictionary::GetAsString() const {
  DCHECK(IsString());
  return string_;
}

void StringOrTestDictionary::SetString(const String& value) {
  DCHECK(IsNull());
  string_ = value;
  type_ = SpecificType::kString;
}

StringOrTestDictionary StringOrTestDictionary::FromString(const String& value) {
  StringOrTestDictionary container;
  container.SetString(value);
  return container;
}

TestDictionary* StringOrTestDictionary::GetAsTestDictionary() const {
  DCHECK(IsTestDictionary());
  return test_dictionary_;
}

void StringOrTestDictionary::SetTestDictionary(TestDictionary* value) {
  DCHECK(IsNull());
  test_dictionary_ = value;
  type_ = SpecificType::kTestDictionary;
}

StringOrTestDictionary StringOrTestDictionary::FromTestDictionary(TestDictionary* value) {
  StringOrTestDictionary container;
  container.SetTestDictionary(value);
  return container;
}

StringOrTestDictionary::StringOrTestDictionary(const StringOrTestDictionary&) = default;
StringOrTestDictionary::~StringOrTestDictionary() = default;
StringOrTestDictionary& StringOrTestDictionary::operator=(const StringOrTestDictionary&) = default;

void StringOrTestDictionary::Trace(Visitor* visitor) const {
  visitor->Trace(test_dictionary_);
}

void V8StringOrTestDictionary::ToImpl(
    v8::Isolate* isolate,
    v8::Local<v8::Value> v8_value,
    StringOrTestDictionary& impl,
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

  {
    V8StringResource<> cpp_value{ v8_value };
    if (!cpp_value.Prepare(exception_state))
      return;
    impl.SetString(cpp_value);
    return;
  }
}

v8::Local<v8::Value> ToV8(const StringOrTestDictionary& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case StringOrTestDictionary::SpecificType::kNone:
      return v8::Null(isolate);
    case StringOrTestDictionary::SpecificType::kString:
      return V8String(isolate, impl.GetAsString());
    case StringOrTestDictionary::SpecificType::kTestDictionary:
      return ToV8(impl.GetAsTestDictionary(), creationContext, isolate);
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

StringOrTestDictionary NativeValueTraits<StringOrTestDictionary>::NativeValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  StringOrTestDictionary impl;
  V8StringOrTestDictionary::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exception_state);
  return impl;
}

}  // namespace blink

