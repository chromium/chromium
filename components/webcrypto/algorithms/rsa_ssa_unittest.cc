// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string_view>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/strings/string_util.h"
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
base::Value::Dict BuildTestJwkPublicKey() {
  base::Value::Dict jwk;
  jwk.Set("kty", "RSA");
  jwk.Set("alg", "RS256");
  jwk.Set("use", "sig");
  jwk.Set("ext", false);
  jwk.Set(
      "n",
      "qLOyhK-OtQs4cDSoYPFGxJGfMYdjzWxVmMiuSBGh4KvEx-CwgtaTpef87Wdc9GaFEncsDLxk"
      "p0LGxjD1M8jMcvYq6DPEC_JYQumEu3i9v5fAEH1VvbZi9cTg-rmEXLUUjvc5LdOq_5OuHmtm"
      "e7PUJHYW1PW6ENTP0ibeiNOfFvs");
  jwk.Set("e", "AQAB");
  return jwk;
}

base::Value::Dict BuildTestJwkPrivateKey() {
  base::Value::Dict jwk;
  jwk.Set("kty", "RSA");
  jwk.Set("d",
          "ZmJJJ3PBfirgPEOb844fI_1_zXn3A09X9fkk-65xeTNo3JeigTPpuB54FC_"
          "GXUmqiXLVx5gynO6cwl9wjxVKYQ");
  jwk.Set("dp", "DOUuUiDhtjpnCuIjcGRWhQYok8NeUO5XV1Uwx1-DxtU");
  jwk.Set("dq", "mKOBL1e74J8OuGtW1kc2-s4VEP5Eeiwe__TAeBm-roE");
  jwk.Set("e", "AQAB");
  jwk.Set("n",
          "tFJAFt_UiJsHlRavDgOxOnYKTHkV-"
          "cF1aTDtkzNg6WYt9geaPbAvFnR3FVO0BFsl8tzPzMOTkI_kbOfCfAw3FQ");
  jwk.Set("p", "7kMQn01JVhyHM7B85hLUuNBDsXiboMc4Di81qmxX7r0");
  jwk.Set("q", "wb7rEiGxG4CrybVYns9voNQM2NPCuCEgWPLA_vCkuzk");
  jwk.Set("qi", "6rrPQ4YaOLNGtG7TrLXUR_FSWpOFSUveHTHbFQU6iNU");
  return jwk;
}

void SwapDictMembers(base::Value::Dict& d, const char* a, const char* b) {
  auto va = d.Extract(a);
  auto vb = d.Extract(b);
  CHECK(va);
  CHECK(vb);
  d.Set(a, std::move(*vb));
  d.Set(b, std::move(*va));
}

std::string FlipHexByte(std::string_view hex, size_t index) {
  auto bytes = HexStringToBytes(hex);
  bytes[index] ^= 0xff;
  return base::HexEncode(base::make_span(bytes));
}

blink::WebCryptoAlgorithm RS256Algorithm() {
  return CreateRsaHashedImportAlgorithm(
      blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
      blink::kWebCryptoAlgorithmIdSha256);
}

blink::WebCryptoKey ImportJwkRS256OrDie(std::string_view jwk) {
  std::vector<uint8_t> jwk_bytes(jwk.begin(), jwk.end());
  blink::WebCryptoKey key;
  Status status =
      ImportKey(blink::kWebCryptoKeyFormatJwk, jwk_bytes, RS256Algorithm(),
                true, blink::kWebCryptoKeyUsageSign, &key);
  CHECK(status.IsSuccess()) << StatusToString(status);
  return key;
}

blink::WebCryptoKey ImportJwkRS256OrDie(const base::Value::Dict& jwk) {
  blink::WebCryptoKey key;
  Status status = ImportKeyJwkFromDict(jwk, RS256Algorithm(), false,
                                       blink::kWebCryptoKeyUsageSign, &key);
  CHECK(status.IsSuccess()) << StatusToString(status);
  return key;
}

Status ImportJwkRS256MustFail(std::string_view jwk) {
  std::vector<uint8_t> jwk_bytes(jwk.begin(), jwk.end());
  blink::WebCryptoKey key;
  Status status =
      ImportKey(blink::kWebCryptoKeyFormatJwk, jwk_bytes, RS256Algorithm(),
                true, blink::kWebCryptoKeyUsageSign, &key);
  CHECK(!status.IsSuccess());
  return status;
}

Status ImportJwkRS256MustFail(const base::Value::Dict& jwk) {
  blink::WebCryptoKey key;
  return ImportKeyJwkFromDict(jwk, RS256Algorithm(), false,
                              blink::kWebCryptoKeyUsageSign, &key);
}

Status ImportSpkiRS256MustFail(std::string_view spki) {
  auto spki_bytes = HexStringToBytes(spki);
  blink::WebCryptoKey key;
  // Note: SPKI keys can only be used for verification, not signing.
  Status status =
      ImportKey(blink::kWebCryptoKeyFormatSpki, spki_bytes, RS256Algorithm(),
                true, blink::kWebCryptoKeyUsageVerify, &key);
  CHECK(!status.IsSuccess());
  return status;
}

Status ImportPkcs8RS256MustFail(std::string_view pkcs8) {
  auto pkcs8_bytes = HexStringToBytes(pkcs8);
  blink::WebCryptoKey key;
  Status status =
      ImportKey(blink::kWebCryptoKeyFormatPkcs8, pkcs8_bytes, RS256Algorithm(),
                true, blink::kWebCryptoKeyUsageSign, &key);
  CHECK(!status.IsSuccess());
  return status;
}

std::vector<uint8_t> ExportPkcs8OrDie(blink::WebCryptoKey key) {
  std::vector<uint8_t> exported;
  Status status = ExportKey(blink::kWebCryptoKeyFormatPkcs8, key, &exported);
  CHECK(status.IsSuccess()) << StatusToString(status);
  return exported;
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
      base::as_bytes(base::make_span(std::string_view(expected_jwk))),
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

const char kRsa512Jwk_0[] =
    R"({
      "alg": "RS256",
      "d": "DXYeCQ4W_Yv9zN4vCIQQtgvunsoeWfPeRvYEgVAIYdhuNFRmcinD9UuNP70VOoe2qiZ0DNAjsQn-uYCW9TEZ4Q",
      "dp": "5f8auF7xPSfhZlklUtBnKFYKEDaYR2dFWg_zQB7oCzE",
      "dq": "hkRVAMcErDAaCKp0V3QzWYhY_J22nJkiNXIxHz4Ja2c",
      "e": "AQAB",
      "kty": "RSA",
      "n": "yLuHfrJbqUSFFqhUu70z585pWrw1IFcnBCccj43uwGOiesMkx0SWw4jyk3UNTux5AO-7VVCU8jb7237YYaOmOw",
      "p": "8rLWJeMlHCtwZstui6p8jyai6m7GQ6fC1hK17vxA_JE",
      "q": "07vkNpoE6SUze7Af2KEP6M_sz8dABZ3EQJuQ6JfiDAs",
      "qi": "kxd6mc-3AhJtuixmzrSywxvwVwChdEG4I6WVTBe_bvE"
   })";
// Note that the first letter of "d" has been changed.
const char kRsa512Jwk_0_Damaged[] =
    R"({
      "alg": "RS256",
      "d": "EXYeCQ4W_Yv9zN4vCIQQtgvunsoeWfPeRvYEgVAIYdhuNFRmcinD9UuNP70VOoe2qiZ0DNAjsQn-uYCW9TEZ4Q",
      "dp": "5f8auF7xPSfhZlklUtBnKFYKEDaYR2dFWg_zQB7oCzE",
      "dq": "hkRVAMcErDAaCKp0V3QzWYhY_J22nJkiNXIxHz4Ja2c",
      "e": "AQAB",
      "kty": "RSA",
      "n": "yLuHfrJbqUSFFqhUu70z585pWrw1IFcnBCccj43uwGOiesMkx0SWw4jyk3UNTux5AO-7VVCU8jb7237YYaOmOw",
      "p": "8rLWJeMlHCtwZstui6p8jyai6m7GQ6fC1hK17vxA_JE",
      "q": "07vkNpoE6SUze7Af2KEP6M_sz8dABZ3EQJuQ6JfiDAs",
      "qi": "kxd6mc-3AhJtuixmzrSywxvwVwChdEG4I6WVTBe_bvE"
   })";
const char kRsa512Pkcs8_0[] =
    "30820156020100300D06092A864886F70D0101010500048201403082013C020100024100C8"
    "BB877EB25BA9448516A854BBBD33E7CE695ABC3520572704271C8F8DEEC063A27AC324C744"
    "96C388F293750D4EEC7900EFBB555094F236FBDB7ED861A3A63B020301000102400D761E09"
    "0E16FD8BFDCCDE2F088410B60BEE9ECA1E59F3DE46F60481500861D86E3454667229C3F54B"
    "8D3FBD153A87B6AA26740CD023B109FEB98096F53119E1022100F2B2D625E3251C2B7066CB"
    "6E8BAA7C8F26A2EA6EC643A7C2D612B5EEFC40FC91022100D3BBE4369A04E925337BB01FD8"
    "A10FE8CFECCFC740059DC4409B90E897E20C0B022100E5FF1AB85EF13D27E166592552D067"
    "28560A1036984767455A0FF3401EE80B3102210086445500C704AC301A08AA745774335988"
    "58FC9DB69C99223572311F3E096B6702210093177A99CFB702126DBA2C66CEB4B2C31BF057"
    "00A17441B823A5954C17BF6EF1";

const char kRsa512Jwk_1[] =
    R"({
      "alg": "RS256",
      "d": "phZ8gCMB14I-A35dwg7j16uSd91COBNN4GuwZchy7FPGH0hNzaH2jOYBU3sWy2ORxwWN8PbKqKOkZb8mh4v_gQ",
      "dp": "PPEZjFS3paYuOvD2ROr6Es1mP2gGeM_9QNouoZjbpZE",
      "dq": "pXDNDS8Z77HJXB2EsG40JLsNv-sUkakmAbEzwDfSoFE",
      "e": "AQAB",
      "kty": "RSA",
      "n": "zg5KF3GIFp9XJdOMD9Iz-SeC_CVdUeI-gTxw2Igpd8FB0cJllMxg6n3FALqZ7YKPAp7rCL3VYhu-GR8OnqhNaQ",
      "p": "8TlLFr-SEpz_ItKjdarp9q8S8_2OHy2RFysdY6yGndE",
      "q": "2q2EDZHQQ_dp9-Cx2Z8kWn7sYo8K9caFneAJge8ZpBk",
      "qi": "GT51ibfjUV05KRQhyjiqeCkGT12aAWvLzKRsaV9VE54"
   })";
const char kRsa512Pkcs8_1[] =
    "30820155020100300D06092A864886F70D01010105000482013F3082013B020100024100CE"
    "0E4A177188169F5725D38C0FD233F92782FC255D51E23E813C70D8882977C141D1C26594CC"
    "60EA7DC500BA99ED828F029EEB08BDD5621BBE191F0E9EA84D690203010001024100A6167C"
    "802301D7823E037E5DC20EE3D7AB9277DD4238134DE06BB065C872EC53C61F484DCDA1F68C"
    "E601537B16CB6391C7058DF0F6CAA8A3A465BF26878BFF81022100F1394B16BF92129CFF22"
    "D2A375AAE9F6AF12F3FD8E1F2D91172B1D63AC869DD1022100DAAD840D91D043F769F7E0B1"
    "D99F245A7EEC628F0AF5C6859DE00981EF19A41902203CF1198C54B7A5A62E3AF0F644EAFA"
    "12CD663F680678CFFD40DA2EA198DBA591022100A570CD0D2F19EFB1C95C1D84B06E3424BB"
    "0DBFEB1491A92601B133C037D2A0510220193E7589B7E3515D39291421CA38AA7829064F5D"
    "9A016BCBCCA46C695F55139E";

// This is a regression test for http://crbug.com/378315, for which importing
// a sequence of keys from JWK could yield the wrong key. The first key would
// be imported correctly, however every key after that would actually import
// the first key.
TEST_F(WebCryptoRsaSsaTest, ImportMultipleRsaKeysJwk) {
  // Ensure that both keys stay alive across the whole test body, to ensure the
  // existence of one doesn't impact the other.
  std::vector<blink::WebCryptoKey> keys = {
      ImportJwkRS256OrDie(kRsa512Jwk_0),
      ImportJwkRS256OrDie(kRsa512Jwk_1),
  };
  EXPECT_EQ(HexStringToBytes(kRsa512Pkcs8_0), ExportPkcs8OrDie(keys[0]));
  EXPECT_EQ(HexStringToBytes(kRsa512Pkcs8_1), ExportPkcs8OrDie(keys[1]));
}

// Import an RSA private key using JWK. Next import a JWK containing the same
// modulus, but mismatched parameters for the rest. It should NOT be possible
// that the second import retrieves the first key. See http://crbug.com/378315
// for how that could happen.
TEST_F(WebCryptoRsaSsaTest, ImportCorruptKeyReusedModulus) {
  blink::WebCryptoKey key = ImportJwkRS256OrDie(kRsa512Jwk_0);
  EXPECT_EQ(Status::DataError(), ImportJwkRS256MustFail(kRsa512Jwk_0_Damaged));
}

TEST_F(WebCryptoRsaSsaTest, GenerateKeyPairRsa) {
  // Successful WebCryptoAlgorithmIdRsaSsaPkcs1v1_5 key generation (sha256)
  const unsigned int modulus_length = 2048;
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

  base::Value::Dict jwk = BuildTestJwkPublicKey();
  jwk.Remove("use");

  for (auto usage : kBadUsages) {
    blink::WebCryptoKey public_key;
    ASSERT_EQ(Status::ErrorCreateKeyBadUsages(),
              ImportKeyJwkFromDict(jwk, algorithm, false, usage, &public_key));
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
  const unsigned int modulus_length = 2048;
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
  const unsigned int modulus_length = 2048;
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
            ImportKeyJwkFromDict(BuildTestJwkPublicKey(), algorithm, false,
                                 usages, &key));
  EXPECT_EQ(algorithm.Id(), key.Algorithm().Id());
  EXPECT_FALSE(key.Extractable());
  EXPECT_EQ(blink::kWebCryptoKeyUsageVerify, key.Usages());
  EXPECT_EQ(blink::kWebCryptoKeyTypePublic, key.GetType());

  // The following are specific failure cases for when kty = "RSA".

  // Fail if either "n" or "e" is not present or malformed.
  for (auto* const param : {"n", "e"}) {
    base::Value::Dict jwk = BuildTestJwkPublicKey();

    // Fail on missing parameter.
    jwk.Remove(param);
    EXPECT_NE(Status::Success(),
              ImportKeyJwkFromDict(jwk, algorithm, false, usages, &key));

    // Fail on bad b64 parameter encoding.
    jwk.Set(param, "Qk3f0DsytU8lfza2au #$% Htaw2xpop9yTuH0");
    EXPECT_NE(Status::Success(),
              ImportKeyJwkFromDict(jwk, algorithm, false, usages, &key));

    // Fail on empty parameter.
    jwk.Set(param, "");
    EXPECT_EQ(Status::ErrorJwkEmptyBigInteger(param),
              ImportKeyJwkFromDict(jwk, algorithm, false, usages, &key));
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

TEST_F(WebCryptoRsaSsaTest, ImportValidJwkPrivateKey) {
  ImportJwkRS256OrDie(BuildTestJwkPrivateKey());
}

TEST_F(WebCryptoRsaSsaTest, ImportInvalidJwkPrivateKey_SwapPQ) {
  auto key = BuildTestJwkPrivateKey();
  SwapDictMembers(key, "p", "q");
  EXPECT_EQ(StatusToString(ImportJwkRS256MustFail(key)), "DataError");
}

TEST_F(WebCryptoRsaSsaTest, ImportInvalidJwkPrivateKey_SwapPQDPDQ) {
  auto key = BuildTestJwkPrivateKey();
  SwapDictMembers(key, "p", "q");
  SwapDictMembers(key, "dp", "dq");
  EXPECT_EQ(StatusToString(ImportJwkRS256MustFail(key)), "DataError");
}

TEST_F(WebCryptoRsaSsaTest, ImportInvalidJwkPrivateKey_MissingMostOptionals) {
  auto key = BuildTestJwkPrivateKey();
  for (const auto* param : {"q", "dp", "dq", "qi"})
    key.Remove(param);
  EXPECT_EQ(StatusToString(ImportJwkRS256MustFail(key)),
            "DataError: The required JWK member \"q\" was missing");
}

TEST_F(WebCryptoRsaSsaTest, ImportInvalidJwkPrivateKey_MissingAllOptionals) {
  // This is a deliberate spec divergence: while JWK allows keys that are
  // missing all these attributes (because they are all optional), Chromium
  // doesn't. That's because without those attributes, we wouldn't be able to
  // serialize these keys as PKCS#8 keys anyway, which would break a bunch
  // of assumptions throughout our implementation.
  //
  // See: https://crbug.com/374927
  auto key = BuildTestJwkPrivateKey();
  for (const auto* param : {"p", "q", "dp", "dq", "qi"})
    key.Remove(param);
  EXPECT_EQ(StatusToString(ImportJwkRS256MustFail(key)),
            "DataError: The required JWK member \"p\" was missing");
}

TEST_F(WebCryptoRsaSsaTest, ImportInvalidJwkPrivateKey_LeadingZeroesOnE) {
  auto key = BuildTestJwkPrivateKey();
  key.Set("e", "AAEAAQ");  // 00 01 00 01
  EXPECT_EQ("DataError: The JWK \"e\" member contained a leading zero.",
            StatusToString(ImportJwkRS256MustFail(key)));
}

TEST_F(WebCryptoRsaSsaTest, ImportInvalidJwkPrivateKey_Base64PaddingInD) {
  auto key = BuildTestJwkPrivateKey();
  auto d = *key.FindString("d");
  key.Set("d", d + "==");
  EXPECT_EQ(
      "DataError: The JWK member \"d\" could not be base64url decoded or"
      " contained padding",
      StatusToString(ImportJwkRS256MustFail(key)));
}

TEST_F(WebCryptoRsaSsaTest, ImportInvalidJwkPrivateKey_Base64UrlInN) {
  auto key = BuildTestJwkPrivateKey();
  auto n = *key.FindString("n");
  ASSERT_TRUE(base::ReplaceChars(n, "_", "/", &n));
  key.Set("n", n);
  EXPECT_EQ(
      "DataError: The JWK member \"n\" could not be base64url decoded or"
      " contained padding",
      StatusToString(ImportJwkRS256MustFail(key)));
}

TEST_F(WebCryptoRsaSsaTest, ImportInvalidJwkPrivateKey_Base64UrlInDQ) {
  auto key = BuildTestJwkPrivateKey();
  auto dq = *key.FindString("dq");
  ASSERT_TRUE(base::ReplaceChars(dq, "-", "+", &dq));
  key.Set("dq", dq);
  EXPECT_EQ(
      "DataError: The JWK member \"dq\" could not be base64url decoded or"
      " contained padding",
      StatusToString(ImportJwkRS256MustFail(key)));
}

TEST_F(WebCryptoRsaSsaTest, ImportInvalidSpki_Empty) {
  EXPECT_EQ("DataError", StatusToString(ImportSpkiRS256MustFail("")));
}

TEST_F(WebCryptoRsaSsaTest, ImportInvalidSpki_BadDER) {
  EXPECT_EQ("DataError", StatusToString(ImportSpkiRS256MustFail("618333c4cb")));
}

TEST_F(WebCryptoRsaSsaTest, ImportInvalidSpki_NotRSA) {
  EXPECT_EQ("DataError",
            StatusToString(ImportSpkiRS256MustFail(
                "3059301306072A8648CE3D020106082A8648CE3D030107034200049CB0CF69"
                "303DAFC761D4E4687B4ECF039E6D34AB964AF80810D8D558A4A8D6F72D5123"
                "3A1788920A86EE08A1962C79EFA317FB7879E297DAD2146DB995FA1C78")));
}

TEST_F(WebCryptoRsaSsaTest, ImportInvalidSpki_TrailingData) {
  EXPECT_EQ(
      "DataError",
      StatusToString(ImportSpkiRS256MustFail(
          "30819F300D06092A864886F70D010101050003818D0030818902818100A56E4A0E70"
          "1017589A5187DC7EA841D156F2EC0E36AD52A44DFEB1E61F7AD991D8C51056FFEDB1"
          "62B4C0F283A12A88A394DFF526AB7291CBB307CEABFCE0B1DFD5CD9508096D5B2B8B"
          "6DF5D671EF6377C0921CB23C270A70E2598E6FF89D19F105ACC2D3F0CB35F29280E1"
          "386B6F64C4EF22E1E1F20D0CE8CFFB2249BD9A2137020301000100000000")));
}

TEST_F(WebCryptoRsaSsaTest, ImportInvalidPkcs8_Empty) {
  EXPECT_EQ("DataError", StatusToString(ImportPkcs8RS256MustFail("")));
}

TEST_F(WebCryptoRsaSsaTest, ImportInvalidPkcs8_BadDER) {
  EXPECT_EQ("DataError",
            StatusToString(ImportPkcs8RS256MustFail("618333c4cb")));
}

TEST_F(WebCryptoRsaSsaTest, ImportInvalidPkcs8_CRTValuesAreZero) {
  EXPECT_EQ(
      "DataError",
      StatusToString(ImportPkcs8RS256MustFail(
          "30820138020100300D06092A864886F70D0101010500048201223082011E02010002"
          "818100A56E4A0E701017589A5187DC7EA841D156F2EC0E36AD52A44DFEB1E61F7AD9"
          "91D8C51056FFEDB162B4C0F283A12A88A394DFF526AB7291CBB307CEABFCE0B1DFD5"
          "CD9508096D5B2B8B6DF5D671EF6377C0921CB23C270A70E2598E6FF89D19F105ACC2"
          "D3F0CB35F29280E1386B6F64C4EF22E1E1F20D0CE8CFFB2249BD9A21370203010001"
          "02818033A5042A90B27D4F5451CA9BBBD0B44771A101AF884340AEF9885F2A4BBE92"
          "E894A724AC3C568C8F97853AD07C0266C8C6A3CA0929F1E8F11231884429FC4D9AE5"
          "5FEE896A10CE707C3ED7E734E44727A39574501A532683109C2ABACABA283C31B4BD"
          "2F53C3EE37E352CEE34F9E503BD80C0622AD79C6DCEE883547C6A3B3250201000201"
          "00020100020100020100")));
}

TEST_F(WebCryptoRsaSsaTest, ImportInvalidPkcs8_NotRSA) {
  EXPECT_EQ("DataError",
            StatusToString(ImportPkcs8RS256MustFail(
                "308187020100301306072A8648CE3D020106082A8648CE3D030107046D306B"
                "02010104201FE33950C5F461124AE992C2BDFDF1C73B1615F571BD567E60D1"
                "9AA1F48CDF42A144034200047C110C66DCFDA807F6E69E45DDB3C74F69A148"
                "4D203E8DC5ADA8E9A9DD7CB3C70DF448986E51BDE5D1576F99901F9C2C6A80"
                "6A47FD907643A72B835597EFC8C6")));
}

TEST_F(WebCryptoRsaSsaTest, ImportInvalidPkcs8_TrailingData) {
  EXPECT_EQ(
      "DataError",
      StatusToString(ImportPkcs8RS256MustFail(
          "30820275020100300D06092A864886F70D01010105000482025F3082025B02010002"
          "818100A56E4A0E701017589A5187DC7EA841D156F2EC0E36AD52A44DFEB1E61F7AD9"
          "91D8C51056FFEDB162B4C0F283A12A88A394DFF526AB7291CBB307CEABFCE0B1DFD5"
          "CD9508096D5B2B8B6DF5D671EF6377C0921CB23C270A70E2598E6FF89D19F105ACC2"
          "D3F0CB35F29280E1386B6F64C4EF22E1E1F20D0CE8CFFB2249BD9A21370203010001"
          "02818033A5042A90B27D4F5451CA9BBBD0B44771A101AF884340AEF9885F2A4BBE92"
          "E894A724AC3C568C8F97853AD07C0266C8C6A3CA0929F1E8F11231884429FC4D9AE5"
          "5FEE896A10CE707C3ED7E734E44727A39574501A532683109C2ABACABA283C31B4BD"
          "2F53C3EE37E352CEE34F9E503BD80C0622AD79C6DCEE883547C6A3B325024100E7E8"
          "942720A877517273A356053EA2A1BC0C94AA72D55C6E86296B2DFC967948C0A72CBC"
          "CCA7EACB35706E09A1DF55A1535BD9B3CC34160B3B6DCD3EDA8E6443024100B69DCA"
          "1CF7D4D7EC81E75B90FCCA874ABCDE123FD2700180AA90479B6E48DE8D67ED24F9F1"
          "9D85BA275874F542CD20DC723E6963364A1F9425452B269A6799FD024028FA139386"
          "55BE1F8A159CBACA5A72EA190C30089E19CD274A556F36C4F6E19F554B34C0777904"
          "27BBDD8DD3EDE2448328F385D81B30E8E43B2FFFA02786197902401A8B38F398FA71"
          "2049898D7FB79EE0A77668791299CDFA09EFC0E507ACB21ED74301EF5BFD48BE455E"
          "AEB6E1678255827580A8E4E8E14151D1510A82A3F2E729024027156ABA4126D24A81"
          "F3A528CBFB27F56886F840A9F6E86E17A44B94FE9319584B8E22FDDE1E5A2E3BD8AA"
          "5BA8D8584194EB2190ACF832B847F13A3D24A79F4D00000000")));
}

TEST_F(WebCryptoRsaSsaTest, ImportInvalidPkcs8_CorruptFields) {
  const struct {
    size_t byte;
    const char* name;
  } kTestCases[] = {
      {50, "n"},  {168, "e"},  {175, "d"},  {333, "p"},
      {373, "q"}, {450, "dp"}, {550, "dq"}, {600, "qi"},
  };
  for (const auto& test : kTestCases) {
    std::string key = FlipHexByte(kPrivateKeyPkcs8DerHex, test.byte);
    EXPECT_EQ("DataError", StatusToString(ImportPkcs8RS256MustFail(key)))
        << "byte flip should have invalidated key: " << test.name;
  }
}

}  // namespace

}  // namespace webcrypto
