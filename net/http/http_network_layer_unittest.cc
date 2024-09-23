// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_layer.h"

#include <memory>
#include <utility>

#include "base/strings/stringprintf.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_transaction_test_util.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/quic/quic_context.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

class HttpNetworkLayerTest : public PlatformTest, public WithTaskEnvironment {
 protected:
  HttpNetworkLayerTest()
      : ssl_config_service_(std::make_unique<SSLConfigServiceDefaults>()) {}

  void SetUp() override {
    ConfigureTestDependencies(ConfiguredProxyResolutionService::CreateDirect());
  }

  void ConfigureTestDependencies(
      std::unique_ptr<ConfiguredProxyResolutionService>
          proxy_resolution_service) {
    cert_verifier_ = std::make_unique<MockCertVerifier>();
    transport_security_state_ = std::make_unique<TransportSecurityState>();
    proxy_resolution_service_ = std::move(proxy_resolution_service);
    HttpNetworkSessionContext session_context;
    session_context.client_socket_factory = &mock_socket_factory_;
    session_context.host_resolver = &host_resolver_;
    session_context.cert_verifier = cert_verifier_.get();
    session_context.transport_security_state = transport_security_state_.get();
    session_context.proxy_resolution_service = proxy_resolution_service_.get();
    session_context.ssl_config_service = ssl_config_service_.get();
    session_context.http_server_properties = &http_server_properties_;
    session_context.quic_context = &quic_context_;
    session_context.http_user_agent_settings = &http_user_agent_settings_;
    network_session_ = std::make_unique<HttpNetworkSession>(
        HttpNetworkSessionParams(), session_context);
    factory_ = std::make_unique<HttpNetworkLayer>(network_session_.get());
  }

  MockClientSocketFactory mock_socket_factory_;
  MockHostResolver host_resolver_{
      /*default_result=*/
      MockHostResolverBase::RuleResolver::GetLocalhostResult()};
  std::unique_ptr<CertVerifier> cert_verifier_;
  std::unique_ptr<TransportSecurityState> transport_security_state_;
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service_;
  StaticHttpUserAgentSettings http_user_agent_settings_ = {"*", "test-ua"};
  std::unique_ptr<SSLConfigService> ssl_config_service_;
  QuicContext quic_context_;
  std::unique_ptr<HttpNetworkSession> network_session_;
  std::unique_ptr<HttpNetworkLayer> factory_;

 private:
  HttpServerProperties http_server_properties_;
};

TEST_F(HttpNetworkLayerTest, CreateAndDestroy) {
  std::unique_ptr<HttpTransaction> trans;
  int rv = factory_->CreateTransaction(DEFAULT_PRIORITY, &trans);
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(trans.get() != nullptr);
}

TEST_F(HttpNetworkLayerTest, Suspend) {
  std::unique_ptr<HttpTransaction> trans;
  int rv = factory_->CreateTransaction(DEFAULT_PRIORITY, &trans);
  EXPECT_THAT(rv, IsOk());

  trans.reset();

  factory_->OnSuspend();

  rv = factory_->CreateTransaction(DEFAULT_PRIORITY, &trans);
  EXPECT_THAT(rv, IsError(ERR_NETWORK_IO_SUSPENDED));

  ASSERT_TRUE(trans == nullptr);

  factory_->OnResume();

  rv = factory_->CreateTransaction(DEFAULT_PRIORITY, &trans);
  EXPECT_THAT(rv, IsOk());
}

TEST_F(HttpNetworkLayerTest, GET) {
  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, OK),
  };
  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "User-Agent: Foo/1.0\r\n\r\n"),
  };
  StaticSocketDataProvider data(data_reads, data_writes);
  mock_socket_factory_.AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  HttpRequestInfo request_info;
  request_info.url = GURL("http://www.google.com/");
  request_info.method = "GET";
  request_info.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent,
                                       "Foo/1.0");
  request_info.load_flags = LOAD_NORMAL;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  std::unique_ptr<HttpTransaction> trans;
  int rv = factory_->CreateTransaction(DEFAULT_PRIORITY, &trans);
  EXPECT_THAT(rv, IsOk());

  rv = trans->Start(&request_info, callback.callback(), NetLogWithSource());
  rv = callback.GetResult(rv);
  ASSERT_THAT(rv, IsOk());

  std::string contents;
  rv = ReadTransaction(trans.get(), &contents);
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ("hello world", contents);
}

TEST_F(HttpNetworkLayerTest, NetworkVerified) {
  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, OK),
  };
  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "User-Agent: Foo/1.0\r\n\r\n"),
  };
  StaticSocketDataProvider data(data_reads, data_writes);
  mock_socket_factory_.AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  HttpRequestInfo request_info;
  request_info.url = GURL("http://www.google.com/");
  request_info.method = "GET";
  request_info.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent,
                                       "Foo/1.0");
  request_info.load_flags = LOAD_NORMAL;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  std::unique_ptr<HttpTransaction> trans;
  int rv = factory_->CreateTransaction(DEFAULT_PRIORITY, &trans);
  EXPECT_THAT(rv, IsOk());

  rv = trans->Start(&request_info, callback.callback(), NetLogWithSource());
  ASSERT_THAT(callback.GetResult(rv), IsOk());

  EXPECT_TRUE(trans->GetResponseInfo()->network_accessed);
}

TEST_F(HttpNetworkLayerTest, NetworkUnVerified) {
  MockRead data_reads[] = {
    MockRead(ASYNC, ERR_CONNECTION_RESET),
  };
  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "User-Agent: Foo/1.0\r\n\r\n"),
  };
  StaticSocketDataProvider data(data_reads, data_writes);
  mock_socket_factory_.AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  HttpRequestInfo request_info;
  request_info.url = GURL("http://www.google.com/");
  request_info.method = "GET";
  request_info.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent,
                                       "Foo/1.0");
  request_info.load_flags = LOAD_NORMAL;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  std::unique_ptr<HttpTransaction> trans;
  int rv = factory_->CreateTransaction(DEFAULT_PRIORITY, &trans);
  EXPECT_THAT(rv, IsOk());

  rv = trans->Start(&request_info, callback.callback(), NetLogWithSource());
  ASSERT_THAT(callback.GetResult(rv), IsError(ERR_CONNECTION_RESET));

  // network_accessed is true; the HTTP stack did try to make a connection.
  EXPECT_TRUE(trans->GetResponseInfo()->network_accessed);
}

}  // namespace

}  // namespace net
