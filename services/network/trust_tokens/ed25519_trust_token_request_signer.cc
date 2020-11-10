// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/ed25519_trust_token_request_signer.h"

#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace network {

Ed25519TrustTokenRequestSigner::Ed25519TrustTokenRequestSigner() = default;
Ed25519TrustTokenRequestSigner::~Ed25519TrustTokenRequestSigner() = default;

base::Optional<std::vector<uint8_t>> Ed25519TrustTokenRequestSigner::Sign(
    base::span<const uint8_t> key,
    base::span<const uint8_t> data) {
  if (key.size() != ED25519_PRIVATE_KEY_LEN)
    return base::nullopt;

  std::vector<uint8_t> ret(ED25519_SIGNATURE_LEN);

  if (!ED25519_sign(ret.data(), data.data(), data.size(), key.data()))
    return base::nullopt;

  return ret;
}

bool Ed25519TrustTokenRequestSigner::Verify(
    base::span<const uint8_t> data,
    base::span<const uint8_t> signature,
    base::span<const uint8_t> verification_key) {
  if (signature.size() != ED25519_SIGNATURE_LEN)
    return false;
  if (verification_key.size() != ED25519_PUBLIC_KEY_LEN)
    return false;

  return ED25519_verify(data.data(), data.size(), signature.data(),
                        verification_key.data());
}

std::string Ed25519TrustTokenRequestSigner::GetAlgorithmIdentifier() {
  return "ed25519";
}

}  // namespace network
