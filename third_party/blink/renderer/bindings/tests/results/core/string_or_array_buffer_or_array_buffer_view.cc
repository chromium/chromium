// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/string_or_array_buffer_or_array_buffer_view.h"

#include "base/cxx17_backports.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer.h"

namespace blink {

StringOrArrayBufferOrArrayBufferView::StringOrArrayBufferOrArrayBufferView() : type_(SpecificType::kNone) {}

TestArrayBuffer* StringOrArrayBufferOrArrayBufferView::GetAsArrayBuffer() const {
  DCHECK(IsArrayBuffer());
  return array_buffer_;
}

void StringOrArrayBufferOrArrayBufferView::SetArrayBuffer(TestArrayBuffer* value) {
  DCHECK(IsNull());
  array_buffer_ = value;
  type_ = SpecificType::kArrayBuffer;
}

StringOrArrayBufferOrArrayBufferView StringOrArrayBufferOrArrayBufferView::FromArrayBuffer(TestArrayBuffer* value) {
  StringOrArrayBufferOrArrayBufferView container;
  container.SetArrayBuffer(value);
  return container;
}

NotShared<TestArrayBufferView> StringOrArrayBufferOrArrayBufferView::GetAsArrayBufferView() const {
  DCHECK(IsArrayBufferView());
  return array_buffer_view_;
}

void StringOrArrayBufferOrArrayBufferView::SetArrayBufferView(NotShared<TestArrayBufferView> value) {
  DCHECK(IsNull());
  array_buffer_view_ = value;
  type_ = SpecificType::kArrayBufferView;
}

StringOrArrayBufferOrArrayBufferView StringOrArrayBufferOrArrayBufferView::FromArrayBufferView(NotShared<TestArrayBufferView> value) {
  StringOrArrayBufferOrArrayBufferView container;
  container.SetArrayBufferView(value);
  return container;
}

const String& StringOrArrayBufferOrArrayBufferView::GetAsString() const {
  DCHECK(IsString());
  return string_;
}

void StringOrArrayBufferOrArrayBufferView::SetString(const String& value) {
  DCHECK(IsNull());
  string_ = value;
  type_ = SpecificType::kString;
}

StringOrArrayBufferOrArrayBufferView StringOrArrayBufferOrArrayBufferView::FromString(const String& value) {
  StringOrArrayBufferOrArrayBufferView container;
  container.SetString(value);
  return container;
}

StringOrArrayBufferOrArrayBufferView::StringOrArrayBufferOrArrayBufferView(const StringOrArrayBufferOrArrayBufferView&) = default;
StringOrArrayBufferOrArrayBufferView::~StringOrArrayBufferOrArrayBufferView() = default;
StringOrArrayBufferOrArrayBufferView& StringOrArrayBufferOrArrayBufferView::operator=(const StringOrArrayBufferOrArrayBufferView&) = default;

void StringOrArrayBufferOrArrayBufferView::Trace(Visitor* visitor) const {
  visitor->Trace(array_buffer_);
  visitor->Trace(array_buffer_view_);
}

void V8StringOrArrayBufferOrArrayBufferView::ToImpl(
    v8::Isolate* isolate,
    v8::Local<v8::Value> v8_value,
    StringOrArrayBufferOrArrayBufferView& impl,
    UnionTypeConversionMode conversion_mode,
    ExceptionState& exception_state) {
  if (v8_value.IsEmpty())
    return;

  if (conversion_mode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8_value))
    return;

  if (v8_value->IsArrayBuffer()) {
    TestArrayBuffer* cpp_value = V8ArrayBuffer::ToImpl(v8::Local<v8::Object>::Cast(v8_value));
    impl.SetArrayBuffer(cpp_value);
    return;
  }

  if (v8_value->IsArrayBufferView()) {
    NotShared<TestArrayBufferView> cpp_value = ToNotShared<NotShared<TestArrayBufferView>>(isolate, v8_value, exception_state);
    if (exception_state.HadException())
      return;
    impl.SetArrayBufferView(cpp_value);
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

v8::Local<v8::Value> ToV8(const StringOrArrayBufferOrArrayBufferView& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case StringOrArrayBufferOrArrayBufferView::SpecificType::kNone:
      return v8::Null(isolate);
    case StringOrArrayBufferOrArrayBufferView::SpecificType::kArrayBuffer:
      return ToV8(impl.GetAsArrayBuffer(), creationContext, isolate);
    case StringOrArrayBufferOrArrayBufferView::SpecificType::kArrayBufferView:
      return ToV8(impl.GetAsArrayBufferView(), creationContext, isolate);
    case StringOrArrayBufferOrArrayBufferView::SpecificType::kString:
      return V8String(isolate, impl.GetAsString());
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

StringOrArrayBufferOrArrayBufferView NativeValueTraits<StringOrArrayBufferOrArrayBufferView>::NativeValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  StringOrArrayBufferOrArrayBufferView impl;
  V8StringOrArrayBufferOrArrayBufferView::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exception_state);
  return impl;
}

}  // namespace blink

