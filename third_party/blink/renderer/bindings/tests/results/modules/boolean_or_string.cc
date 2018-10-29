// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/modules/boolean_or_string.h"

#include "base/stl_util.h"
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

void BooleanOrString::Trace(blink::Visitor* visitor) {
}

void V8BooleanOrString::ToImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, BooleanOrString& impl, UnionTypeConversionMode conversionMode, ExceptionState& exceptionState) {
  if (v8Value.IsEmpty())
    return;

  if (conversionMode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8Value))
    return;

  if (v8Value->IsBoolean()) {
    impl.SetBoolean(v8Value.As<v8::Boolean>()->Value());
    return;
  }

  {
    V8StringResource<> cppValue = v8Value;
    if (!cppValue.Prepare(exceptionState))
      return;
    impl.SetString(cppValue);
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

BooleanOrString NativeValueTraits<BooleanOrString>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  BooleanOrString impl;
  V8BooleanOrString::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exceptionState);
  return impl;
}

}  // namespace blink
