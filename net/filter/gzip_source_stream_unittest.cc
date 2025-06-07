// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/filter/gzip_source_stream.h"

#include <stdint.h>

#include <array>
#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_view_util.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/filter/filter_source_stream_test_util.h"
#include "net/filter/gzip_header.h"
#include "net/filter/mock_source_stream.h"
#include "net/filter/source_stream_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/zlib.h"

namespace net {

namespace {

const int kBigBufferSize = 4096;
const int kSmallBufferSize = 1;

enum class ReadResultType {
  // Each call to AddReadResult is a separate read from the lower layer
  // SourceStream.
  EVERYTHING_AT_ONCE,
  // Whenever AddReadResult is called, each byte is actually a separate read
  // result.
  ONE_BYTE_AT_A_TIME,
};

// How many bytes to leave unused at the end of |source_data_|. This margin is
// present so that tests that need to append data after the zlib EOF do not run
// out of room in the output buffer.
const size_t kEOFMargin = 64;

struct GzipTestParam {
  GzipTestParam(int buf_size,
                MockSourceStream::Mode read_mode,
                ReadResultType read_result_type)
      : buffer_size(buf_size),
        mode(read_mode),
        read_result_type(read_result_type) {}

  const int buffer_size;
  const MockSourceStream::Mode mode;
  const ReadResultType read_result_type;
};

}  // namespace

// Note that these tests cover GZipHeader::HasGZipHeader(), to avoid duplicating
// data passed to the method.
class GzipSourceStreamTest : public ::testing::TestWithParam<GzipTestParam> {
 protected:
  GzipSourceStreamTest() : output_buffer_size_(GetParam().buffer_size) {}

  // Helpful function to initialize the test fixture.|type| specifies which type
  // of GzipSourceStream to create. It must be one of TYPE_GZIP and
  // TYPE_DEFLATE.
  void Init(SourceStreamType type) {
    EXPECT_TRUE(SourceStreamType::kGzip == type ||
                SourceStreamType::kDeflate == type);
    source_data_len_ = kBigBufferSize - kEOFMargin;

    for (size_t i = 0; i < source_data_len_; i++)
      source_data_[i] = i % 256;

    encoded_data_ =
        CompressGzip(source_data_view(), type != SourceStreamType::kDeflate);

    output_buffer_ =
        base::MakeRefCounted<IOBufferWithSize>(output_buffer_size_);
    auto source = std::make_unique<MockSourceStream>();
    if (GetParam().read_result_type == ReadResultType::ONE_BYTE_AT_A_TIME)
      source->set_read_one_byte_at_a_time(true);
    source_ = source.get();
    stream_ = GzipSourceStream::Create(std::move(source), type);
  }

  // If MockSourceStream::Mode is ASYNC, completes reads from |mock_stream|
  // until there's no pending read, and then returns |callback|'s result, once
  // it's invoked. If Mode is not ASYNC, does nothing and returns
  // |previous_result|.
  int CompleteReadsIfAsync(int previous_result,
                           TestCompletionCallback* callback,
                           MockSourceStream* mock_stream) {
    if (GetParam().mode == MockSourceStream::ASYNC) {
      EXPECT_EQ(ERR_IO_PENDING, previous_result);
      while (mock_stream->awaiting_completion())
        mock_stream->CompleteNextRead();
      return callback->WaitForResult();
    }
    return previous_result;
  }

  // Returns `source_data_` as a string view, for comparison.
  std::string_view source_data_view() const {
    return base::as_string_view(source_data_).substr(0, source_data_len_);
  }
  size_t source_data_len() const { return source_data_len_; }

  base::span<uint8_t> encoded_span() { return encoded_data_; }

  MockSourceStream* source() { return source_; }
  GzipSourceStream* stream() { return stream_.get(); }

  // Reads from |stream_| until an error occurs or the EOF is reached.
  // When an error occurs, returns the net error code. When an EOF is reached,
  // returns the number of bytes read and appends data read to |output|.
  int ReadStream(std::string* output) {
    int bytes_read = 0;
    while (true) {
      TestCompletionCallback callback;
      int rv = stream_->Read(output_buffer_.get(), output_buffer_size_,
                             callback.callback());
      if (rv == ERR_IO_PENDING)
        rv = CompleteReadsIfAsync(rv, &callback, source());
      if (rv == OK)
        break;
      if (rv < OK)
        return rv;
      EXPECT_GT(rv, OK);
      bytes_read += rv;
      output->append(base::as_string_view(output_buffer_->first(rv)));
    }
    return bytes_read;
  }

 private:
  std::array<uint8_t, kBigBufferSize> source_data_;
  size_t source_data_len_;

  std::vector<uint8_t> encoded_data_;

  scoped_refptr<IOBuffer> output_buffer_;
  const int output_buffer_size_;

  raw_ptr<MockSourceStream, DanglingUntriaged> source_;
  std::unique_ptr<GzipSourceStream> stream_;
};

INSTANTIATE_TEST_SUITE_P(
    GzipSourceStreamTests,
    GzipSourceStreamTest,
    ::testing::Values(GzipTestParam(kBigBufferSize,
                                    MockSourceStream::SYNC,
                                    ReadResultType::EVERYTHING_AT_ONCE),
                      GzipTestParam(kSmallBufferSize,
                                    MockSourceStream::SYNC,
                                    ReadResultType::EVERYTHING_AT_ONCE),
                      GzipTestParam(kBigBufferSize,
                                    MockSourceStream::ASYNC,
                                    ReadResultType::EVERYTHING_AT_ONCE),
                      GzipTestParam(kSmallBufferSize,
                                    MockSourceStream::ASYNC,
                                    ReadResultType::EVERYTHING_AT_ONCE),
                      GzipTestParam(kBigBufferSize,
                                    MockSourceStream::SYNC,
                                    ReadResultType::ONE_BYTE_AT_A_TIME),
                      GzipTestParam(kSmallBufferSize,
                                    MockSourceStream::SYNC,
                                    ReadResultType::ONE_BYTE_AT_A_TIME),
                      GzipTestParam(kBigBufferSize,
                                    MockSourceStream::ASYNC,
                                    ReadResultType::ONE_BYTE_AT_A_TIME),
                      GzipTestParam(kSmallBufferSize,
                                    MockSourceStream::ASYNC,
                                    ReadResultType::ONE_BYTE_AT_A_TIME)));

TEST_P(GzipSourceStreamTest, EmptyStream) {
  Init(SourceStreamType::kDeflate);
  source()->AddReadResult(base::span<uint8_t>(), OK, GetParam().mode);
  TestCompletionCallback callback;
  std::string actual_output;
  int result = ReadStream(&actual_output);
  EXPECT_EQ(OK, result);
  EXPECT_EQ("DEFLATE", stream()->Description());
  EXPECT_FALSE(GZipHeader::HasGZipHeader(base::span<uint8_t>()));
}

TEST_P(GzipSourceStreamTest, DeflateOneBlock) {
  Init(SourceStreamType::kDeflate);
  source()->AddReadResult(encoded_span(), OK, GetParam().mode);
  source()->AddReadResult(base::span<uint8_t>(), OK, GetParam().mode);
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(static_cast<int>(source_data_len()), rv);
  EXPECT_EQ(source_data_view(), actual_output);
  EXPECT_EQ("DEFLATE", stream()->Description());
  EXPECT_FALSE(GZipHeader::HasGZipHeader(encoded_span()));
}

TEST_P(GzipSourceStreamTest, GzipOneBloc) {
  Init(SourceStreamType::kGzip);
  source()->AddReadResult(encoded_span(), OK, GetParam().mode);
  source()->AddReadResult(base::span<uint8_t>(), OK, GetParam().mode);
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(static_cast<int>(source_data_len()), rv);
  EXPECT_EQ(source_data_view(), actual_output);
  EXPECT_EQ("GZIP", stream()->Description());
  EXPECT_TRUE(GZipHeader::HasGZipHeader(encoded_span()));
}

TEST_P(GzipSourceStreamTest, DeflateTwoReads) {
  Init(SourceStreamType::kDeflate);
  source()->AddReadResult(encoded_span().first(10u), OK, GetParam().mode);
  source()->AddReadResult(encoded_span().subspan(10u), OK, GetParam().mode);
  source()->AddReadResult(base::span<uint8_t>(), OK, GetParam().mode);
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(static_cast<int>(source_data_len()), rv);
  EXPECT_EQ(source_data_view(), actual_output);
  EXPECT_EQ("DEFLATE", stream()->Description());
}

// Check that any extra bytes after the end of the gzipped data are silently
// ignored.
TEST_P(GzipSourceStreamTest, IgnoreDataAfterEof) {
  Init(SourceStreamType::kDeflate);
  const char kExtraData[] = "Hello, World!";
  std::string encoded_data_with_trailing_data(encoded_span().begin(),
                                              encoded_span().end());
  encoded_data_with_trailing_data.append(kExtraData, sizeof(kExtraData));
  source()->AddReadResult(base::as_byte_span(encoded_data_with_trailing_data),
                          OK, GetParam().mode);
  source()->AddReadResult(base::span<uint8_t>(), OK, GetParam().mode);
  // Compressed and uncompressed data get returned as separate Read() results,
  // so this test has to call Read twice.
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(static_cast<int>(source_data_len()), rv);
  EXPECT_EQ(source_data_view(), actual_output);
  EXPECT_EQ("DEFLATE", stream()->Description());
}

TEST_P(GzipSourceStreamTest, MissingZlibHeader) {
  Init(SourceStreamType::kDeflate);
  const size_t kZlibHeaderLen = 2;
  source()->AddReadResult(encoded_span().subspan(kZlibHeaderLen), OK,
                          GetParam().mode);
  source()->AddReadResult(base::span<uint8_t>(), OK, GetParam().mode);
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(static_cast<int>(source_data_len()), rv);
  EXPECT_EQ(source_data_view(), actual_output);
  EXPECT_EQ("DEFLATE", stream()->Description());
  EXPECT_FALSE(GZipHeader::HasGZipHeader(encoded_span().first(kZlibHeaderLen)));
}

TEST_P(GzipSourceStreamTest, CorruptGzipHeader) {
  Init(SourceStreamType::kGzip);
  encoded_span()[1] = 0;
  auto read_span = encoded_span();
  // Needed to a avoid a DCHECK that all reads were consumed.
  if (GetParam().read_result_type == ReadResultType::ONE_BYTE_AT_A_TIME)
    read_span = read_span.first(2u);
  source()->AddReadResult(read_span, OK, GetParam().mode);
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(ERR_CONTENT_DECODING_FAILED, rv);
  EXPECT_EQ("GZIP", stream()->Description());
  EXPECT_FALSE(GZipHeader::HasGZipHeader(encoded_span()));
}

// This test checks that the gzip stream source works correctly on 'golden' data
// as produced by gzip(1).
TEST_P(GzipSourceStreamTest, GzipCorrectness) {
  Init(SourceStreamType::kGzip);
  const char kDecompressedData[] = "Hello, World!";
  const uint8_t kGzipData[] = {
      // From:
      //   echo -n 'Hello, World!' | gzip | xxd -i | sed -e 's/^/  /'
      // The footer is the last 8 bytes.
      0x1f, 0x8b, 0x08, 0x00, 0x2b, 0x02, 0x84, 0x55, 0x00, 0x03, 0xf3,
      0x48, 0xcd, 0xc9, 0xc9, 0xd7, 0x51, 0x08, 0xcf, 0x2f, 0xca, 0x49,
      0x51, 0x04, 0x00, 0xd0, 0xc3, 0x4a, 0xec, 0x0d, 0x00, 0x00, 0x00};
  source()->AddReadResult(kGzipData, OK, GetParam().mode);
  source()->AddReadResult(base::span<uint8_t>(), OK, GetParam().mode);
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(static_cast<int>(strlen(kDecompressedData)), rv);
  EXPECT_EQ(kDecompressedData, actual_output);
  EXPECT_EQ("GZIP", stream()->Description());
  EXPECT_TRUE(GZipHeader::HasGZipHeader(base::as_byte_span(kGzipData)));
}

// Same as GzipCorrectness except that last 8 bytes are removed to test that the
// implementation can handle missing footer.
TEST_P(GzipSourceStreamTest, GzipCorrectnessWithoutFooter) {
  Init(SourceStreamType::kGzip);
  const char kDecompressedData[] = "Hello, World!";
  const uint8_t kGzipData[] = {
      // From:
      //   echo -n 'Hello, World!' | gzip | xxd -i | sed -e 's/^/  /'
      // with the 8 footer bytes removed.
      0x1f, 0x8b, 0x08, 0x00, 0x2b, 0x02, 0x84, 0x55, 0x00,
      0x03, 0xf3, 0x48, 0xcd, 0xc9, 0xc9, 0xd7, 0x51, 0x08,
      0xcf, 0x2f, 0xca, 0x49, 0x51, 0x04, 0x00};
  source()->AddReadResult(kGzipData, OK, GetParam().mode);
  source()->AddReadResult(base::span<uint8_t>(), OK, GetParam().mode);
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(static_cast<int>(strlen(kDecompressedData)), rv);
  EXPECT_EQ(kDecompressedData, actual_output);
  EXPECT_EQ("GZIP", stream()->Description());
  EXPECT_TRUE(GZipHeader::HasGZipHeader(base::as_byte_span(kGzipData)));
}

// Test with the same compressed data as the above tests, but uses deflate with
// header and checksum. Tests the Z_STREAM_END case in
// STATE_SNIFFING_DEFLATE_HEADER.
TEST_P(GzipSourceStreamTest, DeflateWithAdler32) {
  Init(SourceStreamType::kDeflate);
  const char kDecompressedData[] = "Hello, World!";
  const uint8_t kGzipData[] = {0x78, 0x01, 0xf3, 0x48, 0xcd, 0xc9, 0xc9,
                               0xd7, 0x51, 0x08, 0xcf, 0x2f, 0xca, 0x49,
                               0x51, 0x04, 0x00, 0x1f, 0x9e, 0x04, 0x6a};
  source()->AddReadResult(kGzipData, OK, GetParam().mode);
  source()->AddReadResult(base::span<uint8_t>(), OK, GetParam().mode);
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(static_cast<int>(strlen(kDecompressedData)), rv);
  EXPECT_EQ(kDecompressedData, actual_output);
  EXPECT_EQ("DEFLATE", stream()->Description());
  EXPECT_FALSE(GZipHeader::HasGZipHeader(base::as_byte_span(kGzipData)));
}

TEST_P(GzipSourceStreamTest, DeflateWithBadAdler32) {
  Init(SourceStreamType::kDeflate);
  const uint8_t kGzipData[] = {0x78, 0x01, 0xf3, 0x48, 0xcd, 0xc9, 0xc9,
                               0xd7, 0x51, 0x08, 0xcf, 0x2f, 0xca, 0x49,
                               0x51, 0x04, 0x00, 0xFF, 0xFF, 0xFF, 0xFF};
  source()->AddReadResult(kGzipData, OK, GetParam().mode);
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(ERR_CONTENT_DECODING_FAILED, rv);
  EXPECT_EQ("DEFLATE", stream()->Description());
  EXPECT_FALSE(GZipHeader::HasGZipHeader(base::as_byte_span(kGzipData)));
}

TEST_P(GzipSourceStreamTest, DeflateWithoutHeaderWithAdler32) {
  Init(SourceStreamType::kDeflate);
  const char kDecompressedData[] = "Hello, World!";
  const uint8_t kGzipData[] = {0xf3, 0x48, 0xcd, 0xc9, 0xc9, 0xd7, 0x51,
                               0x08, 0xcf, 0x2f, 0xca, 0x49, 0x51, 0x04,
                               0x00, 0x1f, 0x9e, 0x04, 0x6a};
  source()->AddReadResult(kGzipData, OK, GetParam().mode);
  source()->AddReadResult(base::span<uint8_t>(), OK, GetParam().mode);
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(static_cast<int>(strlen(kDecompressedData)), rv);
  EXPECT_EQ(kDecompressedData, actual_output);
  EXPECT_EQ("DEFLATE", stream()->Description());
  EXPECT_FALSE(GZipHeader::HasGZipHeader(base::as_byte_span(kGzipData)));
}

TEST_P(GzipSourceStreamTest, DeflateWithoutHeaderWithBadAdler32) {
  Init(SourceStreamType::kDeflate);
  const uint8_t kGzipData[] = {0xf3, 0x48, 0xcd, 0xc9, 0xc9, 0xd7, 0x51,
                               0x08, 0xcf, 0x2f, 0xca, 0x49, 0x51, 0x04,
                               0x00, 0xFF, 0xFF, 0xFF, 0xFF};
  source()->AddReadResult(kGzipData, OK, GetParam().mode);
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(ERR_CONTENT_DECODING_FAILED, rv);
  EXPECT_EQ("DEFLATE", stream()->Description());
  EXPECT_FALSE(GZipHeader::HasGZipHeader(base::as_byte_span(kGzipData)));
}

}  // namespace net
