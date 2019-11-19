// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/activity_log_converter_strategy.h"

#include <memory>

#include "base/logging.h"
#include "base/values.h"
#include "v8/include/v8.h"

namespace extensions {

namespace {

// Summarize a V8 value. This performs a shallow conversion in all cases, and
// returns only a string with a description of the value (e.g.,
// "[HTMLElement]").
std::unique_ptr<base::Value> SummarizeV8Value(v8::Isolate* isolate,
                                              v8::Local<v8::Object> object) {
  v8::TryCatch try_catch(isolate);
  v8::Isolate::DisallowJavascriptExecutionScope scope(
      isolate, v8::Isolate::DisallowJavascriptExecutionScope::THROW_ON_FAILURE);
  v8::Local<v8::String> name =
      v8::String::NewFromUtf8(isolate, "[", v8::NewStringType::kNormal)
          .ToLocalChecked();
  if (object->IsFunction()) {
    name = v8::String::Concat(
        isolate, name,
        v8::String::NewFromUtf8(isolate, "Function", v8::NewStringType::kNormal)
            .ToLocalChecked());
    v8::Local<v8::Value> fname =
        v8::Local<v8::Function>::Cast(object)->GetName();
    if (fname->IsString() && v8::Local<v8::String>::Cast(fname)->Length()) {
      name = v8::String::Concat(
          isolate, name,
          v8::String::NewFromUtf8(isolate, " ", v8::NewStringType::kNormal)
              .ToLocalChecked());
      name =
          v8::String::Concat(isolate, name, v8::Local<v8::String>::Cast(fname));
      name = v8::String::Concat(
          isolate, name,
          v8::String::NewFromUtf8(isolate, "()", v8::NewStringType::kNormal)
              .ToLocalChecked());
    }
  } else {
    name = v8::String::Concat(isolate, name, object->GetConstructorName());
  }
  name = v8::String::Concat(
      isolate, name,
      v8::String::NewFromUtf8(isolate, "]", v8::NewStringType::kNormal)
          .ToLocalChecked());

  if (try_catch.HasCaught()) {
    return std::unique_ptr<base::Value>(
        new base::Value("[JS Execution Exception]"));
  }

  return std::unique_ptr<base::Value>(
      new base::Value(std::string(*v8::String::Utf8Value(isolate, name))));
}

}  // namespace

ActivityLogConverterStrategy::ActivityLogConverterStrategy() {}

ActivityLogConverterStrategy::~ActivityLogConverterStrategy() {}

bool ActivityLogConverterStrategy::FromV8Object(
    v8::Local<v8::Object> value,
    std::unique_ptr<base::Value>* out,
    v8::Isolate* isolate) {
  return FromV8Internal(value, out, isolate);
}

bool ActivityLogConverterStrategy::FromV8Array(
    v8::Local<v8::Array> value,
    std::unique_ptr<base::Value>* out,
    v8::Isolate* isolate) {
  return FromV8Internal(value, out, isolate);
}

bool ActivityLogConverterStrategy::FromV8Internal(
    v8::Local<v8::Object> value,
    std::unique_ptr<base::Value>* out,
    v8::Isolate* isolate) const {
  *out = SummarizeV8Value(isolate, value);

  return true;
}

}  // namespace extensions
