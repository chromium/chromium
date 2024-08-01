// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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

blink::WebCryptoKey ImportRawKey(const std::vector<uint8_t>& key_bytes) {
  return ImportSecretKeyFromRaw(
      key_bytes, CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc),
      blink::kWebCryptoKeyUsageEncrypt | blink::kWebCryptoKeyUsageDecrypt);
}

std::vector<uint8_t> EncryptOrDie(blink::WebCryptoKey key,
                                  const std::vector<uint8_t>& iv,
                                  const std::vector<uint8_t>& plaintext) {
  std::vector<uint8_t> output;
  Status status = Encrypt(CreateAesCbcAlgorithm(iv), key, plaintext, &output);
  CHECK(status.IsSuccess());
  return output;
}

std::vector<uint8_t> DecryptOrDie(blink::WebCryptoKey key,
                                  const std::vector<uint8_t>& iv,
                                  const std::vector<uint8_t>& ciphertext) {
  std::vector<uint8_t> output;
  Status status = Decrypt(CreateAesCbcAlgorithm(iv), key, ciphertext, &output);
  CHECK(status.IsSuccess());
  return output;
}

std::string EncryptMustFail(blink::WebCryptoKey key,
                            const std::vector<uint8_t>& iv,
                            const std::vector<uint8_t>& plaintext) {
  std::vector<uint8_t> output;
  Status status = Encrypt(CreateAesCbcAlgorithm(iv), key, plaintext, &output);
  CHECK(!status.IsSuccess());
  return StatusToString(status);
}

std::string DecryptMustFail(blink::WebCryptoKey key,
                            const std::vector<uint8_t>& iv,
                            const std::vector<uint8_t>& ciphertext) {
  std::vector<uint8_t> output;
  Status status = Decrypt(CreateAesCbcAlgorithm(iv), key, ciphertext, &output);
  CHECK(!status.IsSuccess());
  return StatusToString(status);
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
  base::span<const uint8_t> input(iv.data(), size_t{INT_MAX} - 3);

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

struct AesCbcKnownAnswer {
  const char* key;
  const char* iv;
  const char* plaintext;
  const char* ciphertext;
};

const AesCbcKnownAnswer kAesCbcKnownAnswers[] = {
    // F.2.1 (CBC-AES128.Encrypt)
    // http://csrc.nist.gov/publications/nistpubs/800-38a/sp800-38a.pdf
    {"2b7e151628aed2a6abf7158809cf4f3c", "000102030405060708090a0b0c0d0e0f",
     "6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e5130c81c46a"
     "35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710",
     // Added a padding block: encryption of {0x10, 0x10, ... 0x10}) (not given
     // by the NIST test vector)
     "7649abac8119b246cee98e9b12e9197d5086cb9b507219ee95db113a917678b273bed6b8e"
     "3c1743b7116e69e222295163ff1caa1681fac09120eca307586e1a78cb82807230e1321d3"
     "fae00d18cc2012"},

    // F.2.6 CBC-AES256.Decrypt [*]
    // http://csrc.nist.gov/publications/nistpubs/800-38a/sp800-38a.pdf
    //
    // [*] Truncated 3 bytes off the plain text, so block 4 differs from the
    // NIST vector.
    {"603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4",
     "000102030405060708090a0b0c0d0e0f",
     // Truncated the last block to make it more interesting.
     "6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e5130c81c46a"
     "35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be6",
     // Last block differs from source vector (due to truncation)
     "f58c4c04d6e5f1ba779eabfb5f7bfbd69cfc4e967edb808d679f777bc6702c7d39f23369a"
     "9d9bacfa530e26304231461c9aaf02a6a54e9e242ccbf48c59daca6"},

    // Taken from encryptor_unittest.cc (EncryptorTest.EmptyEncrypt())
    {"3132383d5369787465656e4279746573", "5377656574205369787465656e204956", "",
     "8518b8878d34e7185e300d0fcc426396"}};

TEST_F(WebCryptoAesCbcTest, KnownAnswers) {
  for (const auto& test : kAesCbcKnownAnswers) {
    SCOPED_TRACE(&test - &kAesCbcKnownAnswers[0]);
    auto key = HexStringToBytes(test.key);
    auto iv = HexStringToBytes(test.iv);
    auto plaintext = HexStringToBytes(test.plaintext);
    auto ciphertext = HexStringToBytes(test.ciphertext);

    blink::WebCryptoKey imported_key = ImportRawKey(key);
    EXPECT_EQ(EncryptOrDie(imported_key, iv, plaintext), ciphertext);
    EXPECT_EQ(DecryptOrDie(imported_key, iv, ciphertext), plaintext);
  }
}

TEST_F(WebCryptoAesCbcTest, IVIsWrongSize) {
  auto key = ImportRawKey(HexStringToBytes("3132383d5369787465656e4279746573"));
  auto plaintext = HexStringToBytes("0000");
  auto ciphertext = HexStringToBytes("8518b8878d34e7185e300d0fcc426396");

  auto short_iv = HexStringToBytes("5300");
  auto long_iv =
      HexStringToBytes("5377656574205369787465656e2049560000000000000000");

  const std::string error =
      "OperationError: The \"iv\" has an unexpected length -- must be 16 bytes";

  EXPECT_EQ(EncryptMustFail(key, short_iv, plaintext), error);
  EXPECT_EQ(EncryptMustFail(key, long_iv, plaintext), error);

  EXPECT_EQ(DecryptMustFail(key, short_iv, ciphertext), error);
  EXPECT_EQ(DecryptMustFail(key, long_iv, ciphertext), error);
}

TEST_F(WebCryptoAesCbcTest, ImportShortKey) {
  auto algo = CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc);
  auto key_bytes = HexStringToBytes("31");
  blink::WebCryptoKey unused_key;
  Status status = ImportKey(
      blink::kWebCryptoKeyFormatRaw, key_bytes, algo, true,
      blink::kWebCryptoKeyUsageEncrypt | blink::kWebCryptoKeyUsageDecrypt,
      &unused_key);
  EXPECT_EQ(StatusToString(status),
            "DataError: AES key data must be 128 or 256 bits");
}

TEST_F(WebCryptoAesCbcTest, ImportKeyWrongFormat) {
  auto algo = CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc);
  auto key_bytes = HexStringToBytes("3132383d5369787465656e4279746573");

  auto try_format = [=](blink::WebCryptoKeyFormat format) {
    blink::WebCryptoKey unused_key;
    Status status = ImportKey(
        format, key_bytes, algo, true,
        blink::kWebCryptoKeyUsageEncrypt | blink::kWebCryptoKeyUsageDecrypt,
        &unused_key);
    EXPECT_EQ(StatusToString(status),
              "NotSupported: Unsupported import key format for algorithm");
  };

  try_format(blink::kWebCryptoKeyFormatSpki);
  try_format(blink::kWebCryptoKeyFormatPkcs8);
}

TEST_F(WebCryptoAesCbcTest, ImportAes192Key) {
  auto algo = CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc);
  auto key_bytes =
      HexStringToBytes("5377656574205369787465656e2049560000000000000000");
  blink::WebCryptoKey unused_key;
  Status status = ImportKey(
      blink::kWebCryptoKeyFormatRaw, key_bytes, algo, true,
      blink::kWebCryptoKeyUsageEncrypt | blink::kWebCryptoKeyUsageDecrypt,
      &unused_key);
  EXPECT_EQ(StatusToString(status),
            "OperationError: 192-bit AES keys are not supported");
}

TEST_F(WebCryptoAesCbcTest, DecryptTruncatedCiphertextFails) {
  auto key = ImportRawKey(HexStringToBytes("2b7e151628aed2a6abf7158809cf4f3c"));
  auto iv = HexStringToBytes("000102030405060708090a0b0c0d0e0f");

  std::vector<uint8_t> plaintext(64, 0);
  auto ciphertext = EncryptOrDie(key, iv, plaintext);

  ASSERT_EQ(plaintext, DecryptOrDie(key, iv, ciphertext));

  // Drop 3 bytes at the end:
  ciphertext.resize(ciphertext.size() - 3);
  EXPECT_EQ(DecryptMustFail(key, iv, ciphertext), "OperationError");

  // Drop the rest of the trailing block:
  ciphertext.resize(ciphertext.size() - 13);
  EXPECT_EQ(DecryptMustFail(key, iv, ciphertext), "OperationError");

  // And try an empty ciphertext:
  ciphertext.clear();
  EXPECT_EQ(DecryptMustFail(key, iv, ciphertext), "OperationError");
}

struct JwkImportFailureTest {
  const char* jwk;
  const char* error;
};

const JwkImportFailureTest kJwkImportFailureTests[] = {
    {R"({
        "kty": "oct",
        "k": "GADWrMRHwQfoNaXU5fZvTg",
        "key_ops": [ "encrypt", "decrypt", "encrypt" ]
      })",
     "DataError: The \"key_ops\" member of the JWK dictionary contains "
     "duplicate usages."},
    {R"({
        "kty": "oct",
        "k": "GADWrMRHwQfoNaXU5fZvTg",
        "key_ops": [ "foopy", "decrypt", "foopy" ]
    })",
     "DataError: The \"key_ops\" member of the JWK dictionary contains "
     "duplicate usages."},
    {R"({
        "kty": "oct",
        "alg": "A127CBC",
        "k": "GADWrMRHwQfoNaXU5fZvTg"
    })",
     "DataError: The JWK \"alg\" member was inconsistent with that specified "
     "by the Web Crypto call"},
    {R"({
        "kty": "foo",
        "k": "GADWrMRHwQfoNaXU5fZvTg"
      })",
     "DataError: The JWK \"kty\" member was not \"oct\""},
    {R"({
        "k": "GADWrMRHwQfoNaXU5fZvTg"
      })",
     "DataError: The required JWK member \"kty\" was missing"},
    {R"({
        "kty": 0.1,
        "k": "GADWrMRHwQfoNaXU5fZvTg"
      })",
     "DataError: The JWK member \"kty\" must be a string"},
    {R"({
        "kty": "oct",
        "use": "foo",
        "k": "GADWrMRHwQfoNaXU5fZvTg"
      })",
     "DataError: The JWK \"use\" member could not be parsed"},
    {R"({
        "kty": "oct",
        "use": true,
        "k": "GADWrMRHwQfoNaXU5fZvTg"
      })",
     "DataError: The JWK member \"use\" must be a string"},
    {R"({
        "kty": "oct",
        "k": "GADWrMRHwQfoNaXU5fZvTg",
        "ext": 0
      })",
     "DataError: The JWK member \"ext\" must be a boolean"},
    {R"({
        "kty": "oct",
        "k": "GADWrMRHwQfoNaXU5fZvTg",
        "key_ops": true
      })",
     "DataError: The JWK member \"key_ops\" must be a list"},
    {R"({
        "kty": "oct",
        "k": "GADWrMRHwQfoNaXU5fZvTg",
        "key_ops": ["encrypt", 3]
      })",
     "DataError: The JWK member \"key_ops[1]\" must be a string"},
    {R"({
        "kty": "oct"
      })",
     "DataError: The required JWK member \"k\" was missing"},
    {R"({
        "kty": "oct",
        "k": "Qk3f0DsytU8lfza2au #$% Htaw2xpop9GYyTuH0p5GghxTI="
      })",
     "DataError: The JWK member \"k\" could not be base64url decoded or "
     "contained padding"},
    {R"({
        "kty": "oct",
        "k": ""
      })",
     "DataError: AES key data must be 128 or 256 bits"},
    {R"({
        "kty": "oct",
        "alg": "A128CBC",
        "k": ""
      })",
     "DataError: The JWK \"k\" member did not include the right length of key "
     "data for the given algorithm."},
    {R"({
        "kty": "oct",
        "alg": "A128CBC",
        "k": "AVj42h0Y5aqGtE3yluKL"
      })",
     "DataError: The JWK \"k\" member did not include the right length of key "
     "data for the given algorithm."},
    {R"({
        "kty": "oct",
        "alg": "A128CBC",
        "k": "dGhpcyAgaXMgIDI0ICBieXRlcyBsb25n"
      })",
     "DataError: The JWK \"k\" member did not include the right length of key "
     "data for the given algorithm."},
    {R"({
        "kty": "oct",
        "alg": "A192CBC",
        "k": "YWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFh"
      })",
     "OperationError: 192-bit AES keys are not supported"},
};

TEST_F(WebCryptoAesCbcTest, JwkImportFailures) {
  for (const auto& test : kJwkImportFailureTests) {
    blink::WebCryptoKey unused_key;
    std::string jwk = test.jwk;
    std::vector<uint8_t> jwk_bytes(jwk.begin(), jwk.end());
    Status status = ImportKey(
        blink::kWebCryptoKeyFormatJwk, jwk_bytes,
        CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc), true,
        blink::kWebCryptoKeyUsageEncrypt | blink::kWebCryptoKeyUsageDecrypt,
        &unused_key);
    ASSERT_TRUE(!status.IsSuccess());
    EXPECT_EQ(StatusToString(status), test.error);
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
  base::Value::Dict dict;
  dict.Set("kty", "oct");
  dict.Set("ext", false);
  dict.Set("k", "GADWrMRHwQfoNaXU5fZvTg");
  dict.Set("key_ops", base::Value::List());

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
  base::Value::Dict dict;
  dict.Set("kty", "oct");
  dict.Set("k", "GADWrMRHwQfoNaXU5fZvTg");

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
  base::Value::Dict dict;
  dict.Set("kty", "oct");
  dict.Set("k", "GADWrMRHwQfoNaXU5fZvTg");
  base::Value::List* key_ops = dict.EnsureList("key_ops");

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
  base::Value::Dict dict;
  dict.Set("kty", "oct");
  dict.Set("k", "GADWrMRHwQfoNaXU5fZvTg");
  base::Value::List key_ops;
  key_ops.Append("encrypt");
  dict.Set("key_ops", std::move(key_ops));

  EXPECT_EQ(
      Status::ErrorJwkKeyopsInconsistent(),
      ImportKeyJwkFromDict(
          dict, CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc), false,
          blink::kWebCryptoKeyUsageEncrypt | blink::kWebCryptoKeyUsageDecrypt,
          &key));
}

TEST_F(WebCryptoAesCbcTest, ImportKeyJwkUseEnc) {
  blink::WebCryptoKey key;
  base::Value::Dict dict;
  dict.Set("kty", "oct");
  dict.Set("k", "GADWrMRHwQfoNaXU5fZvTg");

  // Test JWK composite use 'enc' usage
  dict.Set("alg", "A128CBC");
  dict.Set("use", "enc");
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

TEST_F(WebCryptoAesCbcTest, ImportJwkUnknownKeyOps) {
  blink::WebCryptoKey key;
  const std::string jwk =
      R"({
            "kty": "oct",
            "k": "GADWrMRHwQfoNaXU5fZvTg",
            "key_ops": ["foo", "bar", "encrypt", "decrypt"]
        })";

  EXPECT_EQ(Status::Success(),
            ImportKey(blink::kWebCryptoKeyFormatJwk,
                      base::as_bytes(base::make_span(jwk)),
                      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc),
                      false, blink::kWebCryptoKeyUsageEncrypt, &key));
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

  base::Value::Dict dict;
  dict.Set("kty", "oct");
  dict.Set("k", "GADWrMRHwQfoNaXU5fZvTg");

  base::Value::List key_ops;
  key_ops.Append("foo");
  dict.Set("key_ops", std::move(key_ops));
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
  const unsigned int modulus_length = 2048;
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
