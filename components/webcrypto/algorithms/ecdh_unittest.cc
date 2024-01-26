// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/ranges/algorithm.h"
#include "components/webcrypto/algorithm_dispatch.h"
#include "components/webcrypto/algorithms/ec.h"
#include "components/webcrypto/algorithms/test_helpers.h"
#include "components/webcrypto/jwk.h"
#include "components/webcrypto/status.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"

namespace webcrypto {

namespace {

// TODO(eroman): Test passing an RSA public key instead of ECDH key.
// TODO(eroman): Test passing an ECDSA public key

blink::WebCryptoAlgorithm CreateEcdhImportAlgorithm(
    blink::WebCryptoNamedCurve named_curve) {
  return CreateEcImportAlgorithm(blink::kWebCryptoAlgorithmIdEcdh, named_curve);
}

blink::WebCryptoAlgorithm CreateEcdhDeriveParams(
    const blink::WebCryptoKey& public_key) {
  return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
      blink::kWebCryptoAlgorithmIdEcdh,
      new blink::WebCryptoEcdhKeyDeriveParams(public_key));
}

blink::WebCryptoAlgorithm CreateAesGcmDerivedKeyParams(uint16_t length_bits) {
  return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
      blink::kWebCryptoAlgorithmIdAesGcm,
      new blink::WebCryptoAesDerivedKeyParams(length_bits));
}

struct KeyPair {
  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;
};

// Helper that loads a "public_key" and "private_key" from the test data.
KeyPair ImportKeysFromTest(const base::Value::Dict& test) {
  KeyPair result;

  // Import the public key.
  {
    const base::Value::Dict* public_key_jwk = test.FindDict("public_key");
    auto curve = CurveNameToCurve(*public_key_jwk->FindString("crv"));
    Status status = ImportKey(
        blink::kWebCryptoKeyFormatJwk, MakeJsonVector(*public_key_jwk),
        CreateEcdhImportAlgorithm(curve), true, 0, &result.public_key);
    CHECK(status.IsSuccess());
  }

  // Import the private key.
  {
    const base::Value::Dict* private_key_jwk = test.FindDict("private_key");
    auto curve = CurveNameToCurve(*private_key_jwk->FindString("crv"));
    Status status = ImportKey(blink::kWebCryptoKeyFormatJwk,
                              MakeJsonVector(*private_key_jwk),
                              CreateEcdhImportAlgorithm(curve), true,
                              blink::kWebCryptoKeyUsageDeriveBits |
                                  blink::kWebCryptoKeyUsageDeriveKey,
                              &result.private_key);
    CHECK(status.IsSuccess());
  }
  return result;
}

class WebCryptoEcdhTest : public WebCryptoTestBase {};

TEST_F(WebCryptoEcdhTest, DeriveBitsKnownAnswer) {
  base::Value::List tests = ReadJsonTestFileAsList("ecdh.json");

  for (const base::Value& test_value : tests) {
    SCOPED_TRACE(&test_value - &tests[0]);
    const base::Value::Dict& test = test_value.GetDict();

    // Import the keys.
    KeyPair keys = ImportKeysFromTest(test);

    // Now try to derive bytes.
    std::vector<uint8_t> derived_bytes;
    std::optional<int> length_bits = test.FindInt("length_bits");
    ASSERT_TRUE(length_bits);

    // If the test didn't specify an error, that implies it expects success.
    std::string expected_error = "Success";
    if (auto* r = test.FindString("error")) {
      expected_error = *r;
    }

    Status status = DeriveBits(CreateEcdhDeriveParams(keys.public_key),
                               keys.private_key, *length_bits, &derived_bytes);
    ASSERT_EQ(expected_error, StatusToString(status));
    if (status.IsError())
      continue;

    const auto expected_bytes =
        HexStringToBytes(*test.FindString("derived_bytes"));

    EXPECT_EQ(expected_bytes, derived_bytes);
  }
}

// Loads up a test ECDH public and private key for P-521. The keys
// come from different key pairs, and can be used for key derivation of up to
// 528 bits.
KeyPair LoadTestKeys() {
  base::Value::List tests = ReadJsonTestFileAsList("ecdh.json");
  const auto& test = base::ranges::find_if(tests, [](const base::Value& v) {
    return v.GetDict().FindBool("valid_p521_keys").has_value();
  });

  CHECK(test != tests.end()) << "test key set contains no valid P-521 keys";

  KeyPair keys = ImportKeysFromTest(test->GetDict());

  CHECK_EQ(blink::kWebCryptoNamedCurveP521,
           keys.public_key.Algorithm().EcParams()->NamedCurve())
      << "alleged P-521 key is not P-521";

  return keys;
}

// Try deriving an AES key of length 129 bits.
TEST_F(WebCryptoEcdhTest, DeriveKeyBadAesLength) {
  KeyPair keys = LoadTestKeys();

  blink::WebCryptoKey derived_key;

  ASSERT_EQ(Status::ErrorGetAesKeyLength(),
            DeriveKey(CreateEcdhDeriveParams(keys.public_key), keys.private_key,
                      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesGcm),
                      CreateAesGcmDerivedKeyParams(129), true,
                      blink::kWebCryptoKeyUsageEncrypt, &derived_key));
}

// Try deriving an AES key of length 192 bits.
TEST_F(WebCryptoEcdhTest, DeriveKeyUnsupportedAesLength) {
  KeyPair keys = LoadTestKeys();

  blink::WebCryptoKey derived_key;

  ASSERT_EQ(Status::ErrorAes192BitUnsupported(),
            DeriveKey(CreateEcdhDeriveParams(keys.public_key), keys.private_key,
                      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesGcm),
                      CreateAesGcmDerivedKeyParams(192), true,
                      blink::kWebCryptoKeyUsageEncrypt, &derived_key));
}

// Try deriving an HMAC key of length 0 bits.
TEST_F(WebCryptoEcdhTest, DeriveKeyZeroLengthHmac) {
  KeyPair keys = LoadTestKeys();

  blink::WebCryptoKey derived_key;

  const blink::WebCryptoAlgorithm import_algorithm =
      CreateHmacImportAlgorithm(blink::kWebCryptoAlgorithmIdSha1, 0);

  ASSERT_EQ(Status::ErrorGetHmacKeyLengthZero(),
            DeriveKey(CreateEcdhDeriveParams(keys.public_key), keys.private_key,
                      import_algorithm, import_algorithm, true,
                      blink::kWebCryptoKeyUsageSign, &derived_key));
}

// Derive an HMAC key of length 19 bits.
TEST_F(WebCryptoEcdhTest, DeriveKeyHmac19Bits) {
  KeyPair keys = LoadTestKeys();

  blink::WebCryptoKey derived_key;

  const blink::WebCryptoAlgorithm import_algorithm =
      CreateHmacImportAlgorithm(blink::kWebCryptoAlgorithmIdSha1, 19);

  ASSERT_EQ(Status::Success(),
            DeriveKey(CreateEcdhDeriveParams(keys.public_key), keys.private_key,
                      import_algorithm, import_algorithm, true,
                      blink::kWebCryptoKeyUsageSign, &derived_key));

  ASSERT_EQ(blink::kWebCryptoAlgorithmIdHmac, derived_key.Algorithm().Id());
  ASSERT_EQ(blink::kWebCryptoAlgorithmIdSha1,
            derived_key.Algorithm().HmacParams()->GetHash().Id());
  ASSERT_EQ(19u, derived_key.Algorithm().HmacParams()->LengthBits());

  // Export the key and verify its contents.
  std::vector<uint8_t> raw_key;
  EXPECT_EQ(Status::Success(),
            ExportKey(blink::kWebCryptoKeyFormatRaw, derived_key, &raw_key));
  EXPECT_EQ(3u, raw_key.size());
  // The last 7 bits of the key should be zero.
  EXPECT_EQ(0, raw_key.back() & 0x1f);
}

// Derive an HMAC key with no specified length (just the hash of SHA-256).
TEST_F(WebCryptoEcdhTest, DeriveKeyHmacSha256NoLength) {
  KeyPair keys = LoadTestKeys();

  blink::WebCryptoKey derived_key;

  const blink::WebCryptoAlgorithm import_algorithm =
      CreateHmacImportAlgorithmNoLength(blink::kWebCryptoAlgorithmIdSha256);

  ASSERT_EQ(Status::Success(),
            DeriveKey(CreateEcdhDeriveParams(keys.public_key), keys.private_key,
                      import_algorithm, import_algorithm, true,
                      blink::kWebCryptoKeyUsageSign, &derived_key));

  ASSERT_EQ(blink::kWebCryptoAlgorithmIdHmac, derived_key.Algorithm().Id());
  ASSERT_EQ(blink::kWebCryptoAlgorithmIdSha256,
            derived_key.Algorithm().HmacParams()->GetHash().Id());
  ASSERT_EQ(512u, derived_key.Algorithm().HmacParams()->LengthBits());

  // Export the key and verify its contents.
  std::vector<uint8_t> raw_key;
  EXPECT_EQ(Status::Success(),
            ExportKey(blink::kWebCryptoKeyFormatRaw, derived_key, &raw_key));
  EXPECT_EQ(64u, raw_key.size());
}

// Derive an HMAC key with no specified length (just the hash of SHA-512).
//
// This fails, because ECDH using P-521 can only generate 528 bits, however HMAC
// SHA-512 requires 1024 bits.
//
// In practice, authors won't be directly generating keys from key agreement
// schemes, as that is frequently insecure, and instead be using KDFs to expand
// and generate keys. For simplicity of testing, however, test using an HMAC
// key.
TEST_F(WebCryptoEcdhTest, DeriveKeyHmacSha512NoLength) {
  KeyPair keys = LoadTestKeys();

  blink::WebCryptoKey derived_key;

  const blink::WebCryptoAlgorithm import_algorithm =
      CreateHmacImportAlgorithmNoLength(blink::kWebCryptoAlgorithmIdSha512);

  ASSERT_EQ(Status::ErrorEcdhLengthTooBig(528),
            DeriveKey(CreateEcdhDeriveParams(keys.public_key), keys.private_key,
                      import_algorithm, import_algorithm, true,
                      blink::kWebCryptoKeyUsageSign, &derived_key));
}

// Try deriving an AES key of length 128 bits.
TEST_F(WebCryptoEcdhTest, DeriveKeyAes128) {
  KeyPair keys = LoadTestKeys();

  blink::WebCryptoKey derived_key;

  ASSERT_EQ(Status::Success(),
            DeriveKey(CreateEcdhDeriveParams(keys.public_key), keys.private_key,
                      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesGcm),
                      CreateAesGcmDerivedKeyParams(128), true,
                      blink::kWebCryptoKeyUsageEncrypt, &derived_key));

  ASSERT_EQ(blink::kWebCryptoAlgorithmIdAesGcm, derived_key.Algorithm().Id());
  ASSERT_EQ(128, derived_key.Algorithm().AesParams()->LengthBits());

  // Export the key and verify its contents.
  std::vector<uint8_t> raw_key;
  EXPECT_EQ(Status::Success(),
            ExportKey(blink::kWebCryptoKeyFormatRaw, derived_key, &raw_key));
  EXPECT_EQ(16u, raw_key.size());
}

TEST_F(WebCryptoEcdhTest, ImportKeyEmptyUsage) {
  blink::WebCryptoKey key;

  base::Value::List tests = ReadJsonTestFileAsList("ecdh.json");
  const base::Value::Dict& test = tests[0].GetDict();

  // Import the public key.
  {
    const base::Value::Dict* public_key_jwk = test.FindDict("public_key");
    auto curve = CurveNameToCurve(*public_key_jwk->FindString("crv"));
    Status status = ImportKey(blink::kWebCryptoKeyFormatJwk,
                              MakeJsonVector(*public_key_jwk),
                              CreateEcdhImportAlgorithm(curve), true, 0, &key);
    ASSERT_TRUE(status.IsSuccess());
    EXPECT_EQ(0, key.Usages());
  }

  // Import the private key.
  {
    const base::Value::Dict* private_key_jwk = test.FindDict("private_key");
    auto curve = CurveNameToCurve(*private_key_jwk->FindString("crv"));
    Status status = ImportKey(blink::kWebCryptoKeyFormatJwk,
                              MakeJsonVector(*private_key_jwk),
                              CreateEcdhImportAlgorithm(curve), true, 0, &key);
    ASSERT_EQ(Status::ErrorCreateKeyEmptyUsages(), status);
  }
}

}  // namespace

}  // namespace webcrypto
