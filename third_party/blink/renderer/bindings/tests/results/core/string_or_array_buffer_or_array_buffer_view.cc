// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/string_or_array_buffer_or_array_buffer_view.h"

#include "base/stl_util.h"
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
  array_buffer_view_ = Member<TestArrayBufferView>(value.View());
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

void StringOrArrayBufferOrArrayBufferView::Trace(blink::Visitor* visitor) {
  visitor->Trace(array_buffer_);
  visitor->Trace(array_buffer_view_);
}

void V8StringOrArrayBufferOrArrayBufferView::ToImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, StringOrArrayBufferOrArrayBufferView& impl, UnionTypeConversionMode conversionMode, ExceptionState& exceptionState) {
  if (v8Value.IsEmpty())
    return;

  if (conversionMode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8Value))
    return;

  if (v8Value->IsArrayBuffer()) {
    TestArrayBuffer* cppValue = V8ArrayBuffer::ToImpl(v8::Local<v8::Object>::Cast(v8Value));
    impl.SetArrayBuffer(cppValue);
    return;
  }

  if (v8Value->IsArrayBufferView()) {
    NotShared<TestArrayBufferView> cppValue = ToNotShared<NotShared<TestArrayBufferView>>(isolate, v8Value, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.SetArrayBufferView(cppValue);
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

StringOrArrayBufferOrArrayBufferView NativeValueTraits<StringOrArrayBufferOrArrayBufferView>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  StringOrArrayBufferOrArrayBufferView impl;
  V8StringOrArrayBufferOrArrayBufferView::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exceptionState);
  return impl;
}

}  // namespace blink
