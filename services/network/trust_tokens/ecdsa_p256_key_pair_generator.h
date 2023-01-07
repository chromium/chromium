// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_ECDSA_P256_KEY_PAIR_GENERATOR_H_
#define SERVICES_NETWORK_TRUST_TOKENS_ECDSA_P256_KEY_PAIR_GENERATOR_H_

#include "services/network/trust_tokens/trust_token_request_redemption_helper.h"

namespace network {

// EcdsaP256KeyPairGenerator generates an ECDSA key pair based on the NIST
// P-256 curve. The |verification_key_out| is encoded as an EC point in the
// uncompressed point format and the |signing_key_out| is encoded as a PKCS #8
// PrivateKeyInfo block.
class EcdsaP256KeyPairGenerator
    : public TrustTokenRequestRedemptionHelper::KeyPairGenerator {
 public:
  EcdsaP256KeyPairGenerator() = default;
  ~EcdsaP256KeyPairGenerator() override = default;

  // TrustTokenRequestRedemptionHelper::KeyPairGenerator implementation:
  bool Generate(std::string* signing_key_out,
                std::string* verification_key_out) override;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_ECDSA_P256_KEY_PAIR_GENERATOR_H_
