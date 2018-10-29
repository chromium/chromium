// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/long_or_boolean.h"

#include "base/stl_util.h"
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

void LongOrBoolean::Trace(blink::Visitor* visitor) {
}

void V8LongOrBoolean::ToImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, LongOrBoolean& impl, UnionTypeConversionMode conversionMode, ExceptionState& exceptionState) {
  if (v8Value.IsEmpty())
    return;

  if (conversionMode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8Value))
    return;

  if (v8Value->IsBoolean()) {
    impl.SetBoolean(v8Value.As<v8::Boolean>()->Value());
    return;
  }

  if (v8Value->IsNumber()) {
    int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(isolate, v8Value, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.SetLong(cppValue);
    return;
  }

  {
    int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(isolate, v8Value, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.SetLong(cppValue);
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

LongOrBoolean NativeValueTraits<LongOrBoolean>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  LongOrBoolean impl;
  V8LongOrBoolean::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exceptionState);
  return impl;
}

}  // namespace blink
