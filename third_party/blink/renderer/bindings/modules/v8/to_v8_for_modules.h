// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_MODULES_V8_TO_V8_FOR_MODULES_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_MODULES_V8_TO_V8_FOR_MODULES_H_

#include "base/notreached.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webdatabase/sqlite/sql_value.h"

namespace blink {

class IDBAny;
class IDBKey;
class IDBKeyPath;

inline v8::Local<v8::Value> ToV8(const SQLValue& sql_value,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  switch (sql_value.GetType()) {
    case SQLValue::kNullValue:
      return v8::Null(isolate);
    case SQLValue::kNumberValue:
      return v8::Number::New(isolate, sql_value.Number());
    case SQLValue::kStringValue:
      return V8String(isolate, sql_value.GetString());
  }
  NOTREACHED();
  return v8::Local<v8::Value>();
}

v8::Local<v8::Value> ToV8(const IDBKeyPath&,
                          v8::Local<v8::Object> creation_context,
                          v8::Isolate*);
MODULES_EXPORT v8::Local<v8::Value> ToV8(const IDBKey*,
                                         v8::Local<v8::Object> creation_context,
                                         v8::Isolate*);
v8::Local<v8::Value> ToV8(const IDBAny*,
                          v8::Local<v8::Object> creation_context,
                          v8::Isolate*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_MODULES_V8_TO_V8_FOR_MODULES_H_
