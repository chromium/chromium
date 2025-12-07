// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/filter/brotli_source_stream.h"

#include <stdint.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_view_util.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/filter/mock_source_stream.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

namespace {

const size_t kDefaultBufferSize = 4096;
const size_t kSmallBufferSize = 128;

// Get the path of data directory.
base::FilePath GetTestDataDir() {
  base::FilePath data_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &data_dir);
  data_dir = data_dir.AppendASCII("net");
  data_dir = data_dir.AppendASCII("data");
  data_dir = data_dir.AppendASCII("filter_unittests");
  return data_dir;
}

}  // namespace

class BrotliSourceStreamTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    // Get the path of data directory.
    base::FilePath data_dir = GetTestDataDir();

    // Read data from the original file into buffer.
    base::FilePath file_path = data_dir.AppendASCII("google.txt");
    ASSERT_TRUE(base::ReadFileToString(file_path, &source_data_));
    ASSERT_GE(kDefaultBufferSize, source_data_.size());

    // Read data from the encoded file into buffer.
    base::FilePath encoded_file_path = data_dir.AppendASCII("google.br");
    ASSERT_TRUE(base::ReadFileToString(encoded_file_path, &encoded_buffer_));
    ASSERT_GE(kDefaultBufferSize, encoded_buffer_.size());

    auto source = std::make_unique<MockSourceStream>();
    source_ = source.get();
    brotli_stream_ = CreateBrotliSourceStream(std::move(source));
    out_buffer_ = base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  }

  int ReadStream(net::CompletionOnceCallback callback) {
    return brotli_stream_->Read(out_buffer_.get(), out_buffer_->size(),
                                std::move(callback));
  }

  base::span<const uint8_t> out_span() const { return out_buffer_->span(); }

  std::string source_data() { return source_data_; }

  size_t source_data_len() { return source_data_.length(); }

  std::string_view encoded_view() { return encoded_buffer_; }

  MockSourceStream* source() { return source_; }
  SourceStream* brotli_stream() { return brotli_stream_.get(); }

  const scoped_refptr<IOBufferWithSize>& out_buffer() { return out_buffer_; }
  void set_out_buffer(scoped_refptr<IOBufferWithSize> buffer) {
    out_buffer_ = std::move(buffer);
  }

 private:
  raw_ptr<MockSourceStream, DanglingUntriaged> source_;
  std::unique_ptr<SourceStream> brotli_stream_;
  std::unique_ptr<base::RunLoop> loop_;

  std::string source_data_;
  std::string encoded_buffer_;

  scoped_refptr<IOBufferWithSize> out_buffer_;
};

// Basic scenario: decoding brotli data with big enough buffer.
TEST_F(BrotliSourceStreamTest, DecodeBrotliOneBlockSync) {
  source()->AddReadResult(encoded_view(), OK, MockSourceStream::SYNC);
  TestCompletionCallback callback;
  int bytes_read = ReadStream(callback.callback());

  EXPECT_EQ(static_cast<int>(source_data_len()), bytes_read);
  EXPECT_EQ(source_data(),
            base::as_string_view(out_span().first(source_data_len())));
  EXPECT_EQ("BROTLI", brotli_stream()->Description());
}

// Regression test for crbug.com/659311. The following example is taken out
// of the bug report. For this specific example, Brotli will consume the first
// byte in the 6 available bytes and return 0.
TEST_F(BrotliSourceStreamTest, IgnoreExtraData) {
  const unsigned char kResponse[] = {0x1A, 0xDF, 0x6E, 0x74, 0x74, 0x68};
  source()->AddReadResult(base::as_string_view(kResponse), OK,
                          MockSourceStream::SYNC);
  // Add an EOF.
  source()->AddReadResult(std::string_view(), OK, MockSourceStream::SYNC);
  std::string actual_output;
  TestCompletionCallback callback;
  int bytes_read = ReadStream(callback.callback());
  EXPECT_EQ(0, bytes_read);
  EXPECT_EQ("BROTLI", brotli_stream()->Description());
}

// If there are data after decoding is done, ignore the data. crbug.com/659311.
TEST_F(BrotliSourceStreamTest, IgnoreExtraDataInOneRead) {
  std::string response_with_extra_data(encoded_view());
  response_with_extra_data.append(1000, 'x');
  source()->AddReadResult(response_with_extra_data, OK, MockSourceStream::SYNC);
  // Add an EOF.
  source()->AddReadResult(std::string_view(), OK, MockSourceStream::SYNC);
  std::string actual_output;
  while (true) {
    TestCompletionCallback callback;
    int bytes_read = ReadStream(callback.callback());
    if (bytes_read == OK)
      break;
    ASSERT_GT(bytes_read, OK);
    actual_output.append(base::as_string_view(
        out_span().first(static_cast<size_t>(bytes_read))));
  }
  EXPECT_EQ(source_data_len(), actual_output.size());
  EXPECT_EQ(source_data(), actual_output);
  EXPECT_EQ("BROTLI", brotli_stream()->Description());
}

// Same as above but extra data is in a different read.
TEST_F(BrotliSourceStreamTest, IgnoreExtraDataInDifferentRead) {
  std::string extra_data;
  extra_data.append(1000, 'x');
  source()->AddReadResult(encoded_view(), OK, MockSourceStream::SYNC);
  source()->AddReadResult(extra_data, OK, MockSourceStream::SYNC);
  // Add an EOF.
  source()->AddReadResult(std::string_view(), OK, MockSourceStream::SYNC);
  std::string actual_output;
  while (true) {
    TestCompletionCallback callback;
    int bytes_read = ReadStream(callback.callback());
    if (bytes_read == OK)
      break;
    ASSERT_GT(bytes_read, OK);
    actual_output.append(base::as_string_view(
        out_span().first(static_cast<size_t>(bytes_read))));
  }
  EXPECT_EQ(source_data_len(), actual_output.size());
  EXPECT_EQ(source_data(), actual_output);
  EXPECT_EQ("BROTLI", brotli_stream()->Description());
}

// Basic scenario: decoding brotli data with big enough buffer.
TEST_F(BrotliSourceStreamTest, DecodeBrotliTwoBlockSync) {
  source()->AddReadResult(encoded_view().substr(0, 10), OK,
                          MockSourceStream::SYNC);
  source()->AddReadResult(encoded_view().substr(10), OK,
                          MockSourceStream::SYNC);
  TestCompletionCallback callback;
  int bytes_read = ReadStream(callback.callback());
  EXPECT_EQ(static_cast<int>(source_data_len()), bytes_read);
  EXPECT_EQ(source_data(),
            base::as_string_view(out_span().first(source_data_len())));
  EXPECT_EQ("BROTLI", brotli_stream()->Description());
}

// Basic scenario: decoding brotli data with big enough buffer.
TEST_F(BrotliSourceStreamTest, DecodeBrotliOneBlockAsync) {
  source()->AddReadResult(encoded_view(), OK, MockSourceStream::ASYNC);
  TestCompletionCallback callback;
  int bytes_read = ReadStream(callback.callback());

  EXPECT_EQ(ERR_IO_PENDING, bytes_read);
  source()->CompleteNextRead();
  int rv = callback.WaitForResult();
  EXPECT_EQ(static_cast<int>(source_data_len()), rv);
  EXPECT_EQ(source_data(),
            base::as_string_view(out_span().first(source_data_len())));
  EXPECT_EQ("BROTLI", brotli_stream()->Description());
}

// Tests we can call filter repeatedly to get all the data decoded.
// To do that, we create a filter with a small buffer that can not hold all
// the input data.
TEST_F(BrotliSourceStreamTest, DecodeWithSmallBufferSync) {
  source()->AddReadResult(encoded_view(), OK, MockSourceStream::SYNC);
  // Add a 0 byte read to signal EOF.
  source()->AddReadResult(std::string_view(), OK, MockSourceStream::SYNC);

  set_out_buffer(base::MakeRefCounted<IOBufferWithSize>(kSmallBufferSize));

  std::string decoded_result;
  decoded_result.reserve(source_data_len());
  int bytes_read = 0;
  do {
    TestCompletionCallback callback;
    bytes_read = ReadStream(callback.callback());
    EXPECT_LE(OK, bytes_read);
    EXPECT_GE(kSmallBufferSize, static_cast<size_t>(bytes_read));
    decoded_result.append(base::as_string_view(
        out_span().first(static_cast<size_t>(bytes_read))));
  } while (bytes_read > 0);
  EXPECT_EQ(source_data_len(), decoded_result.size());
  EXPECT_EQ(source_data(), decoded_result);
  EXPECT_EQ("BROTLI", brotli_stream()->Description());
}

// Tests we can call filter repeatedly to get all the data decoded.
// To do that, we create a filter with a small buffer that can not hold all
// the input data.
TEST_F(BrotliSourceStreamTest, DecodeWithSmallBufferAsync) {
  source()->AddReadResult(encoded_view(), OK, MockSourceStream::ASYNC);
  // Add a 0 byte read to signal EOF.
  source()->AddReadResult(std::string_view(), OK, MockSourceStream::ASYNC);

  set_out_buffer(base::MakeRefCounted<IOBufferWithSize>(kSmallBufferSize));

  std::string decoded_result;
  decoded_result.reserve(source_data_len());
  int bytes_read = 0;
  do {
    TestCompletionCallback callback;
    bytes_read = ReadStream(callback.callback());
    if (bytes_read == ERR_IO_PENDING) {
      source()->CompleteNextRead();
      bytes_read = callback.WaitForResult();
    }
    EXPECT_GE(static_cast<int>(kSmallBufferSize), bytes_read);
    decoded_result.append(base::as_string_view(
        out_span().first(static_cast<size_t>(bytes_read))));
  } while (bytes_read > 0);
  EXPECT_EQ(source_data_len(), decoded_result.size());
  EXPECT_EQ(source_data(), decoded_result);
  EXPECT_EQ("BROTLI", brotli_stream()->Description());
}

// Tests we can still decode with just 1 byte buffer in the filter.
// The purpose of this test: sometimes the filter will consume input without
// generating output. Verify filter can handle it correctly.
TEST_F(BrotliSourceStreamTest, DecodeWithOneByteBuffer) {
  source()->AddReadResult(encoded_view(), OK, MockSourceStream::SYNC);
  // Add a 0 byte read to signal EOF.
  source()->AddReadResult(std::string_view(), OK, MockSourceStream::SYNC);
  set_out_buffer(base::MakeRefCounted<IOBufferWithSize>(1));
  std::string decoded_result;
  decoded_result.reserve(source_data_len());
  int bytes_read = 0;
  do {
    TestCompletionCallback callback;
    bytes_read = ReadStream(callback.callback());
    EXPECT_NE(ERR_IO_PENDING, bytes_read);
    EXPECT_GE(bytes_read, 0);
    EXPECT_LE(bytes_read, 1);
    decoded_result.append(base::as_string_view(
        out_span().first(static_cast<size_t>(bytes_read))));
  } while (bytes_read > 0);
  EXPECT_EQ(source_data_len(), decoded_result.size());
  EXPECT_EQ(source_data(), decoded_result);
  EXPECT_EQ("BROTLI", brotli_stream()->Description());
}

// Decoding deflate stream with corrupted data.
TEST_F(BrotliSourceStreamTest, DecodeCorruptedData) {
  std::vector<char> corrupt_data(encoded_view().begin(), encoded_view().end());
  size_t pos = corrupt_data.size() / 2;
  corrupt_data.at(pos) = !corrupt_data.at(pos);

  source()->AddReadResult(base::as_string_view(corrupt_data), OK,
                          MockSourceStream::SYNC);
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
  std::vector<char> corrupt_data(encoded_view().begin(), encoded_view().end());

  size_t pos = corrupt_data.size() / 2;
  CHECK_LT(pos, corrupt_data.size());
  corrupt_data.erase(corrupt_data.begin() + pos);

  // Decode the corrupted data with filter
  source()->AddReadResult(base::as_string_view(corrupt_data), OK,
                          MockSourceStream::SYNC);
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

  source()->AddReadResult(base::as_string_view(data), OK,
                          MockSourceStream::SYNC);
  source()->AddReadResult(std::string_view(), OK, MockSourceStream::SYNC);
  TestCompletionCallback callback;
  int bytes_read = ReadStream(callback.callback());
  EXPECT_EQ(OK, bytes_read);
  EXPECT_EQ("BROTLI", brotli_stream()->Description());
}

TEST_F(BrotliSourceStreamTest, WithDictionary) {
  std::string encoded_buffer;
  std::string dictionary_data;

  base::FilePath data_dir = GetTestDataDir();
  // Read data from the encoded file into buffer.
  base::FilePath encoded_file_path = data_dir.AppendASCII("google.sbr");
  ASSERT_TRUE(base::ReadFileToString(encoded_file_path, &encoded_buffer));

  // Read data from the dictionary file into buffer.
  base::FilePath dictionary_file_path = data_dir.AppendASCII("test.dict");
  ASSERT_TRUE(base::ReadFileToString(dictionary_file_path, &dictionary_data));

  scoped_refptr<net::IOBuffer> dictionary_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(dictionary_data);

  auto source = std::make_unique<MockSourceStream>();
  source->AddReadResult(encoded_buffer, OK, MockSourceStream::SYNC);

  std::unique_ptr<SourceStream> brotli_stream =
      CreateBrotliSourceStreamWithDictionary(
          std::move(source), dictionary_buffer, dictionary_data.size());

  TestCompletionCallback callback;
  int bytes_read = brotli_stream->Read(out_buffer().get(), kDefaultBufferSize,
                                       callback.callback());

  EXPECT_EQ(static_cast<int>(source_data_len()), bytes_read);
  EXPECT_EQ(base::as_string_view(out_buffer()->first(source_data_len())),
            std::string_view(source_data()));
  EXPECT_EQ("BROTLI", brotli_stream->Description());
}

}  // namespace net
