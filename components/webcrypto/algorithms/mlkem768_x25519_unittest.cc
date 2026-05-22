// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/algorithm_dispatch.h"
#include "components/webcrypto/algorithms/test_helpers.h"
#include "components/webcrypto/encapsulate_result.h"
#include "components/webcrypto/status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"

namespace webcrypto {

namespace {

blink::WebCryptoAlgorithm CreateMlKem768X25519Algorithm() {
  return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
      blink::kWebCryptoAlgorithmIdMlKem768X25519, nullptr);
}

class WebCryptoMlKem768X25519Test : public WebCryptoTestBase {};

TEST_F(WebCryptoMlKem768X25519Test, GenerateKey) {
  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;
  ASSERT_EQ(Status::Success(),
            GenerateKeyPair(CreateMlKem768X25519Algorithm(), true,
                            blink::kWebCryptoKeyUsageEncapsulateBits |
                                blink::kWebCryptoKeyUsageDecapsulateBits,
                            &public_key, &private_key));
  EXPECT_EQ(blink::kWebCryptoKeyTypePublic, public_key.GetType());
  EXPECT_EQ(blink::kWebCryptoKeyTypePrivate, private_key.GetType());
  EXPECT_EQ(blink::kWebCryptoAlgorithmIdMlKem768X25519,
            public_key.Algorithm().Id());
  EXPECT_EQ(blink::kWebCryptoAlgorithmIdMlKem768X25519,
            private_key.Algorithm().Id());
  EXPECT_TRUE(public_key.Extractable());
  EXPECT_TRUE(private_key.Extractable());
}

TEST_F(WebCryptoMlKem768X25519Test, ImportExportRaw) {
  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;
  blink::WebCryptoAlgorithm algorithm = CreateMlKem768X25519Algorithm();
  ASSERT_EQ(Status::Success(),
            GenerateKeyPair(algorithm, true,
                            blink::kWebCryptoKeyUsageEncapsulateBits |
                                blink::kWebCryptoKeyUsageDecapsulateBits,
                            &public_key, &private_key));

  // Export public key raw
  std::vector<uint8_t> raw_public;
  ASSERT_EQ(Status::Success(), ExportKey(blink::kWebCryptoKeyFormatRawPublic,
                                         public_key, &raw_public));
  EXPECT_FALSE(raw_public.empty());

  // Import public key raw
  blink::WebCryptoKey imported_public_key;
  ASSERT_EQ(Status::Success(),
            ImportKey(blink::kWebCryptoKeyFormatRawPublic, raw_public,
                      algorithm, true, blink::kWebCryptoKeyUsageEncapsulateBits,
                      &imported_public_key));
  EXPECT_EQ(blink::kWebCryptoKeyTypePublic, imported_public_key.GetType());

  // Export private key raw seed
  std::vector<uint8_t> raw_seed;
  ASSERT_EQ(Status::Success(), ExportKey(blink::kWebCryptoKeyFormatRawSeed,
                                         private_key, &raw_seed));
  EXPECT_FALSE(raw_seed.empty());

  // Import private key raw seed
  blink::WebCryptoKey imported_private_key;
  ASSERT_EQ(Status::Success(),
            ImportKey(blink::kWebCryptoKeyFormatRawSeed, raw_seed, algorithm,
                      true, blink::kWebCryptoKeyUsageDecapsulateBits,
                      &imported_private_key));
  EXPECT_EQ(blink::kWebCryptoKeyTypePrivate, imported_private_key.GetType());
}

TEST_F(WebCryptoMlKem768X25519Test, EncapsulateDecapsulate) {
  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;
  blink::WebCryptoAlgorithm algorithm = CreateMlKem768X25519Algorithm();
  ASSERT_EQ(Status::Success(),
            GenerateKeyPair(algorithm, true,
                            blink::kWebCryptoKeyUsageEncapsulateBits |
                                blink::kWebCryptoKeyUsageDecapsulateBits,
                            &public_key, &private_key));

  // Encapsulate
  EncapsulateBitsResult encap_result;
  ASSERT_EQ(Status::Success(),
            EncapsulateBits(algorithm, public_key, &encap_result));
  EXPECT_FALSE(encap_result.shared_bits().empty());
  EXPECT_FALSE(encap_result.ciphertext().empty());

  // Decapsulate
  std::vector<uint8_t> decap_shared_bits;
  ASSERT_EQ(Status::Success(),
            DecapsulateBits(algorithm, private_key, encap_result.ciphertext(),
                            &decap_shared_bits));

  // Verify secrets match
  EXPECT_EQ(encap_result.shared_bits(), decap_shared_bits);
}

TEST_F(WebCryptoMlKem768X25519Test, CloneKey) {
  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;
  blink::WebCryptoAlgorithm algorithm = CreateMlKem768X25519Algorithm();
  ASSERT_EQ(Status::Success(),
            GenerateKeyPair(algorithm, true,
                            blink::kWebCryptoKeyUsageEncapsulateBits |
                                blink::kWebCryptoKeyUsageDecapsulateBits,
                            &public_key, &private_key));

  // Clone Public Key
  std::vector<uint8_t> serialized_public;
  ASSERT_TRUE(SerializeKeyForClone(public_key, &serialized_public));

  blink::WebCryptoKey cloned_public_key;
  ASSERT_TRUE(DeserializeKeyForClone(
      public_key.Algorithm(), public_key.GetType(), public_key.Extractable(),
      public_key.Usages(), serialized_public, &cloned_public_key));
  EXPECT_EQ(public_key.GetType(), cloned_public_key.GetType());
  EXPECT_EQ(public_key.Algorithm().Id(), cloned_public_key.Algorithm().Id());

  // Clone Private Key
  std::vector<uint8_t> serialized_private;
  ASSERT_TRUE(SerializeKeyForClone(private_key, &serialized_private));

  blink::WebCryptoKey cloned_private_key;
  ASSERT_TRUE(DeserializeKeyForClone(
      private_key.Algorithm(), private_key.GetType(), private_key.Extractable(),
      private_key.Usages(), serialized_private, &cloned_private_key));
  EXPECT_EQ(private_key.GetType(), cloned_private_key.GetType());
  EXPECT_EQ(private_key.Algorithm().Id(), cloned_private_key.Algorithm().Id());

  // Verify cloned keys work
  EncapsulateBitsResult encap_result;
  ASSERT_EQ(Status::Success(),
            EncapsulateBits(algorithm, cloned_public_key, &encap_result));

  std::vector<uint8_t> decap_shared_bits;
  ASSERT_EQ(Status::Success(),
            DecapsulateBits(algorithm, cloned_private_key,
                            encap_result.ciphertext(), &decap_shared_bits));
  EXPECT_EQ(encap_result.shared_bits(), decap_shared_bits);
}

TEST_F(WebCryptoMlKem768X25519Test, UnsupportedFormats) {
  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;
  blink::WebCryptoAlgorithm algorithm = CreateMlKem768X25519Algorithm();
  ASSERT_EQ(Status::Success(),
            GenerateKeyPair(algorithm, true,
                            blink::kWebCryptoKeyUsageEncapsulateBits |
                                blink::kWebCryptoKeyUsageDecapsulateBits,
                            &public_key, &private_key));

  std::vector<uint8_t> buffer;
  // Exporting public key to PKCS8 should fail
  EXPECT_EQ(Status::ErrorUnsupportedExportKeyFormat(),
            ExportKey(blink::kWebCryptoKeyFormatPkcs8, public_key, &buffer));
  // Exporting public key to SPKI should fail
  EXPECT_EQ(Status::ErrorUnsupportedExportKeyFormat(),
            ExportKey(blink::kWebCryptoKeyFormatSpki, public_key, &buffer));
  // Exporting public key to JWK should fail
  EXPECT_EQ(Status::ErrorUnsupportedExportKeyFormat(),
            ExportKey(blink::kWebCryptoKeyFormatJwk, public_key, &buffer));

  // Exporting private key to SPKI should fail
  EXPECT_EQ(Status::ErrorUnsupportedExportKeyFormat(),
            ExportKey(blink::kWebCryptoKeyFormatSpki, private_key, &buffer));
  // Exporting private key to PKCS8 should fail
  EXPECT_EQ(Status::ErrorUnsupportedExportKeyFormat(),
            ExportKey(blink::kWebCryptoKeyFormatPkcs8, private_key, &buffer));
  // Exporting private key to JWK should fail
  EXPECT_EQ(Status::ErrorUnsupportedExportKeyFormat(),
            ExportKey(blink::kWebCryptoKeyFormatJwk, private_key, &buffer));
}

}  // namespace

}  // namespace webcrypto
