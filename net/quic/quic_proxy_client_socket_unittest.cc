// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_proxy_client_socket.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/base/test_proxy_delegate.h"
#include "net/http/http_response_headers.h"
#include "net/log/net_log.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_http_utils.h"
#include "net/quic/quic_proxy_client_socket_test_base.h"
#include "net/quic/test_quic_crypto_client_config_handle.h"
#include "net/quic/test_task_runner.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/qpack/qpack_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_test_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

using testing::_;
using testing::AnyNumber;
using testing::Return;

namespace net::test {

namespace {

std::string ConcatenateHeaderAndBody(std::string_view header,
                                     base::span<const char> body) {
  return std::string(header) + std::string(base::as_string_view(body));
}

}  // namespace

class QuicProxyClientSocketTest : public QuicProxyClientSocketTestBase {
 public:
  void TearDown() override {
    sock_.reset();
    EXPECT_TRUE(mock_quic_data_.AllReadDataConsumed());
    EXPECT_TRUE(mock_quic_data_.AllWriteDataConsumed());
  }

  void InitializeClientSocket() override {
    sock_ = std::make_unique<QuicProxyClientSocket>(
        std::move(stream_handle_), std::move(session_handle_),
        // TODO(crbug.com/40181080) Construct `ProxyChain` with plain
        // `proxy_endpoint_` once it supports `url::SchemeHostPort`.
        ProxyChain(ProxyServer::SCHEME_HTTPS,
                   HostPortPair::FromSchemeHostPort(proxy_endpoint_)),
        /*proxy_chain_index=*/0, user_agent_,
        // TODO(crbug.com/40181080) Construct `QuicProxyClientSocket` with plain
        // `proxy_endpoint_` once it supports `url::SchemeHostPort`.
        HostPortPair::FromSchemeHostPort(destination_endpoint_),
        NetLogWithSource::Make(NetLogSourceType::NONE),
        base::MakeRefCounted<HttpAuthController>(
            HttpAuth::AUTH_PROXY, proxy_endpoint_.GetURL(),
            NetworkAnonymizationKey(), &http_auth_cache_,
            http_auth_handler_factory_.get(), host_resolver_.get()),
        proxy_delegate_.get());

    session_->StartReading();
  }

  void PopulateConnectRequestIR(
      quiche::HttpHeaderBlock* block,
      std::optional<const HttpRequestHeaders> extra_headers) override {
    (*block)[":method"] = "CONNECT";
    (*block)[":authority"] =
        HostPortPair::FromSchemeHostPort(destination_endpoint_).ToString();
    (*block)["user-agent"] = kUserAgent;
    if (extra_headers) {
      HttpRequestHeaders::Iterator it(*extra_headers);
      while (it.GetNext()) {
        std::string name = base::ToLowerASCII(it.name());
        (*block)[name] = it.value();
      }
    }
  }

  void AssertConnectSucceeds() override {
    TestCompletionCallback callback;
    ASSERT_THAT(sock_->Connect(callback.callback()),
                test::IsError(ERR_IO_PENDING));
    ASSERT_THAT(callback.WaitForResult(), test::IsOk());
  }

  void AssertConnectFails(int result) override {
    TestCompletionCallback callback;
    ASSERT_THAT(sock_->Connect(callback.callback()),
                test::IsError(ERR_IO_PENDING));
    EXPECT_EQ(result, callback.WaitForResult());
  }

  void AssertWriteReturns(base::span<const char> data, int rv) override {
    auto buf = base::MakeRefCounted<IOBufferWithSize>(data.size());
    buf->span().copy_from(base::as_bytes(data));
    EXPECT_EQ(rv,
              sock_->Write(buf.get(), buf->size(), write_callback_.callback(),
                           TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  void AssertSyncWriteSucceeds(base::span<const char> data) override {
    AssertWriteReturns(data, base::checked_cast<int>(data.size()));
  }

  void AssertSyncReadEquals(base::span<const char> data) override {
    auto buf = base::MakeRefCounted<IOBufferWithSize>(data.size());
    EXPECT_EQ(data.size(),
              sock_->Read(buf.get(), data.size(), CompletionOnceCallback()));
    EXPECT_EQ(data, base::as_chars(buf->span()));
    ASSERT_TRUE(sock_->IsConnected());
  }

  void AssertAsyncReadEquals(base::span<const char> data) override {
    AssertReadStarts(data);

    ResumeAndRun();

    AssertReadReturns(data);
  }

  void AssertReadStarts(base::span<const char> data) override {
    // Issue the read, which will be completed asynchronously.
    read_buf_ = base::MakeRefCounted<IOBufferWithSize>(data.size());
    ASSERT_EQ(ERR_IO_PENDING, sock_->Read(read_buf_.get(), data.size(),
                                          read_callback_.callback()));
    EXPECT_TRUE(sock_->IsConnected());
  }

  void AssertReadReturns(base::span<const char> data) override {
    EXPECT_TRUE(sock_->IsConnected());

    // Now the read will return.
    EXPECT_EQ(data.size(), read_callback_.WaitForResult());
    EXPECT_EQ(data, base::as_chars(read_buf_->span()));
  }

 protected:
  std::unique_ptr<QuicProxyClientSocket> sock_;
};

TEST_P(QuicProxyClientSocketTest, ConnectSendsCorrectRequest) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPauseForever();
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(packet_number++,
                                            quic::QUIC_STREAM_CANCELLED, 1, 1));

  InitializeSession();
  InitializeClientSocket();

  ASSERT_FALSE(sock_->IsConnected());

  AssertConnectSucceeds();

  const HttpResponseInfo* response = sock_->GetConnectResponseInfo();
  ASSERT_TRUE(response != nullptr);
  ASSERT_EQ(200, response->headers->response_code());

  // Although the underlying HTTP/3 connection uses TLS and negotiates ALPN, the
  // tunnel itself is a TCP connection to the origin and should not report these
  // values.
  net::SSLInfo ssl_info;
  EXPECT_FALSE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(sock_->GetNegotiatedProtocol(), NextProto::kProtoUnknown);
}

TEST_P(QuicProxyClientSocketTest, ProxyDelegateExtraHeaders) {
  // TODO(crbug.com/40284947): Add a version of this test for multi-hop.
  proxy_delegate_ = std::make_unique<TestProxyDelegate>();
  proxy_delegate_->set_extra_header_name(kTestHeaderName);
  // TODO(crbug.com/40181080) Construct `proxy_chain` with plain
  // `proxy_endpoint_` once it supports `url::SchemeHostPort`.
  ProxyChain proxy_chain(ProxyServer::SCHEME_HTTPS,
                         HostPortPair::FromSchemeHostPort(proxy_endpoint_));

  const char kResponseHeaderName[] = "bar";
  const char kResponseHeaderValue[] = "testing";

  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructConnectRequestPacketWithExtraHeaders(
          packet_number++,
          // Order matters! Keep these alphabetical.
          {{kTestQuicHeaderName, ProxyServerToProxyUri(proxy_chain.First())},
           {"user-agent", kUserAgent}}));
  mock_quic_data_.AddRead(
      ASYNC, ConstructServerConnectReplyPacketWithExtraHeaders(
                 1, !kFin, {{kResponseHeaderName, kResponseHeaderValue}}));
  mock_quic_data_.AddReadPauseForever();
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(packet_number++,
                                            quic::QUIC_STREAM_CANCELLED, 1, 1));

  InitializeSession();
  InitializeClientSocket();

  ASSERT_FALSE(sock_->IsConnected());

  AssertConnectSucceeds();

  const HttpResponseInfo* response = sock_->GetConnectResponseInfo();
  ASSERT_TRUE(response != nullptr);
  ASSERT_EQ(200, response->headers->response_code());

  ASSERT_EQ(proxy_delegate_->on_tunnel_headers_received_call_count(), 1u);
  proxy_delegate_->VerifyOnTunnelHeadersReceived(proxy_chain, /*chain_index=*/0,
                                                 kResponseHeaderName,
                                                 kResponseHeaderValue);
}

TEST_P(QuicProxyClientSocketTest, ProxyDelegateExtraHeadersAsync) {
  // TODO(crbug.com/40284947): Add a version of this test for multi-hop.
  proxy_delegate_ = std::make_unique<TestProxyDelegate>();
  proxy_delegate_->MakeOnBeforeTunnelRequestCompleteAsync();
  proxy_delegate_->set_extra_header_name(kTestHeaderName);
  // TODO(crbug.com/40181080) Construct `proxy_chain` with plain
  // `proxy_endpoint_` once it supports `url::SchemeHostPort`.
  ProxyChain proxy_chain(ProxyServer::SCHEME_HTTPS,
                         HostPortPair::FromSchemeHostPort(proxy_endpoint_));

  const char kResponseHeaderName[] = "bar";
  const char kResponseHeaderValue[] = "testing";

  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructConnectRequestPacketWithExtraHeaders(
          packet_number++,
          // Order matters! Keep these alphabetical.
          {{kTestQuicHeaderName, ProxyServerToProxyUri(proxy_chain.First())},
           {"user-agent", kUserAgent}}));
  mock_quic_data_.AddRead(
      ASYNC, ConstructServerConnectReplyPacketWithExtraHeaders(
                 1, !kFin, {{kResponseHeaderName, kResponseHeaderValue}}));
  mock_quic_data_.AddReadPauseForever();
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(packet_number++,
                                            quic::QUIC_STREAM_CANCELLED, 1, 1));

  InitializeSession();
  InitializeClientSocket();

  ASSERT_FALSE(sock_->IsConnected());

  TestCompletionCallback callback;
  ASSERT_THAT(sock_->Connect(callback.callback()),
              test::IsError(ERR_IO_PENDING));

  // This should let Connect run until the ERR_IO_PENDING returned by
  // OnBeforeTunnelRequest. This means we should have not received the tunnel
  // response headers yet.
  proxy_delegate_->WaitForOnBeforeTunnelRequestAsyncCompletion();
  ASSERT_EQ(proxy_delegate_->on_before_tunnel_request_call_count(), 1u);
  ASSERT_EQ(proxy_delegate_->on_tunnel_headers_received_call_count(), 0u);

  // Once we let TestProxyDelegate continue, Connect should be able to terminate
  // in a success.
  proxy_delegate_->ResumeOnBeforeTunnelRequest();
  ASSERT_THAT(callback.WaitForResult(), test::IsOk());
  ASSERT_EQ(proxy_delegate_->on_before_tunnel_request_call_count(), 1u);
  ASSERT_EQ(proxy_delegate_->on_tunnel_headers_received_call_count(), 1u);

  const HttpResponseInfo* response = sock_->GetConnectResponseInfo();
  ASSERT_TRUE(response != nullptr);
  ASSERT_EQ(200, response->headers->response_code());

  ASSERT_EQ(proxy_delegate_->on_tunnel_headers_received_call_count(), 1u);
  proxy_delegate_->VerifyOnTunnelHeadersReceived(proxy_chain, /*chain_index=*/0,
                                                 kResponseHeaderName,
                                                 kResponseHeaderValue);
}

TEST_P(QuicProxyClientSocketTest,
       ProxyDelegateOnTunnelHeadersReceivedSucceedsAsync) {
  proxy_delegate_ = std::make_unique<TestProxyDelegate>();
  proxy_delegate_->MakeOnTunnelHeadersReceivedCompleteAsync();

  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(
      ASYNC, ConstructServerConnectReplyPacket(
                 /*packet_number=*/1, !kFin, /*header_length=*/nullptr));
  mock_quic_data_.AddReadPauseForever();
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(packet_number++,
                                            quic::QUIC_STREAM_CANCELLED, 1, 1));

  InitializeSession();
  InitializeClientSocket();

  ASSERT_FALSE(sock_->IsConnected());

  TestCompletionCallback callback;
  ASSERT_THAT(sock_->Connect(callback.callback()),
              test::IsError(ERR_IO_PENDING));

  // This should let Connect run until the ERR_IO_PENDING returned by
  // OnTunnelHeadersReceived.
  proxy_delegate_->WaitForOnTunnelHeadersReceivedAsyncCompletion();
  ASSERT_EQ(proxy_delegate_->on_before_tunnel_request_call_count(), 1u);
  ASSERT_EQ(proxy_delegate_->on_tunnel_headers_received_call_count(), 1u);
  ASSERT_FALSE(callback.have_result());

  // Once we let TestProxyDelegate continue, Connect should be able to terminate
  // in a success.
  proxy_delegate_->ResumeOnTunnelHeadersReceived();
  ASSERT_EQ(callback.WaitForResult(), OK);
  ASSERT_EQ(proxy_delegate_->on_before_tunnel_request_call_count(), 1u);
  ASSERT_EQ(proxy_delegate_->on_tunnel_headers_received_call_count(), 1u);
}

TEST_P(QuicProxyClientSocketTest,
       ProxyDelegateOnTunnelHeadersReceivedFailsAsync) {
  proxy_delegate_ = std::make_unique<TestProxyDelegate>();
  proxy_delegate_->MakeOnTunnelHeadersReceivedFail(
      ERR_TUNNEL_CONNECTION_FAILED);
  proxy_delegate_->MakeOnTunnelHeadersReceivedCompleteAsync();

  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(
      ASYNC, ConstructServerConnectReplyPacket(
                 /*packet_number=*/1, !kFin, /*header_length=*/nullptr));
  mock_quic_data_.AddReadPauseForever();
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(packet_number++,
                                            quic::QUIC_STREAM_CANCELLED, 1, 1));

  InitializeSession();
  InitializeClientSocket();

  ASSERT_FALSE(sock_->IsConnected());

  TestCompletionCallback callback;
  ASSERT_THAT(sock_->Connect(callback.callback()),
              test::IsError(ERR_IO_PENDING));

  // This should let Connect run until the ERR_IO_PENDING returned by
  // OnTunnelHeadersReceived.
  proxy_delegate_->WaitForOnTunnelHeadersReceivedAsyncCompletion();
  ASSERT_EQ(proxy_delegate_->on_before_tunnel_request_call_count(), 1u);
  ASSERT_EQ(proxy_delegate_->on_tunnel_headers_received_call_count(), 1u);
  ASSERT_FALSE(callback.have_result());

  // Once we let TestProxyDelegate continue, Connect should terminate in a
  // failure with the error passed to MakeOnTunnelHeadersReceivedFail.
  proxy_delegate_->ResumeOnTunnelHeadersReceived();
  ASSERT_EQ(ERR_TUNNEL_CONNECTION_FAILED, callback.WaitForResult());
  ASSERT_EQ(proxy_delegate_->on_before_tunnel_request_call_count(), 1u);
  ASSERT_EQ(proxy_delegate_->on_tunnel_headers_received_call_count(), 1u);
}

TEST_P(QuicProxyClientSocketTest, ConnectWithAuthRequested) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC,
                          ConstructServerConnectAuthReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPauseForever();
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(packet_number++,
                                            quic::QUIC_STREAM_CANCELLED, 1, 1));

  InitializeSession();
  InitializeClientSocket();

  AssertConnectFails(ERR_PROXY_AUTH_REQUESTED);

  const HttpResponseInfo* response = sock_->GetConnectResponseInfo();
  ASSERT_TRUE(response != nullptr);
  ASSERT_EQ(407, response->headers->response_code());
}

TEST_P(QuicProxyClientSocketTest, ConnectWithAuthCredentials) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectAuthRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPauseForever();
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(packet_number++,
                                            quic::QUIC_STREAM_CANCELLED, 1, 1));

  InitializeSession();
  InitializeClientSocket();

  // Add auth to cache
  const std::u16string kFoo(u"foo");
  const std::u16string kBar(u"bar");
  http_auth_cache_.Add(
      url::SchemeHostPort(GURL(kProxyUrl)), HttpAuth::AUTH_PROXY, "MyRealm1",
      HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
      "Basic realm=MyRealm1", AuthCredentials(kFoo, kBar), "/");

  AssertConnectSucceeds();

  const HttpResponseInfo* response = sock_->GetConnectResponseInfo();
  ASSERT_TRUE(response != nullptr);
  ASSERT_EQ(200, response->headers->response_code());
}

// Tests that a redirect response from a CONNECT fails.
TEST_P(QuicProxyClientSocketTest, ConnectRedirects) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC,
                          ConstructServerConnectRedirectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPauseForever();
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(packet_number++,
                                            quic::QUIC_STREAM_CANCELLED, 1, 1));

  InitializeSession();
  InitializeClientSocket();

  AssertConnectFails(ERR_TUNNEL_CONNECTION_FAILED);

  const HttpResponseInfo* response = sock_->GetConnectResponseInfo();
  ASSERT_TRUE(response != nullptr);

  const HttpResponseHeaders* headers = response->headers.get();
  ASSERT_EQ(302, headers->response_code());
  ASSERT_TRUE(headers->HasHeader("set-cookie"));

  std::string location;
  ASSERT_TRUE(headers->IsRedirect(&location));
  ASSERT_EQ(location, kRedirectUrl);
}

TEST_P(QuicProxyClientSocketTest, ConnectFails) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  InitializeSession();
  InitializeClientSocket();

  ASSERT_FALSE(sock_->IsConnected());

  AssertConnectFails(ERR_QUIC_PROTOCOL_ERROR);

  ASSERT_FALSE(sock_->IsConnected());
}

TEST_P(QuicProxyClientSocketTest, WasEverUsedReturnsCorrectValue) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPauseForever();
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(packet_number++,
                                            quic::QUIC_STREAM_CANCELLED, 1, 1));

  InitializeSession();
  InitializeClientSocket();

  EXPECT_TRUE(sock_->WasEverUsed());  // Used due to crypto handshake
  AssertConnectSucceeds();
  EXPECT_TRUE(sock_->WasEverUsed());
  sock_->Disconnect();
  EXPECT_TRUE(sock_->WasEverUsed());
}

TEST_P(QuicProxyClientSocketTest, GetPeerAddressReturnsCorrectValues) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPause();
  mock_quic_data_.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  InitializeSession();
  InitializeClientSocket();

  IPEndPoint addr;
  EXPECT_THAT(sock_->GetPeerAddress(&addr), IsError(ERR_SOCKET_NOT_CONNECTED));

  AssertConnectSucceeds();
  EXPECT_TRUE(sock_->IsConnected());
  EXPECT_THAT(sock_->GetPeerAddress(&addr), IsOk());

  ResumeAndRun();

  EXPECT_FALSE(sock_->IsConnected());
  EXPECT_THAT(sock_->GetPeerAddress(&addr), IsError(ERR_SOCKET_NOT_CONNECTED));

  sock_->Disconnect();

  EXPECT_THAT(sock_->GetPeerAddress(&addr), IsError(ERR_SOCKET_NOT_CONNECTED));
}

TEST_P(QuicProxyClientSocketTest, IsConnectedAndIdle) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPause();

  std::string header = ConstructDataHeader(kMsg1.size());
  mock_quic_data_.AddRead(
      ASYNC,
      ConstructServerDataPacket(2, ConcatenateHeaderAndBody(header, kMsg1)));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructAckPacket(packet_number++, 2, 1));
  mock_quic_data_.AddReadPauseForever();
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructRstPacket(packet_number++, quic::QUIC_STREAM_CANCELLED));

  InitializeSession();
  InitializeClientSocket();

  EXPECT_FALSE(sock_->IsConnectedAndIdle());

  AssertConnectSucceeds();

  EXPECT_TRUE(sock_->IsConnectedAndIdle());

  // The next read is consumed and buffered.
  ResumeAndRun();

  EXPECT_FALSE(sock_->IsConnectedAndIdle());

  AssertSyncReadEquals(kMsg1);

  EXPECT_TRUE(sock_->IsConnectedAndIdle());
}

TEST_P(QuicProxyClientSocketTest, GetTotalReceivedBytes) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  size_t header_length;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(
      ASYNC, ConstructServerConnectReplyPacket(1, !kFin, &header_length));
  mock_quic_data_.AddReadPause();

  std::string data_header = ConstructDataHeader(kMsg333.size());
  mock_quic_data_.AddRead(
      ASYNC, ConstructServerDataPacket(
                 2, ConcatenateHeaderAndBody(data_header, kMsg333)));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructAckPacket(packet_number++, 2, 1));
  mock_quic_data_.AddReadPauseForever();
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructRstPacket(packet_number++, quic::QUIC_STREAM_CANCELLED));

  InitializeSession();
  InitializeClientSocket();

  EXPECT_EQ(0, sock_->GetTotalReceivedBytes());

  AssertConnectSucceeds();

  EXPECT_EQ(base::checked_cast<int64_t>(header_length),
            sock_->GetTotalReceivedBytes());

  // The next read is consumed and buffered.
  ResumeAndRun();

  EXPECT_EQ(base::checked_cast<int64_t>(header_length + data_header.length()),
            sock_->GetTotalReceivedBytes());

  // The payload from the single large data frame will be read across
  // two different reads.
  AssertSyncReadEquals(kMsg33);

  EXPECT_EQ(base::checked_cast<int64_t>(header_length + data_header.length() +
                                        kMsg33.size()),
            sock_->GetTotalReceivedBytes());

  AssertSyncReadEquals(kMsg3);

  EXPECT_EQ(base::checked_cast<int64_t>(header_length + kMsg333.size() +
                                        data_header.length()),
            sock_->GetTotalReceivedBytes());
}

TEST_P(QuicProxyClientSocketTest, SetStreamPriority) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  // Despite setting the priority to HIGHEST, the requests initial priority of
  // LOWEST is used.
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructConnectRequestPacket(packet_number++,
                                    /*extra_headers=*/std::nullopt, LOWEST));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPauseForever();
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(packet_number++,
                                            quic::QUIC_STREAM_CANCELLED, 1, 1));

  InitializeSession();
  InitializeClientSocket();

  sock_->SetStreamPriority(HIGHEST);
  AssertConnectSucceeds();
}

TEST_P(QuicProxyClientSocketTest, WriteSendsDataInDataFrame) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPauseForever();
  std::string header = ConstructDataHeader(kMsg1.size());
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructAckAndDataPacket(packet_number++, 1, 1,
                                {ConcatenateHeaderAndBody(header, kMsg1)}));
  std::string header2 = ConstructDataHeader(kMsg2.size());
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructDataPacket(packet_number++,
                          {ConcatenateHeaderAndBody(header2, kMsg2)}));
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructRstPacket(packet_number++, quic::QUIC_STREAM_CANCELLED));

  InitializeSession();
  InitializeClientSocket();

  AssertConnectSucceeds();

  AssertSyncWriteSucceeds(kMsg1);
  AssertSyncWriteSucceeds(kMsg2);
}

TEST_P(QuicProxyClientSocketTest, WriteSplitsLargeDataIntoMultiplePackets) {
  int write_packet_index = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(write_packet_index++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(write_packet_index++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPauseForever();
  std::string header = ConstructDataHeader(kMsg1.size());
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructAckAndDataPacket(write_packet_index++, 1, 1,
                                {ConcatenateHeaderAndBody(header, kMsg1)}));

  // Expect |kNumDataPackets| data packets, each containing the max possible
  // amount of data.
  int numDataPackets = 3;
  std::string data(numDataPackets * quic::kDefaultMaxPacketSize, 'x');
  quic::QuicStreamOffset offset = kMsg1.size() + header.length();

  numDataPackets++;
  size_t total_data_length = 0;
  for (int i = 0; i < numDataPackets; ++i) {
    size_t max_packet_data_length = GetStreamFrameDataLengthFromPacketLength(
        quic::kDefaultMaxPacketSize, version_, !kIncludeVersion,
        !kIncludeDiversificationNonce, k8ByteConnectionId,
        quic::PACKET_1BYTE_PACKET_NUMBER, offset);
    if (i == 0) {
      // 3661 is the data frame length from packet length.
      std::string header2 = ConstructDataHeader(3661);
      mock_quic_data_.AddWrite(
          SYNCHRONOUS,
          ConstructDataPacket(
              write_packet_index++,
              {header2 + data.substr(0, max_packet_data_length - 7)}));
      offset += max_packet_data_length - header2.length() - 1;
    } else if (i == numDataPackets - 1) {
      mock_quic_data_.AddWrite(
          SYNCHRONOUS,
          ConstructDataPacket(write_packet_index++, data.substr(0, 7)));
      offset += 7;
    } else {
      mock_quic_data_.AddWrite(
          SYNCHRONOUS,
          ConstructDataPacket(write_packet_index++,
                              data.substr(0, max_packet_data_length)));
      offset += max_packet_data_length;
    }
    if (i != 3) {
      total_data_length += max_packet_data_length;
    }
  }

  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructRstPacket(write_packet_index++, quic::QUIC_STREAM_CANCELLED));

  InitializeSession();
  InitializeClientSocket();

  AssertConnectSucceeds();

  // Make a small write. An ACK and STOP_WAITING will be bundled. This prevents
  // ACK and STOP_WAITING from being bundled with the subsequent large write.
  // This allows the test code for computing the size of data sent in each
  // packet to not become too complicated.
  AssertSyncWriteSucceeds(kMsg1);

  // Make large write that should be split up
  AssertSyncWriteSucceeds(std::string_view(data).substr(0, total_data_length));
}

// ----------- Read

TEST_P(QuicProxyClientSocketTest, ReadReadsDataInDataFrame) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPause();

  std::string header = ConstructDataHeader(kMsg1.size());
  mock_quic_data_.AddRead(
      ASYNC,
      ConstructServerDataPacket(2, ConcatenateHeaderAndBody(header, kMsg1)));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructAckPacket(packet_number++, 2, 1));
  mock_quic_data_.AddReadPauseForever();
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructRstPacket(packet_number++, quic::QUIC_STREAM_CANCELLED));

  InitializeSession();
  InitializeClientSocket();

  AssertConnectSucceeds();

  ResumeAndRun();
  AssertSyncReadEquals(kMsg1);
}

TEST_P(QuicProxyClientSocketTest, ReadDataFromBufferedFrames) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPause();

  std::string header = ConstructDataHeader(kMsg1.size());
  mock_quic_data_.AddRead(
      ASYNC,
      ConstructServerDataPacket(2, ConcatenateHeaderAndBody(header, kMsg1)));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructAckPacket(packet_number++, 2, 1));
  mock_quic_data_.AddReadPause();

  std::string header2 = ConstructDataHeader(kMsg2.size());
  mock_quic_data_.AddRead(
      ASYNC,
      ConstructServerDataPacket(3, ConcatenateHeaderAndBody(header2, kMsg2)));
  mock_quic_data_.AddReadPauseForever();

  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(packet_number++,
                                            quic::QUIC_STREAM_CANCELLED, 3, 3));

  InitializeSession();
  InitializeClientSocket();

  AssertConnectSucceeds();

  ResumeAndRun();
  AssertSyncReadEquals(kMsg1);

  ResumeAndRun();
  AssertSyncReadEquals(kMsg2);
}

TEST_P(QuicProxyClientSocketTest, ReadDataMultipleBufferedFrames) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPause();

  std::string header = ConstructDataHeader(kMsg1.size());
  mock_quic_data_.AddRead(
      ASYNC,
      ConstructServerDataPacket(2, ConcatenateHeaderAndBody(header, kMsg1)));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructAckPacket(packet_number++, 2, 1));
  std::string header2 = ConstructDataHeader(kMsg2.size());
  mock_quic_data_.AddRead(
      ASYNC,
      ConstructServerDataPacket(3, ConcatenateHeaderAndBody(header2, kMsg2)));
  mock_quic_data_.AddReadPauseForever();

  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(packet_number++,
                                            quic::QUIC_STREAM_CANCELLED, 3, 3));

  InitializeSession();
  InitializeClientSocket();

  AssertConnectSucceeds();

  // The next two reads are consumed and buffered.
  ResumeAndRun();

  AssertSyncReadEquals(kMsg1);
  AssertSyncReadEquals(kMsg2);
}

TEST_P(QuicProxyClientSocketTest, LargeReadWillMergeDataFromDifferentFrames) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPause();

  std::string header = ConstructDataHeader(kMsg3.size());
  mock_quic_data_.AddRead(
      ASYNC,
      ConstructServerDataPacket(2, ConcatenateHeaderAndBody(header, kMsg3)));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructAckPacket(packet_number++, 2, 1));
  std::string header2 = ConstructDataHeader(kMsg3.size());
  mock_quic_data_.AddRead(
      ASYNC,
      ConstructServerDataPacket(3, ConcatenateHeaderAndBody(header2, kMsg3)));
  mock_quic_data_.AddReadPauseForever();

  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(packet_number++,
                                            quic::QUIC_STREAM_CANCELLED, 3, 3));

  InitializeSession();
  InitializeClientSocket();

  AssertConnectSucceeds();

  // The next two reads are consumed and buffered.
  ResumeAndRun();
  // The payload from two data frames, each with kMsg3 will be combined
  // together into a single read().
  AssertSyncReadEquals(kMsg33);
}

TEST_P(QuicProxyClientSocketTest, MultipleShortReadsThenMoreRead) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPause();

  std::string header = ConstructDataHeader(kMsg1.size());
  mock_quic_data_.AddRead(
      ASYNC,
      ConstructServerDataPacket(2, ConcatenateHeaderAndBody(header, kMsg1)));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructAckPacket(packet_number++, 2, 1));

  std::string header2 = ConstructDataHeader(kMsg3.size());
  mock_quic_data_.AddRead(
      ASYNC,
      ConstructServerDataPacket(3, ConcatenateHeaderAndBody(header2, kMsg3)));
  mock_quic_data_.AddRead(
      ASYNC,
      ConstructServerDataPacket(4, ConcatenateHeaderAndBody(header2, kMsg3)));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructAckPacket(packet_number++, 4, 3));

  std::string header3 = ConstructDataHeader(kMsg2.size());
  mock_quic_data_.AddRead(
      ASYNC,
      ConstructServerDataPacket(5, ConcatenateHeaderAndBody(header3, kMsg2)));
  mock_quic_data_.AddReadPauseForever();

  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(packet_number++,
                                            quic::QUIC_STREAM_CANCELLED, 5, 5));

  InitializeSession();
  InitializeClientSocket();

  AssertConnectSucceeds();

  // The next 4 reads are consumed and buffered.
  ResumeAndRun();

  AssertSyncReadEquals(kMsg1);
  // The payload from two data frames, each with kMsg3 will be combined
  // together into a single read().
  AssertSyncReadEquals(kMsg33);
  AssertSyncReadEquals(kMsg2);
}

TEST_P(QuicProxyClientSocketTest, ReadWillSplitDataFromLargeFrame) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPause();

  std::string header = ConstructDataHeader(kMsg1.size());
  mock_quic_data_.AddRead(
      ASYNC,
      ConstructServerDataPacket(2, ConcatenateHeaderAndBody(header, kMsg1)));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructAckPacket(packet_number++, 2, 1));
  std::string header2 = ConstructDataHeader(kMsg33.size());
  mock_quic_data_.AddRead(
      ASYNC,
      ConstructServerDataPacket(3, ConcatenateHeaderAndBody(header2, kMsg33)));
  mock_quic_data_.AddReadPauseForever();

  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(packet_number++,
                                            quic::QUIC_STREAM_CANCELLED, 3, 3));

  InitializeSession();
  InitializeClientSocket();

  AssertConnectSucceeds();

  // The next 2 reads are consumed and buffered.
  ResumeAndRun();

  AssertSyncReadEquals(kMsg1);
  // The payload from the single large data frame will be read across
  // two different reads.
  AssertSyncReadEquals(kMsg3);
  AssertSyncReadEquals(kMsg3);
}

TEST_P(QuicProxyClientSocketTest, MultipleReadsFromSameLargeFrame) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPause();

  std::string header = ConstructDataHeader(kMsg333.size());
  mock_quic_data_.AddRead(
      ASYNC,
      ConstructServerDataPacket(2, ConcatenateHeaderAndBody(header, kMsg333)));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructAckPacket(packet_number++, 2, 1));
  mock_quic_data_.AddReadPauseForever();

  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructRstPacket(packet_number++, quic::QUIC_STREAM_CANCELLED));

  InitializeSession();
  InitializeClientSocket();

  AssertConnectSucceeds();

  // The next read is consumed and buffered.
  ResumeAndRun();

  // The payload from the single large data frame will be read across
  // two different reads.
  AssertSyncReadEquals(kMsg33);

  // Now attempt to do a read of more data than remains buffered
  auto buf = base::MakeRefCounted<IOBufferWithSize>(kMsg33.size());
  ASSERT_EQ(kMsg3.size(),
            sock_->Read(buf.get(), kMsg33.size(), CompletionOnceCallback()));
  ASSERT_EQ(base::as_bytes(kMsg3), buf->first(kMsg3.size()));
  ASSERT_TRUE(sock_->IsConnected());
}

TEST_P(QuicProxyClientSocketTest, ReadAuthResponseBody) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC,
                          ConstructServerConnectAuthReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPause();

  std::string header = ConstructDataHeader(kMsg1.size());
  mock_quic_data_.AddRead(
      ASYNC,
      ConstructServerDataPacket(2, ConcatenateHeaderAndBody(header, kMsg1)));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructAckPacket(packet_number++, 2, 1));
  std::string header2 = ConstructDataHeader(kMsg2.size());
  mock_quic_data_.AddRead(
      ASYNC,
      ConstructServerDataPacket(3, ConcatenateHeaderAndBody(header2, kMsg2)));
  mock_quic_data_.AddReadPauseForever();

  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(packet_number++,
                                            quic::QUIC_STREAM_CANCELLED, 3, 3));

  InitializeSession();
  InitializeClientSocket();

  AssertConnectFails(ERR_PROXY_AUTH_REQUESTED);

  // The next two reads are consumed and buffered.
  ResumeAndRun();

  AssertSyncReadEquals(kMsg1);
  AssertSyncReadEquals(kMsg2);
}

TEST_P(QuicProxyClientSocketTest, ReadErrorResponseBody) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC,
                          ConstructServerConnectErrorReplyPacket(1, !kFin));
  std::string header = ConstructDataHeader(kMsg1.size());
  mock_quic_data_.AddRead(
      SYNCHRONOUS,
      ConstructServerDataPacket(2, ConcatenateHeaderAndBody(header, kMsg1)));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructAckPacket(packet_number++, 2, 1));
  std::string header2 = ConstructDataHeader(kMsg2.size());
  mock_quic_data_.AddRead(
      SYNCHRONOUS,
      ConstructServerDataPacket(3, ConcatenateHeaderAndBody(header2, kMsg2)));
  mock_quic_data_.AddReadPauseForever();

  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(packet_number++,
                                            quic::QUIC_STREAM_CANCELLED, 3, 3));
  InitializeSession();
  InitializeClientSocket();

  AssertConnectFails(ERR_TUNNEL_CONNECTION_FAILED);
}

// ----------- Reads and Writes

TEST_P(QuicProxyClientSocketTest, AsyncReadAroundWrite) {
  int write_packet_index = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(write_packet_index++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(write_packet_index++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPause();

  std::string header = ConstructDataHeader(kMsg1.size());
  mock_quic_data_.AddRead(
      ASYNC,
      ConstructServerDataPacket(2, ConcatenateHeaderAndBody(header, kMsg1)));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructAckPacket(write_packet_index++, 2, 1));

  std::string header2 = ConstructDataHeader(kMsg2.size());
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructDataPacket(write_packet_index++,
                          {ConcatenateHeaderAndBody(header2, kMsg2)}));

  mock_quic_data_.AddReadPause();

  std::string header3 = ConstructDataHeader(kMsg3.size());
  mock_quic_data_.AddRead(
      ASYNC,
      ConstructServerDataPacket(3, ConcatenateHeaderAndBody(header3, kMsg3)));
  mock_quic_data_.AddReadPauseForever();

  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(write_packet_index++,
                                            quic::QUIC_STREAM_CANCELLED, 3, 3));

  InitializeSession();
  InitializeClientSocket();

  AssertConnectSucceeds();

  ResumeAndRun();

  AssertSyncReadEquals(kMsg1);

  AssertReadStarts(kMsg3);
  // Read should block until after the write succeeds.

  AssertSyncWriteSucceeds(kMsg2);

  ASSERT_FALSE(read_callback_.have_result());
  ResumeAndRun();

  // Now the read will return.
  AssertReadReturns(kMsg3);
}

TEST_P(QuicProxyClientSocketTest, AsyncWriteAroundReads) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPause();

  std::string header = ConstructDataHeader(kMsg1.size());
  mock_quic_data_.AddRead(
      ASYNC,
      ConstructServerDataPacket(2, ConcatenateHeaderAndBody(header, kMsg1)));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructAckPacket(packet_number++, 2, 1));
  mock_quic_data_.AddReadPause();

  std::string header2 = ConstructDataHeader(kMsg3.size());
  mock_quic_data_.AddRead(
      ASYNC,
      ConstructServerDataPacket(3, ConcatenateHeaderAndBody(header2, kMsg3)));
  mock_quic_data_.AddReadPauseForever();

  mock_quic_data_.AddWritePause();

  std::string header3 = ConstructDataHeader(kMsg2.size());
  mock_quic_data_.AddWrite(
      ASYNC, ConstructDataPacket(packet_number++,
                                 {ConcatenateHeaderAndBody(header3, kMsg2)}));
  mock_quic_data_.AddWrite(
      ASYNC,
      ConstructAckAndDataPacket(packet_number++, 3, 3,
                                ConcatenateHeaderAndBody(header3, kMsg2)));

  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructRstPacket(packet_number++, quic::QUIC_STREAM_CANCELLED));

  InitializeSession();
  InitializeClientSocket();

  AssertConnectSucceeds();

  ResumeAndRun();
  AssertSyncReadEquals(kMsg1);

  // Write should block until the next read completes.
  // QuicChromiumClientStream::Handle::WriteStreamData() will only be
  // asynchronous starting with the second time it's called while the UDP socket
  // is write-blocked. Therefore, at least two writes need to be called on
  // |sock_| to get an asynchronous one.
  AssertWriteReturns(kMsg2, kMsg2.size());
  AssertWriteReturns(kMsg2, ERR_IO_PENDING);

  AssertAsyncReadEquals(kMsg3);

  ASSERT_FALSE(write_callback_.have_result());

  // Now the write will complete
  ResumeAndRun();
  EXPECT_EQ(kMsg2.size(), write_callback_.WaitForResult());
}

// ----------- Reading/Writing on Closed socket

// Reading from an already closed socket should return 0
TEST_P(QuicProxyClientSocketTest, ReadOnClosedSocketReturnsZero) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPause();
  mock_quic_data_.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  InitializeSession();
  InitializeClientSocket();

  AssertConnectSucceeds();

  ResumeAndRun();

  ASSERT_FALSE(sock_->IsConnected());
  ASSERT_EQ(0, sock_->Read(nullptr, 1, CompletionOnceCallback()));
  ASSERT_EQ(0, sock_->Read(nullptr, 1, CompletionOnceCallback()));
  ASSERT_EQ(0, sock_->Read(nullptr, 1, CompletionOnceCallback()));
  ASSERT_FALSE(sock_->IsConnectedAndIdle());
}

// Read pending when socket is closed should return 0
TEST_P(QuicProxyClientSocketTest, PendingReadOnCloseReturnsZero) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPause();
  mock_quic_data_.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  InitializeSession();
  InitializeClientSocket();

  AssertConnectSucceeds();

  AssertReadStarts(kMsg1);

  ResumeAndRun();

  ASSERT_EQ(0, read_callback_.WaitForResult());
}

// Reading from a disconnected socket is an error
TEST_P(QuicProxyClientSocketTest, ReadOnDisconnectSocketReturnsNotConnected) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPauseForever();
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(packet_number++,
                                            quic::QUIC_STREAM_CANCELLED, 1, 1));

  InitializeSession();
  InitializeClientSocket();

  AssertConnectSucceeds();

  sock_->Disconnect();

  ASSERT_EQ(ERR_SOCKET_NOT_CONNECTED,
            sock_->Read(nullptr, 1, CompletionOnceCallback()));
}

// Reading data after receiving FIN should return buffered data received before
// FIN, then 0.
TEST_P(QuicProxyClientSocketTest, ReadAfterFinReceivedReturnsBufferedData) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPause();

  std::string header = ConstructDataHeader(kMsg1.size());
  mock_quic_data_.AddRead(
      ASYNC,
      ConstructServerDataFinPacket(2, ConcatenateHeaderAndBody(header, kMsg1)));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructAckPacket(packet_number++, 2, 1));
  mock_quic_data_.AddReadPauseForever();
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructRstPacket(packet_number++, quic::QUIC_STREAM_CANCELLED));

  InitializeSession();
  InitializeClientSocket();

  AssertConnectSucceeds();

  ResumeAndRun();

  AssertSyncReadEquals(kMsg1);
  ASSERT_EQ(0, sock_->Read(nullptr, 1, CompletionOnceCallback()));
  ASSERT_EQ(0, sock_->Read(nullptr, 1, CompletionOnceCallback()));

  sock_->Disconnect();
  ASSERT_EQ(ERR_SOCKET_NOT_CONNECTED,
            sock_->Read(nullptr, 1, CompletionOnceCallback()));
}

// Calling Write() on a closed socket is an error.
TEST_P(QuicProxyClientSocketTest, WriteOnClosedStream) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPause();
  mock_quic_data_.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  InitializeSession();
  InitializeClientSocket();

  AssertConnectSucceeds();

  ResumeAndRun();

  AssertWriteReturns(kMsg1, ERR_QUIC_PROTOCOL_ERROR);
}

// Calling Write() on a disconnected socket is an error.
TEST_P(QuicProxyClientSocketTest, WriteOnDisconnectedSocket) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPauseForever();
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(packet_number++,
                                            quic::QUIC_STREAM_CANCELLED, 1, 1));

  InitializeSession();
  InitializeClientSocket();

  AssertConnectSucceeds();

  sock_->Disconnect();

  AssertWriteReturns(kMsg1, ERR_SOCKET_NOT_CONNECTED);
}

// If the socket is closed with a pending Write(), the callback should be called
// with the same error the session was closed with.
TEST_P(QuicProxyClientSocketTest, WritePendingOnClose) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPauseForever();
  mock_quic_data_.AddWrite(SYNCHRONOUS, ERR_IO_PENDING);

  InitializeSession();
  InitializeClientSocket();

  AssertConnectSucceeds();

  // QuicChromiumClientStream::Handle::WriteStreamData() will only be
  // asynchronous starting with the second time it's called while the UDP socket
  // is write-blocked. Therefore, at least two writes need to be called on
  // |sock_| to get an asynchronous one.
  AssertWriteReturns(kMsg1, kMsg1.size());

  // This second write will be async. This is the pending write that's being
  // tested.
  AssertWriteReturns(kMsg1, ERR_IO_PENDING);

  // Make sure the write actually starts.
  base::RunLoop().RunUntilIdle();

  session_->CloseSessionOnError(ERR_CONNECTION_CLOSED,
                                quic::QUIC_INTERNAL_ERROR,
                                quic::ConnectionCloseBehavior::SILENT_CLOSE);

  EXPECT_THAT(write_callback_.WaitForResult(), IsError(ERR_CONNECTION_CLOSED));
}

TEST_P(QuicProxyClientSocketTest, DisconnectWithWritePending) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPauseForever();
  mock_quic_data_.AddWrite(SYNCHRONOUS, ERR_IO_PENDING);

  InitializeSession();
  InitializeClientSocket();

  AssertConnectSucceeds();

  // QuicChromiumClientStream::Handle::WriteStreamData() will only be
  // asynchronous starting with the second time it's called while the UDP socket
  // is write-blocked. Therefore, at least two writes need to be called on
  // |sock_| to get an asynchronous one.
  AssertWriteReturns(kMsg1, kMsg1.size());

  // This second write will be async. This is the pending write that's being
  // tested.
  AssertWriteReturns(kMsg1, ERR_IO_PENDING);

  // Make sure the write actually starts.
  base::RunLoop().RunUntilIdle();

  sock_->Disconnect();
  EXPECT_FALSE(sock_->IsConnected());

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(sock_->IsConnected());
  EXPECT_FALSE(write_callback_.have_result());
}

// If the socket is Disconnected with a pending Read(), the callback
// should not be called.
TEST_P(QuicProxyClientSocketTest, DisconnectWithReadPending) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPauseForever();
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(packet_number++,
                                            quic::QUIC_STREAM_CANCELLED, 1, 1));

  InitializeSession();
  InitializeClientSocket();

  AssertConnectSucceeds();

  EXPECT_TRUE(sock_->IsConnected());

  AssertReadStarts(kMsg1);

  sock_->Disconnect();
  EXPECT_FALSE(sock_->IsConnected());

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(sock_->IsConnected());
  EXPECT_FALSE(read_callback_.have_result());
}

// If the socket is Reset when both a read and write are pending,
// both should be called back.
TEST_P(QuicProxyClientSocketTest, RstWithReadAndWritePending) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPause();

  mock_quic_data_.AddRead(
      ASYNC, ConstructServerRstPacket(2, quic::QUIC_STREAM_CANCELLED));
  mock_quic_data_.AddReadPauseForever();
  std::string header = ConstructDataHeader(kMsg2.size());
  mock_quic_data_.AddWrite(
      ASYNC,
      ConstructAckAndDataPacket(packet_number++, 1, 1,
                                {ConcatenateHeaderAndBody(header, kMsg2)}));
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstOnlyPacket(
                       packet_number++, quic::QUIC_STREAM_CANCELLED, 2, 2));

  InitializeSession();
  InitializeClientSocket();

  AssertConnectSucceeds();

  EXPECT_TRUE(sock_->IsConnected());

  AssertReadStarts(kMsg1);

  // Write should block until the next read completes.
  // QuicChromiumClientStream::Handle::WriteStreamData() will only be
  // asynchronous starting with the second time it's called while the UDP socket
  // is write-blocked. Therefore, at least two writes need to be called on
  // |sock_| to get an asynchronous one.
  AssertWriteReturns(kMsg2, kMsg2.size());

  AssertWriteReturns(kMsg2, ERR_IO_PENDING);

  ResumeAndRun();

  EXPECT_TRUE(read_callback_.have_result());
  EXPECT_TRUE(write_callback_.have_result());
}

// Makes sure the proxy client socket's source gets the expected NetLog events
// and only the expected NetLog events (No SpdySession events).
TEST_P(QuicProxyClientSocketTest, NetLog) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPause();

  std::string header = ConstructDataHeader(kMsg1.size());
  mock_quic_data_.AddRead(
      ASYNC,
      ConstructServerDataPacket(2, ConcatenateHeaderAndBody(header, kMsg1)));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructAckPacket(packet_number++, 2, 1));
  mock_quic_data_.AddReadPauseForever();
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructRstPacket(packet_number++, quic::QUIC_STREAM_CANCELLED));

  InitializeSession();
  InitializeClientSocket();

  AssertConnectSucceeds();

  ResumeAndRun();
  AssertSyncReadEquals(kMsg1);

  NetLogSource sock_source = sock_->NetLog().source();
  sock_.reset();

  auto entry_list = net_log_observer_.GetEntriesForSource(sock_source);

  ASSERT_EQ(entry_list.size(), 10u);
  EXPECT_TRUE(
      LogContainsBeginEvent(entry_list, 0, NetLogEventType::SOCKET_ALIVE));
  EXPECT_TRUE(LogContainsEvent(entry_list, 1,
                               NetLogEventType::HTTP2_PROXY_CLIENT_SESSION,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(LogContainsBeginEvent(
      entry_list, 2, NetLogEventType::HTTP_TRANSACTION_TUNNEL_SEND_REQUEST));
  EXPECT_TRUE(LogContainsEvent(
      entry_list, 3, NetLogEventType::HTTP_TRANSACTION_SEND_TUNNEL_HEADERS,
      NetLogEventPhase::NONE));
  EXPECT_TRUE(LogContainsEndEvent(
      entry_list, 4, NetLogEventType::HTTP_TRANSACTION_TUNNEL_SEND_REQUEST));
  EXPECT_TRUE(LogContainsBeginEvent(
      entry_list, 5, NetLogEventType::HTTP_TRANSACTION_TUNNEL_READ_HEADERS));
  EXPECT_TRUE(LogContainsEvent(
      entry_list, 6,
      NetLogEventType::HTTP_TRANSACTION_READ_TUNNEL_RESPONSE_HEADERS,
      NetLogEventPhase::NONE));
  EXPECT_TRUE(LogContainsEndEvent(
      entry_list, 7, NetLogEventType::HTTP_TRANSACTION_TUNNEL_READ_HEADERS));
  EXPECT_TRUE(LogContainsEvent(entry_list, 8,
                               NetLogEventType::SOCKET_BYTES_RECEIVED,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(
      LogContainsEndEvent(entry_list, 9, NetLogEventType::SOCKET_ALIVE));
}

// A helper class that will delete |sock| when the callback is invoked.
class DeleteSockCallback : public TestCompletionCallbackBase {
 public:
  explicit DeleteSockCallback(std::unique_ptr<QuicProxyClientSocket>* sock)
      : sock_(sock) {}

  DeleteSockCallback(const DeleteSockCallback&) = delete;
  DeleteSockCallback& operator=(const DeleteSockCallback&) = delete;

  ~DeleteSockCallback() override = default;

  CompletionOnceCallback callback() {
    return base::BindOnce(&DeleteSockCallback::OnComplete,
                          base::Unretained(this));
  }

 private:
  void OnComplete(int result) {
    sock_->reset(nullptr);
    SetResult(result);
  }

  raw_ptr<std::unique_ptr<QuicProxyClientSocket>> sock_;
};

// If the socket is reset when both a read and write are pending, and the
// read callback causes the socket to be deleted, the write callback should
// not be called.
TEST_P(QuicProxyClientSocketTest, RstWithReadAndWritePendingDelete) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddReadPause();

  mock_quic_data_.AddRead(
      ASYNC, ConstructServerRstPacket(2, quic::QUIC_STREAM_CANCELLED));
  mock_quic_data_.AddReadPauseForever();
  std::string header = ConstructDataHeader(kMsg1.size());
  mock_quic_data_.AddWrite(
      ASYNC,
      ConstructAckAndDataPacket(packet_number++, 1, 1,
                                {ConcatenateHeaderAndBody(header, kMsg1)}));
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstOnlyPacket(
                       packet_number++, quic::QUIC_STREAM_CANCELLED, 2, 2));

  InitializeSession();
  InitializeClientSocket();

  AssertConnectSucceeds();

  EXPECT_TRUE(sock_->IsConnected());

  DeleteSockCallback read_callback(&sock_);
  auto read_buf = base::MakeRefCounted<IOBufferWithSize>(kMsg1.size());
  ASSERT_EQ(ERR_IO_PENDING, sock_->Read(read_buf.get(), kMsg1.size(),
                                        read_callback.callback()));

  // QuicChromiumClientStream::Handle::WriteStreamData() will only be
  // asynchronous starting with the second time it's called while the UDP socket
  // is write-blocked. Therefore, at least two writes need to be called on
  // |sock_| to get an asynchronous one.
  AssertWriteReturns(kMsg1, kMsg1.size());

  AssertWriteReturns(kMsg1, ERR_IO_PENDING);

  ResumeAndRun();

  EXPECT_FALSE(sock_.get());

  EXPECT_EQ(0, read_callback.WaitForResult());
  EXPECT_FALSE(write_callback_.have_result());
}

INSTANTIATE_TEST_SUITE_P(VersionIncludeStreamDependencySequence,
                         QuicProxyClientSocketTest,
                         ::testing::ValuesIn(AllSupportedQuicVersions()),
                         ::testing::PrintToStringParamName());

}  // namespace net::test
