// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/boringssl_trust_token_test_utils.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/ranges/algorithm.h"
#include "services/network/trust_tokens/scoped_boringssl_bytes.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/trust_token.h"

namespace network {

TestTrustTokenIssuer::VerificationKey::VerificationKey(
    uint32_t id,
    std::vector<uint8_t> value)
    : id(id), value(std::move(value)) {}
TestTrustTokenIssuer::VerificationKey::~VerificationKey() = default;
TestTrustTokenIssuer::VerificationKey::VerificationKey(const VerificationKey&) =
    default;

TestTrustTokenIssuer::TokenKeyPair::TokenKeyPair() = default;
TestTrustTokenIssuer::TokenKeyPair::~TokenKeyPair() = default;
TestTrustTokenIssuer::TokenKeyPair::TokenKeyPair(const TokenKeyPair&) = default;
TestTrustTokenIssuer::TokenKeyPair::TokenKeyPair(
    std::vector<uint8_t> signing,
    std::vector<uint8_t> verification,
    uint32_t key_id)
    : signing(std::move(signing)),
      verification(std::move(verification)),
      key_id(key_id) {}

TestTrustTokenIssuer::TokenKeyPair TestTrustTokenIssuer::GenerateTokenKeyPair(
    uint32_t key_id) {
  TestTrustTokenIssuer::TokenKeyPair keys;

  keys.key_id = key_id;

  keys.signing.resize(TRUST_TOKEN_MAX_PRIVATE_KEY_SIZE);
  keys.verification.resize(TRUST_TOKEN_MAX_PUBLIC_KEY_SIZE);

  size_t signing_key_len, verification_key_len;
  CHECK(TRUST_TOKEN_generate_key(
      TRUST_TOKEN_experiment_v2_pmb(), keys.signing.data(), &signing_key_len,
      keys.signing.size(), keys.verification.data(), &verification_key_len,
      keys.verification.size(), key_id));
  keys.signing.resize(signing_key_len);
  keys.verification.resize(verification_key_len);

  return keys;
}

TestTrustTokenIssuer::~TestTrustTokenIssuer() = default;
TestTrustTokenIssuer::TestTrustTokenIssuer(uint8_t num_keys,
                                           uint8_t max_issuance)
    : max_issuance_(max_issuance) {
  for (size_t i = 0; i < num_keys; ++i) {
    // Use key IDs 0, 3, 6, ... to ensure that no logic relies on key IDs
    // being consecutive.
    keys_.emplace_back(GenerateTokenKeyPair(/*key_id=*/3 * i));
  }
  for (const auto& key : keys_) {
    VerificationKey verification_key = {
        key.key_id,
        key.verification,
    };
    verification_keys_.emplace_back(std::move(verification_key));
  }

  bssl::UniquePtr<TRUST_TOKEN_ISSUER> ctx(TRUST_TOKEN_ISSUER_new(
      /*method=*/TRUST_TOKEN_experiment_v2_pmb(),
      /*max_batchsize=*/max_issuance_));

  for (const TokenKeyPair& key_pair : keys_) {
    TRUST_TOKEN_ISSUER_add_key(/*ctx=*/ctx.get(),
                               /*key=*/key_pair.signing.data(),
                               /*key_len=*/key_pair.signing.size());
  }
  // Copying the comment from evp.h:
  // The [Ed25519] RFC 8032 private key format is the 32-byte prefix of
  // |ED25519_sign|'s 64-byte private key.
  uint8_t public_key[32], private_key[64];
  ED25519_keypair(/*out_public_key=*/public_key,
                  /*out_private_key=*/private_key);
  bssl::UniquePtr<EVP_PKEY> issuer_rr_key(EVP_PKEY_new_raw_private_key(
      /*type=*/EVP_PKEY_ED25519, /*unused=*/nullptr, /*in=*/private_key,
      /*len=*/32));
  TRUST_TOKEN_ISSUER_set_srr_key(/*ctx=*/ctx.get(),
                                 /*key=*/issuer_rr_key.get());

  ctx_ = std::move(ctx);
}

std::optional<std::string> TestTrustTokenIssuer::Issue(
    const std::string& issuance_request) const {
  DCHECK(!keys_.empty());
  return IssueUsingKey(issuance_request, keys_.front().key_id);
}

std::optional<std::string> TestTrustTokenIssuer::IssueUsingKey(
    const std::string& issuance_request,
    const uint32_t& key_id) const {
  std::string raw_issuance_request;
  if (!base::Base64Decode(issuance_request, &raw_issuance_request)) {
    return std::nullopt;
  }
  ScopedBoringsslBytes raw_issuance_response;
  size_t num_tokens_issued;

  if (!TRUST_TOKEN_ISSUER_issue(
          /*ctx=*/ctx_.get(),
          /*out=*/&raw_issuance_response.mutable_ptr()->AsEphemeralRawAddr(),
          /*out_len=*/raw_issuance_response.mutable_len(),
          /*out_tokens_issued=*/&num_tokens_issued,
          /*request=*/
          base::as_bytes(base::make_span(raw_issuance_request)).data(),
          /*request_len=*/raw_issuance_request.size(),
          /*public_metadata=*/key_id,
          /*private_metadata=*/kPrivateMetadata,
          /*max_issuance=*/max_issuance_)) {
    return std::nullopt;
  }

  return base::Base64Encode(raw_issuance_response.as_span());
}

bssl::UniquePtr<TRUST_TOKEN> TestTrustTokenIssuer::Redeem(
    const std::string& redemption_request) const {
  std::string raw_redemption_request;
  if (!base::Base64Decode(redemption_request, &raw_redemption_request)) {
    return nullptr;
  }

  TRUST_TOKEN* redeemed_token;
  ScopedBoringsslBytes redeemed_client_data;
  uint32_t received_public_metadata;
  uint8_t received_private_metadata;

  if (TRUST_TOKEN_ISSUER_redeem(
          /*ctx=*/ctx_.get(), /*out_public=*/&received_public_metadata,
          /*out_private=*/&received_private_metadata,
          /*out_token=*/&redeemed_token,
          /*out_client_data=*/
          &redeemed_client_data.mutable_ptr()->AsEphemeralRawAddr(),
          /*out_client_data_len=*/redeemed_client_data.mutable_len(),
          /*request=*/
          base::as_bytes(base::make_span(raw_redemption_request)).data(),
          /*request_len=*/raw_redemption_request.size()) != 1) {
    return nullptr;
  };

  EXPECT_EQ(received_private_metadata, kPrivateMetadata);
  EXPECT_NE(base::ranges::find_if(keys_,
                                  [&received_public_metadata](auto& key) {
                                    return key.key_id ==
                                           received_public_metadata;
                                  }),
            std::end(keys_));

  return bssl::UniquePtr<TRUST_TOKEN>(redeemed_token);
}

bssl::UniquePtr<TRUST_TOKEN> TestTrustTokenIssuer::RedeemOverMessage(
    const std::string& redemption_request,
    const std::string& message) const {
  std::string raw_redemption_request;
  if (!base::Base64Decode(redemption_request, &raw_redemption_request)) {
    return nullptr;
  }

  TRUST_TOKEN* redeemed_token;
  ScopedBoringsslBytes redeemed_client_data;
  uint32_t received_public_metadata;
  uint8_t received_private_metadata;

  if (TRUST_TOKEN_ISSUER_redeem_over_message(
          /*ctx=*/ctx_.get(), /*out_public=*/&received_public_metadata,
          /*out_private=*/&received_private_metadata,
          /*out_token=*/&redeemed_token,
          /*out_client_data=*/
          &redeemed_client_data.mutable_ptr()->AsEphemeralRawAddr(),
          /*out_client_data_len=*/redeemed_client_data.mutable_len(),
          /*request=*/
          base::as_bytes(base::make_span(raw_redemption_request)).data(),
          /*request_len=*/raw_redemption_request.size(),
          /*msg=*/reinterpret_cast<const unsigned char*>(message.c_str()),
          /*msg_len=*/message.size()) != 1) {
    return nullptr;
  }

  EXPECT_EQ(received_private_metadata, 1);
  EXPECT_NE(base::ranges::find_if(keys_,
                                  [&received_public_metadata](auto& key) {
                                    return key.key_id ==
                                           received_public_metadata;
                                  }),
            std::end(keys_));

  return bssl::UniquePtr<TRUST_TOKEN>(redeemed_token);
}

}  // namespace network
