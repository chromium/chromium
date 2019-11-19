// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bit_cast.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/filter/brotli_source_stream.h"
#include "net/filter/mock_source_stream.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

namespace {

const size_t kDefaultBufferSize = 4096;
const size_t kSmallBufferSize = 128;

}  // namespace

class BrotliSourceStreamTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    // Get the path of data directory.
    base::FilePath data_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &data_dir);
    data_dir = data_dir.AppendASCII("net");
    data_dir = data_dir.AppendASCII("data");
    data_dir = data_dir.AppendASCII("filter_unittests");

    // Read data from the original file into buffer.
    base::FilePath file_path;
    file_path = data_dir.AppendASCII("google.txt");
    ASSERT_TRUE(base::ReadFileToString(file_path, &source_data_));
    ASSERT_GE(kDefaultBufferSize, source_data_.size());

    // Read data from the encoded file into buffer.
    base::FilePath encoded_file_path;
    encoded_file_path = data_dir.AppendASCII("google.br");
    ASSERT_TRUE(base::ReadFileToString(encoded_file_path, &encoded_buffer_));
    ASSERT_GE(kDefaultBufferSize, encoded_buffer_.size());

    std::unique_ptr<MockSourceStream> source(new MockSourceStream);
    source_ = source.get();
    brotli_stream_ = CreateBrotliSourceStream(std::move(source));
  }

  int ReadStream(net::CompletionOnceCallback callback) {
    return brotli_stream_->Read(out_buffer(), out_data_size(),
                                std::move(callback));
  }

  IOBuffer* out_buffer() { return out_buffer_.get(); }
  char* out_data() { return out_buffer_->data(); }
  size_t out_data_size() { return out_buffer_->size(); }

  std::string source_data() { return source_data_; }

  size_t source_data_len() { return source_data_.length(); }

  char* encoded_buffer() { return &encoded_buffer_[0]; }

  size_t encoded_len() { return encoded_buffer_.length(); }

  MockSourceStream* source() { return source_; }
  SourceStream* brotli_stream() { return brotli_stream_.get(); }
  scoped_refptr<IOBufferWithSize> out_buffer_;

 private:
  MockSourceStream* source_;
  std::unique_ptr<SourceStream> brotli_stream_;
  std::unique_ptr<base::RunLoop> loop_;

  std::string source_data_;
  std::string encoded_buffer_;
};

// Basic scenario: decoding brotli data with big enough buffer.
TEST_F(BrotliSourceStreamTest, DecodeBrotliOneBlockSync) {
  source()->AddReadResult(encoded_buffer(), encoded_len(), OK,
                          MockSourceStream::SYNC);
  out_buffer_ = base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  TestCompletionCallback callback;
  int bytes_read = ReadStream(callback.callback());

  EXPECT_EQ(static_cast<int>(source_data_len()), bytes_read);
  EXPECT_EQ(0, memcmp(out_data(), source_data().c_str(), source_data_len()));
  EXPECT_EQ("BROTLI", brotli_stream()->Description());
}

// Regression test for crbug.com/659311. The following example is taken out
// of the bug report. For this specific example, Brotli will consume the first
// byte in the 6 available bytes and return 0.
TEST_F(BrotliSourceStreamTest, IgnoreExtraData) {
  const unsigned char kResponse[] = {0x1A, 0xDF, 0x6E, 0x74, 0x74, 0x68};
  source()->AddReadResult(reinterpret_cast<const char*>(kResponse),
                          sizeof(kResponse), OK, MockSourceStream::SYNC);
  // Add an EOF.
  source()->AddReadResult(reinterpret_cast<const char*>(kResponse), 0, OK,
                          MockSourceStream::SYNC);
  out_buffer_ = base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  std::string actual_output;
  TestCompletionCallback callback;
  int bytes_read = ReadStream(callback.callback());
  EXPECT_EQ(0, bytes_read);
  EXPECT_EQ("BROTLI", brotli_stream()->Description());
}

// If there are data after decoding is done, ignore the data. crbug.com/659311.
TEST_F(BrotliSourceStreamTest, IgnoreExtraDataInOneRead) {
  std::string response_with_extra_data(encoded_buffer(), encoded_len());
  response_with_extra_data.append(1000, 'x');
  source()->AddReadResult(response_with_extra_data.c_str(),
                          response_with_extra_data.length(), OK,
                          MockSourceStream::SYNC);
  // Add an EOF.
  source()->AddReadResult(response_with_extra_data.c_str(), 0, OK,
                          MockSourceStream::SYNC);
  out_buffer_ = base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  std::string actual_output;
  while (true) {
    TestCompletionCallback callback;
    int bytes_read = ReadStream(callback.callback());
    if (bytes_read == OK)
      break;
    ASSERT_GT(bytes_read, OK);
    actual_output.append(out_data(), bytes_read);
  }
  EXPECT_EQ(source_data_len(), actual_output.size());
  EXPECT_EQ(source_data(), actual_output);
  EXPECT_EQ("BROTLI", brotli_stream()->Description());
}

// Same as above but extra data is in a different read.
TEST_F(BrotliSourceStreamTest, IgnoreExtraDataInDifferentRead) {
  std::string extra_data;
  extra_data.append(1000, 'x');
  source()->AddReadResult(encoded_buffer(), encoded_len(), OK,
                          MockSourceStream::SYNC);
  source()->AddReadResult(extra_data.c_str(), extra_data.length(), OK,
                          MockSourceStream::SYNC);
  // Add an EOF.
  source()->AddReadResult(extra_data.c_str(), 0, OK, MockSourceStream::SYNC);
  out_buffer_ = base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  std::string actual_output;
  while (true) {
    TestCompletionCallback callback;
    int bytes_read = ReadStream(callback.callback());
    if (bytes_read == OK)
      break;
    ASSERT_GT(bytes_read, OK);
    actual_output.append(out_data(), bytes_read);
  }
  EXPECT_EQ(source_data_len(), actual_output.size());
  EXPECT_EQ(source_data(), actual_output);
  EXPECT_EQ("BROTLI", brotli_stream()->Description());
}

// Basic scenario: decoding brotli data with big enough buffer.
TEST_F(BrotliSourceStreamTest, DecodeBrotliTwoBlockSync) {
  source()->AddReadResult(encoded_buffer(), 10, OK, MockSourceStream::SYNC);
  source()->AddReadResult(encoded_buffer() + 10, encoded_len() - 10, OK,
                          MockSourceStream::SYNC);
  out_buffer_ = base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  TestCompletionCallback callback;
  int bytes_read = ReadStream(callback.callback());
  EXPECT_EQ(static_cast<int>(source_data_len()), bytes_read);
  EXPECT_EQ(0, memcmp(out_data(), source_data().c_str(), source_data_len()));
  EXPECT_EQ("BROTLI", brotli_stream()->Description());
}

// Basic scenario: decoding brotli data with big enough buffer.
TEST_F(BrotliSourceStreamTest, DecodeBrotliOneBlockAsync) {
  source()->AddReadResult(encoded_buffer(), encoded_len(), OK,
                          MockSourceStream::ASYNC);
  out_buffer_ = base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  TestCompletionCallback callback;
  int bytes_read = ReadStream(callback.callback());

  EXPECT_EQ(ERR_IO_PENDING, bytes_read);
  source()->CompleteNextRead();
  int rv = callback.WaitForResult();
  EXPECT_EQ(static_cast<int>(source_data_len()), rv);
  EXPECT_EQ(0, memcmp(out_data(), source_data().c_str(), source_data_len()));
  EXPECT_EQ("BROTLI", brotli_stream()->Description());
}

// Tests we can call filter repeatedly to get all the data decoded.
// To do that, we create a filter with a small buffer that can not hold all
// the input data.
TEST_F(BrotliSourceStreamTest, DecodeWithSmallBufferSync) {
  source()->AddReadResult(encoded_buffer(), encoded_len(), OK,
                          MockSourceStream::SYNC);
  // Add a 0 byte read to signal EOF.
  source()->AddReadResult(encoded_buffer(), 0, OK, MockSourceStream::SYNC);

  out_buffer_ = base::MakeRefCounted<IOBufferWithSize>(kSmallBufferSize);

  scoped_refptr<IOBuffer> buffer =
      base::MakeRefCounted<IOBufferWithSize>(source_data_len());
  size_t total_bytes_read = 0;
  int bytes_read = 0;
  do {
    TestCompletionCallback callback;
    bytes_read = ReadStream(callback.callback());
    EXPECT_LE(OK, bytes_read);
    EXPECT_GE(kSmallBufferSize, static_cast<size_t>(bytes_read));
    memcpy(buffer->data() + total_bytes_read, out_data(), bytes_read);
    total_bytes_read += bytes_read;
  } while (bytes_read > 0);
  EXPECT_EQ(source_data_len(), total_bytes_read);
  EXPECT_EQ(0, memcmp(buffer->data(), source_data().c_str(), total_bytes_read));
  EXPECT_EQ("BROTLI", brotli_stream()->Description());
}

// Tests we can call filter repeatedly to get all the data decoded.
// To do that, we create a filter with a small buffer that can not hold all
// the input data.
TEST_F(BrotliSourceStreamTest, DecodeWithSmallBufferAsync) {
  source()->AddReadResult(encoded_buffer(), encoded_len(), OK,
                          MockSourceStream::ASYNC);
  // Add a 0 byte read to signal EOF.
  source()->AddReadResult(encoded_buffer(), 0, OK, MockSourceStream::ASYNC);

  out_buffer_ = base::MakeRefCounted<IOBufferWithSize>(kSmallBufferSize);

  scoped_refptr<IOBuffer> buffer =
      base::MakeRefCounted<IOBufferWithSize>(source_data_len());
  size_t total_bytes_read = 0;
  int bytes_read = 0;
  do {
    TestCompletionCallback callback;
    bytes_read = ReadStream(callback.callback());
    if (bytes_read == ERR_IO_PENDING) {
      source()->CompleteNextRead();
      bytes_read = callback.WaitForResult();
    }
    EXPECT_GE(static_cast<int>(kSmallBufferSize), bytes_read);
    memcpy(buffer->data() + total_bytes_read, out_data(), bytes_read);
    total_bytes_read += bytes_read;
  } while (bytes_read > 0);
  EXPECT_EQ(source_data_len(), total_bytes_read);
  EXPECT_EQ(0, memcmp(buffer->data(), source_data().c_str(), total_bytes_read));
  EXPECT_EQ("BROTLI", brotli_stream()->Description());
}

// Tests we can still decode with just 1 byte buffer in the filter.
// The purpose of this test: sometimes the filter will consume input without
// generating output. Verify filter can handle it correctly.
TEST_F(BrotliSourceStreamTest, DecodeWithOneByteBuffer) {
  source()->AddReadResult(encoded_buffer(), encoded_len(), OK,
                          MockSourceStream::SYNC);
  // Add a 0 byte read to signal EOF.
  source()->AddReadResult(encoded_buffer(), 0, OK, MockSourceStream::SYNC);
  out_buffer_ = base::MakeRefCounted<IOBufferWithSize>(1);
  scoped_refptr<IOBuffer> buffer =
      base::MakeRefCounted<IOBufferWithSize>(source_data_len());
  size_t total_bytes_read = 0;
  int bytes_read = 0;
  do {
    TestCompletionCallback callback;
    bytes_read = ReadStream(callback.callback());
    EXPECT_NE(ERR_IO_PENDING, bytes_read);
    EXPECT_GE(1, bytes_read);
    memcpy(buffer->data() + total_bytes_read, out_data(), bytes_read);
    total_bytes_read += bytes_read;
  } while (bytes_read > 0);
  EXPECT_EQ(source_data_len(), total_bytes_read);
  EXPECT_EQ(0,
            memcmp(buffer->data(), source_data().c_str(), source_data_len()));
  EXPECT_EQ("BROTLI", brotli_stream()->Description());
}

// Decoding deflate stream with corrupted data.
TEST_F(BrotliSourceStreamTest, DecodeCorruptedData) {
  char corrupt_data[kDefaultBufferSize];
  int corrupt_data_len = encoded_len();
  memcpy(corrupt_data, encoded_buffer(), encoded_len());
  int pos = corrupt_data_len / 2;
  corrupt_data[pos] = !corrupt_data[pos];

  source()->AddReadResult(corrupt_data, corrupt_data_len, OK,
                          MockSourceStream::SYNC);
  out_buffer_ = base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  int error = OK;
  do {
    TestCompletionCallback callback;
    error = ReadStream(callback.callback());
    EXPECT_NE(ERR_IO_PENDING, error);
  } while (error > 0);
  // Expect failures
  EXPECT_EQ(ERR_CONTENT_DECODING_FAILED, error);

  // Calling Read again gives the same error.
  TestCompletionCallback callback;
  error = ReadStream(callback.callback());
  EXPECT_EQ(ERR_CONTENT_DECODING_FAILED, error);

  EXPECT_EQ("BROTLI", brotli_stream()->Description());
}

// Decoding deflate stream with missing data.
TEST_F(BrotliSourceStreamTest, DecodeMissingData) {
  char corrupt_data[kDefaultBufferSize];
  int corrupt_data_len = encoded_len();
  memcpy(corrupt_data, encoded_buffer(), encoded_len());

  int pos = corrupt_data_len / 2;
  int len = corrupt_data_len - pos - 1;
  memmove(&corrupt_data[pos], &corrupt_data[pos + 1], len);
  --corrupt_data_len;

  // Decode the corrupted data with filter
  source()->AddReadResult(corrupt_data, corrupt_data_len, OK,
                          MockSourceStream::SYNC);
  out_buffer_ = base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  int error = OK;
  do {
    TestCompletionCallback callback;
    error = ReadStream(callback.callback());
    EXPECT_NE(ERR_IO_PENDING, error);
  } while (error > 0);
  // Expect failures
  EXPECT_EQ(ERR_CONTENT_DECODING_FAILED, error);
  EXPECT_EQ("BROTLI", brotli_stream()->Description());
}

// Decoding brotli stream with empty output data.
TEST_F(BrotliSourceStreamTest, DecodeEmptyData) {
  char data[1] = {6};  // WBITS = 16, ISLAST = 1, ISLASTEMPTY = 1
  int data_len = 1;

  source()->AddReadResult(data, data_len, OK, MockSourceStream::SYNC);
  source()->AddReadResult(data, 0, OK, MockSourceStream::SYNC);
  out_buffer_ = base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  TestCompletionCallback callback;
  int bytes_read = ReadStream(callback.callback());
  EXPECT_EQ(OK, bytes_read);
  EXPECT_EQ("BROTLI", brotli_stream()->Description());
}

}  // namespace net
