// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/double_or_string_or_double_or_string_sequence.h"

#include "base/cxx17_backports.h"
#include "third_party/blink/renderer/bindings/core/v8/double_or_string.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_iterator.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"

namespace blink {

DoubleOrStringOrDoubleOrStringSequence::DoubleOrStringOrDoubleOrStringSequence() : type_(SpecificType::kNone) {}

double DoubleOrStringOrDoubleOrStringSequence::GetAsDouble() const {
  DCHECK(IsDouble());
  return double_;
}

void DoubleOrStringOrDoubleOrStringSequence::SetDouble(double value) {
  DCHECK(IsNull());
  double_ = value;
  type_ = SpecificType::kDouble;
}

DoubleOrStringOrDoubleOrStringSequence DoubleOrStringOrDoubleOrStringSequence::FromDouble(double value) {
  DoubleOrStringOrDoubleOrStringSequence container;
  container.SetDouble(value);
  return container;
}

const HeapVector<DoubleOrString>& DoubleOrStringOrDoubleOrStringSequence::GetAsDoubleOrStringSequence() const {
  DCHECK(IsDoubleOrStringSequence());
  return double_or_string_sequence_;
}

void DoubleOrStringOrDoubleOrStringSequence::SetDoubleOrStringSequence(const HeapVector<DoubleOrString>& value) {
  DCHECK(IsNull());
  double_or_string_sequence_ = value;
  type_ = SpecificType::kDoubleOrStringSequence;
}

DoubleOrStringOrDoubleOrStringSequence DoubleOrStringOrDoubleOrStringSequence::FromDoubleOrStringSequence(const HeapVector<DoubleOrString>& value) {
  DoubleOrStringOrDoubleOrStringSequence container;
  container.SetDoubleOrStringSequence(value);
  return container;
}

const String& DoubleOrStringOrDoubleOrStringSequence::GetAsString() const {
  DCHECK(IsString());
  return string_;
}

void DoubleOrStringOrDoubleOrStringSequence::SetString(const String& value) {
  DCHECK(IsNull());
  string_ = value;
  type_ = SpecificType::kString;
}

DoubleOrStringOrDoubleOrStringSequence DoubleOrStringOrDoubleOrStringSequence::FromString(const String& value) {
  DoubleOrStringOrDoubleOrStringSequence container;
  container.SetString(value);
  return container;
}

DoubleOrStringOrDoubleOrStringSequence::DoubleOrStringOrDoubleOrStringSequence(const DoubleOrStringOrDoubleOrStringSequence&) = default;
DoubleOrStringOrDoubleOrStringSequence::~DoubleOrStringOrDoubleOrStringSequence() = default;
DoubleOrStringOrDoubleOrStringSequence& DoubleOrStringOrDoubleOrStringSequence::operator=(const DoubleOrStringOrDoubleOrStringSequence&) = default;

void DoubleOrStringOrDoubleOrStringSequence::Trace(Visitor* visitor) const {
  visitor->Trace(double_or_string_sequence_);
}

void V8DoubleOrStringOrDoubleOrStringSequence::ToImpl(
    v8::Isolate* isolate,
    v8::Local<v8::Value> v8_value,
    DoubleOrStringOrDoubleOrStringSequence& impl,
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
      HeapVector<DoubleOrString> cpp_value{ NativeValueTraits<IDLSequence<DoubleOrString>>::NativeValue(isolate, std::move(script_iterator), exception_state) };
      if (exception_state.HadException())
        return;
      impl.SetDoubleOrStringSequence(cpp_value);
      return;
    }
  }

  if (v8_value->IsNumber()) {
    double cpp_value{ NativeValueTraits<IDLDouble>::NativeValue(isolate, v8_value, exception_state) };
    if (exception_state.HadException())
      return;
    impl.SetDouble(cpp_value);
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

v8::Local<v8::Value> ToV8(const DoubleOrStringOrDoubleOrStringSequence& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case DoubleOrStringOrDoubleOrStringSequence::SpecificType::kNone:
      return v8::Null(isolate);
    case DoubleOrStringOrDoubleOrStringSequence::SpecificType::kDouble:
      return v8::Number::New(isolate, impl.GetAsDouble());
    case DoubleOrStringOrDoubleOrStringSequence::SpecificType::kDoubleOrStringSequence:
      return ToV8(impl.GetAsDoubleOrStringSequence(), creationContext, isolate);
    case DoubleOrStringOrDoubleOrStringSequence::SpecificType::kString:
      return V8String(isolate, impl.GetAsString());
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

DoubleOrStringOrDoubleOrStringSequence NativeValueTraits<DoubleOrStringOrDoubleOrStringSequence>::NativeValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  DoubleOrStringOrDoubleOrStringSequence impl;
  V8DoubleOrStringOrDoubleOrStringSequence::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exception_state);
  return impl;
}

}  // namespace blink

