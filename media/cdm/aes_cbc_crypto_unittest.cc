// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/aes_cbc_crypto.h"

#include <memory>
#include <optional>

#include "base/containers/span.h"
#include "base/memory/raw_span.h"
#include "crypto/encryptor.h"
#include "crypto/symmetric_key.h"
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

// Keys and IV have to be 128 bits.
const uint8_t kKey1[] = {0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
                         0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13};
static_assert(std::size(kKey1) == 128 / 8, "kKey1 must be 128 bits");

const uint8_t kKey2[] = {0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13,
                         0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b};
static_assert(std::size(kKey2) == 128 / 8, "kKey2 must be 128 bits");

const uint8_t kIv[] = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static_assert(std::size(kIv) == 128 / 8, "kIv must be 128 bits");

const uint8_t kOneBlock[] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
                             'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p'};
static_assert(std::size(kOneBlock) == kBlockSize, "kOneBlock not block sized");

std::string MakeString(const std::vector<uint8_t>& chars) {
  return std::string(chars.begin(), chars.end());
}

// Returns a std::vector<uint8_t> containing |count| copies of |input|.
std::vector<uint8_t> Repeat(const std::vector<uint8_t>& input, size_t count) {
  std::vector<uint8_t> result;
  for (size_t i = 0; i < count; ++i)
    result.insert(result.end(), input.begin(), input.end());
  return result;
}

}  // namespace

class AesCbcCryptoTest : public testing::Test {
 public:
  AesCbcCryptoTest()
      : key1_(crypto::SymmetricKey::Import(
            crypto::SymmetricKey::AES,
            std::string(std::begin(kKey1), std::end(kKey1)))),
        key2_(crypto::SymmetricKey::Import(
            crypto::SymmetricKey::AES,
            std::string(std::begin(kKey2), std::end(kKey2)))),
        iv_(std::begin(kIv), std::end(kIv)),
        one_block_(std::begin(kOneBlock), std::end(kOneBlock)) {}

  // Encrypt |original| using AES-CBC encryption with |key| and |iv|.
  std::vector<uint8_t> Encrypt(const std::vector<uint8_t>& original,
                               const crypto::SymmetricKey& key,
                               base::span<const uint8_t> iv) {
    // This code uses crypto::Encryptor to encrypt |original| rather than
    // calling EVP_EncryptInit_ex() / EVP_EncryptUpdate() / etc. This is done
    // for simplicity, as the crypto:: code wraps all the calls up nicely.
    // However, for AES-CBC encryption, the crypto:: code does add padding to
    // the output, which is simply stripped off.
    crypto::Encryptor encryptor;
    std::string iv_as_string(std::begin(iv), std::end(iv));
    EXPECT_TRUE(encryptor.Init(&key, crypto::Encryptor::CBC, iv_as_string));

    std::string ciphertext;
    EXPECT_TRUE(encryptor.Encrypt(MakeString(original), &ciphertext));

    // CBC encryption adds a block of padding at the end, so discard it.
    EXPECT_GT(ciphertext.size(), original.size());
    ciphertext.resize(original.size());

    return std::vector<uint8_t>(ciphertext.begin(), ciphertext.end());
  }

  // Constants for testing.
  std::unique_ptr<crypto::SymmetricKey> key1_;
  std::unique_ptr<crypto::SymmetricKey> key2_;
  base::raw_span<const uint8_t> iv_;
  const std::vector<uint8_t> one_block_;
};

TEST_F(AesCbcCryptoTest, OneBlock) {
  auto encrypted_block = Encrypt(one_block_, *key1_, iv_);
  EXPECT_EQ(kBlockSize, encrypted_block.size());

  AesCbcCrypto crypto;
  EXPECT_TRUE(crypto.Initialize(*key1_, iv_));

  std::vector<uint8_t> output(encrypted_block.size());
  EXPECT_TRUE(crypto.Decrypt(encrypted_block, output.data()));
  EXPECT_EQ(output, one_block_);
}

TEST_F(AesCbcCryptoTest, WrongKey) {
  auto encrypted_block = Encrypt(one_block_, *key1_, iv_);
  EXPECT_EQ(kBlockSize, encrypted_block.size());

  // Use |key2_| when trying to decrypt.
  AesCbcCrypto crypto;
  EXPECT_TRUE(crypto.Initialize(*key2_, iv_));

  std::vector<uint8_t> output(encrypted_block.size());
  EXPECT_TRUE(crypto.Decrypt(encrypted_block, output.data()));
  EXPECT_NE(output, one_block_);
}

TEST_F(AesCbcCryptoTest, WrongIV) {
  auto encrypted_block = Encrypt(one_block_, *key1_, iv_);
  EXPECT_EQ(kBlockSize, encrypted_block.size());

  // Use a different IV when trying to decrypt.
  AesCbcCrypto crypto;
  std::vector<uint8_t> alternate_iv(iv_.size(), 'a');
  EXPECT_TRUE(crypto.Initialize(*key1_, alternate_iv));

  std::vector<uint8_t> output(encrypted_block.size());
  EXPECT_TRUE(crypto.Decrypt(encrypted_block, output.data()));
  EXPECT_NE(output, one_block_);
}

TEST_F(AesCbcCryptoTest, PartialBlock) {
  auto encrypted_block = Encrypt(one_block_, *key1_, iv_);
  EXPECT_EQ(kBlockSize, encrypted_block.size());

  AesCbcCrypto crypto;
  EXPECT_TRUE(crypto.Initialize(*key2_, iv_));

  // Try to decrypt less than a full block.
  std::vector<uint8_t> output(encrypted_block.size());
  EXPECT_FALSE(crypto.Decrypt(
      base::make_span(encrypted_block).first(encrypted_block.size() - 5),
      output.data()));
}

TEST_F(AesCbcCryptoTest, MultipleBlocks) {
  // Encrypt 10 copies of |one_block_| together.
  constexpr size_t kNumBlocksInData = 10;
  auto encrypted_block =
      Encrypt(Repeat(one_block_, kNumBlocksInData), *key2_, iv_);
  ASSERT_EQ(kNumBlocksInData * kBlockSize, encrypted_block.size());

  AesCbcCrypto crypto;
  EXPECT_TRUE(crypto.Initialize(*key2_, iv_));

  std::vector<uint8_t> output(encrypted_block.size());
  EXPECT_TRUE(crypto.Decrypt(encrypted_block, output.data()));
  EXPECT_EQ(output, Repeat(one_block_, kNumBlocksInData));
}

// As the code in aes_cbc_crypto.cc relies on decrypting the data block by
// block, ensure that the crypto routines work the same way whether it
// decrypts one block at a time or all the blocks in one call.
TEST_F(AesCbcCryptoTest, BlockDecryptionWorks) {
  constexpr size_t kNumBlocksInData = 5;
  std::vector<uint8_t> data = {1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2,
                               3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
                               5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6,
                               7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
                               9, 9, 9, 9, 9, 9, 9, 9, 0, 0, 0, 0, 0, 0, 0, 0};
  ASSERT_EQ(data.size(), kNumBlocksInData * kBlockSize);
  auto encrypted_data = Encrypt(data, *key1_, iv_);

  // Decrypt |encrypted_data| in one pass.
  {
    AesCbcCrypto crypto;
    EXPECT_TRUE(crypto.Initialize(*key1_, iv_));

    std::vector<uint8_t> output(kNumBlocksInData * kBlockSize);
    EXPECT_TRUE(crypto.Decrypt(encrypted_data, output.data()));
    EXPECT_EQ(output, data);
  }

  // Repeat but call Decrypt() once for each block.
  {
    AesCbcCrypto crypto;
    EXPECT_TRUE(crypto.Initialize(*key1_, iv_));

    std::vector<uint8_t> output(kNumBlocksInData * kBlockSize);
    auto input = base::make_span(encrypted_data);
    for (size_t offset = 0; offset < output.size(); offset += kBlockSize) {
      EXPECT_TRUE(
          crypto.Decrypt(input.subspan(offset, kBlockSize), &output[offset]));
    }
    EXPECT_EQ(output, data);
  }
}

}  // namespace media
