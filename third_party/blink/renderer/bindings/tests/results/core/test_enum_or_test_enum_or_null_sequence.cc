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
  NonThrowableExceptionState exception_state;
  const char* const kValidValues[] = {
      nullptr,
      "",
      "EnumValue1",
      "EnumValue2",
      "EnumValue3",
  };
  if (!IsValidEnum(value, kValidValues, base::size(kValidValues), "TestEnum", exception_state)) {
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

void V8TestEnumOrTestEnumOrNullSequence::ToImpl(
    v8::Isolate* isolate,
    v8::Local<v8::Value> v8_value,
    TestEnumOrTestEnumOrNullSequence& impl,
    UnionTypeConversionMode conversion_mode,
    ExceptionState& exception_state) {
  if (v8_value.IsEmpty())
    return;

  if (conversion_mode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8_value))
    return;

  if (HasCallableIteratorSymbol(isolate, v8_value, exception_state)) {
    Vector<String> cpp_value = NativeValueTraits<IDLSequence<IDLStringOrNull>>::NativeValue(isolate, v8_value, exception_state);
    if (exception_state.HadException())
      return;
    const char* const kValidValues[] = {
        nullptr,
        "",
        "EnumValue1",
        "EnumValue2",
        "EnumValue3",
    };
    if (!IsValidEnum(cpp_value, kValidValues, base::size(kValidValues), "TestEnum", exception_state))
      return;
    impl.SetTestEnumOrNullSequence(cpp_value);
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

TestEnumOrTestEnumOrNullSequence NativeValueTraits<TestEnumOrTestEnumOrNullSequence>::NativeValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  TestEnumOrTestEnumOrNullSequence impl;
  V8TestEnumOrTestEnumOrNullSequence::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exception_state);
  return impl;
}

}  // namespace blink
