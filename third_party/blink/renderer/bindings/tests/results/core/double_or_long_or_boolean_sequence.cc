// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/double_or_long_or_boolean_sequence.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/long_or_boolean.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"

namespace blink {

DoubleOrLongOrBooleanSequence::DoubleOrLongOrBooleanSequence() : type_(SpecificType::kNone) {}

double DoubleOrLongOrBooleanSequence::GetAsDouble() const {
  DCHECK(IsDouble());
  return double_;
}

void DoubleOrLongOrBooleanSequence::SetDouble(double value) {
  DCHECK(IsNull());
  double_ = value;
  type_ = SpecificType::kDouble;
}

DoubleOrLongOrBooleanSequence DoubleOrLongOrBooleanSequence::FromDouble(double value) {
  DoubleOrLongOrBooleanSequence container;
  container.SetDouble(value);
  return container;
}

const HeapVector<LongOrBoolean>& DoubleOrLongOrBooleanSequence::GetAsLongOrBooleanSequence() const {
  DCHECK(IsLongOrBooleanSequence());
  return long_or_boolean_sequence_;
}

void DoubleOrLongOrBooleanSequence::SetLongOrBooleanSequence(const HeapVector<LongOrBoolean>& value) {
  DCHECK(IsNull());
  long_or_boolean_sequence_ = value;
  type_ = SpecificType::kLongOrBooleanSequence;
}

DoubleOrLongOrBooleanSequence DoubleOrLongOrBooleanSequence::FromLongOrBooleanSequence(const HeapVector<LongOrBoolean>& value) {
  DoubleOrLongOrBooleanSequence container;
  container.SetLongOrBooleanSequence(value);
  return container;
}

DoubleOrLongOrBooleanSequence::DoubleOrLongOrBooleanSequence(const DoubleOrLongOrBooleanSequence&) = default;
DoubleOrLongOrBooleanSequence::~DoubleOrLongOrBooleanSequence() = default;
DoubleOrLongOrBooleanSequence& DoubleOrLongOrBooleanSequence::operator=(const DoubleOrLongOrBooleanSequence&) = default;

void DoubleOrLongOrBooleanSequence::Trace(blink::Visitor* visitor) {
  visitor->Trace(long_or_boolean_sequence_);
}

void V8DoubleOrLongOrBooleanSequence::ToImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, DoubleOrLongOrBooleanSequence& impl, UnionTypeConversionMode conversionMode, ExceptionState& exceptionState) {
  if (v8Value.IsEmpty())
    return;

  if (conversionMode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8Value))
    return;

  if (HasCallableIteratorSymbol(isolate, v8Value, exceptionState)) {
    HeapVector<LongOrBoolean> cppValue = NativeValueTraits<IDLSequence<LongOrBoolean>>::NativeValue(isolate, v8Value, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.SetLongOrBooleanSequence(cppValue);
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

v8::Local<v8::Value> ToV8(const DoubleOrLongOrBooleanSequence& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case DoubleOrLongOrBooleanSequence::SpecificType::kNone:
      return v8::Null(isolate);
    case DoubleOrLongOrBooleanSequence::SpecificType::kDouble:
      return v8::Number::New(isolate, impl.GetAsDouble());
    case DoubleOrLongOrBooleanSequence::SpecificType::kLongOrBooleanSequence:
      return ToV8(impl.GetAsLongOrBooleanSequence(), creationContext, isolate);
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

DoubleOrLongOrBooleanSequence NativeValueTraits<DoubleOrLongOrBooleanSequence>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  DoubleOrLongOrBooleanSequence impl;
  V8DoubleOrLongOrBooleanSequence::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exceptionState);
  return impl;
}

}  // namespace blink
