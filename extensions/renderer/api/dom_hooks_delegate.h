// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_DOM_HOOKS_DELEGATE_H_
#define EXTENSIONS_RENDERER_API_DOM_HOOKS_DELEGATE_H_

#include <string>

#include "extensions/renderer/bindings/api_binding_hooks_delegate.h"
#include "v8/include/v8-forward.h"

namespace extensions {
class ScriptContext;

// The custom hooks for the chrome.dom API.
class DOMHooksDelegate : public APIBindingHooksDelegate {
 public:
  DOMHooksDelegate();
  ~DOMHooksDelegate() override;
  DOMHooksDelegate(const DOMHooksDelegate&) = delete;
  DOMHooksDelegate& operator=(const DOMHooksDelegate&) = delete;

  // APIBindingHooksDelegate:
  APIBindingHooks::RequestResult HandleRequest(
      const std::string& method_name,
      const APISignature* signature,
      v8::Local<v8::Context> context,
      v8::LocalVector<v8::Value>* arguments,
      const APITypeReferenceMap& refs) override;

 private:
  v8::Local<v8::Value> OpenOrClosedShadowRoot(
      ScriptContext* script_context,
      const v8::LocalVector<v8::Value>& parsed_arguments);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_DOM_HOOKS_DELEGATE_H_
