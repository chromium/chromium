// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/http/http_stream_parser.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "net/base/chunked_upload_data_stream.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/base/upload_file_element_reader.h"
#include "net/http/http_connection_info.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/stream_socket.h"
#include "net/test/gtest_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

const size_t kOutputSize = 1024;  // Just large enough for this test.
// The number of bytes that can fit in a buffer of kOutputSize.
const size_t kMaxPayloadSize =
    kOutputSize - HttpStreamParser::kChunkHeaderFooterSize;

// Helper method to create a connected ClientSocketHandle using |data|.
// Modifies |data|.
std::unique_ptr<StreamSocket> CreateConnectedSocket(SequencedSocketData* data) {
  data->set_connect_data(MockConnect(SYNCHRONOUS, OK));

  auto socket =
      std::make_unique<MockTCPClientSocket>(AddressList(), nullptr, data);

  TestCompletionCallback callback;
  EXPECT_THAT(socket->Connect(callback.callback()), IsOk());

  return socket;
}

class ReadErrorUploadDataStream : public UploadDataStream {
 public:
  enum class FailureMode { SYNC, ASYNC };

  explicit ReadErrorUploadDataStream(FailureMode mode)
      : UploadDataStream(true, 0), async_(mode) {}

  ReadErrorUploadDataStream(const ReadErrorUploadDataStream&) = delete;
  ReadErrorUploadDataStream& operator=(const ReadErrorUploadDataStream&) =
      delete;

 private:
  void CompleteRead() { UploadDataStream::OnReadCompleted(ERR_FAILED); }

  // UploadDataStream implementation:
  int InitInternal(const NetLogWithSource& net_log) override { return OK; }

  int ReadInternal(IOBuffer* buf, int buf_len) override {
    if (async_ == FailureMode::ASYNC) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&ReadErrorUploadDataStream::CompleteRead,
                                    weak_factory_.GetWeakPtr()));
      return ERR_IO_PENDING;
    }
    return ERR_FAILED;
  }

  void ResetInternal() override {}

  const FailureMode async_;

  base::WeakPtrFactory<ReadErrorUploadDataStream> weak_factory_{this};
};

TEST(HttpStreamParser, DataReadErrorSynchronous) {
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, 0, "POST / HTTP/1.1\r\n"),
      MockWrite(SYNCHRONOUS, 1, "Content-Length: 12\r\n\r\n"),
  };

  SequencedSocketData data(base::span<MockRead>(), writes);
  std::unique_ptr<StreamSocket> stream_socket = CreateConnectedSocket(&data);

  ReadErrorUploadDataStream upload_data_stream(
      ReadErrorUploadDataStream::FailureMode::SYNC);

  // Test upload progress before init.
  UploadProgress progress = upload_data_stream.GetUploadProgress();
  EXPECT_EQ(0u, progress.size());
  EXPECT_EQ(0u, progress.position());

  ASSERT_THAT(upload_data_stream.Init(TestCompletionCallback().callback(),
                                      NetLogWithSource()),
              IsOk());

  // Test upload progress after init.
  progress = upload_data_stream.GetUploadProgress();
  EXPECT_EQ(0u, progress.size());
  EXPECT_EQ(0u, progress.position());

  scoped_refptr<GrowableIOBuffer> read_buffer =
      base::MakeRefCounted<GrowableIOBuffer>();
  HttpStreamParser parser(stream_socket.get(), false /* is_reused */,
                          GURL("http://localhost"), "POST", &upload_data_stream,
                          read_buffer.get(), NetLogWithSource());

  HttpRequestHeaders headers;
  headers.SetHeader("Content-Length", "12");

  HttpResponseInfo response;
  TestCompletionCallback callback;
  int result = parser.SendRequest("POST / HTTP/1.1\r\n", headers,
                                  TRAFFIC_ANNOTATION_FOR_TESTS, &response,
                                  callback.callback());
  EXPECT_THAT(callback.GetResult(result), IsError(ERR_FAILED));

  progress = upload_data_stream.GetUploadProgress();
  EXPECT_EQ(0u, progress.size());
  EXPECT_EQ(0u, progress.position());

  EXPECT_EQ(CountWriteBytes(writes), parser.sent_bytes());
}

TEST(HttpStreamParser, DataReadErrorAsynchronous) {
  base::test::TaskEnvironment task_environment;

  MockWrite writes[] = {
      MockWrite(ASYNC, 0, "POST / HTTP/1.1\r\n"),
      MockWrite(ASYNC, 1, "Content-Length: 12\r\n\r\n"),
  };

  SequencedSocketData data(base::span<MockRead>(), writes);
  std::unique_ptr<StreamSocket> stream_socket = CreateConnectedSocket(&data);

  ReadErrorUploadDataStream upload_data_stream(
      ReadErrorUploadDataStream::FailureMode::ASYNC);
  ASSERT_THAT(upload_data_stream.Init(TestCompletionCallback().callback(),
                                      NetLogWithSource()),
              IsOk());

  HttpRequestInfo request;

  scoped_refptr<GrowableIOBuffer> read_buffer =
      base::MakeRefCounted<GrowableIOBuffer>();
  HttpStreamParser parser(stream_socket.get(), false /* is_reused */,
                          GURL("http://localhost"), "POST", &upload_data_stream,
                          read_buffer.get(), NetLogWithSource());

  HttpRequestHeaders headers;
  headers.SetHeader("Content-Length", "12");

  HttpResponseInfo response;
  TestCompletionCallback callback;
  int result = parser.SendRequest("POST / HTTP/1.1\r\n", headers,
                                  TRAFFIC_ANNOTATION_FOR_TESTS, &response,
                                  callback.callback());
  EXPECT_THAT(result, IsError(ERR_IO_PENDING));

  UploadProgress progress = upload_data_stream.GetUploadProgress();
  EXPECT_EQ(0u, progress.size());
  EXPECT_EQ(0u, progress.position());

  EXPECT_THAT(callback.GetResult(result), IsError(ERR_FAILED));

  EXPECT_EQ(CountWriteBytes(writes), parser.sent_bytes());
}

class InitAsyncUploadDataStream : public ChunkedUploadDataStream {
 public:
  explicit InitAsyncUploadDataStream(int64_t identifier)
      : ChunkedUploadDataStream(identifier) {}

  InitAsyncUploadDataStream(const InitAsyncUploadDataStream&) = delete;
  InitAsyncUploadDataStream& operator=(const InitAsyncUploadDataStream&) =
      delete;

 private:
  void CompleteInit() { UploadDataStream::OnInitCompleted(OK); }

  int InitInternal(const NetLogWithSource& net_log) override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&InitAsyncUploadDataStream::CompleteInit,
                                  weak_factory_.GetWeakPtr()));
    return ERR_IO_PENDING;
  }

  base::WeakPtrFactory<InitAsyncUploadDataStream> weak_factory_{this};
};

TEST(HttpStreamParser, InitAsynchronousUploadDataStream) {
  base::test::TaskEnvironment task_environment;

  InitAsyncUploadDataStream upload_data_stream(0);

  TestCompletionCallback callback;
  int result = upload_data_stream.Init(callback.callback(), NetLogWithSource());
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));

  // Should be empty progress while initialization is in progress.
  UploadProgress progress = upload_data_stream.GetUploadProgress();
  EXPECT_EQ(0u, progress.size());
  EXPECT_EQ(0u, progress.position());
  EXPECT_THAT(callback.GetResult(result), IsOk());

  // Initialization complete.
  progress = upload_data_stream.GetUploadProgress();
  EXPECT_EQ(0u, progress.size());
  EXPECT_EQ(0u, progress.position());

  static const char kChunk[] = "Chunk 1";
  MockWrite writes[] = {
      MockWrite(ASYNC, 0, "POST / HTTP/1.1\r\n"),
      MockWrite(ASYNC, 1, "Transfer-Encoding: chunked\r\n\r\n"),
      MockWrite(ASYNC, 2, "7\r\nChunk 1\r\n"),
  };

  SequencedSocketData data(base::span<MockRead>(), writes);
  std::unique_ptr<StreamSocket> stream_socket = CreateConnectedSocket(&data);

  scoped_refptr<GrowableIOBuffer> read_buffer =
      base::MakeRefCounted<GrowableIOBuffer>();
  HttpStreamParser parser(stream_socket.get(), false /* is_reused */,
                          GURL("http://localhost"), "POST", &upload_data_stream,
                          read_buffer.get(), NetLogWithSource());

  HttpRequestHeaders headers;
  headers.SetHeader("Transfer-Encoding", "chunked");

  HttpResponseInfo response;
  TestCompletionCallback callback1;
  int result1 = parser.SendRequest("POST / HTTP/1.1\r\n", headers,
                                   TRAFFIC_ANNOTATION_FOR_TESTS, &response,
                                   callback1.callback());
  EXPECT_EQ(ERR_IO_PENDING, result1);
  base::RunLoop().RunUntilIdle();
  upload_data_stream.AppendData(base::byte_span_from_cstring(kChunk), true);

  // Check progress after read completes.
  progress = upload_data_stream.GetUploadProgress();
  EXPECT_EQ(0u, progress.size());
  EXPECT_EQ(7u, progress.position());

  // Check progress after reset.
  upload_data_stream.Reset();
  progress = upload_data_stream.GetUploadProgress();
  EXPECT_EQ(0u, progress.size());
  EXPECT_EQ(0u, progress.position());
}

// The empty payload is how the last chunk is encoded.
TEST(HttpStreamParser, EncodeChunk_EmptyPayload) {
  char output[kOutputSize];

  const std::string_view kPayload = "";
  const std::string_view kExpected = "0\r\n\r\n";
  const int num_bytes_written = HttpStreamParser::EncodeChunk(
      kPayload, base::as_writable_byte_span(output));
  ASSERT_EQ(kExpected.size(), static_cast<size_t>(num_bytes_written));
  EXPECT_EQ(kExpected, std::string_view(output, num_bytes_written));
}

TEST(HttpStreamParser, EncodeChunk_ShortPayload) {
  char output[kOutputSize];

  const std::string kPayload("foo\x00\x11\x22", 6);
  // 11 = payload size + sizeof("6") + CRLF x 2.
  const std::string kExpected("6\r\nfoo\x00\x11\x22\r\n", 11);
  const int num_bytes_written = HttpStreamParser::EncodeChunk(
      kPayload, base::as_writable_byte_span(output));
  ASSERT_EQ(kExpected.size(), static_cast<size_t>(num_bytes_written));
  EXPECT_EQ(kExpected, std::string_view(output, num_bytes_written));
}

TEST(HttpStreamParser, EncodeChunk_LargePayload) {
  char output[kOutputSize];

  const std::string kPayload(1000, '\xff');  // '\xff' x 1000.
  // 3E8 = 1000 in hex.
  const std::string kExpected = "3E8\r\n" + kPayload + "\r\n";
  const int num_bytes_written = HttpStreamParser::EncodeChunk(
      kPayload, base::as_writable_byte_span(output));
  ASSERT_EQ(kExpected.size(), static_cast<size_t>(num_bytes_written));
  EXPECT_EQ(kExpected, std::string_view(output, num_bytes_written));
}

TEST(HttpStreamParser, EncodeChunk_FullPayload) {
  char output[kOutputSize];

  const std::string kPayload(kMaxPayloadSize, '\xff');
  // 3F4 = 1012 in hex.
  const std::string kExpected = "3F4\r\n" + kPayload + "\r\n";
  const int num_bytes_written = HttpStreamParser::EncodeChunk(
      kPayload, base::as_writable_byte_span(output));
  ASSERT_EQ(kExpected.size(), static_cast<size_t>(num_bytes_written));
  EXPECT_EQ(kExpected, std::string_view(output, num_bytes_written));
}

TEST(HttpStreamParser, EncodeChunk_TooLargePayload) {
  char output[kOutputSize];

  // The payload is one byte larger the output buffer size.
  const std::string kPayload(kMaxPayloadSize + 1, '\xff');
  const int num_bytes_written = HttpStreamParser::EncodeChunk(
      kPayload, base::as_writable_byte_span(output));
  ASSERT_THAT(num_bytes_written, IsError(ERR_INVALID_ARGUMENT));
}

TEST(HttpStreamParser, ShouldMergeRequestHeadersAndBody_NoBody) {
  // Shouldn't be merged if upload data is non-existent.
  ASSERT_FALSE(HttpStreamParser::ShouldMergeRequestHeadersAndBody("some header",
                                                                  nullptr));
}

TEST(HttpStreamParser, ShouldMergeRequestHeadersAndBody_EmptyBody) {
  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  std::unique_ptr<UploadDataStream> body(
      std::make_unique<ElementsUploadDataStream>(std::move(element_readers),
                                                 0));
  ASSERT_THAT(body->Init(CompletionOnceCallback(), NetLogWithSource()), IsOk());
  // Shouldn't be merged if upload data is empty.
  ASSERT_FALSE(HttpStreamParser::ShouldMergeRequestHeadersAndBody(
      "some header", body.get()));
}

TEST(HttpStreamParser, ShouldMergeRequestHeadersAndBody_ChunkedBody) {
  const std::string payload = "123";
  auto body = std::make_unique<ChunkedUploadDataStream>(0);
  body->AppendData(base::as_byte_span(payload), true);
  ASSERT_THAT(
      body->Init(TestCompletionCallback().callback(), NetLogWithSource()),
      IsOk());
  // Shouldn't be merged if upload data carries chunked data.
  ASSERT_FALSE(HttpStreamParser::ShouldMergeRequestHeadersAndBody(
      "some header", body.get()));
}

TEST(HttpStreamParser, ShouldMergeRequestHeadersAndBody_FileBody) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  // Create an empty temporary file.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_file_path;
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir.GetPath(), &temp_file_path));

  {
    std::vector<std::unique_ptr<UploadElementReader>> element_readers;

    element_readers.push_back(std::make_unique<UploadFileElementReader>(
        base::SingleThreadTaskRunner::GetCurrentDefault().get(), temp_file_path,
        0, 0, base::Time()));

    std::unique_ptr<UploadDataStream> body(
        std::make_unique<ElementsUploadDataStream>(std::move(element_readers),
                                                   0));
    TestCompletionCallback callback;
    ASSERT_THAT(body->Init(callback.callback(), NetLogWithSource()),
                IsError(ERR_IO_PENDING));
    ASSERT_THAT(callback.WaitForResult(), IsOk());
    // Shouldn't be merged if upload data carries a file, as it's not in-memory.
    ASSERT_FALSE(HttpStreamParser::ShouldMergeRequestHeadersAndBody(
        "some header", body.get()));
  }

  // UploadFileElementReaders may post clean-up tasks on destruction.
  base::RunLoop().RunUntilIdle();
}

TEST(HttpStreamParser, ShouldMergeRequestHeadersAndBody_SmallBodyInMemory) {
  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  const std::string payload = "123";
  element_readers.push_back(
      std::make_unique<UploadBytesElementReader>(base::as_byte_span(payload)));

  std::unique_ptr<UploadDataStream> body(
      std::make_unique<ElementsUploadDataStream>(std::move(element_readers),
                                                 0));
  ASSERT_THAT(body->Init(CompletionOnceCallback(), NetLogWithSource()), IsOk());
  // Yes, should be merged if the in-memory body is small here.
  ASSERT_TRUE(HttpStreamParser::ShouldMergeRequestHeadersAndBody(
      "some header", body.get()));
}

TEST(HttpStreamParser, ShouldMergeRequestHeadersAndBody_LargeBodyInMemory) {
  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  const std::string payload(10000, 'a');  // 'a' x 10000.
  element_readers.push_back(
      std::make_unique<UploadBytesElementReader>(base::as_byte_span(payload)));

  std::unique_ptr<UploadDataStream> body(
      std::make_unique<ElementsUploadDataStream>(std::move(element_readers),
                                                 0));
  ASSERT_THAT(body->Init(CompletionOnceCallback(), NetLogWithSource()), IsOk());
  // Shouldn't be merged if the in-memory body is large here.
  ASSERT_FALSE(HttpStreamParser::ShouldMergeRequestHeadersAndBody(
      "some header", body.get()));
}

TEST(HttpStreamParser, SentBytesNoHeaders) {
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n\r\n"),
  };

  SequencedSocketData data(base::span<MockRead>(), writes);
  std::unique_ptr<StreamSocket> stream_socket = CreateConnectedSocket(&data);

  scoped_refptr<GrowableIOBuffer> read_buffer =
      base::MakeRefCounted<GrowableIOBuffer>();
  HttpStreamParser parser(stream_socket.get(), false /* is_reused */,
                          GURL("http://localhost"), "GET",
                          /*upload_data_stream=*/nullptr, read_buffer.get(),
                          NetLogWithSource());

  HttpResponseInfo response;
  TestCompletionCallback callback;
  EXPECT_EQ(OK, parser.SendRequest("GET / HTTP/1.1\r\n", HttpRequestHeaders(),
                                   TRAFFIC_ANNOTATION_FOR_TESTS, &response,
                                   callback.callback()));

  EXPECT_EQ(CountWriteBytes(writes), parser.sent_bytes());
}

TEST(HttpStreamParser, SentBytesWithHeaders) {
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, 0,
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Connection: Keep-Alive\r\n\r\n"),
  };

  SequencedSocketData data(base::span<MockRead>(), writes);
  std::unique_ptr<StreamSocket> stream_socket = CreateConnectedSocket(&data);

  scoped_refptr<GrowableIOBuffer> read_buffer =
      base::MakeRefCounted<GrowableIOBuffer>();
  HttpStreamParser parser(stream_socket.get(), false /* is_reused */,
                          GURL("http://localhost"), "GET",
                          /*upload_data_stream=*/nullptr, read_buffer.get(),
                          NetLogWithSource());

  HttpRequestHeaders headers;
  headers.SetHeader("Host", "localhost");
  headers.SetHeader("Connection", "Keep-Alive");

  HttpResponseInfo response;
  TestCompletionCallback callback;
  EXPECT_EQ(OK, parser.SendRequest("GET / HTTP/1.1\r\n", headers,
                                   TRAFFIC_ANNOTATION_FOR_TESTS, &response,
                                   callback.callback()));

  EXPECT_EQ(CountWriteBytes(writes), parser.sent_bytes());
}

TEST(HttpStreamParser, SentBytesWithHeadersMultiWrite) {
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n"),
      MockWrite(SYNCHRONOUS, 1, "Host: localhost\r\n"),
      MockWrite(SYNCHRONOUS, 2, "Connection: Keep-Alive\r\n\r\n"),
  };

  SequencedSocketData data(base::span<MockRead>(), writes);
  std::unique_ptr<StreamSocket> stream_socket = CreateConnectedSocket(&data);

  scoped_refptr<GrowableIOBuffer> read_buffer =
      base::MakeRefCounted<GrowableIOBuffer>();
  HttpStreamParser parser(stream_socket.get(), false /* is_reused */,
                          GURL("http://localhost"), "GET",
                          /*upload_data_stream=*/nullptr, read_buffer.get(),
                          NetLogWithSource());

  HttpRequestHeaders headers;
  headers.SetHeader("Host", "localhost");
  headers.SetHeader("Connection", "Keep-Alive");

  HttpResponseInfo response;
  TestCompletionCallback callback;

  EXPECT_EQ(OK, parser.SendRequest("GET / HTTP/1.1\r\n", headers,
                                   TRAFFIC_ANNOTATION_FOR_TESTS, &response,
                                   callback.callback()));

  EXPECT_EQ(CountWriteBytes(writes), parser.sent_bytes());
}

TEST(HttpStreamParser, SentBytesWithErrorWritingHeaders) {
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n"),
      MockWrite(SYNCHRONOUS, 1, "Host: localhost\r\n"),
      MockWrite(SYNCHRONOUS, ERR_CONNECTION_RESET, 2),
  };

  SequencedSocketData data(base::span<MockRead>(), writes);
  std::unique_ptr<StreamSocket> stream_socket = CreateConnectedSocket(&data);

  scoped_refptr<GrowableIOBuffer> read_buffer =
      base::MakeRefCounted<GrowableIOBuffer>();
  HttpStreamParser parser(stream_socket.get(), false /* is_reused */,
                          GURL("http://localhost"), "GET",
                          /*upload_data_stream=*/nullptr, read_buffer.get(),
                          NetLogWithSource());

  HttpRequestHeaders headers;
  headers.SetHeader("Host", "localhost");
  headers.SetHeader("Connection", "Keep-Alive");

  HttpResponseInfo response;
  TestCompletionCallback callback;
  EXPECT_EQ(ERR_CONNECTION_RESET,
            parser.SendRequest("GET / HTTP/1.1\r\n", headers,
                               TRAFFIC_ANNOTATION_FOR_TESTS, &response,
                               callback.callback()));

  EXPECT_EQ(CountWriteBytes(writes), parser.sent_bytes());
}

TEST(HttpStreamParser, SentBytesPost) {
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, 0, "POST / HTTP/1.1\r\n"),
      MockWrite(SYNCHRONOUS, 1, "Content-Length: 12\r\n\r\n"),
      MockWrite(SYNCHRONOUS, 2, "hello world!"),
  };

  SequencedSocketData data(base::span<MockRead>(), writes);
  std::unique_ptr<StreamSocket> stream_socket = CreateConnectedSocket(&data);

  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  element_readers.push_back(std::make_unique<UploadBytesElementReader>(
      base::byte_span_from_cstring("hello world!")));
  ElementsUploadDataStream upload_data_stream(std::move(element_readers), 0);
  ASSERT_THAT(upload_data_stream.Init(TestCompletionCallback().callback(),
                                      NetLogWithSource()),
              IsOk());

  scoped_refptr<GrowableIOBuffer> read_buffer =
      base::MakeRefCounted<GrowableIOBuffer>();
  HttpStreamParser parser(stream_socket.get(), false /* is_reused */,
                          GURL("http://localhost"), "POST", &upload_data_stream,
                          read_buffer.get(), NetLogWithSource());

  HttpRequestHeaders headers;
  headers.SetHeader("Content-Length", "12");

  HttpResponseInfo response;
  TestCompletionCallback callback;
  EXPECT_EQ(OK, parser.SendRequest("POST / HTTP/1.1\r\n", headers,
                                   TRAFFIC_ANNOTATION_FOR_TESTS, &response,
                                   callback.callback()));

  EXPECT_EQ(CountWriteBytes(writes), parser.sent_bytes());

  UploadProgress progress = upload_data_stream.GetUploadProgress();
  EXPECT_EQ(12u, progress.size());
  EXPECT_EQ(12u, progress.position());
}

TEST(HttpStreamParser, SentBytesChunkedPostError) {
  base::test::TaskEnvironment task_environment;

  static const char kChunk[] = "Chunk 1";

  MockWrite writes[] = {
      MockWrite(ASYNC, 0, "POST / HTTP/1.1\r\n"),
      MockWrite(ASYNC, 1, "Transfer-Encoding: chunked\r\n\r\n"),
      MockWrite(ASYNC, 2, "7\r\nChunk 1\r\n"),
      MockWrite(SYNCHRONOUS, ERR_FAILED, 3),
  };

  SequencedSocketData data(base::span<MockRead>(), writes);
  std::unique_ptr<StreamSocket> stream_socket = CreateConnectedSocket(&data);

  ChunkedUploadDataStream upload_data_stream(0);
  ASSERT_THAT(upload_data_stream.Init(TestCompletionCallback().callback(),
                                      NetLogWithSource()),
              IsOk());

  scoped_refptr<GrowableIOBuffer> read_buffer =
      base::MakeRefCounted<GrowableIOBuffer>();
  HttpStreamParser parser(stream_socket.get(), false /* is_reused */,
                          GURL("http://localhost"), "POST", &upload_data_stream,
                          read_buffer.get(), NetLogWithSource());

  HttpRequestHeaders headers;
  headers.SetHeader("Transfer-Encoding", "chunked");

  HttpResponseInfo response;
  TestCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING, parser.SendRequest("POST / HTTP/1.1\r\n", headers,
                                               TRAFFIC_ANNOTATION_FOR_TESTS,
                                               &response, callback.callback()));

  base::RunLoop().RunUntilIdle();
  upload_data_stream.AppendData(base::byte_span_from_cstring(kChunk), false);

  base::RunLoop().RunUntilIdle();
  // This write should fail.
  upload_data_stream.AppendData(base::byte_span_from_cstring(kChunk), false);
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_FAILED));

  EXPECT_EQ(CountWriteBytes(writes), parser.sent_bytes());

  UploadProgress progress = upload_data_stream.GetUploadProgress();
  EXPECT_EQ(0u, progress.size());
  EXPECT_EQ(14u, progress.position());
}

// Test to ensure the HttpStreamParser state machine does not get confused
// when sending a request with a chunked body with only one chunk that becomes
// available asynchronously.
TEST(HttpStreamParser, AsyncSingleChunkAndAsyncSocket) {
  base::test::TaskEnvironment task_environment;

  static const char kChunk[] = "Chunk";

  MockWrite writes[] = {
      MockWrite(ASYNC, 0,
                "GET /one.html HTTP/1.1\r\n"
                "Transfer-Encoding: chunked\r\n\r\n"),
      MockWrite(ASYNC, 1, "5\r\nChunk\r\n"),
      MockWrite(ASYNC, 2, "0\r\n\r\n"),
  };

  // The size of the response body, as reflected in the Content-Length of the
  // MockRead below.
  static const int kBodySize = 8;

  MockRead reads[] = {
      MockRead(ASYNC, 3, "HTTP/1.1 200 OK\r\n"),
      MockRead(ASYNC, 4, "Content-Length: 8\r\n\r\n"),
      MockRead(ASYNC, 5, "one.html"),
      MockRead(SYNCHRONOUS, 0, 6),  // EOF
  };

  ChunkedUploadDataStream upload_stream(0);
  ASSERT_THAT(upload_stream.Init(TestCompletionCallback().callback(),
                                 NetLogWithSource()),
              IsOk());

  SequencedSocketData data(reads, writes);
  std::unique_ptr<StreamSocket> stream_socket = CreateConnectedSocket(&data);

  scoped_refptr<GrowableIOBuffer> read_buffer =
      base::MakeRefCounted<GrowableIOBuffer>();
  HttpStreamParser parser(stream_socket.get(), false /* is_reused */,
                          GURL("http://localhost"), "POST", &upload_stream,
                          read_buffer.get(), NetLogWithSource());

  HttpRequestHeaders request_headers;
  request_headers.SetHeader("Transfer-Encoding", "chunked");

  HttpResponseInfo response_info;
  TestCompletionCallback callback;
  // This will attempt to Write() the initial request and headers, which will
  // complete asynchronously.
  ASSERT_EQ(ERR_IO_PENDING,
            parser.SendRequest("GET /one.html HTTP/1.1\r\n", request_headers,
                               TRAFFIC_ANNOTATION_FOR_TESTS, &response_info,
                               callback.callback()));

  // Complete the initial request write.  Callback should not have been invoked.
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(callback.have_result());

  // Now append the only chunk and wait for the callback.
  upload_stream.AppendData(base::byte_span_from_cstring(kChunk), true);
  ASSERT_THAT(callback.WaitForResult(), IsOk());

  // Attempt to read the response status and the response headers.
  ASSERT_THAT(parser.ReadResponseHeaders(callback.callback()),
              IsError(ERR_IO_PENDING));
  ASSERT_THAT(callback.WaitForResult(), IsOk());

  // Finally, attempt to read the response body.
  auto body_buffer = base::MakeRefCounted<IOBufferWithSize>(kBodySize);
  ASSERT_EQ(ERR_IO_PENDING,
            parser.ReadResponseBody(body_buffer.get(), kBodySize,
                                    callback.callback()));
  ASSERT_EQ(kBodySize, callback.WaitForResult());

  EXPECT_EQ(CountWriteBytes(writes), parser.sent_bytes());
  EXPECT_EQ(CountReadBytes(reads), parser.received_bytes());
}

// Test to ensure the HttpStreamParser state machine does not get confused
// when sending a request with a chunked body with only one chunk that is
// available synchronously.
TEST(HttpStreamParser, SyncSingleChunkAndAsyncSocket) {
  base::test::TaskEnvironment task_environment;

  static const char kChunk[] = "Chunk";

  MockWrite writes[] = {
      MockWrite(ASYNC, 0,
                "GET /one.html HTTP/1.1\r\n"
                "Transfer-Encoding: chunked\r\n\r\n"),
      MockWrite(ASYNC, 1, "5\r\nChunk\r\n"),
      MockWrite(ASYNC, 2, "0\r\n\r\n"),
  };

  // The size of the response body, as reflected in the Content-Length of the
  // MockRead below.
  static const int kBodySize = 8;

  MockRead reads[] = {
      MockRead(ASYNC, 3, "HTTP/1.1 200 OK\r\n"),
      MockRead(ASYNC, 4, "Content-Length: 8\r\n\r\n"),
      MockRead(ASYNC, 5, "one.html"),
      MockRead(SYNCHRONOUS, 0, 6),  // EOF
  };

  ChunkedUploadDataStream upload_stream(0);
  ASSERT_THAT(upload_stream.Init(TestCompletionCallback().callback(),
                                 NetLogWithSource()),
              IsOk());
  // Append the only chunk.
  upload_stream.AppendData(base::byte_span_from_cstring(kChunk), true);

  SequencedSocketData data(reads, writes);
  std::unique_ptr<StreamSocket> stream_socket = CreateConnectedSocket(&data);

  scoped_refptr<GrowableIOBuffer> read_buffer =
      base::MakeRefCounted<GrowableIOBuffer>();
  HttpStreamParser parser(stream_socket.get(), false /* is_reused */,
                          GURL("http://localhost"), "GET", &upload_stream,
                          read_buffer.get(), NetLogWithSource());

  HttpRequestHeaders request_headers;
  request_headers.SetHeader("Transfer-Encoding", "chunked");

  HttpResponseInfo response_info;
  TestCompletionCallback callback;
  // This will attempt to Write() the initial request and headers, which will
  // complete asynchronously.
  ASSERT_EQ(ERR_IO_PENDING,
            parser.SendRequest("GET /one.html HTTP/1.1\r\n", request_headers,
                               TRAFFIC_ANNOTATION_FOR_TESTS, &response_info,
                               callback.callback()));
  ASSERT_THAT(callback.WaitForResult(), IsOk());

  // Attempt to read the response status and the response headers.
  ASSERT_THAT(parser.ReadResponseHeaders(callback.callback()),
              IsError(ERR_IO_PENDING));
  ASSERT_THAT(callback.WaitForResult(), IsOk());

  // Finally, attempt to read the response body.
  auto body_buffer = base::MakeRefCounted<IOBufferWithSize>(kBodySize);
  ASSERT_EQ(ERR_IO_PENDING,
            parser.ReadResponseBody(body_buffer.get(), kBodySize,
                                    callback.callback()));
  ASSERT_EQ(kBodySize, callback.WaitForResult());

  EXPECT_EQ(CountWriteBytes(writes), parser.sent_bytes());
  EXPECT_EQ(CountReadBytes(reads), parser.received_bytes());
}

// Test to ensure the HttpStreamParser state machine does not get confused
// when sending a request with a chunked body, where chunks become available
// asynchronously, over a socket where writes may also complete
// asynchronously.
// This is a regression test for http://crbug.com/132243
TEST(HttpStreamParser, AsyncChunkAndAsyncSocketWithMultipleChunks) {
  base::test::TaskEnvironment task_environment;

  // The chunks that will be written in the request, as reflected in the
  // MockWrites below.
  static const char kChunk1[] = "Chunk 1";
  static const char kChunk2[] = "Chunky 2";
  static const char kChunk3[] = "Test 3";

  MockWrite writes[] = {
      MockWrite(ASYNC, 0,
                "GET /one.html HTTP/1.1\r\n"
                "Transfer-Encoding: chunked\r\n\r\n"),
      MockWrite(ASYNC, 1, "7\r\nChunk 1\r\n"),
      MockWrite(ASYNC, 2, "8\r\nChunky 2\r\n"),
      MockWrite(ASYNC, 3, "6\r\nTest 3\r\n"),
      MockWrite(ASYNC, 4, "0\r\n\r\n"),
  };

  // The size of the response body, as reflected in the Content-Length of the
  // MockRead below.
  static const int kBodySize = 8;

  MockRead reads[] = {
    MockRead(ASYNC, 5, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 6, "Content-Length: 8\r\n\r\n"),
    MockRead(ASYNC, 7, "one.html"),
    MockRead(SYNCHRONOUS, 0, 8),  // EOF
  };

  ChunkedUploadDataStream upload_stream(0);
  upload_stream.AppendData(base::byte_span_from_cstring(kChunk1), false);
  ASSERT_THAT(upload_stream.Init(TestCompletionCallback().callback(),
                                 NetLogWithSource()),
              IsOk());

  SequencedSocketData data(reads, writes);
  std::unique_ptr<StreamSocket> stream_socket = CreateConnectedSocket(&data);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://localhost");
  request_info.upload_data_stream = &upload_stream;

  scoped_refptr<GrowableIOBuffer> read_buffer =
      base::MakeRefCounted<GrowableIOBuffer>();
  HttpStreamParser parser(stream_socket.get(), false /* is_reused */,
                          GURL("http://localhost"), "GET", &upload_stream,
                          read_buffer.get(), NetLogWithSource());

  HttpRequestHeaders request_headers;
  request_headers.SetHeader("Transfer-Encoding", "chunked");

  HttpResponseInfo response_info;
  TestCompletionCallback callback;
  // This will attempt to Write() the initial request and headers, which will
  // complete asynchronously.
  ASSERT_EQ(ERR_IO_PENDING,
            parser.SendRequest("GET /one.html HTTP/1.1\r\n", request_headers,
                               TRAFFIC_ANNOTATION_FOR_TESTS, &response_info,
                               callback.callback()));
  ASSERT_FALSE(callback.have_result());

  // Sending the request and the first chunk completes.
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(callback.have_result());

  // Now append another chunk.
  upload_stream.AppendData(base::byte_span_from_cstring(kChunk2), false);
  ASSERT_FALSE(callback.have_result());

  // Add the final chunk, while the write for the second is still pending,
  // which should not confuse the state machine.
  upload_stream.AppendData(base::byte_span_from_cstring(kChunk3), true);
  ASSERT_FALSE(callback.have_result());

  // Wait for writes to complete.
  ASSERT_THAT(callback.WaitForResult(), IsOk());

  // Attempt to read the response status and the response headers.
  ASSERT_THAT(parser.ReadResponseHeaders(callback.callback()),
              IsError(ERR_IO_PENDING));
  ASSERT_THAT(callback.WaitForResult(), IsOk());

  // Finally, attempt to read the response body.
  auto body_buffer = base::MakeRefCounted<IOBufferWithSize>(kBodySize);
  ASSERT_EQ(ERR_IO_PENDING,
            parser.ReadResponseBody(body_buffer.get(), kBodySize,
                                    callback.callback()));
  ASSERT_EQ(kBodySize, callback.WaitForResult());

  EXPECT_EQ(CountWriteBytes(writes), parser.sent_bytes());
  EXPECT_EQ(CountReadBytes(reads), parser.received_bytes());
}

// Test to ensure the HttpStreamParser state machine does not get confused
// when there's only one "chunk" with 0 bytes, and is received from the
// UploadStream only after sending the request headers successfully.
TEST(HttpStreamParser, AsyncEmptyChunkedUpload) {
  base::test::TaskEnvironment task_environment;

  MockWrite writes[] = {
      MockWrite(ASYNC, 0,
                "GET /one.html HTTP/1.1\r\n"
                "Transfer-Encoding: chunked\r\n\r\n"),
      MockWrite(ASYNC, 1, "0\r\n\r\n"),
  };

  // The size of the response body, as reflected in the Content-Length of the
  // MockRead below.
  const int kBodySize = 8;

  MockRead reads[] = {
      MockRead(ASYNC, 2, "HTTP/1.1 200 OK\r\n"),
      MockRead(ASYNC, 3, "Content-Length: 8\r\n\r\n"),
      MockRead(ASYNC, 4, "one.html"),
      MockRead(SYNCHRONOUS, 0, 5),  // EOF
  };

  ChunkedUploadDataStream upload_stream(0);
  ASSERT_THAT(upload_stream.Init(TestCompletionCallback().callback(),
                                 NetLogWithSource()),
              IsOk());

  SequencedSocketData data(reads, writes);
  std::unique_ptr<StreamSocket> stream_socket = CreateConnectedSocket(&data);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://localhost");
  request_info.upload_data_stream = &upload_stream;

  scoped_refptr<GrowableIOBuffer> read_buffer =
      base::MakeRefCounted<GrowableIOBuffer>();
  HttpStreamParser parser(stream_socket.get(), false /* is_reused */,
                          GURL("http://localhost"), "GET", &upload_stream,
                          read_buffer.get(), NetLogWithSource());

  HttpRequestHeaders request_headers;
  request_headers.SetHeader("Transfer-Encoding", "chunked");

  HttpResponseInfo response_info;
  TestCompletionCallback callback;
  // This will attempt to Write() the initial request and headers, which will
  // complete asynchronously.
  ASSERT_EQ(ERR_IO_PENDING,
            parser.SendRequest("GET /one.html HTTP/1.1\r\n", request_headers,
                               TRAFFIC_ANNOTATION_FOR_TESTS, &response_info,
                               callback.callback()));

  // Now append the terminal 0-byte "chunk".
  upload_stream.AppendData(base::byte_span_from_cstring(""), true);
  ASSERT_FALSE(callback.have_result());

  ASSERT_THAT(callback.WaitForResult(), IsOk());

  // Attempt to read the response status and the response headers.
  ASSERT_THAT(parser.ReadResponseHeaders(callback.callback()),
              IsError(ERR_IO_PENDING));
  ASSERT_THAT(callback.WaitForResult(), IsOk());

  // Finally, attempt to read the response body.
  auto body_buffer = base::MakeRefCounted<IOBufferWithSize>(kBodySize);
  ASSERT_EQ(ERR_IO_PENDING,
            parser.ReadResponseBody(body_buffer.get(), kBodySize,
                                    callback.callback()));
  ASSERT_EQ(kBodySize, callback.WaitForResult());

  EXPECT_EQ(CountWriteBytes(writes), parser.sent_bytes());
  EXPECT_EQ(CountReadBytes(reads), parser.received_bytes());
}

// Test to ensure the HttpStreamParser state machine does not get confused
// when there's only one "chunk" with 0 bytes, which was already appended before
// the request was started.
TEST(HttpStreamParser, SyncEmptyChunkedUpload) {
  base::test::TaskEnvironment task_environment;

  MockWrite writes[] = {
      MockWrite(ASYNC, 0,
                "GET /one.html HTTP/1.1\r\n"
                "Transfer-Encoding: chunked\r\n\r\n"),
      MockWrite(ASYNC, 1, "0\r\n\r\n"),
  };

  // The size of the response body, as reflected in the Content-Length of the
  // MockRead below.
  const int kBodySize = 8;

  MockRead reads[] = {
      MockRead(ASYNC, 2, "HTTP/1.1 200 OK\r\n"),
      MockRead(ASYNC, 3, "Content-Length: 8\r\n\r\n"),
      MockRead(ASYNC, 4, "one.html"),
      MockRead(SYNCHRONOUS, 0, 5),  // EOF
  };

  ChunkedUploadDataStream upload_stream(0);
  ASSERT_THAT(upload_stream.Init(TestCompletionCallback().callback(),
                                 NetLogWithSource()),
              IsOk());
  // Append final empty chunk.
  upload_stream.AppendData(base::byte_span_from_cstring(""), true);

  SequencedSocketData data(reads, writes);
  std::unique_ptr<StreamSocket> stream_socket = CreateConnectedSocket(&data);

  scoped_refptr<GrowableIOBuffer> read_buffer =
      base::MakeRefCounted<GrowableIOBuffer>();
  HttpStreamParser parser(stream_socket.get(), false /* is_reused */,
                          GURL("http://localhost"), "GET", &upload_stream,
                          read_buffer.get(), NetLogWithSource());

  HttpRequestHeaders request_headers;
  request_headers.SetHeader("Transfer-Encoding", "chunked");

  HttpResponseInfo response_info;
  TestCompletionCallback callback;
  // This will attempt to Write() the initial request and headers, which will
  // complete asynchronously.
  ASSERT_EQ(ERR_IO_PENDING,
            parser.SendRequest("GET /one.html HTTP/1.1\r\n", request_headers,
                               TRAFFIC_ANNOTATION_FOR_TESTS, &response_info,
                               callback.callback()));

  // Complete writing the request headers and body.
  ASSERT_THAT(callback.WaitForResult(), IsOk());

  // Attempt to read the response status and the response headers.
  ASSERT_THAT(parser.ReadResponseHeaders(callback.callback()),
              IsError(ERR_IO_PENDING));
  ASSERT_THAT(callback.WaitForResult(), IsOk());

  // Finally, attempt to read the response body.
  auto body_buffer = base::MakeRefCounted<IOBufferWithSize>(kBodySize);
  ASSERT_EQ(ERR_IO_PENDING,
            parser.ReadResponseBody(body_buffer.get(), kBodySize,
                                    callback.callback()));
  ASSERT_EQ(kBodySize, callback.WaitForResult());

  EXPECT_EQ(CountWriteBytes(writes), parser.sent_bytes());
  EXPECT_EQ(CountReadBytes(reads), parser.received_bytes());
}

TEST(HttpStreamParser, TruncatedHeaders) {
  MockRead truncated_status_reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 20"),
    MockRead(SYNCHRONOUS, 0, 2),  // EOF
  };

  MockRead truncated_after_status_reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 Ok\r\n"),
    MockRead(SYNCHRONOUS, 0, 2),  // EOF
  };

  MockRead truncated_in_header_reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 Ok\r\nHead"),
    MockRead(SYNCHRONOUS, 0, 2),  // EOF
  };

  MockRead truncated_after_header_reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 Ok\r\nHeader: foo\r\n"),
    MockRead(SYNCHRONOUS, 0, 2),  // EOF
  };

  MockRead truncated_after_final_newline_reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 Ok\r\nHeader: foo\r\n\r"),
    MockRead(SYNCHRONOUS, 0, 2),  // EOF
  };

  MockRead not_truncated_reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 Ok\r\nHeader: foo\r\n\r\n"),
    MockRead(SYNCHRONOUS, 0, 2),  // EOF
  };

  base::span<MockRead> reads[] = {
      truncated_status_reads,
      truncated_after_status_reads,
      truncated_in_header_reads,
      truncated_after_header_reads,
      truncated_after_final_newline_reads,
      not_truncated_reads,
  };

  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n\r\n"),
  };

  enum {
    HTTP = 0,
    HTTPS,
    NUM_PROTOCOLS,
  };

  for (size_t protocol = 0; protocol < NUM_PROTOCOLS; protocol++) {
    SCOPED_TRACE(protocol);

    for (size_t i = 0; i < std::size(reads); i++) {
      SCOPED_TRACE(i);
      SequencedSocketData data(reads[i], writes);
      std::unique_ptr<StreamSocket> stream_socket(CreateConnectedSocket(&data));

      GURL url;
      if (protocol == HTTP) {
        url = GURL("http://localhost");
      } else {
        url = GURL("https://localhost");
      }

      scoped_refptr<GrowableIOBuffer> read_buffer =
          base::MakeRefCounted<GrowableIOBuffer>();
      HttpStreamParser parser(stream_socket.get(), false /* is_reused */, url,
                              "GET", /*upload_data_stream=*/nullptr,
                              read_buffer.get(), NetLogWithSource());

      HttpRequestHeaders request_headers;
      HttpResponseInfo response_info;
      TestCompletionCallback callback;
      ASSERT_EQ(OK, parser.SendRequest("GET / HTTP/1.1\r\n", request_headers,
                                       TRAFFIC_ANNOTATION_FOR_TESTS,
                                       &response_info, callback.callback()));

      int rv = parser.ReadResponseHeaders(callback.callback());
      EXPECT_EQ(CountWriteBytes(writes), parser.sent_bytes());
      if (i == std::size(reads) - 1) {
        EXPECT_THAT(rv, IsOk());
        EXPECT_TRUE(response_info.headers.get());
        EXPECT_EQ(CountReadBytes(reads[i]), parser.received_bytes());
      } else {
        if (protocol == HTTP) {
          EXPECT_THAT(rv, IsError(ERR_CONNECTION_CLOSED));
          EXPECT_TRUE(response_info.headers.get());
          EXPECT_EQ(CountReadBytes(reads[i]), parser.received_bytes());
        } else {
          EXPECT_THAT(rv, IsError(ERR_RESPONSE_HEADERS_TRUNCATED));
          EXPECT_FALSE(response_info.headers.get());
          EXPECT_EQ(0, parser.received_bytes());
        }
      }
    }
  }
}

// Confirm that on 101 response, the headers are parsed but the data that
// follows remains in the buffer.
TEST(HttpStreamParser, WebSocket101Response) {
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1,
             "HTTP/1.1 101 Switching Protocols\r\n"
             "Upgrade: websocket\r\n"
             "Connection: Upgrade\r\n"
             "\r\n"
             "a fake websocket frame"),
  };

  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n\r\n"),
  };

  SequencedSocketData data(reads, writes);
  std::unique_ptr<StreamSocket> stream_socket = CreateConnectedSocket(&data);

  scoped_refptr<GrowableIOBuffer> read_buffer =
      base::MakeRefCounted<GrowableIOBuffer>();
  HttpStreamParser parser(stream_socket.get(), false /* is_reused */,
                          GURL("http://localhost"), "GET",
                          /*upload_data_stream=*/nullptr, read_buffer.get(),
                          NetLogWithSource());

  HttpRequestHeaders request_headers;
  HttpResponseInfo response_info;
  TestCompletionCallback callback;
  ASSERT_EQ(OK, parser.SendRequest("GET / HTTP/1.1\r\n", request_headers,
                                   TRAFFIC_ANNOTATION_FOR_TESTS, &response_info,
                                   callback.callback()));

  EXPECT_THAT(parser.ReadResponseHeaders(callback.callback()), IsOk());
  ASSERT_TRUE(response_info.headers.get());
  EXPECT_EQ(101, response_info.headers->response_code());
  EXPECT_TRUE(response_info.headers->HasHeaderValue("Connection", "Upgrade"));
  EXPECT_TRUE(response_info.headers->HasHeaderValue("Upgrade", "websocket"));
  EXPECT_EQ(read_buffer->capacity(), read_buffer->offset());
  EXPECT_EQ("a fake websocket frame",
            base::as_string_view(read_buffer->everything()));

  EXPECT_EQ(CountWriteBytes(writes), parser.sent_bytes());
  EXPECT_EQ(CountReadBytes(reads) -
                static_cast<int64_t>(strlen("a fake websocket frame")),
            parser.received_bytes());
}

// Helper class for constructing HttpStreamParser and running GET requests.
class SimpleGetRunner {
 public:
  SimpleGetRunner()
      : url_("http://localhost"),
        read_buffer_(base::MakeRefCounted<GrowableIOBuffer>()) {
    writes_.emplace_back(
        MockWrite(SYNCHRONOUS, sequence_number_++, "GET / HTTP/1.1\r\n\r\n"));
  }

  void set_url(const GURL& url) { url_ = url; }

  HttpStreamParser* parser() { return parser_.get(); }
  GrowableIOBuffer* read_buffer() { return read_buffer_.get(); }
  HttpResponseInfo* response_info() { return &response_info_; }

  void AddInitialData(const std::string& data) {
    int offset = read_buffer_->offset();
    read_buffer_->SetCapacity(offset + data.size());
    auto span = base::as_byte_span(data);
    read_buffer_->everything().subspan(offset, span.size()).copy_from(span);
    read_buffer_->set_offset(offset + span.size());
  }

  // The data used to back |string_piece| must stay alive until all mock data
  // has been read.
  void AddRead(std::string_view string_piece) {
    reads_.emplace_back(SYNCHRONOUS, string_piece.data(), string_piece.length(),
                        sequence_number_++);
  }

  void AddAsyncRead(std::string_view string_piece) {
    reads_.emplace_back(ASYNC, string_piece.data(), string_piece.length(),
                        sequence_number_++);
  }

  void SetupParserAndSendRequest() {
    reads_.emplace_back(SYNCHRONOUS, 0, sequence_number_++);  // EOF

    data_ = std::make_unique<SequencedSocketData>(reads_, writes_);
    stream_socket_ = CreateConnectedSocket(data_.get());

    parser_ = std::make_unique<HttpStreamParser>(
        stream_socket_.get(), false /* is_reused */, url_, "GET",
        /*upload_data_stream=*/nullptr, read_buffer(), NetLogWithSource());

    TestCompletionCallback callback;
    ASSERT_EQ(OK, parser_->SendRequest("GET / HTTP/1.1\r\n", request_headers_,
                                       TRAFFIC_ANNOTATION_FOR_TESTS,
                                       &response_info_, callback.callback()));
  }

  void ReadHeadersExpectingError(Error error) {
    TestCompletionCallback callback;
    EXPECT_THAT(parser_->ReadResponseHeaders(callback.callback()),
                IsError(error));
  }

  void ReadHeaders() { ReadHeadersExpectingError(OK); }

  std::string ReadBody(int user_buf_len, int* read_lengths) {
    TestCompletionCallback callback;
    auto buffer = base::MakeRefCounted<IOBufferWithSize>(user_buf_len);
    int rv;
    int i = 0;
    std::string body;
    while (true) {
      rv = parser_->ReadResponseBody(
          buffer.get(), user_buf_len, callback.callback());
      EXPECT_EQ(read_lengths[i], rv);
      if (rv == ERR_IO_PENDING) {
        rv = callback.WaitForResult();
        i++;
        EXPECT_EQ(read_lengths[i], rv);
      }
      if (rv > 0)
        body.append(buffer->data(), rv);
      i++;
      if (rv <= 0)
        return body;
    }
  }

 private:
  GURL url_;

  HttpRequestHeaders request_headers_;
  HttpResponseInfo response_info_;
  scoped_refptr<GrowableIOBuffer> read_buffer_;
  std::vector<MockRead> reads_;
  std::vector<MockWrite> writes_;
  std::unique_ptr<StreamSocket> stream_socket_;
  std::unique_ptr<SequencedSocketData> data_;
  std::unique_ptr<HttpStreamParser> parser_;
  int sequence_number_ = 0;
};

// Test that HTTP/0.9 works as expected, only on ports where it should be
// enabled.
TEST(HttpStreamParser, Http09PortTests) {
  struct TestCase {
    const char* url;

    // Expected result when trying to read headers and response is an HTTP/0.9
    // non-Shoutcast response.
    Error expected_09_header_error;

    // Expected result when trying to read headers for a shoutcast response.
    Error expected_shoutcast_header_error;
  };

  const TestCase kTestCases[] = {
      // Default ports should work for HTTP/0.9, regardless of whether the port
      // is explicitly specified or not.
      {"http://foo.com/", OK, OK},
      {"http://foo.com:80/", OK, OK},
      {"https://foo.com/", OK, OK},
      {"https://foo.com:443/", OK, OK},

      // Non-standard ports should not support HTTP/0.9, by default.
      {"http://foo.com:8080/", ERR_INVALID_HTTP_RESPONSE, OK},
      {"https://foo.com:8080/", ERR_INVALID_HTTP_RESPONSE,
       ERR_INVALID_HTTP_RESPONSE},
      {"http://foo.com:443/", ERR_INVALID_HTTP_RESPONSE, OK},
      {"https://foo.com:80/", ERR_INVALID_HTTP_RESPONSE,
       ERR_INVALID_HTTP_RESPONSE},
  };

  const std::string kResponse = "hello\r\nworld\r\n";

  for (const auto& test_case : kTestCases) {
    SimpleGetRunner get_runner;
    get_runner.set_url(GURL(test_case.url));
    get_runner.AddRead(kResponse);
    get_runner.SetupParserAndSendRequest();

    get_runner.ReadHeadersExpectingError(test_case.expected_09_header_error);
    if (test_case.expected_09_header_error != OK)
      continue;

    ASSERT_TRUE(get_runner.response_info()->headers);
    EXPECT_EQ("HTTP/0.9 200 OK",
              get_runner.response_info()->headers->GetStatusLine());

    EXPECT_EQ(0, get_runner.parser()->received_bytes());
    int read_lengths[] = {static_cast<int>(kResponse.size()), 0};
    get_runner.ReadBody(kResponse.size(), read_lengths);
    EXPECT_EQ(kResponse.size(),
              static_cast<size_t>(get_runner.parser()->received_bytes()));
    EXPECT_EQ(HttpConnectionInfo::kHTTP0_9,
              get_runner.response_info()->connection_info);
  }

  const std::string kShoutcastResponse = "ICY 200 blah\r\n\r\n";
  for (const auto& test_case : kTestCases) {
    SimpleGetRunner get_runner;
    get_runner.set_url(GURL(test_case.url));
    get_runner.AddRead(kShoutcastResponse);
    get_runner.SetupParserAndSendRequest();

    get_runner.ReadHeadersExpectingError(
        test_case.expected_shoutcast_header_error);
    if (test_case.expected_shoutcast_header_error != OK)
      continue;

    ASSERT_TRUE(get_runner.response_info()->headers);
    EXPECT_EQ("HTTP/0.9 200 OK",
              get_runner.response_info()->headers->GetStatusLine());

    EXPECT_EQ(0, get_runner.parser()->received_bytes());
    int read_lengths[] = {static_cast<int>(kShoutcastResponse.size()), 0};
    get_runner.ReadBody(kShoutcastResponse.size(), read_lengths);
    EXPECT_EQ(kShoutcastResponse.size(),
              static_cast<size_t>(get_runner.parser()->received_bytes()));
    EXPECT_EQ(HttpConnectionInfo::kHTTP0_9,
              get_runner.response_info()->connection_info);
  }
}

TEST(HttpStreamParser, ContinueWithBody) {
  const std::string kResponse =
      "HTTP/1.1 100 Continue\r\n\r\nhello\r\nworld\r\n";

  SimpleGetRunner get_runner;
  get_runner.set_url(GURL("http://foo.com/"));
  get_runner.AddRead(kResponse);
  get_runner.SetupParserAndSendRequest();

  get_runner.ReadHeadersExpectingError(OK);
  ASSERT_TRUE(get_runner.response_info()->headers);
  EXPECT_EQ("HTTP/1.1 100 Continue",
            get_runner.response_info()->headers->GetStatusLine());

  // We ignore informational responses and start reading the next response in
  // the stream. This simulates the behavior.
  get_runner.ReadHeadersExpectingError(ERR_INVALID_HTTP_RESPONSE);
}

TEST(HttpStreamParser, NullFails) {
  const char kTestHeaders[] =
      "HTTP/1.1 200 OK\r\n"
      "Foo: Bar\r\n"
      "Content-Length: 4\r\n\r\n";

  // Try inserting a null at each position in kTestHeaders. Every location
  // should result in an error.
  //
  // Need to start at 4 because HttpStreamParser will treat the response as
  // HTTP/0.9 if it doesn't see "HTTP", and need to end at -1 because "\r\n\r"
  // is currently treated as a valid end of header marker.
  for (size_t i = 4; i < std::size(kTestHeaders) - 1; ++i) {
    std::string read_data(kTestHeaders);
    read_data.insert(i, 1, '\0');
    read_data.append("body");
    SimpleGetRunner get_runner;
    get_runner.set_url(GURL("http://foo.test/"));
    get_runner.AddRead(read_data);
    get_runner.SetupParserAndSendRequest();

    get_runner.ReadHeadersExpectingError(ERR_INVALID_HTTP_RESPONSE);
  }
}

// Make sure that Shoutcast is recognized when receiving one byte at a time.
TEST(HttpStreamParser, ShoutcastSingleByteReads) {
  SimpleGetRunner get_runner;
  get_runner.set_url(GURL("http://foo.com:8080/"));
  get_runner.AddRead("i");
  get_runner.AddRead("c");
  get_runner.AddRead("Y");
  // Needed because HttpStreamParser::Read returns ERR_CONNECTION_CLOSED on
  // small response headers, which HttpNetworkTransaction replaces with OK.
  // TODO(mmenke): Can we just change that behavior?
  get_runner.AddRead(" Extra stuff");
  get_runner.SetupParserAndSendRequest();

  get_runner.ReadHeadersExpectingError(OK);
  EXPECT_EQ("HTTP/0.9 200 OK",
            get_runner.response_info()->headers->GetStatusLine());
}

// Make sure that Shoutcast is recognized when receiving any string starting
// with "ICY", regardless of capitalization, and without a space following it
// (The latter behavior is just to match HTTP detection).
TEST(HttpStreamParser, ShoutcastWeirdHeader) {
  SimpleGetRunner get_runner;
  get_runner.set_url(GURL("http://foo.com:8080/"));
  get_runner.AddRead("iCyCreamSundae");
  get_runner.SetupParserAndSendRequest();

  get_runner.ReadHeadersExpectingError(OK);
  EXPECT_EQ("HTTP/0.9 200 OK",
            get_runner.response_info()->headers->GetStatusLine());
}

// Make sure that HTTP/0.9 isn't allowed in the truncated header case on a weird
// port.
TEST(HttpStreamParser, Http09TruncatedHeaderPortTest) {
  SimpleGetRunner get_runner;
  get_runner.set_url(GURL("http://foo.com:8080/"));
  std::string response = "HT";
  get_runner.AddRead(response);
  get_runner.SetupParserAndSendRequest();

  get_runner.ReadHeadersExpectingError(ERR_INVALID_HTTP_RESPONSE);
}

// Test basic case where there is no keep-alive or extra data from the socket,
// and the entire response is received in a single read.
TEST(HttpStreamParser, ReceivedBytesNormal) {
  std::string headers =
      "HTTP/1.0 200 OK\r\n"
      "Content-Length: 7\r\n\r\n";
  std::string body = "content";
  std::string response = headers + body;

  SimpleGetRunner get_runner;
  get_runner.AddRead(response);
  get_runner.SetupParserAndSendRequest();
  get_runner.ReadHeaders();
  int64_t headers_size = headers.size();
  EXPECT_EQ(headers_size, get_runner.parser()->received_bytes());
  int body_size = body.size();
  int read_lengths[] = {body_size, 0};
  get_runner.ReadBody(body_size, read_lengths);
  int64_t response_size = response.size();
  EXPECT_EQ(response_size, get_runner.parser()->received_bytes());
  EXPECT_EQ(HttpConnectionInfo::kHTTP1_0,
            get_runner.response_info()->connection_info);
}

// Test that bytes that represent "next" response are not counted
// as current response "received_bytes".
TEST(HttpStreamParser, ReceivedBytesExcludesNextResponse) {
  std::string headers = "HTTP/1.1 200 OK\r\n"
      "Content-Length:  8\r\n\r\n";
  std::string body = "content8";
  std::string response = headers + body;
  std::string next_response = "HTTP/1.1 200 OK\r\n\r\nFOO";
  std::string data = response + next_response;

  SimpleGetRunner get_runner;
  get_runner.AddRead(data);
  get_runner.SetupParserAndSendRequest();
  get_runner.ReadHeaders();
  EXPECT_EQ(39, get_runner.parser()->received_bytes());
  int64_t headers_size = headers.size();
  EXPECT_EQ(headers_size, get_runner.parser()->received_bytes());
  int body_size = body.size();
  int read_lengths[] = {body_size, 0};
  get_runner.ReadBody(body_size, read_lengths);
  int64_t response_size = response.size();
  EXPECT_EQ(response_size, get_runner.parser()->received_bytes());
  EXPECT_EQ(0, get_runner.read_buffer()->offset());
  EXPECT_EQ(HttpConnectionInfo::kHTTP1_1,
            get_runner.response_info()->connection_info);
}

// Test that "received_bytes" calculation works fine when last read
// contains more data than requested by user.
// We send data in two reads:
// 1) Headers + beginning of response
// 2) remaining part of response + next response start
// We setup user read buffer so it fully accepts the beginning of response
// body, but it is larger than remaining part of body.
TEST(HttpStreamParser, ReceivedBytesMultiReadExcludesNextResponse) {
  std::string headers = "HTTP/1.1 200 OK\r\n"
      "Content-Length: 36\r\n\r\n";
  int64_t user_buf_len = 32;
  std::string body_start = std::string(user_buf_len, '#');
  int body_start_size = body_start.size();
  EXPECT_EQ(user_buf_len, body_start_size);
  std::string response_start = headers + body_start;
  std::string body_end = "abcd";
  std::string next_response = "HTTP/1.1 200 OK\r\n\r\nFOO";
  std::string response_end = body_end + next_response;

  SimpleGetRunner get_runner;
  get_runner.AddRead(response_start);
  get_runner.AddRead(response_end);
  get_runner.SetupParserAndSendRequest();
  get_runner.ReadHeaders();
  int64_t headers_size = headers.size();
  EXPECT_EQ(headers_size, get_runner.parser()->received_bytes());
  int body_end_size = body_end.size();
  int read_lengths[] = {body_start_size, body_end_size, 0};
  get_runner.ReadBody(body_start_size, read_lengths);
  int64_t response_size = response_start.size() + body_end_size;
  EXPECT_EQ(response_size, get_runner.parser()->received_bytes());
  EXPECT_EQ(0, get_runner.read_buffer()->offset());
  EXPECT_FALSE(get_runner.parser()->CanReuseConnection());
}

TEST(HttpStreamParser, ReceivedBytesMultiReadExcludesExtraData) {
  const std::string headers =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 36\r\n\r\n";
  const int64_t user_buf_len = 32;
  const std::string body_start = std::string(user_buf_len, '#');
  const int body_start_size = body_start.size();
  EXPECT_EQ(user_buf_len, body_start_size);
  const std::string body_end = "abcd";
  const int body_end_size = body_end.size();
  const std::string body = body_start + body_end;
  const int body_size = body.size();
  const std::string extra_data = "HTTP/1.1 200 OK\r\n\r\nFOO";
  const std::string read_data = body + extra_data;

  SimpleGetRunner get_runner;
  get_runner.AddRead(headers);
  get_runner.AddRead(read_data);
  get_runner.SetupParserAndSendRequest();
  get_runner.ReadHeaders();
  const int headers_size = headers.size();
  EXPECT_EQ(headers_size, get_runner.parser()->received_bytes());
  int read_lengths[] = {body_start_size, body_end_size, 0};
  get_runner.ReadBody(body_start_size, read_lengths);
  const int response_size = headers_size + body_size;
  EXPECT_EQ(response_size, get_runner.parser()->received_bytes());
  EXPECT_EQ(0, get_runner.read_buffer()->offset());
  EXPECT_FALSE(get_runner.parser()->CanReuseConnection());
}

TEST(HttpStreamParser, ReceivedBytesAsyncMultiReadExcludesExtraData) {
  base::test::SingleThreadTaskEnvironment task_environment;

  const std::string headers =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 36\r\n\r\n";
  const int64_t user_buf_len = 32;
  const std::string body_start = std::string(user_buf_len, '#');
  const int body_start_size = body_start.size();
  EXPECT_EQ(user_buf_len, body_start_size);
  const std::string body_end = "abcd";
  const int body_end_size = body_end.size();
  const std::string body = body_start + body_end;
  const int body_size = body.size();
  const std::string extra_data = "HTTP/1.1 200 OK\r\n\r\nFOO";
  const std::string read_data = body_end + extra_data;

  SimpleGetRunner get_runner;
  get_runner.AddRead(headers);
  get_runner.AddRead(body_start);
  get_runner.AddAsyncRead(read_data);
  get_runner.SetupParserAndSendRequest();
  get_runner.ReadHeaders();
  const int headers_size = headers.size();
  EXPECT_EQ(headers_size, get_runner.parser()->received_bytes());
  int read_lengths[] = {body_start_size, -1, body_end_size, 0};
  get_runner.ReadBody(body_start_size, read_lengths);
  const int response_size = headers_size + body_size;
  EXPECT_EQ(response_size, get_runner.parser()->received_bytes());
  EXPECT_EQ(0, get_runner.read_buffer()->offset());
  EXPECT_FALSE(get_runner.parser()->CanReuseConnection());
}

TEST(HttpStreamParser, ReceivedBytesExcludesExtraDataLargeBuffer) {
  const std::string headers =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 36\r\n\r\n";
  const std::string body = std::string(36, '#');
  const int body_size = body.size();
  const std::string extra_data = std::string(14, '!');
  const std::string response = headers + body + extra_data;
  const int response_size = response.size();

  SimpleGetRunner get_runner;
  get_runner.AddRead(response);
  get_runner.SetupParserAndSendRequest();
  get_runner.ReadHeaders();
  const int headers_size = headers.size();
  EXPECT_EQ(headers_size, get_runner.parser()->received_bytes());
  int read_lengths[] = {body_size, 0};
  get_runner.ReadBody(response_size, read_lengths);
  const int actual_response_size = headers_size + body_size;
  EXPECT_EQ(actual_response_size, get_runner.parser()->received_bytes());
  EXPECT_EQ(0, get_runner.read_buffer()->offset());
  EXPECT_FALSE(get_runner.parser()->CanReuseConnection());
}

TEST(HttpStreamParser, ReceivedBytesExcludesExtraDataSmallBuffer) {
  const std::string headers =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 36\r\n\r\n";
  const std::string body = std::string(36, '#');
  const int body_size = body.size();
  const std::string extra_data = std::string(14, '!');
  const std::string response = headers + body + extra_data;

  SimpleGetRunner get_runner;
  get_runner.AddRead(response);
  get_runner.SetupParserAndSendRequest();
  get_runner.ReadHeaders();
  const int headers_size = headers.size();
  EXPECT_EQ(headers_size, get_runner.parser()->received_bytes());
  int read_lengths[] = {10, 10, 10, 6, 0};
  get_runner.ReadBody(10, read_lengths);
  const int actual_response_size = headers_size + body_size;
  EXPECT_EQ(actual_response_size, get_runner.parser()->received_bytes());
  EXPECT_EQ(0, get_runner.read_buffer()->offset());
  EXPECT_FALSE(get_runner.parser()->CanReuseConnection());
}

// Test that "received_bytes" calculation works fine when there is no
// network activity at all; that is when all data is read from read buffer.
// In this case read buffer contains two responses. We expect that only
// bytes that correspond to the first one are taken into account.
TEST(HttpStreamParser, ReceivedBytesFromReadBufExcludesNextResponse) {
  std::string headers = "HTTP/1.1 200 OK\r\n"
      "Content-Length: 7\r\n\r\n";
  std::string body = "content";
  std::string response = headers + body;
  std::string next_response = "HTTP/1.1 200 OK\r\n\r\nFOO";
  std::string data = response + next_response;

  SimpleGetRunner get_runner;
  get_runner.AddInitialData(data);
  get_runner.SetupParserAndSendRequest();
  get_runner.ReadHeaders();
  int64_t headers_size = headers.size();
  EXPECT_EQ(headers_size, get_runner.parser()->received_bytes());
  int body_size = body.size();
  int read_lengths[] = {body_size, 0};
  get_runner.ReadBody(body_size, read_lengths);
  int64_t response_size = response.size();
  EXPECT_EQ(response_size, get_runner.parser()->received_bytes());
  EXPECT_EQ(0, get_runner.read_buffer()->offset());
  EXPECT_FALSE(get_runner.parser()->CanReuseConnection());
}

// Test calculating "received_bytes" when part of request has been already
// loaded and placed to read buffer by previous stream parser.
TEST(HttpStreamParser, ReceivedBytesUseReadBuf) {
  std::string buffer = "HTTP/1.1 200 OK\r\n";
  std::string remaining_headers = "Content-Length: 7\r\n\r\n";
  int64_t headers_size = buffer.size() + remaining_headers.size();
  std::string body = "content";
  std::string response = remaining_headers + body;

  SimpleGetRunner get_runner;
  get_runner.AddInitialData(buffer);
  get_runner.AddRead(response);
  get_runner.SetupParserAndSendRequest();
  get_runner.ReadHeaders();
  EXPECT_EQ(headers_size, get_runner.parser()->received_bytes());
  int body_size = body.size();
  int read_lengths[] = {body_size, 0};
  get_runner.ReadBody(body_size, read_lengths);
  EXPECT_EQ(headers_size + body_size, get_runner.parser()->received_bytes());
  EXPECT_EQ(0, get_runner.read_buffer()->offset());
}

// Test the case when the resulting read_buf contains both unused bytes and
// bytes ejected by chunked-encoding filter.
TEST(HttpStreamParser, ReceivedBytesChunkedTransferExcludesNextResponse) {
  std::string response = "HTTP/1.1 200 OK\r\n"
      "Transfer-Encoding: chunked\r\n\r\n"
      "7\r\nChunk 1\r\n"
      "8\r\nChunky 2\r\n"
      "6\r\nTest 3\r\n"
      "0\r\n\r\n";
  std::string next_response = "foo bar\r\n";
  std::string data = response + next_response;

  SimpleGetRunner get_runner;
  get_runner.AddInitialData(data);
  get_runner.SetupParserAndSendRequest();
  get_runner.ReadHeaders();
  int read_lengths[] = {4, 3, 6, 2, 6, 0};
  get_runner.ReadBody(7, read_lengths);
  int64_t response_size = response.size();
  EXPECT_EQ(response_size, get_runner.parser()->received_bytes());
  int64_t next_response_size = next_response.size();
  EXPECT_EQ(next_response_size, get_runner.read_buffer()->offset());
}

// Test that data transfered in multiple reads is correctly processed.
// We feed data into 4-bytes reads. Also we set length of read
// buffer to 5-bytes to test all possible buffer misaligments.
TEST(HttpStreamParser, ReceivedBytesMultipleReads) {
  std::string headers = "HTTP/1.1 200 OK\r\n"
      "Content-Length: 33\r\n\r\n";
  std::string body = "foo bar baz\r\n"
      "sputnik mir babushka";
  std::string response = headers + body;

  size_t receive_length = 4;
  std::vector<std::string> blocks;
  for (size_t i = 0; i < response.size(); i += receive_length) {
    size_t length = std::min(receive_length, response.size() - i);
    blocks.push_back(response.substr(i, length));
  }

  SimpleGetRunner get_runner;
  for (const auto& block : blocks)
    get_runner.AddRead(block);
  get_runner.SetupParserAndSendRequest();
  get_runner.ReadHeaders();
  int64_t headers_size = headers.size();
  EXPECT_EQ(headers_size, get_runner.parser()->received_bytes());
  int read_lengths[] = {1, 4, 4, 4, 4, 4, 4, 4, 4, 0};
  get_runner.ReadBody(receive_length + 1, read_lengths);
  int64_t response_size = response.size();
  EXPECT_EQ(response_size, get_runner.parser()->received_bytes());
}

// Test timing information of responses that don't have informational (1xx)
// response headers.
TEST(HttpStreamParser, NonInformationalResponseStart) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  std::string response_headers1 = "HTTP/1.1 200 OK\r\n";
  std::string response_headers2 = "Content-Length: 7\r\n\r\n";
  int64_t response_headers_size =
      response_headers1.size() + response_headers2.size();

  std::string response_body = "content";
  int64_t response_size = response_headers_size + response_body.size();

  MockWrite writes[] = {MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n\r\n")};

  MockRead reads[] = {
      // Add pauses between header fragments so that the test runner can advance
      // the mock clock to test timing information.
      MockRead(ASYNC, 1, response_headers1.c_str()),
      MockRead(ASYNC, ERR_IO_PENDING, 2),
      MockRead(ASYNC, 3, response_headers2.c_str()),
      MockRead(ASYNC, 4, response_body.c_str()),
  };

  // Set up the sequenced socket data.
  SequencedSocketData sequenced_socket_data(reads, writes);
  std::unique_ptr<StreamSocket> stream_socket =
      CreateConnectedSocket(&sequenced_socket_data);

  // Set up the http stream parser.
  auto read_buffer = base::MakeRefCounted<GrowableIOBuffer>();
  HttpStreamParser parser(stream_socket.get(), /*connection_is_reused=*/false,
                          GURL("http://localhost"), "GET",
                          /*upload_data_stream=*/nullptr, read_buffer.get(),
                          NetLogWithSource());

  // Send a request.
  HttpResponseInfo response;
  TestCompletionCallback callback;
  EXPECT_THAT(parser.SendRequest("GET / HTTP/1.1\r\n", HttpRequestHeaders(),
                                 TRAFFIC_ANNOTATION_FOR_TESTS, &response,
                                 callback.callback()),
              IsOk());

  EXPECT_THAT(parser.ReadResponseHeaders(callback.callback()),
              IsError(ERR_IO_PENDING));
  task_environment.AdvanceClock(base::Seconds(1));

  // [seq=1 --> seq=2] The parser reads the first fragment of the response
  // headers and then pauses to advance the mock clock.
  base::TimeTicks first_response_start_time = task_environment.NowTicks();
  sequenced_socket_data.RunUntilPaused();
  task_environment.AdvanceClock(base::Seconds(1));

  // [seq=3] The parser reads the second fragment of the response headers.
  sequenced_socket_data.Resume();
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  // Check the received headers.
  EXPECT_EQ(200, response.headers->response_code());
  EXPECT_EQ(response_headers_size, parser.received_bytes());

  // No informational responses were served. The first response start time
  // should be equal to the non-informational response start time.
  EXPECT_EQ(parser.first_response_start_time(), first_response_start_time);
  EXPECT_EQ(parser.non_informational_response_start_time(),
            first_response_start_time);

  // [seq=4] The parser reads the response body.
  auto body_buffer =
      base::MakeRefCounted<IOBufferWithSize>(response_body.size());
  int result = parser.ReadResponseBody(body_buffer.get(), response_body.size(),
                                       callback.callback());
  EXPECT_THAT(callback.GetResult(result), response_body.size());

  // Check the received body.
  EXPECT_EQ(response_size, parser.received_bytes());
}

// Test timing information of responses that don't have informational (1xx)
// response headers, and have no response body.
TEST(HttpStreamParser, NonInformationalResponseStartWithoutBody) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  std::string response_headers1 = "HTTP/1.1 200 OK\r\n";
  std::string response_headers2 = "Content-Length: 0\r\n\r\n";
  int64_t response_size = response_headers1.size() + response_headers2.size();

  MockWrite writes[] = {MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n\r\n")};

  MockRead reads[] = {
      // Add pauses between header fragments so that the test runner can advance
      // the mock clock to test timing information.
      MockRead(ASYNC, 1, response_headers1.c_str()),
      MockRead(ASYNC, ERR_IO_PENDING, 2),
      MockRead(ASYNC, 3, response_headers2.c_str()),
  };

  // Set up the sequenced socket data.
  SequencedSocketData sequenced_socket_data(reads, writes);
  std::unique_ptr<StreamSocket> stream_socket =
      CreateConnectedSocket(&sequenced_socket_data);

  // Set up the http stream parser.
  auto read_buffer = base::MakeRefCounted<GrowableIOBuffer>();
  HttpStreamParser parser(stream_socket.get(), /*connection_is_reused=*/false,
                          GURL("http://localhost"), "GET",
                          /*upload_data_stream=*/nullptr, read_buffer.get(),
                          NetLogWithSource());

  // Send a request.
  HttpResponseInfo response;
  TestCompletionCallback callback;
  EXPECT_THAT(parser.SendRequest("GET / HTTP/1.1\r\n", HttpRequestHeaders(),
                                 TRAFFIC_ANNOTATION_FOR_TESTS, &response,
                                 callback.callback()),
              IsOk());

  EXPECT_THAT(parser.ReadResponseHeaders(callback.callback()),
              IsError(ERR_IO_PENDING));
  task_environment.AdvanceClock(base::Seconds(1));

  // [seq=1 --> seq=2] The parser reads the first fragment of the response
  // headers and then pauses to advance the mock clock.
  base::TimeTicks first_response_start_time = task_environment.NowTicks();
  sequenced_socket_data.RunUntilPaused();
  task_environment.AdvanceClock(base::Seconds(1));

  // [seq=3] The parser reads the second fragment of the response headers.
  sequenced_socket_data.Resume();
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  // Check the received headers.
  EXPECT_EQ(200, response.headers->response_code());
  EXPECT_EQ(response_size, parser.received_bytes());

  // No informational responses were served. The first response start time
  // should be equal to the non-informational response start time.
  EXPECT_EQ(parser.first_response_start_time(), first_response_start_time);
  EXPECT_EQ(parser.non_informational_response_start_time(),
            first_response_start_time);

  // [seq=4] The parser reads the response body.
  auto body_buffer = base::MakeRefCounted<IOBufferWithSize>(10);
  int result = parser.ReadResponseBody(body_buffer.get(), body_buffer->size(),
                                       callback.callback());
  EXPECT_THAT(callback.GetResult(result), IsError(OK));
}

// Test that "continue" HTTP header is counted as "received_bytes".
TEST(HttpStreamParser, ReceivedBytesIncludesContinueHeader) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  std::string status100_response_headers1 = "HTTP/1.1 100 ";
  std::string status100_response_headers2 = "Continue\r\n\r\n";
  int64_t status100_response_headers_size =
      status100_response_headers1.size() + status100_response_headers2.size();

  std::string response_headers1 = "HTTP/1.1 200 OK\r\n";
  std::string response_headers2 = "Content-Length: 7\r\n\r\n";
  int64_t response_headers_size =
      response_headers1.size() + response_headers2.size();

  std::string response_body = "content";
  int64_t response_size = status100_response_headers_size +
                          response_headers_size + response_body.size();

  MockWrite writes[] = {MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n\r\n")};

  MockRead reads[] = {
      // Add pauses between header fragments so that the test runner can advance
      // the mock clock to test timing information.

      // 100 response headers.
      MockRead(ASYNC, 1, status100_response_headers1.c_str()),
      MockRead(ASYNC, ERR_IO_PENDING, 2),
      MockRead(ASYNC, 3, status100_response_headers2.c_str()),
      MockRead(ASYNC, ERR_IO_PENDING, 4),

      // 200 response headers and body.
      MockRead(ASYNC, 5, response_headers1.c_str()),
      MockRead(ASYNC, ERR_IO_PENDING, 6),
      MockRead(ASYNC, 7, response_headers2.c_str()),
      MockRead(ASYNC, 8, response_body.c_str()),
  };

  // Set up the sequenced socket data.
  SequencedSocketData sequenced_socket_data(reads, writes);
  std::unique_ptr<StreamSocket> stream_socket =
      CreateConnectedSocket(&sequenced_socket_data);

  // Set up the http stream parser.
  auto read_buffer = base::MakeRefCounted<GrowableIOBuffer>();
  HttpStreamParser parser(stream_socket.get(), /*connection_is_reused=*/false,
                          GURL("http://localhost"), "GET",
                          /*upload_data_stream=*/nullptr, read_buffer.get(),
                          NetLogWithSource());

  // Send a request.
  HttpResponseInfo response;
  TestCompletionCallback callback;
  EXPECT_THAT(parser.SendRequest("GET / HTTP/1.1\r\n", HttpRequestHeaders(),
                                 TRAFFIC_ANNOTATION_FOR_TESTS, &response,
                                 callback.callback()),
              IsOk());

  EXPECT_THAT(parser.ReadResponseHeaders(callback.callback()),
              IsError(ERR_IO_PENDING));

  // [seq=1 --> seq=2] The parser reads the first fragment of the informational
  // response headers and then pauses to advance the mock clock.
  base::TimeTicks first_response_start_time = task_environment.NowTicks();
  sequenced_socket_data.RunUntilPaused();
  task_environment.AdvanceClock(base::Seconds(1));

  // [seq=3] The parser reads the second fragment of the informational response
  // headers.
  sequenced_socket_data.Resume();
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  // Check the received headers.
  EXPECT_EQ(100, response.headers->response_code());
  EXPECT_EQ(status100_response_headers_size, parser.received_bytes());

  EXPECT_THAT(parser.ReadResponseHeaders(callback.callback()),
              IsError(ERR_IO_PENDING));

  // [seq=4] The parser pauses to advance the clock.
  sequenced_socket_data.RunUntilPaused();
  task_environment.AdvanceClock(base::Seconds(1));

  // [seq=5 --> seq=6] The parser reads the first fragment of the
  // non-informational response headers and then pauses to advance the mock
  // clock.
  base::TimeTicks non_informational_response_start_time =
      task_environment.NowTicks();
  sequenced_socket_data.Resume();
  sequenced_socket_data.RunUntilPaused();
  task_environment.AdvanceClock(base::Seconds(1));

  // [seq=7] The parser reads the second fragment of the non-informational
  // response headers.
  sequenced_socket_data.Resume();
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  task_environment.AdvanceClock(base::Seconds(1));

  // Check the received headers.
  EXPECT_EQ(200, response.headers->response_code());
  EXPECT_EQ(status100_response_headers_size + response_headers_size,
            parser.received_bytes());

  // The first response start time should be captured at the time the first
  // fragment of the informational response headers is received.
  EXPECT_EQ(parser.first_response_start_time(), first_response_start_time);
  // The non-informational response start time should be captured at the time
  // the first fragment of the non-informational response headers is received.
  EXPECT_EQ(parser.non_informational_response_start_time(),
            non_informational_response_start_time);
  // The first response start time should be earlier than the non-informational
  // response start time.
  EXPECT_LT(parser.first_response_start_time(),
            parser.non_informational_response_start_time());

  // [seq=8] The parser reads the non-informational response body.
  auto body_buffer =
      base::MakeRefCounted<IOBufferWithSize>(response_body.size());
  int result = parser.ReadResponseBody(body_buffer.get(), response_body.size(),
                                       callback.callback());
  EXPECT_THAT(callback.GetResult(result), response_body.size());

  // Check the received body.
  EXPECT_EQ(response_size, parser.received_bytes());
}

// Test that "early hints" HTTP header is counted as "received_bytes".
// 103 Early Hints hasn't been implemented yet and should be ignored, but we
// collect timing information for the experiment (https://crbug.com/1093693).
TEST(HttpStreamParser, EarlyHints) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  std::string status103_response_headers1 = "HTTP/1.1 103 Early Hints\r\n";
  std::string status103_response_headers2 =
      "Link: </style.css>; rel=preload; as=style\r\n";
  std::string status103_response_headers3 =
      "Link: </script.js>; rel=preload; as=script\r\n\r\n";
  int64_t status103_response_headers_size = status103_response_headers1.size() +
                                            status103_response_headers2.size() +
                                            status103_response_headers3.size();

  std::string response_headers1 = "HTTP/1.1 200 OK\r\n";
  std::string response_headers2 = "Content-Length: 7\r\n\r\n";
  int64_t response_headers_size =
      response_headers1.size() + response_headers2.size();

  std::string response_body = "content";
  int64_t response_size = status103_response_headers_size +
                          response_headers_size + response_body.size();

  MockWrite writes[] = {MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n\r\n")};

  MockRead reads[] = {
      // Add pauses between header fragments so that the test runner can advance
      // the mock clock to test timing information.

      // 103 Early Hints response headers.
      MockRead(ASYNC, 1, status103_response_headers1.c_str()),
      MockRead(ASYNC, ERR_IO_PENDING, 2),
      MockRead(ASYNC, 3, status103_response_headers2.c_str()),
      MockRead(ASYNC, ERR_IO_PENDING, 4),
      MockRead(ASYNC, 5, status103_response_headers3.c_str()),
      MockRead(ASYNC, ERR_IO_PENDING, 6),

      // 200 response headers and body.
      MockRead(ASYNC, 7, response_headers1.c_str()),
      MockRead(ASYNC, ERR_IO_PENDING, 8),
      MockRead(ASYNC, 9, response_headers2.c_str()),
      MockRead(ASYNC, 10, response_body.c_str()),
  };

  // Set up the sequenced socket data.
  SequencedSocketData sequenced_socket_data(reads, writes);
  std::unique_ptr<StreamSocket> stream_socket =
      CreateConnectedSocket(&sequenced_socket_data);

  // Set up the http stream parser.
  auto read_buffer = base::MakeRefCounted<GrowableIOBuffer>();
  HttpStreamParser parser(stream_socket.get(), /*connection_is_reused=*/false,
                          GURL("http://localhost"), "GET",
                          /*upload_data_stream=*/nullptr, read_buffer.get(),
                          NetLogWithSource());

  // Send a request.
  HttpResponseInfo response;
  TestCompletionCallback callback;
  EXPECT_THAT(parser.SendRequest("GET / HTTP/1.1\r\n", HttpRequestHeaders(),
                                 TRAFFIC_ANNOTATION_FOR_TESTS, &response,
                                 callback.callback()),
              IsOk());

  EXPECT_THAT(parser.ReadResponseHeaders(callback.callback()),
              IsError(ERR_IO_PENDING));

  // [seq=1 --> seq=2] The parser reads the first fragment of the informational
  // response headers and then pauses to advance the mock clock.
  base::TimeTicks first_response_start_time = task_environment.NowTicks();
  sequenced_socket_data.RunUntilPaused();
  task_environment.AdvanceClock(base::Seconds(1));

  // [seq=3 --> seq=4] The parser reads the second fragment of the informational
  // response headers and then pauses to advance the mock clock.
  sequenced_socket_data.Resume();
  sequenced_socket_data.RunUntilPaused();
  task_environment.AdvanceClock(base::Seconds(1));

  // [seq=5] The parser reads the third fragment of the informational response
  // headers.
  sequenced_socket_data.Resume();
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  // Check the received headers.
  EXPECT_EQ(103, response.headers->response_code());
  EXPECT_EQ(status103_response_headers_size, parser.received_bytes());

  EXPECT_THAT(parser.ReadResponseHeaders(callback.callback()),
              IsError(ERR_IO_PENDING));

  // [seq=6] The parser pauses to advance the clock.
  sequenced_socket_data.RunUntilPaused();
  task_environment.AdvanceClock(base::Seconds(1));

  // [seq=7 --> seq=8] The parser reads the first fragment of the
  // non-informational response headers and then pauses to advance the mock
  // clock.
  base::TimeTicks non_informational_response_start_time =
      task_environment.NowTicks();
  sequenced_socket_data.Resume();
  sequenced_socket_data.RunUntilPaused();
  task_environment.AdvanceClock(base::Seconds(1));

  // [seq=9] The parser reads the second fragment of the non-informational
  // response headers.
  sequenced_socket_data.Resume();
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  task_environment.AdvanceClock(base::Seconds(1));

  // Check the received headers.
  EXPECT_EQ(200, response.headers->response_code());
  EXPECT_EQ(status103_response_headers_size + response_headers_size,
            parser.received_bytes());

  // The first response start time and first early hints time should be captured
  // at the time the first fragment of the informational response headers is
  // received.
  EXPECT_EQ(parser.first_response_start_time(), first_response_start_time);
  EXPECT_EQ(parser.first_early_hints_time(), first_response_start_time);
  // The non-informational response start time should be captured at the time
  // the first fragment of the non-informational response headers is received.
  EXPECT_EQ(parser.non_informational_response_start_time(),
            non_informational_response_start_time);
  // The first response start time should be earlier than the non-informational
  // response start time.
  EXPECT_LT(parser.first_response_start_time(),
            parser.non_informational_response_start_time());

  // [seq=10] The parser reads the non-informational response body.
  auto body_buffer =
      base::MakeRefCounted<IOBufferWithSize>(response_body.size());
  int result = parser.ReadResponseBody(body_buffer.get(), response_body.size(),
                                       callback.callback());
  EXPECT_THAT(callback.GetResult(result), response_body.size());

  // Check the received body.
  EXPECT_EQ(response_size, parser.received_bytes());
}

// Test the case where informational response headers and non-informational
// response headers are packed in the same fragment.
TEST(HttpStreamParser, MixedResponseHeaders) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  std::string status100_response_headers = "HTTP/1.1 100 ";
  std::string mixed_response_headers = "Continue\r\n\r\nHTTP/1.1 200 OK\r\n";
  std::string response_headers = "Content-Length: 7\r\n\r\n";
  int64_t status100_response_headers_size =
      status100_response_headers.size() + 12;
  int64_t response_headers_size = response_headers.size() + 17;

  std::string response_body = "content";
  int64_t response_size = status100_response_headers_size +
                          response_headers_size + response_body.size();

  MockWrite writes[] = {MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n\r\n")};

  MockRead reads[] = {
      // Add pauses between header fragments so that the test runner can advance
      // the mock clock to test timing information.

      // 100 response headers.
      MockRead(ASYNC, 1, status100_response_headers.c_str()),
      MockRead(ASYNC, ERR_IO_PENDING, 2),

      // Mixed response headers.
      MockRead(ASYNC, 3, mixed_response_headers.c_str()),
      MockRead(ASYNC, ERR_IO_PENDING, 4),

      // 200 response headers and body.
      MockRead(ASYNC, 5, response_headers.c_str()),
      MockRead(ASYNC, 6, response_body.c_str()),
  };

  // Set up the sequenced socket data.
  SequencedSocketData sequenced_socket_data(reads, writes);
  std::unique_ptr<StreamSocket> stream_socket =
      CreateConnectedSocket(&sequenced_socket_data);

  // Set up the http stream parser.
  auto read_buffer = base::MakeRefCounted<GrowableIOBuffer>();
  HttpStreamParser parser(stream_socket.get(), /*connection_is_reused=*/false,
                          GURL("http://localhost"), "GET",
                          /*upload_data_stream=*/nullptr, read_buffer.get(),
                          NetLogWithSource());

  // Send a request.
  HttpResponseInfo response;
  TestCompletionCallback callback;
  EXPECT_THAT(parser.SendRequest("GET / HTTP/1.1\r\n", HttpRequestHeaders(),
                                 TRAFFIC_ANNOTATION_FOR_TESTS, &response,
                                 callback.callback()),
              IsOk());

  EXPECT_THAT(parser.ReadResponseHeaders(callback.callback()),
              IsError(ERR_IO_PENDING));

  // [seq=1 --> seq=2] The parser reads the first fragment of the informational
  // response headers and then pauses to advance the mock clock.
  base::TimeTicks first_response_start_time = task_environment.NowTicks();
  sequenced_socket_data.RunUntilPaused();
  task_environment.AdvanceClock(base::Seconds(1));

  // [seq=3] The parser reads the second fragment of the informational response
  // headers.
  sequenced_socket_data.Resume();
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  // Check the received headers.
  EXPECT_EQ(100, response.headers->response_code());
  EXPECT_EQ(status100_response_headers_size, parser.received_bytes());

  EXPECT_THAT(parser.ReadResponseHeaders(callback.callback()),
              IsError(ERR_IO_PENDING));

  // [seq=3 --> seq=4] The parser reads the first fragment of the
  // non-informational response headers and then pauses to advance the mock
  // clock.
  base::TimeTicks non_informational_response_start_time =
      task_environment.NowTicks();
  sequenced_socket_data.RunUntilPaused();
  task_environment.AdvanceClock(base::Seconds(1));

  // [seq=5] The parser reads the second fragment of the non-informational
  // response headers.
  sequenced_socket_data.Resume();
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  // Check the received headers.
  EXPECT_EQ(200, response.headers->response_code());
  EXPECT_EQ(status100_response_headers_size + response_headers_size,
            parser.received_bytes());

  // The first response start time should be captured at the time the first
  // fragment of the informational response headers is received.
  EXPECT_EQ(parser.first_response_start_time(), first_response_start_time);
  // The non-informational response start time should be captured at the time
  // the first fragment of the non-informational response headers is received.
  EXPECT_EQ(parser.non_informational_response_start_time(),
            non_informational_response_start_time);
  // The first response start time should be earlier than the non-informational
  // response start time.
  EXPECT_LT(parser.first_response_start_time(),
            parser.non_informational_response_start_time());

  // [seq=6] The parser reads the non-informational response body.
  auto body_buffer =
      base::MakeRefCounted<IOBufferWithSize>(response_body.size());
  int result = parser.ReadResponseBody(body_buffer.get(), response_body.size(),
                                       callback.callback());
  EXPECT_THAT(callback.GetResult(result), response_body.size());

  // Check the received body.
  EXPECT_EQ(response_size, parser.received_bytes());
}

// Test that an HttpStreamParser can be read from after it's received headers
// and data structures owned by its owner have been deleted.  This happens
// when a ResponseBodyDrainer is used.
// Test that an HttpStreamParser can be read from after it's received headers
// and data structures owned by its owner have been deleted.  This happens
// when a ResponseBodyDrainer is used.
TEST(HttpStreamParser, ReadAfterUnownedObjectsDestroyed) {
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, 0,
                "GET /foo.html HTTP/1.1\r\n"
                "Content-Length: 3\r\n\r\n"
                "123"),
  };

  const int kBodySize = 1;
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 OK\r\n"),
      MockRead(SYNCHRONOUS, 2, "Content-Length: 1\r\n"),
      MockRead(SYNCHRONOUS, 3, "Connection: Keep-Alive\r\n\r\n"),
      MockRead(SYNCHRONOUS, 4, "1"),
      MockRead(SYNCHRONOUS, 0, 5),  // EOF
  };

  SequencedSocketData data(reads, writes);
  std::unique_ptr<StreamSocket> stream_socket = CreateConnectedSocket(&data);

  std::vector<std::unique_ptr<UploadElementReader>> element_readers;
  element_readers.push_back(std::make_unique<UploadBytesElementReader>(
      base::byte_span_from_cstring("123")));
  auto upload_data_stream =
      std::make_unique<ElementsUploadDataStream>(std::move(element_readers), 0);
  ASSERT_THAT(upload_data_stream->Init(TestCompletionCallback().callback(),
                                       NetLogWithSource()),
              IsOk());

  scoped_refptr<GrowableIOBuffer> read_buffer =
      base::MakeRefCounted<GrowableIOBuffer>();
  HttpStreamParser parser(stream_socket.get(), false /* is_reused */,
                          GURL("http://somewhere/foo.html"), "POST",
                          upload_data_stream.get(), read_buffer.get(),
                          NetLogWithSource());

  auto request_headers = std::make_unique<HttpRequestHeaders>();
  request_headers->SetHeader("Content-Length", "3");

  auto response_info = std::make_unique<HttpResponseInfo>();
  TestCompletionCallback callback;
  ASSERT_EQ(
      OK, parser.SendRequest("GET /foo.html HTTP/1.1\r\n", *request_headers,
                             TRAFFIC_ANNOTATION_FOR_TESTS, response_info.get(),
                             callback.callback()));
  ASSERT_THAT(parser.ReadResponseHeaders(callback.callback()), IsOk());

  // If the object that owns the HttpStreamParser is deleted, it takes the
  // objects passed to the HttpStreamParser with it.
  upload_data_stream.reset();
  request_headers.reset();
  response_info.reset();

  auto body_buffer = base::MakeRefCounted<IOBufferWithSize>(kBodySize);
  ASSERT_EQ(kBodySize, parser.ReadResponseBody(body_buffer.get(), kBodySize,
                                               callback.callback()));

  EXPECT_EQ(CountWriteBytes(writes), parser.sent_bytes());
  EXPECT_EQ(CountReadBytes(reads), parser.received_bytes());
}

// Case where one byte is received at a time.
TEST(HttpStreamParser, ReceiveOneByteAtATime) {
  constexpr std::string_view kResponseHeaders =
      "HTTP/1.0 200 OK\r\n"
      "Foo: Bar\r\n\r\n";
  constexpr std::string_view kResponseBody = "hi";

  SimpleGetRunner get_runner;
  for (size_t i = 0; i < kResponseHeaders.length(); ++i) {
    get_runner.AddRead(kResponseHeaders.substr(i, 1));
  }
  for (size_t i = 0; i < kResponseBody.length(); ++i) {
    get_runner.AddRead(kResponseBody.substr(i, 1));
  }
  // EOF
  get_runner.AddRead("");

  get_runner.SetupParserAndSendRequest();
  get_runner.ReadHeaders();
  std::string header_value;
  EXPECT_TRUE(get_runner.response_info()->headers->GetNormalizedHeader(
      "Foo", &header_value));
  EXPECT_EQ("Bar", header_value);
  int read_lengths[] = {1, 1, 0};
  EXPECT_EQ(kResponseBody,
            get_runner.ReadBody(kResponseBody.size(), read_lengths));
}

}  // namespace

}  // namespace net
