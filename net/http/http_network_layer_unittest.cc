// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_layer.h"

#include <utility>

#include "base/strings/stringprintf.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_transaction_test_util.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/quic/quic_context.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
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
    ConfigureTestDependencies(ProxyResolutionService::CreateDirect());
  }

  void ConfigureTestDependencies(
      std::unique_ptr<ProxyResolutionService> proxy_resolution_service) {
    cert_verifier_.reset(new MockCertVerifier);
    transport_security_state_.reset(new TransportSecurityState);
    proxy_resolution_service_ = std::move(proxy_resolution_service);
    HttpNetworkSession::Context session_context;
    session_context.client_socket_factory = &mock_socket_factory_;
    session_context.host_resolver = &host_resolver_;
    session_context.cert_verifier = cert_verifier_.get();
    session_context.transport_security_state = transport_security_state_.get();
    session_context.cert_transparency_verifier = &ct_verifier_;
    session_context.ct_policy_enforcer = &ct_policy_enforcer_;
    session_context.proxy_resolution_service = proxy_resolution_service_.get();
    session_context.ssl_config_service = ssl_config_service_.get();
    session_context.http_server_properties = &http_server_properties_;
    session_context.quic_context = &quic_context_;
    network_session_.reset(
        new HttpNetworkSession(HttpNetworkSession::Params(), session_context));
    factory_.reset(new HttpNetworkLayer(network_session_.get()));
  }

  void ExecuteRequestExpectingContentAndHeader(const std::string& method,
                                               const std::string& content,
                                               const std::string& header,
                                               const std::string& value) {
    TestCompletionCallback callback;

    HttpRequestInfo request_info;
    request_info.url = GURL("http://www.google.com/");
    request_info.method = method;
    request_info.load_flags = LOAD_NORMAL;
    request_info.traffic_annotation =
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

    std::unique_ptr<HttpTransaction> trans;
    int rv = factory_->CreateTransaction(DEFAULT_PRIORITY, &trans);
    EXPECT_THAT(rv, IsOk());

    rv = trans->Start(&request_info, callback.callback(), NetLogWithSource());
    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();
    ASSERT_THAT(rv, IsOk());

    std::string contents;
    rv = ReadTransaction(trans.get(), &contents);
    EXPECT_THAT(rv, IsOk());
    EXPECT_EQ(content, contents);

    if (!header.empty()) {
      // We also have a server header here that isn't set by the proxy.
      EXPECT_TRUE(trans->GetResponseInfo()->headers->HasHeaderValue(
          header, value));
    }
  }

  // Check that |proxy_count| proxies are in the retry list.
  // These will be, in order, |bad_proxy| and |bad_proxy2|".
  void TestBadProxies(unsigned int proxy_count, const std::string& bad_proxy,
                      const std::string& bad_proxy2) {
    const ProxyRetryInfoMap& retry_info =
        proxy_resolution_service_->proxy_retry_info();
    ASSERT_EQ(proxy_count, retry_info.size());
    if (proxy_count > 0)
      ASSERT_TRUE(retry_info.find(bad_proxy) != retry_info.end());
    if (proxy_count > 1)
      ASSERT_TRUE(retry_info.find(bad_proxy2) != retry_info.end());
  }

  // Simulates a request through a proxy which returns a bypass, which is then
  // retried through a second proxy that doesn't bypass.
  // Checks that the expected requests were issued, the expected content was
  // received, and the first proxy |bad_proxy| was marked as bad.
  void TestProxyFallback(const std::string& bad_proxy) {
    MockRead data_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"
               "Chrome-Proxy: bypass=0\r\n\r\n"),
      MockRead("Bypass message"),
      MockRead(SYNCHRONOUS, OK),
    };
    TestProxyFallbackWithMockReads(bad_proxy, "", data_reads, 1u);
  }

  void TestProxyFallbackWithMockReads(const std::string& bad_proxy,
                                      const std::string& bad_proxy2,
                                      base::span<const MockRead> data_reads,
                                      unsigned int expected_retry_info_size) {
    TestProxyFallbackByMethodWithMockReads(bad_proxy, bad_proxy2, data_reads,
                                           "GET", "content", true,
                                           expected_retry_info_size);
  }

  void TestProxyFallbackByMethodWithMockReads(
      const std::string& bad_proxy,
      const std::string& bad_proxy2,
      base::span<const MockRead> data_reads,
      std::string method,
      std::string content,
      bool retry_expected,
      unsigned int expected_retry_info_size) {
    std::string trailer =
        (method == "HEAD" || method == "PUT" || method == "POST") ?
        "Content-Length: 0\r\n\r\n" : "\r\n";
    std::string request =
        base::StringPrintf("%s http://www.google.com/ HTTP/1.1\r\n"
                           "Host: www.google.com\r\n"
                           "Proxy-Connection: keep-alive\r\n"
                           "%s", method.c_str(), trailer.c_str());

    MockWrite data_writes[] = {
      MockWrite(request.c_str()),
    };

    StaticSocketDataProvider data1(data_reads, data_writes);
    mock_socket_factory_.AddSocketDataProvider(&data1);

    // Second data provider returns the expected content.
    MockRead data_reads2[3];
    size_t data_reads2_index = 0;
    data_reads2[data_reads2_index++] = MockRead("HTTP/1.0 200 OK\r\n"
                                                "Server: not-proxy\r\n\r\n");
    if (!content.empty())
      data_reads2[data_reads2_index++] = MockRead(content.c_str());
    data_reads2[data_reads2_index++] = MockRead(SYNCHRONOUS, OK);

    MockWrite data_writes2[] = {
      MockWrite(request.c_str()),
    };
    StaticSocketDataProvider data2(
        base::make_span(data_reads2, data_reads2_index), data_writes2);
    mock_socket_factory_.AddSocketDataProvider(&data2);

    // Expect that we get "content" and not "Bypass message", and that there's
    // a "not-proxy" "Server:" header in the final response.
    if (retry_expected) {
      ExecuteRequestExpectingContentAndHeader(method, content,
                                              "server", "not-proxy");
    } else {
      ExecuteRequestExpectingContentAndHeader(method, content, "", "");
    }

    // We should also observe the bad proxy in the retry list.
    TestBadProxies(expected_retry_info_size, bad_proxy, bad_proxy2);
  }

  // Simulates a request through a proxy which returns a bypass, which is then
  // retried through a direct connection to the origin site.
  // Checks that the expected requests were issued, the expected content was
  // received, and the proxy |bad_proxy| was marked as bad.
  void TestProxyFallbackToDirect(const std::string& bad_proxy) {
    MockRead data_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"
               "Chrome-Proxy: bypass=0\r\n\r\n"),
      MockRead("Bypass message"),
      MockRead(SYNCHRONOUS, OK),
    };
    MockWrite data_writes[] = {
      MockWrite("GET http://www.google.com/ HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Proxy-Connection: keep-alive\r\n\r\n"),
    };
    StaticSocketDataProvider data1(data_reads, data_writes);
    mock_socket_factory_.AddSocketDataProvider(&data1);

    // Second data provider returns the expected content.
    MockRead data_reads2[] = {
      MockRead("HTTP/1.0 200 OK\r\n"
               "Server: not-proxy\r\n\r\n"),
      MockRead("content"),
      MockRead(SYNCHRONOUS, OK),
    };
    MockWrite data_writes2[] = {
      MockWrite("GET / HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Connection: keep-alive\r\n\r\n"),
    };
    StaticSocketDataProvider data2(data_reads2, data_writes2);
    mock_socket_factory_.AddSocketDataProvider(&data2);

    // Expect that we get "content" and not "Bypass message", and that there's
    // a "not-proxy" "Server:" header in the final response.
    ExecuteRequestExpectingContentAndHeader("GET", "content",
                                            "server", "not-proxy");

    // We should also observe the bad proxy in the retry list.
    TestBadProxies(1u, bad_proxy, "");
  }

  // Simulates a request through a proxy which returns a bypass, under a
  // configuration where there is no valid bypass. |proxy_count| proxies
  // are expected to be configured.
  // Checks that the expected requests were issued, the bypass message was the
  // final received content,  and all proxies were marked as bad.
  void TestProxyFallbackFail(unsigned int proxy_count,
                             const std::string& bad_proxy,
                             const std::string& bad_proxy2) {
    MockRead data_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"
               "Chrome-Proxy: bypass=0\r\n\r\n"),
      MockRead("Bypass message"),
      MockRead(SYNCHRONOUS, OK),
    };
    MockWrite data_writes[] = {
      MockWrite("GET http://www.google.com/ HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Proxy-Connection: keep-alive\r\n\r\n"),
    };
    StaticSocketDataProvider data1(data_reads, data_writes);
    StaticSocketDataProvider data2(data_reads, data_writes);

    mock_socket_factory_.AddSocketDataProvider(&data1);
    if (proxy_count > 1)
      mock_socket_factory_.AddSocketDataProvider(&data2);

    // Expect that we get "Bypass message", and not "content"..
    ExecuteRequestExpectingContentAndHeader("GET", "Bypass message", "", "");

    // We should also observe the bad proxy or proxies in the retry list.
    TestBadProxies(proxy_count, bad_proxy, bad_proxy2);
  }

  MockClientSocketFactory mock_socket_factory_;
  MockHostResolver host_resolver_;
  std::unique_ptr<CertVerifier> cert_verifier_;
  std::unique_ptr<TransportSecurityState> transport_security_state_;
  MultiLogCTVerifier ct_verifier_;
  DefaultCTPolicyEnforcer ct_policy_enforcer_;
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service_;
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
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

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
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

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
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

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
