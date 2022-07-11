// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/values.h"
#include "components/webcrypto/algorithm_dispatch.h"
#include "components/webcrypto/algorithms/test_helpers.h"
#include "components/webcrypto/status.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"

namespace webcrypto {

namespace {

// Creates an AES-CBC algorithm.
blink::WebCryptoAlgorithm CreateAesCbcAlgorithm(
    const std::vector<uint8_t>& iv) {
  return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
      blink::kWebCryptoAlgorithmIdAesCbc, new blink::WebCryptoAesCbcParams(iv));
}

blink::WebCryptoAlgorithm CreateAesCbcKeyGenAlgorithm(
    uint16_t key_length_bits) {
  return CreateAesKeyGenAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc,
                                  key_length_bits);
}

blink::WebCryptoKey GetTestAesCbcKey() {
  const std::string key_hex = "2b7e151628aed2a6abf7158809cf4f3c";
  blink::WebCryptoKey key = ImportSecretKeyFromRaw(
      HexStringToBytes(key_hex),
      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc),
      blink::kWebCryptoKeyUsageEncrypt | blink::kWebCryptoKeyUsageDecrypt);

  // Verify exported raw key is identical to the imported data
  std::vector<uint8_t> raw_key;
  EXPECT_EQ(Status::Success(),
            ExportKey(blink::kWebCryptoKeyFormatRaw, key, &raw_key));
  EXPECT_BYTES_EQ_HEX(key_hex, raw_key);
  return key;
}

class WebCryptoAesCbcTest : public WebCryptoTestBase {};

TEST_F(WebCryptoAesCbcTest, InputTooLarge) {
  std::vector<uint8_t> output;

  std::vector<uint8_t> iv(16);

  // Give an input that is too large. It would cause integer overflow when
  // narrowing the ciphertext size to an int, since OpenSSL operates on signed
  // int lengths NOT unsigned.
  //
  // Pretend the input is large. Don't pass data pointer as NULL in case that
  // is special cased; the implementation shouldn't actually dereference the
  // data.
  base::span<const uint8_t> input(iv.data(), INT_MAX - 3);

  EXPECT_EQ(
      Status::ErrorDataTooLarge(),
      Encrypt(CreateAesCbcAlgorithm(iv), GetTestAesCbcKey(), input, &output));
  EXPECT_EQ(
      Status::ErrorDataTooLarge(),
      Decrypt(CreateAesCbcAlgorithm(iv), GetTestAesCbcKey(), input, &output));
}

TEST_F(WebCryptoAesCbcTest, ExportKeyUnsupportedFormat) {
  std::vector<uint8_t> output;

  // Fail exporting the key in SPKI and PKCS#8 formats (not allowed for secret
  // keys).
  EXPECT_EQ(
      Status::ErrorUnsupportedExportKeyFormat(),
      ExportKey(blink::kWebCryptoKeyFormatSpki, GetTestAesCbcKey(), &output));
  EXPECT_EQ(
      Status::ErrorUnsupportedExportKeyFormat(),
      ExportKey(blink::kWebCryptoKeyFormatPkcs8, GetTestAesCbcKey(), &output));
}

// Tests importing of keys (in a variety of formats), errors during import,
// encryption, and decryption, using known answers.
TEST_F(WebCryptoAesCbcTest, KnownAnswerEncryptDecrypt) {
  base::Value::List tests = ReadJsonTestFileAsList("aes_cbc.json");
  for (const auto& test_value : tests) {
    SCOPED_TRACE(&test_value - &tests[0]);
    ASSERT_TRUE(test_value.is_dict());
    const base::DictionaryValue* test =
        &base::Value::AsDictionaryValue(test_value);

    blink::WebCryptoKeyFormat key_format = GetKeyFormatFromJsonTestCase(test);
    std::vector<uint8_t> key_data =
        GetKeyDataFromJsonTestCase(test, key_format);
    std::string import_error = "Success";
    test->GetString("import_error", &import_error);

    // Import the key.
    blink::WebCryptoKey key;
    Status status = ImportKey(
        key_format, key_data,
        CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc), true,
        blink::kWebCryptoKeyUsageEncrypt | blink::kWebCryptoKeyUsageDecrypt,
        &key);
    ASSERT_EQ(import_error, StatusToString(status));
    if (status.IsError())
      continue;

    // Test encryption.
    if (test->FindKey("plain_text")) {
      std::vector<uint8_t> test_plain_text =
          GetBytesFromHexString(test, "plain_text");

      std::vector<uint8_t> test_iv = GetBytesFromHexString(test, "iv");

      std::string encrypt_error = "Success";
      test->GetString("encrypt_error", &encrypt_error);

      std::vector<uint8_t> output;
      status = Encrypt(CreateAesCbcAlgorithm(test_iv), key, test_plain_text,
                       &output);
      ASSERT_EQ(encrypt_error, StatusToString(status));
      if (status.IsError())
        continue;

      std::vector<uint8_t> test_cipher_text =
          GetBytesFromHexString(test, "cipher_text");

      EXPECT_BYTES_EQ(test_cipher_text, output);
    }

    // Test decryption.
    if (test->FindKey("cipher_text")) {
      std::vector<uint8_t> test_cipher_text =
          GetBytesFromHexString(test, "cipher_text");

      std::vector<uint8_t> test_iv = GetBytesFromHexString(test, "iv");

      std::string decrypt_error = "Success";
      test->GetString("decrypt_error", &decrypt_error);

      std::vector<uint8_t> output;
      status = Decrypt(CreateAesCbcAlgorithm(test_iv), key, test_cipher_text,
                       &output);
      ASSERT_EQ(decrypt_error, StatusToString(status));
      if (status.IsError())
        continue;

      std::vector<uint8_t> test_plain_text =
          GetBytesFromHexString(test, "plain_text");

      EXPECT_BYTES_EQ(test_plain_text, output);
    }
  }
}

// TODO(eroman): Do this same test for AES-GCM, AES-KW, AES-CTR ?
TEST_F(WebCryptoAesCbcTest, GenerateKeyIsRandom) {
  // Check key generation for each allowed key length.
  std::vector<blink::WebCryptoAlgorithm> algorithm;
  for (uint16_t key_length : {128, 256}) {
    blink::WebCryptoKey key;

    std::vector<std::vector<uint8_t>> keys;
    std::vector<uint8_t> key_bytes;

    // Generate a small sample of keys.
    for (int j = 0; j < 16; ++j) {
      ASSERT_EQ(Status::Success(),
                GenerateSecretKey(CreateAesCbcKeyGenAlgorithm(key_length), true,
                                  blink::kWebCryptoKeyUsageEncrypt, &key));
      EXPECT_TRUE(key.Handle());
      EXPECT_EQ(blink::kWebCryptoKeyTypeSecret, key.GetType());
      ASSERT_EQ(Status::Success(),
                ExportKey(blink::kWebCryptoKeyFormatRaw, key, &key_bytes));
      EXPECT_EQ(key_bytes.size() * 8,
                key.Algorithm().AesParams()->LengthBits());
      keys.push_back(key_bytes);
    }
    // Ensure all entries in the key sample set are unique. This is a simplistic
    // estimate of whether the generated keys appear random.
    EXPECT_FALSE(CopiesExist(keys));
  }
}

TEST_F(WebCryptoAesCbcTest, GenerateKeyBadLength) {
  blink::WebCryptoKey key;
  for (uint16_t key_length : {0, 127, 257}) {
    SCOPED_TRACE(key_length);
    EXPECT_EQ(Status::ErrorGenerateAesKeyLength(),
              GenerateSecretKey(CreateAesCbcKeyGenAlgorithm(key_length), true,
                                blink::kWebCryptoKeyUsageEncrypt, &key));
  }
}

TEST_F(WebCryptoAesCbcTest, ImportKeyEmptyUsage) {
  blink::WebCryptoKey key;
  ASSERT_EQ(Status::ErrorCreateKeyEmptyUsages(),
            ImportKey(blink::kWebCryptoKeyFormatRaw, std::vector<uint8_t>(16),
                      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc), true,
                      0, &key));
}

// If key_ops is specified but empty, no key usages are allowed for the key.
TEST_F(WebCryptoAesCbcTest, ImportKeyJwkEmptyKeyOps) {
  blink::WebCryptoKey key;
  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetBoolean("ext", false);
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg");
  dict.SetKey("key_ops", base::ListValue());

  // The JWK does not contain encrypt usages.
  EXPECT_EQ(Status::ErrorJwkKeyopsInconsistent(),
            ImportKeyJwkFromDict(
                dict, CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc),
                false, blink::kWebCryptoKeyUsageEncrypt, &key));

  // The JWK does not contain sign usage (nor is it applicable).
  EXPECT_EQ(Status::ErrorCreateKeyBadUsages(),
            ImportKeyJwkFromDict(
                dict, CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc),
                false, blink::kWebCryptoKeyUsageSign, &key));
}

// If key_ops is missing, then any key usages can be specified.
TEST_F(WebCryptoAesCbcTest, ImportKeyJwkNoKeyOps) {
  blink::WebCryptoKey key;
  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg");

  EXPECT_EQ(Status::Success(),
            ImportKeyJwkFromDict(
                dict, CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc),
                false, blink::kWebCryptoKeyUsageEncrypt, &key));

  EXPECT_EQ(blink::kWebCryptoKeyUsageEncrypt, key.Usages());

  // The JWK does not contain sign usage (nor is it applicable).
  EXPECT_EQ(Status::ErrorCreateKeyBadUsages(),
            ImportKeyJwkFromDict(
                dict, CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc),
                false, blink::kWebCryptoKeyUsageVerify, &key));
}

TEST_F(WebCryptoAesCbcTest, ImportKeyJwkKeyOpsEncryptDecrypt) {
  blink::WebCryptoKey key;
  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg");
  base::Value* key_ops =
      dict.SetKey("key_ops", base::Value(base::Value::Type::LIST));

  key_ops->Append("encrypt");

  EXPECT_EQ(Status::Success(),
            ImportKeyJwkFromDict(
                dict, CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc),
                false, blink::kWebCryptoKeyUsageEncrypt, &key));

  EXPECT_EQ(blink::kWebCryptoKeyUsageEncrypt, key.Usages());

  key_ops->Append("decrypt");

  EXPECT_EQ(Status::Success(),
            ImportKeyJwkFromDict(
                dict, CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc),
                false, blink::kWebCryptoKeyUsageDecrypt, &key));

  EXPECT_EQ(blink::kWebCryptoKeyUsageDecrypt, key.Usages());

  EXPECT_EQ(
      Status::Success(),
      ImportKeyJwkFromDict(
          dict, CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc), false,
          blink::kWebCryptoKeyUsageDecrypt | blink::kWebCryptoKeyUsageEncrypt,
          &key));

  EXPECT_EQ(blink::kWebCryptoKeyUsageEncrypt | blink::kWebCryptoKeyUsageDecrypt,
            key.Usages());
}

// Test failure if input usage is NOT a strict subset of the JWK usage.
TEST_F(WebCryptoAesCbcTest, ImportKeyJwkKeyOpsNotSuperset) {
  blink::WebCryptoKey key;
  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg");
  base::ListValue key_ops;
  key_ops.Append("encrypt");
  dict.SetKey("key_ops", std::move(key_ops));

  EXPECT_EQ(
      Status::ErrorJwkKeyopsInconsistent(),
      ImportKeyJwkFromDict(
          dict, CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc), false,
          blink::kWebCryptoKeyUsageEncrypt | blink::kWebCryptoKeyUsageDecrypt,
          &key));
}

TEST_F(WebCryptoAesCbcTest, ImportKeyJwkUseEnc) {
  blink::WebCryptoKey key;
  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg");

  // Test JWK composite use 'enc' usage
  dict.SetString("alg", "A128CBC");
  dict.SetString("use", "enc");
  EXPECT_EQ(
      Status::Success(),
      ImportKeyJwkFromDict(
          dict, CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc), false,
          blink::kWebCryptoKeyUsageDecrypt | blink::kWebCryptoKeyUsageEncrypt |
              blink::kWebCryptoKeyUsageWrapKey |
              blink::kWebCryptoKeyUsageUnwrapKey,
          &key));
  EXPECT_EQ(
      blink::kWebCryptoKeyUsageDecrypt | blink::kWebCryptoKeyUsageEncrypt |
          blink::kWebCryptoKeyUsageWrapKey | blink::kWebCryptoKeyUsageUnwrapKey,
      key.Usages());
}

TEST_F(WebCryptoAesCbcTest, ImportJwkInvalidJson) {
  blink::WebCryptoKey key;
  // Fail on empty JSON.
  EXPECT_EQ(Status::ErrorJwkNotDictionary(),
            ImportKey(blink::kWebCryptoKeyFormatJwk, {},
                      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc),
                      false, blink::kWebCryptoKeyUsageEncrypt, &key));

  // Fail on invalid JSON.
  const std::string bad_json = R"({ "kty": "oct", "alg": "HS256", "use": )";
  EXPECT_EQ(Status::ErrorJwkNotDictionary(),
            ImportKey(blink::kWebCryptoKeyFormatJwk,
                      base::as_bytes(base::make_span(bad_json)),
                      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc),
                      false, blink::kWebCryptoKeyUsageEncrypt, &key));
}

// Fail on inconsistent key_ops - asking for "encrypt" however JWK contains
// only "foo".
TEST_F(WebCryptoAesCbcTest, ImportJwkKeyOpsLacksUsages) {
  blink::WebCryptoKey key;

  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg");

  base::ListValue key_ops;
  key_ops.Append("foo");
  dict.SetKey("key_ops", std::move(key_ops));
  EXPECT_EQ(Status::ErrorJwkKeyopsInconsistent(),
            ImportKeyJwkFromDict(
                dict, CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc),
                false, blink::kWebCryptoKeyUsageEncrypt, &key));
}

TEST_F(WebCryptoAesCbcTest, ImportExportJwk) {
  const blink::WebCryptoAlgorithm algorithm =
      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc);

  // AES-CBC 128
  ImportExportJwkSymmetricKey(
      128, algorithm,
      blink::kWebCryptoKeyUsageEncrypt | blink::kWebCryptoKeyUsageDecrypt,
      "A128CBC");

  // AES-CBC 256
  ImportExportJwkSymmetricKey(256, algorithm, blink::kWebCryptoKeyUsageDecrypt,
                              "A256CBC");

  // Large usage value
  ImportExportJwkSymmetricKey(
      256, algorithm,
      blink::kWebCryptoKeyUsageEncrypt | blink::kWebCryptoKeyUsageDecrypt |
          blink::kWebCryptoKeyUsageWrapKey | blink::kWebCryptoKeyUsageUnwrapKey,
      "A256CBC");
}

// 192-bit AES is intentionally unsupported (http://crbug.com/533699).
TEST_F(WebCryptoAesCbcTest, GenerateAesCbc192) {
  blink::WebCryptoKey key;
  Status status = GenerateSecretKey(CreateAesCbcKeyGenAlgorithm(192), true,
                                    blink::kWebCryptoKeyUsageEncrypt, &key);
  ASSERT_EQ(Status::ErrorAes192BitUnsupported(), status);
}

// 192-bit AES is intentionally unsupported (http://crbug.com/533699).
TEST_F(WebCryptoAesCbcTest, UnwrapAesCbc192) {
  std::vector<uint8_t> wrapping_key_data(16, 0);
  std::vector<uint8_t> wrapped_key = HexStringToBytes(
      "1A07ACAB6C906E50883173C29441DB1DE91D34F45C435B5F99C822867FB3956F");

  blink::WebCryptoKey wrapping_key = ImportSecretKeyFromRaw(
      wrapping_key_data, CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesKw),
      blink::kWebCryptoKeyUsageUnwrapKey);

  blink::WebCryptoKey unwrapped_key;
  ASSERT_EQ(Status::ErrorAes192BitUnsupported(),
            UnwrapKey(blink::kWebCryptoKeyFormatRaw, wrapped_key, wrapping_key,
                      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesKw),
                      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc), true,
                      blink::kWebCryptoKeyUsageEncrypt, &unwrapped_key));
}

// Try importing an AES-CBC key with unsupported key usages using raw
// format. AES-CBC keys support the following usages:
//   'encrypt', 'decrypt', 'wrapKey', 'unwrapKey'
TEST_F(WebCryptoAesCbcTest, ImportKeyBadUsage_Raw) {
  const blink::WebCryptoAlgorithm algorithm =
      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc);

  const blink::WebCryptoKeyUsageMask kBadUsages[] = {
      blink::kWebCryptoKeyUsageSign,
      blink::kWebCryptoKeyUsageSign | blink::kWebCryptoKeyUsageDecrypt,
      blink::kWebCryptoKeyUsageDeriveBits,
      blink::kWebCryptoKeyUsageUnwrapKey | blink::kWebCryptoKeyUsageVerify,
  };

  std::vector<uint8_t> key_bytes(16);

  for (auto usage : kBadUsages) {
    SCOPED_TRACE(usage);

    blink::WebCryptoKey key;
    ASSERT_EQ(Status::ErrorCreateKeyBadUsages(),
              ImportKey(blink::kWebCryptoKeyFormatRaw, key_bytes, algorithm,
                        true, usage, &key));
  }
}

// Generate an AES-CBC key with invalid usages. AES-CBC supports:
//   'encrypt', 'decrypt', 'wrapKey', 'unwrapKey'
TEST_F(WebCryptoAesCbcTest, GenerateKeyBadUsages) {
  const blink::WebCryptoKeyUsageMask kBadUsages[] = {
      blink::kWebCryptoKeyUsageSign,
      blink::kWebCryptoKeyUsageVerify,
      blink::kWebCryptoKeyUsageDecrypt | blink::kWebCryptoKeyUsageVerify,
  };

  for (auto usage : kBadUsages) {
    SCOPED_TRACE(usage);

    blink::WebCryptoKey key;

    ASSERT_EQ(
        Status::ErrorCreateKeyBadUsages(),
        GenerateSecretKey(CreateAesCbcKeyGenAlgorithm(128), true, usage, &key));
  }
}

// Generate an AES-CBC key with no usages.
TEST_F(WebCryptoAesCbcTest, GenerateKeyEmptyUsages) {
  blink::WebCryptoKey key;

  ASSERT_EQ(Status::ErrorCreateKeyEmptyUsages(),
            GenerateSecretKey(CreateAesCbcKeyGenAlgorithm(128), true, 0, &key));
}

// Generate an AES-CBC key and an RSA key pair. Use the AES-CBC key to wrap the
// key pair (using SPKI format for public key, PKCS8 format for private key).
// Then unwrap the wrapped key pair and verify that the key data is the same.
TEST_F(WebCryptoAesCbcTest, WrapUnwrapRoundtripSpkiPkcs8) {
  // Generate the wrapping key.
  blink::WebCryptoKey wrapping_key;
  ASSERT_EQ(Status::Success(),
            GenerateSecretKey(CreateAesCbcKeyGenAlgorithm(128), true,
                              blink::kWebCryptoKeyUsageWrapKey |
                                  blink::kWebCryptoKeyUsageUnwrapKey,
                              &wrapping_key));

  // Generate an RSA key pair to be wrapped.
  const unsigned int modulus_length = 256;
  const std::vector<uint8_t> public_exponent = HexStringToBytes("010001");

  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;
  ASSERT_EQ(Status::Success(),
            GenerateKeyPair(CreateRsaHashedKeyGenAlgorithm(
                                blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                                blink::kWebCryptoAlgorithmIdSha256,
                                modulus_length, public_exponent),
                            true, blink::kWebCryptoKeyUsageSign, &public_key,
                            &private_key));

  // Export key pair as SPKI + PKCS8
  std::vector<uint8_t> public_key_spki;
  ASSERT_EQ(Status::Success(), ExportKey(blink::kWebCryptoKeyFormatSpki,
                                         public_key, &public_key_spki));

  std::vector<uint8_t> private_key_pkcs8;
  ASSERT_EQ(Status::Success(), ExportKey(blink::kWebCryptoKeyFormatPkcs8,
                                         private_key, &private_key_pkcs8));

  // Wrap the key pair.
  blink::WebCryptoAlgorithm wrap_algorithm =
      CreateAesCbcAlgorithm(std::vector<uint8_t>(16, 0));

  std::vector<uint8_t> wrapped_public_key;
  ASSERT_EQ(Status::Success(),
            WrapKey(blink::kWebCryptoKeyFormatSpki, public_key, wrapping_key,
                    wrap_algorithm, &wrapped_public_key));

  std::vector<uint8_t> wrapped_private_key;
  ASSERT_EQ(Status::Success(),
            WrapKey(blink::kWebCryptoKeyFormatPkcs8, private_key, wrapping_key,
                    wrap_algorithm, &wrapped_private_key));

  // Unwrap the key pair.
  blink::WebCryptoAlgorithm rsa_import_algorithm =
      CreateRsaHashedImportAlgorithm(
          blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
          blink::kWebCryptoAlgorithmIdSha256);

  blink::WebCryptoKey unwrapped_public_key;

  ASSERT_EQ(Status::Success(),
            UnwrapKey(blink::kWebCryptoKeyFormatSpki, wrapped_public_key,
                      wrapping_key, wrap_algorithm, rsa_import_algorithm, true,
                      blink::kWebCryptoKeyUsageVerify, &unwrapped_public_key));

  blink::WebCryptoKey unwrapped_private_key;

  ASSERT_EQ(Status::Success(),
            UnwrapKey(blink::kWebCryptoKeyFormatPkcs8, wrapped_private_key,
                      wrapping_key, wrap_algorithm, rsa_import_algorithm, true,
                      blink::kWebCryptoKeyUsageSign, &unwrapped_private_key));

  // Export unwrapped key pair as SPKI + PKCS8
  std::vector<uint8_t> unwrapped_public_key_spki;
  ASSERT_EQ(Status::Success(),
            ExportKey(blink::kWebCryptoKeyFormatSpki, unwrapped_public_key,
                      &unwrapped_public_key_spki));

  std::vector<uint8_t> unwrapped_private_key_pkcs8;
  ASSERT_EQ(Status::Success(),
            ExportKey(blink::kWebCryptoKeyFormatPkcs8, unwrapped_private_key,
                      &unwrapped_private_key_pkcs8));

  EXPECT_EQ(public_key_spki, unwrapped_public_key_spki);
  EXPECT_EQ(private_key_pkcs8, unwrapped_private_key_pkcs8);

  EXPECT_NE(public_key_spki, wrapped_public_key);
  EXPECT_NE(private_key_pkcs8, wrapped_private_key);
}

}  // namespace

}  // namespace webcrypto
