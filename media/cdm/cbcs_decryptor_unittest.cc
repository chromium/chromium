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
#include "base/strings/string_view_util.h"
#include "base/time/time.h"
#include "crypto/aes_cbc.h"
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

// Combine multiple std::vector<uint8_t> into one.
std::vector<uint8_t> Combine(
    base::span<const base::span<const uint8_t>> inputs) {
  std::vector<uint8_t> result;
  for (const auto& input : inputs)
    result.insert(result.end(), input.begin(), input.end());

  return result;
}

// Extract the |n|th block of |input|. The first block is number 1.
std::vector<uint8_t> GetBlock(size_t n, base::span<const uint8_t> input) {
  DCHECK_LE(n, input.size() / kBlockSize);
  auto it = input.begin() + ((n - 1) * kBlockSize);
  return std::vector<uint8_t>(it, it + kBlockSize);
}

// Returns a std::vector<uint8_t> containing |count| copies of |input|.
std::vector<uint8_t> Repeat(base::span<const uint8_t> input, size_t count) {
  std::vector<uint8_t> result;
  for (size_t i = 0; i < count; ++i)
    result.insert(result.end(), input.begin(), input.end());
  return result;
}

std::vector<uint8_t> Encrypt(base::span<const uint8_t> plaintext,
                             base::span<const uint8_t> key,
                             base::span<const uint8_t> iv) {
  std::vector<uint8_t> ciphertext = crypto::aes_cbc::Encrypt(
      key, base::span<const uint8_t, crypto::aes_cbc::kBlockSize>(iv),
      plaintext);

  // Strip the PKCS#5 padding block off the end.
  ciphertext.resize(plaintext.size());
  return ciphertext;
}

// Returns a 'cbcs' DecoderBuffer using the data and other parameters.
scoped_refptr<DecoderBuffer> CreateEncryptedBuffer(
    base::span<const uint8_t> data,
    base::span<const uint8_t> iv,
    const std::vector<SubsampleEntry>& subsample_entries,
    std::optional<EncryptionPattern> encryption_pattern) {
  EXPECT_FALSE(data.empty());
  EXPECT_FALSE(iv.empty());

  auto encrypted_buffer = DecoderBuffer::CopyFrom(data);

  // Key_ID is never used.
  encrypted_buffer->set_decrypt_config(DecryptConfig::CreateCbcsConfig(
      "key_id", std::string(base::as_string_view(iv)), subsample_entries,
      encryption_pattern));
  return encrypted_buffer;
}

std::vector<uint8_t> DecryptWithKey(scoped_refptr<DecoderBuffer> encrypted,
                                    base::span<const uint8_t> key) {
  auto decrypted = DecryptCbcsBuffer(*encrypted, key);

  std::vector<uint8_t> decrypted_data;
  if (decrypted.get()) {
    EXPECT_FALSE(decrypted->empty());
    decrypted_data = base::ToVector(base::span(*decrypted));
  }

  return decrypted_data;
}

}  // namespace

using CbcsDecryptorTest = ::testing::Test;

TEST(CbcsDecryptorTest, OneBlock) {
  auto encrypted_block = Encrypt(kOneBlock, kKey, kIv);
  DCHECK_EQ(kBlockSize, encrypted_block.size());

  // Only 1 subsample, all encrypted data.
  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size())}};

  auto encrypted_buffer = CreateEncryptedBuffer(
      encrypted_block, kIv, subsamples, EncryptionPattern(1, 9));
  EXPECT_EQ(kOneBlock,
            base::as_byte_span(DecryptWithKey(encrypted_buffer, kKey)));
}

TEST(CbcsDecryptorTest, AdditionalData) {
  auto encrypted_block = Encrypt(kOneBlock, kKey, kIv);
  DCHECK_EQ(kBlockSize, encrypted_block.size());

  // Only 1 subsample, all encrypted data.
  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size())}};

  auto encrypted_buffer = CreateEncryptedBuffer(
      encrypted_block, kIv, subsamples, EncryptionPattern(1, 9));
  encrypted_buffer->set_timestamp(base::Days(2));
  encrypted_buffer->set_duration(base::Minutes(5));
  encrypted_buffer->set_is_key_frame(true);
  encrypted_buffer->WritableSideData().alpha_data =
      base::HeapArray<uint8_t>::CopiedFrom(encrypted_block);

  auto decrypted_buffer = DecryptCbcsBuffer(*encrypted_buffer, kKey);
  EXPECT_EQ(encrypted_buffer->timestamp(), decrypted_buffer->timestamp());
  EXPECT_EQ(encrypted_buffer->duration(), decrypted_buffer->duration());
  EXPECT_EQ(encrypted_buffer->end_of_stream(),
            decrypted_buffer->end_of_stream());
  EXPECT_EQ(encrypted_buffer->is_key_frame(), decrypted_buffer->is_key_frame());
  EXPECT_TRUE(decrypted_buffer->side_data());
  EXPECT_TRUE(
      encrypted_buffer->side_data()->Matches(*decrypted_buffer->side_data()));
}

TEST(CbcsDecryptorTest, DifferentPattern) {
  auto encrypted_block = Encrypt(kOneBlock, kKey, kIv);
  DCHECK_EQ(kBlockSize, encrypted_block.size());

  // Only 1 subsample, all encrypted data.
  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size())}};

  auto encrypted_buffer = CreateEncryptedBuffer(
      encrypted_block, kIv, subsamples, EncryptionPattern(1, 0));
  EXPECT_EQ(kOneBlock,
            base::as_byte_span(DecryptWithKey(encrypted_buffer, kKey)));
}

TEST(CbcsDecryptorTest, EmptyPattern) {
  auto encrypted_block = Encrypt(kOneBlock, kKey, kIv);
  DCHECK_EQ(kBlockSize, encrypted_block.size());

  // Only 1 subsample, all encrypted data.
  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size())}};

  // Pattern 0:0 treats the buffer as all encrypted.
  auto encrypted_buffer = CreateEncryptedBuffer(
      encrypted_block, kIv, subsamples, EncryptionPattern(0, 0));
  EXPECT_EQ(kOneBlock,
            base::as_byte_span(DecryptWithKey(encrypted_buffer, kKey)));
}

TEST(CbcsDecryptorTest, PatternTooLarge) {
  auto encrypted_block = Encrypt(kOneBlock, kKey, kIv);
  DCHECK_EQ(kBlockSize, encrypted_block.size());

  // Only 1 subsample, all encrypted data.
  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size())}};

  // Pattern 100:0 is too large, so decryption will fail.
  auto encrypted_buffer = CreateEncryptedBuffer(
      encrypted_block, kIv, subsamples, EncryptionPattern(100, 0));
  EXPECT_EQ(std::vector<uint8_t>(), DecryptWithKey(encrypted_buffer, kKey));
}

TEST(CbcsDecryptorTest, NoSubsamples) {
  auto encrypted_block = Encrypt(kOneBlock, kKey, kIv);
  DCHECK_EQ(kBlockSize, encrypted_block.size());

  std::vector<SubsampleEntry> subsamples = {};

  auto encrypted_buffer = CreateEncryptedBuffer(
      encrypted_block, kIv, subsamples, EncryptionPattern(1, 9));
  EXPECT_EQ(kOneBlock,
            base::as_byte_span(DecryptWithKey(encrypted_buffer, kKey)));
}

TEST(CbcsDecryptorTest, BadSubsamples) {
  auto encrypted_block = Encrypt(kOneBlock, kKey, kIv);

  // Subsample size > data size.
  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size() + 1)}};

  auto encrypted_buffer = CreateEncryptedBuffer(
      encrypted_block, kIv, subsamples, EncryptionPattern(1, 0));
  EXPECT_EQ(std::vector<uint8_t>(), DecryptWithKey(encrypted_buffer, kKey));
}

TEST(CbcsDecryptorTest, InvalidIv) {
  auto encrypted_block = Encrypt(kOneBlock, kKey, kIv);

  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size())}};

  // Use an invalid IV for decryption. Call should succeed, but return
  // something other than the original data.
  // clang-format off
  std::array<uint8_t, std::size(kIv)> kBadIv({
      'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a',
      'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a',
  });
  // clang-format on
  auto encrypted_buffer = CreateEncryptedBuffer(
      encrypted_block, kBadIv, subsamples, EncryptionPattern(1, 0));
  EXPECT_NE(kOneBlock,
            base::as_byte_span(DecryptWithKey(encrypted_buffer, kKey)));
}

TEST(CbcsDecryptorTest, InvalidKey) {
  auto encrypted_block = Encrypt(kOneBlock, kKey, kIv);

  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size())}};

  // Use a different key for decryption. Call should succeed, but return
  // something other than the original data.
  // clang-format off
  std::array<uint8_t, std::size(kKey)> kBadKey({
      'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b',
      'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b',
  });
  // clang-format on
  auto encrypted_buffer = CreateEncryptedBuffer(
      encrypted_block, kIv, subsamples, EncryptionPattern(1, 0));
  EXPECT_NE(kOneBlock,
            base::as_byte_span(DecryptWithKey(encrypted_buffer, kBadKey)));
}

TEST(CbcsDecryptorTest, PartialBlock) {
  // Only 1 subsample, all "encrypted" data. However, as it's not a full block,
  // it will be treated as unencrypted.
  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(kPartialBlock.size())}};

  auto encrypted_buffer = CreateEncryptedBuffer(kPartialBlock, kIv, subsamples,
                                                EncryptionPattern(1, 0));
  EXPECT_EQ(kPartialBlock,
            base::as_byte_span(DecryptWithKey(encrypted_buffer, kKey)));
}

TEST(CbcsDecryptorTest, SingleBlockWithExtraData) {
  // Create some data that is longer than a single block. The full block will
  // be encrypted, but the extra data at the end will be considered unencrypted.
  auto encrypted_block =
      Combine({Encrypt(kOneBlock, kKey, kIv), kPartialBlock});
  auto expected_result = Combine({kOneBlock, kPartialBlock});

  // Only 1 subsample, all "encrypted" data.
  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size())}};

  auto encrypted_buffer = CreateEncryptedBuffer(
      encrypted_block, kIv, subsamples, EncryptionPattern(1, 0));
  EXPECT_EQ(expected_result, DecryptWithKey(encrypted_buffer, kKey));
}

TEST(CbcsDecryptorTest, SkipBlock) {
  // Only 1 subsample, but all unencrypted data.
  std::vector<SubsampleEntry> subsamples = {
      {static_cast<uint32_t>(kOneBlock.size()), 0}};

  auto encrypted_buffer = CreateEncryptedBuffer(kOneBlock, kIv, subsamples,
                                                EncryptionPattern(1, 0));
  EXPECT_EQ(kOneBlock,
            base::as_byte_span(DecryptWithKey(encrypted_buffer, kKey)));
}

TEST(CbcsDecryptorTest, MultipleBlocks) {
  // Encrypt 2 copies of |kOneBlock| together using kKey and kIv.
  auto encrypted_block = Encrypt(Repeat(kOneBlock, 2), kKey, kIv);
  DCHECK_EQ(2 * kBlockSize, encrypted_block.size());

  // 1 subsample, 4 blocks in (1,1) pattern.
  // Encrypted blocks come from |encrypted_block|.
  // data:       | enc1 | clear | enc2 | clear |
  // subsamples: |         subsample#1         |
  //             |eeeeeeeeeeeeeeeeeeeeeeeeeeeee|
  auto input_data = Combine({GetBlock(1, encrypted_block), kOneBlock,
                             GetBlock(2, encrypted_block), kOneBlock});
  auto expected_result = Repeat(kOneBlock, 4);
  std::vector<SubsampleEntry> subsamples = {{0, 4 * kBlockSize}};

  auto encrypted_buffer = CreateEncryptedBuffer(input_data, kIv, subsamples,
                                                EncryptionPattern(1, 1));
  EXPECT_EQ(expected_result,
            base::as_byte_span(DecryptWithKey(encrypted_buffer, kKey)));
}

TEST(CbcsDecryptorTest, PartialPattern) {
  // Encrypt 4 copies of |kOneBlock| together using kKey and kIv.
  auto encrypted_block = Encrypt(Repeat(kOneBlock, 4), kKey, kIv);
  DCHECK_EQ(4 * kBlockSize, encrypted_block.size());

  // 1 subsample, 4 blocks in (8,2) pattern. Even though there is not a full
  // pattern (10 blocks), all 4 blocks should be decrypted.
  auto expected_result = Repeat(kOneBlock, 4);
  std::vector<SubsampleEntry> subsamples = {{0, 4 * kBlockSize}};

  auto encrypted_buffer = CreateEncryptedBuffer(
      encrypted_block, kIv, subsamples, EncryptionPattern(8, 2));
  EXPECT_EQ(expected_result,
            base::as_byte_span(DecryptWithKey(encrypted_buffer, kKey)));
}

TEST(CbcsDecryptorTest, SkipBlocks) {
  // Encrypt 5 blocks together using kKey and kIv.
  auto encrypted_block = Encrypt(Repeat(kOneBlock, 5), kKey, kIv);
  DCHECK_EQ(5 * kBlockSize, encrypted_block.size());

  // 1 subsample, 1 unencrypted block followed by 7 blocks in (2,1) pattern.
  // Encrypted blocks come from |encrypted_block|.
  // data:       | clear | enc1 | enc2 | clear | enc3 | enc4 | clear | enc5 |
  // subsamples: |                  subsample#1                             |
  //             |uuuuuuu eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee|
  auto input_data = Combine(
      {kOneBlock, GetBlock(1, encrypted_block), GetBlock(2, encrypted_block),
       kOneBlock, GetBlock(3, encrypted_block), GetBlock(4, encrypted_block),
       kOneBlock, GetBlock(5, encrypted_block)});
  auto expected_result = Repeat(kOneBlock, 8);
  std::vector<SubsampleEntry> subsamples = {{kBlockSize, 7 * kBlockSize}};

  auto encrypted_buffer = CreateEncryptedBuffer(input_data, kIv, subsamples,
                                                EncryptionPattern(2, 1));
  EXPECT_EQ(expected_result,
            base::as_byte_span(DecryptWithKey(encrypted_buffer, kKey)));
}

TEST(CbcsDecryptorTest, MultipleSubsamples) {
  // Encrypt |kOneBlock| using kKey and kIv.
  auto encrypted_block = Encrypt(kOneBlock, kKey, kIv);
  DCHECK_EQ(kBlockSize, encrypted_block.size());

  // 3 subsamples, each 1 block of |encrypted_block|.
  // data:       |  encrypted  |  encrypted  |  encrypted  |
  // subsamples: | subsample#1 | subsample#2 | subsample#3 |
  //             |eeeeeeeeeeeee|eeeeeeeeeeeee|eeeeeeeeeeeee|
  auto input_data = Repeat(encrypted_block, 3);
  auto expected_result = Repeat(kOneBlock, 3);
  std::vector<SubsampleEntry> subsamples = {
      {0, kBlockSize}, {0, kBlockSize}, {0, kBlockSize}};

  auto encrypted_buffer = CreateEncryptedBuffer(input_data, kIv, subsamples,
                                                EncryptionPattern(1, 0));
  EXPECT_EQ(expected_result,
            base::as_byte_span(DecryptWithKey(encrypted_buffer, kKey)));
}

TEST(CbcsDecryptorTest, MultipleSubsamplesWithClearBytes) {
  // Encrypt |kOneBlock| using kKey and kIv.
  auto encrypted_block = Encrypt(kOneBlock, kKey, kIv);
  DCHECK_EQ(kBlockSize, encrypted_block.size());

  // Combine into alternating clear/encrypted blocks in 3 subsamples. Split
  // the second and third clear blocks into part of encrypted data of the
  // previous block (which as a partial block will be considered unencrypted).
  // data:       | clear | encrypted | clear | encrypted | clear | encrypted |
  // subsamples: |    subsample#1     |    subsample#2        | subsample#3  |
  //             |uuuuuuu eeeeeeeeeeee|uuuuuu eeeeeeeeeeeeeeee|uu eeeeeeeeeee|
  auto input_data = Combine({kOneBlock, encrypted_block, kOneBlock,
                             encrypted_block, kOneBlock, encrypted_block});
  auto expected_result = Repeat(kOneBlock, 6);
  std::vector<SubsampleEntry> subsamples = {{kBlockSize, kBlockSize + 1},
                                            {kBlockSize - 1, kBlockSize + 10},
                                            {kBlockSize - 10, kBlockSize}};

  auto encrypted_buffer = CreateEncryptedBuffer(input_data, kIv, subsamples,
                                                EncryptionPattern(1, 0));
  EXPECT_EQ(expected_result,
            base::as_byte_span(DecryptWithKey(encrypted_buffer, kKey)));
}

}  // namespace media
