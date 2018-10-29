// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/boolean_or_test_callback_interface.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_callback_interface.h"

namespace blink {

BooleanOrTestCallbackInterface::BooleanOrTestCallbackInterface() : type_(SpecificType::kNone) {}

bool BooleanOrTestCallbackInterface::GetAsBoolean() const {
  DCHECK(IsBoolean());
  return boolean_;
}

void BooleanOrTestCallbackInterface::SetBoolean(bool value) {
  DCHECK(IsNull());
  boolean_ = value;
  type_ = SpecificType::kBoolean;
}

BooleanOrTestCallbackInterface BooleanOrTestCallbackInterface::FromBoolean(bool value) {
  BooleanOrTestCallbackInterface container;
  container.SetBoolean(value);
  return container;
}

V8TestCallbackInterface* BooleanOrTestCallbackInterface::GetAsTestCallbackInterface() const {
  DCHECK(IsTestCallbackInterface());
  return test_callback_interface_;
}

void BooleanOrTestCallbackInterface::SetTestCallbackInterface(V8TestCallbackInterface* value) {
  DCHECK(IsNull());
  test_callback_interface_ = value;
  type_ = SpecificType::kTestCallbackInterface;
}

BooleanOrTestCallbackInterface BooleanOrTestCallbackInterface::FromTestCallbackInterface(V8TestCallbackInterface* value) {
  BooleanOrTestCallbackInterface container;
  container.SetTestCallbackInterface(value);
  return container;
}

BooleanOrTestCallbackInterface::BooleanOrTestCallbackInterface(const BooleanOrTestCallbackInterface&) = default;
BooleanOrTestCallbackInterface::~BooleanOrTestCallbackInterface() = default;
BooleanOrTestCallbackInterface& BooleanOrTestCallbackInterface::operator=(const BooleanOrTestCallbackInterface&) = default;

void BooleanOrTestCallbackInterface::Trace(blink::Visitor* visitor) {
  visitor->Trace(test_callback_interface_);
}

void V8BooleanOrTestCallbackInterface::ToImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, BooleanOrTestCallbackInterface& impl, UnionTypeConversionMode conversionMode, ExceptionState& exceptionState) {
  if (v8Value.IsEmpty())
    return;

  if (conversionMode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8Value))
    return;

  if (V8TestCallbackInterface::hasInstance(v8Value, isolate)) {
    V8TestCallbackInterface* cppValue = V8TestCallbackInterface::ToImpl(v8::Local<v8::Object>::Cast(v8Value));
    impl.SetTestCallbackInterface(cppValue);
    return;
  }

  if (v8Value->IsBoolean()) {
    impl.SetBoolean(v8Value.As<v8::Boolean>()->Value());
    return;
  }

  {
    impl.SetBoolean(v8Value->BooleanValue(isolate->GetCurrentContext()).ToChecked());
    return;
  }
}

v8::Local<v8::Value> ToV8(const BooleanOrTestCallbackInterface& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case BooleanOrTestCallbackInterface::SpecificType::kNone:
      return v8::Null(isolate);
    case BooleanOrTestCallbackInterface::SpecificType::kBoolean:
      return v8::Boolean::New(isolate, impl.GetAsBoolean());
    case BooleanOrTestCallbackInterface::SpecificType::kTestCallbackInterface:
      return ToV8(impl.GetAsTestCallbackInterface(), creationContext, isolate);
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

BooleanOrTestCallbackInterface NativeValueTraits<BooleanOrTestCallbackInterface>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  BooleanOrTestCallbackInterface impl;
  V8BooleanOrTestCallbackInterface::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exceptionState);
  return impl;
}

}  // namespace blink
