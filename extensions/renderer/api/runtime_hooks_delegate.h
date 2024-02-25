// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_RUNTIME_HOOKS_DELEGATE_H_
#define EXTENSIONS_RENDERER_API_RUNTIME_HOOKS_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "extensions/renderer/bindings/api_binding_hooks_delegate.h"
#include "extensions/renderer/bindings/api_signature.h"
#include "v8/include/v8-forward.h"

namespace extensions {
class NativeRendererMessagingService;
class ScriptContext;

// The custom hooks for the runtime API.
class RuntimeHooksDelegate : public APIBindingHooksDelegate {
 public:
  explicit RuntimeHooksDelegate(
      NativeRendererMessagingService* messaging_service);

  RuntimeHooksDelegate(const RuntimeHooksDelegate&) = delete;
  RuntimeHooksDelegate& operator=(const RuntimeHooksDelegate&) = delete;

  ~RuntimeHooksDelegate() override;

  // Returns an absolute url for a path inside of an extension, as requested
  // through the getURL API call.
  // NOTE: Static as the logic is used by both the runtime and extension
  // hooks.
  static APIBindingHooks::RequestResult GetURL(
      ScriptContext* script_context,
      const v8::LocalVector<v8::Value>& arguments);

  // APIBindingHooksDelegate:
  APIBindingHooks::RequestResult HandleRequest(
      const std::string& method_name,
      const APISignature* signature,
      v8::Local<v8::Context> context,
      v8::LocalVector<v8::Value>* arguments,
      const APITypeReferenceMap& refs) override;
  void InitializeTemplate(v8::Isolate* isolate,
                          v8::Local<v8::ObjectTemplate> object_template,
                          const APITypeReferenceMap& type_refs) override;

 private:
  // Request handlers for the corresponding API methods.
  APIBindingHooks::RequestResult HandleGetManifest(
      ScriptContext* script_context,
      const APISignature::V8ParseResult& parse_result);
  APIBindingHooks::RequestResult HandleGetURL(
      ScriptContext* script_context,
      const APISignature::V8ParseResult& parse_result);
  APIBindingHooks::RequestResult HandleSendMessage(
      ScriptContext* script_context,
      const APISignature::V8ParseResult& parse_result);
  APIBindingHooks::RequestResult HandleSendNativeMessage(
      ScriptContext* script_context,
      const APISignature::V8ParseResult& parse_result);
  APIBindingHooks::RequestResult HandleConnect(
      ScriptContext* script_context,
      const APISignature::V8ParseResult& parse_result);
  APIBindingHooks::RequestResult HandleConnectNative(
      ScriptContext* script_context,
      const APISignature::V8ParseResult& parse_result);
  APIBindingHooks::RequestResult HandleGetBackgroundPage(
      ScriptContext* script_context,
      const APISignature::V8ParseResult& parse_result);
  APIBindingHooks::RequestResult HandleGetPackageDirectoryEntryCallback(
      ScriptContext* script_context,
      const APISignature::V8ParseResult& parse_result);
  APIBindingHooks::RequestResult HandleRequestUpdateCheck(
      ScriptContext* script_context,
      const APISignature::V8ParseResult& parse_result);

  // The messaging service to handle connect() and sendMessage() calls.
  // Guaranteed to outlive this object.
  const raw_ptr<NativeRendererMessagingService, DanglingUntriaged>
      messaging_service_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_RUNTIME_HOOKS_DELEGATE_H_
