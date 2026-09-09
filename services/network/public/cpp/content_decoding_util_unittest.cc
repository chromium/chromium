// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/content_decoding_util.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "net/filter/filter_source_stream_test_util.h"
#include "services/network/public/cpp/basic_data_buffer_factory.h"
#include "services/network/public/cpp/data_buffer_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

// Returns the path to `services/test/data/decoder` directory.
base::FilePath GetTestDataDir() {
  base::FilePath source_root;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root);
  return source_root.AppendASCII("services")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("decoder");
}

std::string GetTestData(const std::string_view file_name) {
  base::FilePath file_path = GetTestDataDir().AppendASCII(file_name);
  std::string data;
  CHECK(base::ReadFileToString(file_path, &data));
  return data;
}

std::string GetStringFromBuffers(const DataBufferList& buffers) {
  std::string decoded_string;
  for (auto buffer : buffers) {
    decoded_string.append(reinterpret_cast<const char*>(buffer.data()),
                          buffer.size());
  }
  return decoded_string;
}

}  // namespace

TEST(ContentDecodingUtilTest, DecodeGzip) {
  std::string original_data = "Hello, this is a test string to be gzipped!";
  std::vector<uint8_t> compressed_data =
      net::CompressGzip(original_data, /*gzip_framing=*/true);

  auto factory = base::MakeRefCounted<BasicDataBufferFactory>();
  auto result = ContentDecodingUtil::Decode(
      compressed_data, {net::SourceStreamType::kGzip}, *factory);

  ASSERT_TRUE(result);
  EXPECT_EQ(GetStringFromBuffers(*result), original_data);
}

TEST(ContentDecodingUtilTest, DecodeBrotli) {
  std::string compressed_data = GetTestData("google.br");

  auto factory = base::MakeRefCounted<BasicDataBufferFactory>();
  auto result =
      ContentDecodingUtil::Decode(base::as_byte_span(compressed_data),
                                  {net::SourceStreamType::kBrotli}, *factory);

  ASSERT_TRUE(result);
  EXPECT_EQ(GetStringFromBuffers(*result), GetTestData("google.txt"));
}

TEST(ContentDecodingUtilTest, DecodeZstd) {
  std::string compressed_data = GetTestData("google.zst");

  auto factory = base::MakeRefCounted<BasicDataBufferFactory>();
  auto result =
      ContentDecodingUtil::Decode(base::as_byte_span(compressed_data),
                                  {net::SourceStreamType::kZstd}, *factory);

  ASSERT_TRUE(result);
  EXPECT_EQ(GetStringFromBuffers(*result), GetTestData("google.txt"));
}

TEST(ContentDecodingUtilTest, DecodeGzipLarge) {
  // Create a payload larger than the buffer size (64KB).
  std::string original_data;
  for (int i = 0; i < 2000; ++i) {
    original_data += "This is a test string to create a large payload. ";
  }
  std::vector<uint8_t> compressed_data =
      net::CompressGzip(original_data, /*gzip_framing=*/true);

  auto factory = base::MakeRefCounted<BasicDataBufferFactory>();
  auto result = ContentDecodingUtil::Decode(
      compressed_data, {net::SourceStreamType::kGzip}, *factory);

  ASSERT_TRUE(result);
  EXPECT_GT(result->size(), 1u);  // Ensures multiple buffers were used

  EXPECT_EQ(GetStringFromBuffers(*result), original_data);
}

TEST(ContentDecodingUtilTest, DecodeError) {
  // Invalid GZIP data (not starting with gzip magic number)
  std::vector<uint8_t> invalid_data = {0x00, 0x01, 0x02, 0x03,
                                       0x04, 0x05, 0x06, 0x07};

  auto factory = base::MakeRefCounted<BasicDataBufferFactory>();
  auto result = ContentDecodingUtil::Decode(
      invalid_data, {net::SourceStreamType::kGzip}, *factory);

  EXPECT_FALSE(result);
}

TEST(ContentDecodingUtilTest, DecodeGzipAndBrotli) {
  std::string brotli_compressed_data = GetTestData("google.br");

  // Gzip the brotli compressed data
  std::vector<uint8_t> compressed_data =
      net::CompressGzip(brotli_compressed_data, /*gzip_framing=*/true);

  auto factory = base::MakeRefCounted<BasicDataBufferFactory>();
  auto result = ContentDecodingUtil::Decode(
      compressed_data,
      {net::SourceStreamType::kBrotli, net::SourceStreamType::kGzip}, *factory);
  ASSERT_TRUE(result);
  EXPECT_EQ(GetStringFromBuffers(*result), GetTestData("google.txt"));
}

TEST(ContentDecodingUtilTest, DecodeDeflate) {
  std::string original_data = "Hello, this is a test string to be deflated!";
  std::vector<uint8_t> compressed_data =
      net::CompressGzip(original_data, /*gzip_framing=*/false);

  auto factory = base::MakeRefCounted<BasicDataBufferFactory>();
  auto result = ContentDecodingUtil::Decode(
      compressed_data, {net::SourceStreamType::kDeflate}, *factory);

  ASSERT_TRUE(result);
  EXPECT_EQ(GetStringFromBuffers(*result), original_data);
}

TEST(ContentDecodingUtilTest, DecodeEmptyData) {
  auto factory = base::MakeRefCounted<BasicDataBufferFactory>();
  auto result = ContentDecodingUtil::Decode(
      base::span<const uint8_t>(), {net::SourceStreamType::kGzip}, *factory);

  ASSERT_TRUE(result);
  EXPECT_TRUE(result->empty());
}

}  // namespace network
