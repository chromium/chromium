// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/encrypt.h"

#include <array>

#include "base/strings/string_number_conversions.h"
#include "crypto/keypair.h"
#include "crypto/test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crypto::encrypt {
namespace {

TEST(EncryptTest, RsaOaepSha1) {
  // Use hardcoded test keys.
  crypto::keypair::PrivateKey private_key =
      crypto::test::FixedRsa2048PrivateKeyForTesting();
  crypto::keypair::PublicKey public_key =
      crypto::test::FixedRsa2048PublicKeyForTesting();

  std::vector<uint8_t> plaintext = {1, 2, 3, 4, 5};

  // Encrypt the plaintext.
  auto ciphertext = Encrypt(RSA_OAEP_SHA1, public_key, plaintext);
  EXPECT_NE(plaintext, ciphertext);
  EXPECT_EQ(ciphertext.size(), 256u);  // RSA 2048 ciphertext size is 256 bytes.

  // Decrypt the ciphertext.
  auto decrypted = Decrypt(RSA_OAEP_SHA1, private_key, ciphertext);
  ASSERT_TRUE(decrypted.has_value());
  EXPECT_EQ(plaintext, *decrypted);
}

TEST(EncryptTest, RsaOaepSha1KnownAnswer) {
  crypto::keypair::PrivateKey private_key =
      crypto::test::FixedRsa2048PrivateKeyForTesting();

  // Ciphertext generated for "chromium" using RSA-OAEP-SHA1 with the fixed
  // 2048-bit key.
  static constexpr auto kCiphertext = std::to_array<uint8_t>(
      {0x73, 0xef, 0xe5, 0x25, 0x82, 0x7e, 0x47, 0x6a, 0xfe, 0xcb, 0xdf, 0xc4,
       0xfc, 0x18, 0xf1, 0x92, 0x47, 0x32, 0x31, 0xed, 0x97, 0x34, 0x9f, 0xad,
       0xf8, 0xcc, 0x3f, 0x22, 0x31, 0x97, 0xb1, 0x56, 0x9c, 0xb0, 0x3f, 0xf8,
       0x46, 0xbd, 0x71, 0xe6, 0x18, 0xde, 0x0a, 0x80, 0x48, 0x1b, 0x94, 0x04,
       0x9c, 0x43, 0xdf, 0xe9, 0xce, 0x0b, 0x25, 0x3a, 0xb1, 0x77, 0xd9, 0x06,
       0x40, 0xb6, 0x37, 0xb4, 0x90, 0x63, 0x42, 0xa8, 0x85, 0xe8, 0xeb, 0x67,
       0xff, 0x38, 0x27, 0x5f, 0x02, 0x4c, 0x78, 0x21, 0x42, 0x19, 0x79, 0xbf,
       0xb8, 0x31, 0x11, 0xfc, 0x54, 0x59, 0xda, 0xc3, 0x18, 0xb6, 0x1e, 0x61,
       0xf0, 0x69, 0x70, 0x74, 0xae, 0x36, 0xa3, 0xb7, 0xd5, 0xe2, 0xac, 0xea,
       0x4f, 0xd5, 0x26, 0x0f, 0x9a, 0xd9, 0x99, 0xd2, 0x1b, 0x21, 0x91, 0x3c,
       0xc7, 0xb9, 0xc5, 0x48, 0x83, 0x1e, 0xce, 0x21, 0xa9, 0x74, 0xeb, 0xc9,
       0x4c, 0x23, 0xad, 0xe7, 0x1b, 0xd5, 0x93, 0x69, 0x3f, 0x78, 0x1e, 0xf1,
       0xb2, 0x31, 0x39, 0xcc, 0x8b, 0x0f, 0xe0, 0x4d, 0x3c, 0xae, 0xc0, 0x2d,
       0x03, 0xb0, 0xc7, 0x70, 0xce, 0xb4, 0x54, 0xce, 0x5f, 0xa2, 0x56, 0x3c,
       0xe9, 0x99, 0xa7, 0x47, 0x87, 0x5d, 0xe0, 0x8a, 0x4d, 0x2f, 0xb3, 0x7a,
       0xaa, 0x26, 0x0f, 0x3b, 0x91, 0x27, 0xfe, 0x37, 0xa1, 0x90, 0xfe, 0xaf,
       0x9f, 0xc4, 0x2f, 0x8f, 0xb0, 0x83, 0xdb, 0x73, 0xa9, 0x62, 0x51, 0x50,
       0x49, 0x8b, 0x94, 0xa0, 0x39, 0x63, 0xc5, 0x00, 0x10, 0x2c, 0x81, 0x6f,
       0x6e, 0x9f, 0xab, 0x38, 0xcb, 0x8e, 0x73, 0x8b, 0xf7, 0x53, 0x11, 0x30,
       0x22, 0x66, 0x1f, 0x5d, 0x3f, 0xd7, 0xb1, 0x21, 0x83, 0x0f, 0xe5, 0xfb,
       0x94, 0x09, 0xbf, 0x39, 0x16, 0xce, 0x6f, 0xb0, 0x97, 0x60, 0x61, 0xf7,
       0x22, 0xdd, 0xed, 0x12});

  auto decrypted = Decrypt(RSA_OAEP_SHA1, private_key, kCiphertext);
  ASSERT_TRUE(decrypted.has_value());

  std::vector<uint8_t> expected = {'c', 'h', 'r', 'o', 'm', 'i', 'u', 'm'};
  EXPECT_EQ(*decrypted, expected);
}

TEST(EncryptTest, TooLargeMessage) {
  crypto::keypair::PublicKey public_key =
      crypto::test::FixedRsa2048PublicKeyForTesting();
  crypto::keypair::PrivateKey private_key =
      crypto::test::FixedRsa2048PrivateKeyForTesting();

  size_t max_plaintext_size = GetMaxPlaintextSize(RSA_OAEP_SHA1, public_key);
  std::vector<uint8_t> too_large_plaintext(max_plaintext_size + 1, 0);

  // Encrypt should crash because the plaintext is too large.
  EXPECT_DEATH_IF_SUPPORTED(
      Encrypt(RSA_OAEP_SHA1, public_key, too_large_plaintext), "");

  size_t ciphertext_size = GetCiphertextSize(private_key);
  std::vector<uint8_t> too_large_ciphertext(ciphertext_size + 1, 0);

  // Decrypt should return nullopt because the ciphertext is too large.
  auto decrypted = Decrypt(RSA_OAEP_SHA1, private_key, too_large_ciphertext);
  EXPECT_FALSE(decrypted.has_value());
}

TEST(EncryptTest, GetSizes) {
  crypto::keypair::PrivateKey private_key_2048 =
      crypto::test::FixedRsa2048PrivateKeyForTesting();
  crypto::keypair::PublicKey public_key_2048 =
      crypto::test::FixedRsa2048PublicKeyForTesting();

  EXPECT_EQ(GetCiphertextSize(public_key_2048), 256u);
  EXPECT_EQ(GetCiphertextSize(private_key_2048), 256u);
  EXPECT_EQ(GetMaxPlaintextSize(RSA_OAEP_SHA1, public_key_2048), 214u);
  EXPECT_EQ(GetMaxPlaintextSize(RSA_OAEP_SHA1, private_key_2048), 214u);

  crypto::keypair::PrivateKey private_key_4096 =
      crypto::test::FixedRsa4096PrivateKeyForTesting();
  crypto::keypair::PublicKey public_key_4096 =
      crypto::test::FixedRsa4096PublicKeyForTesting();

  EXPECT_EQ(GetCiphertextSize(public_key_4096), 512u);
  EXPECT_EQ(GetCiphertextSize(private_key_4096), 512u);
  EXPECT_EQ(GetMaxPlaintextSize(RSA_OAEP_SHA1, public_key_4096), 470u);
  EXPECT_EQ(GetMaxPlaintextSize(RSA_OAEP_SHA1, private_key_4096), 470u);
}

TEST(EncryptTest, InvalidKeyType) {
  // Generate an EC key pair, which should not work with RSA OAEP.
  crypto::keypair::PrivateKey private_key =
      crypto::keypair::PrivateKey::GenerateEcP256();
  crypto::keypair::PublicKey public_key =
      crypto::keypair::PublicKey::FromPrivateKey(private_key);

  std::array<uint8_t, 5> plaintext = {1, 2, 3, 4, 5};

  // Encrypt should crash because it's not an RSA key.
  EXPECT_DEATH_IF_SUPPORTED(Encrypt(RSA_OAEP_SHA1, public_key, plaintext), "");

  // Decrypt should also crash because it's not an RSA key.
  std::vector<uint8_t> dummy_ciphertext(256, 0);
  EXPECT_DEATH_IF_SUPPORTED(
      Decrypt(RSA_OAEP_SHA1, private_key, dummy_ciphertext), "");
}

}  // namespace
}  // namespace crypto::encrypt
