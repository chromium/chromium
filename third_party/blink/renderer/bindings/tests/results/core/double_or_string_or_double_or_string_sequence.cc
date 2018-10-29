// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/double_or_string_or_double_or_string_sequence.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/double_or_string.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
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

void DoubleOrStringOrDoubleOrStringSequence::Trace(blink::Visitor* visitor) {
  visitor->Trace(double_or_string_sequence_);
}

void V8DoubleOrStringOrDoubleOrStringSequence::ToImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, DoubleOrStringOrDoubleOrStringSequence& impl, UnionTypeConversionMode conversionMode, ExceptionState& exceptionState) {
  if (v8Value.IsEmpty())
    return;

  if (conversionMode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8Value))
    return;

  if (HasCallableIteratorSymbol(isolate, v8Value, exceptionState)) {
    HeapVector<DoubleOrString> cppValue = NativeValueTraits<IDLSequence<DoubleOrString>>::NativeValue(isolate, v8Value, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.SetDoubleOrStringSequence(cppValue);
    return;
  }

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

DoubleOrStringOrDoubleOrStringSequence NativeValueTraits<DoubleOrStringOrDoubleOrStringSequence>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  DoubleOrStringOrDoubleOrStringSequence impl;
  V8DoubleOrStringOrDoubleOrStringSequence::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exceptionState);
  return impl;
}

}  // namespace blink
