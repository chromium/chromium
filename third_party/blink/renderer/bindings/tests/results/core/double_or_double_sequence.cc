// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/double_or_double_sequence.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"

namespace blink {

DoubleOrDoubleSequence::DoubleOrDoubleSequence() : type_(SpecificType::kNone) {}

double DoubleOrDoubleSequence::GetAsDouble() const {
  DCHECK(IsDouble());
  return double_;
}

void DoubleOrDoubleSequence::SetDouble(double value) {
  DCHECK(IsNull());
  double_ = value;
  type_ = SpecificType::kDouble;
}

DoubleOrDoubleSequence DoubleOrDoubleSequence::FromDouble(double value) {
  DoubleOrDoubleSequence container;
  container.SetDouble(value);
  return container;
}

const Vector<double>& DoubleOrDoubleSequence::GetAsDoubleSequence() const {
  DCHECK(IsDoubleSequence());
  return double_sequence_;
}

void DoubleOrDoubleSequence::SetDoubleSequence(const Vector<double>& value) {
  DCHECK(IsNull());
  double_sequence_ = value;
  type_ = SpecificType::kDoubleSequence;
}

DoubleOrDoubleSequence DoubleOrDoubleSequence::FromDoubleSequence(const Vector<double>& value) {
  DoubleOrDoubleSequence container;
  container.SetDoubleSequence(value);
  return container;
}

DoubleOrDoubleSequence::DoubleOrDoubleSequence(const DoubleOrDoubleSequence&) = default;
DoubleOrDoubleSequence::~DoubleOrDoubleSequence() = default;
DoubleOrDoubleSequence& DoubleOrDoubleSequence::operator=(const DoubleOrDoubleSequence&) = default;

void DoubleOrDoubleSequence::Trace(blink::Visitor* visitor) {
}

void V8DoubleOrDoubleSequence::ToImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, DoubleOrDoubleSequence& impl, UnionTypeConversionMode conversionMode, ExceptionState& exceptionState) {
  if (v8Value.IsEmpty())
    return;

  if (conversionMode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8Value))
    return;

  if (HasCallableIteratorSymbol(isolate, v8Value, exceptionState)) {
    Vector<double> cppValue = NativeValueTraits<IDLSequence<IDLDouble>>::NativeValue(isolate, v8Value, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.SetDoubleSequence(cppValue);
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
    double cppValue = NativeValueTraits<IDLDouble>::NativeValue(isolate, v8Value, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.SetDouble(cppValue);
    return;
  }
}

v8::Local<v8::Value> ToV8(const DoubleOrDoubleSequence& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case DoubleOrDoubleSequence::SpecificType::kNone:
      return v8::Null(isolate);
    case DoubleOrDoubleSequence::SpecificType::kDouble:
      return v8::Number::New(isolate, impl.GetAsDouble());
    case DoubleOrDoubleSequence::SpecificType::kDoubleSequence:
      return ToV8(impl.GetAsDoubleSequence(), creationContext, isolate);
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

DoubleOrDoubleSequence NativeValueTraits<DoubleOrDoubleSequence>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  DoubleOrDoubleSequence impl;
  V8DoubleOrDoubleSequence::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exceptionState);
  return impl;
}

}  // namespace blink
