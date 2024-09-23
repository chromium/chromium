// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/v8_utils.h"

#include <string_view>

#include "v8-array-buffer.h"
#include "v8-container.h"

namespace ax {

// static
V8ValueConverter* V8ValueConverter::GetInstance() {
  static V8ValueConverter converter;
  return &converter;
}

v8::Local<v8::Value> V8ValueConverter::ConvertToV8Value(
    base::ValueView value,
    v8::Local<v8::Context> context) const {
  v8::Context::Scope context_scope(context);
  v8::EscapableHandleScope handle_scope(context->GetIsolate());
  return handle_scope.Escape(
      ToV8Value(context->GetIsolate(), context->Global(), value));
}

v8::Local<v8::Value> V8ValueConverter::ToV8Value(
    v8::Isolate* isolate,
    v8::Local<v8::Object> creation_context,
    base::ValueView value) const {
  struct Visitor {
    raw_ptr<const V8ValueConverter> converter;
    raw_ptr<v8::Isolate> isolate;
    v8::Local<v8::Object> creation_context;

    v8::Local<v8::Value> operator()(absl::monostate value) {
      return v8::Null(isolate);
    }

    v8::Local<v8::Value> operator()(bool value) {
      return v8::Boolean::New(isolate, value);
    }

    v8::Local<v8::Value> operator()(int value) {
      return v8::Integer::New(isolate, value);
    }

    v8::Local<v8::Value> operator()(double value) {
      return v8::Number::New(isolate, value);
    }

    v8::Local<v8::Value> operator()(std::string_view value) {
      return v8::String::NewFromUtf8(isolate, value.data(),
                                     v8::NewStringType::kNormal, value.length())
          .ToLocalChecked();
    }

    v8::Local<v8::Value> operator()(const base::Value::BlobStorage& value) {
      return converter->ToArrayBuffer(isolate, creation_context, value);
    }

    v8::Local<v8::Value> operator()(const base::Value::Dict& value) {
      return converter->ToV8Object(isolate, creation_context, value);
    }

    v8::Local<v8::Value> operator()(const base::Value::List& value) {
      return converter->ToV8Array(isolate, creation_context, value);
    }
  };

  return value.Visit(Visitor{.converter = this,
                             .isolate = isolate,
                             .creation_context = creation_context});
}

v8::Local<v8::Value> V8ValueConverter::ToArrayBuffer(
    v8::Isolate* isolate,
    v8::Local<v8::Object> creation_context,
    const base::Value::BlobStorage& value) const {
  v8::Local<v8::ArrayBuffer> buffer =
      v8::ArrayBuffer::New(isolate, value.size());
  base::ranges::copy(value,
                     static_cast<uint8_t*>(buffer->GetBackingStore()->Data()));
  return buffer;
}

v8::Local<v8::Value> V8ValueConverter::ToV8Object(
    v8::Isolate* isolate,
    v8::Local<v8::Object> creation_context,
    const base::Value::Dict& val) const {
  v8::Local<v8::Object> result(v8::Object::New(isolate));

  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  for (const auto [key, value] : val) {
    v8::Local<v8::Value> child_v8 = ToV8Value(isolate, creation_context, value);
    CHECK(!child_v8.IsEmpty());

    v8::Maybe<bool> maybe = result->CreateDataProperty(
        context,
        v8::String::NewFromUtf8(isolate, key.c_str(),
                                v8::NewStringType::kNormal, key.length())
            .ToLocalChecked(),
        child_v8);
    CHECK(maybe.IsJust());
  }

  return result;
}

v8::Local<v8::Value> V8ValueConverter::ToV8Array(
    v8::Isolate* isolate,
    v8::Local<v8::Object> creation_context,
    const base::Value::List& val) const {
  v8::Local<v8::Array> result(v8::Array::New(isolate, val.size()));
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  for (size_t i = 0; i < val.size(); ++i) {
    const base::Value& child = val[i];
    v8::Local<v8::Value> child_v8 = ToV8Value(isolate, creation_context, child);
    CHECK(!child_v8.IsEmpty());

    v8::Maybe<bool> maybe =
        result->CreateDataProperty(context, static_cast<uint32_t>(i), child_v8);
    CHECK(maybe.IsJust());
  }

  return result;
}

}  // namespace ax
