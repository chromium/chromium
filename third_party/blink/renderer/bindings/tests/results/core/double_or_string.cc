// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/double_or_string.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"

namespace blink {

DoubleOrString::DoubleOrString() : type_(SpecificType::kNone) {}

double DoubleOrString::GetAsDouble() const {
  DCHECK(IsDouble());
  return double_;
}

void DoubleOrString::SetDouble(double value) {
  DCHECK(IsNull());
  double_ = value;
  type_ = SpecificType::kDouble;
}

DoubleOrString DoubleOrString::FromDouble(double value) {
  DoubleOrString container;
  container.SetDouble(value);
  return container;
}

const String& DoubleOrString::GetAsString() const {
  DCHECK(IsString());
  return string_;
}

void DoubleOrString::SetString(const String& value) {
  DCHECK(IsNull());
  string_ = value;
  type_ = SpecificType::kString;
}

DoubleOrString DoubleOrString::FromString(const String& value) {
  DoubleOrString container;
  container.SetString(value);
  return container;
}

DoubleOrString::DoubleOrString(const DoubleOrString&) = default;
DoubleOrString::~DoubleOrString() = default;
DoubleOrString& DoubleOrString::operator=(const DoubleOrString&) = default;

void DoubleOrString::Trace(blink::Visitor* visitor) {
}

void V8DoubleOrString::ToImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, DoubleOrString& impl, UnionTypeConversionMode conversionMode, ExceptionState& exceptionState) {
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
    impl.SetString(cppValue);
    return;
  }
}

v8::Local<v8::Value> ToV8(const DoubleOrString& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case DoubleOrString::SpecificType::kNone:
      return v8::Null(isolate);
    case DoubleOrString::SpecificType::kDouble:
      return v8::Number::New(isolate, impl.GetAsDouble());
    case DoubleOrString::SpecificType::kString:
      return V8String(isolate, impl.GetAsString());
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

DoubleOrString NativeValueTraits<DoubleOrString>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  DoubleOrString impl;
  V8DoubleOrString::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exceptionState);
  return impl;
}

}  // namespace blink
