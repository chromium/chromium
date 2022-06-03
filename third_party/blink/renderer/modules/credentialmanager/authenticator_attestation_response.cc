// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanager/authenticator_attestation_response.h"

#include "third_party/blink/renderer/modules/credentialmanager/credential_manager_type_converters.h"

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
  return ret;
}

void AuthenticatorAttestationResponse::Trace(Visitor* visitor) const {
  visitor->Trace(attestation_object_);
  visitor->Trace(authenticator_data_);
  visitor->Trace(public_key_der_);
  AuthenticatorResponse::Trace(visitor);
}

}  // namespace blink
