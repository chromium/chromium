// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bit_cast.h"
#include "base/callback.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/filter/filter_source_stream_test_util.h"
#include "net/filter/gzip_source_stream.h"
#include "net/filter/mock_source_stream.h"
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

class GzipSourceStreamTest : public ::testing::TestWithParam<GzipTestParam> {
 protected:
  GzipSourceStreamTest() : output_buffer_size_(GetParam().buffer_size) {}

  // Helpful function to initialize the test fixture.|type| specifies which type
  // of GzipSourceStream to create. It must be one of TYPE_GZIP and
  // TYPE_DEFLATE.
  void Init(SourceStream::SourceType type) {
    EXPECT_TRUE(SourceStream::TYPE_GZIP == type ||
                SourceStream::TYPE_DEFLATE == type);
    source_data_len_ = kBigBufferSize - kEOFMargin;

    for (size_t i = 0; i < source_data_len_; i++)
      source_data_[i] = i % 256;

    encoded_data_len_ = kBigBufferSize;
    CompressGzip(source_data_, source_data_len_, encoded_data_,
                 &encoded_data_len_, type != SourceStream::TYPE_DEFLATE);

    output_buffer_ = base::MakeRefCounted<IOBuffer>(output_buffer_size_);
    std::unique_ptr<MockSourceStream> source(new MockSourceStream());
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

  char* source_data() { return source_data_; }
  size_t source_data_len() { return source_data_len_; }

  char* encoded_data() { return encoded_data_; }
  size_t encoded_data_len() { return encoded_data_len_; }

  IOBuffer* output_buffer() { return output_buffer_.get(); }
  char* output_data() { return output_buffer_->data(); }
  size_t output_buffer_size() { return output_buffer_size_; }

  MockSourceStream* source() { return source_; }
  GzipSourceStream* stream() { return stream_.get(); }

  // Reads from |stream_| until an error occurs or the EOF is reached.
  // When an error occurs, returns the net error code. When an EOF is reached,
  // returns the number of bytes read and appends data read to |output|.
  int ReadStream(std::string* output) {
    int bytes_read = 0;
    while (true) {
      TestCompletionCallback callback;
      int rv = stream_->Read(output_buffer(), output_buffer_size(),
                             callback.callback());
      if (rv == ERR_IO_PENDING)
        rv = CompleteReadsIfAsync(rv, &callback, source());
      if (rv == OK)
        break;
      if (rv < OK)
        return rv;
      EXPECT_GT(rv, OK);
      bytes_read += rv;
      output->append(output_data(), rv);
    }
    return bytes_read;
  }

 private:
  char source_data_[kBigBufferSize];
  size_t source_data_len_;

  char encoded_data_[kBigBufferSize];
  size_t encoded_data_len_;

  scoped_refptr<IOBuffer> output_buffer_;
  const int output_buffer_size_;

  MockSourceStream* source_;
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
  Init(SourceStream::TYPE_DEFLATE);
  source()->AddReadResult(nullptr, 0, OK, GetParam().mode);
  TestCompletionCallback callback;
  std::string actual_output;
  int result = ReadStream(&actual_output);
  EXPECT_EQ(OK, result);
  EXPECT_EQ("DEFLATE", stream()->Description());
}

TEST_P(GzipSourceStreamTest, DeflateOneBlock) {
  Init(SourceStream::TYPE_DEFLATE);
  source()->AddReadResult(encoded_data(), encoded_data_len(), OK,
                          GetParam().mode);
  source()->AddReadResult(nullptr, 0, OK, GetParam().mode);
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(static_cast<int>(source_data_len()), rv);
  EXPECT_EQ(std::string(source_data(), source_data_len()), actual_output);
  EXPECT_EQ("DEFLATE", stream()->Description());
}

TEST_P(GzipSourceStreamTest, GzipOneBloc) {
  Init(SourceStream::TYPE_GZIP);
  source()->AddReadResult(encoded_data(), encoded_data_len(), OK,
                          GetParam().mode);
  source()->AddReadResult(nullptr, 0, OK, GetParam().mode);
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(static_cast<int>(source_data_len()), rv);
  EXPECT_EQ(std::string(source_data(), source_data_len()), actual_output);
  EXPECT_EQ("GZIP", stream()->Description());
}

TEST_P(GzipSourceStreamTest, DeflateTwoReads) {
  Init(SourceStream::TYPE_DEFLATE);
  source()->AddReadResult(encoded_data(), 10, OK, GetParam().mode);
  source()->AddReadResult(encoded_data() + 10, encoded_data_len() - 10, OK,
                          GetParam().mode);
  source()->AddReadResult(nullptr, 0, OK, GetParam().mode);
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(static_cast<int>(source_data_len()), rv);
  EXPECT_EQ(std::string(source_data(), source_data_len()), actual_output);
  EXPECT_EQ("DEFLATE", stream()->Description());
}

// Check that any extra bytes after the end of the gzipped data are silently
// ignored.
TEST_P(GzipSourceStreamTest, IgnoreDataAfterEof) {
  Init(SourceStream::TYPE_DEFLATE);
  const char kExtraData[] = "Hello, World!";
  std::string encoded_data_with_trailing_data(encoded_data(),
                                              encoded_data_len());
  encoded_data_with_trailing_data.append(kExtraData, sizeof(kExtraData));
  source()->AddReadResult(encoded_data_with_trailing_data.c_str(),
                          encoded_data_with_trailing_data.length(), OK,
                          GetParam().mode);
  source()->AddReadResult(nullptr, 0, OK, GetParam().mode);
  // Compressed and uncompressed data get returned as separate Read() results,
  // so this test has to call Read twice.
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  std::string expected_output(source_data(), source_data_len());
  EXPECT_EQ(static_cast<int>(expected_output.size()), rv);
  EXPECT_EQ(expected_output, actual_output);
  EXPECT_EQ("DEFLATE", stream()->Description());
}

TEST_P(GzipSourceStreamTest, MissingZlibHeader) {
  Init(SourceStream::TYPE_DEFLATE);
  const size_t kZlibHeaderLen = 2;
  source()->AddReadResult(encoded_data() + kZlibHeaderLen,
                          encoded_data_len() - kZlibHeaderLen, OK,
                          GetParam().mode);
  source()->AddReadResult(nullptr, 0, OK, GetParam().mode);
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(static_cast<int>(source_data_len()), rv);
  EXPECT_EQ(std::string(source_data(), source_data_len()), actual_output);
  EXPECT_EQ("DEFLATE", stream()->Description());
}

TEST_P(GzipSourceStreamTest, CorruptGzipHeader) {
  Init(SourceStream::TYPE_GZIP);
  encoded_data()[1] = 0;
  int read_len = encoded_data_len();
  // Needed to a avoid a DCHECK that all reads were consumed.
  if (GetParam().read_result_type == ReadResultType::ONE_BYTE_AT_A_TIME)
    read_len = 2;
  source()->AddReadResult(encoded_data(), read_len, OK, GetParam().mode);
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(ERR_CONTENT_DECODING_FAILED, rv);
  EXPECT_EQ("GZIP", stream()->Description());
}

// This test checks that the gzip stream source works correctly on 'golden' data
// as produced by gzip(1).
TEST_P(GzipSourceStreamTest, GzipCorrectness) {
  Init(SourceStream::TYPE_GZIP);
  const char kDecompressedData[] = "Hello, World!";
  const unsigned char kGzipData[] = {
      // From:
      //   echo -n 'Hello, World!' | gzip | xxd -i | sed -e 's/^/  /'
      // The footer is the last 8 bytes.
      0x1f, 0x8b, 0x08, 0x00, 0x2b, 0x02, 0x84, 0x55, 0x00, 0x03, 0xf3,
      0x48, 0xcd, 0xc9, 0xc9, 0xd7, 0x51, 0x08, 0xcf, 0x2f, 0xca, 0x49,
      0x51, 0x04, 0x00, 0xd0, 0xc3, 0x4a, 0xec, 0x0d, 0x00, 0x00, 0x00};
  source()->AddReadResult(reinterpret_cast<const char*>(kGzipData),
                          sizeof(kGzipData), OK, GetParam().mode);
  source()->AddReadResult(nullptr, 0, OK, GetParam().mode);
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(static_cast<int>(strlen(kDecompressedData)), rv);
  EXPECT_EQ(kDecompressedData, actual_output);
  EXPECT_EQ("GZIP", stream()->Description());
}

// Same as GzipCorrectness except that last 8 bytes are removed to test that the
// implementation can handle missing footer.
TEST_P(GzipSourceStreamTest, GzipCorrectnessWithoutFooter) {
  Init(SourceStream::TYPE_GZIP);
  const char kDecompressedData[] = "Hello, World!";
  const unsigned char kGzipData[] = {
      // From:
      //   echo -n 'Hello, World!' | gzip | xxd -i | sed -e 's/^/  /'
      // with the 8 footer bytes removed.
      0x1f, 0x8b, 0x08, 0x00, 0x2b, 0x02, 0x84, 0x55, 0x00,
      0x03, 0xf3, 0x48, 0xcd, 0xc9, 0xc9, 0xd7, 0x51, 0x08,
      0xcf, 0x2f, 0xca, 0x49, 0x51, 0x04, 0x00};
  source()->AddReadResult(reinterpret_cast<const char*>(kGzipData),
                          sizeof(kGzipData), OK, GetParam().mode);
  source()->AddReadResult(nullptr, 0, OK, GetParam().mode);
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(static_cast<int>(strlen(kDecompressedData)), rv);
  EXPECT_EQ(kDecompressedData, actual_output);
  EXPECT_EQ("GZIP", stream()->Description());
}

// Test with the same compressed data as the above tests, but uses deflate with
// header and checksum. Tests the Z_STREAM_END case in
// STATE_SNIFFING_DEFLATE_HEADER.
TEST_P(GzipSourceStreamTest, DeflateWithAdler32) {
  Init(SourceStream::TYPE_DEFLATE);
  const char kDecompressedData[] = "Hello, World!";
  const unsigned char kGzipData[] = {0x78, 0x01, 0xf3, 0x48, 0xcd, 0xc9, 0xc9,
                                     0xd7, 0x51, 0x08, 0xcf, 0x2f, 0xca, 0x49,
                                     0x51, 0x04, 0x00, 0x1f, 0x9e, 0x04, 0x6a};
  source()->AddReadResult(reinterpret_cast<const char*>(kGzipData),
                          sizeof(kGzipData), OK, GetParam().mode);
  source()->AddReadResult(nullptr, 0, OK, GetParam().mode);
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(static_cast<int>(strlen(kDecompressedData)), rv);
  EXPECT_EQ(kDecompressedData, actual_output);
  EXPECT_EQ("DEFLATE", stream()->Description());
}

TEST_P(GzipSourceStreamTest, DeflateWithBadAdler32) {
  Init(SourceStream::TYPE_DEFLATE);
  const unsigned char kGzipData[] = {0x78, 0x01, 0xf3, 0x48, 0xcd, 0xc9, 0xc9,
                                     0xd7, 0x51, 0x08, 0xcf, 0x2f, 0xca, 0x49,
                                     0x51, 0x04, 0x00, 0xFF, 0xFF, 0xFF, 0xFF};
  source()->AddReadResult(reinterpret_cast<const char*>(kGzipData),
                          sizeof(kGzipData), OK, GetParam().mode);
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(ERR_CONTENT_DECODING_FAILED, rv);
  EXPECT_EQ("DEFLATE", stream()->Description());
}

TEST_P(GzipSourceStreamTest, DeflateWithoutHeaderWithAdler32) {
  Init(SourceStream::TYPE_DEFLATE);
  const char kDecompressedData[] = "Hello, World!";
  const unsigned char kGzipData[] = {0xf3, 0x48, 0xcd, 0xc9, 0xc9, 0xd7, 0x51,
                                     0x08, 0xcf, 0x2f, 0xca, 0x49, 0x51, 0x04,
                                     0x00, 0x1f, 0x9e, 0x04, 0x6a};
  source()->AddReadResult(reinterpret_cast<const char*>(kGzipData),
                          sizeof(kGzipData), OK, GetParam().mode);
  source()->AddReadResult(nullptr, 0, OK, GetParam().mode);
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(static_cast<int>(strlen(kDecompressedData)), rv);
  EXPECT_EQ(kDecompressedData, actual_output);
  EXPECT_EQ("DEFLATE", stream()->Description());
}

TEST_P(GzipSourceStreamTest, DeflateWithoutHeaderWithBadAdler32) {
  Init(SourceStream::TYPE_DEFLATE);
  const unsigned char kGzipData[] = {0xf3, 0x48, 0xcd, 0xc9, 0xc9, 0xd7, 0x51,
                                     0x08, 0xcf, 0x2f, 0xca, 0x49, 0x51, 0x04,
                                     0x00, 0xFF, 0xFF, 0xFF, 0xFF};
  source()->AddReadResult(reinterpret_cast<const char*>(kGzipData),
                          sizeof(kGzipData), OK, GetParam().mode);
  std::string actual_output;
  int rv = ReadStream(&actual_output);
  EXPECT_EQ(ERR_CONTENT_DECODING_FAILED, rv);
  EXPECT_EQ("DEFLATE", stream()->Description());
}

}  // namespace net
