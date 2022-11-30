// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/values.h"
#include "components/webcrypto/algorithm_dispatch.h"
#include "components/webcrypto/algorithms/test_helpers.h"
#include "components/webcrypto/status.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"

namespace webcrypto {

namespace {

blink::WebCryptoAlgorithm CreateAesKwKeyGenAlgorithm(uint16_t key_length_bits) {
  return CreateAesKeyGenAlgorithm(blink::kWebCryptoAlgorithmIdAesKw,
                                  key_length_bits);
}

class WebCryptoAesKwTest : public WebCryptoTestBase {};

struct AesKwKnownAnswer {
  const char* kek;
  const char* key;
  const char* ciphertext;
};

const AesKwKnownAnswer kAesKwKnownAnswers[] = {
    // AES-KW test vectors from http://www.ietf.org/rfc/rfc3394.txt
    // 4.1 Wrap 128 bits of Key Data with a 128-bit KEK
    {"000102030405060708090A0B0C0D0E0F", "00112233445566778899AABBCCDDEEFF",
     "1FA68B0A8112B447AEF34BD8FB5A7B829D3E862371D2CFE5"},
    // 4.3 Wrap 128 bits of Key Data with a 256-bit KEK
    {"000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F",
     "00112233445566778899AABBCCDDEEFF",
     "64E8C3F9CE0F5BA263E9777905818A2A93C8191E7D6E8AE7"},
    // 4.5 Wrap 192 bits of Key Data with a 256-bit KEK
    {"000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F",
     "00112233445566778899AABBCCDDEEFF0001020304050607",
     "A8F9BC1612C68B3FF6E6F4FBE30E71E4769C8B80A32CB8958CD5D17D6B254DA1"},
    // 4.6 Wrap 256 bits of Key Data with a 256-bit KEK
    {"000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F",
     "00112233445566778899AABBCCDDEEFF000102030405060708090A0B0C0D0E0F",
     "28C9F404C4B810F4CBCCB35CFB87F8263F5786E2D80ED326CBC7F0E71A99F43BFB988B9B7"
     "A02DD21"}};

TEST_F(WebCryptoAesKwTest, GenerateKeyBadLength) {
  blink::WebCryptoKey key;
  for (auto len : {0, 127, 257}) {
    SCOPED_TRACE(len);
    EXPECT_EQ(Status::ErrorGenerateAesKeyLength(),
              GenerateSecretKey(CreateAesKwKeyGenAlgorithm(len), true,
                                blink::kWebCryptoKeyUsageWrapKey, &key));
  }
}

TEST_F(WebCryptoAesKwTest, GenerateKeyEmptyUsage) {
  blink::WebCryptoKey key;
  EXPECT_EQ(Status::ErrorCreateKeyEmptyUsages(),
            GenerateSecretKey(CreateAesKwKeyGenAlgorithm(256), true, 0, &key));
}

TEST_F(WebCryptoAesKwTest, ImportKeyEmptyUsage) {
  blink::WebCryptoKey key;
  EXPECT_EQ(Status::ErrorCreateKeyEmptyUsages(),
            ImportKey(blink::kWebCryptoKeyFormatRaw, std::vector<uint8_t>(16),
                      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesKw), true,
                      0, &key));
}

TEST_F(WebCryptoAesKwTest, ImportKeyJwkKeyOpsWrapUnwrap) {
  blink::WebCryptoKey key;
  base::Value::Dict dict;
  dict.Set("kty", "oct");
  dict.Set("k", "GADWrMRHwQfoNaXU5fZvTg");
  auto& key_ops = dict.Set("key_ops", base::Value::List())->GetList();

  key_ops.Append("wrapKey");

  EXPECT_EQ(Status::Success(),
            ImportKeyJwkFromDict(
                dict, CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesKw), false,
                blink::kWebCryptoKeyUsageWrapKey, &key));

  EXPECT_EQ(blink::kWebCryptoKeyUsageWrapKey, key.Usages());

  key_ops.Append("unwrapKey");

  EXPECT_EQ(Status::Success(),
            ImportKeyJwkFromDict(
                dict, CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesKw), false,
                blink::kWebCryptoKeyUsageUnwrapKey, &key));

  EXPECT_EQ(blink::kWebCryptoKeyUsageUnwrapKey, key.Usages());
}

TEST_F(WebCryptoAesKwTest, ImportExportJwk) {
  const blink::WebCryptoAlgorithm algorithm =
      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesKw);

  // AES-KW 128
  ImportExportJwkSymmetricKey(
      128, algorithm,
      blink::kWebCryptoKeyUsageWrapKey | blink::kWebCryptoKeyUsageUnwrapKey,
      "A128KW");

  // AES-KW 256
  ImportExportJwkSymmetricKey(
      256, algorithm,
      blink::kWebCryptoKeyUsageWrapKey | blink::kWebCryptoKeyUsageUnwrapKey,
      "A256KW");
}

TEST_F(WebCryptoAesKwTest, AesKwKeyImport) {
  blink::WebCryptoKey key;
  blink::WebCryptoAlgorithm algorithm =
      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesKw);

  // Import a 128-bit Key Encryption Key (KEK)
  std::string key_raw_hex_in = "025a8cf3f08b4f6c5f33bbc76a471939";
  ASSERT_EQ(
      Status::Success(),
      ImportKey(blink::kWebCryptoKeyFormatRaw, HexStringToBytes(key_raw_hex_in),
                algorithm, true, blink::kWebCryptoKeyUsageWrapKey, &key));
  std::vector<uint8_t> key_raw_out;
  EXPECT_EQ(Status::Success(),
            ExportKey(blink::kWebCryptoKeyFormatRaw, key, &key_raw_out));
  EXPECT_BYTES_EQ_HEX(key_raw_hex_in, key_raw_out);

  // Import a 192-bit KEK
  key_raw_hex_in = "c0192c6466b2370decbb62b2cfef4384544ffeb4d2fbc103";
  ASSERT_EQ(
      Status::ErrorAes192BitUnsupported(),
      ImportKey(blink::kWebCryptoKeyFormatRaw, HexStringToBytes(key_raw_hex_in),
                algorithm, true, blink::kWebCryptoKeyUsageWrapKey, &key));

  // Import a 256-bit Key Encryption Key (KEK)
  key_raw_hex_in =
      "e11fe66380d90fa9ebefb74e0478e78f95664d0c67ca20ce4a0b5842863ac46f";
  ASSERT_EQ(
      Status::Success(),
      ImportKey(blink::kWebCryptoKeyFormatRaw, HexStringToBytes(key_raw_hex_in),
                algorithm, true, blink::kWebCryptoKeyUsageWrapKey, &key));
  EXPECT_EQ(Status::Success(),
            ExportKey(blink::kWebCryptoKeyFormatRaw, key, &key_raw_out));
  EXPECT_BYTES_EQ_HEX(key_raw_hex_in, key_raw_out);

  // Fail import of 0 length key
  EXPECT_EQ(Status::ErrorImportAesKeyLength(),
            ImportKey(blink::kWebCryptoKeyFormatRaw, HexStringToBytes(""),
                      algorithm, true, blink::kWebCryptoKeyUsageWrapKey, &key));

  // Fail import of 120-bit KEK
  key_raw_hex_in = "3e4566a2bdaa10cb68134fa66c15dd";
  EXPECT_EQ(
      Status::ErrorImportAesKeyLength(),
      ImportKey(blink::kWebCryptoKeyFormatRaw, HexStringToBytes(key_raw_hex_in),
                algorithm, true, blink::kWebCryptoKeyUsageWrapKey, &key));

  // Fail import of 200-bit KEK
  key_raw_hex_in = "0a1d88608a5ad9fec64f1ada269ebab4baa2feeb8d95638c0e";
  EXPECT_EQ(
      Status::ErrorImportAesKeyLength(),
      ImportKey(blink::kWebCryptoKeyFormatRaw, HexStringToBytes(key_raw_hex_in),
                algorithm, true, blink::kWebCryptoKeyUsageWrapKey, &key));
}

TEST_F(WebCryptoAesKwTest, UnwrapFailures) {
  const auto& test = kAesKwKnownAnswers[0];
  const auto test_kek = HexStringToBytes(test.kek);
  const auto test_ciphertext = HexStringToBytes(test.ciphertext);

  blink::WebCryptoKey unwrapped_key;

  // Using a wrapping algorithm that does not match the wrapping key algorithm
  // should fail.
  blink::WebCryptoKey wrapping_key = ImportSecretKeyFromRaw(
      test_kek, CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesKw),
      blink::kWebCryptoKeyUsageUnwrapKey);
  EXPECT_EQ(
      Status::ErrorUnexpected(),
      UnwrapKey(blink::kWebCryptoKeyFormatRaw, test_ciphertext, wrapping_key,
                CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc),
                CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc), true,
                blink::kWebCryptoKeyUsageEncrypt, &unwrapped_key));
}

TEST_F(WebCryptoAesKwTest, AesKwRawSymkeyWrapUnwrapKnownAnswer) {
  for (const auto& test : kAesKwKnownAnswers) {
    const auto test_kek = HexStringToBytes(test.kek);
    const auto test_key = HexStringToBytes(test.key);
    const auto test_ciphertext = HexStringToBytes(test.ciphertext);
    const blink::WebCryptoAlgorithm wrapping_algorithm =
        CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesKw);

    // Import the wrapping key.
    blink::WebCryptoKey wrapping_key = ImportSecretKeyFromRaw(
        test_kek, wrapping_algorithm,
        blink::kWebCryptoKeyUsageWrapKey | blink::kWebCryptoKeyUsageUnwrapKey);

    // Import the key to be wrapped.
    blink::WebCryptoKey key = ImportSecretKeyFromRaw(
        test_key,
        CreateHmacImportAlgorithmNoLength(blink::kWebCryptoAlgorithmIdSha1),
        blink::kWebCryptoKeyUsageSign);

    // Wrap the key and verify the ciphertext result against the known answer.
    std::vector<uint8_t> wrapped_key;
    ASSERT_EQ(Status::Success(),
              WrapKey(blink::kWebCryptoKeyFormatRaw, key, wrapping_key,
                      wrapping_algorithm, &wrapped_key));
    EXPECT_BYTES_EQ(test_ciphertext, wrapped_key);

    // Unwrap the known ciphertext to get a new test_key.
    blink::WebCryptoKey unwrapped_key;
    ASSERT_EQ(Status::Success(),
              UnwrapKey(blink::kWebCryptoKeyFormatRaw, test_ciphertext,
                        wrapping_key, wrapping_algorithm,
                        CreateHmacImportAlgorithmNoLength(
                            blink::kWebCryptoAlgorithmIdSha1),
                        true, blink::kWebCryptoKeyUsageSign, &unwrapped_key));
    EXPECT_FALSE(key.IsNull());
    EXPECT_TRUE(key.Handle());
    EXPECT_EQ(blink::kWebCryptoKeyTypeSecret, key.GetType());
    EXPECT_EQ(blink::kWebCryptoAlgorithmIdHmac, key.Algorithm().Id());
    EXPECT_EQ(true, key.Extractable());
    EXPECT_EQ(blink::kWebCryptoKeyUsageSign, key.Usages());

    // Export the new key and compare its raw bytes with the original known key.
    std::vector<uint8_t> raw_key;
    EXPECT_EQ(Status::Success(), ExportKey(blink::kWebCryptoKeyFormatRaw,
                                           unwrapped_key, &raw_key));
    EXPECT_BYTES_EQ(test_key, raw_key);
  }
}

// Unwrap a HMAC key using AES-KW, and then try doing a sign/verify with the
// unwrapped key
TEST_F(WebCryptoAesKwTest, AesKwRawSymkeyUnwrapSignVerifyHmac) {
  const auto& test = kAesKwKnownAnswers[0];
  const auto test_kek = HexStringToBytes(test.kek);
  const auto test_ciphertext = HexStringToBytes(test.ciphertext);
  const blink::WebCryptoAlgorithm wrapping_algorithm =
      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesKw);

  // Import the wrapping key.
  blink::WebCryptoKey wrapping_key = ImportSecretKeyFromRaw(
      test_kek, wrapping_algorithm, blink::kWebCryptoKeyUsageUnwrapKey);

  // Unwrap the known ciphertext.
  blink::WebCryptoKey key;
  ASSERT_EQ(
      Status::Success(),
      UnwrapKey(
          blink::kWebCryptoKeyFormatRaw, test_ciphertext, wrapping_key,
          wrapping_algorithm,
          CreateHmacImportAlgorithmNoLength(blink::kWebCryptoAlgorithmIdSha1),
          false,
          blink::kWebCryptoKeyUsageSign | blink::kWebCryptoKeyUsageVerify,
          &key));

  EXPECT_EQ(blink::kWebCryptoKeyTypeSecret, key.GetType());
  EXPECT_EQ(blink::kWebCryptoAlgorithmIdHmac, key.Algorithm().Id());
  EXPECT_FALSE(key.Extractable());
  EXPECT_EQ(blink::kWebCryptoKeyUsageSign | blink::kWebCryptoKeyUsageVerify,
            key.Usages());

  // Sign an empty message and ensure it is verified.
  std::vector<uint8_t> test_message;
  std::vector<uint8_t> signature;

  ASSERT_EQ(Status::Success(),
            Sign(CreateAlgorithm(blink::kWebCryptoAlgorithmIdHmac), key,
                 test_message, &signature));

  EXPECT_GT(signature.size(), 0u);

  bool verify_result;
  ASSERT_EQ(Status::Success(),
            Verify(CreateAlgorithm(blink::kWebCryptoAlgorithmIdHmac), key,
                   signature, test_message, &verify_result));
}

TEST_F(WebCryptoAesKwTest, AesKwRawSymkeyWrapUnwrapErrors) {
  // Use 256 bits of data with a 256-bit KEK
  const auto& test = kAesKwKnownAnswers[3];
  const auto test_kek = HexStringToBytes(test.kek);
  const auto test_key = HexStringToBytes(test.key);
  const auto test_ciphertext = HexStringToBytes(test.ciphertext);

  const blink::WebCryptoAlgorithm wrapping_algorithm =
      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesKw);
  const blink::WebCryptoAlgorithm key_algorithm =
      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc);
  // Import the wrapping key.
  blink::WebCryptoKey wrapping_key = ImportSecretKeyFromRaw(
      test_kek, wrapping_algorithm,
      blink::kWebCryptoKeyUsageWrapKey | blink::kWebCryptoKeyUsageUnwrapKey);
  // Import the key to be wrapped.
  blink::WebCryptoKey key = ImportSecretKeyFromRaw(
      test_key, CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc),
      blink::kWebCryptoKeyUsageEncrypt);

  // Unwrap with wrapped data too small must fail.
  const std::vector<uint8_t> small_data(test_ciphertext.begin(),
                                        test_ciphertext.begin() + 23);
  blink::WebCryptoKey unwrapped_key;
  EXPECT_EQ(Status::ErrorDataTooSmall(),
            UnwrapKey(blink::kWebCryptoKeyFormatRaw, small_data, wrapping_key,
                      wrapping_algorithm, key_algorithm, true,
                      blink::kWebCryptoKeyUsageEncrypt, &unwrapped_key));

  // Unwrap with wrapped data size not a multiple of 8 bytes must fail.
  const std::vector<uint8_t> unaligned_data(test_ciphertext.begin(),
                                            test_ciphertext.end() - 2);
  EXPECT_EQ(Status::ErrorInvalidAesKwDataLength(),
            UnwrapKey(blink::kWebCryptoKeyFormatRaw, unaligned_data,
                      wrapping_key, wrapping_algorithm, key_algorithm, true,
                      blink::kWebCryptoKeyUsageEncrypt, &unwrapped_key));
}

TEST_F(WebCryptoAesKwTest, AesKwRawSymkeyUnwrapCorruptData) {
  // Use 256 bits of data with a 256-bit KEK
  const auto& test = kAesKwKnownAnswers[3];
  const auto test_kek = HexStringToBytes(test.kek);
  const auto test_key = HexStringToBytes(test.key);
  const auto test_ciphertext = HexStringToBytes(test.ciphertext);
  const blink::WebCryptoAlgorithm wrapping_algorithm =
      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesKw);

  // Import the wrapping key.
  blink::WebCryptoKey wrapping_key = ImportSecretKeyFromRaw(
      test_kek, wrapping_algorithm,
      blink::kWebCryptoKeyUsageWrapKey | blink::kWebCryptoKeyUsageUnwrapKey);

  // Unwrap of a corrupted version of the known ciphertext should fail, due to
  // AES-KW's built-in integrity check.
  blink::WebCryptoKey unwrapped_key;
  EXPECT_EQ(Status::OperationError(),
            UnwrapKey(blink::kWebCryptoKeyFormatRaw, Corrupted(test_ciphertext),
                      wrapping_key, wrapping_algorithm,
                      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc), true,
                      blink::kWebCryptoKeyUsageEncrypt, &unwrapped_key));
}

TEST_F(WebCryptoAesKwTest, AesKwJwkSymkeyUnwrapKnownData) {
  // The following data lists a known HMAC SHA-256 key, then a JWK
  // representation of this key which was encrypted ("wrapped") using AES-KW and
  // the following wrapping key.
  // For reference, the intermediate clear JWK is
  // {"alg":"HS256","ext":true,"k":<b64urlKey>,"key_ops":["verify"],"kty":"oct"}
  // (Not shown is space padding to ensure the cleartext meets the size
  // requirements of the AES-KW algorithm.)
  const std::vector<uint8_t> key_data = HexStringToBytes(
      "000102030405060708090A0B0C0D0E0F000102030405060708090A0B0C0D0E0F");
  const std::vector<uint8_t> wrapped_key_data = HexStringToBytes(
      "14E6380B35FDC5B72E1994764B6CB7BFDD64E7832894356AAEE6C3768FC3D0F115E6B0"
      "6729756225F999AA99FDF81FD6A359F1576D3D23DE6CB69C3937054EB497AC1E8C38D5"
      "5E01B9783A20C8D930020932CF25926103002213D0FC37279888154FEBCEDF31832158"
      "97938C5CFE5B10B4254D0C399F39D0");
  const std::vector<uint8_t> wrapping_key_data =
      HexStringToBytes("000102030405060708090A0B0C0D0E0F");
  const blink::WebCryptoAlgorithm wrapping_algorithm =
      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesKw);

  // Import the wrapping key.
  blink::WebCryptoKey wrapping_key =
      ImportSecretKeyFromRaw(wrapping_key_data, wrapping_algorithm,
                             blink::kWebCryptoKeyUsageUnwrapKey);

  // Unwrap the known wrapped key data to produce a new key
  blink::WebCryptoKey unwrapped_key;
  ASSERT_EQ(Status::Success(),
            UnwrapKey(blink::kWebCryptoKeyFormatJwk, wrapped_key_data,
                      wrapping_key, wrapping_algorithm,
                      CreateHmacImportAlgorithmNoLength(
                          blink::kWebCryptoAlgorithmIdSha256),
                      true, blink::kWebCryptoKeyUsageVerify, &unwrapped_key));

  // Validate the new key's attributes.
  EXPECT_FALSE(unwrapped_key.IsNull());
  EXPECT_TRUE(unwrapped_key.Handle());
  EXPECT_EQ(blink::kWebCryptoKeyTypeSecret, unwrapped_key.GetType());
  EXPECT_EQ(blink::kWebCryptoAlgorithmIdHmac, unwrapped_key.Algorithm().Id());
  EXPECT_EQ(blink::kWebCryptoAlgorithmIdSha256,
            unwrapped_key.Algorithm().HmacParams()->GetHash().Id());
  EXPECT_EQ(256u, unwrapped_key.Algorithm().HmacParams()->LengthBits());
  EXPECT_EQ(true, unwrapped_key.Extractable());
  EXPECT_EQ(blink::kWebCryptoKeyUsageVerify, unwrapped_key.Usages());

  // Export the new key's raw data and compare to the known original.
  std::vector<uint8_t> raw_key;
  EXPECT_EQ(Status::Success(),
            ExportKey(blink::kWebCryptoKeyFormatRaw, unwrapped_key, &raw_key));
  EXPECT_BYTES_EQ(key_data, raw_key);
}

// Try importing an AES-KW key with unsupported key usages using raw
// format. AES-KW keys support the following usages:
//   'wrapKey', 'unwrapKey'
TEST_F(WebCryptoAesKwTest, ImportKeyBadUsage_Raw) {
  const blink::WebCryptoAlgorithm algorithm =
      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesKw);

  const blink::WebCryptoKeyUsageMask kBadUsages[] = {
      blink::kWebCryptoKeyUsageEncrypt,
      blink::kWebCryptoKeyUsageDecrypt,
      blink::kWebCryptoKeyUsageSign,
      blink::kWebCryptoKeyUsageSign | blink::kWebCryptoKeyUsageUnwrapKey,
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

// Try unwrapping an HMAC key with unsupported usages using JWK format and
// AES-KW. HMAC keys support the following usages:
//   'sign', 'verify'
TEST_F(WebCryptoAesKwTest, UnwrapHmacKeyBadUsage_JWK) {
  const blink::WebCryptoAlgorithm unwrap_algorithm =
      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesKw);

  const blink::WebCryptoKeyUsageMask kBadUsages[] = {
      blink::kWebCryptoKeyUsageEncrypt,
      blink::kWebCryptoKeyUsageDecrypt,
      blink::kWebCryptoKeyUsageWrapKey,
      blink::kWebCryptoKeyUsageSign | blink::kWebCryptoKeyUsageWrapKey,
      blink::kWebCryptoKeyUsageVerify | blink::kWebCryptoKeyUsageDeriveKey,
  };

  // Import the wrapping key.
  blink::WebCryptoKey wrapping_key;
  ASSERT_EQ(Status::Success(),
            ImportKey(blink::kWebCryptoKeyFormatRaw, std::vector<uint8_t>(16),
                      unwrap_algorithm, true,
                      blink::kWebCryptoKeyUsageUnwrapKey, &wrapping_key));

  // The JWK plain text is:
  //   {"kty":"oct","alg":"HS256","k":"GADWrMRHwQfoNaXU5fZvTg"}
  const char* kWrappedJwk =
      "C2B7F19A32EE31372CD40C9C969B8CD67553E5AEA7FD1144874584E46ABCD79FDC308848"
      "B2DD8BD36A2D61062B9C5B8B499B8D6EF8EB320D87A614952B4EE771";

  for (auto usage : kBadUsages) {
    SCOPED_TRACE(usage);

    blink::WebCryptoKey key;

    ASSERT_EQ(
        Status::ErrorCreateKeyBadUsages(),
        UnwrapKey(blink::kWebCryptoKeyFormatJwk, HexStringToBytes(kWrappedJwk),
                  wrapping_key, unwrap_algorithm,
                  CreateHmacImportAlgorithmNoLength(
                      blink::kWebCryptoAlgorithmIdSha256),
                  true, usage, &key));
  }
}

// Try unwrapping an RSA-SSA public key with unsupported usages using JWK format
// and AES-KW. RSA-SSA public keys support the following usages:
//   'verify'
TEST_F(WebCryptoAesKwTest, UnwrapRsaSsaPublicKeyBadUsage_JWK) {
  const blink::WebCryptoAlgorithm unwrap_algorithm =
      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesKw);

  const blink::WebCryptoKeyUsageMask kBadUsages[] = {
      blink::kWebCryptoKeyUsageEncrypt,
      blink::kWebCryptoKeyUsageSign,
      blink::kWebCryptoKeyUsageDecrypt,
      blink::kWebCryptoKeyUsageWrapKey,
      blink::kWebCryptoKeyUsageSign | blink::kWebCryptoKeyUsageWrapKey,
  };

  // Import the wrapping key.
  blink::WebCryptoKey wrapping_key;
  ASSERT_EQ(Status::Success(),
            ImportKey(blink::kWebCryptoKeyFormatRaw, std::vector<uint8_t>(16),
                      unwrap_algorithm, true,
                      blink::kWebCryptoKeyUsageUnwrapKey, &wrapping_key));

  // The JWK plaintext is:
  // {    "kty": "RSA","alg": "RS256","n": "...","e": "AQAB"}

  const char* kWrappedJwk =
      "CE8DAEF99E977EE58958B8C4494755C846E883B2ECA575C5366622839AF71AB30875F152"
      "E8E33E15A7817A3A2874EB53EFE05C774D98BC936BA9BA29BEB8BB3F3C3CE2323CB3359D"
      "E3F426605CF95CCF0E01E870ABD7E35F62E030B5FB6E520A5885514D1D850FB64B57806D"
      "1ADA57C6E27DF345D8292D80F6B074F1BE51C4CF3D76ECC8886218551308681B44FAC60B"
      "8CF6EA439BC63239103D0AE81ADB96F908680586C6169284E32EB7DD09D31103EBDAC0C2"
      "40C72DCF0AEA454113CC47457B13305B25507CBEAB9BDC8D8E0F867F9167F9DCEF0D9F9B"
      "30F2EE83CEDFD51136852C8A5939B768";

  for (auto usage : kBadUsages) {
    SCOPED_TRACE(usage);

    blink::WebCryptoKey key;

    ASSERT_EQ(
        Status::ErrorCreateKeyBadUsages(),
        UnwrapKey(blink::kWebCryptoKeyFormatJwk, HexStringToBytes(kWrappedJwk),
                  wrapping_key, unwrap_algorithm,
                  CreateRsaHashedImportAlgorithm(
                      blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                      blink::kWebCryptoAlgorithmIdSha256),
                  true, usage, &key));
  }
}

}  // namespace

}  // namespace webcrypto
