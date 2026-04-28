// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/hpke.h"

#include <array>
#include <vector>

#include "base/containers/span.h"
#include "crypto/keypair.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crypto::hpke {
namespace {

const HpkeParams kParams = {.kem = KemType::kX25519HkdfSha256,
                            .kdf = KdfType::kHkdfSha256,
                            .aead = AeadType::kChaCha20Poly1305};

TEST(HpkeTest, AuthSealOpenRoundTrip) {
  auto sender_key = crypto::keypair::PrivateKey::GenerateX25519();
  auto recipient_key = crypto::keypair::PrivateKey::GenerateX25519();

  auto recipient_pub_bytes = recipient_key.ToX25519PublicKey();
  auto recipient_pub =
      crypto::keypair::PublicKey::FromX25519PublicKey(recipient_pub_bytes);

  auto sender_pub_bytes = sender_key.ToX25519PublicKey();
  auto sender_pub =
      crypto::keypair::PublicKey::FromX25519PublicKey(sender_pub_bytes);

  const auto plaintext = std::to_array<uint8_t>({1, 2, 3, 4, 5});
  const auto info = std::to_array<uint8_t>({6, 7, 8});

  auto encrypted =
      AuthSeal(kParams, sender_key, recipient_pub, plaintext, info, {});
  ASSERT_TRUE(encrypted.has_value());
  EXPECT_GT(encrypted->size(), 32u);  // Encapsulated key (32) + ciphertext

  auto decrypted =
      AuthOpen(kParams, recipient_key, sender_pub, *encrypted, info, {});
  ASSERT_TRUE(decrypted.has_value());
  EXPECT_THAT(*decrypted, testing::ElementsAreArray(plaintext));
}

TEST(HpkeTest, AuthOpenCorruptedCiphertext) {
  auto sender_key = crypto::keypair::PrivateKey::GenerateX25519();
  auto recipient_key = crypto::keypair::PrivateKey::GenerateX25519();

  auto recipient_pub_bytes = recipient_key.ToX25519PublicKey();
  auto recipient_pub =
      crypto::keypair::PublicKey::FromX25519PublicKey(recipient_pub_bytes);

  auto sender_pub_bytes = sender_key.ToX25519PublicKey();
  auto sender_pub =
      crypto::keypair::PublicKey::FromX25519PublicKey(sender_pub_bytes);

  const auto plaintext = std::to_array<uint8_t>({1, 2, 3, 4, 5});
  const auto info = std::to_array<uint8_t>({6, 7, 8});

  auto encrypted =
      AuthSeal(kParams, sender_key, recipient_pub, plaintext, info, {});
  ASSERT_TRUE(encrypted.has_value());

  // Corrupt the ciphertext (last byte)
  (*encrypted)[encrypted->size() - 1] ^= 0xFF;

  auto decrypted =
      AuthOpen(kParams, recipient_key, sender_pub, *encrypted, info, {});
  EXPECT_FALSE(decrypted.has_value());
}

TEST(HpkeTest, AuthOpenWrongInfo) {
  auto sender_key = crypto::keypair::PrivateKey::GenerateX25519();
  auto recipient_key = crypto::keypair::PrivateKey::GenerateX25519();

  auto recipient_pub_bytes = recipient_key.ToX25519PublicKey();
  auto recipient_pub =
      crypto::keypair::PublicKey::FromX25519PublicKey(recipient_pub_bytes);

  auto sender_pub_bytes = sender_key.ToX25519PublicKey();
  auto sender_pub =
      crypto::keypair::PublicKey::FromX25519PublicKey(sender_pub_bytes);

  const auto plaintext = std::to_array<uint8_t>({1, 2, 3, 4, 5});
  const auto info = std::to_array<uint8_t>({6, 7, 8});
  const auto wrong_info = std::to_array<uint8_t>({6, 7, 9});

  auto encrypted =
      AuthSeal(kParams, sender_key, recipient_pub, plaintext, info, {});
  ASSERT_TRUE(encrypted.has_value());

  auto decrypted =
      AuthOpen(kParams, recipient_key, sender_pub, *encrypted, wrong_info, {});
  EXPECT_FALSE(decrypted.has_value());
}

TEST(HpkeTest, AuthOpenTooShortEncryptedData) {
  auto recipient_key = crypto::keypair::PrivateKey::GenerateX25519();
  auto sender_key = crypto::keypair::PrivateKey::GenerateX25519();

  auto sender_pub_bytes = sender_key.ToX25519PublicKey();
  auto sender_pub =
      crypto::keypair::PublicKey::FromX25519PublicKey(sender_pub_bytes);

  const auto too_short_data =
      std::to_array<uint8_t>({1, 2, 3});  // Less than 32 bytes
  const std::array<uint8_t, 0> info = {};

  auto decrypted =
      AuthOpen(kParams, recipient_key, sender_pub, too_short_data, info, {});
  EXPECT_FALSE(decrypted.has_value());
}

TEST(HpkeTest, AuthOpenKnownAnswer) {
  // RFC 9180, Appendix A.2.3. Auth Setup Information
  // mode: 2 kem_id: 32 kdf_id: 1 aead_id: 3

  const auto skRm = std::to_array<uint8_t>(
      {0x3c, 0xa2, 0x2a, 0x6d, 0x1c, 0xda, 0x1b, 0xb9, 0x48, 0x09, 0x49,
       0xec, 0x53, 0x29, 0xd3, 0xbf, 0x0b, 0x08, 0x0c, 0xa4, 0xc4, 0x58,
       0x79, 0xc9, 0x5e, 0xdd, 0xb5, 0x5c, 0x70, 0xb8, 0x0b, 0x82});

  const auto pkSm = std::to_array<uint8_t>(
      {0xf0, 0xf4, 0xf9, 0xe9, 0x6c, 0x54, 0xae, 0xed, 0x3f, 0x32, 0x3d,
       0xe8, 0x53, 0x4f, 0xff, 0xd7, 0xe0, 0x57, 0x7e, 0x4c, 0xe2, 0x69,
       0x89, 0x67, 0x16, 0xbc, 0xb9, 0x56, 0x43, 0xc8, 0x71, 0x2b});

  const auto info = std::to_array<uint8_t>(
      {0x4f, 0x64, 0x65, 0x20, 0x6f, 0x6e, 0x20, 0x61, 0x20, 0x47,
       0x72, 0x65, 0x63, 0x69, 0x61, 0x6e, 0x20, 0x55, 0x72, 0x6e});

  // enc (32 bytes) + ct (49 bytes)
  const auto encrypted_data = std::to_array<uint8_t>(
      {// enc
       0xf7, 0x67, 0x4c, 0xc8, 0xcd, 0x7b, 0xaa, 0x58, 0x72, 0xd1, 0xf3, 0x3d,
       0xba, 0xff, 0xe3, 0x31, 0x42, 0x39, 0xf6, 0x19, 0x7d, 0xdf, 0x5d, 0xed,
       0x17, 0x46, 0x76, 0x0b, 0xfc, 0x84, 0x7e, 0x0e,
       // ct
       0xab, 0x1a, 0x13, 0xc9, 0xd4, 0xf0, 0x1a, 0x87, 0xec, 0x34, 0x40, 0xdb,
       0xd7, 0x56, 0xe2, 0x67, 0x7b, 0xd2, 0xec, 0xf9, 0xdf, 0x0c, 0xe7, 0xed,
       0x73, 0x86, 0x9b, 0x98, 0xe0, 0x0c, 0x09, 0xbe, 0x11, 0x1c, 0xb9, 0xfd,
       0xf0, 0x77, 0x34, 0x7a, 0xeb, 0x88, 0xe6, 0x1b, 0xdf});

  const auto expected_plaintext = std::to_array<uint8_t>(
      {0x42, 0x65, 0x61, 0x75, 0x74, 0x79, 0x20, 0x69, 0x73, 0x20,
       0x74, 0x72, 0x75, 0x74, 0x68, 0x2c, 0x20, 0x74, 0x72, 0x75,
       0x74, 0x68, 0x20, 0x62, 0x65, 0x61, 0x75, 0x74, 0x79});

  auto recipient_priv = crypto::keypair::PrivateKey::FromX25519PrivateKey(skRm);
  auto sender_pub = crypto::keypair::PublicKey::FromX25519PublicKey(pkSm);

  const auto ad = std::to_array<uint8_t>(
      {0x43, 0x6f, 0x75, 0x6e, 0x74, 0x2d, 0x30});  // Count-0

  auto decrypted =
      AuthOpen(kParams, recipient_priv, sender_pub, encrypted_data, info, ad);
  ASSERT_TRUE(decrypted.has_value());
  EXPECT_THAT(*decrypted, testing::ElementsAreArray(expected_plaintext));
}

}  // namespace
}  // namespace crypto::hpke
