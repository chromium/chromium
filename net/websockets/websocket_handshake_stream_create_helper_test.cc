// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_handshake_stream_create_helper.h"

#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "net/base/completion_once_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_server.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/connect_job.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket.h"
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
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/websockets/websocket_basic_handshake_stream.h"
#include "net/websockets/websocket_stream.h"
#include "net/websockets/websocket_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

using ::net::test::IsError;
using ::net::test::IsOk;
using ::testing::StrictMock;
using ::testing::TestWithParam;
using ::testing::Values;
using ::testing::_;

namespace net {
namespace {

enum HandshakeStreamType { BASIC_HANDSHAKE_STREAM, HTTP2_HANDSHAKE_STREAM };

// This class encapsulates the details of creating a mock ClientSocketHandle.
class MockClientSocketHandleFactory {
 public:
  MockClientSocketHandleFactory()
      : common_connect_job_params_(
            socket_factory_maker_.factory(),
            nullptr /* host_resolver */,
            nullptr /* http_auth_cache */,
            nullptr /* http_auth_handler_factory */,
            nullptr /* spdy_session_pool */,
            nullptr /* quic_supported_versions */,
            nullptr /* quic_stream_factory */,
            nullptr /* proxy_delegate */,
            nullptr /* http_user_agent_settings */,
            nullptr /* ssl_client_context */,
            nullptr /* socket_performance_watcher_factory */,
            nullptr /* network_quality_estimator */,
            nullptr /* net_log */,
            nullptr /* websocket_endpoint_lock_manager */),
        pool_(1, 1, &common_connect_job_params_) {}

  MockClientSocketHandleFactory(const MockClientSocketHandleFactory&) = delete;
  MockClientSocketHandleFactory& operator=(
      const MockClientSocketHandleFactory&) = delete;

  // The created socket expects |expect_written| to be written to the socket,
  // and will respond with |return_to_read|. The test will fail if the expected
  // text is not written, or if all the bytes are not read.
  std::unique_ptr<ClientSocketHandle> CreateClientSocketHandle(
      const std::string& expect_written,
      const std::string& return_to_read) {
    socket_factory_maker_.SetExpectations(expect_written, return_to_read);
    auto socket_handle = std::make_unique<ClientSocketHandle>();
    socket_handle->Init(
        ClientSocketPool::GroupId(
            url::SchemeHostPort(url::kHttpScheme, "a", 80),
            PrivacyMode::PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
            SecureDnsPolicy::kAllow),
        scoped_refptr<ClientSocketPool::SocketParams>(),
        absl::nullopt /* proxy_annotation_tag */, MEDIUM, SocketTag(),
        ClientSocketPool::RespectLimits::ENABLED, CompletionOnceCallback(),
        ClientSocketPool::ProxyAuthCallback(), &pool_, NetLogWithSource());
    return socket_handle;
  }

 private:
  WebSocketMockClientSocketFactoryMaker socket_factory_maker_;
  const CommonConnectJobParams common_connect_job_params_;
  MockTransportClientSocketPool pool_;
};

class TestConnectDelegate : public WebSocketStream::ConnectDelegate {
 public:
  ~TestConnectDelegate() override = default;

  void OnCreateRequest(URLRequest* request) override {}
  void OnSuccess(
      std::unique_ptr<WebSocketStream> stream,
      std::unique_ptr<WebSocketHandshakeResponseInfo> response) override {}
  void OnFailure(const std::string& failure_message,
                 int net_error,
                 absl::optional<int> response_code) override {}
  void OnStartOpeningHandshake(
      std::unique_ptr<WebSocketHandshakeRequestInfo> request) override {}
  void OnSSLCertificateError(
      std::unique_ptr<WebSocketEventInterface::SSLErrorCallbacks>
          ssl_error_callbacks,
      int net_error,
      const SSLInfo& ssl_info,
      bool fatal) override {}
  int OnAuthRequired(const AuthChallengeInfo& auth_info,
                     scoped_refptr<HttpResponseHeaders> response_headers,
                     const IPEndPoint& host_port_pair,
                     base::OnceCallback<void(const AuthCredentials*)> callback,
                     absl::optional<AuthCredentials>* credentials) override {
    *credentials = absl::nullopt;
    return OK;
  }
};

class MockWebSocketStreamRequestAPI : public WebSocketStreamRequestAPI {
 public:
  ~MockWebSocketStreamRequestAPI() override = default;

  MOCK_METHOD1(OnBasicHandshakeStreamCreated,
               void(WebSocketBasicHandshakeStream* handshake_stream));
  MOCK_METHOD1(OnHttp2HandshakeStreamCreated,
               void(WebSocketHttp2HandshakeStream* handshake_stream));
  MOCK_METHOD3(OnFailure,
               void(const std::string& message,
                    int net_error,
                    absl::optional<int> response_code));
};

class WebSocketHandshakeStreamCreateHelperTest
    : public TestWithParam<HandshakeStreamType>,
      public WithTaskEnvironment {
 protected:
  std::unique_ptr<WebSocketStream> CreateAndInitializeStream(
      const std::vector<std::string>& sub_protocols,
      const WebSocketExtraHeaders& extra_request_headers,
      const WebSocketExtraHeaders& extra_response_headers) {
    const char kPath[] = "/";
    const char kOrigin[] = "http://origin.example.org";
    const GURL url("wss://www.example.org/");
    NetLogWithSource net_log;

    WebSocketHandshakeStreamCreateHelper create_helper(
        &connect_delegate_, sub_protocols, &stream_request_);

    switch (GetParam()) {
      case BASIC_HANDSHAKE_STREAM:
        EXPECT_CALL(stream_request_, OnBasicHandshakeStreamCreated(_)).Times(1);
        break;

      case HTTP2_HANDSHAKE_STREAM:
        EXPECT_CALL(stream_request_, OnHttp2HandshakeStreamCreated(_)).Times(1);
        break;

      default:
        NOTREACHED();
    }

    EXPECT_CALL(stream_request_, OnFailure(_, _, _)).Times(0);

    HttpRequestInfo request_info;
    request_info.url = url;
    request_info.method = "GET";
    request_info.load_flags = LOAD_DISABLE_CACHE;
    request_info.traffic_annotation =
        MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

    auto headers = WebSocketCommonTestHeaders();

    switch (GetParam()) {
      case BASIC_HANDSHAKE_STREAM: {
        std::unique_ptr<ClientSocketHandle> socket_handle =
            socket_handle_factory_.CreateClientSocketHandle(
                WebSocketStandardRequest(kPath, "www.example.org",
                                         url::Origin::Create(GURL(kOrigin)),
                                         /*send_additional_request_headers=*/{},
                                         extra_request_headers),
                WebSocketStandardResponse(
                    WebSocketExtraHeadersToString(extra_response_headers)));

        std::unique_ptr<WebSocketHandshakeStreamBase> handshake =
            create_helper.CreateBasicStream(std::move(socket_handle), false,
                                            &websocket_endpoint_lock_manager_);

        // If in future the implementation type returned by CreateBasicStream()
        // changes, this static_cast will be wrong. However, in that case the
        // test will fail and AddressSanitizer should identify the issue.
        static_cast<WebSocketBasicHandshakeStream*>(handshake.get())
            ->SetWebSocketKeyForTesting("dGhlIHNhbXBsZSBub25jZQ==");

        handshake->RegisterRequest(&request_info);
        int rv = handshake->InitializeStream(true, DEFAULT_PRIORITY, net_log,
                                             CompletionOnceCallback());
        EXPECT_THAT(rv, IsOk());

        HttpResponseInfo response;
        TestCompletionCallback request_callback;
        rv = handshake->SendRequest(headers, &response,
                                    request_callback.callback());
        EXPECT_THAT(rv, IsOk());

        TestCompletionCallback response_callback;
        rv = handshake->ReadResponseHeaders(response_callback.callback());
        EXPECT_THAT(rv, IsOk());
        EXPECT_EQ(101, response.headers->response_code());
        EXPECT_TRUE(response.headers->HasHeaderValue("Connection", "Upgrade"));
        EXPECT_TRUE(response.headers->HasHeaderValue("Upgrade", "websocket"));
        return handshake->Upgrade();
      }
      case HTTP2_HANDSHAKE_STREAM: {
        SpdyTestUtil spdy_util;
        spdy::Http2HeaderBlock request_header_block = WebSocketHttp2Request(
            kPath, "www.example.org", kOrigin, extra_request_headers);
        spdy::SpdySerializedFrame request_headers(
            spdy_util.ConstructSpdyHeaders(1, std::move(request_header_block),
                                           DEFAULT_PRIORITY, false));
        MockWrite writes[] = {CreateMockWrite(request_headers, 0)};

        spdy::Http2HeaderBlock response_header_block =
            WebSocketHttp2Response(extra_response_headers);
        spdy::SpdySerializedFrame response_headers(
            spdy_util.ConstructSpdyResponseHeaders(
                1, std::move(response_header_block), false));
        MockRead reads[] = {CreateMockRead(response_headers, 1),
                            MockRead(ASYNC, 0, 2)};

        SequencedSocketData data(reads, writes);

        SSLSocketDataProvider ssl(ASYNC, OK);
        ssl.ssl_info.cert =
            ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem");

        SpdySessionDependencies session_deps;
        session_deps.socket_factory->AddSocketDataProvider(&data);
        session_deps.socket_factory->AddSSLSocketDataProvider(&ssl);

        std::unique_ptr<HttpNetworkSession> http_network_session =
            SpdySessionDependencies::SpdyCreateSession(&session_deps);
        const SpdySessionKey key(
            HostPortPair::FromURL(url), ProxyServer::Direct(),
            PRIVACY_MODE_DISABLED, SpdySessionKey::IsProxySession::kFalse,
            SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow);
        base::WeakPtr<SpdySession> spdy_session =
            CreateSpdySession(http_network_session.get(), key, net_log);
        std::unique_ptr<WebSocketHandshakeStreamBase> handshake =
            create_helper.CreateHttp2Stream(spdy_session, {} /* dns_aliases */);

        handshake->RegisterRequest(&request_info);
        int rv = handshake->InitializeStream(true, DEFAULT_PRIORITY,
                                             NetLogWithSource(),
                                             CompletionOnceCallback());
        EXPECT_THAT(rv, IsOk());

        HttpResponseInfo response;
        TestCompletionCallback request_callback;
        rv = handshake->SendRequest(headers, &response,
                                    request_callback.callback());
        EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
        rv = request_callback.WaitForResult();
        EXPECT_THAT(rv, IsOk());

        TestCompletionCallback response_callback;
        rv = handshake->ReadResponseHeaders(response_callback.callback());
        EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
        rv = response_callback.WaitForResult();
        EXPECT_THAT(rv, IsOk());

        EXPECT_EQ(200, response.headers->response_code());
        return handshake->Upgrade();
      }
      default:
        NOTREACHED();
        return nullptr;
    }
  }

 private:
  MockClientSocketHandleFactory socket_handle_factory_;
  TestConnectDelegate connect_delegate_;
  StrictMock<MockWebSocketStreamRequestAPI> stream_request_;
  WebSocketEndpointLockManager websocket_endpoint_lock_manager_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         WebSocketHandshakeStreamCreateHelperTest,
                         Values(BASIC_HANDSHAKE_STREAM,
                                HTTP2_HANDSHAKE_STREAM));

// Confirm that the basic case works as expected.
TEST_P(WebSocketHandshakeStreamCreateHelperTest, BasicStream) {
  std::unique_ptr<WebSocketStream> stream =
      CreateAndInitializeStream({}, {}, {});
  EXPECT_EQ("", stream->GetExtensions());
  EXPECT_EQ("", stream->GetSubProtocol());
}

// Verify that the sub-protocols are passed through.
TEST_P(WebSocketHandshakeStreamCreateHelperTest, SubProtocols) {
  std::vector<std::string> sub_protocols;
  sub_protocols.push_back("chat");
  sub_protocols.push_back("superchat");
  std::unique_ptr<WebSocketStream> stream = CreateAndInitializeStream(
      sub_protocols, {{"Sec-WebSocket-Protocol", "chat, superchat"}},
      {{"Sec-WebSocket-Protocol", "superchat"}});
  EXPECT_EQ("superchat", stream->GetSubProtocol());
}

// Verify that extension name is available. Bad extension names are tested in
// websocket_stream_test.cc.
TEST_P(WebSocketHandshakeStreamCreateHelperTest, Extensions) {
  std::unique_ptr<WebSocketStream> stream = CreateAndInitializeStream(
      {}, {}, {{"Sec-WebSocket-Extensions", "permessage-deflate"}});
  EXPECT_EQ("permessage-deflate", stream->GetExtensions());
}

// Verify that extension parameters are available. Bad parameters are tested in
// websocket_stream_test.cc.
TEST_P(WebSocketHandshakeStreamCreateHelperTest, ExtensionParameters) {
  std::unique_ptr<WebSocketStream> stream = CreateAndInitializeStream(
      {}, {},
      {{"Sec-WebSocket-Extensions",
        "permessage-deflate;"
        " client_max_window_bits=14; server_max_window_bits=14;"
        " server_no_context_takeover; client_no_context_takeover"}});

  EXPECT_EQ(
      "permessage-deflate;"
      " client_max_window_bits=14; server_max_window_bits=14;"
      " server_no_context_takeover; client_no_context_takeover",
      stream->GetExtensions());
}

}  // namespace
}  // namespace net
