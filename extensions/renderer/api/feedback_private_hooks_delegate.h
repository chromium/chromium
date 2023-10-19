// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_FEEDBACK_PRIVATE_HOOKS_DELEGATE_H_
#define EXTENSIONS_RENDERER_API_FEEDBACK_PRIVATE_HOOKS_DELEGATE_H_

#include "extensions/renderer/bindings/api_binding_hooks_delegate.h"
#include "extensions/renderer/bindings/api_signature.h"
#include "v8/include/v8-forward.h"

namespace extensions {

// Custom native hooks for the feedbackPrivate API.
class FeedbackPrivateHooksDelegate : public APIBindingHooksDelegate {
 public:
  FeedbackPrivateHooksDelegate();

  FeedbackPrivateHooksDelegate(const FeedbackPrivateHooksDelegate&) = delete;
  FeedbackPrivateHooksDelegate& operator=(const FeedbackPrivateHooksDelegate&) =
      delete;

  ~FeedbackPrivateHooksDelegate() override;

  // APIBindingHooksDelegate:
  APIBindingHooks::RequestResult HandleRequest(
      const std::string& method_name,
      const APISignature* signature,
      v8::Local<v8::Context> context,
      v8::LocalVector<v8::Value>* arguments,
      const APITypeReferenceMap& refs) override;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_FEEDBACK_PRIVATE_HOOKS_DELEGATE_H_
