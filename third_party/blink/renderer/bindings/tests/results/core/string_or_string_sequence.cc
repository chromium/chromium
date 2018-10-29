// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/string_or_string_sequence.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"

namespace blink {

StringOrStringSequence::StringOrStringSequence() : type_(SpecificType::kNone) {}

const String& StringOrStringSequence::GetAsString() const {
  DCHECK(IsString());
  return string_;
}

void StringOrStringSequence::SetString(const String& value) {
  DCHECK(IsNull());
  string_ = value;
  type_ = SpecificType::kString;
}

StringOrStringSequence StringOrStringSequence::FromString(const String& value) {
  StringOrStringSequence container;
  container.SetString(value);
  return container;
}

const Vector<String>& StringOrStringSequence::GetAsStringSequence() const {
  DCHECK(IsStringSequence());
  return string_sequence_;
}

void StringOrStringSequence::SetStringSequence(const Vector<String>& value) {
  DCHECK(IsNull());
  string_sequence_ = value;
  type_ = SpecificType::kStringSequence;
}

StringOrStringSequence StringOrStringSequence::FromStringSequence(const Vector<String>& value) {
  StringOrStringSequence container;
  container.SetStringSequence(value);
  return container;
}

StringOrStringSequence::StringOrStringSequence(const StringOrStringSequence&) = default;
StringOrStringSequence::~StringOrStringSequence() = default;
StringOrStringSequence& StringOrStringSequence::operator=(const StringOrStringSequence&) = default;

void StringOrStringSequence::Trace(blink::Visitor* visitor) {
}

void V8StringOrStringSequence::ToImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, StringOrStringSequence& impl, UnionTypeConversionMode conversionMode, ExceptionState& exceptionState) {
  if (v8Value.IsEmpty())
    return;

  if (conversionMode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8Value))
    return;

  if (HasCallableIteratorSymbol(isolate, v8Value, exceptionState)) {
    Vector<String> cppValue = NativeValueTraits<IDLSequence<IDLString>>::NativeValue(isolate, v8Value, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.SetStringSequence(cppValue);
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

v8::Local<v8::Value> ToV8(const StringOrStringSequence& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case StringOrStringSequence::SpecificType::kNone:
      return v8::Null(isolate);
    case StringOrStringSequence::SpecificType::kString:
      return V8String(isolate, impl.GetAsString());
    case StringOrStringSequence::SpecificType::kStringSequence:
      return ToV8(impl.GetAsStringSequence(), creationContext, isolate);
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

StringOrStringSequence NativeValueTraits<StringOrStringSequence>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  StringOrStringSequence impl;
  V8StringOrStringSequence::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exceptionState);
  return impl;
}

}  // namespace blink
