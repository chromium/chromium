// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/authenticator_assertion_response.h"

#include <utility>

#include "third_party/blink/renderer/bindings/modules/v8/v8_authenticator_assertion_response_js_on.h"
#include "third_party/blink/renderer/modules/credentialmanagement/json.h"

namespace blink {

AuthenticatorAssertionResponse::AuthenticatorAssertionResponse(
    const Vector<uint8_t> client_data_json,
    const Vector<uint8_t> authenticator_data,
    const Vector<uint8_t> signature,
    std::optional<Vector<uint8_t>> optional_user_handle)
    : AuthenticatorAssertionResponse(
          DOMArrayBuffer::Create(client_data_json),
          DOMArrayBuffer::Create(authenticator_data),
          DOMArrayBuffer::Create(signature),
          optional_user_handle && optional_user_handle->size() > 0
              ? DOMArrayBuffer::Create(std::move(*optional_user_handle))
              : nullptr) {}

AuthenticatorAssertionResponse::AuthenticatorAssertionResponse(
    DOMArrayBuffer* client_data_json,
    DOMArrayBuffer* authenticator_data,
    DOMArrayBuffer* signature,
    DOMArrayBuffer* user_handle)
    : AuthenticatorResponse(client_data_json),
      authenticator_data_(authenticator_data),
      signature_(signature),
      user_handle_(user_handle) {}

AuthenticatorAssertionResponse::~AuthenticatorAssertionResponse() = default;

absl::variant<AuthenticatorAssertionResponseJSON*,
              AuthenticatorAttestationResponseJSON*>
AuthenticatorAssertionResponse::toJSON() const {
  auto* json = AuthenticatorAssertionResponseJSON::Create();
  json->setClientDataJSON(WebAuthnBase64UrlEncode(clientDataJSON()));
  json->setAuthenticatorData(WebAuthnBase64UrlEncode(authenticatorData()));
  json->setSignature(WebAuthnBase64UrlEncode(signature()));
  if (user_handle_) {
    json->setUserHandle(WebAuthnBase64UrlEncode(userHandle()));
  }
  return json;
}

void AuthenticatorAssertionResponse::Trace(Visitor* visitor) const {
  visitor->Trace(authenticator_data_);
  visitor->Trace(signature_);
  visitor->Trace(user_handle_);
  AuthenticatorResponse::Trace(visitor);
}

}  // namespace blink
