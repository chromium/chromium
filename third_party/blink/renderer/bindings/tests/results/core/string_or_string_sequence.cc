// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/string_or_string_sequence.h"

#include "base/cxx17_backports.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_iterator.h"
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

void StringOrStringSequence::Trace(Visitor* visitor) const {
}

void V8StringOrStringSequence::ToImpl(
    v8::Isolate* isolate,
    v8::Local<v8::Value> v8_value,
    StringOrStringSequence& impl,
    UnionTypeConversionMode conversion_mode,
    ExceptionState& exception_state) {
  if (v8_value.IsEmpty())
    return;

  if (conversion_mode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8_value))
    return;

  if (v8_value->IsObject()) {
    ScriptIterator script_iterator = ScriptIterator::FromIterable(
        isolate, v8_value.As<v8::Object>(), exception_state);
    if (exception_state.HadException())
      return;
    if (!script_iterator.IsNull()) {
      Vector<String> cpp_value{ NativeValueTraits<IDLSequence<IDLString>>::NativeValue(isolate, std::move(script_iterator), exception_state) };
      if (exception_state.HadException())
        return;
      impl.SetStringSequence(cpp_value);
      return;
    }
  }

  {
    V8StringResource<> cpp_value{ v8_value };
    if (!cpp_value.Prepare(exception_state))
      return;
    impl.SetString(cpp_value);
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

StringOrStringSequence NativeValueTraits<StringOrStringSequence>::NativeValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  StringOrStringSequence impl;
  V8StringOrStringSequence::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exception_state);
  return impl;
}

}  // namespace blink

