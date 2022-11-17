// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_UTILS_NATIVE_HANDLER_H_
#define EXTENSIONS_RENDERER_UTILS_NATIVE_HANDLER_H_

#include "extensions/renderer/object_backed_native_handler.h"
#include "v8/include/v8-forward.h"

namespace extensions {
class ScriptContext;

class UtilsNativeHandler : public ObjectBackedNativeHandler {
 public:
  explicit UtilsNativeHandler(ScriptContext* context);

  UtilsNativeHandler(const UtilsNativeHandler&) = delete;
  UtilsNativeHandler& operator=(const UtilsNativeHandler&) = delete;

  ~UtilsNativeHandler() override;

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

 private:
  // |args| consists of one argument: an arbitrary value. Returns a deep copy of
  // that value. The copy will have no references to nested values of the
  // argument.
  void DeepCopy(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Returns true if the ScriptContext is for a service worker.
  void IsInServiceWorker(const v8::FunctionCallbackInfo<v8::Value>& args);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_UTILS_NATIVE_HANDLER_H_
