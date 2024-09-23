// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/base/chunked_upload_data_stream.h"

#include <memory>
#include <string>

#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/base/upload_data_stream.h"
#include "net/log/net_log_with_source.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

constexpr char kTestData[] = "0123456789";
constexpr size_t kTestDataSize = std::size(kTestData) - 1;
constexpr size_t kTestBufferSize = 1 << 14;  // 16KB.

}  // namespace

// Reads data once from the upload data stream, and returns the data as string.
// Expects the read to succeed synchronously.
std::string ReadSync(UploadDataStream* stream, int buffer_size) {
  auto buf = base::MakeRefCounted<IOBufferWithSize>(buffer_size);
  int result = stream->Read(buf.get(),
                            buffer_size,
                            TestCompletionCallback().callback());
  EXPECT_GE(result, 0);
  return std::string(buf->data(), result);
}

// Check the case data is added after the first read attempt.
TEST(ChunkedUploadDataStreamTest, AppendOnce) {
  ChunkedUploadDataStream stream(0);

  ASSERT_THAT(
      stream.Init(TestCompletionCallback().callback(), NetLogWithSource()),
      IsOk());
  EXPECT_FALSE(stream.IsInMemory());
  EXPECT_EQ(0u, stream.size());  // Content-Length is 0 for chunked data.
  EXPECT_EQ(0u, stream.position());
  EXPECT_FALSE(stream.IsEOF());

  TestCompletionCallback callback;
  auto buf = base::MakeRefCounted<IOBufferWithSize>(kTestBufferSize);
  int result = stream.Read(buf.get(), kTestBufferSize, callback.callback());
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));

  stream.AppendData(base::byte_span_from_cstring(kTestData), true);
  int read = callback.WaitForResult();
  ASSERT_GE(read, 0);
  EXPECT_EQ(kTestData, std::string(buf->data(), read));
  EXPECT_EQ(0u, stream.size());  // Content-Length is 0 for chunked data.
  EXPECT_EQ(kTestDataSize, stream.position());
  EXPECT_TRUE(stream.IsEOF());
}

TEST(ChunkedUploadDataStreamTest, AppendOnceBeforeRead) {
  ChunkedUploadDataStream stream(0);

  ASSERT_THAT(
      stream.Init(TestCompletionCallback().callback(), NetLogWithSource()),
      IsOk());
  EXPECT_FALSE(stream.IsInMemory());
  EXPECT_EQ(0u, stream.size());  // Content-Length is 0 for chunked data.
  EXPECT_EQ(0u, stream.position());
  EXPECT_FALSE(stream.IsEOF());

  stream.AppendData(base::byte_span_from_cstring(kTestData), true);
  EXPECT_EQ(0u, stream.size());  // Content-Length is 0 for chunked data.
  EXPECT_EQ(0u, stream.position());
  EXPECT_FALSE(stream.IsEOF());

  std::string data = ReadSync(&stream, kTestBufferSize);
  EXPECT_EQ(kTestData, data);
  EXPECT_EQ(0u, stream.size());  // Content-Length is 0 for chunked data.
  EXPECT_EQ(kTestDataSize, stream.position());
  EXPECT_TRUE(stream.IsEOF());
}

TEST(ChunkedUploadDataStreamTest, AppendOnceBeforeInit) {
  ChunkedUploadDataStream stream(0);

  stream.AppendData(base::byte_span_from_cstring(kTestData), true);
  ASSERT_THAT(
      stream.Init(TestCompletionCallback().callback(), NetLogWithSource()),
      IsOk());
  EXPECT_FALSE(stream.IsInMemory());
  EXPECT_EQ(0u, stream.size());  // Content-Length is 0 for chunked data.
  EXPECT_EQ(0u, stream.position());
  EXPECT_FALSE(stream.IsEOF());

  std::string data = ReadSync(&stream, kTestBufferSize);
  EXPECT_EQ(kTestData, data);
  EXPECT_EQ(0u, stream.size());  // Content-Length is 0 for chunked data.
  EXPECT_EQ(kTestDataSize, stream.position());
  EXPECT_TRUE(stream.IsEOF());
}

TEST(ChunkedUploadDataStreamTest, MultipleAppends) {
  ChunkedUploadDataStream stream(0);

  ASSERT_THAT(
      stream.Init(TestCompletionCallback().callback(), NetLogWithSource()),
      IsOk());
  EXPECT_FALSE(stream.IsInMemory());
  EXPECT_EQ(0u, stream.size());
  EXPECT_EQ(0u, stream.position());
  EXPECT_FALSE(stream.IsEOF());

  TestCompletionCallback callback;
  auto buf = base::MakeRefCounted<IOBufferWithSize>(kTestBufferSize);
  for (size_t i = 0; i < kTestDataSize; ++i) {
    EXPECT_EQ(0u, stream.size());  // Content-Length is 0 for chunked data.
    EXPECT_EQ(i, stream.position());
    ASSERT_FALSE(stream.IsEOF());
    int bytes_read = stream.Read(buf.get(),
                                 kTestBufferSize,
                                 callback.callback());
    ASSERT_THAT(bytes_read, IsError(ERR_IO_PENDING));
    stream.AppendData(base::byte_span_from_cstring(kTestData).subspan(i, 1u),
                      i == kTestDataSize - 1);
    ASSERT_EQ(1, callback.WaitForResult());
    EXPECT_EQ(kTestData[i], buf->data()[0]);
  }

  EXPECT_EQ(0u, stream.size());  // Content-Length is 0 for chunked data.
  EXPECT_EQ(kTestDataSize, stream.position());
  ASSERT_TRUE(stream.IsEOF());
}

TEST(ChunkedUploadDataStreamTest, MultipleAppendsBetweenReads) {
  ChunkedUploadDataStream stream(0);

  ASSERT_THAT(
      stream.Init(TestCompletionCallback().callback(), NetLogWithSource()),
      IsOk());
  EXPECT_FALSE(stream.IsInMemory());
  EXPECT_EQ(0u, stream.size());  // Content-Length is 0 for chunked data.
  EXPECT_EQ(0u, stream.position());
  EXPECT_FALSE(stream.IsEOF());

  auto buf = base::MakeRefCounted<IOBufferWithSize>(kTestBufferSize);
  for (size_t i = 0; i < kTestDataSize; ++i) {
    EXPECT_EQ(i, stream.position());
    ASSERT_FALSE(stream.IsEOF());
    stream.AppendData(base::byte_span_from_cstring(kTestData).subspan(i, 1u),
                      i == kTestDataSize - 1);
    int bytes_read = stream.Read(buf.get(),
                                 kTestBufferSize,
                                 TestCompletionCallback().callback());
    ASSERT_EQ(1, bytes_read);
    EXPECT_EQ(kTestData[i], buf->data()[0]);
  }

  EXPECT_EQ(kTestDataSize, stream.position());
  ASSERT_TRUE(stream.IsEOF());
}

// Checks that multiple reads can be merged.
TEST(ChunkedUploadDataStreamTest, MultipleAppendsBeforeInit) {
  ChunkedUploadDataStream stream(0);
  stream.AppendData(base::byte_span_from_cstring(kTestData).first(1u), false);
  stream.AppendData(base::byte_span_from_cstring(kTestData).subspan(1u, 1u),
                    false);
  stream.AppendData(base::byte_span_from_cstring(kTestData).subspan(2u), true);

  ASSERT_THAT(
      stream.Init(TestCompletionCallback().callback(), NetLogWithSource()),
      IsOk());
  EXPECT_FALSE(stream.IsInMemory());
  EXPECT_EQ(0u, stream.size());  // Content-Length is 0 for chunked data.
  EXPECT_EQ(0u, stream.position());
  EXPECT_FALSE(stream.IsEOF());

  std::string data = ReadSync(&stream, kTestBufferSize);
  EXPECT_EQ(kTestData, data);
  EXPECT_EQ(kTestDataSize, stream.position());
  ASSERT_TRUE(stream.IsEOF());
}

TEST(ChunkedUploadDataStreamTest, MultipleReads) {
  // Use a read size different from the write size to test bounds checking.
  const size_t kReadSize = kTestDataSize + 3;

  ChunkedUploadDataStream stream(0);
  stream.AppendData(base::byte_span_from_cstring(kTestData), false);
  stream.AppendData(base::byte_span_from_cstring(kTestData), false);
  stream.AppendData(base::byte_span_from_cstring(kTestData), false);
  stream.AppendData(base::byte_span_from_cstring(kTestData), true);

  ASSERT_THAT(
      stream.Init(TestCompletionCallback().callback(), NetLogWithSource()),
      IsOk());
  EXPECT_FALSE(stream.IsInMemory());
  EXPECT_EQ(0u, stream.size());  // Content-Length is 0 for chunked data.
  EXPECT_EQ(0u, stream.position());
  EXPECT_FALSE(stream.IsEOF());

  std::string data = ReadSync(&stream, kReadSize);
  EXPECT_EQ("0123456789012", data);
  EXPECT_EQ(kReadSize, stream.position());
  EXPECT_FALSE(stream.IsEOF());

  data = ReadSync(&stream, kReadSize);
  EXPECT_EQ("3456789012345", data);
  EXPECT_EQ(2 * kReadSize, stream.position());
  EXPECT_FALSE(stream.IsEOF());

  data = ReadSync(&stream, kReadSize);
  EXPECT_EQ("6789012345678", data);
  EXPECT_EQ(3 * kReadSize, stream.position());
  EXPECT_FALSE(stream.IsEOF());

  data = ReadSync(&stream, kReadSize);
  EXPECT_EQ("9", data);
  EXPECT_EQ(4 * kTestDataSize, stream.position());
  EXPECT_TRUE(stream.IsEOF());
}

TEST(ChunkedUploadDataStreamTest, EmptyUpload) {
  ChunkedUploadDataStream stream(0);

  ASSERT_THAT(
      stream.Init(TestCompletionCallback().callback(), NetLogWithSource()),
      IsOk());
  EXPECT_FALSE(stream.IsInMemory());
  EXPECT_EQ(0u, stream.size());  // Content-Length is 0 for chunked data.
  EXPECT_EQ(0u, stream.position());
  EXPECT_FALSE(stream.IsEOF());

  TestCompletionCallback callback;
  auto buf = base::MakeRefCounted<IOBufferWithSize>(kTestBufferSize);
  int result = stream.Read(buf.get(), kTestBufferSize, callback.callback());
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));

  stream.AppendData({}, true);
  int read = callback.WaitForResult();
  EXPECT_EQ(0, read);
  EXPECT_EQ(0u, stream.position());
  EXPECT_TRUE(stream.IsEOF());
}

TEST(ChunkedUploadDataStreamTest, EmptyUploadEndedBeforeInit) {
  ChunkedUploadDataStream stream(0);
  stream.AppendData({}, true);

  ASSERT_THAT(
      stream.Init(TestCompletionCallback().callback(), NetLogWithSource()),
      IsOk());
  EXPECT_FALSE(stream.IsInMemory());
  EXPECT_EQ(0u, stream.size());  // Content-Length is 0 for chunked data.
  EXPECT_EQ(0u, stream.position());
  EXPECT_FALSE(stream.IsEOF());

  std::string data = ReadSync(&stream, kTestBufferSize);
  ASSERT_EQ("", data);
  EXPECT_EQ(0u, stream.position());
  EXPECT_TRUE(stream.IsEOF());
}

TEST(ChunkedUploadDataStreamTest, RewindAfterComplete) {
  ChunkedUploadDataStream stream(0);
  stream.AppendData(base::byte_span_from_cstring(kTestData).first(1u), false);
  stream.AppendData(base::byte_span_from_cstring(kTestData).subspan(1u), true);

  ASSERT_THAT(
      stream.Init(TestCompletionCallback().callback(), NetLogWithSource()),
      IsOk());
  EXPECT_FALSE(stream.IsInMemory());
  EXPECT_EQ(0u, stream.size());  // Content-Length is 0 for chunked data.
  EXPECT_EQ(0u, stream.position());
  EXPECT_FALSE(stream.IsEOF());

  std::string data = ReadSync(&stream, kTestBufferSize);
  EXPECT_EQ(kTestData, data);
  EXPECT_EQ(kTestDataSize, stream.position());
  ASSERT_TRUE(stream.IsEOF());

  // Rewind stream and repeat.
  ASSERT_THAT(
      stream.Init(TestCompletionCallback().callback(), NetLogWithSource()),
      IsOk());
  EXPECT_FALSE(stream.IsInMemory());
  EXPECT_EQ(0u, stream.size());  // Content-Length is 0 for chunked data.
  EXPECT_EQ(0u, stream.position());
  EXPECT_FALSE(stream.IsEOF());

  data = ReadSync(&stream, kTestBufferSize);
  EXPECT_EQ(kTestData, data);
  EXPECT_EQ(kTestDataSize, stream.position());
  ASSERT_TRUE(stream.IsEOF());
}

TEST(ChunkedUploadDataStreamTest, RewindWhileReading) {
  ChunkedUploadDataStream stream(0);

  ASSERT_THAT(
      stream.Init(TestCompletionCallback().callback(), NetLogWithSource()),
      IsOk());
  EXPECT_FALSE(stream.IsInMemory());
  EXPECT_EQ(0u, stream.size());  // Content-Length is 0 for chunked data.
  EXPECT_EQ(0u, stream.position());
  EXPECT_FALSE(stream.IsEOF());

  TestCompletionCallback callback;
  auto buf = base::MakeRefCounted<IOBufferWithSize>(kTestBufferSize);
  int result = stream.Read(buf.get(), kTestBufferSize, callback.callback());
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));

  ASSERT_THAT(
      stream.Init(TestCompletionCallback().callback(), NetLogWithSource()),
      IsOk());
  EXPECT_FALSE(stream.IsInMemory());
  EXPECT_EQ(0u, stream.size());  // Content-Length is 0 for chunked data.
  EXPECT_EQ(0u, stream.position());
  EXPECT_FALSE(stream.IsEOF());

  // Adding data now should not result in calling the original read callback,
  // since the stream was re-initialized for reuse, which cancels all pending
  // reads.
  stream.AppendData(base::byte_span_from_cstring(kTestData), true);
  EXPECT_FALSE(callback.have_result());

  std::string data = ReadSync(&stream, kTestBufferSize);
  EXPECT_EQ(kTestData, data);
  EXPECT_EQ(kTestDataSize, stream.position());
  ASSERT_TRUE(stream.IsEOF());
  EXPECT_FALSE(callback.have_result());
}

// Check the behavior of ChunkedUploadDataStream::Writer.
TEST(ChunkedUploadDataStreamTest, ChunkedUploadDataStreamWriter) {
  auto stream = std::make_unique<ChunkedUploadDataStream>(0);
  std::unique_ptr<ChunkedUploadDataStream::Writer> writer(
      stream->CreateWriter());

  // Write before Init.
  ASSERT_TRUE(writer->AppendData(
      base::byte_span_from_cstring(kTestData).first(1u), false));
  ASSERT_THAT(
      stream->Init(TestCompletionCallback().callback(), NetLogWithSource()),
      IsOk());

  // Write after Init.
  ASSERT_TRUE(writer->AppendData(
      base::byte_span_from_cstring(kTestData).subspan(1u), false));

  TestCompletionCallback callback;
  std::string data = ReadSync(stream.get(), kTestBufferSize);
  EXPECT_EQ(kTestData, data);

  // Writing data should gracefully fail if the stream is deleted while still
  // appending data to it.
  stream.reset();
  EXPECT_FALSE(
      writer->AppendData(base::byte_span_from_cstring(kTestData), true));
}

}  // namespace net
