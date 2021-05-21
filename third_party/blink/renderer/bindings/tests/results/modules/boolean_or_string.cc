// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/modules/boolean_or_string.h"

#include "base/cxx17_backports.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"

namespace blink {

BooleanOrString::BooleanOrString() : type_(SpecificType::kNone) {}

bool BooleanOrString::GetAsBoolean() const {
  DCHECK(IsBoolean());
  return boolean_;
}

void BooleanOrString::SetBoolean(bool value) {
  DCHECK(IsNull());
  boolean_ = value;
  type_ = SpecificType::kBoolean;
}

BooleanOrString BooleanOrString::FromBoolean(bool value) {
  BooleanOrString container;
  container.SetBoolean(value);
  return container;
}

const String& BooleanOrString::GetAsString() const {
  DCHECK(IsString());
  return string_;
}

void BooleanOrString::SetString(const String& value) {
  DCHECK(IsNull());
  string_ = value;
  type_ = SpecificType::kString;
}

BooleanOrString BooleanOrString::FromString(const String& value) {
  BooleanOrString container;
  container.SetString(value);
  return container;
}

BooleanOrString::BooleanOrString(const BooleanOrString&) = default;
BooleanOrString::~BooleanOrString() = default;
BooleanOrString& BooleanOrString::operator=(const BooleanOrString&) = default;

void BooleanOrString::Trace(Visitor* visitor) const {
}

void V8BooleanOrString::ToImpl(
    v8::Isolate* isolate,
    v8::Local<v8::Value> v8_value,
    BooleanOrString& impl,
    UnionTypeConversionMode conversion_mode,
    ExceptionState& exception_state) {
  if (v8_value.IsEmpty())
    return;

  if (conversion_mode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8_value))
    return;

  if (v8_value->IsBoolean()) {
    impl.SetBoolean(v8_value.As<v8::Boolean>()->Value());
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

v8::Local<v8::Value> ToV8(const BooleanOrString& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case BooleanOrString::SpecificType::kNone:
      return v8::Null(isolate);
    case BooleanOrString::SpecificType::kBoolean:
      return v8::Boolean::New(isolate, impl.GetAsBoolean());
    case BooleanOrString::SpecificType::kString:
      return V8String(isolate, impl.GetAsString());
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

BooleanOrString NativeValueTraits<BooleanOrString>::NativeValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  BooleanOrString impl;
  V8BooleanOrString::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exception_state);
  return impl;
}

}  // namespace blink

