// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/scoped_promise_resolver.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

ScopedPromiseResolver::ScopedPromiseResolver(
    ScriptPromiseResolverBase* resolver,
    ConnectionType connection_type)
    : resolver_(resolver), connection_type_(connection_type) {}

ScopedPromiseResolver::~ScopedPromiseResolver() {
  if (resolver_)
    OnConnectionError();
}

ScriptPromiseResolverBase* ScopedPromiseResolver::Release() {
  return resolver_.Release();
}

void ScopedPromiseResolver::OnConnectionError() {
  switch (connection_type_) {
    case ConnectionType::kAuthenticator:
      resolver_->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "Error connecting to Web Authentication service."));
      break;
    case ConnectionType::kFedCm:
      resolver_->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "Error connecting to Federated Credential service."));
      break;
    case ConnectionType::kCredentialManager:
      resolver_->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "Error connecting to Credential Management service."));
      break;
    case ConnectionType::kPaymentConfirmation:
      resolver_->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "Error connecting to Secure Payment Confirmation service."));
      break;
  }
}

}  // namespace blink
