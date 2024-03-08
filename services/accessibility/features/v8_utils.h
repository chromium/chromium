// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_FEATURES_V8_UTILS_H_
#define SERVICES_ACCESSIBILITY_FEATURES_V8_UTILS_H_

#include "base/values.h"
#include "v8-context.h"
#include "v8-local-handle.h"
#include "v8-object.h"
#include "v8-value.h"

namespace ax {

// This class converts from base::Value types to v8::Values so they can be
// dispatched to JS. Usage: V8ValueConverter::GetInstance()->ConvertToV8Value().
class V8ValueConverter {
 public:
  ~V8ValueConverter() = default;

  // Returns the singleton converter.
  static V8ValueConverter* GetInstance();

  v8::Local<v8::Value> ConvertToV8Value(base::ValueView value,
                                        v8::Local<v8::Context> context) const;

 protected:
  // Singleton, constructor protected.
  V8ValueConverter() = default;

 private:
  v8::Local<v8::Value> ToV8Value(v8::Isolate* isolate,
                                 v8::Local<v8::Object> creation_context,
                                 base::ValueView value) const;
  v8::Local<v8::Value> ToArrayBuffer(
      v8::Isolate* isolate,
      v8::Local<v8::Object> creation_context,
      const base::Value::BlobStorage& value) const;
  v8::Local<v8::Value> ToV8Object(v8::Isolate* isolate,
                                  v8::Local<v8::Object> creation_context,
                                  const base::Value::Dict& val) const;
  v8::Local<v8::Value> ToV8Array(v8::Isolate* isolate,
                                 v8::Local<v8::Object> creation_context,
                                 const base::Value::List& val) const;
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_FEATURES_V8_UTILS_H_
