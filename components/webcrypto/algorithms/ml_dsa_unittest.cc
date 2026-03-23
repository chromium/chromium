// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/algorithms/ml_dsa.h"

#include <sstream>

#include "components/webcrypto/algorithm_dispatch.h"
#include "components/webcrypto/algorithms/test_helpers.h"
#include "components/webcrypto/status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"

namespace webcrypto {

namespace {

blink::WebCryptoAlgorithm CreateMlDsaAlgorithm(blink::WebCryptoAlgorithmId id) {
  return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(id, nullptr);
}

blink::WebCryptoAlgorithm CreateMlDsaAlgorithmWithContext(
    blink::WebCryptoAlgorithmId id,
    const std::vector<uint8_t>& context) {
  return blink::WebCryptoAlgorithm(
      id, std::make_unique<blink::WebCryptoContextParams>(context));
}

class WebCryptoMlDsaTest
    : public WebCryptoTestBase,
      public ::testing::WithParamInterface<blink::WebCryptoAlgorithmId> {
 public:
  blink::WebCryptoAlgorithmId GetAlgorithm() { return GetParam(); }
};

TEST_P(WebCryptoMlDsaTest, GenerateKeyMlDsa) {
  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;
  ASSERT_EQ(Status::Success(),
            GenerateKeyPair(
                CreateMlDsaAlgorithm(GetAlgorithm()), true,
                blink::kWebCryptoKeyUsageSign | blink::kWebCryptoKeyUsageVerify,
                &public_key, &private_key));
  EXPECT_EQ(blink::kWebCryptoKeyTypePublic, public_key.GetType());
  EXPECT_EQ(blink::kWebCryptoKeyTypePrivate, private_key.GetType());
  EXPECT_EQ(GetAlgorithm(), public_key.Algorithm().Id());
}

TEST_P(WebCryptoMlDsaTest, SignVerifyMlDsa) {
  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;
  blink::WebCryptoAlgorithm algorithm = CreateMlDsaAlgorithm(GetAlgorithm());
  ASSERT_EQ(Status::Success(),
            GenerateKeyPair(
                algorithm, true,
                blink::kWebCryptoKeyUsageSign | blink::kWebCryptoKeyUsageVerify,
                &public_key, &private_key));

  std::vector<uint8_t> message = {1, 2, 3, 4};
  std::vector<uint8_t> signature;
  ASSERT_EQ(Status::Success(),
            Sign(algorithm, private_key, message, &signature));
  EXPECT_FALSE(signature.empty());

  bool signature_matches = false;
  ASSERT_EQ(Status::Success(), Verify(algorithm, public_key, signature, message,
                                      &signature_matches));
  EXPECT_TRUE(signature_matches);

  // Verification should fail if message is different.
  std::vector<uint8_t> wrong_message = {1, 2, 3, 5};
  ASSERT_EQ(Status::Success(), Verify(algorithm, public_key, signature,
                                      wrong_message, &signature_matches));
  EXPECT_FALSE(signature_matches);
}

TEST_P(WebCryptoMlDsaTest, SignVerifyMlDsaWithContext) {
  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;
  auto context_bytes = HexStringToBytes("deadbeef");
  blink::WebCryptoAlgorithm algorithm = CreateMlDsaAlgorithm(GetAlgorithm());

  ASSERT_EQ(Status::Success(),
            GenerateKeyPair(
                algorithm, true,
                blink::kWebCryptoKeyUsageSign | blink::kWebCryptoKeyUsageVerify,
                &public_key, &private_key));

  std::vector<uint8_t> message = {1, 2, 3, 4};
  std::vector<uint8_t> signature;
  ASSERT_EQ(Status::Success(),
            Sign(CreateMlDsaAlgorithmWithContext(GetAlgorithm(), context_bytes),
                 private_key, message, &signature));
  EXPECT_FALSE(signature.empty());

  bool signature_matches = false;
  ASSERT_EQ(
      Status::Success(),
      Verify(CreateMlDsaAlgorithmWithContext(GetAlgorithm(), context_bytes),

             public_key, signature, message, &signature_matches));
  EXPECT_TRUE(signature_matches);

  // Verification should fail if context is different
  ASSERT_EQ(Status::Success(), Verify(algorithm, public_key, signature, message,
                                      &signature_matches));
  EXPECT_FALSE(signature_matches);
}

TEST_P(WebCryptoMlDsaTest, ImportExportJwkMlDsa) {
  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;
  blink::WebCryptoAlgorithm algorithm = CreateMlDsaAlgorithm(GetAlgorithm());
  ASSERT_EQ(Status::Success(),
            GenerateKeyPair(
                algorithm, true,
                blink::kWebCryptoKeyUsageSign | blink::kWebCryptoKeyUsageVerify,
                &public_key, &private_key));

  std::vector<uint8_t> jwk_public;
  ASSERT_EQ(Status::Success(),
            ExportKey(blink::kWebCryptoKeyFormatJwk, public_key, &jwk_public));

  blink::WebCryptoKey imported_public_key;
  ASSERT_EQ(
      Status::Success(),
      ImportKey(blink::kWebCryptoKeyFormatJwk, jwk_public, algorithm, true,
                blink::kWebCryptoKeyUsageVerify, &imported_public_key));
  EXPECT_EQ(blink::kWebCryptoKeyTypePublic, imported_public_key.GetType());

  std::vector<uint8_t> jwk_private;
  ASSERT_EQ(Status::Success(), ExportKey(blink::kWebCryptoKeyFormatJwk,
                                         private_key, &jwk_private));

  blink::WebCryptoKey imported_private_key;
  ASSERT_EQ(
      Status::Success(),
      ImportKey(blink::kWebCryptoKeyFormatJwk, jwk_private, algorithm, true,
                blink::kWebCryptoKeyUsageSign, &imported_private_key));
  EXPECT_EQ(blink::kWebCryptoKeyTypePrivate, imported_private_key.GetType());
}

INSTANTIATE_TEST_SUITE_P(
    MlDsa,
    WebCryptoMlDsaTest,
    testing::Values(blink::kWebCryptoAlgorithmIdMlDsa44,
                    blink::kWebCryptoAlgorithmIdMlDsa65,
                    blink::kWebCryptoAlgorithmIdMlDsa87),
    [](const testing::TestParamInfo<blink::WebCryptoAlgorithmId>& info) {
      std::ostringstream oss;
      switch (info.param) {
        case blink::kWebCryptoAlgorithmIdMlDsa44:
          oss << "44";
          break;
        case blink::kWebCryptoAlgorithmIdMlDsa65:
          oss << "65";
          break;
        case blink::kWebCryptoAlgorithmIdMlDsa87:
          oss << "87";
          break;
        default:
          NOTREACHED();
      }
      return oss.str();
    });

}  // namespace

}  // namespace webcrypto
