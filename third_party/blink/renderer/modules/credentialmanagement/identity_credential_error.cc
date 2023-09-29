// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/identity_credential_error.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_credential_error_init.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// static
IdentityCredentialError* IdentityCredentialError::Create(
    const String& message,
    const IdentityCredentialErrorInit* options) {
  return MakeGarbageCollected<IdentityCredentialError>(message, options);
}

IdentityCredentialError::IdentityCredentialError(
    const String& message,
    const IdentityCredentialErrorInit* options)
    : DOMException(DOMExceptionCode::kIdentityCredentialError, message),
      code_(options->hasCode() ? options->code() : ""),
      url_(options->hasUrl() ? options->url() : "") {}

IdentityCredentialError::IdentityCredentialError(const String& message,
                                                 const String& code,
                                                 const String& url)
    : DOMException(DOMExceptionCode::kIdentityCredentialError, message),
      code_(code),
      url_(url) {}

}  // namespace blink
