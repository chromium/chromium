// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"
#include "components/webcrypto/algorithm_dispatch.h"
#include "components/webcrypto/algorithms/test_helpers.h"
#include "components/webcrypto/status.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"

namespace webcrypto {

namespace {

// Creates an AES-CTR algorithm for encryption/decryption.
blink::WebCryptoAlgorithm CreateAesCtrAlgorithm(
    const std::vector<uint8_t>& counter,
    uint8_t length_bits) {
  return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
      blink::kWebCryptoAlgorithmIdAesCtr,
      new blink::WebCryptoAesCtrParams(length_bits, counter));
}

blink::WebCryptoKey AesCtrKeyFromBytes(const std::vector<uint8_t>& bytes) {
  blink::WebCryptoKey key = ImportSecretKeyFromRaw(
      bytes, CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCtr),
      blink::kWebCryptoKeyUsageEncrypt | blink::kWebCryptoKeyUsageDecrypt);
  EXPECT_EQ(bytes.size() * 8, key.Algorithm().AesParams()->LengthBits());
  return key;
}

class WebCryptoAesCtrTest : public WebCryptoTestBase {};

struct AesCtrKnownAnswer {
  const char* key;
  const char* plaintext;
  const char* counter;
  size_t counter_length;
  const char* ciphertext;
};

const char k128BitTestKey[] = "7691BE035E5020A8AC6E618529F9A0DC";
const char k256BitTestKey[] =
    "F6D66D6BD52D59BB0796365879EFF886C66DD51A5B6A99744B50590C87A23884";

const AesCtrKnownAnswer kAesCtrKnownAnswers[] = {
    // RFC 3686 test vector #3:
    {k128BitTestKey,
     "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F20212223",
     "00E0017B27777F3F4A1786F000000001", 32,
     "C1CF48A89F2FFDD9CF4652E9EFDB72D74540A42BDE6D7836D59A5CEAAEF3105325B2072"
     "F"},
    // RFC 3686 test vector #8:
    {k256BitTestKey,
     "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F",
     "00FAAC24C1585EF15A43D87500000001", 32,
     "F05E231B3894612C49EE000B804EB2A9B8306B508F839D6A5530831D9344AF1C"},
    // Empty plaintext, same key as above:
    {k256BitTestKey, "", "00FAAC24C1585EF15A43D87500000001", 32, ""},
    // 32-bit counter wraparound:
    {k256BitTestKey,
     "F05E231B3894612C49EE000B804EB2A9B8306B508F839D6A5530831D9344AF1CC1CF48A89"
     "F2FFDD9CF4652E9EFDB72D7",
     "00FAAC24C1585EF15A43D875FFFFFFFF", 32,
     "2E32E02FF9E69A1D6B78AC4308A67592C5DD5505589B79183D4189619A1467E4319069B0A"
     "3BE9AF28EA158E96398CE71"},
    // 1-bit counter wraparound:
    {k128BitTestKey,
     "C05E231B3894612C49EE000B804EB2A6B8306B508F839D6A5530831D9344AF1C",
     "00FAAC24C1585EF15A43D875000000FF", 1,
     "52334727723A84F4278FB319386CD7B5587DD8B2D9AA394D83EF8A826C4761AA"},
    // 4-bit counter wraparound:
    {k128BitTestKey,
     "C05E231B3894612C49EE000B804EB2A6B8306B508F839D6A5530831D9344AF1C141516171"
     "8191A1B1C1D1E1F20212223",
     "00FAAC24C1585EF15A43D8750000111E", 4,
     "5573894046DEF46162ED54966A22D8F0517B61A0CE7E657A5A5124A7F62AAE149A3C78567"
     "11C59D67F34F31374CF7A72"},
    // same, but plaintext is not a multiple of block size:
    {k128BitTestKey,
     "C05E231B3894612C49EE000B804EB2A6B8306B508F839D6A5530831D9344AF1C141516171"
     "8191A1B1C1D1E1F20",
     "00FAAC24C1585EF15A43D8750000111E", 4,
     "5573894046DEF46162ED54966A22D8F0517B61A0CE7E657A5A5124A7F62AAE149A3C78567"
     "11C59D67F34F31374"},
    // 128-bit counter wraparound:
    {k128BitTestKey,
     "C05E231B3894612C49EE000B804EB2A6B8306B508F839D6A5530831D9344AF1C141516171"
     "8191A1B1C1D1E1F20212223",
     "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFE", 128,
     "D2C49B275BC73814DC90ECE98959041C9A3481F2247E08B0AF5D8DE3F521C9DAF535B0A81"
     "56DF9D2370EE7328103C8AD"},
};

TEST_F(WebCryptoAesCtrTest, EncryptDecryptKnownAnswer) {
  for (const auto& test : kAesCtrKnownAnswers) {
    SCOPED_TRACE(&test - &kAesCtrKnownAnswers[0]);

    std::vector<uint8_t> key_bytes = HexStringToBytes(test.key);
    std::vector<uint8_t> counter = HexStringToBytes(test.counter);
    std::vector<uint8_t> plaintext = HexStringToBytes(test.plaintext);
    std::vector<uint8_t> ciphertext = HexStringToBytes(test.ciphertext);

    blink::WebCryptoKey key = AesCtrKeyFromBytes(key_bytes);

    std::vector<uint8_t> output;

    // Test encryption.
    EXPECT_EQ(Status::Success(),
              Encrypt(CreateAesCtrAlgorithm(counter, test.counter_length), key,
                      plaintext, &output));
    EXPECT_EQ(ciphertext, output);

    // Test decryption.
    EXPECT_EQ(Status::Success(),
              Decrypt(CreateAesCtrAlgorithm(counter, test.counter_length), key,
                      ciphertext, &output));
    EXPECT_EQ(plaintext, output);
  }
}

// The counter block must be exactly 16 bytes.
TEST_F(WebCryptoAesCtrTest, InvalidCounterBlockLength) {
  blink::WebCryptoKey key = AesCtrKeyFromBytes(std::vector<uint8_t>(16));

  std::vector<uint8_t> input(32);
  std::vector<uint8_t> output;

  for (size_t bad_length : {0, 15, 17}) {
    std::vector<uint8_t> bad_counter(bad_length);

    EXPECT_EQ(
        Status::ErrorIncorrectSizeAesCtrCounter(),
        Encrypt(CreateAesCtrAlgorithm(bad_counter, 128), key, input, &output));

    EXPECT_EQ(
        Status::ErrorIncorrectSizeAesCtrCounter(),
        Decrypt(CreateAesCtrAlgorithm(bad_counter, 128), key, input, &output));
  }
}

TEST_F(WebCryptoAesCtrTest, InvalidCounterLength) {
  blink::WebCryptoKey key = AesCtrKeyFromBytes(std::vector<uint8_t>(16));

  std::vector<uint8_t> counter(16);
  std::vector<uint8_t> input(32);
  std::vector<uint8_t> output;

  // The counter length cannot be less than 1 or greater than 128.
  for (uint8_t bad_length : {0, 129}) {
    EXPECT_EQ(Status::ErrorInvalidAesCtrCounterLength(),
              Encrypt(CreateAesCtrAlgorithm(counter, bad_length), key, input,
                      &output));

    EXPECT_EQ(Status::ErrorInvalidAesCtrCounterLength(),
              Decrypt(CreateAesCtrAlgorithm(counter, bad_length), key, input,
                      &output));
  }
}

// Tests wrap-around using a 4-bit counter.
//
// Wrap-around is allowed, however if the counter repeats itself an error should
// be thrown.
//
// Using a 4-bit counter it is possible to encrypt 16 blocks. However the 17th
// block would end up wrapping back to the starting value.
TEST_F(WebCryptoAesCtrTest, OverflowAndRepeatCounter) {
  const uint8_t kCounterLengthBits = 4;

  blink::WebCryptoKey key = AesCtrKeyFromBytes(std::vector<uint8_t>(16));

  std::vector<uint8_t> buffer(272);

  // 16 and 17 AES blocks worth of data respectively (AES blocks are 16 bytes
  // long).
  auto input_16 = base::make_span(buffer).first(256u);
  auto input_17 = base::make_span(buffer).first(272u);

  std::vector<uint8_t> output;

  for (uint8_t start : {0, 1, 15}) {
    std::vector<uint8_t> counter(16);
    counter[15] = start;

    // Baseline test: Encrypting 16 blocks should work (don't bother to check
    // output, the known answer tests already do that).
    EXPECT_EQ(Status::Success(),
              Encrypt(CreateAesCtrAlgorithm(counter, kCounterLengthBits), key,
                      input_16, &output));

    // Encrypting/Decrypting 17 however should fail.
    EXPECT_EQ(Status::ErrorAesCtrInputTooLongCounterRepeated(),
              Encrypt(CreateAesCtrAlgorithm(counter, kCounterLengthBits), key,
                      input_17, &output));
    EXPECT_EQ(Status::ErrorAesCtrInputTooLongCounterRepeated(),
              Decrypt(CreateAesCtrAlgorithm(counter, kCounterLengthBits), key,
                      input_17, &output));
  }
}

}  // namespace

}  // namespace webcrypto
