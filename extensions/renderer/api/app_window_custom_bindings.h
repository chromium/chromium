// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_APP_WINDOW_CUSTOM_BINDINGS_H_
#define EXTENSIONS_RENDERER_API_APP_WINDOW_CUSTOM_BINDINGS_H_

#include "extensions/renderer/object_backed_native_handler.h"
#include "v8/include/v8-forward.h"

namespace extensions {

// Implements custom bindings for the app.window API.
class AppWindowCustomBindings : public ObjectBackedNativeHandler {
 public:
  AppWindowCustomBindings(ScriptContext* context);

  AppWindowCustomBindings(const AppWindowCustomBindings&) = delete;
  AppWindowCustomBindings& operator=(const AppWindowCustomBindings&) = delete;

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

 private:
  void GetFrame(const v8::FunctionCallbackInfo<v8::Value>& args);
  void ResumeParser(const v8::FunctionCallbackInfo<v8::Value>& args);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_APP_WINDOW_CUSTOM_BINDINGS_H_
