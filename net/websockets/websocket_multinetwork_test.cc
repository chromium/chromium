// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/timer/timer.h"
#include "net/base/auth.h"
#include "net/base/isolation_info.h"
#include "net/base/net_errors.h"
#include "net/base/network_handle.h"
#include "net/cookies/site_for_cookies.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_transaction_factory.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_key.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/ssl_info.h"
#include "net/storage_access_api/status.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/target_network_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "net/websockets/websocket_channel.h"
#include "net/websockets/websocket_event_interface.h"
#include "net/websockets/websocket_handshake_request_info.h"
#include "net/websockets/websocket_handshake_response_info.h"
#include "net/websockets/websocket_stream.h"
#include "net/websockets/websocket_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"
#include "url/origin.h"

using net::test::IsOk;

namespace net {

class WebSocketMultiNetworkTest : public PlatformTest,
                                  public WithTaskEnvironment {
 public:
  WebSocketMultiNetworkTest() = default;

 protected:
  void Init(std::vector<handles::NetworkHandle> expected_networks_for_sockets,
            std::vector<handles::NetworkHandle> expected_networks_for_dns,
            std::vector<handles::NetworkHandle> expected_networks_for_proxies,
            std::optional<std::string> user_agent = std::nullopt) {
    auto context_builder = CreateTestURLRequestContextBuilder();
    context_builder->DisableHttpCache();
    HttpNetworkSessionParams session_params;
    // Disable QUIC as WebSocket over HTTP/3 is currently not enabled (see
    // net::features::kEnableWebsocketsOverHttp3 in net/base/features.h).
    // Consider enabling once kEnableWebsocketsOverHttp3 starts being rolled
    // out.
    session_params.enable_quic = false;
    context_builder->set_http_network_session_params(session_params);
    if (user_agent.has_value()) {
      context_builder->set_http_user_agent_settings(
          std::make_unique<StaticHttpUserAgentSettings>("en-us,fr",
                                                        user_agent.value()));
    }

    socket_factory_ = std::make_unique<TargetNetworkCheckingSocketFactory>(
        expected_networks_for_sockets);
    context_builder->set_client_socket_factory_for_testing(
        socket_factory_.get());

    auto host_resolver = std::make_unique<TargetNetworkCheckingHostResolver>(
        expected_networks_for_dns);
    host_resolver_ = host_resolver.get();
    context_builder->set_host_resolver(std::move(host_resolver));

    auto proxy_service =
        std::make_unique<TargetNetworkCheckingProxyResolutionService>(
            ConfiguredProxyResolutionService::CreateDirect(),
            expected_networks_for_proxies);
    proxy_resolution_service_ = proxy_service.get();
    context_builder->set_proxy_resolution_service(std::move(proxy_service));

    context_ = context_builder->Build();
  }

  // Disables automatic transmission of client SETTINGS and WINDOW_UPDATE
  // frames, which keeps MockWrite expectations simpler and easier to maintain.
  void DisableSpdyInitialData() {
    HttpNetworkSession* session =
        context_->http_transaction_factory()->GetSession();
    SpdySessionPool* pool = session->spdy_session_pool();
    SpdySessionPoolPeer pool_peer(pool);
    pool_peer.SetEnableSendingInitialData(false);
  }

  // Establishes an HTTP/2 session and receives the
  // SETTINGS_ENABLE_CONNECT_PROTOCOL frame. Without priming the connection pool
  // this way, Chromium falls back to HTTP/1.1 for WebSocket connections.
  void PrimeHttp2ConnectionPool(handles::NetworkHandle network) {
    TestDelegate delegate;
    std::unique_ptr<URLRequest> request = context_->CreateRequest(
        GURL("https://www.example.org/"), DEFAULT_PRIORITY, &delegate,
        TRAFFIC_ANNOTATION_FOR_TESTS, network);
    request->Start();
    delegate.RunUntilComplete();
    EXPECT_THAT(delegate.request_status(), IsOk());
  }

  // Connects WebSocketChannel with a test stream request API so the challenge
  // key is deterministic ("dGhlIHNhbXBsZSBub25jZQ==") for mock socket matching.
  void ConnectChannelWithDeterministicKey(
      WebSocketChannel* channel,
      const GURL& socket_url,
      const url::Origin& origin,
      const IsolationInfo& isolation_info,
      handles::NetworkHandle target_network) {
    channel->SendAddChannelRequestForTesting(
        socket_url, /*requested_protocols=*/{}, origin,
        StorageAccessApiStatus::kNone, isolation_info, HttpRequestHeaders(),
        WebSocketPriorityHint::kDefault, TRAFFIC_ANNOTATION_FOR_TESTS,
        base::BindOnce(
            [](const GURL& socket_url,
               const std::vector<std::string>& requested_subprotocols,
               const url::Origin& origin,
               StorageAccessApiStatus storage_access_api_status,
               const IsolationInfo& isolation_info,
               const HttpRequestHeaders& additional_headers,
               URLRequestContext* url_request_context,
               const NetLogWithSource& net_log,
               WebSocketPriorityHint priority_hint,
               NetworkTrafficAnnotationTag traffic_annotation,
               std::unique_ptr<WebSocketStream::ConnectDelegate>
                   connect_delegate,
               handles::NetworkHandle target_network) {
              return WebSocketStream::CreateAndConnectStreamForTesting(
                  socket_url, requested_subprotocols, origin,
                  storage_access_api_status, isolation_info, additional_headers,
                  url_request_context, net_log, priority_hint,
                  traffic_annotation, std::move(connect_delegate),
                  std::make_unique<base::OneShotTimer>(),
                  std::make_unique<TestWebSocketStreamRequestAPI>(),
                  target_network);
            }),
        target_network);
  }

  std::unique_ptr<TargetNetworkCheckingSocketFactory> socket_factory_;
  std::unique_ptr<URLRequestContext> context_;
  raw_ptr<TargetNetworkCheckingHostResolver> host_resolver_;
  raw_ptr<TargetNetworkCheckingProxyResolutionService>
      proxy_resolution_service_;
};

TEST_F(WebSocketMultiNetworkTest, HTTP1TargetNetworkIsCorrectlyPropagated) {
  const handles::NetworkHandle kTargetNetwork = 42;
  Init(/*expected_networks_for_sockets=*/{kTargetNetwork},
       /*expected_networks_for_dns=*/{kTargetNetwork},
       /*expected_networks_for_proxies=*/{kTargetNetwork});

  url::Origin origin = url::Origin::Create(GURL("http://example.com"));
  IsolationInfo isolation_info =
      IsolationInfo::Create(IsolationInfo::RequestType::kOther, origin, origin,
                            SiteForCookies::FromOrigin(origin));

  std::string request_str =
      WebSocketStandardRequest("/", "www.example.com", origin, {}, {});
  std::string response_str = WebSocketStandardResponse("");
  MockWrite writes[] = {MockWrite(request_str)};
  MockRead reads[] = {MockRead(response_str)};

  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_->AddSocketDataProvider(&socket_data);

  auto event_interface = std::make_unique<ConnectTestingEventInterface>();
  auto* event_interface_ptr = event_interface.get();
  auto channel = std::make_unique<WebSocketChannel>(std::move(event_interface),
                                                    context_.get());

  ConnectChannelWithDeterministicKey(channel.get(),
                                     GURL("ws://www.example.com/"), origin,
                                     isolation_info, kTargetNetwork);
  event_interface_ptr->WaitForResponse();
  EXPECT_FALSE(event_interface_ptr->failed());
}

TEST_F(WebSocketMultiNetworkTest, HTTP1DefaultNetworkIsCorrectlyPropagated) {
  Init(/*expected_networks_for_sockets=*/{handles::kInvalidNetworkHandle},
       /*expected_networks_for_dns=*/{handles::kInvalidNetworkHandle},
       /*expected_networks_for_proxies=*/{handles::kInvalidNetworkHandle});

  url::Origin origin = url::Origin::Create(GURL("http://example.com"));
  IsolationInfo isolation_info =
      IsolationInfo::Create(IsolationInfo::RequestType::kOther, origin, origin,
                            SiteForCookies::FromOrigin(origin));

  std::string request_str =
      WebSocketStandardRequest("/", "www.example.com", origin, {}, {});
  std::string response_str = WebSocketStandardResponse("");
  MockWrite writes[] = {MockWrite(request_str)};
  MockRead reads[] = {MockRead(response_str)};

  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_->AddSocketDataProvider(&socket_data);

  auto event_interface = std::make_unique<ConnectTestingEventInterface>();
  auto* event_interface_ptr = event_interface.get();
  auto channel = std::make_unique<WebSocketChannel>(std::move(event_interface),
                                                    context_.get());

  ConnectChannelWithDeterministicKey(
      channel.get(), GURL("ws://www.example.com/"), origin, isolation_info,
      handles::kInvalidNetworkHandle);
  event_interface_ptr->WaitForResponse();
  EXPECT_FALSE(event_interface_ptr->failed());
}

TEST_F(WebSocketMultiNetworkTest, HTTP2TargetNetworkIsCorrectlyPropagated) {
  const handles::NetworkHandle kTargetNetwork = 42;
  // Two proxy resolutions are expected (one for URLRequest and one for
  // WebSocketChannel), but only one DNS resolution and one socket are created
  // because WebSocketChannel reuses the HTTP/2 session established by
  // URLRequest.
  Init(/*expected_networks_for_sockets=*/{kTargetNetwork},
       /*expected_networks_for_dns=*/{kTargetNetwork},
       /*expected_networks_for_proxies=*/{kTargetNetwork, kTargetNetwork});
  DisableSpdyInitialData();

  SpdyTestUtil spdy_util(true);

  // Server advertising WebSocket over HTTP/2 support.
  spdy::SettingsMap read_settings;
  read_settings[spdy::SETTINGS_ENABLE_CONNECT_PROTOCOL] = 1;
  spdy::SpdySerializedFrame settings_frame =
      spdy_util.ConstructSpdySettings(read_settings);
  spdy::SpdySerializedFrame settings_ack = spdy_util.ConstructSpdySettingsAck();

  const std::string_view kExtraHeaders[] = {
      "user-agent",      "",         "accept-encoding", "gzip, deflate",
      "accept-language", "en-us,fr",
  };
  spdy::SpdySerializedFrame http_req =
      spdy_util.ConstructSpdyGet(kExtraHeaders, 1, DEFAULT_PRIORITY);
  spdy::SpdySerializedFrame http_resp =
      spdy_util.ConstructSpdyGetReply(base::span<const std::string_view>(), 1);
  spdy::SpdySerializedFrame http_data =
      spdy_util.ConstructSpdyDataFrame(1, true);

  spdy_util.UpdateWithStreamDestruction(1);

  quiche::HttpHeaderBlock ws_req_headers =
      WebSocketHttp2Request("/", "www.example.org", "http://example.org", {});
  spdy::SpdySerializedFrame ws_req = spdy_util.ConstructSpdyHeaders(
      3, std::move(ws_req_headers), DEFAULT_PRIORITY, false);

  quiche::HttpHeaderBlock ws_resp_headers = WebSocketHttp2Response({});
  spdy::SpdySerializedFrame ws_resp = spdy_util.ConstructSpdyResponseHeaders(
      3, std::move(ws_resp_headers), false);

  MockWrite writes[] = {
      CreateMockWrite(settings_ack, 1),
      CreateMockWrite(http_req, 2),
      CreateMockWrite(ws_req, 5),
  };
  MockRead reads[] = {
      CreateMockRead(settings_frame, 0),  CreateMockRead(http_resp, 3),
      CreateMockRead(http_data, 4),       CreateMockRead(ws_resp, 6),
      MockRead(ASYNC, ERR_IO_PENDING, 7), MockRead(ASYNC, OK, 8),
  };

  SequencedSocketData socket_data(reads, writes);
  socket_factory_->AddSocketDataProvider(&socket_data);

  SSLSocketDataProvider ssl_data(ASYNC, OK);
  ssl_data.next_proto = NextProto::kProtoHTTP2;
  ssl_data.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem");
  socket_factory_->AddSSLSocketDataProvider(&ssl_data);

  // 1. Establish HTTP/2 session on kTargetNetwork (priming is required to
  // receive SETTINGS_ENABLE_CONNECT_PROTOCOL so WebSockets can use HTTP/2).
  PrimeHttp2ConnectionPool(kTargetNetwork);

  // 2. Open WebSocket over the existing HTTP/2 session on kTargetNetwork
  url::Origin origin = url::Origin::Create(GURL("http://example.org"));
  IsolationInfo isolation_info =
      IsolationInfo::Create(IsolationInfo::RequestType::kOther, origin, origin,
                            SiteForCookies::FromOrigin(origin));

  auto event_interface = std::make_unique<ConnectTestingEventInterface>();
  auto* event_interface_ptr = event_interface.get();
  auto channel = std::make_unique<WebSocketChannel>(std::move(event_interface),
                                                    context_.get());

  channel->SendAddChannelRequest(
      GURL("wss://www.example.org/"), /*requested_protocols=*/{}, origin,
      StorageAccessApiStatus::kNone, isolation_info, HttpRequestHeaders(),
      WebSocketPriorityHint::kDefault, TRAFFIC_ANNOTATION_FOR_TESTS,
      kTargetNetwork);

  event_interface_ptr->WaitForResponse();
  EXPECT_FALSE(event_interface_ptr->failed());
}

TEST_F(WebSocketMultiNetworkTest, HTTP2DefaultNetworkIsCorrectlyPropagated) {
  // Two proxy resolutions are expected (one for URLRequest and one for
  // WebSocketChannel), but only one DNS resolution and one socket are created
  // because WebSocketChannel reuses the HTTP/2 session established by
  // URLRequest.
  Init(/*expected_networks_for_sockets=*/{handles::kInvalidNetworkHandle},
       /*expected_networks_for_dns=*/{handles::kInvalidNetworkHandle},
       /*expected_networks_for_proxies=*/
       {handles::kInvalidNetworkHandle, handles::kInvalidNetworkHandle});
  DisableSpdyInitialData();

  SpdyTestUtil spdy_util(true);

  spdy::SettingsMap read_settings;
  read_settings[spdy::SETTINGS_ENABLE_CONNECT_PROTOCOL] = 1;
  spdy::SpdySerializedFrame settings_frame =
      spdy_util.ConstructSpdySettings(read_settings);
  spdy::SpdySerializedFrame settings_ack = spdy_util.ConstructSpdySettingsAck();

  const std::string_view kExtraHeaders[] = {
      "user-agent",      "",         "accept-encoding", "gzip, deflate",
      "accept-language", "en-us,fr",
  };
  spdy::SpdySerializedFrame http_req =
      spdy_util.ConstructSpdyGet(kExtraHeaders, 1, DEFAULT_PRIORITY);
  spdy::SpdySerializedFrame http_resp =
      spdy_util.ConstructSpdyGetReply(base::span<const std::string_view>(), 1);
  spdy::SpdySerializedFrame http_data =
      spdy_util.ConstructSpdyDataFrame(1, true);

  spdy_util.UpdateWithStreamDestruction(1);

  quiche::HttpHeaderBlock ws_req_headers =
      WebSocketHttp2Request("/", "www.example.org", "http://example.org", {});
  spdy::SpdySerializedFrame ws_req = spdy_util.ConstructSpdyHeaders(
      3, std::move(ws_req_headers), DEFAULT_PRIORITY, false);

  quiche::HttpHeaderBlock ws_resp_headers = WebSocketHttp2Response({});
  spdy::SpdySerializedFrame ws_resp = spdy_util.ConstructSpdyResponseHeaders(
      3, std::move(ws_resp_headers), false);

  MockWrite writes[] = {
      CreateMockWrite(settings_ack, 1),
      CreateMockWrite(http_req, 2),
      CreateMockWrite(ws_req, 5),
  };
  MockRead reads[] = {
      CreateMockRead(settings_frame, 0),  CreateMockRead(http_resp, 3),
      CreateMockRead(http_data, 4),       CreateMockRead(ws_resp, 6),
      MockRead(ASYNC, ERR_IO_PENDING, 7), MockRead(ASYNC, OK, 8),
  };

  SequencedSocketData socket_data(reads, writes);
  socket_factory_->AddSocketDataProvider(&socket_data);

  SSLSocketDataProvider ssl_data(ASYNC, OK);
  ssl_data.next_proto = NextProto::kProtoHTTP2;
  ssl_data.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem");
  socket_factory_->AddSSLSocketDataProvider(&ssl_data);

  // 1. Establish HTTP/2 session on default network (priming is required to
  // receive SETTINGS_ENABLE_CONNECT_PROTOCOL so WebSockets can use HTTP/2).
  PrimeHttp2ConnectionPool(handles::kInvalidNetworkHandle);

  // 2. Open WebSocket over the existing HTTP/2 session on default network
  url::Origin origin = url::Origin::Create(GURL("http://example.org"));
  IsolationInfo isolation_info =
      IsolationInfo::Create(IsolationInfo::RequestType::kOther, origin, origin,
                            SiteForCookies::FromOrigin(origin));

  auto event_interface = std::make_unique<ConnectTestingEventInterface>();
  auto* event_interface_ptr = event_interface.get();
  auto channel = std::make_unique<WebSocketChannel>(std::move(event_interface),
                                                    context_.get());

  channel->SendAddChannelRequest(
      GURL("wss://www.example.org/"), /*requested_protocols=*/{}, origin,
      StorageAccessApiStatus::kNone, isolation_info, HttpRequestHeaders(),
      WebSocketPriorityHint::kDefault, TRAFFIC_ANNOTATION_FOR_TESTS,
      handles::kInvalidNetworkHandle);

  event_interface_ptr->WaitForResponse();
  EXPECT_FALSE(event_interface_ptr->failed());
}

TEST_F(WebSocketMultiNetworkTest,
       HTTP2DifferentTargetNetworksNeverShareConnections) {
  const handles::NetworkHandle kNetwork1 = 1;
  const handles::NetworkHandle kNetwork2 = 2;
  Init(/*expected_networks_for_sockets=*/{kNetwork1, kNetwork2},
       /*expected_networks_for_dns=*/{kNetwork1, kNetwork2},
       /*expected_networks_for_proxies=*/
       {kNetwork1, kNetwork1, kNetwork2, kNetwork2});
  DisableSpdyInitialData();

  SpdyTestUtil spdy_util1(true);
  SpdyTestUtil spdy_util2(true);

  const std::string_view kExtraHeaders[] = {
      "user-agent",      "",         "accept-encoding", "gzip, deflate",
      "accept-language", "en-us,fr",
  };

  // Socket 1 Data
  spdy::SettingsMap read_settings1;
  read_settings1[spdy::SETTINGS_ENABLE_CONNECT_PROTOCOL] = 1;
  spdy::SpdySerializedFrame settings_frame1 =
      spdy_util1.ConstructSpdySettings(read_settings1);
  spdy::SpdySerializedFrame settings_ack1 =
      spdy_util1.ConstructSpdySettingsAck();

  spdy::SpdySerializedFrame http_req1 =
      spdy_util1.ConstructSpdyGet(kExtraHeaders, 1, DEFAULT_PRIORITY);
  spdy::SpdySerializedFrame http_resp1 =
      spdy_util1.ConstructSpdyGetReply(base::span<const std::string_view>(), 1);
  spdy::SpdySerializedFrame http_data1 =
      spdy_util1.ConstructSpdyDataFrame(1, true);
  spdy_util1.UpdateWithStreamDestruction(1);

  quiche::HttpHeaderBlock ws_req_headers1 =
      WebSocketHttp2Request("/", "www.example.org", "http://example.org", {});
  spdy::SpdySerializedFrame ws_req1 = spdy_util1.ConstructSpdyHeaders(
      3, std::move(ws_req_headers1), DEFAULT_PRIORITY, false);
  quiche::HttpHeaderBlock ws_resp_headers1 = WebSocketHttp2Response({});
  spdy::SpdySerializedFrame ws_resp1 = spdy_util1.ConstructSpdyResponseHeaders(
      3, std::move(ws_resp_headers1), false);

  MockWrite writes1[] = {
      CreateMockWrite(settings_ack1, 1),
      CreateMockWrite(http_req1, 2),
      CreateMockWrite(ws_req1, 5),
  };
  MockRead reads1[] = {
      CreateMockRead(settings_frame1, 0), CreateMockRead(http_resp1, 3),
      CreateMockRead(http_data1, 4),      CreateMockRead(ws_resp1, 6),
      MockRead(ASYNC, ERR_IO_PENDING, 7), MockRead(ASYNC, OK, 8),
  };
  SequencedSocketData socket_data1(reads1, writes1);
  socket_factory_->AddSocketDataProvider(&socket_data1);

  SSLSocketDataProvider ssl_data1(ASYNC, OK);
  ssl_data1.next_proto = NextProto::kProtoHTTP2;
  ssl_data1.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem");
  socket_factory_->AddSSLSocketDataProvider(&ssl_data1);

  // Socket 2 Data
  spdy::SettingsMap read_settings2;
  read_settings2[spdy::SETTINGS_ENABLE_CONNECT_PROTOCOL] = 1;
  spdy::SpdySerializedFrame settings_frame2 =
      spdy_util2.ConstructSpdySettings(read_settings2);
  spdy::SpdySerializedFrame settings_ack2 =
      spdy_util2.ConstructSpdySettingsAck();

  spdy::SpdySerializedFrame http_req2 =
      spdy_util2.ConstructSpdyGet(kExtraHeaders, 1, DEFAULT_PRIORITY);
  spdy::SpdySerializedFrame http_resp2 =
      spdy_util2.ConstructSpdyGetReply(base::span<const std::string_view>(), 1);
  spdy::SpdySerializedFrame http_data2 =
      spdy_util2.ConstructSpdyDataFrame(1, true);
  spdy_util2.UpdateWithStreamDestruction(1);

  quiche::HttpHeaderBlock ws_req_headers2 =
      WebSocketHttp2Request("/", "www.example.org", "http://example.org", {});
  spdy::SpdySerializedFrame ws_req2 = spdy_util2.ConstructSpdyHeaders(
      3, std::move(ws_req_headers2), DEFAULT_PRIORITY, false);
  quiche::HttpHeaderBlock ws_resp_headers2 = WebSocketHttp2Response({});
  spdy::SpdySerializedFrame ws_resp2 = spdy_util2.ConstructSpdyResponseHeaders(
      3, std::move(ws_resp_headers2), false);

  MockWrite writes2[] = {
      CreateMockWrite(settings_ack2, 1),
      CreateMockWrite(http_req2, 2),
      CreateMockWrite(ws_req2, 5),
  };
  MockRead reads2[] = {
      CreateMockRead(settings_frame2, 0), CreateMockRead(http_resp2, 3),
      CreateMockRead(http_data2, 4),      CreateMockRead(ws_resp2, 6),
      MockRead(ASYNC, ERR_IO_PENDING, 7), MockRead(ASYNC, OK, 8),
  };
  SequencedSocketData socket_data2(reads2, writes2);
  socket_factory_->AddSocketDataProvider(&socket_data2);

  SSLSocketDataProvider ssl_data2(ASYNC, OK);
  ssl_data2.next_proto = NextProto::kProtoHTTP2;
  ssl_data2.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem");
  socket_factory_->AddSSLSocketDataProvider(&ssl_data2);

  url::Origin origin = url::Origin::Create(GURL("http://example.org"));
  IsolationInfo isolation_info =
      IsolationInfo::Create(IsolationInfo::RequestType::kOther, origin, origin,
                            SiteForCookies::FromOrigin(origin));

  // 1. Prime HTTP/2 session on Network 1
  PrimeHttp2ConnectionPool(kNetwork1);

  // 2. Open WebSocket on Network 1
  auto event_interface1 = std::make_unique<ConnectTestingEventInterface>();
  auto* event_interface1_ptr = event_interface1.get();
  auto channel1 = std::make_unique<WebSocketChannel>(
      std::move(event_interface1), context_.get());
  channel1->SendAddChannelRequest(
      GURL("wss://www.example.org/"), /*requested_protocols=*/{}, origin,
      StorageAccessApiStatus::kNone, isolation_info, HttpRequestHeaders(),
      WebSocketPriorityHint::kDefault, TRAFFIC_ANNOTATION_FOR_TESTS, kNetwork1);
  event_interface1_ptr->WaitForResponse();
  EXPECT_FALSE(event_interface1_ptr->failed());

  // 3. Prime HTTP/2 session on Network 2
  PrimeHttp2ConnectionPool(kNetwork2);

  // 4. Open WebSocket on Network 2
  auto event_interface2 = std::make_unique<ConnectTestingEventInterface>();
  auto* event_interface2_ptr = event_interface2.get();
  auto channel2 = std::make_unique<WebSocketChannel>(
      std::move(event_interface2), context_.get());
  channel2->SendAddChannelRequest(
      GURL("wss://www.example.org/"), /*requested_protocols=*/{}, origin,
      StorageAccessApiStatus::kNone, isolation_info, HttpRequestHeaders(),
      WebSocketPriorityHint::kDefault, TRAFFIC_ANNOTATION_FOR_TESTS, kNetwork2);
  event_interface2_ptr->WaitForResponse();
  EXPECT_FALSE(event_interface2_ptr->failed());

  EXPECT_EQ(socket_factory_->tcp_socket_count(), 2u);
}

TEST_F(WebSocketMultiNetworkTest, HTTP2SameTargetNetworkSharesConnections) {
  const handles::NetworkHandle kNetwork = 1;
  // Proxy resolutions occur for the URLRequest and each WebSocketChannel (3
  // total). Only one DNS resolution and one socket are created because all
  // requests share the same HTTP/2 session.
  Init(/*expected_networks_for_sockets=*/{kNetwork},
       /*expected_networks_for_dns=*/{kNetwork},
       /*expected_networks_for_proxies=*/{kNetwork, kNetwork, kNetwork});
  DisableSpdyInitialData();

  SpdyTestUtil spdy_util(true);

  spdy::SettingsMap read_settings;
  read_settings[spdy::SETTINGS_ENABLE_CONNECT_PROTOCOL] = 1;
  spdy::SpdySerializedFrame settings_frame =
      spdy_util.ConstructSpdySettings(read_settings);
  spdy::SpdySerializedFrame settings_ack = spdy_util.ConstructSpdySettingsAck();

  // First HTTP request on stream 1
  const std::string_view kExtraHeaders[] = {
      "user-agent",      "",         "accept-encoding", "gzip, deflate",
      "accept-language", "en-us,fr",
  };
  spdy::SpdySerializedFrame http_req =
      spdy_util.ConstructSpdyGet(kExtraHeaders, 1, DEFAULT_PRIORITY);
  spdy::SpdySerializedFrame http_resp =
      spdy_util.ConstructSpdyGetReply(base::span<const std::string_view>(), 1);
  spdy::SpdySerializedFrame http_data =
      spdy_util.ConstructSpdyDataFrame(1, true);
  spdy_util.UpdateWithStreamDestruction(1);

  // First WebSocket request on stream 3
  quiche::HttpHeaderBlock ws_req_headers1 =
      WebSocketHttp2Request("/", "www.example.org", "http://example.org", {});
  spdy::SpdySerializedFrame ws_req1 = spdy_util.ConstructSpdyHeaders(
      3, std::move(ws_req_headers1), DEFAULT_PRIORITY, false);
  quiche::HttpHeaderBlock ws_resp_headers1 = WebSocketHttp2Response({});
  spdy::SpdySerializedFrame ws_resp1 = spdy_util.ConstructSpdyResponseHeaders(
      3, std::move(ws_resp_headers1), false);

  // Second WebSocket request on stream 5
  quiche::HttpHeaderBlock ws_req_headers2 =
      WebSocketHttp2Request("/", "www.example.org", "http://example.org", {});
  spdy::SpdySerializedFrame ws_req2 = spdy_util.ConstructSpdyHeaders(
      5, std::move(ws_req_headers2), DEFAULT_PRIORITY, false);
  quiche::HttpHeaderBlock ws_resp_headers2 = WebSocketHttp2Response({});
  spdy::SpdySerializedFrame ws_resp2 = spdy_util.ConstructSpdyResponseHeaders(
      5, std::move(ws_resp_headers2), false);

  MockWrite writes[] = {
      CreateMockWrite(settings_ack, 1),
      CreateMockWrite(http_req, 2),
      CreateMockWrite(ws_req1, 5),
      CreateMockWrite(ws_req2, 7),
  };
  MockRead reads[] = {
      CreateMockRead(settings_frame, 0), CreateMockRead(http_resp, 3),
      CreateMockRead(http_data, 4),      CreateMockRead(ws_resp1, 6),
      CreateMockRead(ws_resp2, 8),       MockRead(ASYNC, ERR_IO_PENDING, 9),
      MockRead(ASYNC, OK, 10),
  };

  SequencedSocketData socket_data(reads, writes);
  socket_factory_->AddSocketDataProvider(&socket_data);

  SSLSocketDataProvider ssl_data(ASYNC, OK);
  ssl_data.next_proto = NextProto::kProtoHTTP2;
  ssl_data.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem");
  socket_factory_->AddSSLSocketDataProvider(&ssl_data);

  // 1. Establish HTTP/2 session on kNetwork (priming is required to
  // receive SETTINGS_ENABLE_CONNECT_PROTOCOL so WebSockets can use HTTP/2).
  PrimeHttp2ConnectionPool(kNetwork);

  url::Origin origin = url::Origin::Create(GURL("http://example.org"));
  IsolationInfo isolation_info =
      IsolationInfo::Create(IsolationInfo::RequestType::kOther, origin, origin,
                            SiteForCookies::FromOrigin(origin));

  // 2. Request 1 on kNetwork
  auto event_interface1 = std::make_unique<ConnectTestingEventInterface>();
  auto* event_interface1_ptr = event_interface1.get();
  auto channel1 = std::make_unique<WebSocketChannel>(
      std::move(event_interface1), context_.get());
  channel1->SendAddChannelRequest(
      GURL("wss://www.example.org/"), /*requested_protocols=*/{}, origin,
      StorageAccessApiStatus::kNone, isolation_info, HttpRequestHeaders(),
      WebSocketPriorityHint::kDefault, TRAFFIC_ANNOTATION_FOR_TESTS, kNetwork);
  event_interface1_ptr->WaitForResponse();
  EXPECT_FALSE(event_interface1_ptr->failed());

  // 3. Request 2 on kNetwork (reuses the same SpdySession on kNetwork)
  auto event_interface2 = std::make_unique<ConnectTestingEventInterface>();
  auto* event_interface2_ptr = event_interface2.get();
  auto channel2 = std::make_unique<WebSocketChannel>(
      std::move(event_interface2), context_.get());
  channel2->SendAddChannelRequest(
      GURL("wss://www.example.org/"), /*requested_protocols=*/{}, origin,
      StorageAccessApiStatus::kNone, isolation_info, HttpRequestHeaders(),
      WebSocketPriorityHint::kDefault, TRAFFIC_ANNOTATION_FOR_TESTS, kNetwork);
  event_interface2_ptr->WaitForResponse();
  EXPECT_FALSE(event_interface2_ptr->failed());

  EXPECT_EQ(socket_factory_->tcp_socket_count(), 1u);
}

}  // namespace net
