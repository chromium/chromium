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
  v8::Local<v8::Object> global = context->Global();
  v8::Local<v8::Object> chrome;
  v8::Local<v8::Value> chrome_value;
  if (!global->Get(context, gin::StringToV8(isolate, "chrome"))
           .ToLocal(&chrome_value) ||
      !chrome_value->IsObject()) {
    chrome = v8::Object::New(isolate);
    global->Set(context, gin::StringToSymbol(isolate, "chrome"), chrome)
        .Check();
  } else {
    chrome = v8::Local<v8::Object>::Cast(chrome_value);
  }
  return chrome;
}

}  // namespace content
