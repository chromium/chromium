// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_WEB_REQUEST_NATIVES_H_
#define EXTENSIONS_RENDERER_API_WEB_REQUEST_NATIVES_H_

#include "base/compiler_specific.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "v8/include/v8-forward.h"

namespace extensions {
class ScriptContext;

// Custom bindings for the webRequest API.
class WebRequestNatives : public ObjectBackedNativeHandler {
 public:
  explicit WebRequestNatives(ScriptContext* context);

  WebRequestNatives(const WebRequestNatives&) = delete;
  WebRequestNatives& operator=(const WebRequestNatives&) = delete;

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

 private:
  void AllowAsyncResponsesForAllEvents(
      const v8::FunctionCallbackInfo<v8::Value>& args);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_WEB_REQUEST_NATIVES_H_
