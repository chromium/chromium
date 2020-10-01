// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/boringssl_trust_token_issuance_cryptographer.h"

#include "base/containers/span.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/trust_token.h"

namespace network {

namespace {

std::string GenerateValidVerificationKey() {
  std::string verification(TRUST_TOKEN_MAX_PUBLIC_KEY_SIZE, 'a'),
      signing(TRUST_TOKEN_MAX_PRIVATE_KEY_SIZE, 'a');
  size_t verification_len, signing_len;
  CHECK(TRUST_TOKEN_generate_key(
      TRUST_TOKEN_experiment_v1(),
      base::as_writable_bytes(base::make_span(signing)).data(), &signing_len,
      signing.size(),
      base::as_writable_bytes(base::make_span(verification)).data(),
      &verification_len, verification.size(),
      /*id=*/0));
  verification.resize(verification_len);

  return verification;
}

}  // namespace

TEST(BoringsslTrustTokenIssuanceCryptographer, RespectsKeyLimit) {
  // Test that adding more than
  // |kMaximumConcurrentlyValidTrustTokenVerificationKeys| many keys fails. This
  // is essentially an integration test ensuring that
  // kMaximumConcurrentlyValidTrustTokenVerificationKeys is no greater than
  // BoringSSL's internally-configured maximum number of permitted keys.
  BoringsslTrustTokenIssuanceCryptographer cryptographer;
  ASSERT_TRUE(
      cryptographer.Initialize(mojom::TrustTokenProtocolVersion::kTrustTokenV1,
                               /*issuer_configured_batch_size=*/10));

  for (size_t i = 0; i < kMaximumConcurrentlyValidTrustTokenVerificationKeys;
       ++i) {
    ASSERT_TRUE(cryptographer.AddKey(GenerateValidVerificationKey())) << i;
  }
  EXPECT_FALSE(cryptographer.AddKey(GenerateValidVerificationKey()));
}

}  // namespace network
