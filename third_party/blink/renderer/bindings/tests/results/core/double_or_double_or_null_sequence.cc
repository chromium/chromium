// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/double_or_double_or_null_sequence.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"

namespace blink {

DoubleOrDoubleOrNullSequence::DoubleOrDoubleOrNullSequence() : type_(SpecificType::kNone) {}

double DoubleOrDoubleOrNullSequence::GetAsDouble() const {
  DCHECK(IsDouble());
  return double_;
}

void DoubleOrDoubleOrNullSequence::SetDouble(double value) {
  DCHECK(IsNull());
  double_ = value;
  type_ = SpecificType::kDouble;
}

DoubleOrDoubleOrNullSequence DoubleOrDoubleOrNullSequence::FromDouble(double value) {
  DoubleOrDoubleOrNullSequence container;
  container.SetDouble(value);
  return container;
}

const Vector<base::Optional<double>>& DoubleOrDoubleOrNullSequence::GetAsDoubleOrNullSequence() const {
  DCHECK(IsDoubleOrNullSequence());
  return double_or_null_sequence_;
}

void DoubleOrDoubleOrNullSequence::SetDoubleOrNullSequence(const Vector<base::Optional<double>>& value) {
  DCHECK(IsNull());
  double_or_null_sequence_ = value;
  type_ = SpecificType::kDoubleOrNullSequence;
}

DoubleOrDoubleOrNullSequence DoubleOrDoubleOrNullSequence::FromDoubleOrNullSequence(const Vector<base::Optional<double>>& value) {
  DoubleOrDoubleOrNullSequence container;
  container.SetDoubleOrNullSequence(value);
  return container;
}

DoubleOrDoubleOrNullSequence::DoubleOrDoubleOrNullSequence(const DoubleOrDoubleOrNullSequence&) = default;
DoubleOrDoubleOrNullSequence::~DoubleOrDoubleOrNullSequence() = default;
DoubleOrDoubleOrNullSequence& DoubleOrDoubleOrNullSequence::operator=(const DoubleOrDoubleOrNullSequence&) = default;

void DoubleOrDoubleOrNullSequence::Trace(blink::Visitor* visitor) {
}

void V8DoubleOrDoubleOrNullSequence::ToImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, DoubleOrDoubleOrNullSequence& impl, UnionTypeConversionMode conversionMode, ExceptionState& exceptionState) {
  if (v8Value.IsEmpty())
    return;

  if (conversionMode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8Value))
    return;

  if (HasCallableIteratorSymbol(isolate, v8Value, exceptionState)) {
    Vector<base::Optional<double>> cppValue = NativeValueTraits<IDLSequence<IDLNullable<IDLDouble>>>::NativeValue(isolate, v8Value, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.SetDoubleOrNullSequence(cppValue);
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

v8::Local<v8::Value> ToV8(const DoubleOrDoubleOrNullSequence& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case DoubleOrDoubleOrNullSequence::SpecificType::kNone:
      return v8::Null(isolate);
    case DoubleOrDoubleOrNullSequence::SpecificType::kDouble:
      return v8::Number::New(isolate, impl.GetAsDouble());
    case DoubleOrDoubleOrNullSequence::SpecificType::kDoubleOrNullSequence:
      return ToV8(impl.GetAsDoubleOrNullSequence(), creationContext, isolate);
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

DoubleOrDoubleOrNullSequence NativeValueTraits<DoubleOrDoubleOrNullSequence>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  DoubleOrDoubleOrNullSequence impl;
  V8DoubleOrDoubleOrNullSequence::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exceptionState);
  return impl;
}

}  // namespace blink
