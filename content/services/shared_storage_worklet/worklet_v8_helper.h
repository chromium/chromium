// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_SHARED_STORAGE_WORKLET_WORKLET_V8_HELPER_H_
#define CONTENT_SERVICES_SHARED_STORAGE_WORKLET_WORKLET_V8_HELPER_H_

#include <string>

#include "base/containers/span.h"
#include "content/common/content_export.h"
#include "url/gurl.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-locker.h"

namespace shared_storage_worklet {

class CONTENT_EXPORT WorkletV8Helper {
 public:
  class CONTENT_EXPORT HandleScope {
   public:
    explicit HandleScope(v8::Isolate* isolate);
    explicit HandleScope(const HandleScope&) = delete;
    HandleScope& operator=(const HandleScope&) = delete;
    ~HandleScope();

   private:
    const v8::Isolate::Scope isolate_scope_;
    const v8::HandleScope handle_scope_;
  };

  static v8::MaybeLocal<v8::Value> InvokeFunction(
      v8::Local<v8::Context> context,
      v8::Local<v8::Function> function,
      base::span<v8::Local<v8::Value>> args,
      std::string* error_message);

  static v8::MaybeLocal<v8::Value> CompileAndRunScript(
      v8::Local<v8::Context> context,
      const std::string& src,
      const GURL& src_url,
      std::string* error_message);
};

}  // namespace shared_storage_worklet

#endif  // CONTENT_SERVICES_SHARED_STORAGE_WORKLET_WORKLET_V8_HELPER_H_
