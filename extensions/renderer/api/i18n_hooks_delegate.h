// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_I18N_HOOKS_DELEGATE_H_
#define EXTENSIONS_RENDERER_API_I18N_HOOKS_DELEGATE_H_

#include "extensions/renderer/bindings/api_binding_hooks_delegate.h"
#include "extensions/renderer/bindings/api_signature.h"
#include "v8/include/v8-forward.h"

namespace extensions {
class ScriptContext;

// Custom native hooks for the i18n API.
class I18nHooksDelegate : public APIBindingHooksDelegate {
 public:
  I18nHooksDelegate();

  I18nHooksDelegate(const I18nHooksDelegate&) = delete;
  I18nHooksDelegate& operator=(const I18nHooksDelegate&) = delete;

  ~I18nHooksDelegate() override;

  // APIBindingHooksDelegate:
  APIBindingHooks::RequestResult HandleRequest(
      const std::string& method_name,
      const APISignature* signature,
      v8::Local<v8::Context> context,
      v8::LocalVector<v8::Value>* arguments,
      const APITypeReferenceMap& refs) override;

 private:
  // Method handlers:
  APIBindingHooks::RequestResult HandleGetMessage(
      ScriptContext* script_context,
      const APISignature::V8ParseResult& parse_result);
  APIBindingHooks::RequestResult HandleGetUILanguage(
      ScriptContext* script_context,
      const APISignature::V8ParseResult& parse_result);
  APIBindingHooks::RequestResult HandleDetectLanguage(
      ScriptContext* script_context,
      const APISignature::V8ParseResult& parse_result);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_I18N_HOOKS_DELEGATE_H_
