// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/cdm/cenc_decryptor.h"

#include <stdint.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "crypto/encryptor.h"
#include "crypto/symmetric_key.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

// Keys and IVs have to be 128 bits.
const uint8_t kKey[] = {0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
                        0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13};
static_assert(std::size(kKey) * 8 == 128, "kKey must be 128 bits");

const uint8_t kIv[] = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static_assert(std::size(kIv) * 8 == 128, "kIv must be 128 bits");

const uint8_t kOneBlock[] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
                             'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p'};

const uint8_t kPartialBlock[] = {'a', 'b', 'c', 'd', 'e', 'f'};

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

// Returns a std::vector<uint8_t> containing |count| copies of |input|.
std::vector<uint8_t> Repeat(const std::vector<uint8_t>& input, size_t count) {
  std::vector<uint8_t> result;
  for (size_t i = 0; i < count; ++i)
    result.insert(result.end(), input.begin(), input.end());
  return result;
}

}  // namespace

// These tests only test decryption logic.
class CencDecryptorTest : public testing::Test {
 public:
  CencDecryptorTest()
      : key_(crypto::SymmetricKey::Import(
            crypto::SymmetricKey::AES,
            std::string(kKey, kKey + std::size(kKey)))),
        iv_(kIv, kIv + std::size(kIv)),
        one_block_(kOneBlock, kOneBlock + std::size(kOneBlock)),
        partial_block_(kPartialBlock,
                       kPartialBlock + std::size(kPartialBlock)) {}

  // Excrypt |original| using AES-CTR encryption with |key| and |iv|.
  std::vector<uint8_t> Encrypt(const std::vector<uint8_t>& original,
                               const crypto::SymmetricKey& key,
                               const std::string& iv) {
    crypto::Encryptor encryptor;
    EXPECT_TRUE(encryptor.Init(&key, crypto::Encryptor::CTR, ""));
    EXPECT_TRUE(encryptor.SetCounter(iv));

    std::string ciphertext;
    EXPECT_TRUE(encryptor.Encrypt(MakeString(original), &ciphertext));
    DCHECK_EQ(ciphertext.size(), original.size());

    return std::vector<uint8_t>(ciphertext.begin(), ciphertext.end());
  }

  // Returns a 'cenc' DecoderBuffer using the data and other parameters.
  scoped_refptr<DecoderBuffer> CreateEncryptedBuffer(
      const std::vector<uint8_t>& data,
      const std::string& iv,
      const std::vector<SubsampleEntry>& subsample_entries) {
    EXPECT_FALSE(data.empty());
    EXPECT_FALSE(iv.empty());

    scoped_refptr<DecoderBuffer> encrypted_buffer =
        DecoderBuffer::CopyFrom(data);

    // Key_ID is never used.
    encrypted_buffer->set_decrypt_config(
        DecryptConfig::CreateCencConfig("key_id", iv, subsample_entries));
    return encrypted_buffer;
  }

  // Calls DecryptCencBuffer() to decrypt |encrypted| using |key|, and then
  // returns the decrypted buffer (empty if decryption fails).
  std::vector<uint8_t> DecryptWithKey(scoped_refptr<DecoderBuffer> encrypted,
                                      const crypto::SymmetricKey& key) {
    scoped_refptr<DecoderBuffer> decrypted = DecryptCencBuffer(*encrypted, key);

    std::vector<uint8_t> decrypted_data;
    if (decrypted.get()) {
      EXPECT_TRUE(decrypted->size());
      decrypted_data.assign(decrypted->data(),
                            decrypted->data() + decrypted->size());
    }

    return decrypted_data;
  }

  // Constants for testing.
  const std::unique_ptr<crypto::SymmetricKey> key_;
  const std::string iv_;
  const std::vector<uint8_t> one_block_;
  const std::vector<uint8_t> partial_block_;
};

TEST_F(CencDecryptorTest, OneBlock) {
  auto encrypted_block = Encrypt(one_block_, *key_, iv_);

  // Only 1 subsample, all encrypted data.
  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size())}};

  auto encrypted_buffer =
      CreateEncryptedBuffer(encrypted_block, iv_, subsamples);
  EXPECT_EQ(one_block_, DecryptWithKey(encrypted_buffer, *key_));
}

TEST_F(CencDecryptorTest, ExtraData) {
  auto encrypted_block = Encrypt(one_block_, *key_, iv_);

  // Only 1 subsample, all encrypted data.
  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size())}};

  auto encrypted_buffer =
      CreateEncryptedBuffer(encrypted_block, iv_, subsamples);
  encrypted_buffer->set_timestamp(base::Days(2));
  encrypted_buffer->set_duration(base::Minutes(5));
  encrypted_buffer->set_is_key_frame(true);
  encrypted_buffer->WritableSideData().alpha_data.assign(
      encrypted_block.begin(), encrypted_block.end());

  auto decrypted_buffer = DecryptCencBuffer(*encrypted_buffer, *key_);
  EXPECT_EQ(encrypted_buffer->timestamp(), decrypted_buffer->timestamp());
  EXPECT_EQ(encrypted_buffer->duration(), decrypted_buffer->duration());
  EXPECT_EQ(encrypted_buffer->end_of_stream(),
            decrypted_buffer->end_of_stream());
  EXPECT_EQ(encrypted_buffer->is_key_frame(), decrypted_buffer->is_key_frame());
  EXPECT_TRUE(decrypted_buffer->has_side_data());
  EXPECT_TRUE(encrypted_buffer->side_data()->Matches(
      decrypted_buffer->side_data().value()));
}

TEST_F(CencDecryptorTest, NoSubsamples) {
  auto encrypted_block = Encrypt(one_block_, *key_, iv_);

  // No subsamples specified.
  std::vector<SubsampleEntry> subsamples = {};

  auto encrypted_buffer =
      CreateEncryptedBuffer(encrypted_block, iv_, subsamples);
  EXPECT_EQ(one_block_, DecryptWithKey(encrypted_buffer, *key_));
}

TEST_F(CencDecryptorTest, BadSubsamples) {
  auto encrypted_block = Encrypt(one_block_, *key_, iv_);

  // Subsample size > data size.
  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size() + 1)}};

  auto encrypted_buffer =
      CreateEncryptedBuffer(encrypted_block, iv_, subsamples);
  EXPECT_EQ(std::vector<uint8_t>(), DecryptWithKey(encrypted_buffer, *key_));
}

TEST_F(CencDecryptorTest, InvalidIv) {
  auto encrypted_block = Encrypt(one_block_, *key_, iv_);

  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size())}};

  // Use an invalid IV for decryption. Call should succeed, but return
  // something other than the original data.
  std::string invalid_iv(iv_.size(), 'a');
  auto encrypted_buffer =
      CreateEncryptedBuffer(encrypted_block, invalid_iv, subsamples);
  EXPECT_NE(one_block_, DecryptWithKey(encrypted_buffer, *key_));
}

TEST_F(CencDecryptorTest, InvalidKey) {
  std::unique_ptr<crypto::SymmetricKey> bad_key = crypto::SymmetricKey::Import(
      crypto::SymmetricKey::AES, std::string(std::size(kKey), 'b'));
  auto encrypted_block = Encrypt(one_block_, *key_, iv_);

  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size())}};

  // Use a different key for decryption. Call should succeed, but return
  // something other than the original data.
  auto encrypted_buffer =
      CreateEncryptedBuffer(encrypted_block, iv_, subsamples);
  EXPECT_NE(one_block_, DecryptWithKey(encrypted_buffer, *bad_key));
}

TEST_F(CencDecryptorTest, PartialBlock) {
  auto encrypted_block = Encrypt(partial_block_, *key_, iv_);

  // Only 1 subsample, all encrypted data.
  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size())}};

  auto encrypted_buffer =
      CreateEncryptedBuffer(encrypted_block, iv_, subsamples);
  EXPECT_EQ(partial_block_, DecryptWithKey(encrypted_buffer, *key_));
}

TEST_F(CencDecryptorTest, MultipleSubsamples) {
  // Encrypt 3 copies of |one_block_| together.
  auto encrypted_block = Encrypt(Repeat(one_block_, 3), *key_, iv_);

  // Treat as 3 subsamples.
  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(one_block_.size())},
      {0, static_cast<uint32_t>(one_block_.size())},
      {0, static_cast<uint32_t>(one_block_.size())}};

  auto encrypted_buffer =
      CreateEncryptedBuffer(encrypted_block, iv_, subsamples);
  EXPECT_EQ(Repeat(one_block_, 3), DecryptWithKey(encrypted_buffer, *key_));
}

TEST_F(CencDecryptorTest, MultipleSubsamplesWithClearBytes) {
  // Create a buffer that looks like:
  // subsamples: |    subsample#1    |     subsample#2     | subsample#3 |
  //             | clear | encrypted |  clear  | encrypted |    clear    |
  // source:     |  one  | partial*  | partial |   one*    |   partial   |
  // where * means the source is encrypted
  auto encrypted_block =
      Encrypt(Combine({partial_block_, one_block_}), *key_, iv_);
  std::vector<uint8_t> encrypted_partial_block(
      encrypted_block.begin(), encrypted_block.begin() + partial_block_.size());
  EXPECT_EQ(encrypted_partial_block.size(), partial_block_.size());
  std::vector<uint8_t> encrypted_one_block(
      encrypted_block.begin() + partial_block_.size(), encrypted_block.end());
  EXPECT_EQ(encrypted_one_block.size(), one_block_.size());

  auto input_data =
      Combine({one_block_, encrypted_partial_block, partial_block_,
               encrypted_one_block, partial_block_});
  auto expected_result = Combine(
      {one_block_, partial_block_, partial_block_, one_block_, partial_block_});
  std::vector<SubsampleEntry> subsamples = {
      {static_cast<uint32_t>(one_block_.size()),
       static_cast<uint32_t>(partial_block_.size())},
      {static_cast<uint32_t>(partial_block_.size()),
       static_cast<uint32_t>(one_block_.size())},
      {static_cast<uint32_t>(partial_block_.size()), 0}};

  auto encrypted_buffer = CreateEncryptedBuffer(input_data, iv_, subsamples);
  EXPECT_EQ(expected_result, DecryptWithKey(encrypted_buffer, *key_));
}

}  // namespace media
