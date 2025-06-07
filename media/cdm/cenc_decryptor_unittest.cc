// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cenc_decryptor.h"

#include <stdint.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/strings/string_view_util.h"
#include "base/time/time.h"
#include "crypto/aes_ctr.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

// Keys and IVs have to be 128 bits.
const std::array<uint8_t, 16> kKey({0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
                                    0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11,
                                    0x12, 0x13});
const std::array<uint8_t, 16> kIv({0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,
                                   0x27, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00});

const auto kOneBlock =
    std::to_array<uint8_t>({'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
                            'k', 'l', 'm', 'n', 'o', 'p'});
const auto kPartialBlock =
    std::to_array<uint8_t>({'a', 'b', 'c', 'd', 'e', 'f'});

// Combine multiple std::vector<uint8_t> into one.
std::vector<uint8_t> Combine(
    const std::vector<base::span<const uint8_t>>& inputs) {
  std::vector<uint8_t> result;
  for (const auto& input : inputs)
    result.insert(result.end(), input.begin(), input.end());

  return result;
}

// Returns a std::vector<uint8_t> containing |count| copies of |input|.
std::vector<uint8_t> Repeat(base::span<const uint8_t> input, size_t count) {
  std::vector<uint8_t> result;
  for (size_t i = 0; i < count; ++i)
    result.insert(result.end(), input.begin(), input.end());
  return result;
}

scoped_refptr<DecoderBuffer> CreateEncryptedBuffer(
    base::span<const uint8_t> data,
    base::span<const uint8_t> iv,
    const std::vector<SubsampleEntry>& subsample_entries) {
  EXPECT_FALSE(data.empty());
  EXPECT_FALSE(iv.empty());

  scoped_refptr<DecoderBuffer> encrypted_buffer = DecoderBuffer::CopyFrom(data);

  // Key_ID is never used.
  encrypted_buffer->set_decrypt_config(DecryptConfig::CreateCencConfig(
      "key_id", std::string(base::as_string_view(iv)), subsample_entries));
  return encrypted_buffer;
}

}  // namespace

using CencDecryptorTest = ::testing::Test;

TEST(CencDecryptorTest, OneBlock) {
  auto encrypted_block = crypto::aes_ctr::Encrypt(kKey, kIv, kOneBlock);

  // Only 1 subsample, all encrypted data.
  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size())}};

  auto encrypted_buffer =
      CreateEncryptedBuffer(encrypted_block, kIv, subsamples);
  auto decrypted_buffer = DecryptCencBuffer(*encrypted_buffer, kKey);
  ASSERT_TRUE(decrypted_buffer);
  EXPECT_EQ(kOneBlock, base::span(*decrypted_buffer));
}

TEST(CencDecryptorTest, ExtraData) {
  auto encrypted_block = crypto::aes_ctr::Encrypt(kKey, kIv, kOneBlock);

  // Only 1 subsample, all encrypted data.
  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size())}};

  auto encrypted_buffer =
      CreateEncryptedBuffer(encrypted_block, kIv, subsamples);
  encrypted_buffer->set_timestamp(base::Days(2));
  encrypted_buffer->set_duration(base::Minutes(5));
  encrypted_buffer->set_is_key_frame(true);
  encrypted_buffer->WritableSideData().alpha_data =
      base::HeapArray<uint8_t>::CopiedFrom(encrypted_block);

  auto decrypted_buffer = DecryptCencBuffer(*encrypted_buffer, kKey);
  ASSERT_TRUE(decrypted_buffer);
  EXPECT_EQ(encrypted_buffer->timestamp(), decrypted_buffer->timestamp());
  EXPECT_EQ(encrypted_buffer->duration(), decrypted_buffer->duration());
  EXPECT_EQ(encrypted_buffer->end_of_stream(),
            decrypted_buffer->end_of_stream());
  EXPECT_EQ(encrypted_buffer->is_key_frame(), decrypted_buffer->is_key_frame());
  EXPECT_TRUE(decrypted_buffer->side_data());
  EXPECT_TRUE(
      encrypted_buffer->side_data()->Matches(*decrypted_buffer->side_data()));
}

TEST(CencDecryptorTest, NoSubsamples) {
  auto encrypted_block = crypto::aes_ctr::Encrypt(kKey, kIv, kOneBlock);

  // No subsamples specified.
  std::vector<SubsampleEntry> subsamples = {};

  auto encrypted_buffer =
      CreateEncryptedBuffer(encrypted_block, kIv, subsamples);
  auto decrypted_buffer = DecryptCencBuffer(*encrypted_buffer, kKey);
  ASSERT_TRUE(decrypted_buffer);
  EXPECT_EQ(kOneBlock, base::span(*decrypted_buffer));
}

TEST(CencDecryptorTest, BadSubsamples) {
  auto encrypted_block = crypto::aes_ctr::Encrypt(kKey, kIv, kOneBlock);

  // Subsample size > data size.
  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size() + 1)}};

  auto encrypted_buffer =
      CreateEncryptedBuffer(encrypted_block, kIv, subsamples);
  auto decrypted_buffer = DecryptCencBuffer(*encrypted_buffer, kKey);
  EXPECT_FALSE(decrypted_buffer);
}

TEST(CencDecryptorTest, InvalidIv) {
  auto encrypted_block = crypto::aes_ctr::Encrypt(kKey, kIv, kOneBlock);

  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size())}};

  // Use an invalid IV for decryption. Call should succeed, but return
  // something other than the original data.
  const std::array<uint8_t, 16> kBadIv{'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a',
                                       'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a'};
  auto encrypted_buffer =
      CreateEncryptedBuffer(encrypted_block, kBadIv, subsamples);
  auto decrypted_buffer = DecryptCencBuffer(*encrypted_buffer, kKey);
  ASSERT_TRUE(decrypted_buffer);
  EXPECT_NE(kOneBlock, base::span(*decrypted_buffer));
}

TEST(CencDecryptorTest, InvalidKey) {
  const std::array<uint8_t, 16> kBadKey{'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b',
                                        'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b'};
  auto encrypted_block = crypto::aes_ctr::Encrypt(kKey, kIv, kOneBlock);

  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size())}};

  // Use a different key for decryption. Call should succeed, but return
  // something other than the original data.
  auto encrypted_buffer =
      CreateEncryptedBuffer(encrypted_block, kIv, subsamples);
  auto decrypted_buffer = DecryptCencBuffer(*encrypted_buffer, kBadKey);
  ASSERT_TRUE(decrypted_buffer);
  EXPECT_NE(kOneBlock, base::span(*decrypted_buffer));
}

TEST(CencDecryptorTest, PartialBlock) {
  auto encrypted_block = crypto::aes_ctr::Encrypt(kKey, kIv, kPartialBlock);

  // Only 1 subsample, all encrypted data.
  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(encrypted_block.size())}};

  auto encrypted_buffer =
      CreateEncryptedBuffer(encrypted_block, kIv, subsamples);
  auto decrypted_buffer = DecryptCencBuffer(*encrypted_buffer, kKey);
  ASSERT_TRUE(decrypted_buffer);
  EXPECT_EQ(kPartialBlock, base::span(*decrypted_buffer));
}

TEST(CencDecryptorTest, MultipleSubsamples) {
  // Encrypt 3 copies of |one_block_| together.
  const auto plaintext = Repeat(kOneBlock, 3);
  auto encrypted_block = crypto::aes_ctr::Encrypt(kKey, kIv, plaintext);

  // Treat as 3 subsamples.
  std::vector<SubsampleEntry> subsamples = {
      {0, static_cast<uint32_t>(kOneBlock.size())},
      {0, static_cast<uint32_t>(kOneBlock.size())},
      {0, static_cast<uint32_t>(kOneBlock.size())}};

  auto encrypted_buffer =
      CreateEncryptedBuffer(encrypted_block, kIv, subsamples);
  auto decrypted_buffer = DecryptCencBuffer(*encrypted_buffer, kKey);
  ASSERT_TRUE(decrypted_buffer);
  EXPECT_EQ(plaintext, base::span(*decrypted_buffer));
}

TEST(CencDecryptorTest, MultipleSubsamplesWithClearBytes) {
  // Create a buffer that looks like:
  // subsamples: |    subsample#1    |     subsample#2     | subsample#3 |
  //             | clear | encrypted |  clear  | encrypted |    clear    |
  // source:     |  one  | partial*  | partial |   one*    |   partial   |
  // where * means the source is encrypted
  auto encrypted_block =
      crypto::aes_ctr::Encrypt(kKey, kIv, Combine({kPartialBlock, kOneBlock}));
  auto [encrypted_partial_block, encrypted_one_block] =
      base::span(encrypted_block).split_at(kPartialBlock.size());

  auto input_data = Combine({kOneBlock, encrypted_partial_block, kPartialBlock,
                             encrypted_one_block, kPartialBlock});
  auto expected_result = Combine(
      {kOneBlock, kPartialBlock, kPartialBlock, kOneBlock, kPartialBlock});
  std::vector<SubsampleEntry> subsamples = {
      {static_cast<uint32_t>(kOneBlock.size()),
       static_cast<uint32_t>(kPartialBlock.size())},
      {static_cast<uint32_t>(kPartialBlock.size()),
       static_cast<uint32_t>(kOneBlock.size())},
      {static_cast<uint32_t>(kPartialBlock.size()), 0}};

  auto encrypted_buffer = CreateEncryptedBuffer(input_data, kIv, subsamples);
  auto decrypted_buffer = DecryptCencBuffer(*encrypted_buffer, kKey);
  ASSERT_TRUE(decrypted_buffer);
  EXPECT_EQ(expected_result, base::span(*decrypted_buffer));
}

}  // namespace media
