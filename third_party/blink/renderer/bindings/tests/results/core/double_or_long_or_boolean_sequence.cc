// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/double_or_long_or_boolean_sequence.h"

#include "base/cxx17_backports.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/long_or_boolean.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_iterator.h"
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

void DoubleOrLongOrBooleanSequence::Trace(Visitor* visitor) const {
  visitor->Trace(long_or_boolean_sequence_);
}

void V8DoubleOrLongOrBooleanSequence::ToImpl(
    v8::Isolate* isolate,
    v8::Local<v8::Value> v8_value,
    DoubleOrLongOrBooleanSequence& impl,
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
      HeapVector<LongOrBoolean> cpp_value{ NativeValueTraits<IDLSequence<LongOrBoolean>>::NativeValue(isolate, std::move(script_iterator), exception_state) };
      if (exception_state.HadException())
        return;
      impl.SetLongOrBooleanSequence(cpp_value);
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
    double cpp_value{ NativeValueTraits<IDLDouble>::NativeValue(isolate, v8_value, exception_state) };
    if (exception_state.HadException())
      return;
    impl.SetDouble(cpp_value);
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

DoubleOrLongOrBooleanSequence NativeValueTraits<DoubleOrLongOrBooleanSequence>::NativeValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  DoubleOrLongOrBooleanSequence impl;
  V8DoubleOrLongOrBooleanSequence::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exception_state);
  return impl;
}

}  // namespace blink

