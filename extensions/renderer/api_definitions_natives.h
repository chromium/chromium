// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_DEFINITIONS_NATIVES_H_
#define EXTENSIONS_RENDERER_API_DEFINITIONS_NATIVES_H_

#include "base/macros.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "v8/include/v8.h"

namespace extensions {
class Dispatcher;
class ScriptContext;

// Native functions for JS to get access to the schemas for extension APIs.
class ApiDefinitionsNatives : public ObjectBackedNativeHandler {
 public:
  ApiDefinitionsNatives(Dispatcher* dispatcher, ScriptContext* context);

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

 private:
  // Returns the list of all schemas that are available to the calling context.
  void GetExtensionAPIDefinitionsForTest(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Not owned.
  Dispatcher* dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(ApiDefinitionsNatives);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_DEFINITIONS_NATIVES_H_
