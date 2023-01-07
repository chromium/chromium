// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_RUNTIME_CUSTOM_BINDINGS_H_
#define EXTENSIONS_RENDERER_RUNTIME_CUSTOM_BINDINGS_H_

#include "base/compiler_specific.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "v8/include/v8-forward.h"

namespace extensions {

// The native component of custom bindings for the chrome.runtime API.
class RuntimeCustomBindings : public ObjectBackedNativeHandler {
 public:
  explicit RuntimeCustomBindings(ScriptContext* context);
  ~RuntimeCustomBindings() override;

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

 private:
  void GetExtensionViews(const v8::FunctionCallbackInfo<v8::Value>& args);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_RUNTIME_CUSTOM_BINDINGS_H_
