// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/server/http_server.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/server/http_server_request_info.h"
#include "net/socket/tcp_client_socket.h"
#include "net/socket/tcp_server_socket.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/websockets/websocket_frame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsOk;

namespace net {

namespace {

const int kMaxExpectedResponseLength = 2048;

class TestHttpClient {
 public:
  TestHttpClient() = default;

  int ConnectAndWait(const IPEndPoint& address) {
    AddressList addresses(address);
    NetLogSource source;
    socket_ = std::make_unique<TCPClientSocket>(addresses, nullptr, nullptr,
                                                nullptr, source);

    TestCompletionCallback callback;
    int rv = socket_->Connect(callback.callback());
    return callback.GetResult(rv);
  }

  void Send(const std::string& data) {
    write_buffer_ = base::MakeRefCounted<DrainableIOBuffer>(
        base::MakeRefCounted<StringIOBuffer>(data), data.length());
    Write();
  }

  bool Read(std::string* message, int expected_bytes) {
    int total_bytes_received = 0;
    message->clear();
    while (total_bytes_received < expected_bytes) {
      TestCompletionCallback callback;
      ReadInternal(&callback);
      int bytes_received = callback.WaitForResult();
      if (bytes_received <= 0) {
        return false;
      }

      total_bytes_received += bytes_received;
      message->append(read_buffer_->data(), bytes_received);
    }
    return true;
  }

  bool ReadResponse(std::string* message) {
    if (!Read(message, 1)) {
      return false;
    }
    while (!IsCompleteResponse(*message)) {
      std::string chunk;
      if (!Read(&chunk, 1)) {
        return false;
      }
      message->append(chunk);
    }
    return true;
  }

  void ExpectUsedThenDisconnectedWithNoData() {
    // Check that the socket was opened...
    ASSERT_TRUE(socket_->WasEverUsed());

    // ...then closed when the server disconnected. Verify that the socket was
    // closed by checking that a Read() fails.
    std::string response;
    ASSERT_FALSE(Read(&response, 1u));
    ASSERT_TRUE(response.empty());
  }

  TCPClientSocket& socket() { return *socket_; }

 private:
  void Write() {
    int result = socket_->Write(
        write_buffer_.get(), write_buffer_->BytesRemaining(),
        base::BindOnce(&TestHttpClient::OnWrite, base::Unretained(this)),
        TRAFFIC_ANNOTATION_FOR_TESTS);
    if (result != ERR_IO_PENDING) {
      OnWrite(result);
    }
  }

  void OnWrite(int result) {
    ASSERT_GT(result, 0);
    write_buffer_->DidConsume(result);
    if (write_buffer_->BytesRemaining()) {
      Write();
    }
  }

  void ReadInternal(TestCompletionCallback* callback) {
    read_buffer_ =
        base::MakeRefCounted<IOBufferWithSize>(kMaxExpectedResponseLength);
    int result = socket_->Read(read_buffer_.get(), kMaxExpectedResponseLength,
                               callback->callback());
    if (result != ERR_IO_PENDING) {
      callback->callback().Run(result);
    }
  }

  bool IsCompleteResponse(const std::string& response) {
    // Check end of headers first.
    size_t end_of_headers =
        HttpUtil::LocateEndOfHeaders(base::as_byte_span(response));
    if (end_of_headers == std::string::npos) {
      return false;
    }

    // Return true if response has data equal to or more than content length.
    int64_t body_size = static_cast<int64_t>(response.size()) - end_of_headers;
    DCHECK_LE(0, body_size);
    auto headers =
        base::MakeRefCounted<HttpResponseHeaders>(HttpUtil::AssembleRawHeaders(
            std::string_view(response.data(), end_of_headers)));
    return body_size >= headers->GetContentLength();
  }

  scoped_refptr<IOBufferWithSize> read_buffer_;
  scoped_refptr<DrainableIOBuffer> write_buffer_;
  std::unique_ptr<TCPClientSocket> socket_;
};

struct ReceivedRequest {
  HttpServerRequestInfo info;
  int connection_id;
};

}  // namespace

class HttpServerTest : public TestWithTaskEnvironment,
                       public HttpServer::Delegate {
 public:
  HttpServerTest() = default;

  void SetUp() override {
    auto server_socket =
        std::make_unique<TCPServerSocket>(nullptr, NetLogSource());
    server_socket->ListenWithAddressAndPort("127.0.0.1", 0, 1);
    server_ = std::make_unique<HttpServer>(std::move(server_socket), this);
    ASSERT_THAT(server_->GetLocalAddress(&server_address_), IsOk());
  }

  void TearDown() override {
    // Run the event loop some to make sure that the memory handed over to
    // DeleteSoon gets fully freed.
    base::RunLoop().RunUntilIdle();
  }

  void OnConnect(int connection_id) override {
    DCHECK(connection_map_.find(connection_id) == connection_map_.end());
    connection_map_[connection_id] = true;
    // This is set in CreateConnection(), which must be invoked once for every
    // expected connection.
    quit_on_create_loop_->Quit();
  }

  void OnHttpRequest(int connection_id,
                     const HttpServerRequestInfo& info) override {
    received_request_.SetValue({.info = info, .connection_id = connection_id});
  }

  void OnWebSocketRequest(int connection_id,
                          const HttpServerRequestInfo& info) override {
    NOTREACHED_IN_MIGRATION();
  }

  void OnWebSocketMessage(int connection_id, std::string data) override {
    NOTREACHED_IN_MIGRATION();
  }

  void OnClose(int connection_id) override {
    DCHECK(connection_map_.find(connection_id) != connection_map_.end());
    connection_map_[connection_id] = false;
    if (connection_id == quit_on_close_connection_) {
      std::move(run_loop_quit_func_).Run();
    }
  }

  ReceivedRequest WaitForRequest() { return received_request_.Take(); }

  bool HasRequest() const { return received_request_.IsReady(); }

  // Connections should only be created using this method, which waits until
  // both the server and the client have received the connected socket.
  void CreateConnection(TestHttpClient* client) {
    ASSERT_FALSE(quit_on_create_loop_);
    quit_on_create_loop_ = std::make_unique<base::RunLoop>();
    EXPECT_THAT(client->ConnectAndWait(server_address_), IsOk());
    quit_on_create_loop_->Run();
    quit_on_create_loop_.reset();
  }

  void RunUntilConnectionIdClosed(int connection_id) {
    quit_on_close_connection_ = connection_id;
    auto iter = connection_map_.find(connection_id);
    if (iter != connection_map_.end() && !iter->second) {
      // Already disconnected.
      return;
    }

    base::RunLoop run_loop;
    base::AutoReset<base::OnceClosure> run_loop_quit_func(
        &run_loop_quit_func_, run_loop.QuitClosure());
    run_loop.Run();

    iter = connection_map_.find(connection_id);
    ASSERT_TRUE(iter != connection_map_.end());
    ASSERT_FALSE(iter->second);
  }

  void HandleAcceptResult(std::unique_ptr<StreamSocket> socket) {
    ASSERT_FALSE(quit_on_create_loop_);
    quit_on_create_loop_ = std::make_unique<base::RunLoop>();
    server_->accepted_socket_ = std::move(socket);
    server_->HandleAcceptResult(OK);
    quit_on_create_loop_->Run();
    quit_on_create_loop_.reset();
  }

  std::unordered_map<int, bool>& connection_map() { return connection_map_; }

 protected:
  std::unique_ptr<HttpServer> server_;
  IPEndPoint server_address_;
  base::OnceClosure run_loop_quit_func_;
  std::unordered_map<int /* connection_id */, bool /* connected */>
      connection_map_;

 private:
  base::test::TestFuture<ReceivedRequest> received_request_;
  std::unique_ptr<base::RunLoop> quit_on_create_loop_;
  int quit_on_close_connection_ = -1;
};

namespace {

class WebSocketTest : public HttpServerTest {
  void OnHttpRequest(int connection_id,
                     const HttpServerRequestInfo& info) override {
    NOTREACHED_IN_MIGRATION();
  }

  void OnWebSocketRequest(int connection_id,
                          const HttpServerRequestInfo& info) override {
    HttpServerTest::OnHttpRequest(connection_id, info);
  }

  void OnWebSocketMessage(int connection_id, std::string data) override {}
};

class WebSocketAcceptingTest : public WebSocketTest {
 public:
  void OnWebSocketRequest(int connection_id,
                          const HttpServerRequestInfo& info) override {
    HttpServerTest::OnHttpRequest(connection_id, info);
    server_->AcceptWebSocket(connection_id, info, TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  void OnWebSocketMessage(int connection_id, std::string data) override {
    last_message_.SetValue(data);
  }

  std::string GetMessage() { return last_message_.Take(); }

 private:
  base::test::TestFuture<std::string> last_message_;
};

std::string EncodeFrame(std::string message,
                        WebSocketFrameHeader::OpCodeEnum op_code,
                        bool mask,
                        bool finish) {
  WebSocketFrameHeader header(op_code);
  header.final = finish;
  header.masked = mask;
  header.payload_length = message.size();
  const size_t header_size = GetWebSocketFrameHeaderSize(header);
  std::string frame_header;
  frame_header.resize(header_size);
  if (mask) {
    WebSocketMaskingKey masking_key = GenerateWebSocketMaskingKey();
    WriteWebSocketFrameHeader(header, &masking_key,
                              base::as_writable_byte_span(frame_header));
    MaskWebSocketFramePayload(masking_key, 0,
                              base::as_writable_byte_span(message));
  } else {
    WriteWebSocketFrameHeader(header, nullptr,
                              base::as_writable_byte_span(frame_header));
  }
  return frame_header + message;
}

TEST_F(HttpServerTest, Request) {
  TestHttpClient client;
  CreateConnection(&client);
  client.Send("GET /test HTTP/1.1\r\n\r\n");
  ReceivedRequest request = WaitForRequest();
  ASSERT_EQ("GET", request.info.method);
  ASSERT_EQ("/test", request.info.path);
  ASSERT_EQ("", request.info.data);
  ASSERT_EQ(0u, request.info.headers.size());
  ASSERT_TRUE(request.info.peer.ToString().starts_with("127.0.0.1"));
}

TEST_F(HttpServerTest, RequestBrokenTermination) {
  TestHttpClient client;
  CreateConnection(&client);
  client.Send("GET /test HTTP/1.1\r\n\r)");
  RunUntilConnectionIdClosed(1);
  EXPECT_FALSE(HasRequest());
  client.ExpectUsedThenDisconnectedWithNoData();
}

TEST_F(HttpServerTest, RequestWithHeaders) {
  TestHttpClient client;
  CreateConnection(&client);
  const char* const kHeaders[][3] = {
      {"Header", ": ", "1"},
      {"HeaderWithNoWhitespace", ":", "1"},
      {"HeaderWithWhitespace", "   :  \t   ", "1 1 1 \t  "},
      {"HeaderWithColon", ": ", "1:1"},
      {"EmptyHeader", ":", ""},
      {"EmptyHeaderWithWhitespace", ":  \t  ", ""},
      {"HeaderWithNonASCII", ":  ", "\xf7"},
  };
  std::string headers;
  for (const auto& header : kHeaders) {
    headers += std::string(header[0]) + header[1] + header[2] + "\r\n";
  }

  client.Send("GET /test HTTP/1.1\r\n" + headers + "\r\n");
  auto request = WaitForRequest();
  ASSERT_EQ("", request.info.data);

  for (const auto& header : kHeaders) {
    std::string field = base::ToLowerASCII(std::string(header[0]));
    std::string value = header[2];
    ASSERT_EQ(1u, request.info.headers.count(field)) << field;
    ASSERT_EQ(value, request.info.headers[field]) << header[0];
  }
}

TEST_F(HttpServerTest, RequestWithDuplicateHeaders) {
  TestHttpClient client;
  CreateConnection(&client);
  const char* const kHeaders[][3] = {
      // clang-format off
      {"FirstHeader", ": ", "1"},
      {"DuplicateHeader", ": ", "2"},
      {"MiddleHeader", ": ", "3"},
      {"DuplicateHeader", ": ", "4"},
      {"LastHeader", ": ", "5"},
      // clang-format on
  };
  std::string headers;
  for (const auto& header : kHeaders) {
    headers += std::string(header[0]) + header[1] + header[2] + "\r\n";
  }

  client.Send("GET /test HTTP/1.1\r\n" + headers + "\r\n");
  auto request = WaitForRequest();
  ASSERT_EQ("", request.info.data);

  for (const auto& header : kHeaders) {
    std::string field = base::ToLowerASCII(std::string(header[0]));
    std::string value = (field == "duplicateheader") ? "2,4" : header[2];
    ASSERT_EQ(1u, request.info.headers.count(field)) << field;
    ASSERT_EQ(value, request.info.headers[field]) << header[0];
  }
}

TEST_F(HttpServerTest, HasHeaderValueTest) {
  TestHttpClient client;
  CreateConnection(&client);
  const char* const kHeaders[] = {
      "Header: Abcd",
      "HeaderWithNoWhitespace:E",
      "HeaderWithWhitespace   :  \t   f \t  ",
      "DuplicateHeader: g",
      "HeaderWithComma: h, i ,j",
      "DuplicateHeader: k",
      "EmptyHeader:",
      "EmptyHeaderWithWhitespace:  \t  ",
      "HeaderWithNonASCII:  \xf7",
  };
  std::string headers;
  for (const char* header : kHeaders) {
    headers += std::string(header) + "\r\n";
  }

  client.Send("GET /test HTTP/1.1\r\n" + headers + "\r\n");
  auto request = WaitForRequest();
  ASSERT_EQ("", request.info.data);

  ASSERT_TRUE(request.info.HasHeaderValue("header", "abcd"));
  ASSERT_FALSE(request.info.HasHeaderValue("header", "bc"));
  ASSERT_TRUE(request.info.HasHeaderValue("headerwithnowhitespace", "e"));
  ASSERT_TRUE(request.info.HasHeaderValue("headerwithwhitespace", "f"));
  ASSERT_TRUE(request.info.HasHeaderValue("duplicateheader", "g"));
  ASSERT_TRUE(request.info.HasHeaderValue("headerwithcomma", "h"));
  ASSERT_TRUE(request.info.HasHeaderValue("headerwithcomma", "i"));
  ASSERT_TRUE(request.info.HasHeaderValue("headerwithcomma", "j"));
  ASSERT_TRUE(request.info.HasHeaderValue("duplicateheader", "k"));
  ASSERT_FALSE(request.info.HasHeaderValue("emptyheader", "x"));
  ASSERT_FALSE(request.info.HasHeaderValue("emptyheaderwithwhitespace", "x"));
  ASSERT_TRUE(request.info.HasHeaderValue("headerwithnonascii", "\xf7"));
}

TEST_F(HttpServerTest, RequestWithBody) {
  TestHttpClient client;
  CreateConnection(&client);
  std::string body = "a" + std::string(1 << 10, 'b') + "c";
  client.Send(
      base::StringPrintf("GET /test HTTP/1.1\r\n"
                         "SomeHeader: 1\r\n"
                         "Content-Length: %" PRIuS "\r\n\r\n%s",
                         body.length(), body.c_str()));
  auto request = WaitForRequest();
  ASSERT_EQ(2u, request.info.headers.size());
  ASSERT_EQ(body.length(), request.info.data.length());
  ASSERT_EQ('a', body[0]);
  ASSERT_EQ('c', *body.rbegin());
}

// Tests that |HttpServer::HandleReadResult| will ignore Upgrade header if value
// is not WebSocket.
TEST_F(HttpServerTest, UpgradeIgnored) {
  TestHttpClient client;
  CreateConnection(&client);
  client.Send(
      "GET /test HTTP/1.1\r\n"
      "Upgrade: h2c\r\n"
      "Connection: SomethingElse, Upgrade\r\n"
      "\r\n");
  WaitForRequest();
}

TEST_F(WebSocketTest, RequestWebSocket) {
  TestHttpClient client;
  CreateConnection(&client);
  client.Send(
      "GET /test HTTP/1.1\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: SomethingElse, Upgrade\r\n"
      "Sec-WebSocket-Version: 8\r\n"
      "Sec-WebSocket-Key: key\r\n"
      "\r\n");
  WaitForRequest();
}

TEST_F(WebSocketTest, RequestWebSocketTrailingJunk) {
  TestHttpClient client;
  CreateConnection(&client);
  client.Send(
      "GET /test HTTP/1.1\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: SomethingElse, Upgrade\r\n"
      "Sec-WebSocket-Version: 8\r\n"
      "Sec-WebSocket-Key: key\r\n"
      "\r\nHello? Anyone");
  RunUntilConnectionIdClosed(1);
  client.ExpectUsedThenDisconnectedWithNoData();
}

TEST_F(WebSocketAcceptingTest, SendPingFrameWithNoMessage) {
  TestHttpClient client;
  CreateConnection(&client);
  std::string response;
  client.Send(
      "GET /test HTTP/1.1\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: SomethingElse, Upgrade\r\n"
      "Sec-WebSocket-Version: 8\r\n"
      "Sec-WebSocket-Key: key\r\n\r\n");
  WaitForRequest();
  ASSERT_TRUE(client.ReadResponse(&response));
  const std::string message = "";
  const std::string ping_frame =
      EncodeFrame(message, WebSocketFrameHeader::OpCodeEnum::kOpCodePing,
                  /* mask= */ true, /* finish= */ true);
  const std::string pong_frame =
      EncodeFrame(message, WebSocketFrameHeader::OpCodeEnum::kOpCodePong,
                  /* mask= */ false, /* finish= */ true);
  client.Send(ping_frame);
  ASSERT_TRUE(client.Read(&response, pong_frame.length()));
  EXPECT_EQ(response, pong_frame);
}

TEST_F(WebSocketAcceptingTest, SendPingFrameWithMessage) {
  TestHttpClient client;
  CreateConnection(&client);
  std::string response;
  client.Send(
      "GET /test HTTP/1.1\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: SomethingElse, Upgrade\r\n"
      "Sec-WebSocket-Version: 8\r\n"
      "Sec-WebSocket-Key: key\r\n\r\n");
  WaitForRequest();
  ASSERT_TRUE(client.ReadResponse(&response));
  const std::string message = "hello";
  const std::string ping_frame =
      EncodeFrame(message, WebSocketFrameHeader::OpCodeEnum::kOpCodePing,
                  /* mask= */ true, /* finish= */ true);
  const std::string pong_frame =
      EncodeFrame(message, WebSocketFrameHeader::OpCodeEnum::kOpCodePong,
                  /* mask= */ false, /* finish= */ true);
  client.Send(ping_frame);
  ASSERT_TRUE(client.Read(&response, pong_frame.length()));
  EXPECT_EQ(response, pong_frame);
}

TEST_F(WebSocketAcceptingTest, SendPongFrame) {
  TestHttpClient client;
  CreateConnection(&client);
  std::string response;
  client.Send(
      "GET /test HTTP/1.1\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: SomethingElse, Upgrade\r\n"
      "Sec-WebSocket-Version: 8\r\n"
      "Sec-WebSocket-Key: key\r\n\r\n");
  WaitForRequest();
  ASSERT_TRUE(client.ReadResponse(&response));
  const std::string ping_frame = EncodeFrame(
      /* message= */ "", WebSocketFrameHeader::OpCodeEnum::kOpCodePing,
      /* mask= */ true, /* finish= */ true);
  const std::string pong_frame_send = EncodeFrame(
      /* message= */ "", WebSocketFrameHeader::OpCodeEnum::kOpCodePong,
      /* mask= */ true, /* finish= */ true);
  const std::string pong_frame_receive = EncodeFrame(
      /* message= */ "", WebSocketFrameHeader::OpCodeEnum::kOpCodePong,
      /* mask= */ false, /* finish= */ true);
  client.Send(pong_frame_send);
  client.Send(ping_frame);
  ASSERT_TRUE(client.Read(&response, pong_frame_receive.length()));
  EXPECT_EQ(response, pong_frame_receive);
}

TEST_F(WebSocketAcceptingTest, SendLongTextFrame) {
  TestHttpClient client;
  CreateConnection(&client);
  std::string response;
  client.Send(
      "GET /test HTTP/1.1\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: SomethingElse, Upgrade\r\n"
      "Sec-WebSocket-Version: 8\r\n"
      "Sec-WebSocket-Key: key\r\n\r\n");
  WaitForRequest();
  ASSERT_TRUE(client.ReadResponse(&response));
  constexpr int kFrameSize = 100000;
  const std::string text_frame(kFrameSize, 'a');
  const std::string continuation_frame(kFrameSize, 'b');
  const std::string text_encoded_frame =
      EncodeFrame(text_frame, WebSocketFrameHeader::OpCodeEnum::kOpCodeText,
                  /* mask= */ true,
                  /* finish= */ false);
  const std::string continuation_encoded_frame = EncodeFrame(
      continuation_frame, WebSocketFrameHeader::OpCodeEnum::kOpCodeContinuation,
      /* mask= */ true, /* finish= */ true);
  client.Send(text_encoded_frame);
  client.Send(continuation_encoded_frame);
  std::string received_message = GetMessage();
  EXPECT_EQ(received_message.size(),
            text_frame.size() + continuation_frame.size());
  EXPECT_EQ(received_message, text_frame + continuation_frame);
}

TEST_F(WebSocketAcceptingTest, SendTwoTextFrame) {
  TestHttpClient client;
  CreateConnection(&client);
  std::string response;
  client.Send(
      "GET /test HTTP/1.1\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: SomethingElse, Upgrade\r\n"
      "Sec-WebSocket-Version: 8\r\n"
      "Sec-WebSocket-Key: key\r\n\r\n");
  WaitForRequest();
  ASSERT_TRUE(client.ReadResponse(&response));
  const std::string text_frame_first = "foo";
  const std::string continuation_frame_first = "bar";
  const std::string text_encoded_frame_first = EncodeFrame(
      text_frame_first, WebSocketFrameHeader::OpCodeEnum::kOpCodeText,
      /* mask= */ true,
      /* finish= */ false);
  const std::string continuation_encoded_frame_first =
      EncodeFrame(continuation_frame_first,
                  WebSocketFrameHeader::OpCodeEnum::kOpCodeContinuation,
                  /* mask= */ true, /* finish= */ true);

  const std::string text_frame_second = "FOO";
  const std::string continuation_frame_second = "BAR";
  const std::string text_encoded_frame_second = EncodeFrame(
      text_frame_second, WebSocketFrameHeader::OpCodeEnum::kOpCodeText,
      /* mask= */ true,
      /* finish= */ false);
  const std::string continuation_encoded_frame_second =
      EncodeFrame(continuation_frame_second,
                  WebSocketFrameHeader::OpCodeEnum::kOpCodeContinuation,
                  /* mask= */ true, /* finish= */ true);

  // text_encoded_frame_first -> text_encoded_frame_second
  client.Send(text_encoded_frame_first);
  client.Send(continuation_encoded_frame_first);
  std::string received_message = GetMessage();
  EXPECT_EQ(received_message, "foobar");
  client.Send(text_encoded_frame_second);
  client.Send(continuation_encoded_frame_second);
  received_message = GetMessage();
  EXPECT_EQ(received_message, "FOOBAR");
}

TEST_F(WebSocketAcceptingTest, SendPingPongFrame) {
  TestHttpClient client;
  CreateConnection(&client);
  std::string response;
  client.Send(
      "GET /test HTTP/1.1\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: SomethingElse, Upgrade\r\n"
      "Sec-WebSocket-Version: 8\r\n"
      "Sec-WebSocket-Key: key\r\n\r\n");
  WaitForRequest();
  ASSERT_TRUE(client.ReadResponse(&response));

  const std::string ping_message_first = "";
  const std::string ping_frame_first = EncodeFrame(
      ping_message_first, WebSocketFrameHeader::OpCodeEnum::kOpCodePing,
      /* mask= */ true, /* finish= */ true);
  const std::string pong_frame_receive_first = EncodeFrame(
      ping_message_first, WebSocketFrameHeader::OpCodeEnum::kOpCodePong,
      /* mask= */ false, /* finish= */ true);
  const std::string pong_frame_send = EncodeFrame(
      /* message= */ "", WebSocketFrameHeader::OpCodeEnum::kOpCodePong,
      /* mask= */ true, /* finish= */ true);
  const std::string ping_message_second = "hello";
  const std::string ping_frame_second = EncodeFrame(
      ping_message_second, WebSocketFrameHeader::OpCodeEnum::kOpCodePing,
      /* mask= */ true, /* finish= */ true);
  const std::string pong_frame_receive_second = EncodeFrame(
      ping_message_second, WebSocketFrameHeader::OpCodeEnum::kOpCodePong,
      /* mask= */ false, /* finish= */ true);

  // ping_frame_first -> pong_frame_send -> ping_frame_second
  client.Send(ping_frame_first);
  ASSERT_TRUE(client.Read(&response, pong_frame_receive_first.length()));
  EXPECT_EQ(response, pong_frame_receive_first);
  client.Send(pong_frame_send);
  client.Send(ping_frame_second);
  ASSERT_TRUE(client.Read(&response, pong_frame_receive_second.length()));
  EXPECT_EQ(response, pong_frame_receive_second);
}

TEST_F(WebSocketAcceptingTest, SendTextAndPingFrame) {
  TestHttpClient client;
  CreateConnection(&client);
  std::string response;
  client.Send(
      "GET /test HTTP/1.1\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: SomethingElse, Upgrade\r\n"
      "Sec-WebSocket-Version: 8\r\n"
      "Sec-WebSocket-Key: key\r\n\r\n");
  WaitForRequest();
  ASSERT_TRUE(client.ReadResponse(&response));

  const std::string text_frame = "foo";
  const std::string continuation_frame = "bar";
  const std::string text_encoded_frame =
      EncodeFrame(text_frame, WebSocketFrameHeader::OpCodeEnum::kOpCodeText,
                  /* mask= */ true,
                  /* finish= */ false);
  const std::string continuation_encoded_frame = EncodeFrame(
      continuation_frame, WebSocketFrameHeader::OpCodeEnum::kOpCodeContinuation,
      /* mask= */ true, /* finish= */ true);
  const std::string ping_message = "ping";
  const std::string ping_frame =
      EncodeFrame(ping_message, WebSocketFrameHeader::OpCodeEnum::kOpCodePing,
                  /* mask= */ true, /* finish= */ true);
  const std::string pong_frame =
      EncodeFrame(ping_message, WebSocketFrameHeader::OpCodeEnum::kOpCodePong,
                  /* mask= */ false, /* finish= */ true);

  // text_encoded_frame -> ping_frame -> continuation_encoded_frame
  client.Send(text_encoded_frame);
  client.Send(ping_frame);
  client.Send(continuation_encoded_frame);
  ASSERT_TRUE(client.Read(&response, pong_frame.length()));
  EXPECT_EQ(response, pong_frame);
  std::string received_message = GetMessage();
  EXPECT_EQ(received_message, "foobar");
}

TEST_F(WebSocketAcceptingTest, SendTextAndPingFrameWithMessage) {
  TestHttpClient client;
  CreateConnection(&client);
  std::string response;
  client.Send(
      "GET /test HTTP/1.1\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: SomethingElse, Upgrade\r\n"
      "Sec-WebSocket-Version: 8\r\n"
      "Sec-WebSocket-Key: key\r\n\r\n");
  WaitForRequest();
  ASSERT_TRUE(client.ReadResponse(&response));

  const std::string text_frame = "foo";
  const std::string continuation_frame = "bar";
  const std::string text_encoded_frame =
      EncodeFrame(text_frame, WebSocketFrameHeader::OpCodeEnum::kOpCodeText,
                  /* mask= */ true,
                  /* finish= */ false);
  const std::string continuation_encoded_frame = EncodeFrame(
      continuation_frame, WebSocketFrameHeader::OpCodeEnum::kOpCodeContinuation,
      /* mask= */ true, /* finish= */ true);
  const std::string ping_message = "hello";
  const std::string ping_frame =
      EncodeFrame(ping_message, WebSocketFrameHeader::OpCodeEnum::kOpCodePing,
                  /* mask= */ true, /* finish= */ true);
  const std::string pong_frame =
      EncodeFrame(ping_message, WebSocketFrameHeader::OpCodeEnum::kOpCodePong,
                  /* mask= */ false, /* finish= */ true);

  // text_encoded_frame -> ping_frame -> continuation_frame
  client.Send(text_encoded_frame);
  client.Send(ping_frame);
  client.Send(continuation_encoded_frame);
  ASSERT_TRUE(client.Read(&response, pong_frame.length()));
  EXPECT_EQ(response, pong_frame);
  std::string received_message = GetMessage();
  EXPECT_EQ(received_message, "foobar");
}

TEST_F(WebSocketAcceptingTest, SendTextAndPongFrame) {
  TestHttpClient client;
  CreateConnection(&client);
  std::string response;
  client.Send(
      "GET /test HTTP/1.1\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: SomethingElse, Upgrade\r\n"
      "Sec-WebSocket-Version: 8\r\n"
      "Sec-WebSocket-Key: key\r\n\r\n");
  WaitForRequest();
  ASSERT_TRUE(client.ReadResponse(&response));

  const std::string text_frame = "foo";
  const std::string continuation_frame = "bar";
  const std::string text_encoded_frame =
      EncodeFrame(text_frame, WebSocketFrameHeader::OpCodeEnum::kOpCodeText,
                  /* mask= */ true,
                  /* finish= */ false);
  const std::string continuation_encoded_frame = EncodeFrame(
      continuation_frame, WebSocketFrameHeader::OpCodeEnum::kOpCodeContinuation,
      /* mask= */ true, /* finish= */ true);
  const std::string pong_message = "pong";
  const std::string pong_frame =
      EncodeFrame(pong_message, WebSocketFrameHeader::OpCodeEnum::kOpCodePong,
                  /* mask= */ true, /* finish= */ true);

  // text_encoded_frame -> pong_frame -> continuation_encoded_frame
  client.Send(text_encoded_frame);
  client.Send(pong_frame);
  client.Send(continuation_encoded_frame);
  std::string received_message = GetMessage();
  EXPECT_EQ(received_message, "foobar");
}

TEST_F(WebSocketAcceptingTest, SendTextPingPongFrame) {
  TestHttpClient client;
  CreateConnection(&client);
  std::string response;
  client.Send(
      "GET /test HTTP/1.1\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: SomethingElse, Upgrade\r\n"
      "Sec-WebSocket-Version: 8\r\n"
      "Sec-WebSocket-Key: key\r\n\r\n");
  WaitForRequest();
  ASSERT_TRUE(client.ReadResponse(&response));

  const std::string text_frame = "foo";
  const std::string continuation_frame = "bar";
  const std::string text_encoded_frame =
      EncodeFrame(text_frame, WebSocketFrameHeader::OpCodeEnum::kOpCodeText,
                  /* mask= */ true,
                  /* finish= */ false);
  const std::string continuation_encoded_frame = EncodeFrame(
      continuation_frame, WebSocketFrameHeader::OpCodeEnum::kOpCodeContinuation,
      /* mask= */ true, /* finish= */ true);

  const std::string ping_message_first = "hello";
  const std::string ping_frame_first = EncodeFrame(
      ping_message_first, WebSocketFrameHeader::OpCodeEnum::kOpCodePing,
      /* mask= */ true, /* finish= */ true);
  const std::string pong_frame_first = EncodeFrame(
      ping_message_first, WebSocketFrameHeader::OpCodeEnum::kOpCodePong,
      /* mask= */ false, /* finish= */ true);

  const std::string ping_message_second = "HELLO";
  const std::string ping_frame_second = EncodeFrame(
      ping_message_second, WebSocketFrameHeader::OpCodeEnum::kOpCodePing,
      /* mask= */ true, /* finish= */ true);
  const std::string pong_frame_second = EncodeFrame(
      ping_message_second, WebSocketFrameHeader::OpCodeEnum::kOpCodePong,
      /* mask= */ false, /* finish= */ true);

  // text_encoded_frame -> ping_frame_first -> ping_frame_second ->
  // continuation_encoded_frame
  client.Send(text_encoded_frame);
  client.Send(ping_frame_first);
  ASSERT_TRUE(client.Read(&response, pong_frame_first.length()));
  EXPECT_EQ(response, pong_frame_first);
  client.Send(ping_frame_second);
  ASSERT_TRUE(client.Read(&response, pong_frame_second.length()));
  EXPECT_EQ(response, pong_frame_second);
  client.Send(continuation_encoded_frame);
  std::string received_message = GetMessage();
  EXPECT_EQ(received_message, "foobar");
}

TEST_F(HttpServerTest, RequestWithTooLargeBody) {
  TestHttpClient client;
  CreateConnection(&client);
  client.Send(
      "GET /test HTTP/1.1\r\n"
      "Content-Length: 1073741824\r\n\r\n");
  std::string response;
  ASSERT_TRUE(client.ReadResponse(&response));
  EXPECT_EQ(
      "HTTP/1.1 500 Internal Server Error\r\n"
      "Content-Length:42\r\n"
      "Content-Type:text/html\r\n\r\n"
      "request content-length too big or unknown.",
      response);
}

TEST_F(HttpServerTest, Send200) {
  TestHttpClient client;
  CreateConnection(&client);
  client.Send("GET /test HTTP/1.1\r\n\r\n");
  auto request = WaitForRequest();
  server_->Send200(request.connection_id, "Response!", "text/plain",
                   TRAFFIC_ANNOTATION_FOR_TESTS);

  std::string response;
  ASSERT_TRUE(client.ReadResponse(&response));
  ASSERT_TRUE(response.starts_with("HTTP/1.1 200 OK"));
  ASSERT_TRUE(response.ends_with("Response!"));
}

TEST_F(HttpServerTest, SendRaw) {
  TestHttpClient client;
  CreateConnection(&client);
  client.Send("GET /test HTTP/1.1\r\n\r\n");
  auto request = WaitForRequest();
  server_->SendRaw(request.connection_id, "Raw Data ",
                   TRAFFIC_ANNOTATION_FOR_TESTS);
  server_->SendRaw(request.connection_id, "More Data",
                   TRAFFIC_ANNOTATION_FOR_TESTS);
  server_->SendRaw(request.connection_id, "Third Piece of Data",
                   TRAFFIC_ANNOTATION_FOR_TESTS);

  const std::string expected_response("Raw Data More DataThird Piece of Data");
  std::string response;
  ASSERT_TRUE(client.Read(&response, expected_response.length()));
  ASSERT_EQ(expected_response, response);
}

TEST_F(HttpServerTest, WrongProtocolRequest) {
  const char* const kBadProtocolRequests[] = {
      "GET /test HTTP/1.0\r\n\r\n",
      "GET /test foo\r\n\r\n",
      "GET /test \r\n\r\n",
  };

  for (const char* bad_request : kBadProtocolRequests) {
    TestHttpClient client;
    CreateConnection(&client);

    client.Send(bad_request);
    client.ExpectUsedThenDisconnectedWithNoData();

    // Assert that the delegate was updated properly.
    ASSERT_EQ(1u, connection_map().size());
    ASSERT_FALSE(connection_map().begin()->second);
    EXPECT_FALSE(HasRequest());

    // Reset the state of the connection map.
    connection_map().clear();
  }
}

// A null byte in the headers should cause the request to be rejected.
TEST_F(HttpServerTest, NullByteInHeaders) {
  constexpr char kNullByteInHeader[] =
      "GET / HTTP/1.1\r\n"
      "User-Agent: Mozilla\0/\r\n"
      "\r\n";
  TestHttpClient client;
  CreateConnection(&client);

  client.Send(std::string(kNullByteInHeader, std::size(kNullByteInHeader) - 1));
  client.ExpectUsedThenDisconnectedWithNoData();

  ASSERT_EQ(1u, connection_map().size());
  ASSERT_FALSE(connection_map().begin()->second);
  EXPECT_FALSE(HasRequest());
}

// A null byte in the body should be accepted.
TEST_F(HttpServerTest, NullByteInBody) {
  // We use the trailing null byte added by the compiler as the "body" of the
  // request.
  constexpr char kNullByteInBody[] =
      "POST /body HTTP/1.1\r\n"
      "User-Agent: Mozilla\r\n"
      "Content-Length: 1\r\n"
      "\r\n";
  TestHttpClient client;
  CreateConnection(&client);

  client.Send(std::string(kNullByteInBody, std::size(kNullByteInBody)));
  auto request = WaitForRequest();
  EXPECT_EQ(request.info.data, std::string_view("\0", 1));
}

class MockStreamSocket : public StreamSocket {
 public:
  MockStreamSocket() = default;

  MockStreamSocket(const MockStreamSocket&) = delete;
  MockStreamSocket& operator=(const MockStreamSocket&) = delete;

  ~MockStreamSocket() override = default;

  // StreamSocket
  int Connect(CompletionOnceCallback callback) override {
    return ERR_NOT_IMPLEMENTED;
  }
  void Disconnect() override {
    connected_ = false;
    if (!read_callback_.is_null()) {
      read_buf_ = nullptr;
      read_buf_len_ = 0;
      std::move(read_callback_).Run(ERR_CONNECTION_CLOSED);
    }
  }
  bool IsConnected() const override { return connected_; }
  bool IsConnectedAndIdle() const override { return IsConnected(); }
  int GetPeerAddress(IPEndPoint* address) const override {
    return ERR_NOT_IMPLEMENTED;
  }
  int GetLocalAddress(IPEndPoint* address) const override {
    return ERR_NOT_IMPLEMENTED;
  }
  const NetLogWithSource& NetLog() const override { return net_log_; }
  bool WasEverUsed() const override { return true; }
  NextProto GetNegotiatedProtocol() const override { return kProtoUnknown; }
  bool GetSSLInfo(SSLInfo* ssl_info) override { return false; }
  int64_t GetTotalReceivedBytes() const override {
    NOTIMPLEMENTED();
    return 0;
  }
  void ApplySocketTag(const SocketTag& tag) override {}

  // Socket
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override {
    if (!connected_) {
      return ERR_SOCKET_NOT_CONNECTED;
    }
    if (pending_read_data_.empty()) {
      read_buf_ = buf;
      read_buf_len_ = buf_len;
      read_callback_ = std::move(callback);
      return ERR_IO_PENDING;
    }
    DCHECK_GT(buf_len, 0);
    int read_len =
        std::min(static_cast<int>(pending_read_data_.size()), buf_len);
    memcpy(buf->data(), pending_read_data_.data(), read_len);
    pending_read_data_.erase(0, read_len);
    return read_len;
  }

  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override {
    return ERR_NOT_IMPLEMENTED;
  }
  int SetReceiveBufferSize(int32_t size) override {
    return ERR_NOT_IMPLEMENTED;
  }
  int SetSendBufferSize(int32_t size) override { return ERR_NOT_IMPLEMENTED; }

  void DidRead(const char* data, int data_len) {
    if (!read_buf_.get()) {
      pending_read_data_.append(data, data_len);
      return;
    }
    int read_len = std::min(data_len, read_buf_len_);
    memcpy(read_buf_->data(), data, read_len);
    pending_read_data_.assign(data + read_len, data_len - read_len);
    read_buf_ = nullptr;
    read_buf_len_ = 0;
    std::move(read_callback_).Run(read_len);
  }

 private:
  bool connected_ = true;
  scoped_refptr<IOBuffer> read_buf_;
  int read_buf_len_ = 0;
  CompletionOnceCallback read_callback_;
  std::string pending_read_data_;
  NetLogWithSource net_log_;
};

TEST_F(HttpServerTest, RequestWithBodySplitAcrossPackets) {
  auto socket = std::make_unique<MockStreamSocket>();
  auto* socket_ptr = socket.get();
  HandleAcceptResult(std::move(socket));
  std::string body("body");
  std::string request_text = base::StringPrintf(
      "GET /test HTTP/1.1\r\n"
      "SomeHeader: 1\r\n"
      "Content-Length: %" PRIuS "\r\n\r\n%s",
      body.length(), body.c_str());
  socket_ptr->DidRead(request_text.c_str(), request_text.length() - 2);
  ASSERT_FALSE(HasRequest());
  socket_ptr->DidRead(request_text.c_str() + request_text.length() - 2, 2);
  ASSERT_TRUE(HasRequest());
  ASSERT_EQ(body, WaitForRequest().info.data);
}

TEST_F(HttpServerTest, MultipleRequestsOnSameConnection) {
  // The idea behind this test is that requests with or without bodies should
  // not break parsing of the next request.
  TestHttpClient client;
  CreateConnection(&client);
  std::string body = "body";
  client.Send(
      base::StringPrintf("GET /test HTTP/1.1\r\n"
                         "Content-Length: %" PRIuS "\r\n\r\n%s",
                         body.length(), body.c_str()));
  auto first_request = WaitForRequest();
  ASSERT_EQ(body, first_request.info.data);

  int client_connection_id = first_request.connection_id;
  server_->Send200(client_connection_id, "Content for /test", "text/plain",
                   TRAFFIC_ANNOTATION_FOR_TESTS);
  std::string response1;
  ASSERT_TRUE(client.ReadResponse(&response1));
  ASSERT_TRUE(response1.starts_with("HTTP/1.1 200 OK"));
  ASSERT_TRUE(response1.ends_with("Content for /test"));

  client.Send("GET /test2 HTTP/1.1\r\n\r\n");
  auto second_request = WaitForRequest();
  ASSERT_EQ("/test2", second_request.info.path);

  ASSERT_EQ(client_connection_id, second_request.connection_id);
  server_->Send404(client_connection_id, TRAFFIC_ANNOTATION_FOR_TESTS);
  std::string response2;
  ASSERT_TRUE(client.ReadResponse(&response2));
  ASSERT_TRUE(response2.starts_with("HTTP/1.1 404 Not Found"));

  client.Send("GET /test3 HTTP/1.1\r\n\r\n");
  auto third_request = WaitForRequest();
  ASSERT_EQ("/test3", third_request.info.path);

  ASSERT_EQ(client_connection_id, third_request.connection_id);
  server_->Send200(client_connection_id, "Content for /test3", "text/plain",
                   TRAFFIC_ANNOTATION_FOR_TESTS);
  std::string response3;
  ASSERT_TRUE(client.ReadResponse(&response3));
  ASSERT_TRUE(response3.starts_with("HTTP/1.1 200 OK"));
  ASSERT_TRUE(response3.ends_with("Content for /test3"));
}

class CloseOnConnectHttpServerTest : public HttpServerTest {
 public:
  void OnConnect(int connection_id) override {
    HttpServerTest::OnConnect(connection_id);
    connection_ids_.push_back(connection_id);
    server_->Close(connection_id);
  }

 protected:
  std::vector<int> connection_ids_;
};

TEST_F(CloseOnConnectHttpServerTest, ServerImmediatelyClosesConnection) {
  TestHttpClient client;
  CreateConnection(&client);
  client.Send("GET / HTTP/1.1\r\n\r\n");

  // The server should close the socket without responding.
  client.ExpectUsedThenDisconnectedWithNoData();

  // Run any tasks the TestServer posted.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1ul, connection_ids_.size());
  // OnHttpRequest() should never have been called, since the connection was
  // closed without reading from it.
  EXPECT_FALSE(HasRequest());
}

}  // namespace

}  // namespace net
