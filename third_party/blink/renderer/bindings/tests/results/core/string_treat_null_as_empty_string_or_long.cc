// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/string_treat_null_as_empty_string_or_long.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"

namespace blink {

StringTreatNullAsEmptyStringOrLong::StringTreatNullAsEmptyStringOrLong() : type_(SpecificType::kNone) {}

int32_t StringTreatNullAsEmptyStringOrLong::GetAsLong() const {
  DCHECK(IsLong());
  return long_;
}

void StringTreatNullAsEmptyStringOrLong::SetLong(int32_t value) {
  DCHECK(IsNull());
  long_ = value;
  type_ = SpecificType::kLong;
}

StringTreatNullAsEmptyStringOrLong StringTreatNullAsEmptyStringOrLong::FromLong(int32_t value) {
  StringTreatNullAsEmptyStringOrLong container;
  container.SetLong(value);
  return container;
}

const String& StringTreatNullAsEmptyStringOrLong::GetAsString() const {
  DCHECK(IsString());
  return string_treat_null_as_empty_string_;
}

void StringTreatNullAsEmptyStringOrLong::SetString(const String& value) {
  DCHECK(IsNull());
  string_treat_null_as_empty_string_ = value;
  type_ = SpecificType::kStringTreatNullAsEmptyString;
}

StringTreatNullAsEmptyStringOrLong StringTreatNullAsEmptyStringOrLong::FromString(const String& value) {
  StringTreatNullAsEmptyStringOrLong container;
  container.SetString(value);
  return container;
}

StringTreatNullAsEmptyStringOrLong::StringTreatNullAsEmptyStringOrLong(const StringTreatNullAsEmptyStringOrLong&) = default;
StringTreatNullAsEmptyStringOrLong::~StringTreatNullAsEmptyStringOrLong() = default;
StringTreatNullAsEmptyStringOrLong& StringTreatNullAsEmptyStringOrLong::operator=(const StringTreatNullAsEmptyStringOrLong&) = default;

void StringTreatNullAsEmptyStringOrLong::Trace(blink::Visitor* visitor) {
}

void V8StringTreatNullAsEmptyStringOrLong::ToImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, StringTreatNullAsEmptyStringOrLong& impl, UnionTypeConversionMode conversionMode, ExceptionState& exceptionState) {
  if (v8Value.IsEmpty())
    return;

  if (conversionMode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8Value))
    return;

  if (v8Value->IsNumber()) {
    int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(isolate, v8Value, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.SetLong(cppValue);
    return;
  }

  {
    V8StringResource<kTreatNullAsEmptyString> cppValue = v8Value;
    if (!cppValue.Prepare(exceptionState))
      return;
    impl.SetString(cppValue);
    return;
  }
}

v8::Local<v8::Value> ToV8(const StringTreatNullAsEmptyStringOrLong& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case StringTreatNullAsEmptyStringOrLong::SpecificType::kNone:
      return v8::Null(isolate);
    case StringTreatNullAsEmptyStringOrLong::SpecificType::kLong:
      return v8::Integer::New(isolate, impl.GetAsLong());
    case StringTreatNullAsEmptyStringOrLong::SpecificType::kStringTreatNullAsEmptyString:
      return V8String(isolate, impl.GetAsStringTreatNullAsEmptyString());
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

StringTreatNullAsEmptyStringOrLong NativeValueTraits<StringTreatNullAsEmptyStringOrLong>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  StringTreatNullAsEmptyStringOrLong impl;
  V8StringTreatNullAsEmptyStringOrLong::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exceptionState);
  return impl;
}

}  // namespace blink
