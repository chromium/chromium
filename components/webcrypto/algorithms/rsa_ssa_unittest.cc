// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/values.h"
#include "components/webcrypto/algorithm_dispatch.h"
#include "components/webcrypto/algorithms/test_helpers.h"
#include "components/webcrypto/status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_crypto_key.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"

namespace webcrypto {

namespace {

// Helper for ImportJwkRsaFailures. Restores the JWK JSON
// dictionary to a good state
void RestoreJwkRsaDictionary(base::DictionaryValue* dict) {
  dict->DictClear();
  dict->SetString("kty", "RSA");
  dict->SetString("alg", "RS256");
  dict->SetString("use", "sig");
  dict->SetBoolean("ext", false);
  dict->SetString(
      "n",
      "qLOyhK-OtQs4cDSoYPFGxJGfMYdjzWxVmMiuSBGh4KvEx-CwgtaTpef87Wdc9GaFEncsDLxk"
      "p0LGxjD1M8jMcvYq6DPEC_JYQumEu3i9v5fAEH1VvbZi9cTg-rmEXLUUjvc5LdOq_5OuHmtm"
      "e7PUJHYW1PW6ENTP0ibeiNOfFvs");
  dict->SetString("e", "AQAB");
}

class WebCryptoRsaSsaTest : public WebCryptoTestBase {};

TEST_F(WebCryptoRsaSsaTest, ImportExportSpki) {
  // Passing case: Import a valid RSA key in SPKI format.
  blink::WebCryptoKey key;
  ASSERT_EQ(Status::Success(),
            ImportKey(blink::kWebCryptoKeyFormatSpki,
                      HexStringToBytes(kPublicKeySpkiDerHex),
                      CreateRsaHashedImportAlgorithm(
                          blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                          blink::kWebCryptoAlgorithmIdSha256),
                      true, blink::kWebCryptoKeyUsageVerify, &key));
  EXPECT_TRUE(key.Handle());
  EXPECT_EQ(blink::kWebCryptoKeyTypePublic, key.GetType());
  EXPECT_TRUE(key.Extractable());
  EXPECT_EQ(blink::kWebCryptoKeyUsageVerify, key.Usages());
  EXPECT_EQ(kModulusLengthBits,
            key.Algorithm().RsaHashedParams()->ModulusLengthBits());
  EXPECT_BYTES_EQ_HEX("010001",
                      key.Algorithm().RsaHashedParams()->PublicExponent());

  // Failing case: Import RSA key but provide an inconsistent input algorithm.
  EXPECT_EQ(Status::ErrorUnsupportedImportKeyFormat(),
            ImportKey(blink::kWebCryptoKeyFormatSpki,
                      HexStringToBytes(kPublicKeySpkiDerHex),
                      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc), true,
                      blink::kWebCryptoKeyUsageEncrypt, &key));

  // Passing case: Export a previously imported RSA public key in SPKI format
  // and compare to original data.
  std::vector<uint8_t> output;
  ASSERT_EQ(Status::Success(),
            ExportKey(blink::kWebCryptoKeyFormatSpki, key, &output));
  EXPECT_BYTES_EQ_HEX(kPublicKeySpkiDerHex, output);

  // Failing case: Try to export a previously imported RSA public key in raw
  // format (not allowed for a public key).
  EXPECT_EQ(Status::ErrorUnsupportedExportKeyFormat(),
            ExportKey(blink::kWebCryptoKeyFormatRaw, key, &output));

  // Failing case: Try to export a non-extractable key
  ASSERT_EQ(Status::Success(),
            ImportKey(blink::kWebCryptoKeyFormatSpki,
                      HexStringToBytes(kPublicKeySpkiDerHex),
                      CreateRsaHashedImportAlgorithm(
                          blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                          blink::kWebCryptoAlgorithmIdSha256),
                      false, blink::kWebCryptoKeyUsageVerify, &key));
  EXPECT_TRUE(key.Handle());
  EXPECT_FALSE(key.Extractable());
  EXPECT_EQ(Status::ErrorKeyNotExtractable(),
            ExportKey(blink::kWebCryptoKeyFormatSpki, key, &output));

  // TODO(eroman): Failing test: Import a SPKI with an unrecognized hash OID
  // TODO(eroman): Failing test: Import a SPKI with invalid algorithm params
  // TODO(eroman): Failing test: Import a SPKI with inconsistent parameters
  // (e.g. SHA-1 in OID, SHA-256 in params)
  // TODO(eroman): Failing test: Import a SPKI for RSA-SSA, but with params
  // as OAEP/PSS
}

TEST_F(WebCryptoRsaSsaTest, ImportExportPkcs8) {
  // Passing case: Import a valid RSA key in PKCS#8 format.
  blink::WebCryptoKey key;
  ASSERT_EQ(Status::Success(),
            ImportKey(blink::kWebCryptoKeyFormatPkcs8,
                      HexStringToBytes(kPrivateKeyPkcs8DerHex),
                      CreateRsaHashedImportAlgorithm(
                          blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                          blink::kWebCryptoAlgorithmIdSha1),
                      true, blink::kWebCryptoKeyUsageSign, &key));
  EXPECT_TRUE(key.Handle());
  EXPECT_EQ(blink::kWebCryptoKeyTypePrivate, key.GetType());
  EXPECT_TRUE(key.Extractable());
  EXPECT_EQ(blink::kWebCryptoKeyUsageSign, key.Usages());
  EXPECT_EQ(blink::kWebCryptoAlgorithmIdSha1,
            key.Algorithm().RsaHashedParams()->GetHash().Id());
  EXPECT_EQ(kModulusLengthBits,
            key.Algorithm().RsaHashedParams()->ModulusLengthBits());
  EXPECT_BYTES_EQ_HEX("010001",
                      key.Algorithm().RsaHashedParams()->PublicExponent());

  std::vector<uint8_t> exported_key;
  ASSERT_EQ(Status::Success(),
            ExportKey(blink::kWebCryptoKeyFormatPkcs8, key, &exported_key));
  EXPECT_BYTES_EQ_HEX(kPrivateKeyPkcs8DerHex, exported_key);

  // Failing case: Import RSA key but provide an inconsistent input algorithm
  // and usage. Several issues here:
  //   * AES-CBC doesn't support PKCS8 key format
  //   * AES-CBC doesn't support "sign" usage
  EXPECT_EQ(Status::ErrorUnsupportedImportKeyFormat(),
            ImportKey(blink::kWebCryptoKeyFormatPkcs8,
                      HexStringToBytes(kPrivateKeyPkcs8DerHex),
                      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc), true,
                      blink::kWebCryptoKeyUsageSign, &key));
}

// Tests JWK import and export by doing a roundtrip key conversion and ensuring
// it was lossless:
//
//   PKCS8 --> JWK --> PKCS8
TEST_F(WebCryptoRsaSsaTest, ImportRsaPrivateKeyJwkToPkcs8RoundTrip) {
  blink::WebCryptoKey key;
  ASSERT_EQ(Status::Success(),
            ImportKey(blink::kWebCryptoKeyFormatPkcs8,
                      HexStringToBytes(kPrivateKeyPkcs8DerHex),
                      CreateRsaHashedImportAlgorithm(
                          blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                          blink::kWebCryptoAlgorithmIdSha1),
                      true, blink::kWebCryptoKeyUsageSign, &key));

  std::vector<uint8_t> exported_key_jwk;
  ASSERT_EQ(Status::Success(),
            ExportKey(blink::kWebCryptoKeyFormatJwk, key, &exported_key_jwk));

  // All of the optional parameters (p, q, dp, dq, qi) should be present in the
  // output.
  const char* expected_jwk =
      "{\"alg\":\"RS1\",\"d\":\"M6UEKpCyfU9UUcqbu9C0R3GhAa-IQ0Cu-YhfKku-"
      "kuiUpySsPFaMj5eFOtB8AmbIxqPKCSnx6PESMYhEKfxNmuVf7olqEM5wfD7X5zTkRyejlXRQ"
      "GlMmgxCcKrrKuig8MbS9L1PD7jfjUs7jT55QO9gMBiKtecbc7og1R8ajsyU\",\"dp\":"
      "\"KPoTk4ZVvh-"
      "KFZy6ylpy6hkMMAieGc0nSlVvNsT24Z9VSzTAd3kEJ7vdjdPt4kSDKPOF2Bsw6OQ7L_-"
      "gJ4YZeQ\",\"dq\":\"Gos485j6cSBJiY1_t57gp3ZoeRKZzfoJ78DlB6yyHtdDAe9b_Ui-"
      "RV6utuFnglWCdYCo5OjhQVHRUQqCo_LnKQ\",\"e\":\"AQAB\",\"ext\":true,\"key_"
      "ops\":[\"sign\"],\"kty\":\"RSA\",\"n\":"
      "\"pW5KDnAQF1iaUYfcfqhB0Vby7A42rVKkTf6x5h962ZHYxRBW_-2xYrTA8oOhKoijlN_"
      "1JqtykcuzB86r_OCx39XNlQgJbVsri2311nHvY3fAkhyyPCcKcOJZjm_4nRnxBazC0_"
      "DLNfKSgOE4a29kxO8i4eHyDQzoz_siSb2aITc\",\"p\":\"5-"
      "iUJyCod1Fyc6NWBT6iobwMlKpy1VxuhilrLfyWeUjApyy8zKfqyzVwbgmh31WhU1vZs8w0Fg"
      "s7bc0-2o5kQw\",\"q\":\"tp3KHPfU1-yB51uQ_MqHSrzeEj_"
      "ScAGAqpBHm25I3o1n7ST58Z2FuidYdPVCzSDccj5pYzZKH5QlRSsmmmeZ_Q\",\"qi\":"
      "\"JxVqukEm0kqB86Uoy_sn9WiG-"
      "ECp9uhuF6RLlP6TGVhLjiL93h5aLjvYqluo2FhBlOshkKz4MrhH8To9JKefTQ\"}";

  EXPECT_BYTES_EQ(
      base::as_bytes(base::make_span(base::StringPiece(expected_jwk))),
      exported_key_jwk);

  ASSERT_EQ(Status::Success(),
            ImportKey(blink::kWebCryptoKeyFormatJwk, exported_key_jwk,
                      CreateRsaHashedImportAlgorithm(
                          blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                          blink::kWebCryptoAlgorithmIdSha1),
                      true, blink::kWebCryptoKeyUsageSign, &key));

  std::vector<uint8_t> exported_key_pkcs8;
  ASSERT_EQ(Status::Success(), ExportKey(blink::kWebCryptoKeyFormatPkcs8, key,
                                         &exported_key_pkcs8));

  ASSERT_EQ(HexStringToBytes(kPrivateKeyPkcs8DerHex), exported_key_pkcs8);
}

// Tests importing multiple RSA private keys from JWK, and then exporting to
// PKCS8.
//
// This is a regression test for http://crbug.com/378315, for which importing
// a sequence of keys from JWK could yield the wrong key. The first key would
// be imported correctly, however every key after that would actually import
// the first key.
TEST_F(WebCryptoRsaSsaTest, ImportMultipleRSAPrivateKeysJwk) {
  // For this test to be meaningful the keys MUST be kept alive before importing
  // new keys.
  std::vector<blink::WebCryptoKey> live_keys;

  base::Value::List keys = ReadJsonTestFileAsList("rsa_private_keys.json");
  for (const auto& key_values : keys) {
    SCOPED_TRACE(&key_values - &keys[0]);

    ASSERT_TRUE(key_values.is_dict());
    const base::DictionaryValue* key_values_dict =
        &base::Value::AsDictionaryValue(key_values);

    // Get the JWK representation of the key.
    const base::DictionaryValue* key_jwk;
    ASSERT_TRUE(key_values_dict->GetDictionary("jwk", &key_jwk));

    // Get the PKCS8 representation of the key.
    std::string pkcs8_hex_string;
    ASSERT_TRUE(key_values_dict->GetString("pkcs8", &pkcs8_hex_string));
    std::vector<uint8_t> pkcs8_bytes = HexStringToBytes(pkcs8_hex_string);

    // Get the modulus length for the key.
    absl::optional<int> modulus_length_bits =
        key_values_dict->FindIntKey("modulusLength");
    ASSERT_TRUE(modulus_length_bits);

    blink::WebCryptoKey private_key;

    // Import the key from JWK.
    ASSERT_EQ(Status::Success(),
              ImportKeyJwkFromDict(
                  *key_jwk,
                  CreateRsaHashedImportAlgorithm(
                      blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                      blink::kWebCryptoAlgorithmIdSha256),
                  true, blink::kWebCryptoKeyUsageSign, &private_key));

    live_keys.push_back(private_key);

    EXPECT_EQ(
        *modulus_length_bits,
        static_cast<int>(
            private_key.Algorithm().RsaHashedParams()->ModulusLengthBits()));

    // Export to PKCS8 and verify that it matches expectation.
    std::vector<uint8_t> exported_key_pkcs8;
    ASSERT_EQ(Status::Success(), ExportKey(blink::kWebCryptoKeyFormatPkcs8,
                                           private_key, &exported_key_pkcs8));

    EXPECT_BYTES_EQ(pkcs8_bytes, exported_key_pkcs8);
  }
}

// Import an RSA private key using JWK. Next import a JWK containing the same
// modulus, but mismatched parameters for the rest. It should NOT be possible
// that the second import retrieves the first key. See http://crbug.com/378315
// for how that could happen.
TEST_F(WebCryptoRsaSsaTest, ImportJwkExistingModulusAndInvalid) {
  base::Value::List key_list = ReadJsonTestFileAsList("rsa_private_keys.json");

  // Import a 1024-bit private key.
  const base::Value& key1_props_value = key_list[1];
  ASSERT_TRUE(key1_props_value.is_dict());
  const base::DictionaryValue* key1_props =
      &base::Value::AsDictionaryValue(key1_props_value);
  const base::DictionaryValue* key1_jwk;
  ASSERT_TRUE(key1_props->GetDictionary("jwk", &key1_jwk));

  blink::WebCryptoKey key1;
  ASSERT_EQ(
      Status::Success(),
      ImportKeyJwkFromDict(*key1_jwk,
                           CreateRsaHashedImportAlgorithm(
                               blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                               blink::kWebCryptoAlgorithmIdSha256),
                           true, blink::kWebCryptoKeyUsageSign, &key1));

  ASSERT_EQ(1024u, key1.Algorithm().RsaHashedParams()->ModulusLengthBits());

  // Construct a JWK using the modulus of key1, but all the other fields from
  // another key (also a 1024-bit private key).
  base::Value& key2_props_value = key_list[5];
  ASSERT_TRUE(key2_props_value.is_dict());
  base::DictionaryValue* key2_props = const_cast<base::DictionaryValue*>(
      &base::Value::AsDictionaryValue(key2_props_value));
  base::DictionaryValue* key2_jwk;
  ASSERT_TRUE(key2_props->GetDictionary("jwk", &key2_jwk));
  std::string modulus;
  key1_jwk->GetString("n", &modulus);
  key2_jwk->SetString("n", modulus);

  // This should fail, as the n,e,d parameters are not consistent. It MUST NOT
  // somehow return the key created earlier.
  blink::WebCryptoKey key2;
  ASSERT_EQ(
      Status::OperationError(),
      ImportKeyJwkFromDict(*key2_jwk,
                           CreateRsaHashedImportAlgorithm(
                               blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                               blink::kWebCryptoAlgorithmIdSha256),
                           true, blink::kWebCryptoKeyUsageSign, &key2));
}

TEST_F(WebCryptoRsaSsaTest, GenerateKeyPairRsa) {
  // Note: using unrealistic short key lengths here to avoid bogging down tests.

  // Successful WebCryptoAlgorithmIdRsaSsaPkcs1v1_5 key generation (sha256)
  const unsigned int modulus_length = 256;
  const std::vector<uint8_t> public_exponent = HexStringToBytes("010001");
  blink::WebCryptoAlgorithm algorithm = CreateRsaHashedKeyGenAlgorithm(
      blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
      blink::kWebCryptoAlgorithmIdSha256, modulus_length, public_exponent);
  bool extractable = true;
  const blink::WebCryptoKeyUsageMask public_usages =
      blink::kWebCryptoKeyUsageVerify;
  const blink::WebCryptoKeyUsageMask private_usages =
      blink::kWebCryptoKeyUsageSign;
  const blink::WebCryptoKeyUsageMask usages = public_usages | private_usages;
  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;

  EXPECT_EQ(Status::Success(), GenerateKeyPair(algorithm, extractable, usages,
                                               &public_key, &private_key));
  ASSERT_FALSE(public_key.IsNull());
  ASSERT_FALSE(private_key.IsNull());
  EXPECT_EQ(blink::kWebCryptoKeyTypePublic, public_key.GetType());
  EXPECT_EQ(blink::kWebCryptoKeyTypePrivate, private_key.GetType());
  EXPECT_EQ(modulus_length,
            public_key.Algorithm().RsaHashedParams()->ModulusLengthBits());
  EXPECT_EQ(modulus_length,
            private_key.Algorithm().RsaHashedParams()->ModulusLengthBits());
  EXPECT_EQ(blink::kWebCryptoAlgorithmIdSha256,
            public_key.Algorithm().RsaHashedParams()->GetHash().Id());
  EXPECT_EQ(blink::kWebCryptoAlgorithmIdSha256,
            private_key.Algorithm().RsaHashedParams()->GetHash().Id());
  EXPECT_TRUE(public_key.Extractable());
  EXPECT_EQ(extractable, private_key.Extractable());
  EXPECT_EQ(public_usages, public_key.Usages());
  EXPECT_EQ(private_usages, private_key.Usages());

  // Try exporting the generated key pair, and then re-importing to verify that
  // the exported data was valid.
  std::vector<uint8_t> public_key_spki;
  EXPECT_EQ(Status::Success(), ExportKey(blink::kWebCryptoKeyFormatSpki,
                                         public_key, &public_key_spki));

  public_key = blink::WebCryptoKey::CreateNull();
  ASSERT_EQ(Status::Success(),
            ImportKey(blink::kWebCryptoKeyFormatSpki, public_key_spki,
                      CreateRsaHashedImportAlgorithm(
                          blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                          blink::kWebCryptoAlgorithmIdSha256),
                      true, public_usages, &public_key));
  EXPECT_EQ(modulus_length,
            public_key.Algorithm().RsaHashedParams()->ModulusLengthBits());

  std::vector<uint8_t> private_key_pkcs8;
  EXPECT_EQ(Status::Success(), ExportKey(blink::kWebCryptoKeyFormatPkcs8,
                                         private_key, &private_key_pkcs8));
  private_key = blink::WebCryptoKey::CreateNull();
  ASSERT_EQ(Status::Success(),
            ImportKey(blink::kWebCryptoKeyFormatPkcs8, private_key_pkcs8,
                      CreateRsaHashedImportAlgorithm(
                          blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                          blink::kWebCryptoAlgorithmIdSha256),
                      true, private_usages, &private_key));
  EXPECT_EQ(modulus_length,
            private_key.Algorithm().RsaHashedParams()->ModulusLengthBits());

  // Fail with bad modulus.
  algorithm = CreateRsaHashedKeyGenAlgorithm(
      blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
      blink::kWebCryptoAlgorithmIdSha256, 0, public_exponent);
  EXPECT_EQ(Status::ErrorGenerateRsaUnsupportedModulus(),
            GenerateKeyPair(algorithm, extractable, usages, &public_key,
                            &private_key));

  // Fail with bad exponent: larger than unsigned long.
  unsigned int exponent_length = sizeof(unsigned long) + 1;  // NOLINT
  const std::vector<uint8_t> long_exponent(exponent_length, 0x01);
  algorithm = CreateRsaHashedKeyGenAlgorithm(
      blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
      blink::kWebCryptoAlgorithmIdSha256, modulus_length, long_exponent);
  EXPECT_EQ(Status::ErrorGenerateKeyPublicExponent(),
            GenerateKeyPair(algorithm, extractable, usages, &public_key,
                            &private_key));

  // Fail with bad exponent: empty.
  const std::vector<uint8_t> empty_exponent;
  algorithm = CreateRsaHashedKeyGenAlgorithm(
      blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
      blink::kWebCryptoAlgorithmIdSha256, modulus_length, empty_exponent);
  EXPECT_EQ(Status::ErrorGenerateKeyPublicExponent(),
            GenerateKeyPair(algorithm, extractable, usages, &public_key,
                            &private_key));

  // Fail with bad exponent: all zeros.
  std::vector<uint8_t> exponent_with_leading_zeros(15, 0x00);
  algorithm = CreateRsaHashedKeyGenAlgorithm(
      blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
      blink::kWebCryptoAlgorithmIdSha256, modulus_length,
      exponent_with_leading_zeros);
  EXPECT_EQ(Status::ErrorGenerateKeyPublicExponent(),
            GenerateKeyPair(algorithm, extractable, usages, &public_key,
                            &private_key));

  // Key generation success using exponent with leading zeros.
  exponent_with_leading_zeros.insert(exponent_with_leading_zeros.end(),
                                     public_exponent.begin(),
                                     public_exponent.end());
  algorithm = CreateRsaHashedKeyGenAlgorithm(
      blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
      blink::kWebCryptoAlgorithmIdSha256, modulus_length,
      exponent_with_leading_zeros);
  EXPECT_EQ(Status::Success(), GenerateKeyPair(algorithm, extractable, usages,
                                               &public_key, &private_key));
  EXPECT_FALSE(public_key.IsNull());
  EXPECT_FALSE(private_key.IsNull());
  EXPECT_EQ(blink::kWebCryptoKeyTypePublic, public_key.GetType());
  EXPECT_EQ(blink::kWebCryptoKeyTypePrivate, private_key.GetType());
  EXPECT_TRUE(public_key.Extractable());
  EXPECT_EQ(extractable, private_key.Extractable());
  EXPECT_EQ(public_usages, public_key.Usages());
  EXPECT_EQ(private_usages, private_key.Usages());

  // Successful WebCryptoAlgorithmIdRsaSsaPkcs1v1_5 key generation (sha1)
  algorithm = CreateRsaHashedKeyGenAlgorithm(
      blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
      blink::kWebCryptoAlgorithmIdSha1, modulus_length, public_exponent);
  EXPECT_EQ(Status::Success(), GenerateKeyPair(algorithm, false, usages,
                                               &public_key, &private_key));
  EXPECT_FALSE(public_key.IsNull());
  EXPECT_FALSE(private_key.IsNull());
  EXPECT_EQ(blink::kWebCryptoKeyTypePublic, public_key.GetType());
  EXPECT_EQ(blink::kWebCryptoKeyTypePrivate, private_key.GetType());
  EXPECT_EQ(modulus_length,
            public_key.Algorithm().RsaHashedParams()->ModulusLengthBits());
  EXPECT_EQ(modulus_length,
            private_key.Algorithm().RsaHashedParams()->ModulusLengthBits());
  EXPECT_EQ(blink::kWebCryptoAlgorithmIdSha1,
            public_key.Algorithm().RsaHashedParams()->GetHash().Id());
  EXPECT_EQ(blink::kWebCryptoAlgorithmIdSha1,
            private_key.Algorithm().RsaHashedParams()->GetHash().Id());
  // Even though "extractable" was set to false, the public key remains
  // extractable.
  EXPECT_TRUE(public_key.Extractable());
  EXPECT_FALSE(private_key.Extractable());
  EXPECT_EQ(public_usages, public_key.Usages());
  EXPECT_EQ(private_usages, private_key.Usages());

  // Exporting a private key as SPKI format doesn't make sense. However this
  // will first fail because the key is not extractable.
  std::vector<uint8_t> output;
  EXPECT_EQ(Status::ErrorKeyNotExtractable(),
            ExportKey(blink::kWebCryptoKeyFormatSpki, private_key, &output));

  // Re-generate an extractable private_key and try to export it as SPKI format.
  // This should fail since spki is for public keys.
  EXPECT_EQ(Status::Success(), GenerateKeyPair(algorithm, true, usages,
                                               &public_key, &private_key));
  EXPECT_EQ(Status::ErrorUnexpectedKeyType(),
            ExportKey(blink::kWebCryptoKeyFormatSpki, private_key, &output));
}

TEST_F(WebCryptoRsaSsaTest, GenerateKeyPairRsaBadModulusLength) {
  const unsigned int kBadModulusBits[] = {
      0,
      248,         // Too small.
      257,         // Not a multiple of 8.
      1023,        // Not a multiple of 8.
      0xFFFFFFFF,  // Too big.
      16384 + 8,   // 16384 is the maxmimum length that NSS succeeds for.
  };

  const std::vector<uint8_t> public_exponent = HexStringToBytes("010001");

  for (auto modulus_length_bits : kBadModulusBits) {
    blink::WebCryptoAlgorithm algorithm = CreateRsaHashedKeyGenAlgorithm(
        blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
        blink::kWebCryptoAlgorithmIdSha256, modulus_length_bits,
        public_exponent);
    bool extractable = true;
    const blink::WebCryptoKeyUsageMask usages = blink::kWebCryptoKeyUsageSign;
    blink::WebCryptoKey public_key;
    blink::WebCryptoKey private_key;

    EXPECT_EQ(Status::ErrorGenerateRsaUnsupportedModulus(),
              GenerateKeyPair(algorithm, extractable, usages, &public_key,
                              &private_key));
  }
}

// Try generating RSA key pairs using unsupported public exponents. Only
// exponents of 3 and 65537 are supported. Although OpenSSL can support other
// values, it can also hang when given invalid exponents. To avoid hanging, use
// a whitelist of known safe exponents.
TEST_F(WebCryptoRsaSsaTest, GenerateKeyPairRsaBadExponent) {
  const unsigned int modulus_length = 1024;

  const char* const kPublicExponents[] = {
      "11",  // 17 - This is a valid public exponent, but currently disallowed.
      "00",
      "01",
      "02",
      "010000",  // 65536
  };

  for (auto* const exponent : kPublicExponents) {
    SCOPED_TRACE(&exponent - &kPublicExponents[0]);
    blink::WebCryptoAlgorithm algorithm = CreateRsaHashedKeyGenAlgorithm(
        blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
        blink::kWebCryptoAlgorithmIdSha256, modulus_length,
        HexStringToBytes(exponent));

    blink::WebCryptoKey public_key;
    blink::WebCryptoKey private_key;

    EXPECT_EQ(Status::ErrorGenerateKeyPublicExponent(),
              GenerateKeyPair(algorithm, true, blink::kWebCryptoKeyUsageSign,
                              &public_key, &private_key));
  }
}

TEST_F(WebCryptoRsaSsaTest, SignVerifyFailures) {
  // Import a key pair.
  blink::WebCryptoAlgorithm import_algorithm = CreateRsaHashedImportAlgorithm(
      blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
      blink::kWebCryptoAlgorithmIdSha1);
  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;
  ASSERT_NO_FATAL_FAILURE(ImportRsaKeyPair(
      HexStringToBytes(kPublicKeySpkiDerHex),
      HexStringToBytes(kPrivateKeyPkcs8DerHex), import_algorithm, false,
      blink::kWebCryptoKeyUsageVerify, blink::kWebCryptoKeyUsageSign,
      &public_key, &private_key));

  blink::WebCryptoAlgorithm algorithm =
      CreateAlgorithm(blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5);

  std::vector<uint8_t> signature;
  bool signature_match;

  // Compute a signature.
  const std::vector<uint8_t> data = HexStringToBytes("010203040506070809");
  ASSERT_EQ(Status::Success(), Sign(algorithm, private_key, data, &signature));

  // Ensure truncated signature does not verify by passing one less byte.
  EXPECT_EQ(Status::Success(),
            Verify(algorithm, public_key,
                   base::make_span(signature).first(signature.size() - 1), data,
                   &signature_match));
  EXPECT_FALSE(signature_match);

  // Ensure truncated signature does not verify by passing no bytes.
  EXPECT_EQ(Status::Success(),
            Verify(algorithm, public_key, {}, data, &signature_match));
  EXPECT_FALSE(signature_match);

  // Ensure corrupted signature does not verify.
  std::vector<uint8_t> corrupt_sig = signature;
  corrupt_sig[corrupt_sig.size() / 2] ^= 0x1;
  EXPECT_EQ(Status::Success(),
            Verify(algorithm, public_key, corrupt_sig, data, &signature_match));
  EXPECT_FALSE(signature_match);

  // Ensure signatures that are greater than the modulus size fail.
  const size_t long_message_size_bytes = 1024;
  DCHECK_GT(long_message_size_bytes, kModulusLengthBits / 8);
  const unsigned char kLongSignature[long_message_size_bytes] = {0};
  EXPECT_EQ(Status::Success(), Verify(algorithm, public_key, kLongSignature,
                                      data, &signature_match));
  EXPECT_FALSE(signature_match);

  // Ensure that signing and verifying with an incompatible algorithm fails.
  algorithm = CreateAlgorithm(blink::kWebCryptoAlgorithmIdRsaOaep);

  EXPECT_EQ(Status::ErrorUnexpected(),
            Sign(algorithm, private_key, data, &signature));
  EXPECT_EQ(Status::ErrorUnexpected(),
            Verify(algorithm, public_key, signature, data, &signature_match));

  // Some crypto libraries (NSS) can automatically select the RSA SSA inner hash
  // based solely on the contents of the input signature data. In the Web Crypto
  // implementation, the inner hash should be specified uniquely by the key
  // algorithm parameter. To validate this behavior, call Verify with a computed
  // signature that used one hash type (SHA-1), but pass in a key with a
  // different inner hash type (SHA-256). If the hash type is determined by the
  // signature itself (undesired), the verify will pass, while if the hash type
  // is specified by the key algorithm (desired), the verify will fail.

  // Compute a signature using SHA-1 as the inner hash.
  EXPECT_EQ(Status::Success(),
            Sign(CreateAlgorithm(blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5),
                 private_key, data, &signature));

  blink::WebCryptoKey public_key_256;
  EXPECT_EQ(Status::Success(),
            ImportKey(blink::kWebCryptoKeyFormatSpki,
                      HexStringToBytes(kPublicKeySpkiDerHex),
                      CreateRsaHashedImportAlgorithm(
                          blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                          blink::kWebCryptoAlgorithmIdSha256),
                      true, blink::kWebCryptoKeyUsageVerify, &public_key_256));

  // Now verify using an algorithm whose inner hash is SHA-256, not SHA-1. The
  // signature should not verify.
  // NOTE: public_key was produced by generateKey, and so its associated
  // algorithm has WebCryptoRsaKeyGenParams and not WebCryptoRsaSsaParams. Thus
  // it has no inner hash to conflict with the input algorithm.
  EXPECT_EQ(blink::kWebCryptoAlgorithmIdSha1,
            private_key.Algorithm().RsaHashedParams()->GetHash().Id());
  EXPECT_EQ(blink::kWebCryptoAlgorithmIdSha256,
            public_key_256.Algorithm().RsaHashedParams()->GetHash().Id());

  bool is_match;
  EXPECT_EQ(Status::Success(),
            Verify(CreateAlgorithm(blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5),
                   public_key_256, signature, data, &is_match));
  EXPECT_FALSE(is_match);
}

TEST_F(WebCryptoRsaSsaTest, SignVerifyKnownAnswer) {
  // Import the key pair.
  blink::WebCryptoAlgorithm import_algorithm = CreateRsaHashedImportAlgorithm(
      blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
      blink::kWebCryptoAlgorithmIdSha1);
  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;
  ASSERT_NO_FATAL_FAILURE(ImportRsaKeyPair(
      HexStringToBytes(kPublicKeySpkiDerHex),
      HexStringToBytes(kPrivateKeyPkcs8DerHex), import_algorithm, false,
      blink::kWebCryptoKeyUsageVerify, blink::kWebCryptoKeyUsageSign,
      &public_key, &private_key));

  blink::WebCryptoAlgorithm algorithm =
      CreateAlgorithm(blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5);

  // Validate the signatures are computed and verified as expected.
  base::Value::List tests = ReadJsonTestFileAsList("pkcs1v15_sign.json");

  std::vector<uint8_t> signature;
  for (const auto& test_value : tests) {
    SCOPED_TRACE(&test_value - &tests[0]);

    ASSERT_TRUE(test_value.is_dict());
    const base::DictionaryValue* test =
        &base::Value::AsDictionaryValue(test_value);

    std::vector<uint8_t> test_message =
        GetBytesFromHexString(test, "message_hex");
    std::vector<uint8_t> test_signature =
        GetBytesFromHexString(test, "signature_hex");

    signature.clear();
    ASSERT_EQ(Status::Success(),
              Sign(algorithm, private_key, test_message, &signature));
    EXPECT_BYTES_EQ(test_signature, signature);

    bool is_match = false;
    ASSERT_EQ(Status::Success(), Verify(algorithm, public_key, test_signature,
                                        test_message, &is_match));
    EXPECT_TRUE(is_match);
  }
}

// Try importing an RSA-SSA public key with unsupported key usages using SPKI
// format. RSA-SSA public keys only support the 'verify' usage.
TEST_F(WebCryptoRsaSsaTest, ImportRsaSsaPublicKeyBadUsage_SPKI) {
  const blink::WebCryptoAlgorithm algorithm = CreateRsaHashedImportAlgorithm(
      blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
      blink::kWebCryptoAlgorithmIdSha256);

  const blink::WebCryptoKeyUsageMask kBadUsages[] = {
      blink::kWebCryptoKeyUsageSign,
      blink::kWebCryptoKeyUsageSign | blink::kWebCryptoKeyUsageVerify,
      blink::kWebCryptoKeyUsageEncrypt,
      blink::kWebCryptoKeyUsageEncrypt | blink::kWebCryptoKeyUsageDecrypt,
  };

  for (auto usage : kBadUsages) {
    blink::WebCryptoKey public_key;
    ASSERT_EQ(Status::ErrorCreateKeyBadUsages(),
              ImportKey(blink::kWebCryptoKeyFormatSpki,
                        HexStringToBytes(kPublicKeySpkiDerHex), algorithm,
                        false, usage, &public_key));
  }
}

// Try importing an RSA-SSA public key with unsupported key usages using JWK
// format. RSA-SSA public keys only support the 'verify' usage.
TEST_F(WebCryptoRsaSsaTest, ImportRsaSsaPublicKeyBadUsage_JWK) {
  const blink::WebCryptoAlgorithm algorithm = CreateRsaHashedImportAlgorithm(
      blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
      blink::kWebCryptoAlgorithmIdSha256);

  const blink::WebCryptoKeyUsageMask kBadUsages[] = {
      blink::kWebCryptoKeyUsageSign,
      blink::kWebCryptoKeyUsageSign | blink::kWebCryptoKeyUsageVerify,
      blink::kWebCryptoKeyUsageEncrypt,
      blink::kWebCryptoKeyUsageEncrypt | blink::kWebCryptoKeyUsageDecrypt,
  };

  base::DictionaryValue dict;
  RestoreJwkRsaDictionary(&dict);
  dict.RemoveKey("use");
  dict.SetString("alg", "RS256");

  for (auto usage : kBadUsages) {
    blink::WebCryptoKey public_key;
    ASSERT_EQ(Status::ErrorCreateKeyBadUsages(),
              ImportKeyJwkFromDict(dict, algorithm, false, usage, &public_key));
  }
}

// Generate an RSA-SSA key pair with invalid usages. RSA-SSA supports:
//   'sign', 'verify'
TEST_F(WebCryptoRsaSsaTest, GenerateKeyBadUsages) {
  const blink::WebCryptoKeyUsageMask kBadUsages[] = {
      blink::kWebCryptoKeyUsageDecrypt,
      blink::kWebCryptoKeyUsageVerify | blink::kWebCryptoKeyUsageDecrypt,
      blink::kWebCryptoKeyUsageWrapKey,
  };

  const unsigned int modulus_length = 256;
  const std::vector<uint8_t> public_exponent = HexStringToBytes("010001");

  for (auto usage : kBadUsages) {
    blink::WebCryptoKey public_key;
    blink::WebCryptoKey private_key;

    ASSERT_EQ(Status::ErrorCreateKeyBadUsages(),
              GenerateKeyPair(CreateRsaHashedKeyGenAlgorithm(
                                  blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                                  blink::kWebCryptoAlgorithmIdSha256,
                                  modulus_length, public_exponent),
                              true, usage, &public_key, &private_key));
  }
}

// Generate an RSA-SSA key pair. The public and private keys should select the
// key usages which are applicable, and not have the exact same usages as was
// specified to GenerateKey
TEST_F(WebCryptoRsaSsaTest, GenerateKeyPairIntersectUsages) {
  const unsigned int modulus_length = 256;
  const std::vector<uint8_t> public_exponent = HexStringToBytes("010001");

  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;

  ASSERT_EQ(
      Status::Success(),
      GenerateKeyPair(
          CreateRsaHashedKeyGenAlgorithm(
              blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
              blink::kWebCryptoAlgorithmIdSha256, modulus_length,
              public_exponent),
          true, blink::kWebCryptoKeyUsageSign | blink::kWebCryptoKeyUsageVerify,
          &public_key, &private_key));

  EXPECT_EQ(blink::kWebCryptoKeyUsageVerify, public_key.Usages());
  EXPECT_EQ(blink::kWebCryptoKeyUsageSign, private_key.Usages());

  // Try again but this time without the Verify usages.
  ASSERT_EQ(Status::Success(),
            GenerateKeyPair(CreateRsaHashedKeyGenAlgorithm(
                                blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                                blink::kWebCryptoAlgorithmIdSha256,
                                modulus_length, public_exponent),
                            true, blink::kWebCryptoKeyUsageSign, &public_key,
                            &private_key));

  EXPECT_EQ(0, public_key.Usages());
  EXPECT_EQ(blink::kWebCryptoKeyUsageSign, private_key.Usages());
}

TEST_F(WebCryptoRsaSsaTest, GenerateKeyPairEmptyUsages) {
  const unsigned int modulus_length = 256;
  const std::vector<uint8_t> public_exponent = HexStringToBytes("010001");

  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;

  ASSERT_EQ(Status::ErrorCreateKeyEmptyUsages(),
            GenerateKeyPair(CreateRsaHashedKeyGenAlgorithm(
                                blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                                blink::kWebCryptoAlgorithmIdSha256,
                                modulus_length, public_exponent),
                            true, 0, &public_key, &private_key));
}

TEST_F(WebCryptoRsaSsaTest, ImportKeyEmptyUsages) {
  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;

  // Public without usage does not throw an error.
  ASSERT_EQ(Status::Success(),
            ImportKey(blink::kWebCryptoKeyFormatSpki,
                      HexStringToBytes(kPublicKeySpkiDerHex),
                      CreateRsaHashedImportAlgorithm(
                          blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                          blink::kWebCryptoAlgorithmIdSha256),
                      true, 0, &public_key));
  EXPECT_EQ(0, public_key.Usages());

  // Private empty usage will throw an error.
  ASSERT_EQ(Status::ErrorCreateKeyEmptyUsages(),
            ImportKey(blink::kWebCryptoKeyFormatPkcs8,
                      HexStringToBytes(kPrivateKeyPkcs8DerHex),
                      CreateRsaHashedImportAlgorithm(
                          blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                          blink::kWebCryptoAlgorithmIdSha1),
                      true, 0, &private_key));

  std::vector<uint8_t> public_jwk;
  ASSERT_EQ(Status::Success(),
            ExportKey(blink::kWebCryptoKeyFormatJwk, public_key, &public_jwk));

  ASSERT_EQ(Status::Success(),
            ImportKey(blink::kWebCryptoKeyFormatJwk, public_jwk,
                      CreateRsaHashedImportAlgorithm(
                          blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                          blink::kWebCryptoAlgorithmIdSha256),
                      true, 0, &public_key));
  EXPECT_EQ(0, public_key.Usages());

  // With correct usage to get correct imported private_key
  std::vector<uint8_t> private_jwk;
  ImportKey(blink::kWebCryptoKeyFormatPkcs8,
            HexStringToBytes(kPrivateKeyPkcs8DerHex),
            CreateRsaHashedImportAlgorithm(
                blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                blink::kWebCryptoAlgorithmIdSha1),
            true, blink::kWebCryptoKeyUsageSign, &private_key);

  ASSERT_EQ(Status::Success(), ExportKey(blink::kWebCryptoKeyFormatJwk,
                                         private_key, &private_jwk));

  ASSERT_EQ(Status::ErrorCreateKeyEmptyUsages(),
            ImportKey(blink::kWebCryptoKeyFormatJwk, private_jwk,
                      CreateRsaHashedImportAlgorithm(
                          blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                          blink::kWebCryptoAlgorithmIdSha1),
                      true, 0, &private_key));
}

TEST_F(WebCryptoRsaSsaTest, ImportExportJwkRsaPublicKey) {
  struct TestCase {
    const blink::WebCryptoAlgorithmId hash;
    const blink::WebCryptoKeyUsageMask usage;
    const char* const jwk_alg;
  };
  const TestCase kTests[] = {{blink::kWebCryptoAlgorithmIdSha1,
                              blink::kWebCryptoKeyUsageVerify, "RS1"},
                             {blink::kWebCryptoAlgorithmIdSha256,
                              blink::kWebCryptoKeyUsageVerify, "RS256"},
                             {blink::kWebCryptoAlgorithmIdSha384,
                              blink::kWebCryptoKeyUsageVerify, "RS384"},
                             {blink::kWebCryptoAlgorithmIdSha512,
                              blink::kWebCryptoKeyUsageVerify, "RS512"}};

  for (const auto& test : kTests) {
    SCOPED_TRACE(&test - &kTests[0]);

    const blink::WebCryptoAlgorithm import_algorithm =
        CreateRsaHashedImportAlgorithm(
            blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5, test.hash);

    // Import the spki to create a public key
    blink::WebCryptoKey public_key;
    ASSERT_EQ(Status::Success(),
              ImportKey(blink::kWebCryptoKeyFormatSpki,
                        HexStringToBytes(kPublicKeySpkiDerHex),
                        import_algorithm, true, test.usage, &public_key));

    // Export the public key as JWK and verify its contents
    std::vector<uint8_t> jwk;
    ASSERT_EQ(Status::Success(),
              ExportKey(blink::kWebCryptoKeyFormatJwk, public_key, &jwk));
    EXPECT_TRUE(VerifyPublicJwk(jwk, test.jwk_alg, kPublicKeyModulusHex,
                                kPublicKeyExponentHex, test.usage));

    // Import the JWK back in to create a new key
    blink::WebCryptoKey public_key2;
    ASSERT_EQ(Status::Success(),
              ImportKey(blink::kWebCryptoKeyFormatJwk, jwk, import_algorithm,
                        true, test.usage, &public_key2));
    ASSERT_TRUE(public_key2.Handle());
    EXPECT_EQ(blink::kWebCryptoKeyTypePublic, public_key2.GetType());
    EXPECT_TRUE(public_key2.Extractable());
    EXPECT_EQ(import_algorithm.Id(), public_key2.Algorithm().Id());

    // Export the new key as spki and compare to the original.
    std::vector<uint8_t> spki;
    ASSERT_EQ(Status::Success(),
              ExportKey(blink::kWebCryptoKeyFormatSpki, public_key2, &spki));
    EXPECT_BYTES_EQ_HEX(kPublicKeySpkiDerHex, spki);
  }
}

TEST_F(WebCryptoRsaSsaTest, ImportJwkRsaFailures) {
  base::DictionaryValue dict;
  RestoreJwkRsaDictionary(&dict);
  blink::WebCryptoAlgorithm algorithm = CreateRsaHashedImportAlgorithm(
      blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
      blink::kWebCryptoAlgorithmIdSha256);
  blink::WebCryptoKeyUsageMask usages = blink::kWebCryptoKeyUsageVerify;
  blink::WebCryptoKey key;

  // An RSA public key JWK _must_ have an "n" (modulus) and an "e" (exponent)
  // entry, while an RSA private key must have those plus at least a "d"
  // (private exponent) entry.
  // See http://tools.ietf.org/html/draft-ietf-jose-json-web-algorithms-18,
  // section 6.3.

  // Baseline pass.
  EXPECT_EQ(Status::Success(),
            ImportKeyJwkFromDict(dict, algorithm, false, usages, &key));
  EXPECT_EQ(algorithm.Id(), key.Algorithm().Id());
  EXPECT_FALSE(key.Extractable());
  EXPECT_EQ(blink::kWebCryptoKeyUsageVerify, key.Usages());
  EXPECT_EQ(blink::kWebCryptoKeyTypePublic, key.GetType());

  // The following are specific failure cases for when kty = "RSA".

  // Fail if either "n" or "e" is not present or malformed.
  for (auto* const parm : {"n", "e"}) {
    // Fail on missing parameter.
    dict.RemoveKey(parm);
    EXPECT_NE(Status::Success(),
              ImportKeyJwkFromDict(dict, algorithm, false, usages, &key));
    RestoreJwkRsaDictionary(&dict);

    // Fail on bad b64 parameter encoding.
    dict.SetString(parm, "Qk3f0DsytU8lfza2au #$% Htaw2xpop9yTuH0");
    EXPECT_NE(Status::Success(),
              ImportKeyJwkFromDict(dict, algorithm, false, usages, &key));
    RestoreJwkRsaDictionary(&dict);

    // Fail on empty parameter.
    dict.SetString(parm, "");
    EXPECT_EQ(Status::ErrorJwkEmptyBigInteger(parm),
              ImportKeyJwkFromDict(dict, algorithm, false, usages, &key));
    RestoreJwkRsaDictionary(&dict);
  }
}

// Try importing an RSA-SSA key from JWK format, having specified both Sign and
// Verify usage, AND an invalid JWK.
//
// Parsing the invalid JWK will fail before the usage check is done.
TEST_F(WebCryptoRsaSsaTest, ImportRsaSsaJwkBadUsageAndData) {
  std::string bad_data = "hello";

  blink::WebCryptoKey key;
  ASSERT_EQ(
      Status::ErrorJwkNotDictionary(),
      ImportKey(blink::kWebCryptoKeyFormatJwk,
                base::as_bytes(base::make_span(bad_data)),
                CreateRsaHashedImportAlgorithm(
                    blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                    blink::kWebCryptoAlgorithmIdSha256),
                true,
                blink::kWebCryptoKeyUsageVerify | blink::kWebCryptoKeyUsageSign,
                &key));
}

// Imports invalid JWK/SPKI/PKCS8 data and verifies that it fails as expected.
TEST_F(WebCryptoRsaSsaTest, ImportInvalidKeyData) {
  base::Value::List tests = ReadJsonTestFileAsList("bad_rsa_keys.json");
  for (const auto& test_value : tests) {
    SCOPED_TRACE(&test_value - &tests[0]);

    ASSERT_TRUE(test_value.is_dict());
    const base::DictionaryValue* test =
        &base::Value::AsDictionaryValue(test_value);

    blink::WebCryptoKeyFormat key_format = GetKeyFormatFromJsonTestCase(test);
    std::vector<uint8_t> key_data =
        GetKeyDataFromJsonTestCase(test, key_format);
    std::string test_error;
    ASSERT_TRUE(test->GetString("error", &test_error));

    blink::WebCryptoKeyUsageMask usages = blink::kWebCryptoKeyUsageSign;
    if (key_format == blink::kWebCryptoKeyFormatSpki)
      usages = blink::kWebCryptoKeyUsageVerify;
    blink::WebCryptoKey key;
    Status status = ImportKey(key_format, key_data,
                              CreateRsaHashedImportAlgorithm(
                                  blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                                  blink::kWebCryptoAlgorithmIdSha256),
                              true, usages, &key);
    EXPECT_EQ(test_error, StatusToString(status));
  }
}

}  // namespace

}  // namespace webcrypto
