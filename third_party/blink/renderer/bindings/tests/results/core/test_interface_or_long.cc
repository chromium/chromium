// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/test_interface_or_long.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface.h"
#include "third_party/blink/renderer/bindings/tests/idls/core/test_interface_mixin_2.h"
#include "third_party/blink/renderer/bindings/tests/idls/core/test_interface_partial.h"
#include "third_party/blink/renderer/bindings/tests/idls/core/test_interface_partial_2_implementation.h"
#include "third_party/blink/renderer/bindings/tests/idls/core/test_interface_partial_secure_context.h"
#include "third_party/blink/renderer/bindings/tests/idls/core/test_mixin_3_implementation.h"

namespace blink {

TestInterfaceOrLong::TestInterfaceOrLong() : type_(SpecificType::kNone) {}

int32_t TestInterfaceOrLong::GetAsLong() const {
  DCHECK(IsLong());
  return long_;
}

void TestInterfaceOrLong::SetLong(int32_t value) {
  DCHECK(IsNull());
  long_ = value;
  type_ = SpecificType::kLong;
}

TestInterfaceOrLong TestInterfaceOrLong::FromLong(int32_t value) {
  TestInterfaceOrLong container;
  container.SetLong(value);
  return container;
}

TestInterfaceImplementation* TestInterfaceOrLong::GetAsTestInterface() const {
  DCHECK(IsTestInterface());
  return test_interface_;
}

void TestInterfaceOrLong::SetTestInterface(TestInterfaceImplementation* value) {
  DCHECK(IsNull());
  test_interface_ = value;
  type_ = SpecificType::kTestInterface;
}

TestInterfaceOrLong TestInterfaceOrLong::FromTestInterface(TestInterfaceImplementation* value) {
  TestInterfaceOrLong container;
  container.SetTestInterface(value);
  return container;
}

TestInterfaceOrLong::TestInterfaceOrLong(const TestInterfaceOrLong&) = default;
TestInterfaceOrLong::~TestInterfaceOrLong() = default;
TestInterfaceOrLong& TestInterfaceOrLong::operator=(const TestInterfaceOrLong&) = default;

void TestInterfaceOrLong::Trace(blink::Visitor* visitor) {
  visitor->Trace(test_interface_);
}

void V8TestInterfaceOrLong::ToImpl(
    v8::Isolate* isolate,
    v8::Local<v8::Value> v8_value,
    TestInterfaceOrLong& impl,
    UnionTypeConversionMode conversion_mode,
    ExceptionState& exception_state) {
  if (v8_value.IsEmpty())
    return;

  if (conversion_mode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8_value))
    return;

  if (V8TestInterface::HasInstance(v8_value, isolate)) {
    TestInterfaceImplementation* cpp_value = V8TestInterface::ToImpl(v8::Local<v8::Object>::Cast(v8_value));
    impl.SetTestInterface(cpp_value);
    return;
  }

  if (v8_value->IsNumber()) {
    int32_t cpp_value = NativeValueTraits<IDLLong>::NativeValue(isolate, v8_value, exception_state);
    if (exception_state.HadException())
      return;
    impl.SetLong(cpp_value);
    return;
  }

  {
    int32_t cpp_value = NativeValueTraits<IDLLong>::NativeValue(isolate, v8_value, exception_state);
    if (exception_state.HadException())
      return;
    impl.SetLong(cpp_value);
    return;
  }
}

v8::Local<v8::Value> ToV8(const TestInterfaceOrLong& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case TestInterfaceOrLong::SpecificType::kNone:
      return v8::Null(isolate);
    case TestInterfaceOrLong::SpecificType::kLong:
      return v8::Integer::New(isolate, impl.GetAsLong());
    case TestInterfaceOrLong::SpecificType::kTestInterface:
      return ToV8(impl.GetAsTestInterface(), creationContext, isolate);
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

TestInterfaceOrLong NativeValueTraits<TestInterfaceOrLong>::NativeValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  TestInterfaceOrLong impl;
  V8TestInterfaceOrLong::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exception_state);
  return impl;
}

}  // namespace blink
