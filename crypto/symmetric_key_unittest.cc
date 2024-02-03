// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/symmetric_key.h"

#include <memory>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(SymmetricKeyTest, GenerateRandomKey) {
  std::unique_ptr<crypto::SymmetricKey> key(
      crypto::SymmetricKey::GenerateRandomKey(crypto::SymmetricKey::AES, 256));
  ASSERT_TRUE(key);
  EXPECT_EQ(32U, key->key().size());

  // Do it again and check that the keys are different.
  // (Note: this has a one-in-10^77 chance of failure!)
  std::unique_ptr<crypto::SymmetricKey> key2(
      crypto::SymmetricKey::GenerateRandomKey(crypto::SymmetricKey::AES, 256));
  ASSERT_TRUE(key2);
  EXPECT_EQ(32U, key2->key().size());
  EXPECT_NE(key->key(), key2->key());
}

TEST(SymmetricKeyTest, ImportGeneratedKey) {
  std::unique_ptr<crypto::SymmetricKey> key1(
      crypto::SymmetricKey::GenerateRandomKey(crypto::SymmetricKey::AES, 256));
  ASSERT_TRUE(key1);

  std::unique_ptr<crypto::SymmetricKey> key2(
      crypto::SymmetricKey::Import(crypto::SymmetricKey::AES, key1->key()));
  ASSERT_TRUE(key2);

  EXPECT_EQ(key1->key(), key2->key());
}

TEST(SymmetricKeyTest, ImportDerivedKey) {
  std::unique_ptr<crypto::SymmetricKey> key1(
      crypto::SymmetricKey::DeriveKeyFromPasswordUsingPbkdf2(
          crypto::SymmetricKey::HMAC_SHA1, "password", "somesalt", 1024, 160));
  ASSERT_TRUE(key1);

  std::unique_ptr<crypto::SymmetricKey> key2(crypto::SymmetricKey::Import(
      crypto::SymmetricKey::HMAC_SHA1, key1->key()));
  ASSERT_TRUE(key2);

  EXPECT_EQ(key1->key(), key2->key());
}

struct PBKDF2TestVector {
  crypto::SymmetricKey::Algorithm algorithm;
  const char* password;
  const char* salt;
  unsigned int rounds;
  unsigned int key_size_in_bits;
  const char* expected;  // ASCII encoded hex bytes.
};

struct ScryptTestVector {
  crypto::SymmetricKey::Algorithm algorithm;
  const char* password;
  const char* salt;
  unsigned int cost_parameter;
  unsigned int block_size;
  unsigned int parallelization_parameter;
  unsigned int key_size_in_bits;
  const char* expected;  // ASCII encoded hex bytes.
};

class SymmetricKeyDeriveKeyFromPasswordUsingPbkdf2Test
    : public testing::TestWithParam<PBKDF2TestVector> {};

class SymmetricKeyDeriveKeyFromPasswordUsingScryptTest
    : public testing::TestWithParam<ScryptTestVector> {};

TEST_P(SymmetricKeyDeriveKeyFromPasswordUsingPbkdf2Test,
       DeriveKeyFromPasswordUsingPbkdf2) {
  PBKDF2TestVector test_data(GetParam());
  std::unique_ptr<crypto::SymmetricKey> key(
      crypto::SymmetricKey::DeriveKeyFromPasswordUsingPbkdf2(
          test_data.algorithm, test_data.password, test_data.salt,
          test_data.rounds, test_data.key_size_in_bits));
  ASSERT_TRUE(key);

  const std::string& raw_key = key->key();
  EXPECT_EQ(test_data.key_size_in_bits / 8, raw_key.size());
  EXPECT_EQ(test_data.expected, base::ToLowerASCII(base::HexEncode(raw_key)));
}

TEST_P(SymmetricKeyDeriveKeyFromPasswordUsingScryptTest,
       DeriveKeyFromPasswordUsingScrypt) {
  const int kScryptMaxMemoryBytes = 128 * 1024 * 1024;  // 128 MiB.

  ScryptTestVector test_data(GetParam());
  std::unique_ptr<crypto::SymmetricKey> key(
      crypto::SymmetricKey::DeriveKeyFromPasswordUsingScrypt(
          test_data.algorithm, test_data.password, test_data.salt,
          test_data.cost_parameter, test_data.block_size,
          test_data.parallelization_parameter, kScryptMaxMemoryBytes,
          test_data.key_size_in_bits));
  ASSERT_TRUE(key);

  const std::string& raw_key = key->key();
  EXPECT_EQ(test_data.key_size_in_bits / 8, raw_key.size());
  EXPECT_EQ(test_data.expected, base::ToLowerASCII(base::HexEncode(
                                    raw_key.data(), raw_key.size())));
}

static const PBKDF2TestVector kTestVectorsPbkdf2[] = {
    // These tests come from
    // http://www.ietf.org/id/draft-josefsson-pbkdf2-test-vectors-00.txt.
    {
        crypto::SymmetricKey::HMAC_SHA1, "password", "salt", 1, 160,
        "0c60c80f961f0e71f3a9b524af6012062fe037a6",
    },
    {
        crypto::SymmetricKey::HMAC_SHA1, "password", "salt", 2, 160,
        "ea6c014dc72d6f8ccd1ed92ace1d41f0d8de8957",
    },
    {
        crypto::SymmetricKey::HMAC_SHA1, "password", "salt", 4096, 160,
        "4b007901b765489abead49d926f721d065a429c1",
    },
// This test takes over 30s to run on the trybots.
#if 0
  {
    crypto::SymmetricKey::HMAC_SHA1,
    "password",
    "salt",
    16777216,
    160,
    "eefe3d61cd4da4e4e9945b3d6ba2158c2634e984",
  },
#endif

    // These tests come from RFC 3962, via BSD source code at
    // http://www.openbsd.org/cgi-bin/cvsweb/src/sbin/bioctl/pbkdf2.c?rev=HEAD&content-type=text/plain.
    {
        crypto::SymmetricKey::HMAC_SHA1, "password", "ATHENA.MIT.EDUraeburn", 1,
        160, "cdedb5281bb2f801565a1122b25635150ad1f7a0",
    },
    {
        crypto::SymmetricKey::HMAC_SHA1, "password", "ATHENA.MIT.EDUraeburn", 2,
        160, "01dbee7f4a9e243e988b62c73cda935da05378b9",
    },
    {
        crypto::SymmetricKey::HMAC_SHA1, "password", "ATHENA.MIT.EDUraeburn",
        1200, 160, "5c08eb61fdf71e4e4ec3cf6ba1f5512ba7e52ddb",
    },
    {
        crypto::SymmetricKey::HMAC_SHA1, "password",
        "\022"
        "4VxxV4\022", /* 0x1234567878563412 */
        5, 160, "d1daa78615f287e6a1c8b120d7062a493f98d203",
    },
    {
        crypto::SymmetricKey::HMAC_SHA1,
        "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
        "pass phrase equals block size", 1200, 160,
        "139c30c0966bc32ba55fdbf212530ac9c5ec59f1",
    },
    {
        crypto::SymmetricKey::HMAC_SHA1,
        "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
        "pass phrase exceeds block size", 1200, 160,
        "9ccad6d468770cd51b10e6a68721be611a8b4d28",
    },
    {
        crypto::SymmetricKey::HMAC_SHA1,
        "\360\235\204\236", /* g-clef (0xf09d849e) */
        "EXAMPLE.COMpianist", 50, 160,
        "6b9cf26d45455a43a5b8bb276a403b39e7fe37a0",
    },

    // Regression tests for AES keys, derived from the Linux NSS implementation.
    {
        crypto::SymmetricKey::AES, "A test password", "saltsalt", 1, 256,
        "44899a7777f0e6e8b752f875f02044b8ac593de146de896f2e8a816e315a36de",
    },
    {
        crypto::SymmetricKey::AES,
        "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
        "pass phrase exceeds block size", 20, 256,
        "e0739745dc28b8721ba402e05214d2ac1eab54cf72bee1fba388297a09eb493c",
    },
};

static const ScryptTestVector kTestVectorsScrypt[] = {
    // From RFC 7914, "The scrypt Password-Based Key Derivation Function",
    // https://tools.ietf.org/html/rfc7914.html. The fourth test vector is
    // intentionally not used, as it would make the test significantly slower,
    // due to the very high cost parameter.
    {crypto::SymmetricKey::HMAC_SHA1, "", "", 16, 1, 1, 512,
     "77d6576238657b203b19ca42c18a0497f16b4844e3074ae8dfdffa3fede21442fcd0069de"
     "d0948f8326a753a0fc81f17e8d3e0fb2e0d3628cf35e20c38d18906"},
    {crypto::SymmetricKey::HMAC_SHA1, "password", "NaCl", 1024, 8, 16, 512,
     "fdbabe1c9d3472007856e7190d01e9fe7c6ad7cbc8237830e77376634b3731622eaf30d92"
     "e22a3886ff109279d9830dac727afb94a83ee6d8360cbdfa2cc0640"},
    {crypto::SymmetricKey::HMAC_SHA1, "pleaseletmein", "SodiumChloride", 16384,
     8, 1, 512,
     "7023bdcb3afd7348461c06cd81fd38ebfda8fbba904f8e3ea9b543f6545da1f2d54329556"
     "13f0fcf62d49705242a9af9e61e85dc0d651e40dfcf017b45575887"}};

INSTANTIATE_TEST_SUITE_P(All,
                         SymmetricKeyDeriveKeyFromPasswordUsingPbkdf2Test,
                         testing::ValuesIn(kTestVectorsPbkdf2));

INSTANTIATE_TEST_SUITE_P(All,
                         SymmetricKeyDeriveKeyFromPasswordUsingScryptTest,
                         testing::ValuesIn(kTestVectorsScrypt));
