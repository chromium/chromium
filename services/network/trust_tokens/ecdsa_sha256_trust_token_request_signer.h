// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_ECDSA_SHA256_TRUST_TOKEN_REQUEST_SIGNER_H_
#define SERVICES_NETWORK_TRUST_TOKENS_ECDSA_SHA256_TRUST_TOKEN_REQUEST_SIGNER_H_

#include "services/network/trust_tokens/trust_token_request_signing_helper.h"

namespace network {

// EcdsaSha256TrustTokenRequestSigner provides a wrapper around BoringSSL's
// ECDSA signing and verification routines capable of satisfying the Trust
// Tokens signing request helper's Signer delegate interface. The signature is a
// DER encoded ECDSA-Sig-Value from RFC 3279.
class EcdsaSha256TrustTokenRequestSigner
    : public TrustTokenRequestSigningHelper::Signer {
 public:
  EcdsaSha256TrustTokenRequestSigner();
  ~EcdsaSha256TrustTokenRequestSigner() override;

  // TrustTokenRequestSigningHelper::Signer implementation:
  absl::optional<std::vector<uint8_t>> Sign(
      base::span<const uint8_t> key,
      base::span<const uint8_t> data) override;

  bool Verify(base::span<const uint8_t> data,
              base::span<const uint8_t> signature,
              base::span<const uint8_t> verification_key) override;

  std::string GetAlgorithmIdentifier() const override;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_ECDSA_SHA256_TRUST_TOKEN_REQUEST_SIGNER_H_
