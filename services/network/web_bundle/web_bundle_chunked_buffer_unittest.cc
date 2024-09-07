// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/network/web_bundle/web_bundle_chunked_buffer.h"

#include "base/check.h"
#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {
constexpr char kNumeric10Chars[] = "0123456789";
constexpr char kSmallAlphabet10Chars[] = "abcdefghij";
constexpr char kLargeAlphabet10Chars[] = "ABCDEFGHIJ";
}  // namespace

class WebBundleChunkedBufferTest : public ::testing::Test {
 protected:
  using Chunk = WebBundleChunkedBuffer::Chunk;
  using ChunkVector = WebBundleChunkedBuffer::ChunkVector;

  struct ReadTestCase {
    uint64_t offset;
    uint64_t length;
    bool expected_contains_all;
    uint64_t expected_available_length;
    std::string expected_read_result;
  };
  void RunBasicReadTest(const WebBundleChunkedBuffer& buffer,
                        const ReadTestCase& test_case) {
    SCOPED_TRACE(::testing::Message() << "offset: " << test_case.offset
                                      << " length: " << test_case.length);
    EXPECT_EQ(test_case.expected_contains_all,
              buffer.ContainsAll(test_case.offset, test_case.length));
    EXPECT_EQ(test_case.expected_available_length,
              buffer.GetAvailableLength(test_case.offset, test_case.length));
    std::vector<unsigned char> data(test_case.length);
    EXPECT_EQ(test_case.expected_available_length,
              buffer.ReadData(test_case.offset, data));
    EXPECT_EQ(std::string(data.begin(),
                          data.begin() + test_case.expected_available_length),
              test_case.expected_read_result);

    if (test_case.expected_available_length > 0) {
      data = std::vector<unsigned char>(test_case.expected_available_length, 0);
      auto data_source =
          buffer.CreateDataSource(test_case.offset, test_case.length);
      EXPECT_EQ(test_case.expected_available_length, data_source->GetLength());
      auto result =
          data_source->Read(0, base::as_writable_chars(base::span(data)));
      EXPECT_EQ(MOJO_RESULT_OK, result.result);
      EXPECT_EQ(test_case.expected_available_length, result.bytes_read);
      EXPECT_EQ(std::string(data.begin(),
                            data.begin() + test_case.expected_available_length),
                test_case.expected_read_result);
    }
  }
};

TEST_F(WebBundleChunkedBufferTest, Chunk) {
  constexpr unsigned char kData[] = "Hello World!";
  constexpr size_t kDataLength = sizeof(kData);
  auto data = base::MakeRefCounted<base::RefCountedBytes>(kData);
  uint64_t start_pos = 10;
  Chunk chunk = Chunk(start_pos, std::move(data));
  EXPECT_EQ(start_pos, chunk.start_pos());
  EXPECT_EQ(start_pos + kDataLength, chunk.end_pos());
  EXPECT_EQ(kDataLength, chunk.size());
  EXPECT_EQ(0, memcmp(chunk.data(), kData, kDataLength));
}

TEST_F(WebBundleChunkedBufferTest, EmptyBuffer) {
  WebBundleChunkedBuffer buffer;
  EXPECT_TRUE(buffer.empty());
  EXPECT_TRUE(buffer.ContainsAll(0, 0));
  EXPECT_TRUE(buffer.ContainsAll(10, 0));
  EXPECT_FALSE(buffer.ContainsAll(0, 10));
  EXPECT_FALSE(buffer.ContainsAll(10, 10));
  EXPECT_EQ(0ull, buffer.GetAvailableLength(0, 0));
  EXPECT_EQ(0ull, buffer.GetAvailableLength(0, 10));
  EXPECT_EQ(0ull, buffer.GetAvailableLength(10, 10));
  EXPECT_TRUE(buffer.CreateDataSource(0, 0) == nullptr);
  EXPECT_TRUE(buffer.CreateDataSource(0, 10) == nullptr);
  EXPECT_TRUE(buffer.CreateDataSource(10, 10) == nullptr);
  std::vector<unsigned char> data(10);
  EXPECT_FALSE(buffer.ReadData(0, data));
  EXPECT_FALSE(buffer.ReadData(10, data));

  // Appendding 0 bytes doesn't do anything.
  buffer.Append({});
  EXPECT_TRUE(buffer.empty());
}

TEST_F(WebBundleChunkedBufferTest, ReadTest_OneChunk) {
  WebBundleChunkedBuffer buffer;
  buffer.Append(base::byte_span_from_cstring(kNumeric10Chars));

  ReadTestCase test_cases[] = {
      {0, 0, true, 0, ""},
      {0, 10, true, 10, "0123456789"},
      {0, 11, false, 10, "0123456789"},
      {1, 0, true, 0, ""},
      {1, 9, true, 9, "123456789"},
      {1, 10, false, 9, "123456789"},
      {5, 5, true, 5, "56789"},
      {5, 6, false, 5, "56789"},
      {9, 1, true, 1, "9"},
      {9, 2, false, 1, "9"},
      {10, 1, false, 0, ""},
  };

  for (const auto& test_case : test_cases) {
    RunBasicReadTest(buffer, test_case);
  }
}

TEST_F(WebBundleChunkedBufferTest, ReadTest_MultipleChunks) {
  WebBundleChunkedBuffer buffer;
  buffer.Append(base::byte_span_from_cstring(kNumeric10Chars));
  buffer.Append(base::byte_span_from_cstring(kSmallAlphabet10Chars));
  buffer.Append(base::byte_span_from_cstring(kLargeAlphabet10Chars));

  ReadTestCase test_cases1[] = {
      {0, 0, true, 0, ""},
      {0, 10, true, 10, "0123456789"},
      {0, 11, true, 11, "0123456789a"},
      {0, 20, true, 20, "0123456789abcdefghij"},
      {0, 21, true, 21, "0123456789abcdefghijA"},
      {0, 30, true, 30, "0123456789abcdefghijABCDEFGHIJ"},
      {0, 31, false, 30, "0123456789abcdefghijABCDEFGHIJ"},

      {1, 0, true, 0, ""},
      {1, 9, true, 9, "123456789"},
      {1, 10, true, 10, "123456789a"},
      {1, 19, true, 19, "123456789abcdefghij"},
      {1, 20, true, 20, "123456789abcdefghijA"},
      {1, 29, true, 29, "123456789abcdefghijABCDEFGHIJ"},
      {1, 30, false, 29, "123456789abcdefghijABCDEFGHIJ"},

      {13, 3, true, 3, "def"},
      {13, 10, true, 10, "defghijABC"},
      {13, 30, false, 17, "defghijABCDEFGHIJ"},

      {29, 10, false, 1, "J"},
      {30, 10, false, 0, ""},
  };

  for (const auto& test_case : test_cases1) {
    RunBasicReadTest(buffer, test_case);
  }

  // Append many chunks to trigger the binary search logic in FindChunk.
  for (size_t i = 0; i < 50; ++i) {
    buffer.Append(base::byte_span_from_cstring(kNumeric10Chars));
    buffer.Append(base::byte_span_from_cstring(kSmallAlphabet10Chars));
    buffer.Append(base::byte_span_from_cstring(kLargeAlphabet10Chars));
  }

  ReadTestCase test_cases2[] = {
      {30 * 3 + 5, 30, true, 30, "56789abcdefghijABCDEFGHIJ01234"},
      {30 * 9 + 8, 20, true, 20, "89abcdefghijABCDEFGH"},
  };
  for (const auto& test_case : test_cases2) {
    RunBasicReadTest(buffer, test_case);
  }
}

TEST_F(WebBundleChunkedBufferTest, PartialBuffer) {
  WebBundleChunkedBuffer buffer;

  buffer.Append(base::byte_span_from_cstring(kNumeric10Chars));
  buffer.Append(base::byte_span_from_cstring(kSmallAlphabet10Chars));
  buffer.Append(base::byte_span_from_cstring(kLargeAlphabet10Chars));

  // 0123456789 abcdefghij ABCDEFGHIJ
  //  ~~~~~~
  auto partial = buffer.CreatePartialBuffer(1, 5);
  EXPECT_EQ(1u, partial->chunks_.size());
  EXPECT_EQ(0ull, partial->chunks_[0].start_pos());

  // 0123456789 abcdefghij ABCDEFGHIJ
  //      ~~~~~
  partial = buffer.CreatePartialBuffer(5, 5);
  EXPECT_EQ(1u, partial->chunks_.size());
  EXPECT_EQ(0ull, partial->chunks_[0].start_pos());

  // 0123456789 abcdefghij ABCDEFGHIJ
  //      ~~~~~ ~
  partial = buffer.CreatePartialBuffer(5, 6);
  EXPECT_EQ(2u, partial->chunks_.size());
  EXPECT_EQ(0ull, partial->chunks_[0].start_pos());
  EXPECT_EQ(10ull, partial->chunks_[1].start_pos());

  // 0123456789 abcdefghij ABCDEFGHIJ
  //              ~~~~~~~~
  partial = buffer.CreatePartialBuffer(12, 8);
  EXPECT_EQ(1u, partial->chunks_.size());
  EXPECT_EQ(10ull, partial->chunks_[0].start_pos());

  // 0123456789 abcdefghij ABCDEFGHIJ
  //              ~~~~~~~~ ~
  partial = buffer.CreatePartialBuffer(12, 9);
  EXPECT_EQ(2u, partial->chunks_.size());
  EXPECT_EQ(10ull, partial->chunks_[0].start_pos());
  EXPECT_EQ(20ull, partial->chunks_[1].start_pos());

  // 0123456789 abcdefghij ABCDEFGHIJ
  //              ~~~~~~~~ ~~~~~~~
  partial = buffer.CreatePartialBuffer(12, 15);
  EXPECT_EQ(2u, partial->chunks_.size());
  EXPECT_EQ(10ull, partial->chunks_[0].start_pos());
  EXPECT_EQ(20ull, partial->chunks_[1].start_pos());
  ReadTestCase test_cases[] = {
      {9, 10, false, 0, ""},
      {10, 10, true, 10, "abcdefghij"},
      {10, 11, true, 11, "abcdefghijA"},
      {10, 19, true, 19, "abcdefghijABCDEFGHI"},
      {10, 20, true, 20, "abcdefghijABCDEFGHIJ"},
      {10, 21, false, 20, "abcdefghijABCDEFGHIJ"},
  };
  for (const auto& test_case : test_cases) {
    RunBasicReadTest(*partial, test_case);
  }
}

TEST_F(WebBundleChunkedBufferTest, FindChunk) {
  WebBundleChunkedBuffer buffer;
  // FindChunk() always returns chunks_.end() when empty.
  EXPECT_TRUE(buffer.FindChunk(0) == buffer.chunks_.end());
  EXPECT_TRUE(buffer.FindChunk(1) == buffer.chunks_.end());

  // Append many chunks to trigger the binary search logic in FindChunk.
  for (size_t i = 0; i < 50; ++i) {
    buffer.Append(base::byte_span_from_cstring(kNumeric10Chars));
  }
  for (int i = 0; i < 510; ++i) {
    SCOPED_TRACE(::testing::Message() << "i: " << i);
    EXPECT_EQ(i / 10, buffer.FindChunk(i) - buffer.chunks_.begin());
  }
  EXPECT_TRUE(buffer.FindChunk(510) == buffer.chunks_.end());

  // 0123456789 0123456789 0123456789 012...
  //                 ~~~~~ ~~~~~
  auto partial = buffer.CreatePartialBuffer(15, 10);
  EXPECT_TRUE(partial->FindChunk(9) == partial->chunks_.end());
  EXPECT_TRUE(partial->FindChunk(10) == partial->chunks_.begin());
  EXPECT_TRUE(partial->FindChunk(19) == partial->chunks_.begin());
  EXPECT_TRUE(partial->FindChunk(20) == partial->chunks_.begin() + 1);
  EXPECT_TRUE(partial->FindChunk(29) == partial->chunks_.begin() + 1);
  EXPECT_TRUE(partial->FindChunk(30) == partial->chunks_.end());
}

TEST_F(WebBundleChunkedBufferTest, DataSource) {
  WebBundleChunkedBuffer buffer;

  buffer.Append(base::byte_span_from_cstring(kNumeric10Chars));
  buffer.Append(base::byte_span_from_cstring(kSmallAlphabet10Chars));
  buffer.Append(base::byte_span_from_cstring(kLargeAlphabet10Chars));
  buffer.Append(base::byte_span_from_cstring(kNumeric10Chars));

  // 0123456789 abcdefghij ABCDEFGHIJ 0123456789
  //                 ~~~~~ ~~
  auto data_source = buffer.CreateDataSource(15, 7);
  ASSERT_TRUE(data_source);
  EXPECT_EQ(7ull, data_source->GetLength());

  struct TestCase {
    uint64_t offset;
    size_t length;
    MojoResult expected_result;
    uint64_t expected_bytes_read;
    std::string expected_read_result;
  } test_cases[] = {
      {0, 6, MOJO_RESULT_OK, 6, "fghijAXXXX"},
      {0, 7, MOJO_RESULT_OK, 7, "fghijABXXX"},
      {0, 8, MOJO_RESULT_OK, 7, "fghijABXXX"},
      {3, 4, MOJO_RESULT_OK, 4, "ijABXXXXXX"},
      {3, 5, MOJO_RESULT_OK, 4, "ijABXXXXXX"},
      {6, 10, MOJO_RESULT_OK, 1, "BXXXXXXXXX"},
      {7, 10, MOJO_RESULT_OUT_OF_RANGE, 0, "XXXXXXXXXX"},
      {8, 10, MOJO_RESULT_OUT_OF_RANGE, 0, "XXXXXXXXXX"},
  };
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(::testing::Message() << "offset: " << test_case.offset
                                      << " length: " << test_case.length);
    auto data = std::vector<unsigned char>(10, 'X');
    auto result = data_source->Read(
        test_case.offset, base::span<char>(reinterpret_cast<char*>(data.data()),
                                           test_case.length));
    EXPECT_EQ(test_case.expected_result, result.result);
    EXPECT_EQ(test_case.expected_bytes_read, result.bytes_read);
    EXPECT_EQ(test_case.expected_read_result,
              std::string(data.begin(), data.begin() + 10));
  }
}

}  // namespace network
