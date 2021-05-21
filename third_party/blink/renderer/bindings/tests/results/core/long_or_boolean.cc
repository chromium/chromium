// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/long_or_boolean.h"

#include "base/cxx17_backports.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"

namespace blink {

LongOrBoolean::LongOrBoolean() : type_(SpecificType::kNone) {}

bool LongOrBoolean::GetAsBoolean() const {
  DCHECK(IsBoolean());
  return boolean_;
}

void LongOrBoolean::SetBoolean(bool value) {
  DCHECK(IsNull());
  boolean_ = value;
  type_ = SpecificType::kBoolean;
}

LongOrBoolean LongOrBoolean::FromBoolean(bool value) {
  LongOrBoolean container;
  container.SetBoolean(value);
  return container;
}

int32_t LongOrBoolean::GetAsLong() const {
  DCHECK(IsLong());
  return long_;
}

void LongOrBoolean::SetLong(int32_t value) {
  DCHECK(IsNull());
  long_ = value;
  type_ = SpecificType::kLong;
}

LongOrBoolean LongOrBoolean::FromLong(int32_t value) {
  LongOrBoolean container;
  container.SetLong(value);
  return container;
}

LongOrBoolean::LongOrBoolean(const LongOrBoolean&) = default;
LongOrBoolean::~LongOrBoolean() = default;
LongOrBoolean& LongOrBoolean::operator=(const LongOrBoolean&) = default;

void LongOrBoolean::Trace(Visitor* visitor) const {
}

void V8LongOrBoolean::ToImpl(
    v8::Isolate* isolate,
    v8::Local<v8::Value> v8_value,
    LongOrBoolean& impl,
    UnionTypeConversionMode conversion_mode,
    ExceptionState& exception_state) {
  if (v8_value.IsEmpty())
    return;

  if (conversion_mode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8_value))
    return;

  if (v8_value->IsBoolean()) {
    impl.SetBoolean(v8_value.As<v8::Boolean>()->Value());
    return;
  }

  if (v8_value->IsNumber()) {
    int32_t cpp_value{ NativeValueTraits<IDLLong>::NativeValue(isolate, v8_value, exception_state) };
    if (exception_state.HadException())
      return;
    impl.SetLong(cpp_value);
    return;
  }

  {
    int32_t cpp_value{ NativeValueTraits<IDLLong>::NativeValue(isolate, v8_value, exception_state) };
    if (exception_state.HadException())
      return;
    impl.SetLong(cpp_value);
    return;
  }
}

v8::Local<v8::Value> ToV8(const LongOrBoolean& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case LongOrBoolean::SpecificType::kNone:
      return v8::Null(isolate);
    case LongOrBoolean::SpecificType::kBoolean:
      return v8::Boolean::New(isolate, impl.GetAsBoolean());
    case LongOrBoolean::SpecificType::kLong:
      return v8::Integer::New(isolate, impl.GetAsLong());
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

LongOrBoolean NativeValueTraits<LongOrBoolean>::NativeValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  LongOrBoolean impl;
  V8LongOrBoolean::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exception_state);
  return impl;
}

}  // namespace blink

