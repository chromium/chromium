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

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/values.h"
#include "components/webcrypto/algorithm_dispatch.h"
#include "components/webcrypto/algorithms/test_helpers.h"
#include "components/webcrypto/status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"

namespace webcrypto {

namespace {

// Creates an HMAC algorithm whose parameters struct is compatible with key
// generation. It is an error to call this with a hash_id that is not a SHA*.
// The key_length_bits parameter is optional, with zero meaning unspecified.
blink::WebCryptoAlgorithm CreateHmacKeyGenAlgorithm(
    blink::WebCryptoAlgorithmId hash_id,
    unsigned int key_length_bits) {
  DCHECK(blink::WebCryptoAlgorithm::IsHash(hash_id));
  // key_length_bytes == 0 means unspecified
  return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
      blink::kWebCryptoAlgorithmIdHmac,
      new blink::WebCryptoHmacKeyGenParams(
          CreateAlgorithm(hash_id), (key_length_bits != 0), key_length_bits));
}

blink::WebCryptoAlgorithm CreateHmacImportAlgorithmWithLength(
    blink::WebCryptoAlgorithmId hash_id,
    unsigned int length_bits) {
  DCHECK(blink::WebCryptoAlgorithm::IsHash(hash_id));
  return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
      blink::kWebCryptoAlgorithmIdHmac,
      new blink::WebCryptoHmacImportParams(CreateAlgorithm(hash_id), true,
                                           length_bits));
}

blink::WebCryptoKey GenerateHmacKey(blink::WebCryptoAlgorithmId hash,
                                    size_t key_length_bits) {
  blink::WebCryptoKey key;
  auto status =
      GenerateSecretKey(CreateHmacKeyGenAlgorithm(hash, key_length_bits), true,
                        blink::kWebCryptoKeyUsageSign, &key);
  CHECK(status == Status::Success());
  return key;
}

class WebCryptoHmacTest : public WebCryptoTestBase {};

struct HmacKnownAnswer {
  blink::WebCryptoAlgorithmId hash;
  const char* key;
  const char* message;
  const char* hmac;
};

const HmacKnownAnswer kHmacKnownAnswers[] = {
    // A single byte key with an empty message, generated with:
    //   openssl dgst -sha{1,256} -hmac "" < /dev/null
    {blink::kWebCryptoAlgorithmIdSha1, "00", "",
     "fbdb1d1b18aa6c08324b7d64b71fb76370690e1d"},
    {blink::kWebCryptoAlgorithmIdSha256, "00", "",
     "b613679a0814d9ec772f95d778c35fc5ff1697c493715653c6c712144292c5ad"},

    // NIST test vectors from:
    // http://csrc.nist.gov/groups/STM/cavp/documents/mac/hmactestvectors.zip
    // L = 20, set 45:
    {blink::kWebCryptoAlgorithmIdSha1, "59785928d72516e31272",
     "a3ce8899df1022e8d2d539b47bf0e309c66f84095e21438ec355bf119ce5fdcb4e73a619c"
     "df36f25b369d8c38ff419997f0c59830108223606e31223483fd39edeaa4d3f0d21198862"
     "d239c9fd26074130ff6c86493f5227ab895c8f244bd42c7afce5d147a20a590798c68e708"
     "e964902d124dadecdbda9dbd0051ed710e9bf",
     "3c8162589aafaee024fc9a5ca50dd2336fe3eb28"},
    // L = 20, set 299:
    {blink::kWebCryptoAlgorithmIdSha1,
     "ceb9aedf8d6efcf0ae52bea0fa99a9e26ae81bacea0cff4d5eecf201e3bca3c3577480621"
     "b818fd717ba99d6ff958ea3d59b2527b019c343bb199e648090225867d994607962f5866a"
     "a62930d75b58f6",
     "99958aa459604657c7bf6e4cdfcc8785f0abf06ffe636b5b64ecd931bd8a456305592421f"
     "c28dbcccb8a82acea2be8e54161d7a78e0399a6067ebaca3f2510274dc9f92f2c8ae4265e"
     "ec13d7d42e9f8612d7bc258f913ecb5a3a5c610339b49fb90e9037b02d684fc60da835657"
     "cb24eab352750c8b463b1a8494660d36c3ab2",
     "4ac41ab89f625c60125ed65ffa958c6b490ea670"},
    // L = 32, set 30:
    {blink::kWebCryptoAlgorithmIdSha256,
     "9779d9120642797f1747025d5b22b7ac607cab08e1758f2f3a46c8be1e25c53b8c6a8f58f"
     "fefa176",
     "b1689c2591eaf3c9e66070f8a77954ffb81749f1b00346f9dfe0b2ee905dcc288baf4a92d"
     "e3f4001dd9f44c468c3d07d6c6ee82faceafc97c2fc0fc0601719d2dcd0aa2aec92d1b0ae"
     "933c65eb06a03c9c935c2bad0459810241347ab87e9f11adb30415424c6c7f5f22a003b8a"
     "b8de54f6ded0e3ab9245fa79568451dfa258e",
     "769f00d3e6a6cc1fb426a14a4f76c6462e6149726e0dee0ec0cf97a16605ac8b"},
    // L = 32, set 224:
    {blink::kWebCryptoAlgorithmIdSha256,
     "4b7ab133efe99e02fc89a28409ee187d579e774f4cba6fc223e13504e3511bef8d4f638b9"
     "aca55d4a43b8fbd64cf9d74dcc8c9e8d52034898c70264ea911a3fd70813fa73b08337128"
     "9b",
     "138efc832c64513d11b9873c6fd4d8a65dbf367092a826ddd587d141b401580b798c69025"
     "ad510cff05fcfbceb6cf0bb03201aaa32e423d5200925bddfadd418d8e30e18050eb4f061"
     "8eb9959d9f78c1157d4b3e02cd5961f138afd57459939917d9144c95d8e6a94c8f6d4eef3"
     "418c17b1ef0b46c2a7188305d9811dccb3d99",
     "4f1ee7cb36c58803a8721d4ac8c4cf8cae5d8832392eed2a96dc59694252801b"},
    // L = 48, count 50:
    {blink::kWebCryptoAlgorithmIdSha384,
     "d137f3e6cc4af28554beb03ba7a97e60c9d3959cd3bb08068edbf68d402d0498c6ee0ae9e"
     "3a20dc7d8586e5c352f605cee19",
     "64a884670d1c1dff555483dcd3da305dfba54bdc4d817c33ccb8fe7eb2ebf623624103109"
     "ec41644fa078491900c59a0f666f0356d9bc0b45bcc79e5fc9850f4543d96bc68009044ad"
     "d0838ac1260e80592fbc557b2ddaf5ed1b86d3ed8f09e622e567f1d39a340857f6a850cce"
     "ef6060c48dac3dd0071fe68eb4ed2ed9aca01",
     "c550fa53514da34f15e7f98ea87226ab6896cdfae25d3ec2335839f755cdc9a4992092e70"
     "b7e5bd422784380b6396cf5"},
    // L = 64, count 65:
    {blink::kWebCryptoAlgorithmIdSha512,
     "c367aeb5c02b727883ffe2a4ceebf911b01454beb328fb5d57fc7f11bf744576aba421e2a"
     "63426ea8109bd28ff21f53cd2bf1a11c6c989623d6ec27cdb0bbf458250857d819ff84408"
     "b4f3dce08b98b1587ee59683af8852a0a5f55bda3ab5e132b4010e",
     "1a7331c8ff1b748e3cee96952190fdbbe4ee2f79e5753bbb368255ee5b19c05a4ed9f1b2c"
     "72ff1e9b9cb0348205087befa501e7793770faf0606e9c901836a9bc8afa00d7db94ee29e"
     "b191d5cf3fc3e8da95a0f9f4a2a7964289c3129b512bd890de8700a9205420f28a8965b6c"
     "67be28ba7fe278e5fcd16f0f22cf2b2eacbb9",
     "4459066109cb11e6870fa9c6bfd251adfa304c0a2928ca915049704972edc560cc7c0bc38"
     "249e9101aae2f7d4da62eaff83fb07134efc277de72b9e4ab360425"}};

blink::WebCryptoKey HmacKeyFromHexBytes(blink::WebCryptoAlgorithmId hash,
                                        const char* key) {
  return ImportSecretKeyFromRaw(
      HexStringToBytes(key), CreateHmacImportAlgorithmNoLength(hash),
      blink::kWebCryptoKeyUsageSign | blink::kWebCryptoKeyUsageVerify);
}

std::vector<uint8_t> BytesFromHmacKey(blink::WebCryptoKey key) {
  std::vector<uint8_t> raw_key;
  auto status = ExportKey(blink::kWebCryptoKeyFormatRaw, key, &raw_key);
  CHECK(status == Status::Success());
  return raw_key;
}

std::vector<uint8_t> HmacSign(blink::WebCryptoKey key,
                              const std::vector<uint8_t>& message) {
  std::vector<uint8_t> output;
  auto status = Sign(CreateAlgorithm(blink::kWebCryptoAlgorithmIdHmac), key,
                     message, &output);
  CHECK(status == Status::Success());
  return output;
}

bool HmacVerify(blink::WebCryptoKey key,
                const std::vector<uint8_t>& message,
                const std::vector<uint8_t>& hmac) {
  bool match = false;
  auto status = Verify(CreateAlgorithm(blink::kWebCryptoAlgorithmIdHmac), key,
                       hmac, message, &match);
  CHECK(status == Status::Success());
  return match;
}

TEST_F(WebCryptoHmacTest, KnownAnswers) {
  for (const auto& test : kHmacKnownAnswers) {
    SCOPED_TRACE(&test - &kHmacKnownAnswers[0]);

    std::vector<uint8_t> key_bytes = HexStringToBytes(test.key);
    std::vector<uint8_t> message = HexStringToBytes(test.message);
    std::vector<uint8_t> expected_hmac = HexStringToBytes(test.hmac);

    blink::WebCryptoKey key = HmacKeyFromHexBytes(test.hash, test.key);

    EXPECT_EQ(test.hash, key.Algorithm().HmacParams()->GetHash().Id());
    EXPECT_EQ(key_bytes.size() * 8, key.Algorithm().HmacParams()->LengthBits());
    EXPECT_BYTES_EQ(key_bytes, BytesFromHmacKey(key));

    std::vector<uint8_t> actual_hmac = HmacSign(key, message);

    EXPECT_EQ(expected_hmac, actual_hmac);

    std::vector<uint8_t> truncated_hmac(expected_hmac.begin(),
                                        expected_hmac.end() - 1);
    std::vector<uint8_t> empty_hmac;
    std::vector<uint8_t> long_hmac(1024);

    EXPECT_TRUE(HmacVerify(key, message, actual_hmac));
    EXPECT_FALSE(HmacVerify(key, message, truncated_hmac));
    EXPECT_FALSE(HmacVerify(key, message, empty_hmac));
    EXPECT_FALSE(HmacVerify(key, message, long_hmac));
  }
}

TEST_F(WebCryptoHmacTest, GeneratedKeysHaveExpectedProperties) {
  auto key = GenerateHmacKey(blink::kWebCryptoAlgorithmIdSha1, 512);

  EXPECT_FALSE(key.IsNull());
  EXPECT_TRUE(key.Handle());
  EXPECT_EQ(blink::kWebCryptoKeyTypeSecret, key.GetType());
  EXPECT_EQ(blink::kWebCryptoAlgorithmIdHmac, key.Algorithm().Id());
  EXPECT_EQ(blink::kWebCryptoAlgorithmIdSha1,
            key.Algorithm().HmacParams()->GetHash().Id());
  EXPECT_EQ(512u, key.Algorithm().HmacParams()->LengthBits());
}

TEST_F(WebCryptoHmacTest, GeneratedKeysAreRandomIsh) {
  base::flat_set<std::vector<uint8_t>> seen_keys;
  for (int i = 0; i < 16; ++i) {
    std::vector<uint8_t> key_bytes = BytesFromHmacKey(
        GenerateHmacKey(blink::kWebCryptoAlgorithmIdSha1, 512));
    EXPECT_FALSE(base::Contains(seen_keys, key_bytes));
    seen_keys.insert(key_bytes);
  }
}

// If the key length is not provided, then the block size is used.
TEST_F(WebCryptoHmacTest, GeneratedKeysDefaultToBlockSize) {
  auto sha1_key = GenerateHmacKey(blink::kWebCryptoAlgorithmIdSha1, 0);
  auto sha512_key = GenerateHmacKey(blink::kWebCryptoAlgorithmIdSha512, 0);

  EXPECT_EQ(64u, BytesFromHmacKey(sha1_key).size());
  EXPECT_EQ(128u, BytesFromHmacKey(sha512_key).size());
}

TEST_F(WebCryptoHmacTest, Generating1BitKeyWorks) {
  std::vector<uint8_t> key_bytes =
      BytesFromHmacKey(GenerateHmacKey(blink::kWebCryptoAlgorithmIdSha1, 1));
  ASSERT_EQ(1u, key_bytes.size());
  EXPECT_EQ(key_bytes[0] & 0x7f, 0);
}

TEST_F(WebCryptoHmacTest, GenerateKeyEmptyUsage) {
  blink::WebCryptoKey key;
  blink::WebCryptoAlgorithm algorithm =
      CreateHmacKeyGenAlgorithm(blink::kWebCryptoAlgorithmIdSha512, 0);
  ASSERT_EQ(Status::ErrorCreateKeyEmptyUsages(),
            GenerateSecretKey(algorithm, true, 0, &key));
}

TEST_F(WebCryptoHmacTest, ImportKeyEmptyUsage) {
  blink::WebCryptoKey key;
  std::string key_raw_hex_in = "025a8cf3f08b4f6c5f33bbc76a471939";
  EXPECT_EQ(
      Status::ErrorCreateKeyEmptyUsages(),
      ImportKey(
          blink::kWebCryptoKeyFormatRaw, HexStringToBytes(key_raw_hex_in),
          CreateHmacImportAlgorithmNoLength(blink::kWebCryptoAlgorithmIdSha1),
          true, 0, &key));
}

TEST_F(WebCryptoHmacTest, ImportKeyJwkKeyOpsSignVerify) {
  blink::WebCryptoKey key;
  base::Value::Dict dict;
  dict.Set("kty", "oct");
  dict.Set("k", "GADWrMRHwQfoNaXU5fZvTg");
  dict.Set("key_ops", base::Value::List());
  dict.FindList("key_ops")->Append("sign");

  EXPECT_EQ(Status::Success(),
            ImportKeyJwkFromDict(dict,
                                 CreateHmacImportAlgorithmNoLength(
                                     blink::kWebCryptoAlgorithmIdSha256),
                                 false, blink::kWebCryptoKeyUsageSign, &key));

  EXPECT_EQ(blink::kWebCryptoKeyUsageSign, key.Usages());

  dict.FindList("key_ops")->Append("verify");

  EXPECT_EQ(Status::Success(),
            ImportKeyJwkFromDict(dict,
                                 CreateHmacImportAlgorithmNoLength(
                                     blink::kWebCryptoAlgorithmIdSha256),
                                 false, blink::kWebCryptoKeyUsageVerify, &key));

  EXPECT_EQ(blink::kWebCryptoKeyUsageVerify, key.Usages());
}

// Test 'use' inconsistent with 'key_ops'.
TEST_F(WebCryptoHmacTest, ImportKeyJwkUseInconsisteWithKeyOps) {
  blink::WebCryptoKey key;
  base::Value::Dict dict;
  dict.Set("kty", "oct");
  dict.Set("k", "GADWrMRHwQfoNaXU5fZvTg");
  dict.Set("alg", "HS256");
  dict.Set("use", "sig");

  base::Value::List key_ops;
  key_ops.Append("sign");
  key_ops.Append("verify");
  key_ops.Append("encrypt");
  dict.Set("key_ops", std::move(key_ops));
  EXPECT_EQ(
      Status::ErrorJwkUseAndKeyopsInconsistent(),
      ImportKeyJwkFromDict(
          dict,
          CreateHmacImportAlgorithmNoLength(blink::kWebCryptoAlgorithmIdSha256),
          false,
          blink::kWebCryptoKeyUsageSign | blink::kWebCryptoKeyUsageVerify,
          &key));
}

// Test JWK composite 'sig' use
TEST_F(WebCryptoHmacTest, ImportKeyJwkUseSig) {
  blink::WebCryptoKey key;
  base::Value::Dict dict;
  dict.Set("kty", "oct");
  dict.Set("k", "GADWrMRHwQfoNaXU5fZvTg");
  dict.Set("use", "sig");

  EXPECT_EQ(
      Status::Success(),
      ImportKeyJwkFromDict(
          dict,
          CreateHmacImportAlgorithmNoLength(blink::kWebCryptoAlgorithmIdSha256),
          false,
          blink::kWebCryptoKeyUsageSign | blink::kWebCryptoKeyUsageVerify,
          &key));

  EXPECT_EQ(blink::kWebCryptoKeyUsageSign | blink::kWebCryptoKeyUsageVerify,
            key.Usages());
}

TEST_F(WebCryptoHmacTest, ImportJwkInputConsistency) {
  // The Web Crypto spec says that if a JWK value is present, but is
  // inconsistent with the input value, the operation must fail.

  // Consistency rules when JWK value is not present: Inputs should be used.
  blink::WebCryptoKey key;
  bool extractable = false;
  blink::WebCryptoAlgorithm algorithm =
      CreateHmacImportAlgorithmNoLength(blink::kWebCryptoAlgorithmIdSha256);
  blink::WebCryptoKeyUsageMask usages = blink::kWebCryptoKeyUsageVerify;
  base::Value::Dict dict;
  dict.Set("kty", "oct");
  dict.Set("k", "l3nZEgZCeX8XRwJdWyK3rGB8qwjhdY8vOkbIvh4lxTuMao9Y_--hdg");
  std::vector<uint8_t> json_vec = MakeJsonVector(dict);
  EXPECT_EQ(Status::Success(),
            ImportKey(blink::kWebCryptoKeyFormatJwk, json_vec, algorithm,
                      extractable, usages, &key));
  EXPECT_TRUE(key.Handle());
  EXPECT_EQ(blink::kWebCryptoKeyTypeSecret, key.GetType());
  EXPECT_EQ(extractable, key.Extractable());
  EXPECT_EQ(blink::kWebCryptoAlgorithmIdHmac, key.Algorithm().Id());
  EXPECT_EQ(blink::kWebCryptoAlgorithmIdSha256,
            key.Algorithm().HmacParams()->GetHash().Id());
  EXPECT_EQ(320u, key.Algorithm().HmacParams()->LengthBits());
  EXPECT_EQ(blink::kWebCryptoKeyUsageVerify, key.Usages());
  key = blink::WebCryptoKey::CreateNull();

  // Consistency rules when JWK value exists: Fail if inconsistency is found.

  // Pass: All input values are consistent with the JWK values.
  dict.clear();
  dict.Set("kty", "oct");
  dict.Set("alg", "HS256");
  dict.Set("use", "sig");
  dict.Set("ext", false);
  dict.Set("k", "l3nZEgZCeX8XRwJdWyK3rGB8qwjhdY8vOkbIvh4lxTuMao9Y_--hdg");
  json_vec = MakeJsonVector(dict);
  EXPECT_EQ(Status::Success(),
            ImportKey(blink::kWebCryptoKeyFormatJwk, json_vec, algorithm,
                      extractable, usages, &key));

  // Extractable cases:
  // 1. input=T, JWK=F ==> fail (inconsistent)
  // 4. input=F, JWK=F ==> pass, result extractable is F
  // 2. input=T, JWK=T ==> pass, result extractable is T
  // 3. input=F, JWK=T ==> pass, result extractable is F
  EXPECT_EQ(Status::ErrorJwkExtInconsistent(),
            ImportKey(blink::kWebCryptoKeyFormatJwk, json_vec, algorithm, true,
                      usages, &key));
  EXPECT_EQ(Status::Success(),
            ImportKey(blink::kWebCryptoKeyFormatJwk, json_vec, algorithm, false,
                      usages, &key));
  EXPECT_FALSE(key.Extractable());
  dict.Set("ext", true);
  EXPECT_EQ(Status::Success(),
            ImportKeyJwkFromDict(dict, algorithm, true, usages, &key));
  EXPECT_TRUE(key.Extractable());
  EXPECT_EQ(Status::Success(),
            ImportKeyJwkFromDict(dict, algorithm, false, usages, &key));
  EXPECT_FALSE(key.Extractable());

  // Fail: Input algorithm (AES-CBC) is inconsistent with JWK value
  // (HMAC SHA256).
  dict.clear();
  dict.Set("kty", "oct");
  dict.Set("alg", "HS256");
  dict.Set("k", "l3nZEgZCeX8XRwJdWyK3rGB8qwjhdY8vOkbIvh4lxTuMao9Y_--hdg");
  EXPECT_EQ(Status::ErrorJwkAlgorithmInconsistent(),
            ImportKeyJwkFromDict(
                dict, CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc),
                extractable, blink::kWebCryptoKeyUsageEncrypt, &key));
  // Fail: Input usage (encrypt) is inconsistent with JWK value (use=sig).
  EXPECT_EQ(Status::ErrorJwkUseInconsistent(),
            ImportKey(blink::kWebCryptoKeyFormatJwk, json_vec,
                      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCbc),
                      extractable, blink::kWebCryptoKeyUsageEncrypt, &key));

  // Fail: Input algorithm (HMAC SHA1) is inconsistent with JWK value
  // (HMAC SHA256).
  EXPECT_EQ(Status::ErrorJwkAlgorithmInconsistent(),
            ImportKey(blink::kWebCryptoKeyFormatJwk, json_vec,
                      CreateHmacImportAlgorithmNoLength(
                          blink::kWebCryptoAlgorithmIdSha1),
                      extractable, usages, &key));

  // Pass: JWK alg missing but input algorithm specified: use input value
  dict.Remove("alg");
  EXPECT_EQ(Status::Success(),
            ImportKeyJwkFromDict(dict,
                                 CreateHmacImportAlgorithmNoLength(
                                     blink::kWebCryptoAlgorithmIdSha256),
                                 extractable, usages, &key));
  EXPECT_EQ(blink::kWebCryptoAlgorithmIdHmac, algorithm.Id());
  dict.Set("alg", "HS256");

  // Fail: Input usages (encrypt) is not a subset of the JWK value
  // (sign|verify). Moreover "encrypt" is not a valid usage for HMAC.
  EXPECT_EQ(Status::ErrorCreateKeyBadUsages(),
            ImportKey(blink::kWebCryptoKeyFormatJwk, json_vec, algorithm,
                      extractable, blink::kWebCryptoKeyUsageEncrypt, &key));

  // Fail: Input usages (encrypt|sign|verify) is not a subset of the JWK
  // value (sign|verify). Moreover "encrypt" is not a valid usage for HMAC.
  usages = blink::kWebCryptoKeyUsageEncrypt | blink::kWebCryptoKeyUsageSign |
           blink::kWebCryptoKeyUsageVerify;
  EXPECT_EQ(Status::ErrorCreateKeyBadUsages(),
            ImportKey(blink::kWebCryptoKeyFormatJwk, json_vec, algorithm,
                      extractable, usages, &key));

  // TODO(padolph): kty vs alg consistency tests: Depending on the kty value,
  // only certain alg values are permitted. For example, when kty = "RSA" alg
  // must be of the RSA family, or when kty = "oct" alg must be symmetric
  // algorithm.

  // TODO(padolph): key_ops consistency tests
}

TEST_F(WebCryptoHmacTest, ImportJwkHappy) {
  // This test verifies the happy path of JWK import, including the application
  // of the imported key material.

  blink::WebCryptoKey key;
  bool extractable = false;
  blink::WebCryptoAlgorithm algorithm =
      CreateHmacImportAlgorithmNoLength(blink::kWebCryptoAlgorithmIdSha256);
  blink::WebCryptoKeyUsageMask usages = blink::kWebCryptoKeyUsageSign;

  // Import a symmetric key JWK and HMAC-SHA256 sign()
  // Uses the first SHA256 test vector from the HMAC sample set above.

  base::Value::Dict dict;
  dict.Set("kty", "oct");
  dict.Set("alg", "HS256");
  dict.Set("use", "sig");
  dict.Set("ext", false);
  dict.Set("k", "l3nZEgZCeX8XRwJdWyK3rGB8qwjhdY8vOkbIvh4lxTuMao9Y_--hdg");

  ASSERT_EQ(Status::Success(),
            ImportKeyJwkFromDict(dict, algorithm, extractable, usages, &key));

  EXPECT_EQ(blink::kWebCryptoAlgorithmIdSha256,
            key.Algorithm().HmacParams()->GetHash().Id());

  const std::vector<uint8_t> message_raw = HexStringToBytes(
      "b1689c2591eaf3c9e66070f8a77954ffb81749f1b00346f9dfe0b2ee905dcc288baf4a"
      "92de3f4001dd9f44c468c3d07d6c6ee82faceafc97c2fc0fc0601719d2dcd0aa2aec92"
      "d1b0ae933c65eb06a03c9c935c2bad0459810241347ab87e9f11adb30415424c6c7f5f"
      "22a003b8ab8de54f6ded0e3ab9245fa79568451dfa258e");

  std::vector<uint8_t> output;

  ASSERT_EQ(Status::Success(),
            Sign(CreateAlgorithm(blink::kWebCryptoAlgorithmIdHmac), key,
                 message_raw, &output));

  const std::string mac_raw =
      "769f00d3e6a6cc1fb426a14a4f76c6462e6149726e0dee0ec0cf97a16605ac8b";

  EXPECT_BYTES_EQ_HEX(mac_raw, output);

  // TODO(padolph): Import an RSA public key JWK and use it
}

TEST_F(WebCryptoHmacTest, ImportExportJwk) {
  // HMAC SHA-1
  ImportExportJwkSymmetricKey(
      256, CreateHmacImportAlgorithmNoLength(blink::kWebCryptoAlgorithmIdSha1),
      blink::kWebCryptoKeyUsageSign | blink::kWebCryptoKeyUsageVerify, "HS1");

  // HMAC SHA-384
  ImportExportJwkSymmetricKey(
      384,
      CreateHmacImportAlgorithmNoLength(blink::kWebCryptoAlgorithmIdSha384),
      blink::kWebCryptoKeyUsageSign, "HS384");

  // HMAC SHA-512
  ImportExportJwkSymmetricKey(
      512,
      CreateHmacImportAlgorithmNoLength(blink::kWebCryptoAlgorithmIdSha512),
      blink::kWebCryptoKeyUsageVerify, "HS512");
}

TEST_F(WebCryptoHmacTest, ExportJwkEmptyKey) {
  blink::WebCryptoKeyUsageMask usages = blink::kWebCryptoKeyUsageSign;

  // Importing empty HMAC key is no longer allowed. However such a key can be
  // created via de-serialization.
  blink::WebCryptoKey key;
  ASSERT_TRUE(DeserializeKeyForClone(blink::WebCryptoKeyAlgorithm::CreateHmac(
                                         blink::kWebCryptoAlgorithmIdSha1, 0),
                                     blink::kWebCryptoKeyTypeSecret, true,
                                     usages, {}, &key));

  // Export the key in JWK format and validate.
  std::vector<uint8_t> json;
  ASSERT_EQ(Status::Success(),
            ExportKey(blink::kWebCryptoKeyFormatJwk, key, &json));
  EXPECT_TRUE(VerifySecretJwk(json, "HS1", "", usages));

  // Now try re-importing the JWK key.
  key = blink::WebCryptoKey::CreateNull();
  EXPECT_EQ(Status::ErrorHmacImportEmptyKey(),
            ImportKey(blink::kWebCryptoKeyFormatJwk, json,
                      CreateHmacImportAlgorithmNoLength(
                          blink::kWebCryptoAlgorithmIdSha1),
                      true, usages, &key));
}

// Imports an HMAC key contaning no byte data.
TEST_F(WebCryptoHmacTest, ImportRawEmptyKey) {
  const blink::WebCryptoAlgorithm import_algorithm =
      CreateHmacImportAlgorithmNoLength(blink::kWebCryptoAlgorithmIdSha1);

  blink::WebCryptoKeyUsageMask usages = blink::kWebCryptoKeyUsageSign;
  blink::WebCryptoKey key;

  ASSERT_EQ(Status::ErrorHmacImportEmptyKey(),
            ImportKey(blink::kWebCryptoKeyFormatRaw, {}, import_algorithm, true,
                      usages, &key));
}

// Imports an HMAC key contaning 1 byte data, however the length was set to 0.
TEST_F(WebCryptoHmacTest, ImportRawKeyWithZeroLength) {
  const blink::WebCryptoAlgorithm import_algorithm =
      CreateHmacImportAlgorithm(blink::kWebCryptoAlgorithmIdSha1, 0);

  blink::WebCryptoKeyUsageMask usages = blink::kWebCryptoKeyUsageSign;
  blink::WebCryptoKey key;

  std::vector<uint8_t> key_data(1);
  ASSERT_EQ(Status::ErrorHmacImportBadLength(),
            ImportKey(blink::kWebCryptoKeyFormatRaw, key_data, import_algorithm,
                      true, usages, &key));
}

// Import a huge hmac key (UINT_MAX bytes).
TEST_F(WebCryptoHmacTest, ImportRawKeyTooLarge) {
  // This uses `reinterpret_cast` of `1` to avoid nullness `CHECK` in the
  // constructor of `span`.
  const void* invalid_data = reinterpret_cast<void*>(1);
  // Invalid data of big length. This span is invalid, but ImportKey should fail
  // before actually reading the bytes, as the key is too large.
  base::span<const uint8_t> big_data(static_cast<const uint8_t*>(invalid_data),
                                     UINT_MAX);

  blink::WebCryptoKey key;
  EXPECT_EQ(Status::ErrorDataTooLarge(),
            ImportKey(blink::kWebCryptoKeyFormatRaw, big_data,
                      CreateHmacImportAlgorithmNoLength(
                          blink::kWebCryptoAlgorithmIdSha1),
                      true, blink::kWebCryptoKeyUsageSign, &key));
}

// Import an HMAC key with 120 bits of data, however request 128 bits worth.
TEST_F(WebCryptoHmacTest, ImportRawKeyLengthTooLarge) {
  blink::WebCryptoKey key;
  EXPECT_EQ(Status::ErrorHmacImportBadLength(),
            ImportKey(blink::kWebCryptoKeyFormatRaw, std::vector<uint8_t>(15),
                      CreateHmacImportAlgorithmWithLength(
                          blink::kWebCryptoAlgorithmIdSha1, 128),
                      true, blink::kWebCryptoKeyUsageSign, &key));
}

// Import an HMAC key with 128 bits of data, however request 120 bits worth.
TEST_F(WebCryptoHmacTest, ImportRawKeyLengthTooSmall) {
  blink::WebCryptoKey key;
  EXPECT_EQ(Status::ErrorHmacImportBadLength(),
            ImportKey(blink::kWebCryptoKeyFormatRaw, std::vector<uint8_t>(16),
                      CreateHmacImportAlgorithmWithLength(
                          blink::kWebCryptoAlgorithmIdSha1, 120),
                      true, blink::kWebCryptoKeyUsageSign, &key));
}

// Import an HMAC key with 16 bits of data and request a 12 bit key, using the
// "raw" format.
TEST_F(WebCryptoHmacTest, ImportRawKeyTruncation) {
  const std::vector<uint8_t> data = HexStringToBytes("b1ff");

  blink::WebCryptoKey key;
  EXPECT_EQ(Status::Success(),
            ImportKey(blink::kWebCryptoKeyFormatRaw, data,
                      CreateHmacImportAlgorithmWithLength(
                          blink::kWebCryptoAlgorithmIdSha1, 12),
                      true, blink::kWebCryptoKeyUsageSign, &key));

  // On export the last 4 bits has been set to zero.
  std::vector<uint8_t> raw_key;
  EXPECT_EQ(Status::Success(),
            ExportKey(blink::kWebCryptoKeyFormatRaw, key, &raw_key));
  EXPECT_BYTES_EQ(HexStringToBytes("b1f0"), raw_key);
}

// The same test as above, but using the JWK format.
TEST_F(WebCryptoHmacTest, ImportJwkKeyTruncation) {
  base::Value::Dict dict;
  dict.Set("kty", "oct");
  dict.Set("k", "sf8");  // 0xB1FF

  blink::WebCryptoKey key;
  EXPECT_EQ(Status::Success(),
            ImportKeyJwkFromDict(dict,
                                 CreateHmacImportAlgorithmWithLength(
                                     blink::kWebCryptoAlgorithmIdSha1, 12),
                                 true, blink::kWebCryptoKeyUsageSign, &key));

  // On export the last 4 bits has been set to zero.
  std::vector<uint8_t> raw_key;
  EXPECT_EQ(Status::Success(),
            ExportKey(blink::kWebCryptoKeyFormatRaw, key, &raw_key));
  EXPECT_BYTES_EQ(HexStringToBytes("b1f0"), raw_key);
}

}  // namespace

}  // namespace webcrypto
