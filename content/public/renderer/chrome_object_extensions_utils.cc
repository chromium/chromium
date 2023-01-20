// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/chrome_object_extensions_utils.h"

#include "gin/converter.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-object.h"

namespace content {

v8::Local<v8::Object> GetOrCreateChromeObject(v8::Isolate* isolate,
                                              v8::Local<v8::Context> context) {
  return GetOrCreateObject(isolate, context, "chrome");
}

v8::Local<v8::Object> GetOrCreateObject(v8::Isolate* isolate,
                                        v8::Local<v8::Context> context,
                                        const std::string& object_name) {
  v8::Local<v8::Object> global = context->Global();
  return GetOrCreateObject(isolate, context, global, object_name);
}

v8::Local<v8::Object> GetOrCreateObject(v8::Isolate* isolate,
                                        v8::Local<v8::Context> context,
                                        v8::Local<v8::Object> parent,
                                        const std::string& object_name) {
  v8::Local<v8::Object> new_object;
  v8::Local<v8::Value> object_value;
  if (!parent->Get(context, gin::StringToV8(isolate, object_name))
           .ToLocal(&object_value) ||
      !object_value->IsObject()) {
    new_object = v8::Object::New(isolate);
    parent->Set(context, gin::StringToSymbol(isolate, object_name), new_object)
        .Check();
  } else {
    new_object = v8::Local<v8::Object>::Cast(object_value);
  }
  return new_object;
}

}  // namespace content
