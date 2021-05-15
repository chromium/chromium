// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/ed25519_trust_token_request_signer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace network {

namespace {

struct Keys {
  std::array<uint8_t, ED25519_PRIVATE_KEY_LEN> signing;
  std::array<uint8_t, ED25519_PUBLIC_KEY_LEN> verification;
};

// The fixed constant 32 comes from curve25519.h and is not defined in a macro.
Keys KeysFromSeed(base::span<const uint8_t, 32> seed) {
  Keys ret;

  // Cannot fail.
  ED25519_keypair_from_seed(ret.verification.data(), ret.signing.data(),
                            seed.data());

  return ret;
}

const char kLongishMessage[] =
    "Four score and seven years ago our fathers brought forth on this "
    "continent, a new nation, conceived in Liberty, and dedicated to the "
    "proposition that all men are created equal.";

}  // namespace

TEST(Ed25519TrustTokenRequestSigner, Roundtrip) {
  auto message = base::as_bytes(base::make_span(kLongishMessage));

  std::array<uint8_t, 32> seed{1, 2, 3, 4, 5};
  Keys keys = KeysFromSeed(seed);

  Ed25519TrustTokenRequestSigner signer;

  absl::optional<std::vector<uint8_t>> signature =
      signer.Sign(keys.signing, message);
  ASSERT_TRUE(signature);

  EXPECT_TRUE(signer.Verify(message, *signature, keys.verification));
}

TEST(Ed25519TrustTokenRequestSigner, EmptyMessage) {
  auto message = base::span<const uint8_t>();

  std::array<uint8_t, 32> seed{1, 2, 3, 4, 5};
  Keys keys = KeysFromSeed(seed);

  Ed25519TrustTokenRequestSigner signer;

  absl::optional<std::vector<uint8_t>> signature =
      signer.Sign(keys.signing, message);
  ASSERT_TRUE(signature);

  EXPECT_TRUE(signer.Verify(message, *signature, keys.verification));
}

TEST(Ed25519TrustTokenRequestSigner, ShortMessage) {
  auto message = base::as_bytes(base::make_span("Hello"));

  std::array<uint8_t, 32> seed{1, 2, 3, 4, 5};
  Keys keys = KeysFromSeed(seed);

  Ed25519TrustTokenRequestSigner signer;

  absl::optional<std::vector<uint8_t>> signature =
      signer.Sign(keys.signing, message);
  ASSERT_TRUE(signature);

  EXPECT_TRUE(signer.Verify(message, *signature, keys.verification));
}

TEST(Ed25519TrustTokenRequestSigner, LongerMessage) {
  std::vector<uint8_t> message(1000000);
  std::array<uint8_t, 32> seed{1, 2, 3, 4, 5};
  Keys keys = KeysFromSeed(seed);

  Ed25519TrustTokenRequestSigner signer;

  absl::optional<std::vector<uint8_t>> signature =
      signer.Sign(keys.signing, message);
  ASSERT_TRUE(signature);

  EXPECT_TRUE(signer.Verify(message, *signature, keys.verification));
}

TEST(Ed25519TrustTokenRequestSigner, VerificationFromDifferentSigner) {
  // Test that Verify works without prior initialization and signing, as its
  // contract promises.
  auto message = base::as_bytes(base::make_span(kLongishMessage));

  std::array<uint8_t, 32> seed{1, 2, 3, 4, 5};
  Keys keys = KeysFromSeed(seed);

  Ed25519TrustTokenRequestSigner signer;

  absl::optional<std::vector<uint8_t>> signature =
      signer.Sign(keys.signing, message);

  Ed25519TrustTokenRequestSigner verifier;
  EXPECT_TRUE(verifier.Verify(message, *signature, keys.verification));
}

TEST(Ed25519TrustTokenRequestSigner, SigningKeyTooShort) {
  auto message = base::as_bytes(base::make_span(kLongishMessage));

  std::array<uint8_t, 32> seed{1, 2, 3, 4, 5};
  Keys keys = KeysFromSeed(seed);

  Ed25519TrustTokenRequestSigner signer;

  absl::optional<std::vector<uint8_t>> signature =
      signer.Sign(base::make_span(keys.signing).subspan(1), message);
  EXPECT_FALSE(signature);
}

TEST(Ed25519TrustTokenRequestSigner, SigningKeyTooLong) {
  auto message = base::as_bytes(base::make_span(kLongishMessage));

  Ed25519TrustTokenRequestSigner signer;

  std::vector<uint8_t> overlong_signing_key(ED25519_PRIVATE_KEY_LEN + 1);

  absl::optional<std::vector<uint8_t>> signature =
      signer.Sign(overlong_signing_key, message);
  EXPECT_FALSE(signature);
}

TEST(Ed25519TrustTokenRequestSigner, VerificationKeyTooShort) {
  auto message = base::as_bytes(base::make_span(kLongishMessage));

  std::array<uint8_t, 32> seed{1, 2, 3, 4, 5};
  Keys keys = KeysFromSeed(seed);

  Ed25519TrustTokenRequestSigner signer;

  absl::optional<std::vector<uint8_t>> signature =
      signer.Sign(keys.signing, message);

  EXPECT_FALSE(signer.Verify(message, *signature,
                             base::make_span(keys.verification).subspan(1)));
}

TEST(Ed25519TrustTokenRequestSigner, VerificationKeyTooLong) {
  auto message = base::as_bytes(base::make_span(kLongishMessage));

  std::array<uint8_t, 32> seed{1, 2, 3, 4, 5};
  Keys keys = KeysFromSeed(seed);

  Ed25519TrustTokenRequestSigner signer;

  absl::optional<std::vector<uint8_t>> signature =
      signer.Sign(keys.signing, message);

  std::vector<uint8_t> overlong_verification_key(ED25519_PUBLIC_KEY_LEN + 1);

  EXPECT_FALSE(signer.Verify(message, *signature, overlong_verification_key));
}

TEST(Ed25519TrustTokenRequestSigner, SignatureTooShort) {
  auto message = base::as_bytes(base::make_span(kLongishMessage));

  std::array<uint8_t, 32> seed{1, 2, 3, 4, 5};
  Keys keys = KeysFromSeed(seed);

  Ed25519TrustTokenRequestSigner signer;

  absl::optional<std::vector<uint8_t>> signature =
      signer.Sign(keys.signing, message);
  signature->pop_back();

  EXPECT_FALSE(signer.Verify(message, *signature, keys.verification));
}

TEST(Ed25519TrustTokenRequestSigner, SignatureTooLong) {
  auto message = base::as_bytes(base::make_span(kLongishMessage));

  std::array<uint8_t, 32> seed{1, 2, 3, 4, 5};
  Keys keys = KeysFromSeed(seed);

  Ed25519TrustTokenRequestSigner signer;

  absl::optional<std::vector<uint8_t>> signature =
      signer.Sign(keys.signing, message);
  signature->push_back(0);

  EXPECT_FALSE(
      signer.Verify(message, base::make_span(*signature), keys.verification));
}

TEST(Ed25519TrustTokenRequestSigner, SignatureWrong) {
  auto message = base::as_bytes(base::make_span(kLongishMessage));

  std::array<uint8_t, 32> seed{1, 2, 3, 4, 5};
  Keys keys = KeysFromSeed(seed);

  Ed25519TrustTokenRequestSigner signer;

  absl::optional<std::vector<uint8_t>> signature =
      signer.Sign(keys.signing, message);

  // Corrupt the signature.
  signature->front() += 1;

  EXPECT_FALSE(signer.Verify(message, *signature, keys.verification));
}

}  // namespace network
