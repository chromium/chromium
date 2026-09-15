// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/base/network_handle.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/alternative_service.h"
#include "net/http/bidirectional_stream.h"
#include "net/http/bidirectional_stream_request_info.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_transaction_factory.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_session_key.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/test/gtest_util.h"
#include "net/test/target_network_test_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

namespace {

class BidirectionalStreamTestDelegate : public BidirectionalStream::Delegate {
 public:
  BidirectionalStreamTestDelegate() = default;

  void OnStreamReady(bool request_headers_sent) override {}
  void OnHeadersReceived(const quiche::HttpHeaderBlock& response_headers,
                         const net::ProxyInfo& used_proxy_info) override {}
  void OnDataRead(int bytes_read) override {}
  void OnDataSent() override {}
  void OnTrailersReceived(const quiche::HttpHeaderBlock& trailers) override {}
  void OnFailed(int status) override {}
};

}  // namespace

class BidirectionalStreamMultiNetworkTest : public PlatformTest,
                                           public WithTaskEnvironment {
 public:
  BidirectionalStreamMultiNetworkTest() = default;

 protected:
  void Init(std::vector<handles::NetworkHandle> expected_networks_for_sockets,
            std::vector<handles::NetworkHandle> expected_networks_for_dns,
            std::vector<handles::NetworkHandle> expected_networks_for_proxies,
            std::optional<std::string> user_agent = std::nullopt,
            bool enable_quic = false) {
    auto context_builder = CreateTestURLRequestContextBuilder();
    context_builder->DisableHttpCache();
    HttpNetworkSessionParams session_params;
    session_params.enable_quic = enable_quic;
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

  void DisableSpdyInitialData() {
    HttpNetworkSession* session =
        context_->http_transaction_factory()->GetSession();
    SpdySessionPool* pool = session->spdy_session_pool();
    SpdySessionPoolPeer pool_peer(pool);
    pool_peer.SetEnableSendingInitialData(false);
  }

  std::unique_ptr<TargetNetworkCheckingSocketFactory> socket_factory_;
  std::unique_ptr<URLRequestContext> context_;
  raw_ptr<TargetNetworkCheckingHostResolver> host_resolver_;
  raw_ptr<TargetNetworkCheckingProxyResolutionService>
      proxy_resolution_service_;
};

TEST_F(BidirectionalStreamMultiNetworkTest,
       BidirectionalStreamHTTP2TargetNetworkIsCorrectlyPropagated) {
  const handles::NetworkHandle kTargetNetwork = 42;
  Init(/*expected_networks_for_sockets=*/{kTargetNetwork},
       /*expected_networks_for_dns=*/{kTargetNetwork},
       /*expected_networks_for_proxies=*/{kTargetNetwork});
  DisableSpdyInitialData();

  SpdyTestUtil spdy_util;
  spdy::SpdySerializedFrame req =
      spdy_util.ConstructSpdyGet("https://www.example.com", 1, LOW);

  MockWrite writes[] = {
      CreateMockWrite(req, 0),
  };
  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 1),
      MockRead(ASYNC, OK, 2),
  };
  SequencedSocketData socket_data(reads, writes);
  socket_factory_->AddSocketDataProvider(&socket_data);

  SSLSocketDataProvider ssl_data(ASYNC, OK);
  ssl_data.next_proto = NextProto::kProtoHTTP2;
  socket_factory_->AddSSLSocketDataProvider(&ssl_data);

  auto request_info = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info->method = "GET";
  request_info->url = GURL("https://www.example.com");
  request_info->end_stream_on_headers = true;
  request_info->target_network = kTargetNetwork;

  BidirectionalStreamTestDelegate delegate;
  auto stream = std::make_unique<BidirectionalStream>(
      std::move(request_info),
      context_->http_transaction_factory()->GetSession(), true, &delegate);
  ASSERT_TRUE(
      base::test::RunUntil([&] { return socket_data.AllWriteDataConsumed(); }));

  EXPECT_EQ(host_resolver_->num_resolve(), 1u);
  EXPECT_EQ(socket_factory_->tcp_socket_count(), 1u);
}

TEST_F(BidirectionalStreamMultiNetworkTest,
       BidirectionalStreamHTTP2DefaultNetworkIsCorrectlyPropagated) {
  Init(/*expected_networks_for_sockets=*/{handles::kInvalidNetworkHandle},
       /*expected_networks_for_dns=*/{handles::kInvalidNetworkHandle},
       /*expected_networks_for_proxies=*/{handles::kInvalidNetworkHandle});
  DisableSpdyInitialData();

  SpdyTestUtil spdy_util;
  spdy::SpdySerializedFrame req =
      spdy_util.ConstructSpdyGet("https://www.example.com", 1, LOW);

  MockWrite writes[] = {
      CreateMockWrite(req, 0),
  };
  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 1),
      MockRead(ASYNC, OK, 2),
  };
  SequencedSocketData socket_data(reads, writes);
  socket_factory_->AddSocketDataProvider(&socket_data);

  SSLSocketDataProvider ssl_data(ASYNC, OK);
  ssl_data.next_proto = NextProto::kProtoHTTP2;
  socket_factory_->AddSSLSocketDataProvider(&ssl_data);

  auto request_info = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info->method = "GET";
  request_info->url = GURL("https://www.example.com");
  request_info->end_stream_on_headers = true;
  request_info->target_network = handles::kInvalidNetworkHandle;

  BidirectionalStreamTestDelegate delegate;
  auto stream = std::make_unique<BidirectionalStream>(
      std::move(request_info),
      context_->http_transaction_factory()->GetSession(), true, &delegate);
  ASSERT_TRUE(
      base::test::RunUntil([&] { return socket_data.AllWriteDataConsumed(); }));

  EXPECT_EQ(host_resolver_->num_resolve(), 1u);
  EXPECT_EQ(socket_factory_->tcp_socket_count(), 1u);
}

TEST_F(BidirectionalStreamMultiNetworkTest,
       BidirectionalStreamHTTP2DifferentTargetNetworksNeverShareConnections) {
  const handles::NetworkHandle kNetwork1 = 1;
  const handles::NetworkHandle kNetwork2 = 2;
  Init(/*expected_networks_for_sockets=*/{kNetwork1, kNetwork2},
       /*expected_networks_for_dns=*/{kNetwork1, kNetwork2},
       /*expected_networks_for_proxies=*/{kNetwork1, kNetwork2});
  DisableSpdyInitialData();

  SpdyTestUtil spdy_util1;
  SpdyTestUtil spdy_util2;

  spdy::SpdySerializedFrame req1 =
      spdy_util1.ConstructSpdyGet("https://www.example.com", 1, LOW);
  spdy::SpdySerializedFrame req2 =
      spdy_util2.ConstructSpdyGet("https://www.example.com", 1, LOW);

  MockWrite writes1[] = {
      CreateMockWrite(req1, 0),
  };
  MockRead reads1[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 1),
      MockRead(ASYNC, OK, 2),
  };
  SequencedSocketData socket_data1(reads1, writes1);
  socket_factory_->AddSocketDataProvider(&socket_data1);

  SSLSocketDataProvider ssl_data1(ASYNC, OK);
  ssl_data1.next_proto = NextProto::kProtoHTTP2;
  socket_factory_->AddSSLSocketDataProvider(&ssl_data1);

  MockWrite writes2[] = {
      CreateMockWrite(req2, 0),
  };
  MockRead reads2[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 1),
      MockRead(ASYNC, OK, 2),
  };
  SequencedSocketData socket_data2(reads2, writes2);
  socket_factory_->AddSocketDataProvider(&socket_data2);

  SSLSocketDataProvider ssl_data2(ASYNC, OK);
  ssl_data2.next_proto = NextProto::kProtoHTTP2;
  socket_factory_->AddSSLSocketDataProvider(&ssl_data2);

  auto request_info1 = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info1->method = "GET";
  request_info1->url = GURL("https://www.example.com");
  request_info1->end_stream_on_headers = true;
  request_info1->target_network = kNetwork1;

  BidirectionalStreamTestDelegate delegate1;
  auto stream1 = std::make_unique<BidirectionalStream>(
      std::move(request_info1),
      context_->http_transaction_factory()->GetSession(), true, &delegate1);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return socket_factory_->mock_data().next_index() == 1; }));

  auto request_info2 = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info2->method = "GET";
  request_info2->url = GURL("https://www.example.com");
  request_info2->end_stream_on_headers = true;
  request_info2->target_network = kNetwork2;

  BidirectionalStreamTestDelegate delegate2;
  auto stream2 = std::make_unique<BidirectionalStream>(
      std::move(request_info2),
      context_->http_transaction_factory()->GetSession(), true, &delegate2);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return socket_factory_->mock_data().next_index() == 2; }));

  ASSERT_TRUE(
      base::test::RunUntil([&] {
        return socket_data1.AllWriteDataConsumed() &&
               socket_data2.AllWriteDataConsumed();
      }));
  EXPECT_EQ(host_resolver_->num_resolve(), 2u);
  EXPECT_EQ(socket_factory_->tcp_socket_count(), 2u);
}

TEST_F(BidirectionalStreamMultiNetworkTest,
       BidirectionalStreamHTTP2SameTargetNetworkSharesConnections) {
  const handles::NetworkHandle kNetwork = 1;
  Init(/*expected_networks_for_sockets=*/{kNetwork},
       /*expected_networks_for_dns=*/{kNetwork},
       /*expected_networks_for_proxies=*/{kNetwork, kNetwork});
  DisableSpdyInitialData();

  SpdyTestUtil spdy_util;

  spdy::SpdySerializedFrame req1 =
      spdy_util.ConstructSpdyGet("https://www.example.com", 1, LOW);
  spdy::SpdySerializedFrame req2 =
      spdy_util.ConstructSpdyGet("https://www.example.com", 3, LOW);

  MockWrite writes[] = {
      CreateMockWrite(req1, 0),
      CreateMockWrite(req2, 1),
  };
  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 2),
      MockRead(ASYNC, OK, 3),
  };

  SequencedSocketData socket_data(reads, writes);
  socket_factory_->AddSocketDataProvider(&socket_data);

  SSLSocketDataProvider ssl_data(ASYNC, OK);
  ssl_data.next_proto = NextProto::kProtoHTTP2;
  socket_factory_->AddSSLSocketDataProvider(&ssl_data);

  auto request_info1 = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info1->method = "GET";
  request_info1->url = GURL("https://www.example.com");
  request_info1->end_stream_on_headers = true;
  request_info1->target_network = kNetwork;

  BidirectionalStreamTestDelegate delegate1;
  auto stream1 = std::make_unique<BidirectionalStream>(
      std::move(request_info1),
      context_->http_transaction_factory()->GetSession(), true, &delegate1);

  SpdySessionKey key(
      HostPortPair("www.example.com", 443), PRIVACY_MODE_DISABLED,
      ProxyChain::Direct(), SessionUsage::kDestination, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      /*disable_cert_verification_network_fetches=*/false, kNetwork);

  ASSERT_TRUE(base::test::RunUntil([&] {
    return context_->http_transaction_factory()
               ->GetSession()
               ->spdy_session_pool()
               ->FindAvailableSession(key,
                                      /*enable_ip_based_pooling_for_h2=*/true,
                                      /*is_websocket=*/false,
                                      NetLogWithSource()) != nullptr;
  }));

  auto request_info2 = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info2->method = "GET";
  request_info2->url = GURL("https://www.example.com");
  request_info2->end_stream_on_headers = true;
  request_info2->target_network = kNetwork;

  BidirectionalStreamTestDelegate delegate2;
  auto stream2 = std::make_unique<BidirectionalStream>(
      std::move(request_info2),
      context_->http_transaction_factory()->GetSession(), true, &delegate2);

  ASSERT_TRUE(
      base::test::RunUntil([&] { return socket_data.AllWriteDataConsumed(); }));

  EXPECT_EQ(socket_factory_->mock_data().next_index(), 1u);
  EXPECT_EQ(host_resolver_->num_resolve(), 1u);
  EXPECT_EQ(socket_factory_->tcp_socket_count(), 1u);
}

TEST_F(BidirectionalStreamMultiNetworkTest,
       BidirectionalStreamQUICTargetNetworkIsCorrectlyPropagated) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kAsyncQuicSession);
  const handles::NetworkHandle kTargetNetwork = 42;

  Init(/*expected_networks_for_sockets=*/{kTargetNetwork},
       /*expected_networks_for_dns=*/{kTargetNetwork},
       /*expected_networks_for_proxies=*/{kTargetNetwork}, std::nullopt,
       /*enable_quic=*/true);
  host_resolver_->set_ondemand_mode(true);

  AlternativeServiceInfo alternative_service_info =
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          AlternativeService(NextProto::kProtoQUIC, "www.example.com", 443),
          base::Time::Now() + base::Days(1), DefaultSupportedQuicVersions());
  url::SchemeHostPort server("https", "www.example.com", 443);
  context_->http_server_properties()->SetAlternativeServices(
      server, NetworkAnonymizationKey(), {alternative_service_info});

  MockConnect udp_connect(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData udp_data(udp_connect, base::span<const MockRead>(),
                                base::span<const MockWrite>());
  socket_factory_->AddSocketDataProvider(&udp_data);

  auto request_info = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info->method = "GET";
  request_info->url = GURL("https://www.example.com");
  request_info->end_stream_on_headers = true;
  request_info->target_network = kTargetNetwork;

  BidirectionalStreamTestDelegate delegate;
  auto stream = std::make_unique<BidirectionalStream>(
      std::move(request_info),
      context_->http_transaction_factory()->GetSession(), true, &delegate);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return host_resolver_->num_resolve() == 1u; }));

  host_resolver_->ResolveAllPending();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return socket_factory_->udp_socket_count() == 1u; }));

  EXPECT_EQ(socket_factory_->tcp_socket_count(), 0u);
}

TEST_F(BidirectionalStreamMultiNetworkTest,
       BidirectionalStreamQUICDefaultNetworkIsCorrectlyPropagated) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kAsyncQuicSession);

  Init(/*expected_networks_for_sockets=*/{handles::kInvalidNetworkHandle},
       /*expected_networks_for_dns=*/{handles::kInvalidNetworkHandle},
       /*expected_networks_for_proxies=*/{handles::kInvalidNetworkHandle},
       std::nullopt,
       /*enable_quic=*/true);
  host_resolver_->set_ondemand_mode(true);

  AlternativeServiceInfo alternative_service_info =
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          AlternativeService(NextProto::kProtoQUIC, "www.example.com", 443),
          base::Time::Now() + base::Days(1), DefaultSupportedQuicVersions());
  url::SchemeHostPort server("https", "www.example.com", 443);
  context_->http_server_properties()->SetAlternativeServices(
      server, NetworkAnonymizationKey(), {alternative_service_info});

  MockConnect udp_connect(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData udp_data(udp_connect, base::span<const MockRead>(),
                                base::span<const MockWrite>());
  socket_factory_->AddSocketDataProvider(&udp_data);

  auto request_info = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info->method = "GET";
  request_info->url = GURL("https://www.example.com");
  request_info->end_stream_on_headers = true;
  request_info->target_network = handles::kInvalidNetworkHandle;

  BidirectionalStreamTestDelegate delegate;
  auto stream = std::make_unique<BidirectionalStream>(
      std::move(request_info),
      context_->http_transaction_factory()->GetSession(), true, &delegate);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return host_resolver_->num_resolve() == 1u; }));

  host_resolver_->ResolveAllPending();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return socket_factory_->udp_socket_count() == 1u; }));

  EXPECT_EQ(socket_factory_->tcp_socket_count(), 0u);
}

TEST_F(BidirectionalStreamMultiNetworkTest,
       BidirectionalStreamQUICDifferentTargetNetworksNeverShareConnections) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kAsyncQuicSession);
  const handles::NetworkHandle kNetwork1 = 1;
  const handles::NetworkHandle kNetwork2 = 2;

  Init(/*expected_networks_for_sockets=*/{kNetwork1, kNetwork2},
       /*expected_networks_for_dns=*/{kNetwork1, kNetwork2},
       /*expected_networks_for_proxies=*/{kNetwork1, kNetwork2}, std::nullopt,
       /*enable_quic=*/true);
  host_resolver_->set_ondemand_mode(true);

  AlternativeServiceInfo alternative_service_info =
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          AlternativeService(NextProto::kProtoQUIC, "www.example.com", 443),
          base::Time::Now() + base::Days(1), DefaultSupportedQuicVersions());
  url::SchemeHostPort server("https", "www.example.com", 443);
  context_->http_server_properties()->SetAlternativeServices(
      server, NetworkAnonymizationKey(), {alternative_service_info});

  MockConnect udp_connect1(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData udp_data1(udp_connect1, base::span<const MockRead>(),
                                base::span<const MockWrite>());
  socket_factory_->AddSocketDataProvider(&udp_data1);

  MockConnect udp_connect2(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData udp_data2(udp_connect2, base::span<const MockRead>(),
                                base::span<const MockWrite>());
  socket_factory_->AddSocketDataProvider(&udp_data2);

  auto request_info1 = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info1->method = "GET";
  request_info1->url = GURL("https://www.example.com");
  request_info1->end_stream_on_headers = true;
  request_info1->target_network = kNetwork1;

  BidirectionalStreamTestDelegate delegate1;
  auto stream1 = std::make_unique<BidirectionalStream>(
      std::move(request_info1),
      context_->http_transaction_factory()->GetSession(), true, &delegate1);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return host_resolver_->num_resolve() == 1u; }));

  host_resolver_->ResolveAllPending();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return socket_factory_->udp_socket_count() == 1u; }));

  auto request_info2 = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info2->method = "GET";
  request_info2->url = GURL("https://www.example.com");
  request_info2->end_stream_on_headers = true;
  request_info2->target_network = kNetwork2;

  BidirectionalStreamTestDelegate delegate2;
  auto stream2 = std::make_unique<BidirectionalStream>(
      std::move(request_info2),
      context_->http_transaction_factory()->GetSession(), true, &delegate2);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return host_resolver_->num_resolve() == 2u; }));

  host_resolver_->ResolveAllPending();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return socket_factory_->udp_socket_count() == 2u; }));

  EXPECT_EQ(socket_factory_->tcp_socket_count(), 0u);
}

TEST_F(BidirectionalStreamMultiNetworkTest,
       BidirectionalStreamQUICSameTargetNetworkSharesConnections) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kAsyncQuicSession);
  const handles::NetworkHandle kNetwork = 1;

  Init(/*expected_networks_for_sockets=*/{kNetwork},
       /*expected_networks_for_dns=*/{kNetwork},
       /*expected_networks_for_proxies=*/{kNetwork, kNetwork}, std::nullopt,
       /*enable_quic=*/true);
  host_resolver_->set_ondemand_mode(true);

  AlternativeServiceInfo alternative_service_info =
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          AlternativeService(NextProto::kProtoQUIC, "www.example.com", 443),
          base::Time::Now() + base::Days(1), DefaultSupportedQuicVersions());
  url::SchemeHostPort server("https", "www.example.com", 443);
  context_->http_server_properties()->SetAlternativeServices(
      server, NetworkAnonymizationKey(), {alternative_service_info});

  MockConnect udp_connect(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData udp_data(udp_connect, base::span<const MockRead>(),
                                base::span<const MockWrite>());
  socket_factory_->AddSocketDataProvider(&udp_data);

  auto request_info1 = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info1->method = "GET";
  request_info1->url = GURL("https://www.example.com");
  request_info1->end_stream_on_headers = true;
  request_info1->target_network = kNetwork;

  BidirectionalStreamTestDelegate delegate1;
  auto stream1 = std::make_unique<BidirectionalStream>(
      std::move(request_info1),
      context_->http_transaction_factory()->GetSession(), true, &delegate1);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return host_resolver_->num_resolve() == 1u; }));

  host_resolver_->ResolveAllPending();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return socket_factory_->udp_socket_count() == 1u; }));

  auto request_info2 = std::make_unique<BidirectionalStreamRequestInfo>();
  request_info2->method = "GET";
  request_info2->url = GURL("https://www.example.com");
  request_info2->end_stream_on_headers = true;
  request_info2->target_network = kNetwork;

  BidirectionalStreamTestDelegate delegate2;
  auto stream2 = std::make_unique<BidirectionalStream>(
      std::move(request_info2),
      context_->http_transaction_factory()->GetSession(), true, &delegate2);
  ASSERT_TRUE(base::test::RunUntil([&] {
    return proxy_resolution_service_->current_proxy_request() == 2u;
  }));

  EXPECT_EQ(host_resolver_->num_resolve(), 1u);
  EXPECT_EQ(socket_factory_->udp_socket_count(), 1u);
}

}  // namespace net
