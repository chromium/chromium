// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_basic_stream_adapters.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_server.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/connect_job.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/socket/websocket_endpoint_lock_manager.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_key.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/websockets/websocket_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Test;
using testing::StrictMock;
using testing::_;

namespace net {

namespace test {

class WebSocketClientSocketHandleAdapterTest : public TestWithTaskEnvironment {
 protected:
  WebSocketClientSocketHandleAdapterTest()
      : host_port_pair_("www.example.org", 443),
        network_session_(
            SpdySessionDependencies::SpdyCreateSession(&session_deps_)),
        websocket_endpoint_lock_manager_(
            network_session_->websocket_endpoint_lock_manager()) {}

  ~WebSocketClientSocketHandleAdapterTest() override = default;

  bool InitClientSocketHandle(ClientSocketHandle* connection) {
    scoped_refptr<ClientSocketPool::SocketParams> socks_params =
        base::MakeRefCounted<ClientSocketPool::SocketParams>(
            std::make_unique<SSLConfig>() /* ssl_config_for_origin */,
            nullptr /* ssl_config_for_proxy */);
    TestCompletionCallback callback;
    int rv = connection->Init(
        ClientSocketPool::GroupId(
            host_port_pair_, ClientSocketPool::SocketType::kSsl,
            PrivacyMode::PRIVACY_MODE_DISABLED, NetworkIsolationKey(),
            false /* disable_secure_dns */),
        socks_params, TRAFFIC_ANNOTATION_FOR_TESTS /* proxy_annotation_tag */,
        MEDIUM, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
        callback.callback(), ClientSocketPool::ProxyAuthCallback(),
        network_session_->GetSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL,
                                        ProxyServer::Direct()),
        NetLogWithSource());
    rv = callback.GetResult(rv);
    return rv == OK;
  }

  const HostPortPair host_port_pair_;
  SpdySessionDependencies session_deps_;
  std::unique_ptr<HttpNetworkSession> network_session_;
  WebSocketEndpointLockManager* websocket_endpoint_lock_manager_;
};

TEST_F(WebSocketClientSocketHandleAdapterTest, Uninitialized) {
  auto connection = std::make_unique<ClientSocketHandle>();
  WebSocketClientSocketHandleAdapter adapter(std::move(connection));
  EXPECT_FALSE(adapter.is_initialized());
}

TEST_F(WebSocketClientSocketHandleAdapterTest, IsInitialized) {
  StaticSocketDataProvider data;
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data);

  auto connection = std::make_unique<ClientSocketHandle>();
  ClientSocketHandle* const connection_ptr = connection.get();

  WebSocketClientSocketHandleAdapter adapter(std::move(connection));
  EXPECT_FALSE(adapter.is_initialized());

  EXPECT_TRUE(InitClientSocketHandle(connection_ptr));

  EXPECT_TRUE(adapter.is_initialized());
}

TEST_F(WebSocketClientSocketHandleAdapterTest, Disconnect) {
  StaticSocketDataProvider data;
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data);

  auto connection = std::make_unique<ClientSocketHandle>();
  EXPECT_TRUE(InitClientSocketHandle(connection.get()));

  StreamSocket* const socket = connection->socket();

  WebSocketClientSocketHandleAdapter adapter(std::move(connection));
  EXPECT_TRUE(adapter.is_initialized());

  EXPECT_TRUE(socket->IsConnected());
  adapter.Disconnect();
  EXPECT_FALSE(socket->IsConnected());
}

TEST_F(WebSocketClientSocketHandleAdapterTest, Read) {
  MockRead reads[] = {MockRead(SYNCHRONOUS, "foo"), MockRead("bar")};
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data);

  auto connection = std::make_unique<ClientSocketHandle>();
  EXPECT_TRUE(InitClientSocketHandle(connection.get()));

  WebSocketClientSocketHandleAdapter adapter(std::move(connection));
  EXPECT_TRUE(adapter.is_initialized());

  // Buffer larger than each MockRead.
  const int kReadBufSize = 1024;
  auto read_buf = base::MakeRefCounted<IOBuffer>(kReadBufSize);
  int rv = adapter.Read(read_buf.get(), kReadBufSize, CompletionOnceCallback());
  ASSERT_EQ(3, rv);
  EXPECT_EQ("foo", base::StringPiece(read_buf->data(), rv));

  TestCompletionCallback callback;
  rv = adapter.Read(read_buf.get(), kReadBufSize, callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  ASSERT_EQ(3, rv);
  EXPECT_EQ("bar", base::StringPiece(read_buf->data(), rv));

  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

TEST_F(WebSocketClientSocketHandleAdapterTest, ReadIntoSmallBuffer) {
  MockRead reads[] = {MockRead(SYNCHRONOUS, "foo"), MockRead("bar")};
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data);

  auto connection = std::make_unique<ClientSocketHandle>();
  EXPECT_TRUE(InitClientSocketHandle(connection.get()));

  WebSocketClientSocketHandleAdapter adapter(std::move(connection));
  EXPECT_TRUE(adapter.is_initialized());

  // Buffer smaller than each MockRead.
  const int kReadBufSize = 2;
  auto read_buf = base::MakeRefCounted<IOBuffer>(kReadBufSize);
  int rv = adapter.Read(read_buf.get(), kReadBufSize, CompletionOnceCallback());
  ASSERT_EQ(2, rv);
  EXPECT_EQ("fo", base::StringPiece(read_buf->data(), rv));

  rv = adapter.Read(read_buf.get(), kReadBufSize, CompletionOnceCallback());
  ASSERT_EQ(1, rv);
  EXPECT_EQ("o", base::StringPiece(read_buf->data(), rv));

  TestCompletionCallback callback1;
  rv = adapter.Read(read_buf.get(), kReadBufSize, callback1.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback1.WaitForResult();
  ASSERT_EQ(2, rv);
  EXPECT_EQ("ba", base::StringPiece(read_buf->data(), rv));

  rv = adapter.Read(read_buf.get(), kReadBufSize, CompletionOnceCallback());
  ASSERT_EQ(1, rv);
  EXPECT_EQ("r", base::StringPiece(read_buf->data(), rv));

  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

TEST_F(WebSocketClientSocketHandleAdapterTest, Write) {
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, "foo"), MockWrite("bar")};
  StaticSocketDataProvider data(base::span<MockRead>(), writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data);

  auto connection = std::make_unique<ClientSocketHandle>();
  EXPECT_TRUE(InitClientSocketHandle(connection.get()));

  WebSocketClientSocketHandleAdapter adapter(std::move(connection));
  EXPECT_TRUE(adapter.is_initialized());

  auto write_buf1 = base::MakeRefCounted<StringIOBuffer>("foo");
  int rv =
      adapter.Write(write_buf1.get(), write_buf1->size(),
                    CompletionOnceCallback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  ASSERT_EQ(3, rv);

  auto write_buf2 = base::MakeRefCounted<StringIOBuffer>("bar");
  TestCompletionCallback callback;
  rv = adapter.Write(write_buf2.get(), write_buf2->size(), callback.callback(),
                     TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  ASSERT_EQ(3, rv);

  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

// Test that if both Read() and Write() returns asynchronously,
// the two callbacks are handled correctly.
TEST_F(WebSocketClientSocketHandleAdapterTest, AsyncReadAndWrite) {
  MockRead reads[] = {MockRead("foobar")};
  MockWrite writes[] = {MockWrite("baz")};
  StaticSocketDataProvider data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data);

  auto connection = std::make_unique<ClientSocketHandle>();
  EXPECT_TRUE(InitClientSocketHandle(connection.get()));

  WebSocketClientSocketHandleAdapter adapter(std::move(connection));
  EXPECT_TRUE(adapter.is_initialized());

  const int kReadBufSize = 1024;
  auto read_buf = base::MakeRefCounted<IOBuffer>(kReadBufSize);
  TestCompletionCallback read_callback;
  int rv = adapter.Read(read_buf.get(), kReadBufSize, read_callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  auto write_buf = base::MakeRefCounted<StringIOBuffer>("baz");
  TestCompletionCallback write_callback;
  rv = adapter.Write(write_buf.get(), write_buf->size(),
                     write_callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  rv = read_callback.WaitForResult();
  ASSERT_EQ(6, rv);
  EXPECT_EQ("foobar", base::StringPiece(read_buf->data(), rv));

  rv = write_callback.WaitForResult();
  ASSERT_EQ(3, rv);

  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

class MockDelegate : public WebSocketSpdyStreamAdapter::Delegate {
 public:
  ~MockDelegate() override = default;
  MOCK_METHOD0(OnHeadersSent, void());
  MOCK_METHOD1(OnHeadersReceived, void(const spdy::SpdyHeaderBlock&));
  MOCK_METHOD1(OnClose, void(int));
};

class WebSocketSpdyStreamAdapterTest : public TestWithTaskEnvironment {
 protected:
  WebSocketSpdyStreamAdapterTest()
      : url_("wss://www.example.org/"),
        key_(HostPortPair::FromURL(url_),
             ProxyServer::Direct(),
             PRIVACY_MODE_DISABLED,
             SpdySessionKey::IsProxySession::kFalse,
             SocketTag(),
             NetworkIsolationKey(),
             false /* disable_secure_dns */),
        session_(SpdySessionDependencies::SpdyCreateSession(&session_deps_)),
        ssl_(SYNCHRONOUS, OK) {}

  ~WebSocketSpdyStreamAdapterTest() override = default;

  static spdy::SpdyHeaderBlock RequestHeaders() {
    return WebSocketHttp2Request("/", "www.example.org:443",
                                 "http://www.example.org", {});
  }

  static spdy::SpdyHeaderBlock ResponseHeaders() {
    return WebSocketHttp2Response({});
  }

  void AddSocketData(SocketDataProvider* data) {
    session_deps_.socket_factory->AddSocketDataProvider(data);
  }

  void AddSSLSocketData() {
    ssl_.ssl_info.cert =
        ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem");
    ASSERT_TRUE(ssl_.ssl_info.cert);
    session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_);
  }

  base::WeakPtr<SpdySession> CreateSpdySession() {
    return ::net::CreateSpdySession(session_.get(), key_, NetLogWithSource());
  }

  base::WeakPtr<SpdyStream> CreateSpdyStream(
      base::WeakPtr<SpdySession> session) {
    return CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM, session, url_,
                                     LOWEST, NetLogWithSource());
  }

  SpdyTestUtil spdy_util_;
  StrictMock<MockDelegate> mock_delegate_;

 private:
  const GURL url_;
  const SpdySessionKey key_;
  SpdySessionDependencies session_deps_;
  std::unique_ptr<HttpNetworkSession> session_;
  SSLSocketDataProvider ssl_;
};

TEST_F(WebSocketSpdyStreamAdapterTest, Disconnect) {
  MockRead reads[] = {MockRead(ASYNC, ERR_IO_PENDING, 0),
                      MockRead(ASYNC, 0, 1)};
  SequencedSocketData data(reads, base::span<MockWrite>());
  AddSocketData(&data);
  AddSSLSocketData();

  base::WeakPtr<SpdySession> session = CreateSpdySession();
  base::WeakPtr<SpdyStream> stream = CreateSpdyStream(session);
  WebSocketSpdyStreamAdapter adapter(stream, &mock_delegate_,
                                     NetLogWithSource());
  EXPECT_TRUE(adapter.is_initialized());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(stream);
  adapter.Disconnect();
  EXPECT_FALSE(stream);

  // Read EOF.
  EXPECT_TRUE(session);
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session);

  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

TEST_F(WebSocketSpdyStreamAdapterTest, SendRequestHeadersThenDisconnect) {
  MockRead reads[] = {MockRead(ASYNC, ERR_IO_PENDING, 0),
                      MockRead(ASYNC, 0, 3)};
  spdy::SpdySerializedFrame headers(spdy_util_.ConstructSpdyHeaders(
      1, RequestHeaders(), DEFAULT_PRIORITY, false));
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));
  MockWrite writes[] = {CreateMockWrite(headers, 1), CreateMockWrite(rst, 2)};
  SequencedSocketData data(reads, writes);
  AddSocketData(&data);
  AddSSLSocketData();

  base::WeakPtr<SpdySession> session = CreateSpdySession();
  base::WeakPtr<SpdyStream> stream = CreateSpdyStream(session);
  WebSocketSpdyStreamAdapter adapter(stream, &mock_delegate_,
                                     NetLogWithSource());
  EXPECT_TRUE(adapter.is_initialized());

  int rv = stream->SendRequestHeaders(RequestHeaders(), MORE_DATA_TO_SEND);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // First read is a pause and it has lower sequence number than first write.
  // Therefore writing headers does not complete while |data| is paused.
  base::RunLoop().RunUntilIdle();

  // Reset the stream before writing completes.
  // OnHeadersSent() will never be called.
  EXPECT_TRUE(stream);
  adapter.Disconnect();
  EXPECT_FALSE(stream);

  // Resume |data|, finish writing headers, and read EOF.
  EXPECT_TRUE(session);
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session);

  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

TEST_F(WebSocketSpdyStreamAdapterTest, OnHeadersSentThenDisconnect) {
  MockRead reads[] = {MockRead(ASYNC, 0, 2)};
  spdy::SpdySerializedFrame headers(spdy_util_.ConstructSpdyHeaders(
      1, RequestHeaders(), DEFAULT_PRIORITY, false));
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));
  MockWrite writes[] = {CreateMockWrite(headers, 0), CreateMockWrite(rst, 1)};
  SequencedSocketData data(reads, writes);
  AddSocketData(&data);
  AddSSLSocketData();

  EXPECT_CALL(mock_delegate_, OnHeadersSent());

  base::WeakPtr<SpdySession> session = CreateSpdySession();
  base::WeakPtr<SpdyStream> stream = CreateSpdyStream(session);
  WebSocketSpdyStreamAdapter adapter(stream, &mock_delegate_,
                                     NetLogWithSource());
  EXPECT_TRUE(adapter.is_initialized());

  int rv = stream->SendRequestHeaders(RequestHeaders(), MORE_DATA_TO_SEND);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Finish asynchronous write of headers.  This calls OnHeadersSent().
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(stream);
  adapter.Disconnect();
  EXPECT_FALSE(stream);

  // Read EOF.
  EXPECT_TRUE(session);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session);

  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

TEST_F(WebSocketSpdyStreamAdapterTest, OnHeadersReceivedThenDisconnect) {
  spdy::SpdySerializedFrame response_headers(
      spdy_util_.ConstructSpdyResponseHeaders(1, ResponseHeaders(), false));
  MockRead reads[] = {CreateMockRead(response_headers, 1),
                      MockRead(ASYNC, 0, 3)};
  spdy::SpdySerializedFrame request_headers(spdy_util_.ConstructSpdyHeaders(
      1, RequestHeaders(), DEFAULT_PRIORITY, false));
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));
  MockWrite writes[] = {CreateMockWrite(request_headers, 0),
                        CreateMockWrite(rst, 2)};
  SequencedSocketData data(reads, writes);
  AddSocketData(&data);
  AddSSLSocketData();

  EXPECT_CALL(mock_delegate_, OnHeadersSent());
  EXPECT_CALL(mock_delegate_, OnHeadersReceived(_));

  base::WeakPtr<SpdySession> session = CreateSpdySession();
  base::WeakPtr<SpdyStream> stream = CreateSpdyStream(session);
  WebSocketSpdyStreamAdapter adapter(stream, &mock_delegate_,
                                     NetLogWithSource());
  EXPECT_TRUE(adapter.is_initialized());

  int rv = stream->SendRequestHeaders(RequestHeaders(), MORE_DATA_TO_SEND);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(stream);
  adapter.Disconnect();
  EXPECT_FALSE(stream);

  // Read EOF.
  EXPECT_TRUE(session);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session);

  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

TEST_F(WebSocketSpdyStreamAdapterTest, ServerClosesConnection) {
  MockRead reads[] = {MockRead(ASYNC, 0, 0)};
  SequencedSocketData data(reads, base::span<MockWrite>());
  AddSocketData(&data);
  AddSSLSocketData();

  EXPECT_CALL(mock_delegate_, OnClose(ERR_CONNECTION_CLOSED));

  base::WeakPtr<SpdySession> session = CreateSpdySession();
  base::WeakPtr<SpdyStream> stream = CreateSpdyStream(session);
  WebSocketSpdyStreamAdapter adapter(stream, &mock_delegate_,
                                     NetLogWithSource());
  EXPECT_TRUE(adapter.is_initialized());

  EXPECT_TRUE(session);
  EXPECT_TRUE(stream);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session);
  EXPECT_FALSE(stream);

  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

TEST_F(WebSocketSpdyStreamAdapterTest,
       SendRequestHeadersThenServerClosesConnection) {
  MockRead reads[] = {MockRead(ASYNC, 0, 1)};
  spdy::SpdySerializedFrame headers(spdy_util_.ConstructSpdyHeaders(
      1, RequestHeaders(), DEFAULT_PRIORITY, false));
  MockWrite writes[] = {CreateMockWrite(headers, 0)};
  SequencedSocketData data(reads, writes);
  AddSocketData(&data);
  AddSSLSocketData();

  EXPECT_CALL(mock_delegate_, OnHeadersSent());
  EXPECT_CALL(mock_delegate_, OnClose(ERR_CONNECTION_CLOSED));

  base::WeakPtr<SpdySession> session = CreateSpdySession();
  base::WeakPtr<SpdyStream> stream = CreateSpdyStream(session);
  WebSocketSpdyStreamAdapter adapter(stream, &mock_delegate_,
                                     NetLogWithSource());
  EXPECT_TRUE(adapter.is_initialized());

  int rv = stream->SendRequestHeaders(RequestHeaders(), MORE_DATA_TO_SEND);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  EXPECT_TRUE(session);
  EXPECT_TRUE(stream);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session);
  EXPECT_FALSE(stream);

  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

TEST_F(WebSocketSpdyStreamAdapterTest,
       OnHeadersReceivedThenServerClosesConnection) {
  spdy::SpdySerializedFrame response_headers(
      spdy_util_.ConstructSpdyResponseHeaders(1, ResponseHeaders(), false));
  MockRead reads[] = {CreateMockRead(response_headers, 1),
                      MockRead(ASYNC, 0, 2)};
  spdy::SpdySerializedFrame request_headers(spdy_util_.ConstructSpdyHeaders(
      1, RequestHeaders(), DEFAULT_PRIORITY, false));
  MockWrite writes[] = {CreateMockWrite(request_headers, 0)};
  SequencedSocketData data(reads, writes);
  AddSocketData(&data);
  AddSSLSocketData();

  EXPECT_CALL(mock_delegate_, OnHeadersSent());
  EXPECT_CALL(mock_delegate_, OnHeadersReceived(_));
  EXPECT_CALL(mock_delegate_, OnClose(ERR_CONNECTION_CLOSED));

  base::WeakPtr<SpdySession> session = CreateSpdySession();
  base::WeakPtr<SpdyStream> stream = CreateSpdyStream(session);
  WebSocketSpdyStreamAdapter adapter(stream, &mock_delegate_,
                                     NetLogWithSource());
  EXPECT_TRUE(adapter.is_initialized());

  int rv = stream->SendRequestHeaders(RequestHeaders(), MORE_DATA_TO_SEND);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  EXPECT_TRUE(session);
  EXPECT_TRUE(stream);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session);
  EXPECT_FALSE(stream);

  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

TEST_F(WebSocketSpdyStreamAdapterTest, DetachDelegate) {
  spdy::SpdySerializedFrame response_headers(
      spdy_util_.ConstructSpdyResponseHeaders(1, ResponseHeaders(), false));
  MockRead reads[] = {CreateMockRead(response_headers, 1),
                      MockRead(ASYNC, 0, 2)};
  spdy::SpdySerializedFrame request_headers(spdy_util_.ConstructSpdyHeaders(
      1, RequestHeaders(), DEFAULT_PRIORITY, false));
  MockWrite writes[] = {CreateMockWrite(request_headers, 0)};
  SequencedSocketData data(reads, writes);
  AddSocketData(&data);
  AddSSLSocketData();

  base::WeakPtr<SpdySession> session = CreateSpdySession();
  base::WeakPtr<SpdyStream> stream = CreateSpdyStream(session);
  WebSocketSpdyStreamAdapter adapter(stream, &mock_delegate_,
                                     NetLogWithSource());
  EXPECT_TRUE(adapter.is_initialized());

  // No Delegate methods shall be called after this.
  adapter.DetachDelegate();

  int rv = stream->SendRequestHeaders(RequestHeaders(), MORE_DATA_TO_SEND);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  EXPECT_TRUE(session);
  EXPECT_TRUE(stream);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session);
  EXPECT_FALSE(stream);

  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

TEST_F(WebSocketSpdyStreamAdapterTest, Read) {
  spdy::SpdySerializedFrame response_headers(
      spdy_util_.ConstructSpdyResponseHeaders(1, ResponseHeaders(), false));
  // First read is the same size as the buffer, next is smaller, last is larger.
  spdy::SpdySerializedFrame data_frame1(
      spdy_util_.ConstructSpdyDataFrame(1, "foo", false));
  spdy::SpdySerializedFrame data_frame2(
      spdy_util_.ConstructSpdyDataFrame(1, "ba", false));
  spdy::SpdySerializedFrame data_frame3(
      spdy_util_.ConstructSpdyDataFrame(1, "rbaz", true));
  MockRead reads[] = {CreateMockRead(response_headers, 1),
                      CreateMockRead(data_frame1, 2),
                      CreateMockRead(data_frame2, 3),
                      CreateMockRead(data_frame3, 4), MockRead(ASYNC, 0, 5)};
  spdy::SpdySerializedFrame request_headers(spdy_util_.ConstructSpdyHeaders(
      1, RequestHeaders(), DEFAULT_PRIORITY, false));
  MockWrite writes[] = {CreateMockWrite(request_headers, 0)};
  SequencedSocketData data(reads, writes);
  AddSocketData(&data);
  AddSSLSocketData();

  EXPECT_CALL(mock_delegate_, OnHeadersSent());
  EXPECT_CALL(mock_delegate_, OnHeadersReceived(_));

  base::WeakPtr<SpdySession> session = CreateSpdySession();
  base::WeakPtr<SpdyStream> stream = CreateSpdyStream(session);
  WebSocketSpdyStreamAdapter adapter(stream, &mock_delegate_,
                                     NetLogWithSource());
  EXPECT_TRUE(adapter.is_initialized());

  int rv = stream->SendRequestHeaders(RequestHeaders(), MORE_DATA_TO_SEND);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  const int kReadBufSize = 3;
  auto read_buf = base::MakeRefCounted<IOBuffer>(kReadBufSize);
  TestCompletionCallback callback;
  rv = adapter.Read(read_buf.get(), kReadBufSize, callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  ASSERT_EQ(3, rv);
  EXPECT_EQ("foo", base::StringPiece(read_buf->data(), rv));

  // Read EOF to destroy the connection and the stream.
  // This calls SpdySession::Delegate::OnClose().
  EXPECT_TRUE(session);
  EXPECT_TRUE(stream);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session);
  EXPECT_FALSE(stream);

  // Two socket reads are concatenated by WebSocketSpdyStreamAdapter.
  rv = adapter.Read(read_buf.get(), kReadBufSize, CompletionOnceCallback());
  ASSERT_EQ(3, rv);
  EXPECT_EQ("bar", base::StringPiece(read_buf->data(), rv));

  rv = adapter.Read(read_buf.get(), kReadBufSize, CompletionOnceCallback());
  ASSERT_EQ(3, rv);
  EXPECT_EQ("baz", base::StringPiece(read_buf->data(), rv));

  // Even though connection and stream are already closed,
  // WebSocketSpdyStreamAdapter::Delegate::OnClose() is only called after all
  // buffered data are read.
  EXPECT_CALL(mock_delegate_, OnClose(ERR_CONNECTION_CLOSED));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

TEST_F(WebSocketSpdyStreamAdapterTest, CallDelegateOnCloseShouldNotCrash) {
  spdy::SpdySerializedFrame response_headers(
      spdy_util_.ConstructSpdyResponseHeaders(1, ResponseHeaders(), false));
  spdy::SpdySerializedFrame data_frame1(
      spdy_util_.ConstructSpdyDataFrame(1, "foo", false));
  spdy::SpdySerializedFrame data_frame2(
      spdy_util_.ConstructSpdyDataFrame(1, "bar", false));
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));
  MockRead reads[] = {CreateMockRead(response_headers, 1),
                      CreateMockRead(data_frame1, 2),
                      CreateMockRead(data_frame2, 3), CreateMockRead(rst, 4),
                      MockRead(ASYNC, 0, 5)};
  spdy::SpdySerializedFrame request_headers(spdy_util_.ConstructSpdyHeaders(
      1, RequestHeaders(), DEFAULT_PRIORITY, false));
  MockWrite writes[] = {CreateMockWrite(request_headers, 0)};
  SequencedSocketData data(reads, writes);
  AddSocketData(&data);
  AddSSLSocketData();

  EXPECT_CALL(mock_delegate_, OnHeadersSent());
  EXPECT_CALL(mock_delegate_, OnHeadersReceived(_));

  base::WeakPtr<SpdySession> session = CreateSpdySession();
  base::WeakPtr<SpdyStream> stream = CreateSpdyStream(session);
  WebSocketSpdyStreamAdapter adapter(stream, &mock_delegate_,
                                     NetLogWithSource());
  EXPECT_TRUE(adapter.is_initialized());

  int rv = stream->SendRequestHeaders(RequestHeaders(), MORE_DATA_TO_SEND);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Buffer larger than each MockRead.
  const int kReadBufSize = 1024;
  auto read_buf = base::MakeRefCounted<IOBuffer>(kReadBufSize);
  TestCompletionCallback callback;
  rv = adapter.Read(read_buf.get(), kReadBufSize, callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  ASSERT_EQ(3, rv);
  EXPECT_EQ("foo", base::StringPiece(read_buf->data(), rv));

  // Read RST_STREAM to destroy the stream.
  // This calls SpdySession::Delegate::OnClose().
  EXPECT_TRUE(session);
  EXPECT_TRUE(stream);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session);
  EXPECT_FALSE(stream);

  // Read remaining buffered data.  This will PostTask CallDelegateOnClose().
  rv = adapter.Read(read_buf.get(), kReadBufSize, CompletionOnceCallback());
  ASSERT_EQ(3, rv);
  EXPECT_EQ("bar", base::StringPiece(read_buf->data(), rv));

  adapter.DetachDelegate();

  // Run CallDelegateOnClose(), which should not crash
  // even if |delegate_| is null.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

TEST_F(WebSocketSpdyStreamAdapterTest, Write) {
  spdy::SpdySerializedFrame response_headers(
      spdy_util_.ConstructSpdyResponseHeaders(1, ResponseHeaders(), false));
  MockRead reads[] = {CreateMockRead(response_headers, 1),
                      MockRead(ASYNC, 0, 3)};
  spdy::SpdySerializedFrame request_headers(spdy_util_.ConstructSpdyHeaders(
      1, RequestHeaders(), DEFAULT_PRIORITY, false));
  spdy::SpdySerializedFrame data_frame(
      spdy_util_.ConstructSpdyDataFrame(1, "foo", false));
  MockWrite writes[] = {CreateMockWrite(request_headers, 0),
                        CreateMockWrite(data_frame, 2)};
  SequencedSocketData data(reads, writes);
  AddSocketData(&data);
  AddSSLSocketData();

  base::WeakPtr<SpdySession> session = CreateSpdySession();
  base::WeakPtr<SpdyStream> stream = CreateSpdyStream(session);
  WebSocketSpdyStreamAdapter adapter(stream, nullptr, NetLogWithSource());
  EXPECT_TRUE(adapter.is_initialized());

  int rv = stream->SendRequestHeaders(RequestHeaders(), MORE_DATA_TO_SEND);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  base::RunLoop().RunUntilIdle();

  auto write_buf = base::MakeRefCounted<StringIOBuffer>("foo");
  TestCompletionCallback callback;
  rv = adapter.Write(write_buf.get(), write_buf->size(), callback.callback(),
                     TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  ASSERT_EQ(3, rv);

  // Read EOF.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

// Test that if both Read() and Write() returns asynchronously,
// the two callbacks are handled correctly.
TEST_F(WebSocketSpdyStreamAdapterTest, AsyncReadAndWrite) {
  spdy::SpdySerializedFrame response_headers(
      spdy_util_.ConstructSpdyResponseHeaders(1, ResponseHeaders(), false));
  spdy::SpdySerializedFrame read_data_frame(
      spdy_util_.ConstructSpdyDataFrame(1, "foobar", true));
  MockRead reads[] = {CreateMockRead(response_headers, 1),
                      CreateMockRead(read_data_frame, 3),
                      MockRead(ASYNC, 0, 4)};
  spdy::SpdySerializedFrame request_headers(spdy_util_.ConstructSpdyHeaders(
      1, RequestHeaders(), DEFAULT_PRIORITY, false));
  spdy::SpdySerializedFrame write_data_frame(
      spdy_util_.ConstructSpdyDataFrame(1, "baz", false));
  MockWrite writes[] = {CreateMockWrite(request_headers, 0),
                        CreateMockWrite(write_data_frame, 2)};
  SequencedSocketData data(reads, writes);
  AddSocketData(&data);
  AddSSLSocketData();

  base::WeakPtr<SpdySession> session = CreateSpdySession();
  base::WeakPtr<SpdyStream> stream = CreateSpdyStream(session);
  WebSocketSpdyStreamAdapter adapter(stream, nullptr, NetLogWithSource());
  EXPECT_TRUE(adapter.is_initialized());

  int rv = stream->SendRequestHeaders(RequestHeaders(), MORE_DATA_TO_SEND);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  base::RunLoop().RunUntilIdle();

  const int kReadBufSize = 1024;
  auto read_buf = base::MakeRefCounted<IOBuffer>(kReadBufSize);
  TestCompletionCallback read_callback;
  rv = adapter.Read(read_buf.get(), kReadBufSize, read_callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  auto write_buf = base::MakeRefCounted<StringIOBuffer>("baz");
  TestCompletionCallback write_callback;
  rv = adapter.Write(write_buf.get(), write_buf->size(),
                     write_callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  rv = read_callback.WaitForResult();
  ASSERT_EQ(6, rv);
  EXPECT_EQ("foobar", base::StringPiece(read_buf->data(), rv));

  rv = write_callback.WaitForResult();
  ASSERT_EQ(3, rv);

  // Read EOF.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

// A helper class that will delete |adapter| when the callback is invoked.
class KillerCallback : public TestCompletionCallbackBase {
 public:
  explicit KillerCallback(std::unique_ptr<WebSocketSpdyStreamAdapter> adapter)
      : adapter_(std::move(adapter)) {}

  ~KillerCallback() override = default;

  CompletionOnceCallback callback() {
    return base::BindOnce(&KillerCallback::OnComplete, base::Unretained(this));
  }

 private:
  void OnComplete(int result) {
    adapter_.reset();
    SetResult(result);
  }

  std::unique_ptr<WebSocketSpdyStreamAdapter> adapter_;
};

TEST_F(WebSocketSpdyStreamAdapterTest, ReadCallbackDestroysAdapter) {
  spdy::SpdySerializedFrame response_headers(
      spdy_util_.ConstructSpdyResponseHeaders(1, ResponseHeaders(), false));
  MockRead reads[] = {CreateMockRead(response_headers, 1),
                      MockRead(ASYNC, ERR_IO_PENDING, 2),
                      MockRead(ASYNC, 0, 3)};
  spdy::SpdySerializedFrame request_headers(spdy_util_.ConstructSpdyHeaders(
      1, RequestHeaders(), DEFAULT_PRIORITY, false));
  MockWrite writes[] = {CreateMockWrite(request_headers, 0)};
  SequencedSocketData data(reads, writes);
  AddSocketData(&data);
  AddSSLSocketData();

  EXPECT_CALL(mock_delegate_, OnHeadersSent());
  EXPECT_CALL(mock_delegate_, OnHeadersReceived(_));

  base::WeakPtr<SpdySession> session = CreateSpdySession();
  base::WeakPtr<SpdyStream> stream = CreateSpdyStream(session);
  auto adapter = std::make_unique<WebSocketSpdyStreamAdapter>(
      stream, &mock_delegate_, NetLogWithSource());
  EXPECT_TRUE(adapter->is_initialized());

  int rv = stream->SendRequestHeaders(RequestHeaders(), MORE_DATA_TO_SEND);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Send headers.
  base::RunLoop().RunUntilIdle();

  WebSocketSpdyStreamAdapter* adapter_raw = adapter.get();
  KillerCallback callback(std::move(adapter));

  const int kReadBufSize = 1024;
  auto read_buf = base::MakeRefCounted<IOBuffer>(kReadBufSize);
  rv = adapter_raw->Read(read_buf.get(), kReadBufSize, callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Read EOF while read is pending.  WebSocketSpdyStreamAdapter::OnClose()
  // should not crash if read callback destroys |adapter|.
  data.Resume();
  rv = callback.WaitForResult();
  EXPECT_THAT(rv, IsError(ERR_CONNECTION_CLOSED));

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session);
  EXPECT_FALSE(stream);

  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

TEST_F(WebSocketSpdyStreamAdapterTest, WriteCallbackDestroysAdapter) {
  spdy::SpdySerializedFrame response_headers(
      spdy_util_.ConstructSpdyResponseHeaders(1, ResponseHeaders(), false));
  MockRead reads[] = {CreateMockRead(response_headers, 1),
                      MockRead(ASYNC, ERR_IO_PENDING, 2),
                      MockRead(ASYNC, 0, 3)};
  spdy::SpdySerializedFrame request_headers(spdy_util_.ConstructSpdyHeaders(
      1, RequestHeaders(), DEFAULT_PRIORITY, false));
  MockWrite writes[] = {CreateMockWrite(request_headers, 0)};
  SequencedSocketData data(reads, writes);
  AddSocketData(&data);
  AddSSLSocketData();

  EXPECT_CALL(mock_delegate_, OnHeadersSent());
  EXPECT_CALL(mock_delegate_, OnHeadersReceived(_));

  base::WeakPtr<SpdySession> session = CreateSpdySession();
  base::WeakPtr<SpdyStream> stream = CreateSpdyStream(session);
  auto adapter = std::make_unique<WebSocketSpdyStreamAdapter>(
      stream, &mock_delegate_, NetLogWithSource());
  EXPECT_TRUE(adapter->is_initialized());

  int rv = stream->SendRequestHeaders(RequestHeaders(), MORE_DATA_TO_SEND);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Send headers.
  base::RunLoop().RunUntilIdle();

  WebSocketSpdyStreamAdapter* adapter_raw = adapter.get();
  KillerCallback callback(std::move(adapter));

  auto write_buf = base::MakeRefCounted<StringIOBuffer>("foo");
  rv = adapter_raw->Write(write_buf.get(), write_buf->size(),
                          callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Read EOF while write is pending.  WebSocketSpdyStreamAdapter::OnClose()
  // should not crash if write callback destroys |adapter|.
  data.Resume();
  rv = callback.WaitForResult();
  EXPECT_THAT(rv, IsError(ERR_CONNECTION_CLOSED));

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(session);
  EXPECT_FALSE(stream);

  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

}  // namespace test

}  // namespace net
