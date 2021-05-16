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
#include "third_party/blink/renderer/bindings/core/v8/script_iterator.h"
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

const Vector<absl::optional<double>>& DoubleOrDoubleOrNullSequence::GetAsDoubleOrNullSequence() const {
  DCHECK(IsDoubleOrNullSequence());
  return double_or_null_sequence_;
}

void DoubleOrDoubleOrNullSequence::SetDoubleOrNullSequence(const Vector<absl::optional<double>>& value) {
  DCHECK(IsNull());
  double_or_null_sequence_ = value;
  type_ = SpecificType::kDoubleOrNullSequence;
}

DoubleOrDoubleOrNullSequence DoubleOrDoubleOrNullSequence::FromDoubleOrNullSequence(const Vector<absl::optional<double>>& value) {
  DoubleOrDoubleOrNullSequence container;
  container.SetDoubleOrNullSequence(value);
  return container;
}

DoubleOrDoubleOrNullSequence::DoubleOrDoubleOrNullSequence(const DoubleOrDoubleOrNullSequence&) = default;
DoubleOrDoubleOrNullSequence::~DoubleOrDoubleOrNullSequence() = default;
DoubleOrDoubleOrNullSequence& DoubleOrDoubleOrNullSequence::operator=(const DoubleOrDoubleOrNullSequence&) = default;

void DoubleOrDoubleOrNullSequence::Trace(Visitor* visitor) const {
}

void V8DoubleOrDoubleOrNullSequence::ToImpl(
    v8::Isolate* isolate,
    v8::Local<v8::Value> v8_value,
    DoubleOrDoubleOrNullSequence& impl,
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
      Vector<absl::optional<double>> cpp_value{ NativeValueTraits<IDLSequence<IDLNullable<IDLDouble>>>::NativeValue(isolate, std::move(script_iterator), exception_state) };
      if (exception_state.HadException())
        return;
      impl.SetDoubleOrNullSequence(cpp_value);
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

DoubleOrDoubleOrNullSequence NativeValueTraits<DoubleOrDoubleOrNullSequence>::NativeValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  DoubleOrDoubleOrNullSequence impl;
  V8DoubleOrDoubleOrNullSequence::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exception_state);
  return impl;
}

}  // namespace blink

