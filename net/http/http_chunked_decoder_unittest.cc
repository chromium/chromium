// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_chunked_decoder.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/format_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_view_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/net_errors.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

typedef testing::Test HttpChunkedDecoderTest;

void RunTest(base::span<const std::string_view> inputs,
             const char* expected_output,
             bool expected_eof,
             int bytes_after_eof) {
  HttpChunkedDecoder decoder;
  EXPECT_FALSE(decoder.reached_eof());

  std::string result;

  for (const auto input : inputs) {
    // FilterBuf() modifies the input, so copy to a vector, which can be written
    // to.
    std::vector<uint8_t> copy(input.begin(), input.end());
    int n = decoder.FilterBuf(copy);
    EXPECT_GE(n, 0);
    if (n > 0) {
      copy.resize(n);
      result.append(copy.begin(), copy.end());
    }
  }

  EXPECT_EQ(expected_output, result);
  EXPECT_EQ(expected_eof, decoder.reached_eof());
  EXPECT_EQ(bytes_after_eof, decoder.bytes_after_eof());
}

// Feed the inputs to the decoder, until it returns an error.
void RunTestUntilFailure(base::span<const std::string_view> inputs,
                         size_t fail_index) {
  HttpChunkedDecoder decoder;
  EXPECT_FALSE(decoder.reached_eof());

  for (size_t i = 0; i < inputs.size(); ++i) {
    // FilterBuf() modifies the input, so copy to a vector, which can be written
    // to.
    std::vector<uint8_t> copy(inputs[i].begin(), inputs[i].end());
    int n = decoder.FilterBuf(copy);
    if (n < 0) {
      EXPECT_THAT(n, IsError(ERR_INVALID_CHUNKED_ENCODING));
      EXPECT_EQ(fail_index, i);
      return;
    }
  }
  FAIL();  // We should have failed on the |fail_index| iteration of the loop.
}

TEST(HttpChunkedDecoderTest, Basic) {
  const std::vector<std::string_view> inputs = {
      "B\r\nhello hello\r\n0\r\n\r\n"};
  RunTest(inputs, "hello hello", true, 0);
}

TEST(HttpChunkedDecoderTest, OneChunk) {
  const std::vector<std::string_view> inputs = {"5\r\nhello\r\n"};
  RunTest(inputs, "hello", false, 0);
}

TEST(HttpChunkedDecoderTest, Typical) {
  // clang-format off
  const std::vector<std::string_view> inputs = {
      "5\r\nhello\r\n",
      "1\r\n \r\n",
      "5\r\nworld\r\n",
      "0\r\n\r\n"
  };
  // clang-format on

  RunTest(inputs, "hello world", true, 0);
}

TEST(HttpChunkedDecoderTest, Incremental) {
  // clang-format off
  const std::vector<std::string_view> inputs = {
      "5",
      "\r",
      "\n",
      "hello",
      "\r",
      "\n",
      "0",
      "\r",
      "\n",
      "\r",
      "\n"
  };
  // clang-format on

  RunTest(inputs, "hello", true, 0);
}

// Same as above, but group carriage returns with previous input.
TEST(HttpChunkedDecoderTest, Incremental2) {
  // clang-format off
  const std::vector<std::string_view> inputs = {
      "5\r",
      "\n",
      "hello\r",
      "\n",
      "0\r",
      "\n\r",
      "\n"
  };
  // clang-format on

  RunTest(inputs, "hello", true, 0);
}

TEST(HttpChunkedDecoderTest, LF_InsteadOf_CRLF) {
  // clang-format off
  const std::vector<std::string_view> inputs = {
      "5\nhello\n",
      "1\n \n",
      "5\nworld\n",
      "0\n\n"
  };
  // clang-format on

  RunTest(inputs, "hello world", true, 0);
}

TEST(HttpChunkedDecoderTest, Extensions) {
  // clang-format off
  const std::vector<std::string_view> inputs = {
      "5;x=0\r\nhello\r\n",
      "0;y=\"2 \"\r\n\r\n"
  };
  // clang-format on

  RunTest(inputs, "hello", true, 0);
}

TEST(HttpChunkedDecoderTest, Trailers) {
  // clang-format off
  const std::vector<std::string_view> inputs = {
      "5\r\nhello\r\n",
      "0\r\n",
      "Foo: 1\r\n",
      "Bar: 2\r\n",
      "\r\n"
  };
  // clang-format on

  RunTest(inputs, "hello", true, 0);
}

TEST(HttpChunkedDecoderTest, TrailersUnfinished) {
  // clang-format off
  const std::vector<std::string_view> inputs = {
      "5\r\nhello\r\n",
      "0\r\n",
      "Foo: 1\r\n"
  };
  // clang-format on

  RunTest(inputs, "hello", false, 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_TooBig) {
  // clang-format off
  const std::vector<std::string_view> inputs = {
      // This chunked body is not terminated.
      // However we will fail decoding because the chunk-size
      // number is larger than we can handle.
      "48469410265455838241\r\nhello\r\n",
      "0\r\n\r\n"
  };
  // clang-format on

  RunTestUntilFailure(inputs, 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_0X) {
  // clang-format off
  const std::vector<std::string_view> inputs = {
      "0x5\r\nhello\r\n",
      "0\r\n\r\n"
  };
  // clang-format on

  RunTestUntilFailure(inputs, 0);
}

// Note that this test covers behavior contrary to RFC 7230.
TEST(HttpChunkedDecoderTest, ChunkSize_TrailingSpace) {
  // clang-format off
  const std::vector<std::string_view> inputs = {
      "5      \r\nhello\r\n",
      "0\r\n\r\n"
  };
  // clang-format on

  RunTest(inputs, "hello", true, 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_TrailingTab) {
  // clang-format off
  const std::vector<std::string_view> inputs = {
      "5\t\r\nhello\r\n",
      "0\r\n\r\n"
  };
  // clang-format on

  RunTestUntilFailure(inputs, 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_TrailingFormFeed) {
  // clang-format off
  const std::vector<std::string_view> inputs = {
      "5\f\r\nhello\r\n",
      "0\r\n\r\n"
  };
  // clang-format on

  RunTestUntilFailure(inputs, 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_TrailingVerticalTab) {
  // clang-format off
  const std::vector<std::string_view> inputs = {
      "5\v\r\nhello\r\n",
      "0\r\n\r\n"
  };
  // clang-format on

  RunTestUntilFailure(inputs, 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_TrailingNonHexDigit) {
  // clang-format off
  const std::vector<std::string_view> inputs = {
      "5H\r\nhello\r\n",
      "0\r\n\r\n"
  };
  // clang-format on

  RunTestUntilFailure(inputs, 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_LeadingSpace) {
  // clang-format off
  const std::vector<std::string_view> inputs = {
      " 5\r\nhello\r\n",
      "0\r\n\r\n"
  };
  // clang-format on

  RunTestUntilFailure(inputs, 0);
}

TEST(HttpChunkedDecoderTest, InvalidLeadingSeparator) {
  // clang-format off
  const std::vector<std::string_view> inputs = {
       "\r\n5\r\nhello\r\n",
       "0\r\n\r\n"
  };
  // clang-format on

  RunTestUntilFailure(inputs, 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_NoSeparator) {
  // clang-format off
  const std::vector<std::string_view> inputs = {
      "5\r\nhello",
      "1\r\n \r\n",
      "0\r\n\r\n"
  };
  // clang-format on

  RunTestUntilFailure(inputs, 1);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_Negative) {
  // clang-format off
  const std::vector<std::string_view> inputs = {
      "8\r\n12345678\r\n-5\r\nhello\r\n",
      "0\r\n\r\n"
  };
  // clang-format on

  RunTestUntilFailure(inputs, 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_Plus) {
  // clang-format off
  const std::vector<std::string_view> inputs = {
      "+5\r\nhello\r\n",
      "0\r\n\r\n"
  };
  // clang-format on

  RunTestUntilFailure(inputs, 0);
}

TEST(HttpChunkedDecoderTest, InvalidConsecutiveCRLFs) {
  // clang-format off
  const std::vector<std::string_view> inputs = {
      "5\r\nhello\r\n",
      "\r\n\r\n\r\n\r\n",
      "0\r\n\r\n"
  };
  // clang-format on

  RunTestUntilFailure(inputs, 1);
}

TEST(HttpChunkedDecoderTest, ReallyBigChunks) {
  // Number of bytes sent through the chunked decoder per loop iteration. To
  // minimize runtime, should be the square root of the chunk lengths, below.
  const size_t kWrittenBytesPerIteration = 0x10000;

  // Length of chunks to test. Must be multiples of kWrittenBytesPerIteration.
  int64_t kChunkLengths[] = {
      // Overflows when cast to a signed int32.
      0x0c0000000,
      // Overflows when cast to an unsigned int32.
      0x100000000,
  };

  for (int64_t chunk_length : kChunkLengths) {
    HttpChunkedDecoder decoder;
    EXPECT_FALSE(decoder.reached_eof());

    // Feed just the header to the decode.
    std::string chunk_header =
        base::StringPrintf("%" PRIx64 "\r\n", chunk_length);
    std::vector<char> data(chunk_header.begin(), chunk_header.end());
    EXPECT_EQ(OK, decoder.FilterBuf(base::as_writable_byte_span(data)));
    EXPECT_FALSE(decoder.reached_eof());

    // Set |data| to be kWrittenBytesPerIteration long, and have a repeating
    // pattern.
    data.clear();
    data.reserve(kWrittenBytesPerIteration);
    for (size_t i = 0; i < kWrittenBytesPerIteration; i++) {
      data.push_back(static_cast<char>(i));
    }

    // Repeatedly feed the data to the chunked decoder. Since the data doesn't
    // include any chunk lengths, the decode will never have to move the data,
    // and should run fairly quickly.
    for (int64_t total_written = 0; total_written < chunk_length;
         total_written += kWrittenBytesPerIteration) {
      EXPECT_EQ(kWrittenBytesPerIteration,
                base::checked_cast<size_t>(
                    decoder.FilterBuf(base::as_writable_byte_span(data).first(
                        kWrittenBytesPerIteration))));
      EXPECT_FALSE(decoder.reached_eof());
    }

    // Chunk terminator and the final chunk.
    char final_chunk[] = "\r\n0\r\n\r\n";
    EXPECT_EQ(OK, decoder.FilterBuf(base::as_writable_byte_span(final_chunk)));
    EXPECT_TRUE(decoder.reached_eof());

    // Since |data| never included any chunk headers, it should not have been
    // modified.
    for (size_t i = 0; i < kWrittenBytesPerIteration; i++) {
      EXPECT_EQ(static_cast<char>(i), data[i]);
    }
  }
}

TEST(HttpChunkedDecoderTest, ExcessiveChunkLen) {
  // Smallest number that can't be represented as a signed int64.
  const std::vector<std::string_view> inputs = {
      "8000000000000000\r\nhello\r\n"};
  RunTestUntilFailure(inputs, 0);
}

TEST(HttpChunkedDecoderTest, ExcessiveChunkLen2) {
  // Smallest number that can't be represented as an unsigned int64.
  const std::vector<std::string_view> inputs = {
      "10000000000000000\r\nhello\r\n"};
  RunTestUntilFailure(inputs, 0);
}

TEST(HttpChunkedDecoderTest, BasicExtraData) {
  const std::vector<std::string_view> inputs = {
      "5\r\nhello\r\n0\r\n\r\nextra bytes"};
  RunTest(inputs, "hello", true, 11);
}

TEST(HttpChunkedDecoderTest, IncrementalExtraData) {
  // clang-format off
  const std::vector<std::string_view> inputs = {
      "5",
      "\r",
      "\n",
      "hello",
      "\r",
      "\n",
      "0",
      "\r",
      "\n",
      "\r",
      "\nextra bytes"
  };
  // clang-format on

  RunTest(inputs, "hello", true, 11);
}

TEST(HttpChunkedDecoderTest, MultipleExtraDataBlocks) {
  // clang-format off
  const std::vector<std::string_view> inputs = {
      "5\r\nhello\r\n0\r\n\r\nextra",
      " bytes"
  };
  // clang-format on

  RunTest(inputs, "hello", true, 11);
}

// Test when the line with the chunk length is too long.
TEST(HttpChunkedDecoderTest, LongChunkLengthLine) {
  const int kBigChunkLength = HttpChunkedDecoder::kMaxLineBufLen;
  std::vector<char> big_chunk(kBigChunkLength, '0');
  const std::vector<std::string_view> inputs = {base::as_string_view(big_chunk),
                                                "5"};
  RunTestUntilFailure(inputs, 1);
}

// Test when the extension portion of the line with the chunk length is too
// long.
TEST(HttpChunkedDecoderTest, LongLengthLengthLine) {
  const int kBigChunkLength = HttpChunkedDecoder::kMaxLineBufLen;
  std::vector<char> big_chunk(kBigChunkLength, '0');
  const std::vector<std::string_view> inputs = {
      "5;", base::as_string_view(big_chunk)};
  RunTestUntilFailure(inputs, 1);
}

}  // namespace

}  // namespace net
