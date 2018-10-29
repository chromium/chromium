// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/test_interface_2_or_uint8_array.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface_2.h"

namespace blink {

TestInterface2OrUint8Array::TestInterface2OrUint8Array() : type_(SpecificType::kNone) {}

TestInterface2* TestInterface2OrUint8Array::GetAsTestInterface2() const {
  DCHECK(IsTestInterface2());
  return test_interface_2_;
}

void TestInterface2OrUint8Array::SetTestInterface2(TestInterface2* value) {
  DCHECK(IsNull());
  test_interface_2_ = value;
  type_ = SpecificType::kTestInterface2;
}

TestInterface2OrUint8Array TestInterface2OrUint8Array::FromTestInterface2(TestInterface2* value) {
  TestInterface2OrUint8Array container;
  container.SetTestInterface2(value);
  return container;
}

NotShared<DOMUint8Array> TestInterface2OrUint8Array::GetAsUint8Array() const {
  DCHECK(IsUint8Array());
  return uint8_array_;
}

void TestInterface2OrUint8Array::SetUint8Array(NotShared<DOMUint8Array> value) {
  DCHECK(IsNull());
  uint8_array_ = Member<DOMUint8Array>(value.View());
  type_ = SpecificType::kUint8Array;
}

TestInterface2OrUint8Array TestInterface2OrUint8Array::FromUint8Array(NotShared<DOMUint8Array> value) {
  TestInterface2OrUint8Array container;
  container.SetUint8Array(value);
  return container;
}

TestInterface2OrUint8Array::TestInterface2OrUint8Array(const TestInterface2OrUint8Array&) = default;
TestInterface2OrUint8Array::~TestInterface2OrUint8Array() = default;
TestInterface2OrUint8Array& TestInterface2OrUint8Array::operator=(const TestInterface2OrUint8Array&) = default;

void TestInterface2OrUint8Array::Trace(blink::Visitor* visitor) {
  visitor->Trace(test_interface_2_);
  visitor->Trace(uint8_array_);
}

void V8TestInterface2OrUint8Array::ToImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, TestInterface2OrUint8Array& impl, UnionTypeConversionMode conversionMode, ExceptionState& exceptionState) {
  if (v8Value.IsEmpty())
    return;

  if (conversionMode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8Value))
    return;

  if (V8TestInterface2::hasInstance(v8Value, isolate)) {
    TestInterface2* cppValue = V8TestInterface2::ToImpl(v8::Local<v8::Object>::Cast(v8Value));
    impl.SetTestInterface2(cppValue);
    return;
  }

  if (v8Value->IsUint8Array()) {
    NotShared<DOMUint8Array> cppValue = ToNotShared<NotShared<DOMUint8Array>>(isolate, v8Value, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.SetUint8Array(cppValue);
    return;
  }

  exceptionState.ThrowTypeError("The provided value is not of type '(TestInterface2 or Uint8Array)'");
}

v8::Local<v8::Value> ToV8(const TestInterface2OrUint8Array& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case TestInterface2OrUint8Array::SpecificType::kNone:
      return v8::Null(isolate);
    case TestInterface2OrUint8Array::SpecificType::kTestInterface2:
      return ToV8(impl.GetAsTestInterface2(), creationContext, isolate);
    case TestInterface2OrUint8Array::SpecificType::kUint8Array:
      return ToV8(impl.GetAsUint8Array(), creationContext, isolate);
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

TestInterface2OrUint8Array NativeValueTraits<TestInterface2OrUint8Array>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestInterface2OrUint8Array impl;
  V8TestInterface2OrUint8Array::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exceptionState);
  return impl;
}

}  // namespace blink
