// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/ec_private_key.h"
#include "services/network/trust_tokens/ecdsa_sha256_trust_token_request_signer.h"
#include "services/network/trust_tokens/ed25519_trust_token_request_signer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace network {

namespace {

struct Keys {
  std::vector<uint8_t> signing;
  std::vector<uint8_t> verification;
};

const char kLongishMessage[] =
    "Four score and seven years ago our fathers brought forth on this "
    "continent, a new nation, conceived in Liberty, and dedicated to the "
    "proposition that all men are created equal.";

enum class RequestSigner {
  kEd25519 = 0,
  kEcdsaSha256 = 1,
};

}  // namespace

class TrustTokenRequestSigner : public ::testing::TestWithParam<RequestSigner> {
 protected:
  std::unique_ptr<TrustTokenRequestSigningHelper::Signer> CreateSigner() {
    switch (GetParam()) {
      case RequestSigner::kEd25519:
        return std::make_unique<Ed25519TrustTokenRequestSigner>();

      case RequestSigner::kEcdsaSha256:
        return std::make_unique<EcdsaSha256TrustTokenRequestSigner>();
    }
  }

  // The fixed constant 32 comes from curve25519.h and is not defined in a
  // macro.
  Keys GetTestKeys() {
    Keys ret;
    switch (GetParam()) {
      case RequestSigner::kEd25519: {
        ret.signing.resize(ED25519_PRIVATE_KEY_LEN);
        ret.verification.resize(ED25519_PUBLIC_KEY_LEN);
        // Cannot fail.
        std::array<uint8_t, 32> seed{1, 2, 3, 4, 5};
        ED25519_keypair_from_seed(ret.verification.data(), ret.signing.data(),
                                  seed.data());
        break;
      }
      case RequestSigner::kEcdsaSha256:
        const std::vector<uint8_t> private_key = {
            0x30, 0x81, 0x87, 0x02, 0x01, 0x00, 0x30, 0x13, 0x06, 0x07, 0x2a,
            0x86, 0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a, 0x86, 0x48,
            0xce, 0x3d, 0x03, 0x01, 0x07, 0x04, 0x6d, 0x30, 0x6b, 0x02, 0x01,
            0x01, 0x04, 0x20, 0x7f, 0x4c, 0x85, 0x5d, 0xcb, 0xd5, 0x3e, 0x9e,
            0xed, 0x0a, 0x34, 0xc9, 0xbf, 0xbc, 0xfb, 0x0e, 0xcd, 0xd8, 0xa0,
            0x89, 0x7e, 0x1d, 0xaf, 0x1c, 0x1e, 0x9f, 0x8c, 0x9f, 0xac, 0x21,
            0xee, 0xa5, 0xa1, 0x44, 0x03, 0x42, 0x00, 0x04, 0x62, 0x22, 0x44,
            0x4b, 0x41, 0x0e, 0x16, 0xcc, 0x6e, 0xbb, 0x72, 0xb9, 0xe5, 0x70,
            0xba, 0x13, 0xd0, 0xd2, 0x1f, 0x8f, 0x2a, 0x10, 0x57, 0x32, 0x77,
            0xb8, 0xd0, 0x62, 0x7e, 0x4d, 0x18, 0x6d, 0xc2, 0x87, 0x25, 0x17,
            0x45, 0x11, 0x82, 0xf2, 0x93, 0xed, 0xd5, 0x60, 0x7f, 0xae, 0x67,
            0x87, 0x39, 0x15, 0x90, 0x16, 0x91, 0x3c, 0xf9, 0x11, 0x76, 0x09,
            0xfa, 0x51, 0x90, 0xa4, 0x2f, 0x9a};
        std::unique_ptr<crypto::ECPrivateKey> key =
            crypto::ECPrivateKey::CreateFromPrivateKeyInfo(private_key);
        std::vector<uint8_t> ec_private_key;
        key->ExportPrivateKey(&ec_private_key);
        ret.signing.swap(ec_private_key);

        std::string public_key;
        key->ExportRawPublicKey(&public_key);
        ret.verification =
            std::vector<uint8_t>(public_key.begin(), public_key.end());
        std::string raw_public_key;
        key->ExportRawPublicKey(&raw_public_key);
        break;
    }
    return ret;
  }
};  // namespace network

TEST_P(TrustTokenRequestSigner, Roundtrip) {
  auto message = base::as_bytes(base::make_span(kLongishMessage));

  Keys keys = GetTestKeys();

  std::unique_ptr<TrustTokenRequestSigningHelper::Signer> signer =
      CreateSigner();

  std::optional<std::vector<uint8_t>> signature =
      signer->Sign(keys.signing, message);
  ASSERT_TRUE(signature);

  EXPECT_TRUE(signer->Verify(message, *signature, keys.verification));
}

TEST_P(TrustTokenRequestSigner, EmptyMessage) {
  auto message = base::span<const uint8_t>();

  Keys keys = GetTestKeys();

  std::unique_ptr<TrustTokenRequestSigningHelper::Signer> signer =
      CreateSigner();

  std::optional<std::vector<uint8_t>> signature =
      signer->Sign(keys.signing, message);
  ASSERT_TRUE(signature);

  EXPECT_TRUE(signer->Verify(message, *signature, keys.verification));
}

TEST_P(TrustTokenRequestSigner, ShortMessage) {
  auto message = base::as_bytes(base::make_span("Hello"));

  Keys keys = GetTestKeys();

  std::unique_ptr<TrustTokenRequestSigningHelper::Signer> signer =
      CreateSigner();

  std::optional<std::vector<uint8_t>> signature =
      signer->Sign(keys.signing, message);
  ASSERT_TRUE(signature);

  EXPECT_TRUE(signer->Verify(message, *signature, keys.verification));
}

TEST_P(TrustTokenRequestSigner, LongerMessage) {
  std::vector<uint8_t> message(1000000);
  Keys keys = GetTestKeys();

  std::unique_ptr<TrustTokenRequestSigningHelper::Signer> signer =
      CreateSigner();

  std::optional<std::vector<uint8_t>> signature =
      signer->Sign(keys.signing, message);
  ASSERT_TRUE(signature);

  EXPECT_TRUE(signer->Verify(message, *signature, keys.verification));
}

TEST_P(TrustTokenRequestSigner, VerificationFromDifferentSigner) {
  // Test that Verify works without prior initialization and signing, as its
  // contract promises.
  auto message = base::as_bytes(base::make_span(kLongishMessage));

  Keys keys = GetTestKeys();

  std::unique_ptr<TrustTokenRequestSigningHelper::Signer> signer =
      CreateSigner();

  std::optional<std::vector<uint8_t>> signature =
      signer->Sign(keys.signing, message);

  std::unique_ptr<TrustTokenRequestSigningHelper::Signer> verifier =
      CreateSigner();
  EXPECT_TRUE(verifier->Verify(message, *signature, keys.verification));
}

TEST_P(TrustTokenRequestSigner, SigningKeyTooShort) {
  auto message = base::as_bytes(base::make_span(kLongishMessage));

  Keys keys = GetTestKeys();

  std::unique_ptr<TrustTokenRequestSigningHelper::Signer> signer =
      CreateSigner();

  std::optional<std::vector<uint8_t>> signature =
      signer->Sign(base::make_span(keys.signing).subspan(1), message);
  EXPECT_FALSE(signature);
}

TEST_P(TrustTokenRequestSigner, SigningKeyTooLong) {
  auto message = base::as_bytes(base::make_span(kLongishMessage));

  std::unique_ptr<TrustTokenRequestSigningHelper::Signer> signer =
      CreateSigner();

  Keys keys = GetTestKeys();
  std::vector<uint8_t> overlong_signing_key(keys.signing.size() + 1);

  std::optional<std::vector<uint8_t>> signature =
      signer->Sign(overlong_signing_key, message);
  EXPECT_FALSE(signature);
}

TEST_P(TrustTokenRequestSigner, VerificationKeyTooShort) {
  auto message = base::as_bytes(base::make_span(kLongishMessage));

  Keys keys = GetTestKeys();

  std::unique_ptr<TrustTokenRequestSigningHelper::Signer> signer =
      CreateSigner();

  std::optional<std::vector<uint8_t>> signature =
      signer->Sign(keys.signing, message);

  EXPECT_FALSE(signer->Verify(message, *signature,
                              base::make_span(keys.verification).subspan(1)));
}

TEST_P(TrustTokenRequestSigner, VerificationKeyTooLong) {
  auto message = base::as_bytes(base::make_span(kLongishMessage));

  Keys keys = GetTestKeys();

  std::unique_ptr<TrustTokenRequestSigningHelper::Signer> signer =
      CreateSigner();

  std::optional<std::vector<uint8_t>> signature =
      signer->Sign(keys.signing, message);

  std::vector<uint8_t> overlong_verification_key(keys.verification.size() + 1);

  EXPECT_FALSE(signer->Verify(message, *signature, overlong_verification_key));
}

TEST_P(TrustTokenRequestSigner, SignatureTooShort) {
  auto message = base::as_bytes(base::make_span(kLongishMessage));

  Keys keys = GetTestKeys();

  std::unique_ptr<TrustTokenRequestSigningHelper::Signer> signer =
      CreateSigner();

  std::optional<std::vector<uint8_t>> signature =
      signer->Sign(keys.signing, message);
  signature->pop_back();

  EXPECT_FALSE(signer->Verify(message, *signature, keys.verification));
}

TEST_P(TrustTokenRequestSigner, SignatureTooLong) {
  auto message = base::as_bytes(base::make_span(kLongishMessage));

  Keys keys = GetTestKeys();

  std::unique_ptr<TrustTokenRequestSigningHelper::Signer> signer =
      CreateSigner();

  std::optional<std::vector<uint8_t>> signature =
      signer->Sign(keys.signing, message);
  signature->push_back(0);

  EXPECT_FALSE(
      signer->Verify(message, base::make_span(*signature), keys.verification));
}

TEST_P(TrustTokenRequestSigner, SignatureWrong) {
  auto message = base::as_bytes(base::make_span(kLongishMessage));

  Keys keys = GetTestKeys();

  std::unique_ptr<TrustTokenRequestSigningHelper::Signer> signer =
      CreateSigner();

  std::optional<std::vector<uint8_t>> signature =
      signer->Sign(keys.signing, message);

  // Corrupt the signature.
  signature->front() += 1;

  EXPECT_FALSE(signer->Verify(message, *signature, keys.verification));
}

INSTANTIATE_TEST_SUITE_P(
    Algorithm,
    TrustTokenRequestSigner,
    ::testing::Values(RequestSigner::kEd25519, RequestSigner::kEcdsaSha256),
    [](const testing::TestParamInfo<TrustTokenRequestSigner::ParamType>& info) {
      return info.param == RequestSigner::kEd25519 ? "ED25519" : "ECDSA_SHA256";
    });

}  // namespace network
