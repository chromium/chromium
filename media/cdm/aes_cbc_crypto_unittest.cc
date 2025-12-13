// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/aes_cbc_crypto.h"

#include <memory>
#include <optional>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/memory/raw_span.h"
#include "crypto/aes_cbc.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/aes.h"
#include "third_party/boringssl/src/include/openssl/crypto.h"
#include "third_party/boringssl/src/include/openssl/err.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace media {

namespace {

// Pattern decryption uses 16-byte blocks.
constexpr size_t kBlockSize = 16;
constexpr size_t kKeySize = 16;

const std::array<uint8_t, kKeySize> kKey1{0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
                                          0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
                                          0x10, 0x11, 0x12, 0x13};

const std::array<uint8_t, kKeySize> kKey2{0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11,
                                          0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                                          0x18, 0x19, 0x1a, 0x1b};

const std::array<uint8_t, kBlockSize> kIv{0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
                                          0x26, 0x27, 0x00, 0x00, 0x00, 0x00,
                                          0x00, 0x00, 0x00, 0x00};

const std::array<uint8_t, kBlockSize> kOneBlock{'a', 'b', 'c', 'd', 'e', 'f',
                                                'g', 'h', 'i', 'j', 'k', 'l',
                                                'm', 'n', 'o', 'p'};

// Returns a std::vector<uint8_t> containing |count| copies of |input|.
std::vector<uint8_t> Repeat(base::span<const uint8_t> input, size_t count) {
  std::vector<uint8_t> result;
  for (size_t i = 0; i < count; ++i)
    result.insert(result.end(), input.begin(), input.end());
  return result;
}

// Encrypt |original| using AES-CBC encryption with |key| and |iv|.
std::vector<uint8_t> Encrypt(
    base::span<const uint8_t> plaintext,
    base::span<const uint8_t> key,
    base::span<const uint8_t, crypto::aes_cbc::kBlockSize> iv) {
  auto ciphertext = crypto::aes_cbc::Encrypt(key, iv, plaintext);

  // Strip off the PKCS#5 padding block.
  ciphertext.resize(plaintext.size());

  return ciphertext;
}

}  // namespace

using AesCbcCryptoTest = ::testing::Test;

TEST(AesCbcCryptoTest, OneBlock) {
  auto encrypted_block = Encrypt(kOneBlock, kKey1, kIv);
  EXPECT_EQ(kBlockSize, encrypted_block.size());

  AesCbcCrypto crypto;
  EXPECT_TRUE(crypto.Initialize(kKey1, kIv));

  std::array<uint8_t, kBlockSize> output;
  EXPECT_TRUE(crypto.Decrypt(encrypted_block, output));
  EXPECT_EQ(output, kOneBlock);
}

TEST(AesCbcCryptoTest, WrongKey) {
  auto encrypted_block = Encrypt(kOneBlock, kKey1, kIv);
  EXPECT_EQ(kBlockSize, encrypted_block.size());

  // Use |key2_| when trying to decrypt.
  AesCbcCrypto crypto;
  EXPECT_TRUE(crypto.Initialize(kKey2, kIv));

  std::array<uint8_t, kBlockSize> output;
  EXPECT_TRUE(crypto.Decrypt(encrypted_block, output));
  EXPECT_NE(output, kOneBlock);
}

TEST(AesCbcCryptoTest, WrongIV) {
  auto encrypted_block = Encrypt(kOneBlock, kKey1, kIv);
  EXPECT_EQ(kBlockSize, encrypted_block.size());

  // Use a different IV when trying to decrypt.
  AesCbcCrypto crypto;
  // clang-format off
  constexpr auto kWrongIv = std::to_array<uint8_t>({
      'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a',
      'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a',
  });
  // clang-format on
  EXPECT_TRUE(crypto.Initialize(kKey1, kWrongIv));

  std::array<uint8_t, kBlockSize> output;
  EXPECT_TRUE(crypto.Decrypt(encrypted_block, output));
  EXPECT_NE(output, kOneBlock);
}

TEST(AesCbcCryptoTest, PartialBlock) {
  auto encrypted_block = Encrypt(kOneBlock, kKey1, kIv);
  EXPECT_EQ(kBlockSize, encrypted_block.size());

  AesCbcCrypto crypto;
  EXPECT_TRUE(crypto.Initialize(kKey2, kIv));

  // Try to decrypt less than a full block.
  std::array<uint8_t, kBlockSize> output;
  EXPECT_FALSE(crypto.Decrypt(
      base::span(encrypted_block).first(encrypted_block.size() - 5), output));
}

TEST(AesCbcCryptoTest, MultipleBlocks) {
  // Encrypt 10 copies of |kOneBlock| together.
  constexpr size_t kNumBlocksInData = 10;
  auto encrypted_block =
      Encrypt(Repeat(kOneBlock, kNumBlocksInData), kKey2, kIv);
  ASSERT_EQ(kNumBlocksInData * kBlockSize, encrypted_block.size());

  AesCbcCrypto crypto;
  EXPECT_TRUE(crypto.Initialize(kKey2, kIv));

  std::array<uint8_t, kNumBlocksInData * kBlockSize> output;
  EXPECT_TRUE(crypto.Decrypt(encrypted_block, output));
  EXPECT_EQ(output, base::as_byte_span(Repeat(kOneBlock, kNumBlocksInData)));
}

// As the code in aes_cbc_crypto.cc relies on decrypting the data block by
// block, ensure that the crypto routines work the same way whether it
// decrypts one block at a time or all the blocks in one call.
TEST(AesCbcCryptoTest, BlockDecryptionWorks) {
  constexpr size_t kNumBlocksInData = 5;
  constexpr auto kData = std::to_array<uint8_t>(
      {1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3,
       3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5,
       6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8,
       8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 0, 0, 0, 0, 0, 0, 0, 0});
  ASSERT_EQ(kData.size(), kNumBlocksInData * kBlockSize);
  auto encrypted_data = Encrypt(kData, kKey1, kIv);

  // Decrypt |encrypted_data| in one pass.
  {
    AesCbcCrypto crypto;
    EXPECT_TRUE(crypto.Initialize(kKey1, kIv));

    std::array<uint8_t, kNumBlocksInData * kBlockSize> output;
    ;
    EXPECT_TRUE(crypto.Decrypt(encrypted_data, output));
    EXPECT_EQ(output, kData);
  }

  // Repeat but call Decrypt() once for each block.
  {
    AesCbcCrypto crypto;
    EXPECT_TRUE(crypto.Initialize(kKey1, kIv));

    std::array<uint8_t, kNumBlocksInData * kBlockSize> output;
    for (size_t offset = 0; offset < output.size(); offset += kBlockSize) {
      EXPECT_TRUE(
          crypto.Decrypt(base::span(encrypted_data).subspan(offset, kBlockSize),
                         base::span(output).subspan(offset, kBlockSize)));
    }
    EXPECT_EQ(output, kData);
  }
}

TEST(AesCbcCryptoTest, InitializeWithWrongKeySize) {
  AesCbcCrypto crypto;
  const std::vector<uint8_t> kWrongSizeKey(kKeySize - 1, 0);
  EXPECT_FALSE(crypto.Initialize(kWrongSizeKey, kIv));
}

TEST(AesCbcCryptoTest, InitializeWithWrongIvSize) {
  AesCbcCrypto crypto;
  const std::vector<uint8_t> kWrongSizeIv(kBlockSize - 1, 0);
  EXPECT_FALSE(crypto.Initialize(kKey1, kWrongSizeIv));
}

}  // namespace media
