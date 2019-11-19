// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/server/http_server.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_response_headers.h"
#include "net/log/net_log_source.h"
#include "net/test/gtest_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/server/http_connection.h"
#include "services/network/public/cpp/server/http_server_request_info.h"
#include "services/network/socket_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsOk;

namespace {

class TestHttpClient {
 public:
  TestHttpClient() : factory_(nullptr, &url_request_context_) {}

  int ConnectAndWait(const net::IPEndPoint& address) {
    net::AddressList addresses(address);
    base::RunLoop run_loop;
    int net_error = net::ERR_FAILED;
    factory_.CreateTCPConnectedSocket(
        base::nullopt /* local address */, addresses,
        nullptr /* tcp_connected_socket_options */,
        TRAFFIC_ANNOTATION_FOR_TESTS, socket_.BindNewPipeAndPassReceiver(),
        mojo::NullRemote() /* observer */,
        base::BindOnce(
            [](base::RunLoop* run_loop, int* result_out,
               mojo::ScopedDataPipeConsumerHandle* receive_pipe_handle_out,
               mojo::ScopedDataPipeProducerHandle* send_pipe_handle_out,
               int result, const base::Optional<net::IPEndPoint>& local_addr,
               const base::Optional<net::IPEndPoint>& peer_addr,
               mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
               mojo::ScopedDataPipeProducerHandle send_pipe_handle) {
              *receive_pipe_handle_out = std::move(receive_pipe_handle);
              *send_pipe_handle_out = std::move(send_pipe_handle);
              *result_out = result;
              run_loop->Quit();
            },
            base::Unretained(&run_loop), base::Unretained(&net_error),
            &receive_pipe_handle_, &send_pipe_handle_));
    run_loop.Run();
    return net_error;
  }

  void Send(const std::string& message) {
    size_t index = 0;
    uint32_t write_size = message.size();
    while (write_size > 0) {
      base::RunLoop().RunUntilIdle();
      MojoResult result = send_pipe_handle_->WriteData(
          message.data() + index, &write_size, MOJO_WRITE_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_SHOULD_WAIT)
        continue;
      if (result != MOJO_RESULT_OK)
        return;
      index += write_size;
      write_size = message.size() - index;
    }
  }

  bool Read(std::string* data, size_t num_bytes) {
    while (data->size() < num_bytes) {
      base::RunLoop().RunUntilIdle();
      std::vector<char> buffer(num_bytes);
      uint32_t read_size = num_bytes;
      MojoResult result = receive_pipe_handle_->ReadData(
          buffer.data(), &read_size, MOJO_READ_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_SHOULD_WAIT)
        continue;
      if (result != MOJO_RESULT_OK)
        return false;
      data->append(buffer.data(), read_size);
    }
    return true;
  }

  bool ReadResponse(std::string* message) {
    if (!Read(message, 1))
      return false;
    while (!IsCompleteResponse(*message)) {
      std::string chunk;
      if (!Read(&chunk, 1))
        return false;
      message->append(chunk);
    }
    return true;
  }

  void ExpectUsedThenDisconnectedWithNoData() {
    // Check that the socket was closed when the server disconnected. Verify
    // that the socket was closed by checking that a Read() fails.
    std::string response;
    ASSERT_FALSE(Read(&response, 1u));
    ASSERT_TRUE(response.empty());
  }

  void Close() { socket_.reset(); }

 private:
  bool IsCompleteResponse(const std::string& response) {
    // Check end of headers first.
    size_t end_of_headers =
        net::HttpUtil::LocateEndOfHeaders(response.data(), response.size());
    if (end_of_headers == std::string::npos)
      return false;

    // Return true if response has data equal to or more than content length.
    int64_t body_size = static_cast<int64_t>(response.size()) - end_of_headers;
    DCHECK_LE(0, body_size);
    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(
            base::StringPiece(response.data(), end_of_headers)));
    return body_size >= headers->GetContentLength();
  }

  net::TestURLRequestContext url_request_context_;
  network::SocketFactory factory_;
  scoped_refptr<net::IOBufferWithSize> read_buffer_;
  scoped_refptr<net::DrainableIOBuffer> write_buffer_;

  mojo::ScopedDataPipeConsumerHandle receive_pipe_handle_;
  mojo::ScopedDataPipeProducerHandle send_pipe_handle_;

  mojo::Remote<network::mojom::TCPConnectedSocket> socket_;
};

}  // namespace

namespace network {

namespace server {

class HttpServerTest : public testing::Test, public HttpServer::Delegate {
 public:
  HttpServerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        quit_after_request_count_(0),
        quit_on_close_connection_(-1),
        factory_(nullptr, &url_request_context_) {}

  void SetUp() override {
    int net_error = net::ERR_FAILED;
    base::RunLoop run_loop;
    factory_.CreateTCPServerSocket(
        net::IPEndPoint(net::IPAddress::IPv6Localhost(), 0), 1 /* backlog */,
        TRAFFIC_ANNOTATION_FOR_TESTS,
        server_socket_.InitWithNewPipeAndPassReceiver(),
        base::BindOnce(
            [](base::RunLoop* run_loop, int* result_out,
               net::IPEndPoint* local_addr_out, int result,
               const base::Optional<net::IPEndPoint>& local_addr) {
              *result_out = result;
              if (local_addr)
                *local_addr_out = local_addr.value();
              run_loop->Quit();
            },
            base::Unretained(&run_loop), base::Unretained(&net_error),
            base::Unretained(&server_address_)));
    run_loop.Run();
    EXPECT_EQ(net::OK, net_error);

    server_.reset(new HttpServer(std::move(server_socket_), this));
  }

  void OnConnect(int connection_id) override {
    DCHECK(connection_map_.find(connection_id) == connection_map_.end());
    connection_map_[connection_id] = true;
  }

  void OnHttpRequest(int connection_id,
                     const HttpServerRequestInfo& info) override {
    requests_.push_back(std::make_pair(info, connection_id));
    if (requests_.size() == quit_after_request_count_) {
      run_loop_quit_func_.Run();
    }
  }

  void OnWebSocketRequest(int connection_id,
                          const HttpServerRequestInfo& info) override {
    NOTREACHED();
  }

  void OnWebSocketMessage(int connection_id, std::string data) override {
    NOTREACHED();
  }

  void OnClose(int connection_id) override {
    DCHECK(connection_map_.find(connection_id) != connection_map_.end());
    connection_map_[connection_id] = false;
    if (connection_id == quit_on_close_connection_)
      run_loop_quit_func_.Run();
  }

  // This waits until the _total_ requests received is equal to |count|.  This
  // means that, after waiting for one request, in order to wait for a single
  // additional request, |count| must be 2, not 1.
  void RunUntilRequestsReceived(size_t count) {
    quit_after_request_count_ = count;
    if (requests_.size() == count)
      return;

    base::RunLoop run_loop;
    run_loop_quit_func_ = run_loop.QuitClosure();
    run_loop.Run();
    run_loop_quit_func_.Reset();
  }

  void RunUntilConnectionIdClosed(int connection_id) {
    quit_on_close_connection_ = connection_id;
    auto iter = connection_map_.find(connection_id);
    if (iter != connection_map_.end() && !iter->second) {
      // Already disconnected.
      return;
    }

    base::RunLoop run_loop;
    run_loop_quit_func_ = run_loop.QuitClosure();
    run_loop.Run();
    run_loop_quit_func_.Reset();
  }

  HttpServerRequestInfo GetRequest(size_t request_index) {
    return requests_[request_index].first;
  }

  size_t num_requests() const { return requests_.size(); }

  int GetConnectionId(size_t request_index) {
    return requests_[request_index].second;
  }

  HttpConnection* FindConnection(int connection_id) {
    return server_->FindConnection(connection_id);
  }

  std::unordered_map<int, bool>& connection_map() { return connection_map_; }

 protected:
  std::unique_ptr<HttpServer> server_;
  net::IPEndPoint server_address_;
  base::RepeatingClosure run_loop_quit_func_;
  std::vector<std::pair<HttpServerRequestInfo, int>> requests_;
  std::unordered_map<int /* connection_id */, bool /* connected */>
      connection_map_;

 private:
  base::test::TaskEnvironment task_environment_;
  size_t quit_after_request_count_;
  int quit_on_close_connection_;

  net::TestURLRequestContext url_request_context_;
  SocketFactory factory_;
  mojo::PendingRemote<mojom::TCPServerSocket> server_socket_;
};

class WebSocketTest : public HttpServerTest {
  void OnHttpRequest(int connection_id,
                     const HttpServerRequestInfo& info) override {
    NOTREACHED();
  }

  void OnWebSocketRequest(
      int connection_id,
      const network::server::HttpServerRequestInfo& info) override {
    HttpServerTest::OnHttpRequest(connection_id, info);
  }

  void OnWebSocketMessage(int connection_id, std::string data) override {}
};

TEST_F(HttpServerTest, SetNonexistingConnectionBuffer) {
  EXPECT_FALSE(server_->SetReceiveBufferSize(1, 1000));
  EXPECT_FALSE(server_->SetSendBufferSize(1, 1000));
}

TEST_F(HttpServerTest, Request) {
  TestHttpClient client;
  ASSERT_THAT(client.ConnectAndWait(server_address_), IsOk());
  client.Send("GET /test HTTP/1.1\r\n\r\n");

  int connection_id = connection_map_.begin()->first;
  HttpConnection* conn = FindConnection(connection_id);
  EXPECT_TRUE(server_->SetReceiveBufferSize(connection_id, 5u * 1024 * 1024));
  EXPECT_TRUE(server_->SetSendBufferSize(connection_id, 5u * 1024 * 1024));
  EXPECT_EQ(conn->ReadBufferSize(), 5u * 1024u * 1024u);
  EXPECT_EQ(conn->WriteBufferSize(), 5u * 1024u * 1024u);

  RunUntilRequestsReceived(1);
  ASSERT_EQ("GET", GetRequest(0).method);
  ASSERT_EQ("/test", GetRequest(0).path);
  ASSERT_EQ("", GetRequest(0).data);
  ASSERT_EQ(0u, GetRequest(0).headers.size());
  ASSERT_EQ(GetRequest(0).peer.address(), net::IPAddress::IPv6Localhost());
}

TEST_F(HttpServerTest, RequestBrokenTermination) {
  TestHttpClient client;
  ASSERT_THAT(client.ConnectAndWait(server_address_), IsOk());
  client.Send("GET /test HTTP/1.1\r\n\r)");
  RunUntilConnectionIdClosed(1);
  EXPECT_EQ(0u, num_requests());
  client.ExpectUsedThenDisconnectedWithNoData();
}

TEST_F(HttpServerTest, RequestWithHeaders) {
  TestHttpClient client;
  ASSERT_THAT(client.ConnectAndWait(server_address_), IsOk());
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
  for (size_t i = 0; i < base::size(kHeaders); ++i) {
    headers +=
        std::string(kHeaders[i][0]) + kHeaders[i][1] + kHeaders[i][2] + "\r\n";
  }

  client.Send("GET /test HTTP/1.1\r\n" + headers + "\r\n");
  RunUntilRequestsReceived(1);
  ASSERT_EQ("", GetRequest(0).data);

  for (size_t i = 0; i < base::size(kHeaders); ++i) {
    std::string field = base::ToLowerASCII(std::string(kHeaders[i][0]));
    std::string value = kHeaders[i][2];
    ASSERT_EQ(1u, GetRequest(0).headers.count(field)) << field;
    ASSERT_EQ(value, GetRequest(0).headers[field]) << kHeaders[i][0];
  }
}

TEST_F(HttpServerTest, RequestWithDuplicateHeaders) {
  TestHttpClient client;
  ASSERT_THAT(client.ConnectAndWait(server_address_), IsOk());
  const char* const kHeaders[][3] = {
      {"FirstHeader", ": ", "1"},  {"DuplicateHeader", ": ", "2"},
      {"MiddleHeader", ": ", "3"}, {"DuplicateHeader", ": ", "4"},
      {"LastHeader", ": ", "5"},
  };
  std::string headers;
  for (size_t i = 0; i < base::size(kHeaders); ++i) {
    headers +=
        std::string(kHeaders[i][0]) + kHeaders[i][1] + kHeaders[i][2] + "\r\n";
  }

  client.Send("GET /test HTTP/1.1\r\n" + headers + "\r\n");
  RunUntilRequestsReceived(1);
  ASSERT_EQ("", GetRequest(0).data);

  for (size_t i = 0; i < base::size(kHeaders); ++i) {
    std::string field = base::ToLowerASCII(std::string(kHeaders[i][0]));
    std::string value = (field == "duplicateheader") ? "2,4" : kHeaders[i][2];
    ASSERT_EQ(1u, GetRequest(0).headers.count(field)) << field;
    ASSERT_EQ(value, GetRequest(0).headers[field]) << kHeaders[i][0];
  }
}

TEST_F(HttpServerTest, HasHeaderValueTest) {
  TestHttpClient client;
  ASSERT_THAT(client.ConnectAndWait(server_address_), IsOk());
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
  for (size_t i = 0; i < base::size(kHeaders); ++i) {
    headers += std::string(kHeaders[i]) + "\r\n";
  }

  client.Send("GET /test HTTP/1.1\r\n" + headers + "\r\n");
  RunUntilRequestsReceived(1);
  ASSERT_EQ("", GetRequest(0).data);

  ASSERT_TRUE(GetRequest(0).HasHeaderValue("header", "abcd"));
  ASSERT_FALSE(GetRequest(0).HasHeaderValue("header", "bc"));
  ASSERT_TRUE(GetRequest(0).HasHeaderValue("headerwithnowhitespace", "e"));
  ASSERT_TRUE(GetRequest(0).HasHeaderValue("headerwithwhitespace", "f"));
  ASSERT_TRUE(GetRequest(0).HasHeaderValue("duplicateheader", "g"));
  ASSERT_TRUE(GetRequest(0).HasHeaderValue("headerwithcomma", "h"));
  ASSERT_TRUE(GetRequest(0).HasHeaderValue("headerwithcomma", "i"));
  ASSERT_TRUE(GetRequest(0).HasHeaderValue("headerwithcomma", "j"));
  ASSERT_TRUE(GetRequest(0).HasHeaderValue("duplicateheader", "k"));
  ASSERT_FALSE(GetRequest(0).HasHeaderValue("emptyheader", "x"));
  ASSERT_FALSE(GetRequest(0).HasHeaderValue("emptyheaderwithwhitespace", "x"));
  ASSERT_TRUE(GetRequest(0).HasHeaderValue("headerwithnonascii", "\xf7"));
}

TEST_F(HttpServerTest, RequestWithBody) {
  TestHttpClient client;
  ASSERT_THAT(client.ConnectAndWait(server_address_), IsOk());
  std::string body = "a" + std::string(1 << 10, 'b') + "c";
  client.Send(
      base::StringPrintf("GET /test HTTP/1.1\r\n"
                         "SomeHeader: 1\r\n"
                         "Content-Length: %" PRIuS "\r\n\r\n%s",
                         body.length(), body.c_str()));
  RunUntilRequestsReceived(1);
  ASSERT_EQ(2u, GetRequest(0).headers.size());
  ASSERT_EQ(body.length(), GetRequest(0).data.length());
  ASSERT_EQ('a', body[0]);
  ASSERT_EQ('c', *body.rbegin());
}

// Tests that |HttpServer:OnReadable()| will notice the closure of the connected
// socket and not try to read from an invalid pipe.
TEST_F(WebSocketTest, PipeClosed) {
  TestHttpClient client;
  ASSERT_THAT(client.ConnectAndWait(server_address_), IsOk());
  client.Send(
      "GET /test HTTP/1.1\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: SomethingElse, Upgrade\r\n"
      "Sec-WebSocket-Version: 8\r\n"
      "Sec-WebSocket-Key: key\r\n"
      "\r\n");
  RunUntilRequestsReceived(1);
  client.Close();
  RunUntilConnectionIdClosed(1);
}

TEST_F(WebSocketTest, RequestWebSocket) {
  TestHttpClient client;
  ASSERT_THAT(client.ConnectAndWait(server_address_), IsOk());
  client.Send(
      "GET /test HTTP/1.1\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: SomethingElse, Upgrade\r\n"
      "Sec-WebSocket-Version: 8\r\n"
      "Sec-WebSocket-Key: key\r\n"
      "\r\n");
  RunUntilRequestsReceived(1);
}

TEST_F(WebSocketTest, RequestWebSocketTrailingJunk) {
  TestHttpClient client;
  ASSERT_THAT(client.ConnectAndWait(server_address_), IsOk());
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

TEST_F(HttpServerTest, RequestWithTooLargeBody) {
  TestHttpClient client;
  ASSERT_THAT(client.ConnectAndWait(server_address_), IsOk());
  client.Send(
      "GET /test HTTP/1.1\r\n"
      "Content-Length: 1073741824\r\n\r\n");
  std::string response;
  ASSERT_TRUE(client.ReadResponse(&response));
  EXPECT_EQ(
      "HTTP/1.1 500 Internal Server Error\r\n"
      "Content-Length:53\r\n"
      "Content-Type:text/html\r\n\r\n"
      "request content-length too big or unknown: 1073741824",
      response);
}

TEST_F(HttpServerTest, Send200) {
  TestHttpClient client;
  ASSERT_THAT(client.ConnectAndWait(server_address_), IsOk());
  client.Send("GET /test HTTP/1.1\r\n\r\n");
  RunUntilRequestsReceived(1);
  server_->Send200(GetConnectionId(0), "Response!", "text/plain",
                   TRAFFIC_ANNOTATION_FOR_TESTS);

  std::string response;
  ASSERT_TRUE(client.ReadResponse(&response));
  ASSERT_TRUE(base::StartsWith(response, "HTTP/1.1 200 OK",
                               base::CompareCase::SENSITIVE));
  ASSERT_TRUE(
      base::EndsWith(response, "Response!", base::CompareCase::SENSITIVE));
}

TEST_F(HttpServerTest, SendRaw) {
  TestHttpClient client;
  ASSERT_THAT(client.ConnectAndWait(server_address_), IsOk());
  client.Send("GET /test HTTP/1.1\r\n\r\n");
  RunUntilRequestsReceived(1);
  server_->SendRaw(GetConnectionId(0), "Raw Data ",
                   TRAFFIC_ANNOTATION_FOR_TESTS);
  server_->SendRaw(GetConnectionId(0), "More Data",
                   TRAFFIC_ANNOTATION_FOR_TESTS);
  server_->SendRaw(GetConnectionId(0), "Third Piece of Data",
                   TRAFFIC_ANNOTATION_FOR_TESTS);

  const std::string expected_response("Raw Data More DataThird Piece of Data");
  std::string response;
  ASSERT_TRUE(client.Read(&response, expected_response.length()));
  ASSERT_EQ(expected_response, response);
}

TEST_F(HttpServerTest, SendRawOverTwoConnections) {
  TestHttpClient client1, client2;

  // Requests are staggered so that their order is deterministic - otherwise the
  // test has no way of associating the response with the client object.
  ASSERT_THAT(client1.ConnectAndWait(server_address_), IsOk());
  client1.Send("GET /test1 HTTP/1.1\r\n\r\n");
  RunUntilRequestsReceived(1);

  ASSERT_THAT(client2.ConnectAndWait(server_address_), IsOk());
  client2.Send("GET /test2 HTTP/1.1\r\n\r\n");
  RunUntilRequestsReceived(2);

  server_->SendRaw(GetConnectionId(0), "Raw Data ",
                   TRAFFIC_ANNOTATION_FOR_TESTS);
  server_->SendRaw(GetConnectionId(1), "More Data",
                   TRAFFIC_ANNOTATION_FOR_TESTS);
  server_->SendRaw(GetConnectionId(0), "Third Piece of Data",
                   TRAFFIC_ANNOTATION_FOR_TESTS);
  server_->SendRaw(GetConnectionId(1), "And #4", TRAFFIC_ANNOTATION_FOR_TESTS);

  const std::string expected_response1("Raw Data Third Piece of Data");
  const std::string expected_response2("More DataAnd #4");
  std::string response1, response2;

  ASSERT_TRUE(client1.Read(&response1, expected_response1.length()));
  ASSERT_TRUE(client2.Read(&response2, expected_response2.length()));
  ASSERT_EQ(expected_response1, response1);
  ASSERT_EQ(expected_response2, response2);
}

TEST_F(HttpServerTest, WrongProtocolRequest) {
  const char* const kBadProtocolRequests[] = {
      "GET /test HTTP/1.0\r\n\r\n", "GET /test foo\r\n\r\n",
      "GET /test \r\n\r\n",
  };

  for (size_t i = 0; i < base::size(kBadProtocolRequests); ++i) {
    TestHttpClient client;
    ASSERT_THAT(client.ConnectAndWait(server_address_), IsOk());

    client.Send(kBadProtocolRequests[i]);
    client.ExpectUsedThenDisconnectedWithNoData();

    // Assert that the delegate was updated properly.
    ASSERT_EQ(1u, connection_map().size());
    ASSERT_FALSE(connection_map().begin()->second);
    EXPECT_EQ(0ul, requests_.size());

    // Reset the state of the connection map.
    connection_map().clear();
  }
}

TEST_F(HttpServerTest, RequestWithBodySplitAcrossPackets) {
  TestHttpClient client;
  ASSERT_THAT(client.ConnectAndWait(server_address_), IsOk());

  std::string body("body");
  std::string request_text = base::StringPrintf(
      "GET /test HTTP/1.1\r\n"
      "SomeHeader: 1\r\n"
      "Content-Length: %" PRIuS "\r\n\r\n%s",
      body.length(), body.c_str());

  std::string packet1(request_text.c_str(), request_text.length() - 2);
  std::string packet2(request_text, request_text.length() - 2);

  client.Send(packet1);
  base::RunLoop().RunUntilIdle();
  client.Send(packet2);
  RunUntilRequestsReceived(1);

  ASSERT_EQ(1u, requests_.size());
  ASSERT_EQ(body, GetRequest(0).data);
}

TEST_F(HttpServerTest, MultipleRequestsOnSameConnection) {
  // The idea behind this test is that requests with or without bodies should
  // not break parsing of the next request.
  TestHttpClient client;
  ASSERT_THAT(client.ConnectAndWait(server_address_), IsOk());
  std::string body = "body";
  client.Send(
      base::StringPrintf("GET /test HTTP/1.1\r\n"
                         "Content-Length: %" PRIuS "\r\n\r\n%s",
                         body.length(), body.c_str()));
  RunUntilRequestsReceived(1);
  ASSERT_EQ(body, GetRequest(0).data);

  int client_connection_id = GetConnectionId(0);
  server_->Send200(client_connection_id, "Content for /test", "text/plain",
                   TRAFFIC_ANNOTATION_FOR_TESTS);
  std::string response1;
  ASSERT_TRUE(client.ReadResponse(&response1));
  ASSERT_TRUE(base::StartsWith(response1, "HTTP/1.1 200 OK",
                               base::CompareCase::SENSITIVE));
  ASSERT_TRUE(base::EndsWith(response1, "Content for /test",
                             base::CompareCase::SENSITIVE));

  client.Send("GET /test2 HTTP/1.1\r\n\r\n");
  RunUntilRequestsReceived(2);
  ASSERT_EQ("/test2", GetRequest(1).path);

  ASSERT_EQ(client_connection_id, GetConnectionId(1));
  server_->Send404(client_connection_id, TRAFFIC_ANNOTATION_FOR_TESTS);
  std::string response2;
  ASSERT_TRUE(client.ReadResponse(&response2));
  ASSERT_TRUE(base::StartsWith(response2, "HTTP/1.1 404 Not Found",
                               base::CompareCase::SENSITIVE));

  client.Send("GET /test3 HTTP/1.1\r\n\r\n");
  RunUntilRequestsReceived(3);
  ASSERT_EQ("/test3", GetRequest(2).path);

  ASSERT_EQ(client_connection_id, GetConnectionId(2));
  server_->Send200(client_connection_id, "Content for /test3", "text/plain",
                   TRAFFIC_ANNOTATION_FOR_TESTS);
  std::string response3;
  ASSERT_TRUE(client.ReadResponse(&response3));
  ASSERT_TRUE(base::StartsWith(response3, "HTTP/1.1 200 OK",
                               base::CompareCase::SENSITIVE));
  ASSERT_TRUE(base::EndsWith(response3, "Content for /test3",
                             base::CompareCase::SENSITIVE));
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
  ASSERT_THAT(client.ConnectAndWait(server_address_), IsOk());
  client.Send("GET / HTTP/1.1\r\n\r\n");

  // The server should close the socket without responding.
  client.ExpectUsedThenDisconnectedWithNoData();

  // Run any tasks the TestServer posted.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1ul, connection_ids_.size());
  // OnHttpRequest() should never have been called, since the connection was
  // closed without reading from it.
  EXPECT_EQ(0ul, requests_.size());
}

}  // namespace server

}  // namespace network
