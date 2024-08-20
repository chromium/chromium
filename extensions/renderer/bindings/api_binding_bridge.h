// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_API_BINDING_BRIDGE_H_
#define EXTENSIONS_RENDERER_BINDINGS_API_BINDING_BRIDGE_H_

#include <string>

#include "extensions/common/extension_id.h"
#include "gin/wrappable.h"
#include "v8/include/v8.h"

namespace extensions {
class APIBindingHooks;

// An object that serves as a bridge between the current JS-centric bindings and
// the new native bindings system. This basically needs to conform to the public
// methods of the Binding prototype in binding.js.
class APIBindingBridge final : public gin::Wrappable<APIBindingBridge> {
 public:
  APIBindingBridge(APIBindingHooks* hooks,
                   v8::Local<v8::Context> context,
                   v8::Local<v8::Value> api_object,
                   const ExtensionId& extension_id,
                   const std::string& context_type);

  APIBindingBridge(const APIBindingBridge&) = delete;
  APIBindingBridge& operator=(const APIBindingBridge&) = delete;

  ~APIBindingBridge() override;

  static gin::WrapperInfo kWrapperInfo;

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final;

 private:
  // Runs the given function and registers custom hooks.
  // The function takes three arguments: an object,
  // {
  //   apiFunctions: <JSHookInterface> (see api_bindings_hooks.cc),
  //   compiledApi: <the API object>
  // }
  // as well as a string for the extension ID and a string for the context type.
  // This should register any hooks that the JS needs for the given API.
  void RegisterCustomHook(v8::Isolate* isolate,
                          v8::Local<v8::Function> function);

  // The id of the extension that owns the context this belongs to.
  ExtensionId extension_id_;

  // The type of context this belongs to.
  std::string context_type_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_API_BINDING_BRIDGE_H_
