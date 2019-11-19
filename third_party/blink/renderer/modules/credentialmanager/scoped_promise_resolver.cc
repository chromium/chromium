// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanager/scoped_promise_resolver.h"

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

ScopedPromiseResolver::ScopedPromiseResolver(ScriptPromiseResolver* resolver)
    : resolver_(resolver) {}

ScopedPromiseResolver::~ScopedPromiseResolver() {
  if (resolver_)
    OnConnectionError();
}

ScriptPromiseResolver* ScopedPromiseResolver::Release() {
  return resolver_.Release();
}

void ScopedPromiseResolver::OnConnectionError() {
  // The only anticipated reason for a connection error is that the embedder
  // does not implement mojom::AuthenticatorImpl.
  resolver_->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNotSupportedError,
      "The user agent does not support public key credentials."));
}

}  // namespace blink
