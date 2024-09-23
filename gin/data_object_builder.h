// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_DATA_OBJECT_BUILDER_H_
#define GIN_DATA_OBJECT_BUILDER_H_

#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "gin/converter.h"
#include "gin/gin_export.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-object.h"

namespace gin {

// Constructs a JavaScript object with a series of data properties.
// (As with default data properties in JavaScript, these properties are
// configurable, writable and enumerable.)
//
// Values are automatically converted using gin::Converter, though if
// using a type where the conversion may fail, callers must convert ahead of
// time.
//
// This class avoids the pitfall of using v8::Object::Set, which may invoke
// setters on the object prototype.
//
// Expected usage:
// v8::Local<v8::Object> object = gin::DataObjectBuilder(isolate)
//     .Set("boolean", true)
//     .Set("integer", 42)
//     .Build();
//
// Because this builder class contains local handles, callers must ensure it
// does not outlive the scope in which it is created.
class GIN_EXPORT DataObjectBuilder {
 public:
  explicit DataObjectBuilder(v8::Isolate* isolate);
  DataObjectBuilder(const DataObjectBuilder&) = delete;
  DataObjectBuilder& operator=(const DataObjectBuilder&) = delete;

  ~DataObjectBuilder();

  template <typename T>
  DataObjectBuilder& Set(std::string_view key, T&& value) {
    DCHECK(!object_.IsEmpty());
    v8::Local<v8::String> v8_key = StringToSymbol(isolate_, key);
    v8::Local<v8::Value> v8_value =
        ConvertToV8(isolate_, std::forward<T>(value));
    CHECK(object_->CreateDataProperty(context_, v8_key, v8_value).ToChecked());
    return *this;
  }

  template <typename T>
  DataObjectBuilder& Set(uint32_t index, T&& value) {
    DCHECK(!object_.IsEmpty());
    v8::Local<v8::Value> v8_value =
        ConvertToV8(isolate_, std::forward<T>(value));
    CHECK(object_->CreateDataProperty(context_, index, v8_value).ToChecked());
    return *this;
  }

  v8::Local<v8::Object> Build() {
    DCHECK(!object_.IsEmpty());
    v8::Local<v8::Object> result = object_;
    object_.Clear();
    return result;
  }

 private:
  raw_ptr<v8::Isolate> isolate_;
  v8::Local<v8::Context> context_;
  v8::Local<v8::Object> object_;
};

}  // namespace gin

#endif  // GIN_DATA_OBJECT_BUILDER_H_
