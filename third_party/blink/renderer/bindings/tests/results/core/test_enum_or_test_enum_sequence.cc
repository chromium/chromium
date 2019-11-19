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
  NonThrowableExceptionState exception_state;
  const char* const kValidValues[] = {
      "",
      "EnumValue1",
      "EnumValue2",
      "EnumValue3",
  };
  if (!IsValidEnum(value, kValidValues, base::size(kValidValues), "TestEnum", exception_state)) {
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
  NonThrowableExceptionState exception_state;
  const char* const kValidValues[] = {
      "",
      "EnumValue1",
      "EnumValue2",
      "EnumValue3",
  };
  if (!IsValidEnum(value, kValidValues, base::size(kValidValues), "TestEnum", exception_state)) {
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

void V8TestEnumOrTestEnumSequence::ToImpl(
    v8::Isolate* isolate,
    v8::Local<v8::Value> v8_value,
    TestEnumOrTestEnumSequence& impl,
    UnionTypeConversionMode conversion_mode,
    ExceptionState& exception_state) {
  if (v8_value.IsEmpty())
    return;

  if (conversion_mode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8_value))
    return;

  if (HasCallableIteratorSymbol(isolate, v8_value, exception_state)) {
    Vector<String> cpp_value = NativeValueTraits<IDLSequence<IDLString>>::NativeValue(isolate, v8_value, exception_state);
    if (exception_state.HadException())
      return;
    const char* const kValidValues[] = {
        "",
        "EnumValue1",
        "EnumValue2",
        "EnumValue3",
    };
    if (!IsValidEnum(cpp_value, kValidValues, base::size(kValidValues), "TestEnum", exception_state))
      return;
    impl.SetTestEnumSequence(cpp_value);
    return;
  }

  {
    V8StringResource<> cpp_value = v8_value;
    if (!cpp_value.Prepare(exception_state))
      return;
    const char* const kValidValues[] = {
        "",
        "EnumValue1",
        "EnumValue2",
        "EnumValue3",
    };
    if (!IsValidEnum(cpp_value, kValidValues, base::size(kValidValues), "TestEnum", exception_state))
      return;
    impl.SetTestEnum(cpp_value);
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

TestEnumOrTestEnumSequence NativeValueTraits<TestEnumOrTestEnumSequence>::NativeValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  TestEnumOrTestEnumSequence impl;
  V8TestEnumOrTestEnumSequence::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exception_state);
  return impl;
}

}  // namespace blink
