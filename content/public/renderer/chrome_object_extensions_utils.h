// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_CHROME_OBJECT_EXTENSIONS_UTILS_H_
#define CONTENT_PUBLIC_RENDERER_CHROME_OBJECT_EXTENSIONS_UTILS_H_

#include "content/common/content_export.h"

namespace v8 {
template<class T> class Local;
class Context;
class Object;
class Isolate;
}  // namespace v8

namespace content {

CONTENT_EXPORT v8::Local<v8::Object> GetOrCreateChromeObject(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context);

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_CHROME_OBJECT_EXTENSIONS_UTILS_H_
