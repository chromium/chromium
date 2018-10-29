// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/test_interface_or_test_interface_empty.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface_empty.h"
#include "third_party/blink/renderer/bindings/tests/idls/core/test_implements_2.h"
#include "third_party/blink/renderer/bindings/tests/idls/core/test_implements_3_implementation.h"
#include "third_party/blink/renderer/bindings/tests/idls/core/test_interface_partial.h"
#include "third_party/blink/renderer/bindings/tests/idls/core/test_interface_partial_2_implementation.h"
#include "third_party/blink/renderer/bindings/tests/idls/core/test_interface_partial_secure_context.h"

namespace blink {

TestInterfaceOrTestInterfaceEmpty::TestInterfaceOrTestInterfaceEmpty() : type_(SpecificType::kNone) {}

TestInterfaceImplementation* TestInterfaceOrTestInterfaceEmpty::GetAsTestInterface() const {
  DCHECK(IsTestInterface());
  return test_interface_;
}

void TestInterfaceOrTestInterfaceEmpty::SetTestInterface(TestInterfaceImplementation* value) {
  DCHECK(IsNull());
  test_interface_ = value;
  type_ = SpecificType::kTestInterface;
}

TestInterfaceOrTestInterfaceEmpty TestInterfaceOrTestInterfaceEmpty::FromTestInterface(TestInterfaceImplementation* value) {
  TestInterfaceOrTestInterfaceEmpty container;
  container.SetTestInterface(value);
  return container;
}

TestInterfaceEmpty* TestInterfaceOrTestInterfaceEmpty::GetAsTestInterfaceEmpty() const {
  DCHECK(IsTestInterfaceEmpty());
  return test_interface_empty_;
}

void TestInterfaceOrTestInterfaceEmpty::SetTestInterfaceEmpty(TestInterfaceEmpty* value) {
  DCHECK(IsNull());
  test_interface_empty_ = value;
  type_ = SpecificType::kTestInterfaceEmpty;
}

TestInterfaceOrTestInterfaceEmpty TestInterfaceOrTestInterfaceEmpty::FromTestInterfaceEmpty(TestInterfaceEmpty* value) {
  TestInterfaceOrTestInterfaceEmpty container;
  container.SetTestInterfaceEmpty(value);
  return container;
}

TestInterfaceOrTestInterfaceEmpty::TestInterfaceOrTestInterfaceEmpty(const TestInterfaceOrTestInterfaceEmpty&) = default;
TestInterfaceOrTestInterfaceEmpty::~TestInterfaceOrTestInterfaceEmpty() = default;
TestInterfaceOrTestInterfaceEmpty& TestInterfaceOrTestInterfaceEmpty::operator=(const TestInterfaceOrTestInterfaceEmpty&) = default;

void TestInterfaceOrTestInterfaceEmpty::Trace(blink::Visitor* visitor) {
  visitor->Trace(test_interface_);
  visitor->Trace(test_interface_empty_);
}

void V8TestInterfaceOrTestInterfaceEmpty::ToImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, TestInterfaceOrTestInterfaceEmpty& impl, UnionTypeConversionMode conversionMode, ExceptionState& exceptionState) {
  if (v8Value.IsEmpty())
    return;

  if (conversionMode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8Value))
    return;

  if (V8TestInterface::hasInstance(v8Value, isolate)) {
    TestInterfaceImplementation* cppValue = V8TestInterface::ToImpl(v8::Local<v8::Object>::Cast(v8Value));
    impl.SetTestInterface(cppValue);
    return;
  }

  if (V8TestInterfaceEmpty::hasInstance(v8Value, isolate)) {
    TestInterfaceEmpty* cppValue = V8TestInterfaceEmpty::ToImpl(v8::Local<v8::Object>::Cast(v8Value));
    impl.SetTestInterfaceEmpty(cppValue);
    return;
  }

  exceptionState.ThrowTypeError("The provided value is not of type '(TestInterface or TestInterfaceEmpty)'");
}

v8::Local<v8::Value> ToV8(const TestInterfaceOrTestInterfaceEmpty& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case TestInterfaceOrTestInterfaceEmpty::SpecificType::kNone:
      return v8::Null(isolate);
    case TestInterfaceOrTestInterfaceEmpty::SpecificType::kTestInterface:
      return ToV8(impl.GetAsTestInterface(), creationContext, isolate);
    case TestInterfaceOrTestInterfaceEmpty::SpecificType::kTestInterfaceEmpty:
      return ToV8(impl.GetAsTestInterfaceEmpty(), creationContext, isolate);
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

TestInterfaceOrTestInterfaceEmpty NativeValueTraits<TestInterfaceOrTestInterfaceEmpty>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestInterfaceOrTestInterfaceEmpty impl;
  V8TestInterfaceOrTestInterfaceEmpty::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exceptionState);
  return impl;
}

}  // namespace blink
