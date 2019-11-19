// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_RUNTIME_HOOKS_DELEGATE_H_
#define CHROME_RENDERER_EXTENSIONS_RUNTIME_HOOKS_DELEGATE_H_

#include <vector>

#include "base/macros.h"
#include "extensions/renderer/bindings/api_binding_hooks_delegate.h"
#include "v8/include/v8.h"

namespace extensions {
class NativeRendererMessagingService;
class ScriptContext;

// The custom hooks for the runtime API.
class RuntimeHooksDelegate : public APIBindingHooksDelegate {
 public:
  explicit RuntimeHooksDelegate(
      NativeRendererMessagingService* messaging_service);
  ~RuntimeHooksDelegate() override;

  // Returns an absolute url for a path inside of an extension, as requested
  // through the getURL API call.
  // NOTE: Static as the logic is used by both the runtime and extension
  // hooks.
  static APIBindingHooks::RequestResult GetURL(
      ScriptContext* script_context,
      const std::vector<v8::Local<v8::Value>>& arguments);

  // APIBindingHooksDelegate:
  APIBindingHooks::RequestResult HandleRequest(
      const std::string& method_name,
      const APISignature* signature,
      v8::Local<v8::Context> context,
      std::vector<v8::Local<v8::Value>>* arguments,
      const APITypeReferenceMap& refs) override;
  void InitializeTemplate(v8::Isolate* isolate,
                          v8::Local<v8::ObjectTemplate> object_template,
                          const APITypeReferenceMap& type_refs) override;

 private:
  // Request handlers for the corresponding API methods.
  APIBindingHooks::RequestResult HandleGetManifest(
      ScriptContext* script_context,
      const std::vector<v8::Local<v8::Value>>& parsed_arguments);
  APIBindingHooks::RequestResult HandleGetURL(
      ScriptContext* script_context,
      const std::vector<v8::Local<v8::Value>>& arguments);
  APIBindingHooks::RequestResult HandleSendMessage(
      ScriptContext* script_context,
      const std::vector<v8::Local<v8::Value>>& arguments);
  APIBindingHooks::RequestResult HandleSendNativeMessage(
      ScriptContext* script_context,
      const std::vector<v8::Local<v8::Value>>& arguments);
  APIBindingHooks::RequestResult HandleConnect(
      ScriptContext* script_context,
      const std::vector<v8::Local<v8::Value>>& arguments);
  APIBindingHooks::RequestResult HandleConnectNative(
      ScriptContext* script_context,
      const std::vector<v8::Local<v8::Value>>& arguments);
  APIBindingHooks::RequestResult HandleGetBackgroundPage(
      ScriptContext* script_context,
      const std::vector<v8::Local<v8::Value>>& arguments);
  APIBindingHooks::RequestResult HandleGetPackageDirectoryEntryCallback(
      ScriptContext* script_context,
      const std::vector<v8::Local<v8::Value>>& arguments);

  // The messaging service to handle connect() and sendMessage() calls.
  // Guaranteed to outlive this object.
  NativeRendererMessagingService* const messaging_service_;

  DISALLOW_COPY_AND_ASSIGN(RuntimeHooksDelegate);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_RUNTIME_HOOKS_DELEGATE_H_
