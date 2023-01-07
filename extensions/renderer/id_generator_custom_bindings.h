// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_ID_GENERATOR_CUSTOM_BINDINGS_H_
#define EXTENSIONS_RENDERER_ID_GENERATOR_CUSTOM_BINDINGS_H_

#include "extensions/renderer/object_backed_native_handler.h"
#include "v8/include/v8-forward.h"

namespace extensions {
class ScriptContext;

// Implements function that can be used by JS layer to generate unique integer
// identifiers.
class IdGeneratorCustomBindings : public ObjectBackedNativeHandler {
 public:
  IdGeneratorCustomBindings(ScriptContext* context);

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

 private:
  // Generate a unique ID global to the renderer.
  void GetNextId(const v8::FunctionCallbackInfo<v8::Value>& args);
  // Generate a unique ID scoped to the ScriptContext.
  void GetNextScopedId(const v8::FunctionCallbackInfo<v8::Value>& args);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_ID_GENERATOR_CUSTOM_BINDINGS_H_
