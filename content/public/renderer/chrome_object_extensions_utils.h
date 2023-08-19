// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_CHROME_OBJECT_EXTENSIONS_UTILS_H_
#define CONTENT_PUBLIC_RENDERER_CHROME_OBJECT_EXTENSIONS_UTILS_H_

#include "content/common/content_export.h"
#include "v8/include/v8-local-handle.h"

#include <string>

namespace v8 {
class Context;
class Object;
class Isolate;
}  // namespace v8

namespace content {

// Get or create a "chrome" object in the global object.
CONTENT_EXPORT v8::Local<v8::Object> GetOrCreateChromeObject(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context);

CONTENT_EXPORT v8::Local<v8::Object> GetOrCreateObject(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    const std::string& object_name);

CONTENT_EXPORT v8::Local<v8::Object> GetOrCreateObject(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> parent,
    const std::string& object_name);

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_CHROME_OBJECT_EXTENSIONS_UTILS_H_
