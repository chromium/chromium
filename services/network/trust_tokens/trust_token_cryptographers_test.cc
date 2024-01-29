// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "services/network/trust_tokens/boringssl_trust_token_issuance_cryptographer.h"
#include "services/network/trust_tokens/boringssl_trust_token_redemption_cryptographer.h"
#include "services/network/trust_tokens/boringssl_trust_token_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/trust_token.h"
#include "url/gurl.h"
#include "url/origin.h"

// These integration tests confirm that BoringsslTrustToken{Issuance,
// Redemption}Cryptographer are capable of completing an end-to-end
// issuance-and-redemption flow against the server-side BoringSSL issuer logic.

namespace network {

namespace {

const mojom::TrustTokenProtocolVersion kProtocolVersion =
    mojom::TrustTokenProtocolVersion::kTrustTokenV3Pmb;

// Choose this number to be > 1 but fairly small: setting it to 10
// led to the test running for 2.5 sec on a debug build.
constexpr size_t kNumTokensToRequest = 3;

std::string_view as_string(base::span<const uint8_t> bytes) {
  return std::string_view(reinterpret_cast<const char*>(bytes.data()),
                          bytes.size());
}

std::string_view as_string(const std::vector<uint8_t>& bytes) {
  return as_string(base::make_span(bytes));
}

// If RequestManyTokensAndRetainOneArbitrarily doesn't throw a fatal failure,
// then issuance succeeded and |out_token|'s now contains a token
// suitable for performing a redemption (with the same issuer keys).
//
// The issuer-side logic will use |key_with_which_to_issue| to issue the token;
// this must be a reference to a member of |keys.token_keys|.
//
// Note: This method returns void so that it's possible to ASSERT inside it.
void RequestManyTokensAndRetainOneArbitrarily(
    const TestTrustTokenIssuer& issuer,
    const TestTrustTokenIssuer::VerificationKey& key_with_which_to_issue,
    TrustToken* out_token) {
  BoringsslTrustTokenIssuanceCryptographer issuance_cryptographer;
  ASSERT_TRUE(
      issuance_cryptographer.Initialize(kProtocolVersion, kNumTokensToRequest));

  for (const TestTrustTokenIssuer::VerificationKey& key : issuer.Keys()) {
    ASSERT_TRUE(issuance_cryptographer.AddKey(std::string(
        reinterpret_cast<const char*>(key.value.data()), key.value.size())));
  }

  std::optional<std::string> maybe_base64_encoded_issuance_request =
      issuance_cryptographer.BeginIssuance(kNumTokensToRequest);
  ASSERT_TRUE(maybe_base64_encoded_issuance_request);

  auto maybe_issuance_response =
      issuer.IssueUsingKey(maybe_base64_encoded_issuance_request.value(),
                           key_with_which_to_issue.id);
  ASSERT_TRUE(maybe_issuance_response.has_value());

  std::unique_ptr<
      TrustTokenRequestIssuanceHelper::Cryptographer::UnblindedTokens>
      obtained_tokens = issuance_cryptographer.ConfirmIssuance(
          maybe_issuance_response.value());

  ASSERT_TRUE(obtained_tokens);
  ASSERT_EQ(obtained_tokens->tokens.size(), kNumTokensToRequest);

  out_token->set_body(obtained_tokens->tokens.front());
  out_token->set_signing_key(obtained_tokens->body_of_verifying_key);
  ASSERT_EQ(out_token->signing_key(), as_string(key_with_which_to_issue.value));
}

// Uses a RedemptionCryptographer to construct a redemption request wrapping
// |token_to_redeem|; verifies that the server-side BoringSSL redemption code
// accepts the redemption request, and that the RedemptionCryptographer
// correctly handles the corresponding redemption response.
void RedeemSingleToken(const TestTrustTokenIssuer& issuer,
                       const TrustToken& token_to_redeem) {
  BoringsslTrustTokenRedemptionCryptographer redemption_cryptographer;

  const url::Origin kRedeemingOrigin =
      url::Origin::Create(GURL("https://topframe.example"));

  ASSERT_TRUE(redemption_cryptographer.Initialize(kProtocolVersion,
                                                  kNumTokensToRequest));

  std::optional<std::string> maybe_base64_encoded_redemption_request =
      redemption_cryptographer.BeginRedemption(token_to_redeem,
                                               kRedeemingOrigin);

  ASSERT_TRUE(maybe_base64_encoded_redemption_request);

  bssl::UniquePtr<TRUST_TOKEN> redeemed_token_scoper =
      issuer.Redeem(maybe_base64_encoded_redemption_request.value());
  ASSERT_TRUE(redeemed_token_scoper);
}

}  // namespace

TEST(TrustTokenCryptographersTest, IssuanceAndRedemption) {
  TestTrustTokenIssuer issuer(/*num_keys=*/1,
                              /*max_issuance=*/kNumTokensToRequest);

  TrustToken token;
  ASSERT_NO_FATAL_FAILURE(RequestManyTokensAndRetainOneArbitrarily(
      issuer, issuer.Keys().at(0), &token));

  ASSERT_NO_FATAL_FAILURE(RedeemSingleToken(issuer, token));
}

TEST(TrustTokenCryptographersTest, IssuanceAndRedemptionWithMultipleKeys) {
  TestTrustTokenIssuer issuer(/*num_keys=*/3,
                              /*max_issuance=*/kNumTokensToRequest);

  TrustToken token;
  ASSERT_NO_FATAL_FAILURE(RequestManyTokensAndRetainOneArbitrarily(
      issuer, issuer.Keys().at(0), &token));

  TrustToken another_token;
  ASSERT_NO_FATAL_FAILURE(RequestManyTokensAndRetainOneArbitrarily(
      issuer, issuer.Keys().at(2), &another_token));

  // In both cases, redeeming a token from the issuance should succeed.
  ASSERT_NO_FATAL_FAILURE(RedeemSingleToken(issuer, token));
  ASSERT_NO_FATAL_FAILURE(RedeemSingleToken(issuer, another_token));
}

}  // namespace network
