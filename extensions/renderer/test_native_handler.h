// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_TEST_NATIVE_HANDLER_H_
#define EXTENSIONS_RENDERER_TEST_NATIVE_HANDLER_H_

#include "base/compiler_specific.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-function-callback.h"

namespace extensions {
class ScriptContext;

// NativeHandler for the chrome.test API.
class TestNativeHandler : public ObjectBackedNativeHandler {
 public:
  explicit TestNativeHandler(ScriptContext* context);

  TestNativeHandler(const TestNativeHandler&) = delete;
  TestNativeHandler& operator=(const TestNativeHandler&) = delete;

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

 private:
  void GetWakeEventPage(const v8::FunctionCallbackInfo<v8::Value>& args);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_TEST_NATIVE_HANDLER_H_
