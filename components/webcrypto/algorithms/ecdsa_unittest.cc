// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"
#include "components/webcrypto/algorithm_dispatch.h"
#include "components/webcrypto/algorithms/test_helpers.h"
#include "components/webcrypto/jwk.h"
#include "components/webcrypto/status.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"

namespace webcrypto {

namespace {

blink::WebCryptoAlgorithm CreateEcdsaKeyGenAlgorithm(
    blink::WebCryptoNamedCurve named_curve) {
  return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
      blink::kWebCryptoAlgorithmIdEcdsa,
      new blink::WebCryptoEcKeyGenParams(named_curve));
}

blink::WebCryptoAlgorithm CreateEcdsaImportAlgorithm(
    blink::WebCryptoNamedCurve named_curve) {
  return CreateEcImportAlgorithm(blink::kWebCryptoAlgorithmIdEcdsa,
                                 named_curve);
}

blink::WebCryptoAlgorithm CreateEcdsaAlgorithm(
    blink::WebCryptoAlgorithmId hash_id) {
  return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
      blink::kWebCryptoAlgorithmIdEcdsa,
      new blink::WebCryptoEcdsaParams(CreateAlgorithm(hash_id)));
}

class WebCryptoEcdsaTest : public WebCryptoTestBase {};

// Generates some ECDSA key pairs. Validates basic properties on the keys, and
// ensures the serialized key (as JWK) is unique. This test does nothing to
// ensure that the keys are otherwise usable (by trying to sign/verify with
// them).
TEST_F(WebCryptoEcdsaTest, GenerateKeyIsRandom) {
  blink::WebCryptoNamedCurve named_curve = blink::kWebCryptoNamedCurveP256;

  std::vector<std::vector<uint8_t>> serialized_keys;

  // Generate a small sample of keys.
  for (int j = 0; j < 4; ++j) {
    blink::WebCryptoKey public_key;
    blink::WebCryptoKey private_key;

    ASSERT_EQ(Status::Success(),
              GenerateKeyPair(CreateEcdsaKeyGenAlgorithm(named_curve), true,
                              blink::kWebCryptoKeyUsageSign, &public_key,
                              &private_key));

    // Basic sanity checks on the generated key pair.
    EXPECT_EQ(blink::kWebCryptoKeyTypePublic, public_key.GetType());
    EXPECT_EQ(blink::kWebCryptoKeyTypePrivate, private_key.GetType());
    EXPECT_EQ(named_curve, public_key.Algorithm().EcParams()->NamedCurve());
    EXPECT_EQ(named_curve, private_key.Algorithm().EcParams()->NamedCurve());

    // Export the key pair to JWK.
    std::vector<uint8_t> key_bytes;
    ASSERT_EQ(Status::Success(),
              ExportKey(blink::kWebCryptoKeyFormatJwk, public_key, &key_bytes));
    serialized_keys.push_back(key_bytes);

    ASSERT_EQ(Status::Success(), ExportKey(blink::kWebCryptoKeyFormatJwk,
                                           private_key, &key_bytes));
    serialized_keys.push_back(key_bytes);
  }

  // Ensure all entries in the key sample set are unique. This is a simplistic
  // estimate of whether the generated keys appear random.
  EXPECT_FALSE(CopiesExist(serialized_keys));
}

TEST_F(WebCryptoEcdsaTest, GenerateKeyEmptyUsage) {
  blink::WebCryptoNamedCurve named_curve = blink::kWebCryptoNamedCurveP256;
  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;
  ASSERT_EQ(Status::ErrorCreateKeyEmptyUsages(),
            GenerateKeyPair(CreateEcdsaKeyGenAlgorithm(named_curve), true, 0,
                            &public_key, &private_key));
}

// Verify that ECDSA signatures are probabilistic. Signing the same message two
// times should yield different signatures. However both signatures should
// verify correctly.
TEST_F(WebCryptoEcdsaTest, SignatureIsRandom) {
  // Import a public and private keypair from "ec_private_keys.json". It doesn't
  // really matter which one is used since they are all valid. In this case
  // using the first one.
  base::Value::List private_keys =
      ReadJsonTestFileAsList("ec_private_keys.json");
  const base::Value& key_value = private_keys[0];
  ASSERT_TRUE(key_value.is_dict());
  const base::Value::Dict& key_dict = key_value.GetDict();
  blink::WebCryptoNamedCurve curve = GetCurveNameFromDictionary(key_dict);
  const base::Value::Dict* key_jwk = key_dict.FindDict("jwk");
  ASSERT_TRUE(key_jwk);

  blink::WebCryptoKey private_key;
  ASSERT_EQ(
      Status::Success(),
      ImportKeyJwkFromDict(*key_jwk, CreateEcdsaImportAlgorithm(curve), true,
                           blink::kWebCryptoKeyUsageSign, &private_key));

  // Erase the "d" member so the private key JWK can be used to import the
  // public key (WebCrypto doesn't provide a mechanism for importing a public
  // key given a private key).
  base::Value::Dict key_jwk_copy = key_jwk->Clone();
  key_jwk_copy.Remove("d");
  blink::WebCryptoKey public_key;
  ASSERT_EQ(
      Status::Success(),
      ImportKeyJwkFromDict(key_jwk_copy, CreateEcdsaImportAlgorithm(curve),
                           true, blink::kWebCryptoKeyUsageVerify, &public_key));

  // Sign twice
  std::vector<uint8_t> message(10);
  blink::WebCryptoAlgorithm algorithm =
      CreateEcdsaAlgorithm(blink::kWebCryptoAlgorithmIdSha1);

  std::vector<uint8_t> signature1;
  std::vector<uint8_t> signature2;
  ASSERT_EQ(Status::Success(),
            Sign(algorithm, private_key, message, &signature1));
  ASSERT_EQ(Status::Success(),
            Sign(algorithm, private_key, message, &signature2));

  // The two signatures should be different.
  EXPECT_NE(signature1, signature2);

  // And both should be valid signatures which can be verified.
  bool signature_matches;
  ASSERT_EQ(Status::Success(), Verify(algorithm, public_key, signature1,
                                      message, &signature_matches));
  EXPECT_TRUE(signature_matches);
  ASSERT_EQ(Status::Success(), Verify(algorithm, public_key, signature2,
                                      message, &signature_matches));
  EXPECT_TRUE(signature_matches);
}

// The test file may include either public or private keys. In order to import
// them successfully, the correct usages need to be specified. This function
// determines what usages to use for the key.
blink::WebCryptoKeyUsageMask GetExpectedUsagesForKeyImport(
    blink::WebCryptoKeyFormat key_format,
    const base::Value::Dict& test) {
  blink::WebCryptoKeyUsageMask kPublicUsages = blink::kWebCryptoKeyUsageVerify;
  blink::WebCryptoKeyUsageMask kPrivateUsages = blink::kWebCryptoKeyUsageSign;

  switch (key_format) {
    case blink::kWebCryptoKeyFormatRaw:
    case blink::kWebCryptoKeyFormatSpki:
      return kPublicUsages;
    case blink::kWebCryptoKeyFormatPkcs8:
      return kPrivateUsages;
    case blink::kWebCryptoKeyFormatJwk: {
      const base::Value::Dict* key = test.FindDict("key");
      if (!key)
        ADD_FAILURE() << "Missing key property";
      return key->contains("d") ? kPrivateUsages : kPublicUsages;
    }
  }

  // Appease compiler.
  return kPrivateUsages;
}

// Tests importing bad public/private keys in a variety of formats.
TEST_F(WebCryptoEcdsaTest, ImportBadKeys) {
  base::Value::List tests = ReadJsonTestFileAsList("bad_ec_keys.json");

  for (const auto& test_value : tests) {
    SCOPED_TRACE(&test_value - &tests[0]);

    ASSERT_TRUE(test_value.is_dict());
    const base::Value::Dict& test = test_value.GetDict();

    blink::WebCryptoNamedCurve curve = GetCurveNameFromDictionary(test);
    blink::WebCryptoKeyFormat key_format = GetKeyFormatFromJsonTestCase(test);
    std::vector<uint8_t> key_data =
        GetKeyDataFromJsonTestCase(test, key_format);
    const std::string* expected_error = test.FindString("error");
    ASSERT_TRUE(expected_error);

    blink::WebCryptoKey key;
    Status status =
        ImportKey(key_format, key_data, CreateEcdsaImportAlgorithm(curve), true,
                  GetExpectedUsagesForKeyImport(key_format, test), &key);
    ASSERT_EQ(*expected_error, StatusToString(status));
  }
}

// Tests importing and exporting of EC private keys, using both JWK and PKCS8
// formats.
//
// The test imports a key first using JWK, and then exporting it to JWK and
// PKCS8. It does the same thing using PKCS8 as the original source of truth.
TEST_F(WebCryptoEcdsaTest, ImportExportPrivateKey) {
  base::Value::List tests = ReadJsonTestFileAsList("ec_private_keys.json");
  for (const auto& test_value : tests) {
    SCOPED_TRACE(&test_value - &tests[0]);

    ASSERT_TRUE(test_value.is_dict());
    const base::Value::Dict& test = test_value.GetDict();

    blink::WebCryptoNamedCurve curve = GetCurveNameFromDictionary(test);
    const base::Value::Dict* jwk_dict = test.FindDict("jwk");
    ASSERT_TRUE(jwk_dict);
    std::vector<uint8_t> jwk_bytes = MakeJsonVector(*jwk_dict);
    std::vector<uint8_t> pkcs8_bytes = GetBytesFromHexString(
        test, test.contains("exported_pkcs8") ? "exported_pkcs8" : "pkcs8");

    // -------------------------------------------------
    // Test from JWK, and then export to {JWK, PKCS8}
    // -------------------------------------------------

    // Import the key using JWK
    blink::WebCryptoKey key;
    ASSERT_EQ(Status::Success(),
              ImportKey(blink::kWebCryptoKeyFormatJwk, jwk_bytes,
                        CreateEcdsaImportAlgorithm(curve), true,
                        blink::kWebCryptoKeyUsageSign, &key));

    // Export the key as JWK
    std::vector<uint8_t> exported_bytes;
    ASSERT_EQ(Status::Success(),
              ExportKey(blink::kWebCryptoKeyFormatJwk, key, &exported_bytes));

    // NOTE: The exported bytes can't be directly compared to jwk_bytes because
    // the exported JWK differs from the imported one. In particular it contains
    // extra properties for extractability and key_ops.
    //
    // Verification is instead done by using the first exported JWK bytes as the
    // expectation.
    jwk_bytes = exported_bytes;
    ASSERT_EQ(Status::Success(),
              ImportKey(blink::kWebCryptoKeyFormatJwk, jwk_bytes,
                        CreateEcdsaImportAlgorithm(curve), true,
                        blink::kWebCryptoKeyUsageSign, &key));

    // Export the key as JWK (again)
    ASSERT_EQ(Status::Success(),
              ExportKey(blink::kWebCryptoKeyFormatJwk, key, &exported_bytes));
    EXPECT_EQ(jwk_bytes, exported_bytes);

    // Export the key as PKCS8
    ASSERT_EQ(Status::Success(),
              ExportKey(blink::kWebCryptoKeyFormatPkcs8, key, &exported_bytes));
    EXPECT_EQ(pkcs8_bytes, exported_bytes);

    // -------------------------------------------------
    // Test from PKCS8, and then export to {JWK, PKCS8}
    // -------------------------------------------------

    // The imported PKCS8 bytes may differ from the exported bytes (in the case
    // where the publicKey was missing, it will be synthesized and written back
    // during export).
    std::vector<uint8_t> pkcs8_input_bytes = GetBytesFromHexString(
        test, test.contains("original_pkcs8") ? "original_pkcs8" : "pkcs8");
    base::span<const uint8_t> pkcs8_input_data(
        pkcs8_input_bytes.empty() ? pkcs8_bytes : pkcs8_input_bytes);

    // Import the key using PKCS8
    ASSERT_EQ(Status::Success(),
              ImportKey(blink::kWebCryptoKeyFormatPkcs8, pkcs8_input_data,
                        CreateEcdsaImportAlgorithm(curve), true,
                        blink::kWebCryptoKeyUsageSign, &key));

    // Export the key as PKCS8
    ASSERT_EQ(Status::Success(),
              ExportKey(blink::kWebCryptoKeyFormatPkcs8, key, &exported_bytes));
    EXPECT_EQ(pkcs8_bytes, exported_bytes);

    // Export the key as JWK
    ASSERT_EQ(Status::Success(),
              ExportKey(blink::kWebCryptoKeyFormatJwk, key, &exported_bytes));
    EXPECT_EQ(jwk_bytes, exported_bytes);
  }
}

}  // namespace

}  // namespace webcrypto
