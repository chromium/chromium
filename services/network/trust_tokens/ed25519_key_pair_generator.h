// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_ED25519_KEY_PAIR_GENERATOR_H_
#define SERVICES_NETWORK_TRUST_TOKENS_ED25519_KEY_PAIR_GENERATOR_H_

#include "services/network/trust_tokens/trust_token_request_redemption_helper.h"

namespace network {

// Ed25519KeyPairGenerator generates an Ed25519 key pair in BoringSSL's
// native encoding.
class Ed25519KeyPairGenerator
    : public TrustTokenRequestRedemptionHelper::KeyPairGenerator {
 public:
  Ed25519KeyPairGenerator() = default;
  ~Ed25519KeyPairGenerator() override = default;

  // TrustTokenRequestRedemptionHelper::KeyPairGenerator implementation:
  bool Generate(std::string* signing_key_out,
                std::string* verification_key_out) override;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_ED25519_KEY_PAIR_GENERATOR_H_
