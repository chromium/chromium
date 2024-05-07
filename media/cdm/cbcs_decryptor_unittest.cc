// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cbcs_decryptor.h"

#include <algorithm>
#include <array>
#include <memory>
#include <optional>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/time/time.h"
#include "crypto/encryptor.h"
#include "crypto/symmetric_key.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

// Pattern decryption uses 16-byte blocks.
constexpr size_t kBlockSize = 16;

// Keys and IVs have to be 128 bits.
const std::array<uint8_t, 16> kKey = {0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
                                      0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
                                      0x10, 0x11, 0x12, 0x13};

const std::array<uint8_t, 16> kIv = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
                                     0x26, 0x27, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x00};

const std::array<uint8_t, kBlockSize> kOneBlock = {'a', 'b', 'c', 'd', 'e', 'f',
                                                   'g', 'h', 'i', 'j', 'k', 'l',
                                                   'm', 'n', 'o', 'p'};

const std::array<uint8_t, 6> kPartialBlock = {'a', 'b', 'c', 'd', 'e', 'f'};
static_assert(std::size(kPartialBlock) != kBlockSize, "kPartialBlock wrong");

std::string MakeString(const std::vector<uint8_t>& chars) {
  return std::string(chars.begin(), chars.end());
}

// Combine multiple std::vector<uint8_t> into one.
std::vector<uint8_t> Combine(const std::vector<std::vector<uint8_t>>& inputs) {
  std::vector<uint8_t> result;
  for (const auto& input : inputs)
    result.insert(result.end(), input.begin(), input.end());

  return result;
}

// Extract the |n|th block of |input|. The first block is number 1.
std::vector<uint8_t> GetBlock(size_t n, const std::vector<uint8_t>& input) {
  DCHECK_LE(n, input.size() / kBlockSize);
  auto it = input.begin() + ((n - 1) * kBlockSize);
  return std::vector<uint8_t>(it, it + kBlockSize);
}

// Returns a std::vector<uint8_t> containing |count| copies of |input|.
std::vector<uint8_t> Repeat(const std::vector<uint8_t>& input, size_t count) {
  std::vector<uint8_t> result;
  for (size_t i = 0; i < count; ++i)
    result.insert(result.end(), input.begin(), input.end());
  return result;
}

}  // namespace

class CbcsDecryptorTest : public testing::Test {
 public:
  CbcsDecryptorTest()
      : key_(crypto::SymmetricKey::Import(
            crypto::SymmetricKey::AES,
            std::string(std::begin(kKey), std::end(kKey)))),
        iv_(std::begin(kIv), std::end(kIv)),
        one_block_(std::begin(kOneBlock), std::end(kOneBlock)),
        partial_block_(std::begin(kPartialBlock), std::end(kPartialBlock)) {}

  // Excrypt |original| using AES-CBC encryption with |key| and |iv|.
  std::vector<uint8_t> Encrypt(const std::vector<uint8_t>& original,
                               const crypto::SymmetricKey& key,
                               const std::string& iv) {
    // This code uses crypto::Encryptor to encrypt |original| rather than
    // calling EVP_EncryptInit_ex() / EVP_EncryptUpdate() / etc. This is done
    // for simplicity, as the crypto:: code wraps all the calls up nicely.
    // However, for AES-CBC encryption, the crypto:: code does add padding to
    // the output, which is simply stripped off.
    crypto::Encryptor encryptor;
    EXPECT_TRUE(encryptor.Init(&key, crypto::Encryptor::CBC, iv));

    std::string ciphertext;
    EXPECT_TRUE(encryptor.Encrypt(MakeString(original), &ciphertext));

    // CBC encryption adds a block of padding at the end, so discard it.
    DCHECK_GT(ciphertext.size(), original.size());
    ciphertext.resize(original.size());

    return std::vector<uint8_t>(ciphertext.begin(), ciphertext.end());
  }

  // Returns a 'cbcs' DecoderBuffer using the data and other parameters.
  scoped_refptr<DecoderBuffer> CreateEncryptedBuffer(
      const std::vector<uint8_t>& data,
      const std::string& iv,
      const std::vector<SubsampleEntry>& subsample_entries,
      std::optional<EncryptionPattern> encryption_pattern) {
    EXPECT_FALSE(data.empty());
    EXPECT_FALSE(iv.empty());

    auto encrypted_buffer = DecoderBuffer::CopyFrom(data);

    // Key_ID is never used.
    encrypted_buffer->set_decrypt_config(DecryptConfig::CreateCbcsConfig(
        "key_id", iv, subsample_entries, encryption_pattern));
    return encrypted_buffer;
  }

  // Calls DecryptCbcsBuffer() to decrypt |encrypted| using |key|,
  // and then returns the data in the decrypted buffer.
  std::vector<uint8_t> DecryptWithKey(scoped_refptr<DecoderBuffer> encrypted,
                                      const crypto::SymmetricKey& key) {
    auto decrypted = DecryptCbcsBuffer(*encrypted, key);

    std::vector<uint8_t> decrypted_data;
    if (decrypted.get()) {
      EXPECT_FALSE(decrypted->empty());
      decrypted_data = base::ToVector(base::span(*decrypted));
    }

    return decrypted_data;
  }

  // Constants for testing.
  std::unique_ptr<crypto::SymmetricKey> key_;
  const std::string iv_;
  const std::vector<uint8_t> one_block_;
  const std::vector<uint8_t> partial_block_;
};

TEST_F(CbcsDecryptorTest, OneBlock) {
  auto encrypted_block = Encrypt(one_block_, *key_, iv_);
  DCHECK_EQ(kBlockSize, encrypted_block.size());

  // Only 1 subsample, all encrypted data.
  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size())}};

  auto encrypted_buffer = CreateEncryptedBuffer(
      encrypted_block, iv_, subsamples, EncryptionPattern(1, 9));
  EXPECT_EQ(one_block_, DecryptWithKey(encrypted_buffer, *key_));
}

TEST_F(CbcsDecryptorTest, AdditionalData) {
  auto encrypted_block = Encrypt(one_block_, *key_, iv_);
  DCHECK_EQ(kBlockSize, encrypted_block.size());

  // Only 1 subsample, all encrypted data.
  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size())}};

  auto encrypted_buffer = CreateEncryptedBuffer(
      encrypted_block, iv_, subsamples, EncryptionPattern(1, 9));
  encrypted_buffer->set_timestamp(base::Days(2));
  encrypted_buffer->set_duration(base::Minutes(5));
  encrypted_buffer->set_is_key_frame(true);
  encrypted_buffer->WritableSideData().alpha_data.assign(
      encrypted_block.begin(), encrypted_block.end());

  auto decrypted_buffer = DecryptCbcsBuffer(*encrypted_buffer, *key_);
  EXPECT_EQ(encrypted_buffer->timestamp(), decrypted_buffer->timestamp());
  EXPECT_EQ(encrypted_buffer->duration(), decrypted_buffer->duration());
  EXPECT_EQ(encrypted_buffer->end_of_stream(),
            decrypted_buffer->end_of_stream());
  EXPECT_EQ(encrypted_buffer->is_key_frame(), decrypted_buffer->is_key_frame());
  EXPECT_TRUE(decrypted_buffer->has_side_data());
  EXPECT_TRUE(encrypted_buffer->side_data()->Matches(
      decrypted_buffer->side_data().value()));
}

TEST_F(CbcsDecryptorTest, DifferentPattern) {
  auto encrypted_block = Encrypt(one_block_, *key_, iv_);
  DCHECK_EQ(kBlockSize, encrypted_block.size());

  // Only 1 subsample, all encrypted data.
  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size())}};

  auto encrypted_buffer = CreateEncryptedBuffer(
      encrypted_block, iv_, subsamples, EncryptionPattern(1, 0));
  EXPECT_EQ(one_block_, DecryptWithKey(encrypted_buffer, *key_));
}

TEST_F(CbcsDecryptorTest, EmptyPattern) {
  auto encrypted_block = Encrypt(one_block_, *key_, iv_);
  DCHECK_EQ(kBlockSize, encrypted_block.size());

  // Only 1 subsample, all encrypted data.
  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size())}};

  // Pattern 0:0 treats the buffer as all encrypted.
  auto encrypted_buffer = CreateEncryptedBuffer(
      encrypted_block, iv_, subsamples, EncryptionPattern(0, 0));
  EXPECT_EQ(one_block_, DecryptWithKey(encrypted_buffer, *key_));
}

TEST_F(CbcsDecryptorTest, PatternTooLarge) {
  auto encrypted_block = Encrypt(one_block_, *key_, iv_);
  DCHECK_EQ(kBlockSize, encrypted_block.size());

  // Only 1 subsample, all encrypted data.
  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size())}};

  // Pattern 100:0 is too large, so decryption will fail.
  auto encrypted_buffer = CreateEncryptedBuffer(
      encrypted_block, iv_, subsamples, EncryptionPattern(100, 0));
  EXPECT_EQ(std::vector<uint8_t>(), DecryptWithKey(encrypted_buffer, *key_));
}

TEST_F(CbcsDecryptorTest, NoSubsamples) {
  auto encrypted_block = Encrypt(one_block_, *key_, iv_);
  DCHECK_EQ(kBlockSize, encrypted_block.size());

  std::vector<SubsampleEntry> subsamples = {};

  auto encrypted_buffer = CreateEncryptedBuffer(
      encrypted_block, iv_, subsamples, EncryptionPattern(1, 9));
  EXPECT_EQ(one_block_, DecryptWithKey(encrypted_buffer, *key_));
}

TEST_F(CbcsDecryptorTest, BadSubsamples) {
  auto encrypted_block = Encrypt(one_block_, *key_, iv_);

  // Subsample size > data size.
  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size() + 1)}};

  auto encrypted_buffer = CreateEncryptedBuffer(
      encrypted_block, iv_, subsamples, EncryptionPattern(1, 0));
  EXPECT_EQ(std::vector<uint8_t>(), DecryptWithKey(encrypted_buffer, *key_));
}

TEST_F(CbcsDecryptorTest, InvalidIv) {
  auto encrypted_block = Encrypt(one_block_, *key_, iv_);

  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size())}};

  // Use an invalid IV for decryption. Call should succeed, but return
  // something other than the original data.
  std::string invalid_iv(iv_.size(), 'a');
  auto encrypted_buffer = CreateEncryptedBuffer(
      encrypted_block, invalid_iv, subsamples, EncryptionPattern(1, 0));
  EXPECT_NE(one_block_, DecryptWithKey(encrypted_buffer, *key_));
}

TEST_F(CbcsDecryptorTest, InvalidKey) {
  auto encrypted_block = Encrypt(one_block_, *key_, iv_);

  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size())}};

  // Use a different key for decryption. Call should succeed, but return
  // something other than the original data.
  std::unique_ptr<crypto::SymmetricKey> bad_key = crypto::SymmetricKey::Import(
      crypto::SymmetricKey::AES, std::string(std::size(kKey), 'b'));
  auto encrypted_buffer = CreateEncryptedBuffer(
      encrypted_block, iv_, subsamples, EncryptionPattern(1, 0));
  EXPECT_NE(one_block_, DecryptWithKey(encrypted_buffer, *bad_key));
}

TEST_F(CbcsDecryptorTest, PartialBlock) {
  // Only 1 subsample, all "encrypted" data. However, as it's not a full block,
  // it will be treated as unencrypted.
  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(partial_block_.size())}};

  auto encrypted_buffer = CreateEncryptedBuffer(partial_block_, iv_, subsamples,
                                                EncryptionPattern(1, 0));
  EXPECT_EQ(partial_block_, DecryptWithKey(encrypted_buffer, *key_));
}

TEST_F(CbcsDecryptorTest, SingleBlockWithExtraData) {
  // Create some data that is longer than a single block. The full block will
  // be encrypted, but the extra data at the end will be considered unencrypted.
  auto encrypted_block =
      Combine({Encrypt(one_block_, *key_, iv_), partial_block_});
  auto expected_result = Combine({one_block_, partial_block_});

  // Only 1 subsample, all "encrypted" data.
  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size())}};

  auto encrypted_buffer = CreateEncryptedBuffer(
      encrypted_block, iv_, subsamples, EncryptionPattern(1, 0));
  EXPECT_EQ(expected_result, DecryptWithKey(encrypted_buffer, *key_));
}

TEST_F(CbcsDecryptorTest, SkipBlock) {
  // Only 1 subsample, but all unencrypted data.
  std::vector<SubsampleEntry> subsamples = {
      {static_cast<uint32_t>(one_block_.size()), 0}};

  auto encrypted_buffer = CreateEncryptedBuffer(one_block_, iv_, subsamples,
                                                EncryptionPattern(1, 0));
  EXPECT_EQ(one_block_, DecryptWithKey(encrypted_buffer, *key_));
}

TEST_F(CbcsDecryptorTest, MultipleBlocks) {
  // Encrypt 2 copies of |one_block_| together using kKey and kIv.
  auto encrypted_block = Encrypt(Repeat(one_block_, 2), *key_, iv_);
  DCHECK_EQ(2 * kBlockSize, encrypted_block.size());

  // 1 subsample, 4 blocks in (1,1) pattern.
  // Encrypted blocks come from |encrypted_block|.
  // data:       | enc1 | clear | enc2 | clear |
  // subsamples: |         subsample#1         |
  //             |eeeeeeeeeeeeeeeeeeeeeeeeeeeee|
  auto input_data = Combine({GetBlock(1, encrypted_block), one_block_,
                             GetBlock(2, encrypted_block), one_block_});
  auto expected_result = Repeat(one_block_, 4);
  std::vector<SubsampleEntry> subsamples = {{0, 4 * kBlockSize}};

  auto encrypted_buffer = CreateEncryptedBuffer(input_data, iv_, subsamples,
                                                EncryptionPattern(1, 1));
  EXPECT_EQ(expected_result, DecryptWithKey(encrypted_buffer, *key_));
}

TEST_F(CbcsDecryptorTest, PartialPattern) {
  // Encrypt 4 copies of |one_block_| together using kKey and kIv.
  auto encrypted_block = Encrypt(Repeat(one_block_, 4), *key_, iv_);
  DCHECK_EQ(4 * kBlockSize, encrypted_block.size());

  // 1 subsample, 4 blocks in (8,2) pattern. Even though there is not a full
  // pattern (10 blocks), all 4 blocks should be decrypted.
  auto expected_result = Repeat(one_block_, 4);
  std::vector<SubsampleEntry> subsamples = {{0, 4 * kBlockSize}};

  auto encrypted_buffer = CreateEncryptedBuffer(
      encrypted_block, iv_, subsamples, EncryptionPattern(8, 2));
  EXPECT_EQ(expected_result, DecryptWithKey(encrypted_buffer, *key_));
}

TEST_F(CbcsDecryptorTest, SkipBlocks) {
  // Encrypt 5 blocks together using kKey and kIv.
  auto encrypted_block = Encrypt(Repeat(one_block_, 5), *key_, iv_);
  DCHECK_EQ(5 * kBlockSize, encrypted_block.size());

  // 1 subsample, 1 unencrypted block followed by 7 blocks in (2,1) pattern.
  // Encrypted blocks come from |encrypted_block|.
  // data:       | clear | enc1 | enc2 | clear | enc3 | enc4 | clear | enc5 |
  // subsamples: |                  subsample#1                             |
  //             |uuuuuuu eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee|
  auto input_data = Combine(
      {one_block_, GetBlock(1, encrypted_block), GetBlock(2, encrypted_block),
       one_block_, GetBlock(3, encrypted_block), GetBlock(4, encrypted_block),
       one_block_, GetBlock(5, encrypted_block)});
  auto expected_result = Repeat(one_block_, 8);
  std::vector<SubsampleEntry> subsamples = {{kBlockSize, 7 * kBlockSize}};

  auto encrypted_buffer = CreateEncryptedBuffer(input_data, iv_, subsamples,
                                                EncryptionPattern(2, 1));
  EXPECT_EQ(expected_result, DecryptWithKey(encrypted_buffer, *key_));
}

TEST_F(CbcsDecryptorTest, MultipleSubsamples) {
  // Encrypt |one_block_| using kKey and kIv.
  auto encrypted_block = Encrypt(one_block_, *key_, iv_);
  DCHECK_EQ(kBlockSize, encrypted_block.size());

  // 3 subsamples, each 1 block of |encrypted_block|.
  // data:       |  encrypted  |  encrypted  |  encrypted  |
  // subsamples: | subsample#1 | subsample#2 | subsample#3 |
  //             |eeeeeeeeeeeee|eeeeeeeeeeeee|eeeeeeeeeeeee|
  auto input_data = Repeat(encrypted_block, 3);
  auto expected_result = Repeat(one_block_, 3);
  std::vector<SubsampleEntry> subsamples = {
      {0, kBlockSize}, {0, kBlockSize}, {0, kBlockSize}};

  auto encrypted_buffer = CreateEncryptedBuffer(input_data, iv_, subsamples,
                                                EncryptionPattern(1, 0));
  EXPECT_EQ(expected_result, DecryptWithKey(encrypted_buffer, *key_));
}

TEST_F(CbcsDecryptorTest, MultipleSubsamplesWithClearBytes) {
  // Encrypt |one_block_| using kKey and kIv.
  auto encrypted_block = Encrypt(one_block_, *key_, iv_);
  DCHECK_EQ(kBlockSize, encrypted_block.size());

  // Combine into alternating clear/encrypted blocks in 3 subsamples. Split
  // the second and third clear blocks into part of encrypted data of the
  // previous block (which as a partial block will be considered unencrypted).
  // data:       | clear | encrypted | clear | encrypted | clear | encrypted |
  // subsamples: |    subsample#1     |    subsample#2        | subsample#3  |
  //             |uuuuuuu eeeeeeeeeeee|uuuuuu eeeeeeeeeeeeeeee|uu eeeeeeeeeee|
  auto input_data = Combine({one_block_, encrypted_block, one_block_,
                             encrypted_block, one_block_, encrypted_block});
  auto expected_result = Repeat(one_block_, 6);
  std::vector<SubsampleEntry> subsamples = {{kBlockSize, kBlockSize + 1},
                                            {kBlockSize - 1, kBlockSize + 10},
                                            {kBlockSize - 10, kBlockSize}};

  auto encrypted_buffer = CreateEncryptedBuffer(input_data, iv_, subsamples,
                                                EncryptionPattern(1, 0));
  EXPECT_EQ(expected_result, DecryptWithKey(encrypted_buffer, *key_));
}

}  // namespace media
