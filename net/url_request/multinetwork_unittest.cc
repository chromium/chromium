// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <sstream>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/base/network_handle.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/alternative_service.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_transaction_factory.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/quic/quic_context.h"
#include "net/quic/quic_session_pool.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_session_key.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using net::test::IsOk;

namespace net {

namespace {

class TargetNetworkCheckingSocketFactory : public MockClientSocketFactory {
 public:
  explicit TargetNetworkCheckingSocketFactory(
      std::vector<handles::NetworkHandle> expected_networks)
      : expected_networks_(std::move(expected_networks)) {}
  ~TargetNetworkCheckingSocketFactory() override {
    EXPECT_EQ(current_socket_request_, expected_networks_.size());
  }

  std::unique_ptr<TransportClientSocket> CreateTransportClientSocket(
      const AddressList& addresses,
      handles::NetworkHandle target_network,
      std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
      NetworkQualityEstimator* network_quality_estimator,
      NetLog* net_log,
      const NetLogSource& source) override {
    if (current_socket_request_ >= expected_networks_.size()) {
      ADD_FAILURE() << "Unexpected CreateTransportClientSocket call";
      return nullptr;
    }
    EXPECT_EQ(target_network, expected_networks_[current_socket_request_]);
    tcp_socket_count_++;
    current_socket_request_++;
    return MockClientSocketFactory::CreateTransportClientSocket(
        addresses, target_network, std::move(socket_performance_watcher),
        network_quality_estimator, net_log, source);
  }

  std::unique_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      handles::NetworkHandle target_network,
      NetLog* net_log,
      const NetLogSource& source) override {
    if (current_socket_request_ >= expected_networks_.size()) {
      ADD_FAILURE() << "Unexpected CreateDatagramClientSocket call";
      return nullptr;
    }
    EXPECT_EQ(target_network, expected_networks_[current_socket_request_]);
    udp_socket_count_++;
    current_socket_request_++;
    return MockClientSocketFactory::CreateDatagramClientSocket(
        bind_type, target_network, net_log, source);
  }

  size_t tcp_socket_count() const { return tcp_socket_count_; }
  size_t udp_socket_count() const { return udp_socket_count_; }

 private:
  std::vector<handles::NetworkHandle> expected_networks_;
  size_t current_socket_request_ = 0;
  size_t tcp_socket_count_ = 0;
  size_t udp_socket_count_ = 0;
};

class TargetNetworkCheckingHostResolver : public MockHostResolver {
 public:
  explicit TargetNetworkCheckingHostResolver(
      std::vector<handles::NetworkHandle> expected_networks)
      : MockHostResolver(
            MockHostResolverBase::RuleResolver::GetLocalhostResult()),
        expected_networks_(std::move(expected_networks)) {}
  ~TargetNetworkCheckingHostResolver() override {
    EXPECT_EQ(current_dns_request_, expected_networks_.size());
  }

  std::unique_ptr<ResolveHostRequest> CreateRequest(
      url::SchemeHostPort host,
      NetworkAnonymizationKey network_anonymization_key,
      handles::NetworkHandle target_network,
      NetLogWithSource net_log,
      std::optional<ResolveHostParameters> optional_parameters) override {
    if (current_dns_request_ >= expected_networks_.size()) {
      ADD_FAILURE() << "Unexpected CreateRequest call";
      return nullptr;
    }
    EXPECT_EQ(target_network, expected_networks_[current_dns_request_]);
    current_dns_request_++;
    return MockHostResolver::CreateRequest(
        std::move(host), std::move(network_anonymization_key), target_network,
        std::move(net_log), std::move(optional_parameters));
  }

  std::unique_ptr<ResolveHostRequest> CreateRequest(
      const HostPortPair& host,
      const NetworkAnonymizationKey& network_anonymization_key,
      handles::NetworkHandle target_network,
      const NetLogWithSource& net_log,
      const std::optional<ResolveHostParameters>& optional_parameters)
      override {
    if (current_dns_request_ >= expected_networks_.size()) {
      ADD_FAILURE() << "Unexpected CreateRequest call";
      return nullptr;
    }
    EXPECT_EQ(target_network, expected_networks_[current_dns_request_]);
    current_dns_request_++;
    return MockHostResolver::CreateRequest(host, network_anonymization_key,
                                           target_network, net_log,
                                           optional_parameters);
  }

  std::unique_ptr<ServiceEndpointRequest> CreateServiceEndpointRequest(
      Host host,
      NetworkAnonymizationKey network_anonymization_key,
      handles::NetworkHandle target_network,
      NetLogWithSource net_log,
      ResolveHostParameters parameters) override {
    if (current_dns_request_ >= expected_networks_.size()) {
      ADD_FAILURE() << "Unexpected CreateServiceEndpointRequest call";
      return nullptr;
    }
    EXPECT_EQ(target_network, expected_networks_[current_dns_request_]);
    current_dns_request_++;
    return MockHostResolver::CreateServiceEndpointRequest(
        std::move(host), std::move(network_anonymization_key), target_network,
        std::move(net_log), std::move(parameters));
  }

 private:
  std::vector<handles::NetworkHandle> expected_networks_;
  size_t current_dns_request_ = 0;
};

class TargetNetworkCheckingProxyResolutionService
    : public ProxyResolutionService {
 public:
  TargetNetworkCheckingProxyResolutionService(
      std::unique_ptr<ProxyResolutionService> delegate,
      std::vector<handles::NetworkHandle> expected_networks)
      : delegate_(std::move(delegate)),
        expected_networks_(std::move(expected_networks)) {}

  ~TargetNetworkCheckingProxyResolutionService() override {
    EXPECT_EQ(current_proxy_request_, expected_networks_.size());
  }

  int ResolveProxy(const GURL& url,
                   const std::string& method,
                   const NetworkAnonymizationKey& network_anonymization_key,
                   handles::NetworkHandle target_network,
                   ProxyInfo* results,
                   CompletionOnceCallback callback,
                   std::unique_ptr<ProxyResolutionRequest>* request,
                   const NetLogWithSource& net_log,
                   RequestPriority priority) override {
    if (current_proxy_request_ >= expected_networks_.size()) {
      ADD_FAILURE() << "Unexpected ResolveProxy call";
      return ERR_UNEXPECTED;
    }
    EXPECT_EQ(target_network, expected_networks_[current_proxy_request_]);
    current_proxy_request_++;
    return delegate_->ResolveProxy(url, method, network_anonymization_key,
                                   target_network, results, std::move(callback),
                                   request, net_log, priority);
  }

  void ReportSuccess(const ProxyInfo& proxy_info) override {
    delegate_->ReportSuccess(proxy_info);
  }

  void SetProxyDelegate(ProxyDelegate* delegate) override {
    delegate_->SetProxyDelegate(delegate);
  }

  void OnShutdown() override { delegate_->OnShutdown(); }

  void ClearBadProxiesCache() override { delegate_->ClearBadProxiesCache(); }

  const ProxyRetryInfoMap& proxy_retry_info() const override {
    return delegate_->proxy_retry_info();
  }

  base::DictValue GetProxyNetLogValues() override {
    return delegate_->GetProxyNetLogValues();
  }

  bool CastToConfiguredProxyResolutionService(
      ConfiguredProxyResolutionService** configured_proxy_resolution_service)
      override {
    *configured_proxy_resolution_service = nullptr;
    return false;
  }

  size_t current_proxy_request() const { return current_proxy_request_; }

 private:
  std::unique_ptr<ProxyResolutionService> delegate_;
  std::vector<handles::NetworkHandle> expected_networks_;
  size_t current_proxy_request_ = 0;
};

}  // namespace

class MultiNetworkTest : public PlatformTest, public WithTaskEnvironment {
 public:
  MultiNetworkTest() = default;

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
    // Disable sending initial SETTINGS and window update frames automatically.
    // This is a common pattern in HTTP/2 tests to prevent the session from
    // writing handshake frames that are not expected by the MockWrites.
    pool_peer.SetEnableSendingInitialData(false);
  }

  std::unique_ptr<TargetNetworkCheckingSocketFactory> socket_factory_;
  std::unique_ptr<URLRequestContext> context_;
  raw_ptr<TargetNetworkCheckingHostResolver> host_resolver_;
  raw_ptr<TargetNetworkCheckingProxyResolutionService>
      proxy_resolution_service_;
  TestDelegate delegate_;
};

TEST_F(MultiNetworkTest, HTTP1TargetNetworkIsCorrectlyPropagated) {
  const handles::NetworkHandle kTargetNetwork = 42;
  Init(/*expected_networks_for_sockets=*/{kTargetNetwork},
       /*expected_networks_for_dns=*/{kTargetNetwork},
       /*expected_networks_for_proxies=*/{kTargetNetwork});

  MockWrite writes[] = {
      MockWrite("GET / HTTP/1.1\r\n"
                "Host: www.example.com\r\n"
                "Connection: keep-alive\r\n"
                "User-Agent: \r\n"
                "Accept-Encoding: gzip, deflate\r\n"
                "Accept-Language: en-us,fr\r\n\r\n")};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};

  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_->AddSocketDataProvider(&socket_data);

  std::unique_ptr<URLRequest> request = context_->CreateRequest(
      GURL("http://www.example.com"), DEFAULT_PRIORITY, &delegate_,
      TRAFFIC_ANNOTATION_FOR_TESTS, kTargetNetwork);

  request->Start();
  delegate_.RunUntilComplete();

  EXPECT_THAT(delegate_.request_status(), IsOk());
}

TEST_F(MultiNetworkTest, HTTP1DefaultNetworkIsCorrectlyPropagated) {
  Init(/*expected_networks_for_sockets=*/{handles::kInvalidNetworkHandle},
       /*expected_networks_for_dns=*/{handles::kInvalidNetworkHandle},
       /*expected_networks_for_proxies=*/{handles::kInvalidNetworkHandle});

  MockWrite writes[] = {
      MockWrite("GET / HTTP/1.1\r\n"
                "Host: www.example.com\r\n"
                "Connection: keep-alive\r\n"
                "User-Agent: \r\n"
                "Accept-Encoding: gzip, deflate\r\n"
                "Accept-Language: en-us,fr\r\n\r\n")};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};

  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_->AddSocketDataProvider(&socket_data);

  std::unique_ptr<URLRequest> request = context_->CreateRequest(
      GURL("http://www.example.com"), DEFAULT_PRIORITY, &delegate_,
      TRAFFIC_ANNOTATION_FOR_TESTS, handles::kInvalidNetworkHandle);

  request->Start();
  delegate_.RunUntilComplete();

  EXPECT_THAT(delegate_.request_status(), IsOk());
}

TEST_F(MultiNetworkTest, HTTP1DifferentTargetNetworksNeverShareConnections) {
  const handles::NetworkHandle kNetwork1 = 1;
  const handles::NetworkHandle kNetwork2 = 2;
  Init(/*expected_networks_for_sockets=*/{kNetwork1, kNetwork2},
       /*expected_networks_for_dns=*/{kNetwork1, kNetwork2},
       /*expected_networks_for_proxies=*/{kNetwork1, kNetwork2});

  MockWrite writes1[] = {
      MockWrite("GET / HTTP/1.1\r\n"
                "Host: www.example.com\r\n"
                "Connection: keep-alive\r\n"
                "User-Agent: \r\n"
                "Accept-Encoding: gzip, deflate\r\n"
                "Accept-Language: en-us,fr\r\n\r\n")};
  MockRead reads1[] = {MockRead("HTTP/1.1 200 OK\r\n"
                                "Content-Length: 12\r\n\r\n"),
                       MockRead("Test Content")};
  StaticSocketDataProvider socket_data1(reads1, writes1);
  socket_factory_->AddSocketDataProvider(&socket_data1);

  MockWrite writes2[] = {
      MockWrite("GET / HTTP/1.1\r\n"
                "Host: www.example.com\r\n"
                "Connection: keep-alive\r\n"
                "User-Agent: \r\n"
                "Accept-Encoding: gzip, deflate\r\n"
                "Accept-Language: en-us,fr\r\n\r\n")};
  MockRead reads2[] = {MockRead("HTTP/1.1 200 OK\r\n"
                                "Content-Length: 12\r\n\r\n"),
                       MockRead("Test Content")};
  StaticSocketDataProvider socket_data2(reads2, writes2);
  socket_factory_->AddSocketDataProvider(&socket_data2);

  {
    TestDelegate delegate;
    std::unique_ptr<URLRequest> request = context_->CreateRequest(
        GURL("http://www.example.com"), DEFAULT_PRIORITY, &delegate,
        TRAFFIC_ANNOTATION_FOR_TESTS, kNetwork1);
    request->Start();
    delegate.RunUntilComplete();
    EXPECT_THAT(delegate.request_status(), IsOk());
  }

  {
    TestDelegate delegate;
    std::unique_ptr<URLRequest> request = context_->CreateRequest(
        GURL("http://www.example.com"), DEFAULT_PRIORITY, &delegate,
        TRAFFIC_ANNOTATION_FOR_TESTS, kNetwork2);
    request->Start();
    delegate.RunUntilComplete();
    EXPECT_THAT(delegate.request_status(), IsOk());
  }

  EXPECT_EQ(socket_factory_->mock_data().next_index(), 2u);
}

TEST_F(MultiNetworkTest, HTTP1SameTargetNetworkSharesConnections) {
  const handles::NetworkHandle kNetwork = 1;
  Init(/*expected_networks_for_sockets=*/{kNetwork},
       /*expected_networks_for_dns=*/{kNetwork},
       /*expected_networks_for_proxies=*/{kNetwork, kNetwork});

  MockWrite writes[] = {
      MockWrite(ASYNC, 0,
                "GET / HTTP/1.1\r\n"
                "Host: www.example.com\r\n"
                "Connection: keep-alive\r\n"
                "User-Agent: \r\n"
                "Accept-Encoding: gzip, deflate\r\n"
                "Accept-Language: en-us,fr\r\n\r\n"),
      MockWrite(ASYNC, 3,
                "GET / HTTP/1.1\r\n"
                "Host: www.example.com\r\n"
                "Connection: keep-alive\r\n"
                "User-Agent: \r\n"
                "Accept-Encoding: gzip, deflate\r\n"
                "Accept-Language: en-us,fr\r\n\r\n"),
  };

  MockRead reads[] = {
      MockRead(ASYNC, 1,
               "HTTP/1.1 200 OK\r\n"
               "Content-Length: 12\r\n\r\n"),
      MockRead(ASYNC, 2, "Test Content"),
      MockRead(ASYNC, 4,
               "HTTP/1.1 200 OK\r\n"
               "Content-Length: 12\r\n\r\n"),
      MockRead(ASYNC, 5, "Test Content"),
  };

  SequencedSocketData socket_data(reads, writes);
  socket_factory_->AddSocketDataProvider(&socket_data);

  {
    TestDelegate delegate;
    std::unique_ptr<URLRequest> request = context_->CreateRequest(
        GURL("http://www.example.com"), DEFAULT_PRIORITY, &delegate,
        TRAFFIC_ANNOTATION_FOR_TESTS, kNetwork);
    request->Start();
    delegate.RunUntilComplete();
    EXPECT_THAT(delegate.request_status(), IsOk());
  }

  {
    TestDelegate delegate;
    std::unique_ptr<URLRequest> request = context_->CreateRequest(
        GURL("http://www.example.com"), DEFAULT_PRIORITY, &delegate,
        TRAFFIC_ANNOTATION_FOR_TESTS, kNetwork);
    request->Start();
    delegate.RunUntilComplete();
    EXPECT_THAT(delegate.request_status(), IsOk());
  }

  // Only one socket should be created.
  EXPECT_EQ(socket_factory_->mock_data().next_index(), 1u);
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());

  // DNS should only be resolved once.
  EXPECT_EQ(host_resolver_->num_resolve(), 1u);
}

TEST_F(MultiNetworkTest, HTTP2DifferentTargetNetworksNeverShareConnections) {
  const handles::NetworkHandle kNetwork1 = 1;
  const handles::NetworkHandle kNetwork2 = 2;
  Init(/*expected_networks_for_sockets=*/{kNetwork1, kNetwork2},
       /*expected_networks_for_dns=*/{kNetwork1, kNetwork2},
       /*expected_networks_for_proxies=*/{kNetwork1, kNetwork2}, "test-ua");
  DisableSpdyInitialData();

  SpdyTestUtil spdy_util1(true);
  SpdyTestUtil spdy_util2(true);

  // Construct expected headers manually.
  quiche::HttpHeaderBlock headers1;
  headers1[spdy::kHttp2MethodHeader] = "GET";
  headers1[spdy::kHttp2AuthorityHeader] = "www.example.com";
  headers1[spdy::kHttp2SchemeHeader] = "https";
  headers1[spdy::kHttp2PathHeader] = "/";
  headers1["user-agent"] = "test-ua";
  headers1["accept-encoding"] = "gzip, deflate";
  headers1["accept-language"] = "en-us,fr";

  quiche::HttpHeaderBlock headers2 = headers1.Clone();

  spdy::SpdySerializedFrame req1 = spdy_util1.ConstructSpdyHeaders(
      1, std::move(headers1), DEFAULT_PRIORITY, true);
  spdy::SpdySerializedFrame req2 = spdy_util2.ConstructSpdyHeaders(
      1, std::move(headers2), DEFAULT_PRIORITY, true);

  // Socket 1
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

  // Socket 2
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

  // Start request 1
  TestDelegate delegate1;
  std::unique_ptr<URLRequest> request1 = context_->CreateRequest(
      GURL("https://www.example.com"), DEFAULT_PRIORITY, &delegate1,
      TRAFFIC_ANNOTATION_FOR_TESTS, kNetwork1);
  request1->Start();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return socket_factory_->mock_data().next_index() == 1; }));

  // Start request 2
  TestDelegate delegate2;
  std::unique_ptr<URLRequest> request2 = context_->CreateRequest(
      GURL("https://www.example.com"), DEFAULT_PRIORITY, &delegate2,
      TRAFFIC_ANNOTATION_FOR_TESTS, kNetwork2);
  request2->Start();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return socket_factory_->mock_data().next_index() == 2; }));

  // Both should be pending.
  EXPECT_EQ(delegate1.request_status(), ERR_IO_PENDING);
  EXPECT_EQ(delegate2.request_status(), ERR_IO_PENDING);

  EXPECT_TRUE(socket_data1.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());

  // DNS should be resolved for each request.
  EXPECT_EQ(host_resolver_->num_resolve(), 2u);
}

TEST_F(MultiNetworkTest, HTTP2SameTargetNetworkSharesConnections) {
  const handles::NetworkHandle kNetwork = 1;
  Init(/*expected_networks_for_sockets=*/{kNetwork},
       /*expected_networks_for_dns=*/{kNetwork},
       /*expected_networks_for_proxies=*/{kNetwork, kNetwork}, "test-ua");
  DisableSpdyInitialData();

  SpdyTestUtil spdy_util(true);

  quiche::HttpHeaderBlock headers1;
  headers1[spdy::kHttp2MethodHeader] = "GET";
  headers1[spdy::kHttp2AuthorityHeader] = "www.example.com";
  headers1[spdy::kHttp2SchemeHeader] = "https";
  headers1[spdy::kHttp2PathHeader] = "/";
  headers1["user-agent"] = "test-ua";
  headers1["accept-encoding"] = "gzip, deflate";
  headers1["accept-language"] = "en-us,fr";

  quiche::HttpHeaderBlock headers2 = headers1.Clone();

  spdy::SpdySerializedFrame req1 = spdy_util.ConstructSpdyHeaders(
      1, std::move(headers1), DEFAULT_PRIORITY, true);
  // Second request is on stream 3.
  spdy::SpdySerializedFrame req2 = spdy_util.ConstructSpdyHeaders(
      3, std::move(headers2), DEFAULT_PRIORITY, true);

  MockWrite writes[] = {
      CreateMockWrite(req1, 0),
      CreateMockWrite(req2, 1),
  };

  // We stall on read.
  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 2),
      MockRead(ASYNC, "dummy", 5, 3),  // dummy read to pass validation
  };

  SequencedSocketData socket_data(reads, writes);
  socket_factory_->AddSocketDataProvider(&socket_data);

  // We need SSL socket data because it's HTTPS.
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  ssl_data.next_proto = NextProto::kProtoHTTP2;
  socket_factory_->AddSSLSocketDataProvider(&ssl_data);

  // Start request 1
  TestDelegate delegate1;
  std::unique_ptr<URLRequest> request1 = context_->CreateRequest(
      GURL("https://www.example.com"), DEFAULT_PRIORITY, &delegate1,
      TRAFFIC_ANNOTATION_FOR_TESTS, kNetwork);
  request1->Start();

  // Wait until the session has been created.
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

  // Verify that key constructed by factory is the same.
  HttpRequestInfo http_req2;
  http_req2.method = "GET";
  http_req2.url = GURL("https://www.example.com");
  http_req2.target_network = kNetwork;

  HttpStreamFactory::StreamRequestInfo stream_req2(http_req2);
  SpdySessionKey key2 =
      HttpStreamFactory::GetSpdySessionKey(ProxyChain::Direct(), stream_req2);

  EXPECT_EQ(key, key2);

  // Start request 2
  TestDelegate delegate2;
  std::unique_ptr<URLRequest> request2 = context_->CreateRequest(
      GURL("https://www.example.com"), DEFAULT_PRIORITY, &delegate2,
      TRAFFIC_ANNOTATION_FOR_TESTS, kNetwork);
  request2->Start();

  ASSERT_TRUE(
      base::test::RunUntil([&] { return socket_data.AllWriteDataConsumed(); }));

  // Both requests should be pending.
  EXPECT_EQ(delegate1.request_status(), ERR_IO_PENDING);
  EXPECT_EQ(delegate2.request_status(), ERR_IO_PENDING);

  // Only one socket should be created.
  EXPECT_EQ(socket_factory_->mock_data().next_index(), 1u);

  // DNS should only be resolved once.
  EXPECT_EQ(host_resolver_->num_resolve(), 1u);
}

TEST_F(MultiNetworkTest, QUICAlternativeJobAndTCPFallbackUseTargetNetwork) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kAsyncQuicSession);
  const handles::NetworkHandle kNetwork = 1;
  // We expect:
  // 1. DNS resolve for QUIC job
  // 2. DNS resolve for TCP job
  // 3. UDP socket creation for QUIC job
  // 4. TCP socket creation for TCP job
  Init(/*expected_networks_for_sockets=*/{kNetwork, kNetwork},
       /*expected_networks_for_dns=*/{kNetwork, kNetwork},
       /*expected_networks_for_proxies=*/{kNetwork}, std::nullopt,
       /*enable_quic=*/true);

  // Set Alt-Svc to force QUIC.
  AlternativeServiceInfo alternative_service_info =
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          AlternativeService(NextProto::kProtoQUIC, "www.example.com", 443),
          base::Time::Now() + base::Days(1), DefaultSupportedQuicVersions());
  url::SchemeHostPort server("https", "www.example.com", 443);
  context_->http_server_properties()->SetAlternativeServices(
      server, NetworkAnonymizationKey(), {alternative_service_info});

  // Make UDP connect fail immediately.
  MockConnect udp_connect(ASYNC, ERR_CONNECTION_REFUSED);
  SequencedSocketData udp_data(udp_connect, base::span<const MockRead>(),
                               base::span<const MockWrite>());
  socket_factory_->AddSocketDataProvider(&udp_data);

  // Stall TCP connect.
  MockConnect tcp_connect(ASYNC, ERR_IO_PENDING);
  SequencedSocketData tcp_data(tcp_connect, base::span<const MockRead>(),
                               base::span<const MockWrite>());
  socket_factory_->AddSocketDataProvider(&tcp_data);

  // Start request
  TestDelegate delegate;
  std::unique_ptr<URLRequest> request = context_->CreateRequest(
      GURL("https://www.example.com"), DEFAULT_PRIORITY, &delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS, kNetwork);
  request->Start();

  ASSERT_TRUE(base::test::RunUntil(
      [&] { return socket_factory_->mock_data().next_index() == 2u; }));

  // Request should be pending because TCP job is stalled.
  EXPECT_EQ(delegate.request_status(), ERR_IO_PENDING);

  // DNS should be resolved twice (once for QUIC, once for TCP).
  EXPECT_EQ(host_resolver_->num_resolve(), 2u);
}

TEST_F(MultiNetworkTest, QUICDifferentTargetNetworksNeverShareConnections) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kAsyncQuicSession);
  const handles::NetworkHandle kNetwork1 = 1;
  const handles::NetworkHandle kNetwork2 = 2;

  // We expect:
  // For request 1 (kNetwork1):
  // - DNS resolve (QUIC)
  // - UDP socket (QUIC)
  // For request 2 (kNetwork2):
  // - DNS resolve (QUIC)
  // - UDP socket (QUIC)
  Init(/*expected_networks_for_sockets=*/{kNetwork1, kNetwork2},
       /*expected_networks_for_dns=*/{kNetwork1, kNetwork2},
       /*expected_networks_for_proxies=*/{kNetwork1, kNetwork2}, std::nullopt,
       /*enable_quic=*/true);
  host_resolver_->set_ondemand_mode(true);

  // Set Alt-Svc to force QUIC for both.
  AlternativeServiceInfo alternative_service_info =
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          AlternativeService(NextProto::kProtoQUIC, "www.example.com", 443),
          base::Time::Now() + base::Days(1), DefaultSupportedQuicVersions());
  url::SchemeHostPort server("https", "www.example.com", 443);
  context_->http_server_properties()->SetAlternativeServices(
      server, NetworkAnonymizationKey(), {alternative_service_info});

  // Stall UDP connect for socket 1.
  MockConnect udp_connect1(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData udp_data1(udp_connect1, base::span<const MockRead>(),
                                base::span<const MockWrite>());
  socket_factory_->AddSocketDataProvider(&udp_data1);

  // Stall UDP connect for socket 2.
  MockConnect udp_connect2(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData udp_data2(udp_connect2, base::span<const MockRead>(),
                                base::span<const MockWrite>());
  socket_factory_->AddSocketDataProvider(&udp_data2);

  // Start request 1 on kNetwork1.
  TestDelegate delegate1;
  std::unique_ptr<URLRequest> request1 = context_->CreateRequest(
      GURL("https://www.example.com"), DEFAULT_PRIORITY, &delegate1,
      TRAFFIC_ANNOTATION_FOR_TESTS, kNetwork1);
  request1->Start();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return host_resolver_->num_resolve() == 1u; }));

  // Resolve DNS for request 1.
  host_resolver_->ResolveAllPending();
  // UDP socket 1 should be created.
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return socket_factory_->udp_socket_count() == 1u; }));

  // Start request 2 on kNetwork2.
  TestDelegate delegate2;
  std::unique_ptr<URLRequest> request2 = context_->CreateRequest(
      GURL("https://www.example.com"), DEFAULT_PRIORITY, &delegate2,
      TRAFFIC_ANNOTATION_FOR_TESTS, kNetwork2);
  request2->Start();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return host_resolver_->num_resolve() == 2u; }));

  // Resolve DNS for request 2.
  host_resolver_->ResolveAllPending();
  // UDP socket 2 should be created (no sharing).
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return socket_factory_->udp_socket_count() == 2u; }));

  // No TCP sockets should be created because we didn't fast-forward.
  EXPECT_EQ(socket_factory_->tcp_socket_count(), 0u);
}

TEST_F(MultiNetworkTest, QUICSameTargetNetworkSharesConnections) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kAsyncQuicSession);
  const handles::NetworkHandle kNetwork = 1;

  // We expect:
  // - DNS resolve for request 1 (kNetwork)
  // - UDP socket for request 1 (kNetwork)
  // We pass {kNetwork, kNetwork} because we have 2 requests and thus 2 proxy
  // resolutions will be attempted.
  Init(/*expected_networks_for_sockets=*/{kNetwork},
       /*expected_networks_for_dns=*/{kNetwork},
       /*expected_networks_for_proxies=*/{kNetwork, kNetwork}, std::nullopt,
       /*enable_quic=*/true);
  host_resolver_->set_ondemand_mode(true);

  // Set Alt-Svc to force QUIC.
  AlternativeServiceInfo alternative_service_info =
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          AlternativeService(NextProto::kProtoQUIC, "www.example.com", 443),
          base::Time::Now() + base::Days(1), DefaultSupportedQuicVersions());
  url::SchemeHostPort server("https", "www.example.com", 443);
  context_->http_server_properties()->SetAlternativeServices(
      server, NetworkAnonymizationKey(), {alternative_service_info});

  // Stall UDP connect.
  MockConnect udp_connect(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData udp_data(udp_connect, base::span<const MockRead>(),
                               base::span<const MockWrite>());
  socket_factory_->AddSocketDataProvider(&udp_data);

  // Start request 1.
  TestDelegate delegate1;
  std::unique_ptr<URLRequest> request1 = context_->CreateRequest(
      GURL("https://www.example.com"), DEFAULT_PRIORITY, &delegate1,
      TRAFFIC_ANNOTATION_FOR_TESTS, kNetwork);
  request1->Start();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return host_resolver_->num_resolve() == 1u; }));

  // Resolve DNS for request 1.
  host_resolver_->ResolveAllPending();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return socket_factory_->udp_socket_count() == 1u; }));

  // UDP socket should be created.

  // Start request 2 on same network.
  TestDelegate delegate2;
  std::unique_ptr<URLRequest> request2 = context_->CreateRequest(
      GURL("https://www.example.com"), DEFAULT_PRIORITY, &delegate2,
      TRAFFIC_ANNOTATION_FOR_TESTS, kNetwork);
  request2->Start();
  ASSERT_TRUE(base::test::RunUntil([&] {
    return proxy_resolution_service_->current_proxy_request() == 2u;
  }));

  // It should coalesce to the pending job, so no new DNS resolve.
  EXPECT_EQ(host_resolver_->num_resolve(), 1u);

  // No new socket should be created.
  EXPECT_EQ(socket_factory_->udp_socket_count(), 1u);
}

}  // namespace net
