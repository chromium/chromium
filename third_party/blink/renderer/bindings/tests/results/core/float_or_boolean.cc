// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/float_or_boolean.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"

namespace blink {

FloatOrBoolean::FloatOrBoolean() : type_(SpecificType::kNone) {}

bool FloatOrBoolean::GetAsBoolean() const {
  DCHECK(IsBoolean());
  return boolean_;
}

void FloatOrBoolean::SetBoolean(bool value) {
  DCHECK(IsNull());
  boolean_ = value;
  type_ = SpecificType::kBoolean;
}

FloatOrBoolean FloatOrBoolean::FromBoolean(bool value) {
  FloatOrBoolean container;
  container.SetBoolean(value);
  return container;
}

float FloatOrBoolean::GetAsFloat() const {
  DCHECK(IsFloat());
  return float_;
}

void FloatOrBoolean::SetFloat(float value) {
  DCHECK(IsNull());
  float_ = value;
  type_ = SpecificType::kFloat;
}

FloatOrBoolean FloatOrBoolean::FromFloat(float value) {
  FloatOrBoolean container;
  container.SetFloat(value);
  return container;
}

FloatOrBoolean::FloatOrBoolean(const FloatOrBoolean&) = default;
FloatOrBoolean::~FloatOrBoolean() = default;
FloatOrBoolean& FloatOrBoolean::operator=(const FloatOrBoolean&) = default;

void FloatOrBoolean::Trace(blink::Visitor* visitor) {
}

void V8FloatOrBoolean::ToImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, FloatOrBoolean& impl, UnionTypeConversionMode conversionMode, ExceptionState& exceptionState) {
  if (v8Value.IsEmpty())
    return;

  if (conversionMode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8Value))
    return;

  if (v8Value->IsBoolean()) {
    impl.SetBoolean(v8Value.As<v8::Boolean>()->Value());
    return;
  }

  if (v8Value->IsNumber()) {
    float cppValue = NativeValueTraits<IDLFloat>::NativeValue(isolate, v8Value, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.SetFloat(cppValue);
    return;
  }

  {
    float cppValue = NativeValueTraits<IDLFloat>::NativeValue(isolate, v8Value, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.SetFloat(cppValue);
    return;
  }
}

v8::Local<v8::Value> ToV8(const FloatOrBoolean& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case FloatOrBoolean::SpecificType::kNone:
      return v8::Null(isolate);
    case FloatOrBoolean::SpecificType::kBoolean:
      return v8::Boolean::New(isolate, impl.GetAsBoolean());
    case FloatOrBoolean::SpecificType::kFloat:
      return v8::Number::New(isolate, impl.GetAsFloat());
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

FloatOrBoolean NativeValueTraits<FloatOrBoolean>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  FloatOrBoolean impl;
  V8FloatOrBoolean::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exceptionState);
  return impl;
}

}  // namespace blink
