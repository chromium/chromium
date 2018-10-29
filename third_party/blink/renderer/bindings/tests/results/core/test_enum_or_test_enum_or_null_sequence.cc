// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/test_enum_or_test_enum_or_null_sequence.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"

namespace blink {

TestEnumOrTestEnumOrNullSequence::TestEnumOrTestEnumOrNullSequence() : type_(SpecificType::kNone) {}

const String& TestEnumOrTestEnumOrNullSequence::GetAsTestEnum() const {
  DCHECK(IsTestEnum());
  return test_enum_;
}

void TestEnumOrTestEnumOrNullSequence::SetTestEnum(const String& value) {
  DCHECK(IsNull());
  NonThrowableExceptionState exceptionState;
  const char* validValues[] = {
      "",
      "EnumValue1",
      "EnumValue2",
      "EnumValue3",
  };
  if (!IsValidEnum(value, validValues, base::size(validValues), "TestEnum", exceptionState)) {
    NOTREACHED();
    return;
  }
  test_enum_ = value;
  type_ = SpecificType::kTestEnum;
}

TestEnumOrTestEnumOrNullSequence TestEnumOrTestEnumOrNullSequence::FromTestEnum(const String& value) {
  TestEnumOrTestEnumOrNullSequence container;
  container.SetTestEnum(value);
  return container;
}

const Vector<String>& TestEnumOrTestEnumOrNullSequence::GetAsTestEnumOrNullSequence() const {
  DCHECK(IsTestEnumOrNullSequence());
  return test_enum_or_null_sequence_;
}

void TestEnumOrTestEnumOrNullSequence::SetTestEnumOrNullSequence(const Vector<String>& value) {
  DCHECK(IsNull());
  NonThrowableExceptionState exceptionState;
  const char* validValues[] = {
      nullptr,
      "",
      "EnumValue1",
      "EnumValue2",
      "EnumValue3",
  };
  if (!IsValidEnum(value, validValues, base::size(validValues), "TestEnum", exceptionState)) {
    NOTREACHED();
    return;
  }
  test_enum_or_null_sequence_ = value;
  type_ = SpecificType::kTestEnumOrNullSequence;
}

TestEnumOrTestEnumOrNullSequence TestEnumOrTestEnumOrNullSequence::FromTestEnumOrNullSequence(const Vector<String>& value) {
  TestEnumOrTestEnumOrNullSequence container;
  container.SetTestEnumOrNullSequence(value);
  return container;
}

TestEnumOrTestEnumOrNullSequence::TestEnumOrTestEnumOrNullSequence(const TestEnumOrTestEnumOrNullSequence&) = default;
TestEnumOrTestEnumOrNullSequence::~TestEnumOrTestEnumOrNullSequence() = default;
TestEnumOrTestEnumOrNullSequence& TestEnumOrTestEnumOrNullSequence::operator=(const TestEnumOrTestEnumOrNullSequence&) = default;

void TestEnumOrTestEnumOrNullSequence::Trace(blink::Visitor* visitor) {
}

void V8TestEnumOrTestEnumOrNullSequence::ToImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, TestEnumOrTestEnumOrNullSequence& impl, UnionTypeConversionMode conversionMode, ExceptionState& exceptionState) {
  if (v8Value.IsEmpty())
    return;

  if (conversionMode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8Value))
    return;

  if (HasCallableIteratorSymbol(isolate, v8Value, exceptionState)) {
    Vector<String> cppValue = NativeValueTraits<IDLSequence<IDLStringOrNull>>::NativeValue(isolate, v8Value, exceptionState);
    if (exceptionState.HadException())
      return;
    const char* validValues[] = {
        nullptr,
        "",
        "EnumValue1",
        "EnumValue2",
        "EnumValue3",
    };
    if (!IsValidEnum(cppValue, validValues, base::size(validValues), "TestEnum", exceptionState))
      return;
    impl.SetTestEnumOrNullSequence(cppValue);
    return;
  }

  {
    V8StringResource<> cppValue = v8Value;
    if (!cppValue.Prepare(exceptionState))
      return;
    const char* validValues[] = {
        "",
        "EnumValue1",
        "EnumValue2",
        "EnumValue3",
    };
    if (!IsValidEnum(cppValue, validValues, base::size(validValues), "TestEnum", exceptionState))
      return;
    impl.SetTestEnum(cppValue);
    return;
  }
}

v8::Local<v8::Value> ToV8(const TestEnumOrTestEnumOrNullSequence& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case TestEnumOrTestEnumOrNullSequence::SpecificType::kNone:
      return v8::Null(isolate);
    case TestEnumOrTestEnumOrNullSequence::SpecificType::kTestEnum:
      return V8String(isolate, impl.GetAsTestEnum());
    case TestEnumOrTestEnumOrNullSequence::SpecificType::kTestEnumOrNullSequence:
      return ToV8(impl.GetAsTestEnumOrNullSequence(), creationContext, isolate);
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

TestEnumOrTestEnumOrNullSequence NativeValueTraits<TestEnumOrTestEnumOrNullSequence>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestEnumOrTestEnumOrNullSequence impl;
  V8TestEnumOrTestEnumOrNullSequence::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exceptionState);
  return impl;
}

}  // namespace blink
