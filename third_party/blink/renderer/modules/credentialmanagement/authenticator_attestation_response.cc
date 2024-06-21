// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/authenticator_attestation_response.h"

#include <algorithm>

#include "third_party/blink/renderer/bindings/modules/v8/v8_authenticator_attestation_response_js_on.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_type_converters.h"
#include "third_party/blink/renderer/modules/credentialmanagement/json.h"

namespace blink {

AuthenticatorAttestationResponse::AuthenticatorAttestationResponse(
    DOMArrayBuffer* client_data_json,
    DOMArrayBuffer* attestation_object,
    Vector<mojom::AuthenticatorTransport> transports,
    DOMArrayBuffer* authenticator_data,
    DOMArrayBuffer* public_key_der,
    int32_t public_key_algo)
    : AuthenticatorResponse(client_data_json),
      attestation_object_(attestation_object),
      transports_(std::move(transports)),
      authenticator_data_(authenticator_data),
      public_key_der_(public_key_der),
      public_key_algo_(public_key_algo) {}

AuthenticatorAttestationResponse::~AuthenticatorAttestationResponse() = default;

Vector<String> AuthenticatorAttestationResponse::getTransports() const {
  Vector<String> ret;
  for (auto transport : transports_) {
    ret.emplace_back(mojo::ConvertTo<String>(transport));
  }
  std::sort(ret.begin(), ret.end(), WTF::CodeUnitCompareLessThan);
  ret.erase(std::unique(ret.begin(), ret.end()), ret.end());
  return ret;
}

absl::variant<AuthenticatorAssertionResponseJSON*,
              AuthenticatorAttestationResponseJSON*>
AuthenticatorAttestationResponse::toJSON() const {
  auto* json = AuthenticatorAttestationResponseJSON::Create();
  json->setClientDataJSON(WebAuthnBase64UrlEncode(clientDataJSON()));
  json->setAuthenticatorData(WebAuthnBase64UrlEncode(getAuthenticatorData()));
  json->setTransports(getTransports());
  if (public_key_der_) {
    json->setPublicKey(WebAuthnBase64UrlEncode(getPublicKey()));
  }
  json->setPublicKeyAlgorithm(getPublicKeyAlgorithm());
  json->setAttestationObject(WebAuthnBase64UrlEncode(attestationObject()));
  return json;
}

void AuthenticatorAttestationResponse::Trace(Visitor* visitor) const {
  visitor->Trace(attestation_object_);
  visitor->Trace(authenticator_data_);
  visitor->Trace(public_key_der_);
  AuthenticatorResponse::Trace(visitor);
}

}  // namespace blink
