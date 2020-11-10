// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_ED25519_TRUST_TOKEN_REQUEST_SIGNER_H_
#define SERVICES_NETWORK_TRUST_TOKENS_ED25519_TRUST_TOKEN_REQUEST_SIGNER_H_

#include "services/network/trust_tokens/trust_token_request_signing_helper.h"

namespace network {

// Ed25519TrustTokenRequestSigner provides a wrapper around BoringSSL's Ed25519
// signing and verification routines capable of satisfying the Trust Tokens
// signing request helper's Signer delegate interface.
class Ed25519TrustTokenRequestSigner
    : public TrustTokenRequestSigningHelper::Signer {
 public:
  Ed25519TrustTokenRequestSigner();
  ~Ed25519TrustTokenRequestSigner() override;

  // TrustTokenRequestSigningHelper::Signer implementation:
  base::Optional<std::vector<uint8_t>> Sign(
      base::span<const uint8_t> key,
      base::span<const uint8_t> data) override;

  bool Verify(base::span<const uint8_t> data,
              base::span<const uint8_t> signature,
              base::span<const uint8_t> verification_key) override;

  std::string GetAlgorithmIdentifier() override;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_ED25519_TRUST_TOKEN_REQUEST_SIGNER_H_
