// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/unrestricted_double_or_string.h"

#include "base/cxx17_backports.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"

namespace blink {

UnrestrictedDoubleOrString::UnrestrictedDoubleOrString() : type_(SpecificType::kNone) {}

const String& UnrestrictedDoubleOrString::GetAsString() const {
  DCHECK(IsString());
  return string_;
}

void UnrestrictedDoubleOrString::SetString(const String& value) {
  DCHECK(IsNull());
  string_ = value;
  type_ = SpecificType::kString;
}

UnrestrictedDoubleOrString UnrestrictedDoubleOrString::FromString(const String& value) {
  UnrestrictedDoubleOrString container;
  container.SetString(value);
  return container;
}

double UnrestrictedDoubleOrString::GetAsUnrestrictedDouble() const {
  DCHECK(IsUnrestrictedDouble());
  return unrestricted_double_;
}

void UnrestrictedDoubleOrString::SetUnrestrictedDouble(double value) {
  DCHECK(IsNull());
  unrestricted_double_ = value;
  type_ = SpecificType::kUnrestrictedDouble;
}

UnrestrictedDoubleOrString UnrestrictedDoubleOrString::FromUnrestrictedDouble(double value) {
  UnrestrictedDoubleOrString container;
  container.SetUnrestrictedDouble(value);
  return container;
}

UnrestrictedDoubleOrString::UnrestrictedDoubleOrString(const UnrestrictedDoubleOrString&) = default;
UnrestrictedDoubleOrString::~UnrestrictedDoubleOrString() = default;
UnrestrictedDoubleOrString& UnrestrictedDoubleOrString::operator=(const UnrestrictedDoubleOrString&) = default;

void UnrestrictedDoubleOrString::Trace(Visitor* visitor) const {
}

void V8UnrestrictedDoubleOrString::ToImpl(
    v8::Isolate* isolate,
    v8::Local<v8::Value> v8_value,
    UnrestrictedDoubleOrString& impl,
    UnionTypeConversionMode conversion_mode,
    ExceptionState& exception_state) {
  if (v8_value.IsEmpty())
    return;

  if (conversion_mode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8_value))
    return;

  if (v8_value->IsNumber()) {
    double cpp_value{ NativeValueTraits<IDLUnrestrictedDouble>::NativeValue(isolate, v8_value, exception_state) };
    if (exception_state.HadException())
      return;
    impl.SetUnrestrictedDouble(cpp_value);
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

v8::Local<v8::Value> ToV8(const UnrestrictedDoubleOrString& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case UnrestrictedDoubleOrString::SpecificType::kNone:
      return v8::Null(isolate);
    case UnrestrictedDoubleOrString::SpecificType::kString:
      return V8String(isolate, impl.GetAsString());
    case UnrestrictedDoubleOrString::SpecificType::kUnrestrictedDouble:
      return v8::Number::New(isolate, impl.GetAsUnrestrictedDouble());
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

UnrestrictedDoubleOrString NativeValueTraits<UnrestrictedDoubleOrString>::NativeValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  UnrestrictedDoubleOrString impl;
  V8UnrestrictedDoubleOrString::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exception_state);
  return impl;
}

}  // namespace blink

