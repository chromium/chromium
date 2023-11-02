// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/strings/string_number_conversions.h"
#include "components/webcrypto/algorithm_dispatch.h"
#include "components/webcrypto/algorithms/test_helpers.h"
#include "components/webcrypto/jwk.h"
#include "components/webcrypto/status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_crypto_key.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"

namespace webcrypto {

namespace {

blink::WebCryptoAlgorithm CreateRsaPssAlgorithm(
    unsigned int salt_length_bytes) {
  return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
      blink::kWebCryptoAlgorithmIdRsaPss,
      new blink::WebCryptoRsaPssParams(salt_length_bytes));
}

class WebCryptoRsaPssTest : public WebCryptoTestBase {};

// Test that no two RSA-PSS signatures are identical, when using a non-zero
// lengthed salt.
TEST_F(WebCryptoRsaPssTest, SignIsRandom) {
  // Import public/private key pair.
  blink::WebCryptoKey public_key = blink::WebCryptoKey::CreateNull();
  blink::WebCryptoKey private_key = blink::WebCryptoKey::CreateNull();

  ImportRsaKeyPair(
      HexStringToBytes(kPublicKeySpkiDerHex),
      HexStringToBytes(kPrivateKeyPkcs8DerHex),
      CreateRsaHashedImportAlgorithm(blink::kWebCryptoAlgorithmIdRsaPss,
                                     blink::kWebCryptoAlgorithmIdSha1),
      true, blink::kWebCryptoKeyUsageVerify, blink::kWebCryptoKeyUsageSign,
      &public_key, &private_key);

  // Use a 20-byte length salt.
  blink::WebCryptoAlgorithm params = CreateRsaPssAlgorithm(20);

  // Some random message to sign.
  std::vector<uint8_t> message = HexStringToBytes(kPublicKeySpkiDerHex);

  // Sign twice.
  std::vector<uint8_t> signature1;
  std::vector<uint8_t> signature2;

  ASSERT_EQ(Status::Success(), Sign(params, private_key, message, &signature1));
  ASSERT_EQ(Status::Success(), Sign(params, private_key, message, &signature2));

  // The signatures will be different because of the salt.
  EXPECT_NE(signature1, signature2);

  // However both signatures should work when verifying.
  bool is_match = false;

  ASSERT_EQ(Status::Success(),
            Verify(params, public_key, signature1, message, &is_match));
  EXPECT_TRUE(is_match);

  ASSERT_EQ(Status::Success(),
            Verify(params, public_key, signature2, message, &is_match));
  EXPECT_TRUE(is_match);

  // Corrupt the signature and verification must fail.
  ASSERT_EQ(Status::Success(), Verify(params, public_key, Corrupted(signature2),
                                      message, &is_match));
  EXPECT_FALSE(is_match);
}

// Try signing and verifying when the salt length is 0. The signature in this
// case is not random.
TEST_F(WebCryptoRsaPssTest, SignVerifyNoSalt) {
  // Import public/private key pair.
  blink::WebCryptoKey public_key = blink::WebCryptoKey::CreateNull();
  blink::WebCryptoKey private_key = blink::WebCryptoKey::CreateNull();

  ImportRsaKeyPair(
      HexStringToBytes(kPublicKeySpkiDerHex),
      HexStringToBytes(kPrivateKeyPkcs8DerHex),
      CreateRsaHashedImportAlgorithm(blink::kWebCryptoAlgorithmIdRsaPss,
                                     blink::kWebCryptoAlgorithmIdSha1),
      true, blink::kWebCryptoKeyUsageVerify, blink::kWebCryptoKeyUsageSign,
      &public_key, &private_key);

  // Zero-length salt.
  blink::WebCryptoAlgorithm params = CreateRsaPssAlgorithm(0);

  // Some random message to sign.
  std::vector<uint8_t> message = HexStringToBytes(kPublicKeySpkiDerHex);

  // Sign twice.
  std::vector<uint8_t> signature1;
  std::vector<uint8_t> signature2;

  ASSERT_EQ(Status::Success(), Sign(params, private_key, message, &signature1));
  ASSERT_EQ(Status::Success(), Sign(params, private_key, message, &signature2));

  // The signatures will be the same this time.
  EXPECT_EQ(signature1, signature2);

  // Make sure that verification works.
  bool is_match = false;
  ASSERT_EQ(Status::Success(),
            Verify(params, public_key, signature1, message, &is_match));
  EXPECT_TRUE(is_match);

  // Corrupt the signature and verification must fail.
  ASSERT_EQ(Status::Success(), Verify(params, public_key, Corrupted(signature2),
                                      message, &is_match));
  EXPECT_FALSE(is_match);
}

TEST_F(WebCryptoRsaPssTest, SignEmptyMessage) {
  // Import public/private key pair.
  blink::WebCryptoKey public_key = blink::WebCryptoKey::CreateNull();
  blink::WebCryptoKey private_key = blink::WebCryptoKey::CreateNull();

  ImportRsaKeyPair(
      HexStringToBytes(kPublicKeySpkiDerHex),
      HexStringToBytes(kPrivateKeyPkcs8DerHex),
      CreateRsaHashedImportAlgorithm(blink::kWebCryptoAlgorithmIdRsaPss,
                                     blink::kWebCryptoAlgorithmIdSha1),
      true, blink::kWebCryptoKeyUsageVerify, blink::kWebCryptoKeyUsageSign,
      &public_key, &private_key);

  blink::WebCryptoAlgorithm params = CreateRsaPssAlgorithm(20);
  std::vector<uint8_t> message;  // Empty message.
  std::vector<uint8_t> signature;

  ASSERT_EQ(Status::Success(), Sign(params, private_key, message, &signature));

  // Make sure that verification works.
  bool is_match = false;
  ASSERT_EQ(Status::Success(),
            Verify(params, public_key, signature, message, &is_match));
  EXPECT_TRUE(is_match);

  // Corrupt the signature and verification must fail.
  ASSERT_EQ(Status::Success(), Verify(params, public_key, Corrupted(signature),
                                      message, &is_match));
  EXPECT_FALSE(is_match);
}

// RSA-PSS known answer tests:
const char kKey1[] =
    "30819F300D06092A864886F70D010101050003818D0030818902818100A56E4A0E70101758"
    "9A5187DC7EA841D156F2EC0E36AD52A44DFEB1E61F7AD991D8C51056FFEDB162B4C0F283A1"
    "2A88A394DFF526AB7291CBB307CEABFCE0B1DFD5CD9508096D5B2B8B6DF5D671EF6377C092"
    "1CB23C270A70E2598E6FF89D19F105ACC2D3F0CB35F29280E1386B6F64C4EF22E1E1F20D0C"
    "E8CFFB2249BD9A21370203010001";

const char kKey2[] =
    "30819D300D06092A864886F70D010101050003818B0030818702818100BE499B5E7F06C83F"
    "A0293E31465C8EB6B58AF920BAE52A7B5B9BFEB7AA72DB1264112EB3FD431D31A2A7E50941"
    "566929494A0E891ED5613918B4B51B0D1FB97783B26ACF7D0F384CFB35F4D2824F5DD38062"
    "3A26BF180B63961C619DCDB20CAE406F22F6E276C80A37259490CFEB72C1A71A84F1846D33"
    "0877BA3E3101EC9C7B020111";

const char kNistTestMessage[] =
    "c7f5270fca725f9bd19f519a8d7cca3cc5c079024029f3bae510f9b02140fe238908e4f6c1"
    "8f07a89c687c8684669b1f1db2baf9251a3c829faccb493084e16ec9e28d58868074a5d622"
    "1667dd6e528d16fe2c9f3db4cfaf6c4dce8c8439af38ceaaaa9ce2ecae7bc8f4a5a55e3bf9"
    "6df9cd575c4f9cb327951b8cdfe4087168";

struct RsaPssKnownAnswer {
  blink::WebCryptoAlgorithmId hash;
  const char* key;
  const char* message;
  size_t salt_length;
  const char* signature;
};

const RsaPssKnownAnswer kRsaPssKnownAnswers[] = {
    // Example 1.1 from pss-vect.txt in
    // ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1-vec.zip
    {blink::kWebCryptoAlgorithmIdSha1, kKey1,
     "cdc87da223d786df3b45e0bbbc721326d1ee2af806cc315475cc6f0d9c66e1b62371d4"
     "5ce2392e1ac92844c310102f156a0d8d52c1f4c40ba3aa65095786cb769757a6563ba9"
     "58fed0bcc984e8b517a3d5f515b23b8a41e74aa867693f90dfb061a6e86dfaaee64472"
     "c00e5f20945729cbebe77f06ce78e08f4098fba41f9d6193c0317e8b60d4b6084acb42"
     "d29e3808a3bc372d85e331170fcbf7cc72d0b71c296648b3a4d10f416295d0807aa625"
     "cab2744fd9ea8fd223c42537029828bd16be02546f130fd2e33b936d2676e08aed1b73"
     "318b750a0167d0",
     20,
     "9074308fb598e9701b2294388e52f971faac2b60a5145af185df5287b5ed2887e57ce7"
     "fd44dc8634e407c8e0e4360bc226f3ec227f9d9e54638e8d31f5051215df6ebb9c2f95"
     "79aa77598a38f914b5b9c1bd83c4e2f9f382a0d0aa3542ffee65984a601bc69eb28deb"
     "27dca12c82c2d4c3f66cd500f1ff2b994d8a4e30cbb33c"},
    // Example 1.4 from pss-vect.txt in
    // ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1-vec.zip
    {blink::kWebCryptoAlgorithmIdSha1, kKey1, "bc656747fa9eafb3f0", 20,
     "4609793b23e9d09362dc21bb47da0b4f3a7622649a47d464019b9aeafe53359c178c91"
     "cd58ba6bcb78be0346a7bc637f4b873d4bab38ee661f199634c547a1ad8442e03da015"
     "b136e543f7ab07c0c13e4225b8de8cce25d4f6eb8400f81f7e1833b7ee6e334d370964"
     "ca79fdb872b4d75223b5eeb08101591fb532d155a6de87"},
    // The next three are from SigVerPSS_186-3.rsp in
    // http://csrc.nist.gov/groups/STM/cavp/documents/dss/186-2rsatestvectors.zip
    {blink::kWebCryptoAlgorithmIdSha256, kKey2, kNistTestMessage, 10,
     "11e169f2fd40b07641b9768a2ab19965fb6c27f10fcf0323fcc6d12eb4f1c06b330dda"
     "a1ea504407afa29de9ebe0374fe9d1e7d0ffbd5fc1cf3a3446e4145415d2ab24f789b3"
     "464c5c43a256bbc1d692cf7f04801dac5bb401a4a03ab7d5728a860c19e1a4dc797ca5"
     "42c8203cec2e601eb0c51f567f2eda022b0b9ebddeeefa"},
    {blink::kWebCryptoAlgorithmIdSha384, kKey2, kNistTestMessage, 10,
     "b281ad934b2775c0cba5fb10aa574d2ed85c7f99b942b78e49702480069362ed394bad"
     "ed55e56cfcbe7b0b8d2217a05a60e1acd725cb09060dfac585bc2132b99b41cdbd530c"
     "69d17cdbc84bc6b9830fc7dc8e1b2412cfe06dcf8c1a0cc3453f93f25ebf10cb0c9033"
     "4fac573f449138616e1a194c67f44efac34cc07a526267"},
    {blink::kWebCryptoAlgorithmIdSha512, kKey2, kNistTestMessage, 10,
     "8ffc38f9b820ef6b080fd2ec7de5626c658d79056f3edf610a295b7b0546f73e01ffdf4d0"
     "070ebf79c33fd86c2d608be9438b3d420d09535b97cd3d846ecaf8f6551cdf93197e9f8fb"
     "048044473ab41a801e9f7fc983c62b324361dade9f71a65952bd35c59faaa4d6ff462f68a"
     "6c4ec0b428aa47336f2178aeb276136563b7d"},
};

blink::WebCryptoKey RsaPssKeyFromBytes(blink::WebCryptoAlgorithmId hash,
                                       const char* key) {
  auto pubkey = blink::WebCryptoKey::CreateNull();
  webcrypto::Status result = ImportKey(
      blink::kWebCryptoKeyFormatSpki, HexStringToBytes(key),
      CreateRsaHashedImportAlgorithm(blink::kWebCryptoAlgorithmIdRsaPss, hash),
      true, blink::kWebCryptoKeyUsageVerify, &pubkey);
  CHECK(result == Status::Success());
  return pubkey;
}

bool VerifySignature(size_t salt_length,
                     blink::WebCryptoKey pubkey,
                     const std::vector<uint8_t>& signature,
                     const std::vector<uint8_t>& message) {
  bool is_match = false;
  auto result = Verify(CreateRsaPssAlgorithm(salt_length), pubkey, signature,
                       message, &is_match);
  CHECK(result == Status::Success());
  return is_match;
}

// Iterate through known answers and test verification.
//   * Verify over original message should succeed
//   * Verify over corrupted message should fail
//   * Verification with corrupted signature should fail
TEST_F(WebCryptoRsaPssTest, VerifyKnownAnswer) {
  for (const auto& test : kRsaPssKnownAnswers) {
    SCOPED_TRACE(&test - &kRsaPssKnownAnswers[0]);
    blink::WebCryptoKey pubkey = RsaPssKeyFromBytes(test.hash, test.key);

    std::vector<uint8_t> message = HexStringToBytes(test.message);
    std::vector<uint8_t> signature = HexStringToBytes(test.signature);

    EXPECT_TRUE(VerifySignature(test.salt_length, pubkey, signature, message));

    EXPECT_FALSE(VerifySignature(test.salt_length, pubkey, signature,
                                 Corrupted(message)));
    EXPECT_FALSE(VerifySignature(test.salt_length, pubkey, Corrupted(signature),
                                 message));
  }
}

}  // namespace

}  // namespace webcrypto
