// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/identity_credential_error.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_credential_error_init.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

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
      error_code_(RuntimeEnabledFeatures::FedCmErrorAttributeEnabled()
                      ? (options->hasError() ? options->error() : "")
                      : (options->hasCode() ? options->code() : "")),
      url_(options->hasUrl() ? options->url() : "") {}

IdentityCredentialError::IdentityCredentialError(const String& message,
                                                 const String& code,
                                                 const String& url)
    : DOMException(DOMExceptionCode::kIdentityCredentialError, message),
      error_code_(code),
      url_(url) {}

String IdentityCredentialError::code(ExceptionState& exception_state) const {
  // Add console warning when code attribute is accessed
  if (ScriptState* script_state =
          ScriptState::ForCurrentRealm(exception_state.GetIsolate())) {
    if (ExecutionContext* execution_context =
            ExecutionContext::From(script_state)) {
      execution_context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kJavaScript,
          mojom::blink::ConsoleMessageLevel::kWarning,
          "The 'code' attribute of IdentityCredentialError is deprecated and "
          "will be removed in Chrome 145. Use the 'error' attribute instead."));
    }
  }

  return error_code_;
}

String IdentityCredentialError::error() const {
  return error_code_;
}

}  // namespace blink
