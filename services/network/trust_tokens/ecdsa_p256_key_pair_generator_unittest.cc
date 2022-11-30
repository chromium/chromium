// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/ecdsa_p256_key_pair_generator.h"
#include "base/containers/span.h"
#include "services/network/trust_tokens/ecdsa_sha256_trust_token_request_signer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TEST(EcdsaP256KeyPairGenerator, Roundtrip) {
  auto message = base::as_bytes(base::make_span(
      "Four score and seven years ago our fathers brought forth on this "
      "continent, a new nation, conceived in Liberty, and dedicated to the "
      "proposition that all men are created equal."));

  std::string signing, verification;
  ASSERT_TRUE(EcdsaP256KeyPairGenerator().Generate(&signing, &verification));

  EcdsaSha256TrustTokenRequestSigner signer;

  absl::optional<std::vector<uint8_t>> signature =
      signer.Sign(base::as_bytes(base::make_span(signing)), message);
  ASSERT_TRUE(signature);

  EXPECT_TRUE(signer.Verify(message, *signature,
                            base::as_bytes(base::make_span(verification))));
}

}  // namespace network
