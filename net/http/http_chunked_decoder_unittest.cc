// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/http/http_chunked_decoder.h"

#include <memory>
#include <string>
#include <vector>

#include "base/format_macros.h"
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

void RunTest(const char* const inputs[],
             size_t num_inputs,
             const char* expected_output,
             bool expected_eof,
             int bytes_after_eof) {
  HttpChunkedDecoder decoder;
  EXPECT_FALSE(decoder.reached_eof());

  std::string result;

  for (size_t i = 0; i < num_inputs; ++i) {
    std::string input = inputs[i];
    int n = decoder.FilterBuf(&input[0], static_cast<int>(input.size()));
    EXPECT_GE(n, 0);
    if (n > 0)
      result.append(input.data(), n);
  }

  EXPECT_EQ(expected_output, result);
  EXPECT_EQ(expected_eof, decoder.reached_eof());
  EXPECT_EQ(bytes_after_eof, decoder.bytes_after_eof());
}

// Feed the inputs to the decoder, until it returns an error.
void RunTestUntilFailure(const char* const inputs[],
                         size_t num_inputs,
                         size_t fail_index) {
  HttpChunkedDecoder decoder;
  EXPECT_FALSE(decoder.reached_eof());

  for (size_t i = 0; i < num_inputs; ++i) {
    std::string input = inputs[i];
    int n = decoder.FilterBuf(&input[0], static_cast<int>(input.size()));
    if (n < 0) {
      EXPECT_THAT(n, IsError(ERR_INVALID_CHUNKED_ENCODING));
      EXPECT_EQ(fail_index, i);
      return;
    }
  }
  FAIL();  // We should have failed on the |fail_index| iteration of the loop.
}

TEST(HttpChunkedDecoderTest, Basic) {
  const char* const inputs[] = {
    "B\r\nhello hello\r\n0\r\n\r\n"
  };
  RunTest(inputs, std::size(inputs), "hello hello", true, 0);
}

TEST(HttpChunkedDecoderTest, OneChunk) {
  const char* const inputs[] = {
    "5\r\nhello\r\n"
  };
  RunTest(inputs, std::size(inputs), "hello", false, 0);
}

TEST(HttpChunkedDecoderTest, Typical) {
  const char* const inputs[] = {
    "5\r\nhello\r\n",
    "1\r\n \r\n",
    "5\r\nworld\r\n",
    "0\r\n\r\n"
  };
  RunTest(inputs, std::size(inputs), "hello world", true, 0);
}

TEST(HttpChunkedDecoderTest, Incremental) {
  const char* const inputs[] = {
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
  RunTest(inputs, std::size(inputs), "hello", true, 0);
}

// Same as above, but group carriage returns with previous input.
TEST(HttpChunkedDecoderTest, Incremental2) {
  const char* const inputs[] = {
    "5\r",
    "\n",
    "hello\r",
    "\n",
    "0\r",
    "\n\r",
    "\n"
  };
  RunTest(inputs, std::size(inputs), "hello", true, 0);
}

TEST(HttpChunkedDecoderTest, LF_InsteadOf_CRLF) {
  // Compatibility: [RFC 7230 - Invalid]
  // {Firefox3} - Valid
  // {IE7, Safari3.1, Opera9.51} - Invalid
  const char* const inputs[] = {
    "5\nhello\n",
    "1\n \n",
    "5\nworld\n",
    "0\n\n"
  };
  RunTest(inputs, std::size(inputs), "hello world", true, 0);
}

TEST(HttpChunkedDecoderTest, Extensions) {
  const char* const inputs[] = {
    "5;x=0\r\nhello\r\n",
    "0;y=\"2 \"\r\n\r\n"
  };
  RunTest(inputs, std::size(inputs), "hello", true, 0);
}

TEST(HttpChunkedDecoderTest, Trailers) {
  const char* const inputs[] = {
    "5\r\nhello\r\n",
    "0\r\n",
    "Foo: 1\r\n",
    "Bar: 2\r\n",
    "\r\n"
  };
  RunTest(inputs, std::size(inputs), "hello", true, 0);
}

TEST(HttpChunkedDecoderTest, TrailersUnfinished) {
  const char* const inputs[] = {
    "5\r\nhello\r\n",
    "0\r\n",
    "Foo: 1\r\n"
  };
  RunTest(inputs, std::size(inputs), "hello", false, 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_TooBig) {
  const char* const inputs[] = {
    // This chunked body is not terminated.
    // However we will fail decoding because the chunk-size
    // number is larger than we can handle.
    "48469410265455838241\r\nhello\r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, std::size(inputs), 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_0X) {
  const char* const inputs[] = {
    // Compatibility [RFC 7230 - Invalid]:
    // {Safari3.1, IE7} - Invalid
    // {Firefox3, Opera 9.51} - Valid
    "0x5\r\nhello\r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, std::size(inputs), 0);
}

TEST(HttpChunkedDecoderTest, ChunkSize_TrailingSpace) {
  const char* const inputs[] = {
    // Compatibility [RFC 7230 - Invalid]:
    // {IE7, Safari3.1, Firefox3, Opera 9.51} - Valid
    //
    // At least yahoo.com depends on this being valid.
    "5      \r\nhello\r\n",
    "0\r\n\r\n"
  };
  RunTest(inputs, std::size(inputs), "hello", true, 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_TrailingTab) {
  const char* const inputs[] = {
    // Compatibility [RFC 7230 - Invalid]:
    // {IE7, Safari3.1, Firefox3, Opera 9.51} - Valid
    "5\t\r\nhello\r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, std::size(inputs), 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_TrailingFormFeed) {
  const char* const inputs[] = {
    // Compatibility [RFC 7230- Invalid]:
    // {Safari3.1} - Invalid
    // {IE7, Firefox3, Opera 9.51} - Valid
    "5\f\r\nhello\r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, std::size(inputs), 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_TrailingVerticalTab) {
  const char* const inputs[] = {
    // Compatibility [RFC 7230 - Invalid]:
    // {Safari 3.1} - Invalid
    // {IE7, Firefox3, Opera 9.51} - Valid
    "5\v\r\nhello\r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, std::size(inputs), 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_TrailingNonHexDigit) {
  const char* const inputs[] = {
    // Compatibility [RFC 7230 - Invalid]:
    // {Safari 3.1} - Invalid
    // {IE7, Firefox3, Opera 9.51} - Valid
    "5H\r\nhello\r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, std::size(inputs), 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_LeadingSpace) {
  const char* const inputs[] = {
    // Compatibility [RFC 7230 - Invalid]:
    // {IE7} - Invalid
    // {Safari 3.1, Firefox3, Opera 9.51} - Valid
    " 5\r\nhello\r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, std::size(inputs), 0);
}

TEST(HttpChunkedDecoderTest, InvalidLeadingSeparator) {
  const char* const inputs[] = {
    "\r\n5\r\nhello\r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, std::size(inputs), 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_NoSeparator) {
  const char* const inputs[] = {
    "5\r\nhello",
    "1\r\n \r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, std::size(inputs), 1);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_Negative) {
  const char* const inputs[] = {
    "8\r\n12345678\r\n-5\r\nhello\r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, std::size(inputs), 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_Plus) {
  const char* const inputs[] = {
    // Compatibility [RFC 7230 - Invalid]:
    // {IE7, Safari 3.1} - Invalid
    // {Firefox3, Opera 9.51} - Valid
    "+5\r\nhello\r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, std::size(inputs), 0);
}

TEST(HttpChunkedDecoderTest, InvalidConsecutiveCRLFs) {
  const char* const inputs[] = {
    "5\r\nhello\r\n",
    "\r\n\r\n\r\n\r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, std::size(inputs), 1);
}

TEST(HttpChunkedDecoderTest, ReallyBigChunks) {
  // Number of bytes sent through the chunked decoder per loop iteration. To
  // minimize runtime, should be the square root of the chunk lengths, below.
  const int64_t kWrittenBytesPerIteration = 0x10000;

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
    EXPECT_EQ(OK, decoder.FilterBuf(data.data(), data.size()));
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
                decoder.FilterBuf(data.data(), kWrittenBytesPerIteration));
      EXPECT_FALSE(decoder.reached_eof());
    }

    // Chunk terminator and the final chunk.
    char final_chunk[] = "\r\n0\r\n\r\n";
    EXPECT_EQ(OK, decoder.FilterBuf(final_chunk, std::size(final_chunk)));
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
  const char* const inputs[] = {"8000000000000000\r\nhello\r\n"};
  RunTestUntilFailure(inputs, std::size(inputs), 0);
}

TEST(HttpChunkedDecoderTest, ExcessiveChunkLen2) {
  // Smallest number that can't be represented as an unsigned int64.
  const char* const inputs[] = {"10000000000000000\r\nhello\r\n"};
  RunTestUntilFailure(inputs, std::size(inputs), 0);
}

TEST(HttpChunkedDecoderTest, BasicExtraData) {
  const char* const inputs[] = {
    "5\r\nhello\r\n0\r\n\r\nextra bytes"
  };
  RunTest(inputs, std::size(inputs), "hello", true, 11);
}

TEST(HttpChunkedDecoderTest, IncrementalExtraData) {
  const char* const inputs[] = {
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
  RunTest(inputs, std::size(inputs), "hello", true, 11);
}

TEST(HttpChunkedDecoderTest, MultipleExtraDataBlocks) {
  const char* const inputs[] = {
    "5\r\nhello\r\n0\r\n\r\nextra",
    " bytes"
  };
  RunTest(inputs, std::size(inputs), "hello", true, 11);
}

// Test when the line with the chunk length is too long.
TEST(HttpChunkedDecoderTest, LongChunkLengthLine) {
  int big_chunk_length = HttpChunkedDecoder::kMaxLineBufLen;
  auto big_chunk = std::make_unique<char[]>(big_chunk_length + 1);
  memset(big_chunk.get(), '0', big_chunk_length);
  big_chunk[big_chunk_length] = 0;
  const char* const inputs[] = {
    big_chunk.get(),
    "5"
  };
  RunTestUntilFailure(inputs, std::size(inputs), 1);
}

// Test when the extension portion of the line with the chunk length is too
// long.
TEST(HttpChunkedDecoderTest, LongLengthLengthLine) {
  int big_chunk_length = HttpChunkedDecoder::kMaxLineBufLen;
  auto big_chunk = std::make_unique<char[]>(big_chunk_length + 1);
  memset(big_chunk.get(), '0', big_chunk_length);
  big_chunk[big_chunk_length] = 0;
  const char* const inputs[] = {
    "5;",
    big_chunk.get()
  };
  RunTestUntilFailure(inputs, std::size(inputs), 1);
}

}  // namespace

}  // namespace net
