// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_DECLARATIVE_CONTENT_HOOKS_DELEGATE_H_
#define EXTENSIONS_RENDERER_API_DECLARATIVE_CONTENT_HOOKS_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "extensions/renderer/bindings/api_binding_hooks_delegate.h"
#include "v8/include/v8-forward.h"

namespace extensions {
class APITypeReferenceMap;
class ArgumentSpec;

// Custom hooks for the declarativeContent API.
class DeclarativeContentHooksDelegate : public APIBindingHooksDelegate {
 public:
  // The callback type for handling an API call.
  using HandlerCallback =
      base::RepeatingCallback<void(const v8::FunctionCallbackInfo<v8::Value>&)>;

  DeclarativeContentHooksDelegate();

  DeclarativeContentHooksDelegate(const DeclarativeContentHooksDelegate&) =
      delete;
  DeclarativeContentHooksDelegate& operator=(
      const DeclarativeContentHooksDelegate&) = delete;

  ~DeclarativeContentHooksDelegate() override;

  void InitializeTemplate(v8::Isolate* isolate,
                          v8::Local<v8::ObjectTemplate> object_template,
                          const APITypeReferenceMap& type_refs) override;

 private:
  void HandleCall(const ArgumentSpec* spec,
                  const APITypeReferenceMap* type_refs,
                  const std::string& type_name,
                  const v8::FunctionCallbackInfo<v8::Value>& info);

  // The owned callbacks for the constructor functions. Since these pointers
  // are passed to v8::External, we need to use unique_ptrs rather than just
  // storing the callback directly in a vector.
  std::vector<std::unique_ptr<HandlerCallback>> callbacks_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_DECLARATIVE_CONTENT_HOOKS_DELEGATE_H_
