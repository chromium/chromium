// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/large_blob.h"

#include <optional>

#include "device/fido/fido_parsing_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

class FidoLargeBlobTest : public testing::Test {};

// An empty CBOR array (0x80) followed by LEFT(SHA-256(h'80'), 16).
const std::array<uint8_t, 17> kValidEmptyLargeBlobArray = {
    0x80, 0x76, 0xbe, 0x8b, 0x52, 0x8d, 0x00, 0x75, 0xf7,
    0xaa, 0xe9, 0x8d, 0x6f, 0xa5, 0x7a, 0x6d, 0x3c};

// Something that is not an empty CBOR array (0x10) followed by
// LEFT(SHA-256(h'10'), 16).
const std::array<uint8_t, 17> kInvalidLargeBlobArray = {
    0x10, 0xc5, 0x55, 0xea, 0xb4, 0x5d, 0x08, 0x84, 0x5a,
    0xe9, 0xf1, 0x0d, 0x45, 0x2a, 0x99, 0xbf, 0xcb};

// An "valid" CBOR large blob array with two entries. The first entry is not a
// valid large blob map structure. The second entry is valid.
const std::array<uint8_t, 45> kValidLargeBlobArray = {
    0x82, 0xA2, 0x02, 0x42, 0x11, 0x11, 0x03, 0x02, 0xA3, 0x01, 0x42, 0x22,
    0x22, 0x02, 0x4C, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
    0x30, 0x31, 0x32, 0x03, 0x02, 0x9b, 0x33, 0x75, 0x6c, 0x0a, 0x84, 0xdf,
    0x32, 0xcc, 0xd0, 0xc8, 0x96, 0xea, 0xa7, 0x99, 0x13};

TEST_F(FidoLargeBlobTest, VerifyLargeBlobArrayIntegrityValid) {
  std::vector<uint8_t> large_blob_array =
      fido_parsing_utils::Materialize(kValidEmptyLargeBlobArray);
  EXPECT_TRUE(VerifyLargeBlobArrayIntegrity(large_blob_array));
}

TEST_F(FidoLargeBlobTest, VerifyLargeBlobArrayIntegrityInvalid) {
  std::vector<uint8_t> large_blob_array =
      fido_parsing_utils::Materialize(kValidEmptyLargeBlobArray);
  large_blob_array[0] += 1;
  EXPECT_FALSE(VerifyLargeBlobArrayIntegrity(large_blob_array));

  large_blob_array = fido_parsing_utils::Materialize(kValidEmptyLargeBlobArray);
  large_blob_array.erase(large_blob_array.begin());
  EXPECT_FALSE(VerifyLargeBlobArrayIntegrity(large_blob_array));
}

TEST_F(FidoLargeBlobTest, LargeBlobArrayReader_MaterializeEmpty) {
  LargeBlobArrayReader large_blob_array_reader;
  large_blob_array_reader.Append(
      fido_parsing_utils::Materialize(kValidEmptyLargeBlobArray));
  EXPECT_EQ(0u, large_blob_array_reader.Materialize()->size());
}

TEST_F(FidoLargeBlobTest, LargeBlobArrayReader_MaterializeInvalidCbor) {
  LargeBlobArrayReader large_blob_array_reader;
  large_blob_array_reader.Append(
      fido_parsing_utils::Materialize(kInvalidLargeBlobArray));
  EXPECT_FALSE(large_blob_array_reader.Materialize());
}

TEST_F(FidoLargeBlobTest, LargeBlobArrayReader_MaterializeInvalidHash) {
  std::vector<uint8_t> large_blob_array =
      fido_parsing_utils::Materialize(kValidEmptyLargeBlobArray);
  large_blob_array[0] += 1;
  LargeBlobArrayReader large_blob_array_reader;
  large_blob_array_reader.Append(
      fido_parsing_utils::Materialize(large_blob_array));
  EXPECT_FALSE(large_blob_array_reader.Materialize());
}

TEST_F(FidoLargeBlobTest, LargeBlobArrayReader_MaterializeValid) {
  LargeBlobArrayReader large_blob_array_reader;
  large_blob_array_reader.Append(
      fido_parsing_utils::Materialize(kValidLargeBlobArray));
  cbor::Value::ArrayValue vector = *large_blob_array_reader.Materialize();
  EXPECT_EQ(2u, vector.size());
}

// Test popping the large blob array in a fragment size that does not evenly
// divide the length of the array.
TEST_F(FidoLargeBlobTest, LargeBlobArrayWriter_PopUnevenly) {
  const size_t fragment_size = 8;
  const size_t expected_fragments =
      kValidLargeBlobArray.size() / fragment_size + 1;
  size_t fragments = 0;
  ASSERT_NE(0u, kValidLargeBlobArray.size() % fragment_size);

  LargeBlobArrayWriter large_blob_array_writer({});
  std::vector<uint8_t> large_blob_array =
      fido_parsing_utils::Materialize(kValidLargeBlobArray);
  large_blob_array_writer.set_bytes_for_testing(large_blob_array);
  std::vector<uint8_t> reconstructed;
  EXPECT_TRUE(large_blob_array_writer.has_remaining_fragments());
  while (large_blob_array_writer.has_remaining_fragments()) {
    LargeBlobArrayFragment fragment =
        large_blob_array_writer.Pop(fragment_size);
    ++fragments;
    reconstructed.insert(reconstructed.end(), fragment.bytes.begin(),
                         fragment.bytes.end());
    EXPECT_EQ(fragments != expected_fragments,
              large_blob_array_writer.has_remaining_fragments());
  }

  EXPECT_EQ(expected_fragments, fragments);
  EXPECT_EQ(large_blob_array, reconstructed);
}

// Test popping the large blob array in a fragment size that evenly divides the
// length of the array.
TEST_F(FidoLargeBlobTest, LargeBlobArrayFragments_PopEvenly) {
  const size_t fragment_size = 9;
  const size_t expected_fragments = kValidLargeBlobArray.size() / fragment_size;
  size_t fragments = 0;
  ASSERT_EQ(0u, kValidLargeBlobArray.size() % fragment_size);

  LargeBlobArrayWriter large_blob_array_writer({});
  std::vector<uint8_t> large_blob_array =
      fido_parsing_utils::Materialize(kValidLargeBlobArray);
  large_blob_array_writer.set_bytes_for_testing(large_blob_array);
  std::vector<uint8_t> reconstructed;
  EXPECT_TRUE(large_blob_array_writer.has_remaining_fragments());
  while (large_blob_array_writer.has_remaining_fragments()) {
    LargeBlobArrayFragment fragment =
        large_blob_array_writer.Pop(fragment_size);
    ++fragments;
    reconstructed.insert(reconstructed.end(), fragment.bytes.begin(),
                         fragment.bytes.end());
    EXPECT_EQ(fragments != expected_fragments,
              large_blob_array_writer.has_remaining_fragments());
  }

  EXPECT_EQ(expected_fragments, fragments);
  EXPECT_EQ(large_blob_array, reconstructed);
}

}  // namespace

}  // namespace device
