// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/test_enum_or_test_enum_sequence.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"

namespace blink {

TestEnumOrTestEnumSequence::TestEnumOrTestEnumSequence() : type_(SpecificType::kNone) {}

const String& TestEnumOrTestEnumSequence::GetAsTestEnum() const {
  DCHECK(IsTestEnum());
  return test_enum_;
}

void TestEnumOrTestEnumSequence::SetTestEnum(const String& value) {
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

TestEnumOrTestEnumSequence TestEnumOrTestEnumSequence::FromTestEnum(const String& value) {
  TestEnumOrTestEnumSequence container;
  container.SetTestEnum(value);
  return container;
}

const Vector<String>& TestEnumOrTestEnumSequence::GetAsTestEnumSequence() const {
  DCHECK(IsTestEnumSequence());
  return test_enum_sequence_;
}

void TestEnumOrTestEnumSequence::SetTestEnumSequence(const Vector<String>& value) {
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
  test_enum_sequence_ = value;
  type_ = SpecificType::kTestEnumSequence;
}

TestEnumOrTestEnumSequence TestEnumOrTestEnumSequence::FromTestEnumSequence(const Vector<String>& value) {
  TestEnumOrTestEnumSequence container;
  container.SetTestEnumSequence(value);
  return container;
}

TestEnumOrTestEnumSequence::TestEnumOrTestEnumSequence(const TestEnumOrTestEnumSequence&) = default;
TestEnumOrTestEnumSequence::~TestEnumOrTestEnumSequence() = default;
TestEnumOrTestEnumSequence& TestEnumOrTestEnumSequence::operator=(const TestEnumOrTestEnumSequence&) = default;

void TestEnumOrTestEnumSequence::Trace(blink::Visitor* visitor) {
}

void V8TestEnumOrTestEnumSequence::ToImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, TestEnumOrTestEnumSequence& impl, UnionTypeConversionMode conversionMode, ExceptionState& exceptionState) {
  if (v8Value.IsEmpty())
    return;

  if (conversionMode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8Value))
    return;

  if (HasCallableIteratorSymbol(isolate, v8Value, exceptionState)) {
    Vector<String> cppValue = NativeValueTraits<IDLSequence<IDLString>>::NativeValue(isolate, v8Value, exceptionState);
    if (exceptionState.HadException())
      return;
    const char* validValues[] = {
        "",
        "EnumValue1",
        "EnumValue2",
        "EnumValue3",
    };
    if (!IsValidEnum(cppValue, validValues, base::size(validValues), "TestEnum", exceptionState))
      return;
    impl.SetTestEnumSequence(cppValue);
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

v8::Local<v8::Value> ToV8(const TestEnumOrTestEnumSequence& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case TestEnumOrTestEnumSequence::SpecificType::kNone:
      return v8::Null(isolate);
    case TestEnumOrTestEnumSequence::SpecificType::kTestEnum:
      return V8String(isolate, impl.GetAsTestEnum());
    case TestEnumOrTestEnumSequence::SpecificType::kTestEnumSequence:
      return ToV8(impl.GetAsTestEnumSequence(), creationContext, isolate);
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

TestEnumOrTestEnumSequence NativeValueTraits<TestEnumOrTestEnumSequence>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestEnumOrTestEnumSequence impl;
  V8TestEnumOrTestEnumSequence::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exceptionState);
  return impl;
}

}  // namespace blink
