// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/string_or_double.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"

namespace blink {

StringOrDouble::StringOrDouble() : type_(SpecificType::kNone) {}

double StringOrDouble::GetAsDouble() const {
  DCHECK(IsDouble());
  return double_;
}

void StringOrDouble::SetDouble(double value) {
  DCHECK(IsNull());
  double_ = value;
  type_ = SpecificType::kDouble;
}

StringOrDouble StringOrDouble::FromDouble(double value) {
  StringOrDouble container;
  container.SetDouble(value);
  return container;
}

const String& StringOrDouble::GetAsString() const {
  DCHECK(IsString());
  return string_;
}

void StringOrDouble::SetString(const String& value) {
  DCHECK(IsNull());
  string_ = value;
  type_ = SpecificType::kString;
}

StringOrDouble StringOrDouble::FromString(const String& value) {
  StringOrDouble container;
  container.SetString(value);
  return container;
}

StringOrDouble::StringOrDouble(const StringOrDouble&) = default;
StringOrDouble::~StringOrDouble() = default;
StringOrDouble& StringOrDouble::operator=(const StringOrDouble&) = default;

void StringOrDouble::Trace(blink::Visitor* visitor) {
}

void V8StringOrDouble::ToImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, StringOrDouble& impl, UnionTypeConversionMode conversionMode, ExceptionState& exceptionState) {
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

v8::Local<v8::Value> ToV8(const StringOrDouble& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case StringOrDouble::SpecificType::kNone:
      return v8::Null(isolate);
    case StringOrDouble::SpecificType::kDouble:
      return v8::Number::New(isolate, impl.GetAsDouble());
    case StringOrDouble::SpecificType::kString:
      return V8String(isolate, impl.GetAsString());
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

StringOrDouble NativeValueTraits<StringOrDouble>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  StringOrDouble impl;
  V8StringOrDouble::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exceptionState);
  return impl;
}

}  // namespace blink
