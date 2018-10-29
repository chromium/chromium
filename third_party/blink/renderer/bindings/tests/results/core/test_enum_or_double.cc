// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/test_enum_or_double.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"

namespace blink {

TestEnumOrDouble::TestEnumOrDouble() : type_(SpecificType::kNone) {}

double TestEnumOrDouble::GetAsDouble() const {
  DCHECK(IsDouble());
  return double_;
}

void TestEnumOrDouble::SetDouble(double value) {
  DCHECK(IsNull());
  double_ = value;
  type_ = SpecificType::kDouble;
}

TestEnumOrDouble TestEnumOrDouble::FromDouble(double value) {
  TestEnumOrDouble container;
  container.SetDouble(value);
  return container;
}

const String& TestEnumOrDouble::GetAsTestEnum() const {
  DCHECK(IsTestEnum());
  return test_enum_;
}

void TestEnumOrDouble::SetTestEnum(const String& value) {
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

TestEnumOrDouble TestEnumOrDouble::FromTestEnum(const String& value) {
  TestEnumOrDouble container;
  container.SetTestEnum(value);
  return container;
}

TestEnumOrDouble::TestEnumOrDouble(const TestEnumOrDouble&) = default;
TestEnumOrDouble::~TestEnumOrDouble() = default;
TestEnumOrDouble& TestEnumOrDouble::operator=(const TestEnumOrDouble&) = default;

void TestEnumOrDouble::Trace(blink::Visitor* visitor) {
}

void V8TestEnumOrDouble::ToImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, TestEnumOrDouble& impl, UnionTypeConversionMode conversionMode, ExceptionState& exceptionState) {
  if (v8Value.IsEmpty())
    return;

  if (conversionMode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8Value))
    return;

  if (v8Value->IsNumber()) {
    double cppValue = NativeValueTraits<IDLDouble>::NativeValue(isolate, v8Value, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.SetDouble(cppValue);
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

v8::Local<v8::Value> ToV8(const TestEnumOrDouble& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case TestEnumOrDouble::SpecificType::kNone:
      return v8::Null(isolate);
    case TestEnumOrDouble::SpecificType::kDouble:
      return v8::Number::New(isolate, impl.GetAsDouble());
    case TestEnumOrDouble::SpecificType::kTestEnum:
      return V8String(isolate, impl.GetAsTestEnum());
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

TestEnumOrDouble NativeValueTraits<TestEnumOrDouble>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestEnumOrDouble impl;
  V8TestEnumOrDouble::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exceptionState);
  return impl;
}

}  // namespace blink
