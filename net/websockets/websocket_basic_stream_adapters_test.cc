// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_basic_stream_adapters.h"

#include <stdint.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_handle.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/request_priority.h"
#include "net/base/session_usage.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_verify_result.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_network_session.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/address_utils.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/mock_quic_data.h"
#include "net/quic/quic_chromium_alarm_factory.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_chromium_client_session_peer.h"
#include "net/quic/quic_chromium_connection_helper.h"
#include "net/quic/quic_chromium_packet_reader.h"
#include "net/quic/quic_chromium_packet_writer.h"
#include "net/quic/quic_context.h"
#include "net/quic/quic_http_utils.h"
#include "net/quic/quic_server_info.h"
#include "net/quic/quic_session_alias_key.h"
#include "net/quic/quic_session_key.h"
#include "net/quic/quic_test_packet_maker.h"
#include "net/quic/test_quic_crypto_client_config_handle.h"
#include "net/quic/test_task_runner.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/stream_socket.h"
#include "net/spdy/spdy_session_key.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/third_party/quiche/src/quiche/common/platform/api/quiche_flags.h"
#include "net/third_party/quiche/src/quiche/common/quiche_buffer_allocator.h"
#include "net/third_party/quiche/src/quiche/common/simple_buffer_allocator.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_protocol.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_crypto_client_config.h"
#include "net/third_party/quiche/src/quiche/quic/core/http/http_encoder.h"
#include "net/third_party/quiche/src/quiche/quic/core/qpack/qpack_decoder.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_connection.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_connection_id.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_error_codes.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_time.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_types.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_socket_address.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_clock.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_connection_id_generator.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_random.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/qpack/qpack_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_test_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/websockets/websocket_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace net {
class QuicChromiumClientStream;
class SpdySession;
class WebSocketEndpointLockManager;
class X509Certificate;
}  // namespace net

using testing::_;
using testing::AnyNumber;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;
using testing::Test;

namespace net::test {

class WebSocketClientSocketHandleAdapterTest : public TestWithTaskEnvironment {
 protected:
  WebSocketClientSocketHandleAdapterTest()
      : network_session_(
            SpdySessionDependencies::SpdyCreateSession(&session_deps_)),
        websocket_endpoint_lock_manager_(
            network_session_->websocket_endpoint_lock_manager()) {}

  ~WebSocketClientSocketHandleAdapterTest() override = default;

  bool InitClientSocketHandle(ClientSocketHandle* connection) {
    scoped_refptr<ClientSocketPool::SocketParams> socks_params =
        base::MakeRefCounted<ClientSocketPool::SocketParams>(
            /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>());
    TestCompletionCallback callback;
    int rv = connection->Init(
        ClientSocketPool::GroupId(
            url::SchemeHostPort(url::kHttpsScheme, "www.example.org", 443),
            PrivacyMode::PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
            SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false),
        socks_params, /*proxy_annotation_tag=*/TRAFFIC_ANNOTATION_FOR_TESTS,
        MEDIUM, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
        callback.callback(), ClientSocketPool::ProxyAuthCallback(),
        network_session_->GetSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL,
                                        ProxyChain::Direct()),
        NetLogWithSource());
    rv = callback.GetResult(rv);
    return rv == OK;
  }

  SpdySessionDependencies session_deps_;
  std::unique_ptr<HttpNetworkSession> network_session_;
  raw_ptr<WebSocketEndpointLockManager> websocket_endpoint_lock_manager_;
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
  constexpr int kReadBufSize = 1024;
  auto read_buf = base::MakeRefCounted<IOBufferWithSize>(kReadBufSize);
  int rv = adapter.Read(read_buf.get(), kReadBufSize, CompletionOnceCallback());
  ASSERT_EQ(3, rv);
  EXPECT_EQ("foo", std::string_view(read_buf->data(), rv));

  TestCompletionCallback callback;
  rv = adapter.Read(read_buf.get(), kReadBufSize, callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  ASSERT_EQ(3, rv);
  EXPECT_EQ("bar", std::string_view(read_buf->data(), rv));

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
  constexpr int kReadBufSize = 2;
  auto read_buf = base::MakeRefCounted<IOBufferWithSize>(kReadBufSize);
  int rv = adapter.Read(read_buf.get(), kReadBufSize, CompletionOnceCallback());
  ASSERT_EQ(2, rv);
  EXPECT_EQ("fo", std::string_view(read_buf->data(), rv));

  rv = adapter.Read(read_buf.get(), kReadBufSize, CompletionOnceCallback());
  ASSERT_EQ(1, rv);
  EXPECT_EQ("o", std::string_view(read_buf->data(), rv));

  TestCompletionCallback callback1;
  rv = adapter.Read(read_buf.get(), kReadBufSize, callback1.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback1.WaitForResult();
  ASSERT_EQ(2, rv);
  EXPECT_EQ("ba", std::string_view(read_buf->data(), rv));

  rv = adapter.Read(read_buf.get(), kReadBufSize, CompletionOnceCallback());
  ASSERT_EQ(1, rv);
  EXPECT_EQ("r", std::string_view(read_buf->data(), rv));

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

  constexpr int kReadBufSize = 1024;
  auto read_buf = base::MakeRefCounted<IOBufferWithSize>(kReadBufSize);
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
  EXPECT_EQ("foobar", std::string_view(read_buf->data(), rv));

  rv = write_callback.WaitForResult();
  ASSERT_EQ(3, rv);

  EXPECT_TRUE(data.AllReadDataConsumed());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

class MockDelegate : public WebSocketSpdyStreamAdapter::Delegate {
 public:
  ~MockDelegate() override = default;
  MOCK_METHOD(void, OnHeadersSent, (), (override));
  MOCK_METHOD(void,
              OnHeadersReceived,
              (const quiche::HttpHeaderBlock&),
              (override));
  MOCK_METHOD(void, OnClose, (int), (override));
};

class WebSocketSpdyStreamAdapterTest : public TestWithTaskEnvironment {
 protected:
  WebSocketSpdyStreamAdapterTest()
      : url_("wss://www.example.org/"),
        key_(HostPortPair::FromURL(url_),
             PRIVACY_MODE_DISABLED,
             ProxyChain::Direct(),
             SessionUsage::kDestination,
             SocketTag(),
             NetworkAnonymizationKey(),
             SecureDnsPolicy::kAllow,
             /*disable_cert_verification_network_fetches=*/false),
        session_(SpdySessionDependencies::SpdyCreateSession(&session_deps_)),
        ssl_(SYNCHRONOUS, OK) {}

  ~WebSocketSpdyStreamAdapterTest() override = default;

  static quiche::HttpHeaderBlock RequestHeaders() {
    return WebSocketHttp2Request("/", "www.example.org:443",
                                 "http://www.example.org", {});
  }

  static quiche::HttpHeaderBlock ResponseHeaders() {
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

// Previously we failed to detect a half-close by the server that indicated the
// stream should be closed. This test ensures a half-close is correctly
// detected. See https://crbug.com/1151393.
TEST_F(WebSocketSpdyStreamAdapterTest, OnHeadersReceivedThenStreamEnd) {
  spdy::SpdySerializedFrame response_headers(
      spdy_util_.ConstructSpdyResponseHeaders(1, ResponseHeaders(), false));
  spdy::SpdySerializedFrame stream_end(
      spdy_util_.ConstructSpdyDataFrame(1, "", true));
  MockRead reads[] = {CreateMockRead(response_headers, 1),
                      CreateMockRead(stream_end, 2),
                      MockRead(ASYNC, ERR_IO_PENDING, 3),  // pause here
                      MockRead(ASYNC, 0, 4)};
  spdy::SpdySerializedFrame request_headers(spdy_util_.ConstructSpdyHeaders(
      1, RequestHeaders(), DEFAULT_PRIORITY, /* fin = */ false));
  MockWrite writes[] = {CreateMockWrite(request_headers, 0)};
  SequencedSocketData data(reads, writes);
  AddSocketData(&data);
  AddSSLSocketData();

  EXPECT_CALL(mock_delegate_, OnHeadersSent());
  EXPECT_CALL(mock_delegate_, OnHeadersReceived(_));
  EXPECT_CALL(mock_delegate_, OnClose(ERR_CONNECTION_CLOSED));

  // Must create buffer before `adapter`, since `adapter` doesn't hold onto a
  // reference to it.
  constexpr int kReadBufSize = 1024;
  auto read_buf = base::MakeRefCounted<IOBufferWithSize>(kReadBufSize);

  base::WeakPtr<SpdySession> session = CreateSpdySession();
  base::WeakPtr<SpdyStream> stream = CreateSpdyStream(session);
  WebSocketSpdyStreamAdapter adapter(stream, &mock_delegate_,
                                     NetLogWithSource());
  EXPECT_TRUE(adapter.is_initialized());

  int rv = stream->SendRequestHeaders(RequestHeaders(), MORE_DATA_TO_SEND);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  TestCompletionCallback read_callback;
  rv = adapter.Read(read_buf.get(), kReadBufSize, read_callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  EXPECT_TRUE(session);
  EXPECT_TRUE(stream);
  rv = read_callback.WaitForResult();
  EXPECT_EQ(ERR_CONNECTION_CLOSED, rv);
  EXPECT_TRUE(session);
  EXPECT_FALSE(stream);

  // Close the session.
  data.Resume();

  base::RunLoop().RunUntilIdle();

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

  constexpr int kReadBufSize = 3;
  auto read_buf = base::MakeRefCounted<IOBufferWithSize>(kReadBufSize);
  TestCompletionCallback callback;
  rv = adapter.Read(read_buf.get(), kReadBufSize, callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  ASSERT_EQ(3, rv);
  EXPECT_EQ("foo", std::string_view(read_buf->data(), rv));

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
  EXPECT_EQ("bar", std::string_view(read_buf->data(), rv));

  rv = adapter.Read(read_buf.get(), kReadBufSize, CompletionOnceCallback());
  ASSERT_EQ(3, rv);
  EXPECT_EQ("baz", std::string_view(read_buf->data(), rv));

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
  constexpr int kReadBufSize = 1024;
  auto read_buf = base::MakeRefCounted<IOBufferWithSize>(kReadBufSize);
  TestCompletionCallback callback;
  rv = adapter.Read(read_buf.get(), kReadBufSize, callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  ASSERT_EQ(3, rv);
  EXPECT_EQ("foo", std::string_view(read_buf->data(), rv));

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
  EXPECT_EQ("bar", std::string_view(read_buf->data(), rv));

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

  constexpr int kReadBufSize = 1024;
  auto read_buf = base::MakeRefCounted<IOBufferWithSize>(kReadBufSize);
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
  EXPECT_EQ("foobar", std::string_view(read_buf->data(), rv));

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

  constexpr int kReadBufSize = 1024;
  auto read_buf = base::MakeRefCounted<IOBufferWithSize>(kReadBufSize);
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

TEST_F(WebSocketSpdyStreamAdapterTest,
       OnCloseOkShouldBeTranslatedToConnectionClose) {
  spdy::SpdySerializedFrame response_headers(
      spdy_util_.ConstructSpdyResponseHeaders(1, ResponseHeaders(), false));
  spdy::SpdySerializedFrame close(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_NO_ERROR));
  MockRead reads[] = {CreateMockRead(response_headers, 1),
                      CreateMockRead(close, 2), MockRead(ASYNC, 0, 3)};
  spdy::SpdySerializedFrame request_headers(spdy_util_.ConstructSpdyHeaders(
      1, RequestHeaders(), DEFAULT_PRIORITY, false));
  MockWrite writes[] = {CreateMockWrite(request_headers, 0)};
  SequencedSocketData data(reads, writes);
  AddSocketData(&data);
  AddSSLSocketData();

  EXPECT_CALL(mock_delegate_, OnHeadersSent());
  EXPECT_CALL(mock_delegate_, OnHeadersReceived(_));

  // Must create buffer before `adapter`, since `adapter` doesn't hold onto a
  // reference to it.
  constexpr int kReadBufSize = 1024;
  auto read_buf = base::MakeRefCounted<IOBufferWithSize>(kReadBufSize);

  base::WeakPtr<SpdySession> session = CreateSpdySession();
  base::WeakPtr<SpdyStream> stream = CreateSpdyStream(session);
  WebSocketSpdyStreamAdapter adapter(stream, &mock_delegate_,
                                     NetLogWithSource());
  EXPECT_TRUE(adapter.is_initialized());

  EXPECT_CALL(mock_delegate_, OnClose(ERR_CONNECTION_CLOSED));

  int rv = stream->SendRequestHeaders(RequestHeaders(), MORE_DATA_TO_SEND);
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  TestCompletionCallback callback;
  rv = adapter.Read(read_buf.get(), kReadBufSize, callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  ASSERT_EQ(ERR_CONNECTION_CLOSED, rv);
}

class MockQuicDelegate : public WebSocketQuicStreamAdapter::Delegate {
 public:
  ~MockQuicDelegate() override = default;
  MOCK_METHOD(void, OnHeadersSent, (), (override));
  MOCK_METHOD(void,
              OnHeadersReceived,
              (const quiche::HttpHeaderBlock&),
              (override));
  MOCK_METHOD(void, OnClose, (int), (override));
};

class WebSocketQuicStreamAdapterTest
    : public TestWithTaskEnvironment,
      public ::testing::WithParamInterface<quic::ParsedQuicVersion> {
 protected:
  static quiche::HttpHeaderBlock RequestHeaders() {
    return WebSocketHttp2Request("/", "www.example.org:443",
                                 "http://www.example.org", {});
  }
  WebSocketQuicStreamAdapterTest()
      : version_(GetParam()),
        mock_quic_data_(version_),
        client_data_stream_id1_(quic::QuicUtils::GetFirstBidirectionalStreamId(
            version_.transport_version,
            quic::Perspective::IS_CLIENT)),
        crypto_config_(
            quic::test::crypto_test_utils::ProofVerifierForTesting()),
        connection_id_(quic::test::TestConnectionId(2)),
        client_maker_(version_,
                      connection_id_,
                      &clock_,
                      "mail.example.org",
                      quic::Perspective::IS_CLIENT),
        server_maker_(version_,
                      connection_id_,
                      &clock_,
                      "mail.example.org",
                      quic::Perspective::IS_SERVER),
        peer_addr_(IPAddress(192, 0, 2, 23), 443),
        destination_endpoint_(url::kHttpsScheme, "mail.example.org", 80) {}

  ~WebSocketQuicStreamAdapterTest() override = default;

  void SetUp() override {
    FLAGS_quic_enable_http3_grease_randomness = false;
    clock_.AdvanceTime(quic::QuicTime::Delta::FromMilliseconds(20));
    quic::QuicEnableVersion(version_);
  }

  void TearDown() override {
    EXPECT_TRUE(mock_quic_data_.AllReadDataConsumed());
    EXPECT_TRUE(mock_quic_data_.AllWriteDataConsumed());
  }

  net::QuicChromiumClientSession::Handle* GetQuicSessionHandle() {
    return session_handle_.get();
  }

  // Helper functions for constructing packets sent by the client

  std::unique_ptr<quic::QuicReceivedPacket> ConstructSettingsPacket(
      uint64_t packet_number) {
    return client_maker_.MakeInitialSettingsPacket(packet_number);
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructServerDataPacket(
      uint64_t packet_number,
      std::string_view data) {
    quiche::QuicheBuffer buffer = quic::HttpEncoder::SerializeDataFrameHeader(
        data.size(), quiche::SimpleBufferAllocator::Get());
    return server_maker_.Packet(packet_number)
        .AddStreamFrame(
            client_data_stream_id1_, /*fin=*/false,
            base::StrCat(
                {std::string_view(buffer.data(), buffer.size()), data}))
        .Build();
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructRstPacket(
      uint64_t packet_number,
      quic::QuicRstStreamErrorCode error_code) {
    return client_maker_.Packet(packet_number)
        .AddStopSendingFrame(client_data_stream_id1_, error_code)
        .AddRstStreamFrame(client_data_stream_id1_, error_code)
        .Build();
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructClientAckPacket(
      uint64_t packet_number,
      uint64_t largest_received,
      uint64_t smallest_received) {
    return client_maker_.Packet(packet_number)
        .AddAckFrame(1, largest_received, smallest_received)
        .Build();
  }

  std::unique_ptr<quic::QuicReceivedPacket> ConstructAckAndRstPacket(
      uint64_t packet_number,
      quic::QuicRstStreamErrorCode error_code,
      uint64_t largest_received,
      uint64_t smallest_received) {
    return client_maker_.Packet(packet_number)
        .AddAckFrame(/*first_received=*/1, largest_received, smallest_received)
        .AddStopSendingFrame(client_data_stream_id1_, error_code)
        .AddRstStreamFrame(client_data_stream_id1_, error_code)
        .Build();
  }

  void Initialize() {
    auto socket = std::make_unique<MockUDPClientSocket>(
        mock_quic_data_.InitializeAndGetSequencedSocketData(), NetLog::Get());
    socket->Connect(peer_addr_);

    runner_ = base::MakeRefCounted<TestTaskRunner>(&clock_);
    helper_ = std::make_unique<QuicChromiumConnectionHelper>(
        &clock_, &random_generator_);
    alarm_factory_ =
        std::make_unique<QuicChromiumAlarmFactory>(runner_.get(), &clock_);
    // Ownership of 'writer' is passed to 'QuicConnection'.
    QuicChromiumPacketWriter* writer = new QuicChromiumPacketWriter(
        socket.get(), base::SingleThreadTaskRunner::GetCurrentDefault().get());
    quic::QuicConnection* connection = new quic::QuicConnection(
        connection_id_, quic::QuicSocketAddress(),
        net::ToQuicSocketAddress(peer_addr_), helper_.get(),
        alarm_factory_.get(), writer, true /* owns_writer */,
        quic::Perspective::IS_CLIENT, quic::test::SupportedVersions(version_),
        connection_id_generator_);
    connection->set_visitor(&visitor_);

    // Load a certificate that is valid for *.example.org
    scoped_refptr<X509Certificate> test_cert(
        ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
    EXPECT_TRUE(test_cert.get());

    verify_details_.cert_verify_result.verified_cert = test_cert;
    verify_details_.cert_verify_result.is_issued_by_known_root = true;
    crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details_);

    base::TimeTicks dns_end = base::TimeTicks::Now();
    base::TimeTicks dns_start = dns_end - base::Milliseconds(1);

    session_ = std::make_unique<QuicChromiumClientSession>(
        connection, std::move(socket),
        /*stream_factory=*/nullptr, &crypto_client_stream_factory_, &clock_,
        &transport_security_state_, &ssl_config_service_,
        /*server_info=*/nullptr,
        QuicSessionAliasKey(
            url::SchemeHostPort(),
            QuicSessionKey("mail.example.org", 80, PRIVACY_MODE_DISABLED,
                           ProxyChain::Direct(), SessionUsage::kDestination,
                           SocketTag(), NetworkAnonymizationKey(),
                           SecureDnsPolicy::kAllow,
                           /*require_dns_https_alpn=*/false)),
        /*require_confirmation=*/false,
        /*migrate_session_early_v2=*/false,
        /*migrate_session_on_network_change_v2=*/false,
        /*default_network=*/handles::kInvalidNetworkHandle,
        quic::QuicTime::Delta::FromMilliseconds(
            kDefaultRetransmittableOnWireTimeout.InMilliseconds()),
        /*migrate_idle_session=*/true, /*allow_port_migration=*/false,
        kDefaultIdleSessionMigrationPeriod, /*multi_port_probing_interval=*/0,
        kMaxTimeOnNonDefaultNetwork,
        kMaxMigrationsToNonDefaultNetworkOnWriteError,
        kMaxMigrationsToNonDefaultNetworkOnPathDegrading,
        kQuicYieldAfterPacketsRead,
        quic::QuicTime::Delta::FromMilliseconds(
            kQuicYieldAfterDurationMilliseconds),
        /*cert_verify_flags=*/0, quic::test::DefaultQuicConfig(),
        std::make_unique<TestQuicCryptoClientConfigHandle>(&crypto_config_),
        "CONNECTION_UNKNOWN", dns_start, dns_end,
        base::DefaultTickClock::GetInstance(),
        base::SingleThreadTaskRunner::GetCurrentDefault().get(),
        /*socket_performance_watcher=*/nullptr, ConnectionEndpointMetadata(),
        /*report_ecn=*/true, /*enable_origin_frame=*/true,
        NetLogWithSource::Make(NetLogSourceType::NONE));

    session_->Initialize();

    // Blackhole QPACK decoder stream instead of constructing mock writes.
    session_->qpack_decoder()->set_qpack_stream_sender_delegate(
        &noop_qpack_stream_sender_delegate_);
    TestCompletionCallback callback;
    EXPECT_THAT(session_->CryptoConnect(callback.callback()), IsOk());
    EXPECT_TRUE(session_->OneRttKeysAvailable());
    session_handle_ = session_->CreateHandle(
        url::SchemeHostPort(url::kHttpsScheme, "mail.example.org", 80));
  }

  const quic::ParsedQuicVersion version_;
  MockQuicData mock_quic_data_;
  StrictMock<MockQuicDelegate> mock_delegate_;
  const quic::QuicStreamId client_data_stream_id1_;

 private:
  quic::QuicCryptoClientConfig crypto_config_;
  const quic::QuicConnectionId connection_id_;

 protected:
  QuicTestPacketMaker client_maker_;
  QuicTestPacketMaker server_maker_;
  std::unique_ptr<QuicChromiumClientSession> session_;

 private:
  quic::MockClock clock_;
  std::unique_ptr<QuicChromiumClientSession::Handle> session_handle_;
  scoped_refptr<TestTaskRunner> runner_;
  ProofVerifyDetailsChromium verify_details_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  SSLConfigServiceDefaults ssl_config_service_;
  quic::test::MockConnectionIdGenerator connection_id_generator_;
  std::unique_ptr<QuicChromiumConnectionHelper> helper_;
  std::unique_ptr<QuicChromiumAlarmFactory> alarm_factory_;
  testing::StrictMock<quic::test::MockQuicConnectionVisitor> visitor_;
  TransportSecurityState transport_security_state_;
  IPAddress ip_;
  IPEndPoint peer_addr_;
  quic::test::MockRandom random_generator_{0};
  url::SchemeHostPort destination_endpoint_;
  quic::test::NoopQpackStreamSenderDelegate noop_qpack_stream_sender_delegate_;
};

// Like net::TestCompletionCallback, but for a callback that takes an unbound
// parameter of type WebSocketQuicStreamAdapter.
struct WebSocketQuicStreamAdapterIsPendingHelper {
  bool operator()(
      const std::unique_ptr<WebSocketQuicStreamAdapter>& adapter) const {
    return !adapter;
  }
};

using TestWebSocketQuicStreamAdapterCompletionCallbackBase =
    net::internal::TestCompletionCallbackTemplate<
        std::unique_ptr<WebSocketQuicStreamAdapter>,
        WebSocketQuicStreamAdapterIsPendingHelper>;

class TestWebSocketQuicStreamAdapterCompletionCallback
    : public TestWebSocketQuicStreamAdapterCompletionCallbackBase {
 public:
  base::OnceCallback<void(std::unique_ptr<WebSocketQuicStreamAdapter>)>
  callback();
};

base::OnceCallback<void(std::unique_ptr<WebSocketQuicStreamAdapter>)>
TestWebSocketQuicStreamAdapterCompletionCallback::callback() {
  return base::BindOnce(
      &TestWebSocketQuicStreamAdapterCompletionCallback::SetResult,
      base::Unretained(this));
}

INSTANTIATE_TEST_SUITE_P(QuicVersion,
                         WebSocketQuicStreamAdapterTest,
                         ::testing::ValuesIn(AllSupportedQuicVersions()),
                         ::testing::PrintToStringParamName());

TEST_P(WebSocketQuicStreamAdapterTest, Disconnect) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));

  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructRstPacket(packet_number++, quic::QUIC_STREAM_CANCELLED));

  Initialize();

  net::QuicChromiumClientSession::Handle* session_handle =
      GetQuicSessionHandle();
  ASSERT_TRUE(session_handle);

  TestWebSocketQuicStreamAdapterCompletionCallback callback;
  std::unique_ptr<WebSocketQuicStreamAdapter> adapter =
      session_handle->CreateWebSocketQuicStreamAdapter(
          &mock_delegate_, callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  ASSERT_TRUE(adapter);
  EXPECT_TRUE(adapter->is_initialized());
  adapter->Disconnect();
  // TODO(momoka): Add tests to test both destruction orders.
}

TEST_P(WebSocketQuicStreamAdapterTest, AsyncAdapterCreation) {
  constexpr size_t kMaxOpenStreams = 50;

  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));

  mock_quic_data_.AddWrite(
      SYNCHRONOUS, client_maker_.Packet(packet_number++)
                       .AddStreamsBlockedFrame(/*control_frame_id=*/1,
                                               /*stream_count=*/kMaxOpenStreams,
                                               /* unidirectional = */ false)
                       .Build());

  mock_quic_data_.AddRead(
      ASYNC, server_maker_.Packet(1)
                 .AddMaxStreamsFrame(/*control_frame_id=*/1,
                                     /*stream_count=*/kMaxOpenStreams + 2,
                                     /* unidirectional = */ false)
                 .Build());

  mock_quic_data_.AddRead(ASYNC, ERR_IO_PENDING);
  mock_quic_data_.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  Initialize();

  std::vector<QuicChromiumClientStream*> streams;

  for (size_t i = 0; i < kMaxOpenStreams; i++) {
    QuicChromiumClientStream* stream =
        QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
    ASSERT_TRUE(stream);
    streams.push_back(stream);
    EXPECT_EQ(i + 1, session_->GetNumActiveStreams());
  }

  net::QuicChromiumClientSession::Handle* session_handle =
      GetQuicSessionHandle();
  ASSERT_TRUE(session_handle);

  // Creating an adapter should fail because of the stream limit.
  TestWebSocketQuicStreamAdapterCompletionCallback callback;
  std::unique_ptr<WebSocketQuicStreamAdapter> adapter =
      session_handle->CreateWebSocketQuicStreamAdapter(
          &mock_delegate_, callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  ASSERT_EQ(adapter, nullptr);
  EXPECT_FALSE(callback.have_result());
  EXPECT_EQ(kMaxOpenStreams, session_->GetNumActiveStreams());

  // Read MAX_STREAMS frame that makes it possible to open WebSocket stream.
  session_->StartReading();
  callback.WaitForResult();
  EXPECT_EQ(kMaxOpenStreams + 1, session_->GetNumActiveStreams());

  // Close connection.
  mock_quic_data_.Resume();
  base::RunLoop().RunUntilIdle();
}

TEST_P(WebSocketQuicStreamAdapterTest, SendRequestHeadersThenDisconnect) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  SpdyTestUtil spdy_util;
  quiche::HttpHeaderBlock request_header_block = WebSocketHttp2Request(
      "/", "www.example.org:443", "http://www.example.org", {});
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRequestHeadersPacket(
          packet_number++, client_data_stream_id1_,
          /*fin=*/false, ConvertRequestPriorityToQuicPriority(LOWEST),
          std::move(request_header_block), nullptr));

  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructRstPacket(packet_number++, quic::QUIC_STREAM_CANCELLED));

  Initialize();

  net::QuicChromiumClientSession::Handle* session_handle =
      GetQuicSessionHandle();
  ASSERT_TRUE(session_handle);
  TestWebSocketQuicStreamAdapterCompletionCallback callback;
  std::unique_ptr<WebSocketQuicStreamAdapter> adapter =
      session_handle->CreateWebSocketQuicStreamAdapter(
          &mock_delegate_, callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  ASSERT_TRUE(adapter);
  EXPECT_TRUE(adapter->is_initialized());

  adapter->WriteHeaders(RequestHeaders(), false);

  adapter->Disconnect();
}

TEST_P(WebSocketQuicStreamAdapterTest, OnHeadersReceivedThenDisconnect) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));

  SpdyTestUtil spdy_util;
  quiche::HttpHeaderBlock request_header_block = WebSocketHttp2Request(
      "/", "www.example.org:443", "http://www.example.org", {});
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRequestHeadersPacket(
          packet_number++, client_data_stream_id1_,
          /*fin=*/false, ConvertRequestPriorityToQuicPriority(LOWEST),
          std::move(request_header_block), nullptr));

  quiche::HttpHeaderBlock response_header_block = WebSocketHttp2Response({});
  mock_quic_data_.AddRead(
      ASYNC, server_maker_.MakeResponseHeadersPacket(
                 /*packet_number=*/1, client_data_stream_id1_, /*fin=*/false,
                 std::move(response_header_block),
                 /*spdy_headers_frame_length=*/nullptr));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(packet_number++,
                                            quic::QUIC_STREAM_CANCELLED, 1, 0));
  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(mock_delegate_, OnHeadersReceived(_)).WillOnce(Invoke([&]() {
    std::move(quit_closure).Run();
  }));

  Initialize();

  net::QuicChromiumClientSession::Handle* session_handle =
      GetQuicSessionHandle();
  ASSERT_TRUE(session_handle);

  TestWebSocketQuicStreamAdapterCompletionCallback callback;
  std::unique_ptr<WebSocketQuicStreamAdapter> adapter =
      session_handle->CreateWebSocketQuicStreamAdapter(
          &mock_delegate_, callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  ASSERT_TRUE(adapter);
  EXPECT_TRUE(adapter->is_initialized());

  adapter->WriteHeaders(RequestHeaders(), false);

  session_->StartReading();
  run_loop.Run();

  adapter->Disconnect();
}

TEST_P(WebSocketQuicStreamAdapterTest, Read) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));

  SpdyTestUtil spdy_util;
  quiche::HttpHeaderBlock request_header_block = WebSocketHttp2Request(
      "/", "www.example.org:443", "http://www.example.org", {});
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRequestHeadersPacket(
          packet_number++, client_data_stream_id1_,
          /*fin=*/false, ConvertRequestPriorityToQuicPriority(LOWEST),
          std::move(request_header_block), nullptr));

  quiche::HttpHeaderBlock response_header_block = WebSocketHttp2Response({});
  mock_quic_data_.AddRead(
      ASYNC, server_maker_.MakeResponseHeadersPacket(
                 /*packet_number=*/1, client_data_stream_id1_, /*fin=*/false,
                 std::move(response_header_block),
                 /*spdy_headers_frame_length=*/nullptr));
  mock_quic_data_.AddRead(ASYNC, ERR_IO_PENDING);

  mock_quic_data_.AddRead(ASYNC, ConstructServerDataPacket(2, "foo"));
  mock_quic_data_.AddRead(SYNCHRONOUS,
                          ConstructServerDataPacket(3, "hogehoge"));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);

  mock_quic_data_.AddWrite(ASYNC,
                           ConstructClientAckPacket(packet_number++, 2, 0));
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(packet_number++,
                                            quic::QUIC_STREAM_CANCELLED, 3, 0));

  base::RunLoop run_loop;
  EXPECT_CALL(mock_delegate_, OnHeadersReceived(_)).WillOnce(Invoke([&]() {
    run_loop.Quit();
  }));

  Initialize();

  net::QuicChromiumClientSession::Handle* session_handle =
      GetQuicSessionHandle();
  ASSERT_TRUE(session_handle);

  TestWebSocketQuicStreamAdapterCompletionCallback callback;
  std::unique_ptr<WebSocketQuicStreamAdapter> adapter =
      session_handle->CreateWebSocketQuicStreamAdapter(
          &mock_delegate_, callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  ASSERT_TRUE(adapter);
  EXPECT_TRUE(adapter->is_initialized());

  adapter->WriteHeaders(RequestHeaders(), false);

  session_->StartReading();
  run_loop.Run();

  // Buffer larger than each MockRead.
  constexpr int kReadBufSize = 1024;
  auto read_buf = base::MakeRefCounted<IOBufferWithSize>(kReadBufSize);
  TestCompletionCallback read_callback;

  int rv =
      adapter->Read(read_buf.get(), kReadBufSize, read_callback.callback());

  ASSERT_EQ(ERR_IO_PENDING, rv);

  mock_quic_data_.Resume();
  base::RunLoop().RunUntilIdle();

  rv = read_callback.WaitForResult();
  ASSERT_EQ(3, rv);
  EXPECT_EQ("foo", std::string_view(read_buf->data(), rv));

  rv = adapter->Read(read_buf.get(), kReadBufSize, CompletionOnceCallback());
  ASSERT_EQ(8, rv);
  EXPECT_EQ("hogehoge", std::string_view(read_buf->data(), rv));

  adapter->Disconnect();

  EXPECT_TRUE(mock_quic_data_.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data_.AllWriteDataConsumed());
}

TEST_P(WebSocketQuicStreamAdapterTest, ReadIntoSmallBuffer) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));

  SpdyTestUtil spdy_util;
  quiche::HttpHeaderBlock request_header_block = WebSocketHttp2Request(
      "/", "www.example.org:443", "http://www.example.org", {});
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      client_maker_.MakeRequestHeadersPacket(
          packet_number++, client_data_stream_id1_,
          /*fin=*/false, ConvertRequestPriorityToQuicPriority(LOWEST),
          std::move(request_header_block), nullptr));

  quiche::HttpHeaderBlock response_header_block = WebSocketHttp2Response({});
  mock_quic_data_.AddRead(
      ASYNC, server_maker_.MakeResponseHeadersPacket(
                 /*packet_number=*/1, client_data_stream_id1_, /*fin=*/false,
                 std::move(response_header_block),
                 /*spdy_headers_frame_length=*/nullptr));
  mock_quic_data_.AddRead(ASYNC, ERR_IO_PENDING);
  // First read is the same size as the buffer, next is smaller, last is larger.
  mock_quic_data_.AddRead(ASYNC, ConstructServerDataPacket(2, "abc"));
  mock_quic_data_.AddRead(SYNCHRONOUS, ConstructServerDataPacket(3, "12"));
  mock_quic_data_.AddRead(SYNCHRONOUS, ConstructServerDataPacket(4, "ABCD"));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);

  mock_quic_data_.AddWrite(ASYNC,
                           ConstructClientAckPacket(packet_number++, 2, 0));
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(packet_number++,
                                            quic::QUIC_STREAM_CANCELLED, 4, 0));

  base::RunLoop run_loop;
  EXPECT_CALL(mock_delegate_, OnHeadersReceived(_)).WillOnce(Invoke([&]() {
    run_loop.Quit();
  }));

  Initialize();

  net::QuicChromiumClientSession::Handle* session_handle =
      GetQuicSessionHandle();
  ASSERT_TRUE(session_handle);
  TestWebSocketQuicStreamAdapterCompletionCallback callback;
  std::unique_ptr<WebSocketQuicStreamAdapter> adapter =
      session_handle->CreateWebSocketQuicStreamAdapter(
          &mock_delegate_, callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  ASSERT_TRUE(adapter);
  EXPECT_TRUE(adapter->is_initialized());

  adapter->WriteHeaders(RequestHeaders(), false);

  session_->StartReading();
  run_loop.Run();

  constexpr int kReadBufSize = 3;
  auto read_buf = base::MakeRefCounted<IOBufferWithSize>(kReadBufSize);
  TestCompletionCallback read_callback;

  int rv =
      adapter->Read(read_buf.get(), kReadBufSize, read_callback.callback());

  ASSERT_EQ(ERR_IO_PENDING, rv);

  mock_quic_data_.Resume();
  base::RunLoop().RunUntilIdle();

  rv = read_callback.WaitForResult();
  ASSERT_EQ(3, rv);
  EXPECT_EQ("abc", std::string_view(read_buf->data(), rv));

  rv = adapter->Read(read_buf.get(), kReadBufSize, CompletionOnceCallback());
  ASSERT_EQ(3, rv);
  EXPECT_EQ("12A", std::string_view(read_buf->data(), rv));

  rv = adapter->Read(read_buf.get(), kReadBufSize, CompletionOnceCallback());
  ASSERT_EQ(3, rv);
  EXPECT_EQ("BCD", std::string_view(read_buf->data(), rv));

  adapter->Disconnect();

  EXPECT_TRUE(mock_quic_data_.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data_.AllWriteDataConsumed());
}

}  // namespace net::test
