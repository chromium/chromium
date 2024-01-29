// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_BORINGSSL_TRUST_TOKEN_TEST_UTILS_H_
#define SERVICES_NETWORK_TRUST_TOKENS_BORINGSSL_TRUST_TOKEN_TEST_UTILS_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/trust_token.h"

namespace network {

// This class implements a test Private State Token issuer.
// https://github.com/WICG/trust-token-api/blob/main/ISSUER_PROTOCOL.md
class TestTrustTokenIssuer {
 public:
  struct VerificationKey {
    VerificationKey(uint32_t id, std::vector<uint8_t> value);
    ~VerificationKey();
    VerificationKey(const VerificationKey&);

    uint32_t id;
    std::vector<uint8_t> value;
  };

  explicit TestTrustTokenIssuer(uint8_t num_keys, uint8_t max_issuance);
  ~TestTrustTokenIssuer();

  // Receives a base64 encoded `issuance_request` and issue a token.
  // https://github.com/WICG/trust-token-api/blob/main/ISSUER_PROTOCOL.md#issuing-tokens
  std::optional<std::string> Issue(const std::string& issuance_request) const;

  // Receives a base64 encoded `issuance_request` and issue a token using the
  // `key_id` received. The `key_id` must be present in the instance's `keys_`.
  // https://github.com/WICG/trust-token-api/blob/main/ISSUER_PROTOCOL.md#issuing-tokens
  std::optional<std::string> IssueUsingKey(const std::string& issuance_request,
                                           const uint32_t& key_id) const;

  // Receives a base64 binary blob `redemption_request` and completes the
  // redeem crypto protocol.
  // https://github.com/WICG/trust-token-api/blob/main/ISSUER_PROTOCOL.md#redeeming-tokens
  bssl::UniquePtr<TRUST_TOKEN> Redeem(
      const std::string& redemption_request) const;

  // Receives a base64 binary blob `redemption_request` and completes the
  // redeem crypto protocol over the `message`.
  // https://github.com/WICG/trust-token-api/blob/main/ISSUER_PROTOCOL.md#redeeming-tokens
  bssl::UniquePtr<TRUST_TOKEN> RedeemOverMessage(
      const std::string& redemption_request,
      const std::string& message) const;

  const std::vector<VerificationKey>& Keys() const {
    return verification_keys_;
  }

 private:
  // A static private metadata value is used. The test issuer sets it only to
  // confirm that it can encode the private metadata on issuance and recover it
  // on redemption. see.
  // https://github.com/WICG/trust-token-api#extension-private-metadata for
  // details on the private metatadata extension.
  static constexpr uint8_t kPrivateMetadata = 1;
  struct TokenKeyPair {
    TokenKeyPair();
    TokenKeyPair(const TokenKeyPair&);
    TokenKeyPair(std::vector<uint8_t> signing,
                 std::vector<uint8_t> verification,
                 uint32_t key_id);
    ~TokenKeyPair();

    std::vector<uint8_t> signing;
    std::vector<uint8_t> verification;
    uint32_t key_id;
  };

  TokenKeyPair GenerateTokenKeyPair(uint32_t key_id);

  // Maintains Trust Tokens protocol state.
  bssl::UniquePtr<TRUST_TOKEN_ISSUER> ctx_;

  // This is the set of signing/verification (private/public) keys maintained by
  // the Trust Token issuer.
  std::vector<TokenKeyPair> keys_;

  // To register as Trust Token issuer, the issuer needs to expose verification
  // keys for them to be used by the browser. This property essentially exposes
  // the verification keys contained in the `keys_`.
  std::vector<VerificationKey> verification_keys_;

  uint8_t max_issuance_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_BORINGSSL_TRUST_TOKEN_TEST_UTILS_H_
