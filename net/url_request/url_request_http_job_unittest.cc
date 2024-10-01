// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_http_job.h"

#include <stdint.h>

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "net/base/auth.h"
#include "net/base/features.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/base/request_priority.h"
#include "net/base/test_proxy_delegate.h"
#include "net/cert/ct_policy_status.h"
#include "net/cookies/canonical_cookie_test_helpers.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store_test_callbacks.h"
#include "net/cookies/cookie_store_test_helpers.h"
#include "net/cookies/test_cookie_access_delegate.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_transaction_test_util.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log_event_type.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/net_buildflags.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_test_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "net/url_request/websocket_handshake_userdata_key.h"
#include "net/websockets/websocket_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "net/android/net_test_support_jni/AndroidNetworkLibraryTestUtil_jni.h"
#endif

#if BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)
#include "net/device_bound_sessions/session_service.h"
#include "net/device_bound_sessions/test_util.h"
#endif

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

using ::testing::_;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

const char kSimpleGetMockWrite[] =
    "GET / HTTP/1.1\r\n"
    "Host: www.example.com\r\n"
    "Connection: keep-alive\r\n"
    "User-Agent: \r\n"
    "Accept-Encoding: gzip, deflate\r\n"
    "Accept-Language: en-us,fr\r\n\r\n";

const char kSimpleHeadMockWrite[] =
    "HEAD / HTTP/1.1\r\n"
    "Host: www.example.com\r\n"
    "Connection: keep-alive\r\n"
    "User-Agent: \r\n"
    "Accept-Encoding: gzip, deflate\r\n"
    "Accept-Language: en-us,fr\r\n\r\n";

const char kTrustAnchorRequestHistogram[] =
    "Net.Certificate.TrustAnchor.Request";

// Inherit from URLRequestHttpJob to expose the priority and some
// other hidden functions.
class TestURLRequestHttpJob : public URLRequestHttpJob {
 public:
  explicit TestURLRequestHttpJob(URLRequest* request)
      : URLRequestHttpJob(request,
                          request->context()->http_user_agent_settings()) {}

  TestURLRequestHttpJob(const TestURLRequestHttpJob&) = delete;
  TestURLRequestHttpJob& operator=(const TestURLRequestHttpJob&) = delete;

  ~TestURLRequestHttpJob() override = default;

  // URLRequestJob implementation:
  std::unique_ptr<SourceStream> SetUpSourceStream() override {
    if (use_null_source_stream_)
      return nullptr;
    return URLRequestHttpJob::SetUpSourceStream();
  }

  void set_use_null_source_stream(bool use_null_source_stream) {
    use_null_source_stream_ = use_null_source_stream;
  }

  using URLRequestHttpJob::SetPriority;
  using URLRequestHttpJob::Start;
  using URLRequestHttpJob::Kill;
  using URLRequestHttpJob::priority;

 private:
  bool use_null_source_stream_ = false;
};

class URLRequestHttpJobSetUpSourceTest : public TestWithTaskEnvironment {
 public:
  URLRequestHttpJobSetUpSourceTest() {
    auto context_builder = CreateTestURLRequestContextBuilder();
    context_builder->set_client_socket_factory_for_testing(&socket_factory_);
    context_ = context_builder->Build();
  }

 protected:
  MockClientSocketFactory socket_factory_;

  std::unique_ptr<URLRequestContext> context_;
  TestDelegate delegate_;
};

// Tests that if SetUpSourceStream() returns nullptr, the request fails.
TEST_F(URLRequestHttpJobSetUpSourceTest, SetUpSourceFails) {
  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};

  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);

  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
  auto job = std::make_unique<TestURLRequestHttpJob>(request.get());
  job->set_use_null_source_stream(true);
  TestScopedURLInterceptor interceptor(request->url(), std::move(job));
  request->Start();

  delegate_.RunUntilComplete();
  EXPECT_EQ(ERR_CONTENT_DECODING_INIT_FAILED, delegate_.request_status());
}

// Tests that if there is an unknown content-encoding type, the raw response
// body is passed through.
TEST_F(URLRequestHttpJobSetUpSourceTest, UnknownEncoding) {
  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Encoding: foo, gzip\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};

  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);

  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
  auto job = std::make_unique<TestURLRequestHttpJob>(request.get());
  TestScopedURLInterceptor interceptor(request->url(), std::move(job));
  request->Start();

  delegate_.RunUntilComplete();
  EXPECT_EQ(OK, delegate_.request_status());
  EXPECT_EQ("Test Content", delegate_.data_received());
}

// TaskEnvironment is required to instantiate a
// net::ConfiguredProxyResolutionService, which registers itself as an IP
// Address Observer with the NetworkChangeNotifier.
using URLRequestHttpJobWithProxyTest = TestWithTaskEnvironment;

class URLRequestHttpJobWithProxy {
 public:
  explicit URLRequestHttpJobWithProxy(
      std::unique_ptr<ProxyResolutionService> proxy_resolution_service) {
    auto context_builder = CreateTestURLRequestContextBuilder();
    context_builder->set_client_socket_factory_for_testing(&socket_factory_);
    if (proxy_resolution_service) {
      context_builder->set_proxy_resolution_service(
          std::move(proxy_resolution_service));
    }
    context_ = context_builder->Build();
  }

  URLRequestHttpJobWithProxy(const URLRequestHttpJobWithProxy&) = delete;
  URLRequestHttpJobWithProxy& operator=(const URLRequestHttpJobWithProxy&) =
      delete;

  MockClientSocketFactory socket_factory_;
  std::unique_ptr<URLRequestContext> context_;
};

// Tests that when a proxy is not used, the proxy chain is set correctly on the
// URLRequest.
TEST_F(URLRequestHttpJobWithProxyTest, TestFailureWithoutProxy) {
  URLRequestHttpJobWithProxy http_job_with_proxy(nullptr);

  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_CONNECTION_RESET)};

  StaticSocketDataProvider socket_data(reads, writes);
  http_job_with_proxy.socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      http_job_with_proxy.context_->CreateRequest(
          GURL("http://www.example.com"), DEFAULT_PRIORITY, &delegate,
          TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.request_status(), IsError(ERR_CONNECTION_RESET));
  EXPECT_EQ(ProxyChain::Direct(), request->proxy_chain());
  EXPECT_EQ(0, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes), request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), request->GetTotalReceivedBytes());
}

// Tests that when one proxy chain is in use and the connection to a proxy
// server in the proxy chain fails, the proxy chain is still set correctly on
// the URLRequest.
TEST_F(URLRequestHttpJobWithProxyTest, TestSuccessfulWithOneProxy) {
  const char kSimpleProxyGetMockWrite[] =
      "GET http://www.example.com/ HTTP/1.1\r\n"
      "Host: www.example.com\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "User-Agent: \r\n"
      "Accept-Encoding: gzip, deflate\r\n"
      "Accept-Language: en-us,fr\r\n\r\n";

  const ProxyChain proxy_chain =
      ProxyUriToProxyChain("http://origin.net:80", ProxyServer::SCHEME_HTTP);

  std::unique_ptr<ProxyResolutionService> proxy_resolution_service =
      ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
          ProxyServerToPacResultElement(proxy_chain.First()),
          TRAFFIC_ANNOTATION_FOR_TESTS);

  MockWrite writes[] = {MockWrite(kSimpleProxyGetMockWrite)};
  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_CONNECTION_RESET)};

  StaticSocketDataProvider socket_data(reads, writes);

  URLRequestHttpJobWithProxy http_job_with_proxy(
      std::move(proxy_resolution_service));
  http_job_with_proxy.socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      http_job_with_proxy.context_->CreateRequest(
          GURL("http://www.example.com"), DEFAULT_PRIORITY, &delegate,
          TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.request_status(), IsError(ERR_CONNECTION_RESET));
  // When request fails due to proxy connection errors, the proxy chain should
  // still be set on the `request`.
  EXPECT_EQ(proxy_chain, request->proxy_chain());
  EXPECT_EQ(0, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes), request->GetTotalSentBytes());
  EXPECT_EQ(0, request->GetTotalReceivedBytes());
}

// Tests that when two proxy chains are in use and the connection to a proxy
// server in the first proxy chain fails, the proxy chain is set correctly on
// the URLRequest.
TEST_F(URLRequestHttpJobWithProxyTest,
       TestContentLengthSuccessfulRequestWithTwoProxies) {
  const ProxyChain proxy_chain =
      ProxyUriToProxyChain("http://origin.net:80", ProxyServer::SCHEME_HTTP);

  // Connection to `proxy_chain` would fail. Request should be fetched over
  // DIRECT.
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service =
      ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
          ProxyServerToPacResultElement(proxy_chain.First()) + "; DIRECT",
          TRAFFIC_ANNOTATION_FOR_TESTS);

  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content"), MockRead(ASYNC, OK)};

  MockConnect mock_connect_1(SYNCHRONOUS, ERR_CONNECTION_RESET);
  StaticSocketDataProvider connect_data_1;
  connect_data_1.set_connect_data(mock_connect_1);

  StaticSocketDataProvider socket_data(reads, writes);

  URLRequestHttpJobWithProxy http_job_with_proxy(
      std::move(proxy_resolution_service));
  http_job_with_proxy.socket_factory_.AddSocketDataProvider(&connect_data_1);
  http_job_with_proxy.socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      http_job_with_proxy.context_->CreateRequest(
          GURL("http://www.example.com"), DEFAULT_PRIORITY, &delegate,
          TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(ProxyChain::Direct(), request->proxy_chain());
  EXPECT_EQ(12, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes), request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), request->GetTotalReceivedBytes());
}

// Test that the IP Protection-specific metrics get recorded as expected when
// the direct-only param is enabled.
TEST_F(URLRequestHttpJobWithProxyTest,
       IpProtectionDirectOnlyProxyMetricsRecorded) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kEnableIpProtectionProxy,
      {{net::features::kIpPrivacyDirectOnly.name, "true"}});
  const auto kIpProtectionDirectChain =
      ProxyChain::ForIpProtection(std::vector<ProxyServer>());

  std::unique_ptr<ProxyResolutionService> proxy_resolution_service =
      ConfiguredProxyResolutionService::CreateFixedForTest(
          "https://not-used:70", TRAFFIC_ANNOTATION_FOR_TESTS);
  auto proxy_delegate = std::make_unique<TestProxyDelegate>();
  proxy_delegate->set_proxy_chain(kIpProtectionDirectChain);
  proxy_resolution_service->SetProxyDelegate(proxy_delegate.get());

  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};

  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};

  StaticSocketDataProvider socket_data(reads, writes);

  URLRequestHttpJobWithProxy http_job_with_proxy(
      std::move(proxy_resolution_service));
  http_job_with_proxy.socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  base::HistogramTester histogram_tester;
  std::unique_ptr<URLRequest> request =
      http_job_with_proxy.context_->CreateRequest(
          GURL("http://www.example.com"), DEFAULT_PRIORITY, &delegate,
          TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(kIpProtectionDirectChain, request->proxy_chain());
  EXPECT_EQ(12, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes), request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), request->GetTotalReceivedBytes());

  histogram_tester.ExpectUniqueSample("Net.HttpJob.IpProtection.BytesSent",
                                      std::size(kSimpleGetMockWrite),
                                      /*expected_bucket_count=*/1);

  histogram_tester.ExpectUniqueSample(
      "Net.HttpJob.IpProtection.PrefilterBytesRead.Net",
      /*sample=*/12, /*expected_bucket_count=*/1);

  histogram_tester.ExpectUniqueSample(
      "Net.HttpJob.IpProtection.JobResult",
      /*sample=*/URLRequestHttpJob::IpProtectionJobResult::kProtectionSuccess,
      /*expected_bucket_count=*/1);
}

// Test that IP Protection-specific metrics are NOT recorded for direct requests
// when the direct-only param is disabled.
TEST_F(URLRequestHttpJobWithProxyTest, IpProtectionDirectProxyMetricsRecorded) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kEnableIpProtectionProxy,
      {{net::features::kIpPrivacyDirectOnly.name, "false"}});
  const auto kIpProtectionDirectChain =
      ProxyChain::ForIpProtection(std::vector<ProxyServer>());

  std::unique_ptr<ProxyResolutionService> proxy_resolution_service =
      ConfiguredProxyResolutionService::CreateFixedForTest(
          "https://not-used:70", TRAFFIC_ANNOTATION_FOR_TESTS);
  auto proxy_delegate = std::make_unique<TestProxyDelegate>();
  proxy_delegate->set_proxy_chain(kIpProtectionDirectChain);
  proxy_resolution_service->SetProxyDelegate(proxy_delegate.get());

  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};

  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};

  StaticSocketDataProvider socket_data(reads, writes);

  URLRequestHttpJobWithProxy http_job_with_proxy(
      std::move(proxy_resolution_service));
  http_job_with_proxy.socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  base::HistogramTester histogram_tester;
  std::unique_ptr<URLRequest> request =
      http_job_with_proxy.context_->CreateRequest(
          GURL("http://www.example.com"), DEFAULT_PRIORITY, &delegate,
          TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(kIpProtectionDirectChain, request->proxy_chain());
  EXPECT_EQ(12, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes), request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), request->GetTotalReceivedBytes());

  histogram_tester.ExpectTotalCount("Net.HttpJob.IpProtection.BytesSent", 0);
  histogram_tester.ExpectTotalCount(
      "Net.HttpJob.IpProtection.PrefilterBytesRead.Net", 0);
}

class URLRequestHttpJobTest : public TestWithTaskEnvironment {
 protected:
  URLRequestHttpJobTest() {
    auto context_builder = CreateTestURLRequestContextBuilder();
    context_builder->SetHttpTransactionFactoryForTesting(
        std::make_unique<MockNetworkLayer>());
    context_builder->DisableHttpCache();
    context_builder->set_net_log(NetLog::Get());
    context_ = context_builder->Build();

    req_ = context_->CreateRequest(GURL("http://www.example.com"),
                                   DEFAULT_PRIORITY, &delegate_,
                                   TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  MockNetworkLayer& network_layer() {
    // This cast is safe because we set a MockNetworkLayer in the constructor.
    return *static_cast<MockNetworkLayer*>(
        context_->http_transaction_factory());
  }

  std::unique_ptr<URLRequest> CreateFirstPartyRequest(
      const URLRequestContext& context,
      const GURL& url,
      URLRequest::Delegate* delegate) {
    auto req = context.CreateRequest(url, DEFAULT_PRIORITY, delegate,
                                     TRAFFIC_ANNOTATION_FOR_TESTS);
    req->set_initiator(url::Origin::Create(url));
    req->set_site_for_cookies(SiteForCookies::FromUrl(url));
    return req;
  }

  std::unique_ptr<URLRequestContext> context_;
  TestDelegate delegate_;
  RecordingNetLogObserver net_log_observer_;
  std::unique_ptr<URLRequest> req_;
};

class URLRequestHttpJobWithMockSocketsTest : public TestWithTaskEnvironment {
 protected:
  URLRequestHttpJobWithMockSocketsTest() {
    auto context_builder = CreateTestURLRequestContextBuilder();
    context_builder->set_client_socket_factory_for_testing(&socket_factory_);
    context_ = context_builder->Build();
  }

  MockClientSocketFactory socket_factory_;
  std::unique_ptr<URLRequestContext> context_;
};

TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestContentLengthSuccessfulRequest) {
  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};

  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(12, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes), request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), request->GetTotalReceivedBytes());
}

// Tests a successful HEAD request.
TEST_F(URLRequestHttpJobWithMockSocketsTest, TestSuccessfulHead) {
  MockWrite writes[] = {MockWrite(kSimpleHeadMockWrite)};
  MockRead reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"
               "Content-Length: 0\r\n\r\n")};

  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  request->set_method("HEAD");
  request->Start();
  ASSERT_TRUE(request->is_pending());
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(0, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes), request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), request->GetTotalReceivedBytes());
}

// Similar to above test but tests that even if response body is there in the
// HEAD response stream, it should not be read due to HttpStreamParser's logic.
TEST_F(URLRequestHttpJobWithMockSocketsTest, TestSuccessfulHeadWithContent) {
  MockWrite writes[] = {MockWrite(kSimpleHeadMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};

  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  request->set_method("HEAD");
  request->Start();
  ASSERT_TRUE(request->is_pending());
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(0, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes), request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads) - 12, request->GetTotalReceivedBytes());
}

TEST_F(URLRequestHttpJobWithMockSocketsTest, TestSuccessfulCachedHeadRequest) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example.com"));
  const IsolationInfo kTestIsolationInfo =
      IsolationInfo::CreateForInternalRequest(kOrigin1);

  // Cache the response.
  {
    MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
    MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                                 "Content-Length: 12\r\n\r\n"),
                        MockRead("Test Content")};

    StaticSocketDataProvider socket_data(reads, writes);
    socket_factory_.AddSocketDataProvider(&socket_data);

    TestDelegate delegate;
    std::unique_ptr<URLRequest> request = context_->CreateRequest(
        GURL("http://www.example.com"), DEFAULT_PRIORITY, &delegate,
        TRAFFIC_ANNOTATION_FOR_TESTS);

    request->set_isolation_info(kTestIsolationInfo);
    request->Start();
    ASSERT_TRUE(request->is_pending());
    delegate.RunUntilComplete();

    EXPECT_THAT(delegate.request_status(), IsOk());
    EXPECT_EQ(12, request->received_response_content_length());
    EXPECT_EQ(CountWriteBytes(writes), request->GetTotalSentBytes());
    EXPECT_EQ(CountReadBytes(reads), request->GetTotalReceivedBytes());
  }

  // Send a HEAD request for the cached response.
  {
    MockWrite writes[] = {MockWrite(kSimpleHeadMockWrite)};
    MockRead reads[] = {
        MockRead("HTTP/1.1 200 OK\r\n"
                 "Content-Length: 0\r\n\r\n")};

    StaticSocketDataProvider socket_data(reads, writes);
    socket_factory_.AddSocketDataProvider(&socket_data);

    TestDelegate delegate;
    std::unique_ptr<URLRequest> request = context_->CreateRequest(
        GURL("http://www.example.com"), DEFAULT_PRIORITY, &delegate,
        TRAFFIC_ANNOTATION_FOR_TESTS);

    // Use the cached version.
    request->SetLoadFlags(LOAD_SKIP_CACHE_VALIDATION);
    request->set_method("HEAD");
    request->set_isolation_info(kTestIsolationInfo);
    request->Start();
    ASSERT_TRUE(request->is_pending());
    delegate.RunUntilComplete();

    EXPECT_THAT(delegate.request_status(), IsOk());
    EXPECT_EQ(0, request->received_response_content_length());
    EXPECT_EQ(0, request->GetTotalSentBytes());
    EXPECT_EQ(0, request->GetTotalReceivedBytes());
  }
}

TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestContentLengthSuccessfulHttp09Request) {
  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("Test Content"),
                      MockRead(net::SYNCHRONOUS, net::OK)};

  StaticSocketDataProvider socket_data(reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(12, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes), request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), request->GetTotalReceivedBytes());
}

TEST_F(URLRequestHttpJobWithMockSocketsTest, TestContentLengthFailedRequest) {
  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 20\r\n\r\n"),
                      MockRead("Test Content"),
                      MockRead(net::SYNCHRONOUS, net::ERR_FAILED)};

  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.request_status(), IsError(ERR_FAILED));
  EXPECT_EQ(12, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes), request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), request->GetTotalReceivedBytes());
}

TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestContentLengthCancelledRequest) {
  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 20\r\n\r\n"),
                      MockRead("Test Content"),
                      MockRead(net::SYNCHRONOUS, net::ERR_IO_PENDING)};

  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  delegate.set_cancel_in_received_data(true);
  request->Start();
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.request_status(), IsError(ERR_ABORTED));
  EXPECT_EQ(12, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes), request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), request->GetTotalReceivedBytes());
}

TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestNetworkBytesRedirectedRequest) {
  MockWrite redirect_writes[] = {
      MockWrite("GET / HTTP/1.1\r\n"
                "Host: www.redirect.com\r\n"
                "Connection: keep-alive\r\n"
                "User-Agent: \r\n"
                "Accept-Encoding: gzip, deflate\r\n"
                "Accept-Language: en-us,fr\r\n\r\n")};

  MockRead redirect_reads[] = {
      MockRead("HTTP/1.1 302 Found\r\n"
               "Location: http://www.example.com\r\n\r\n"),
  };
  StaticSocketDataProvider redirect_socket_data(redirect_reads,
                                                redirect_writes);
  socket_factory_.AddSocketDataProvider(&redirect_socket_data);

  MockWrite final_writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead final_reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                                     "Content-Length: 12\r\n\r\n"),
                            MockRead("Test Content")};
  StaticSocketDataProvider final_socket_data(final_reads, final_writes);
  socket_factory_.AddSocketDataProvider(&final_socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.redirect.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(12, request->received_response_content_length());
  // Should not include the redirect.
  EXPECT_EQ(CountWriteBytes(final_writes), request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(final_reads), request->GetTotalReceivedBytes());
}

TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestNetworkBytesCancelledAfterHeaders) {
  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n\r\n")};
  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  delegate.set_cancel_in_response_started(true);
  request->Start();
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.request_status(), IsError(ERR_ABORTED));
  EXPECT_EQ(0, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes), request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), request->GetTotalReceivedBytes());
}

TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestNetworkBytesCancelledImmediately) {
  StaticSocketDataProvider socket_data;
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  request->Cancel();
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.request_status(), IsError(ERR_ABORTED));
  EXPECT_EQ(0, request->received_response_content_length());
  EXPECT_EQ(0, request->GetTotalSentBytes());
  EXPECT_EQ(0, request->GetTotalReceivedBytes());
}

TEST_F(URLRequestHttpJobWithMockSocketsTest, TestHttpTimeToFirstByte) {
  base::HistogramTester histograms;
  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};

  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
  histograms.ExpectTotalCount("Net.HttpTimeToFirstByte", 0);

  request->Start();
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.request_status(), IsOk());
  histograms.ExpectTotalCount("Net.HttpTimeToFirstByte", 1);
}

TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestHttpTimeToFirstByteForCancelledTask) {
  base::HistogramTester histograms;
  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};

  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  request->Cancel();
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.request_status(), IsError(ERR_ABORTED));
  histograms.ExpectTotalCount("Net.HttpTimeToFirstByte", 0);
}

TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestHttpJobSuccessPriorityKeyedTotalTime) {
  base::HistogramTester histograms;

  for (int priority = 0; priority < net::NUM_PRIORITIES; ++priority) {
    for (int request_index = 0; request_index <= priority; ++request_index) {
      MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
      MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                                   "Content-Length: 12\r\n\r\n"),
                          MockRead("Test Content")};

      StaticSocketDataProvider socket_data(reads, writes);
      socket_factory_.AddSocketDataProvider(&socket_data);

      TestDelegate delegate;
      std::unique_ptr<URLRequest> request =
          context_->CreateRequest(GURL("http://www.example.com/"),
                                  static_cast<net::RequestPriority>(priority),
                                  &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

      request->Start();
      delegate.RunUntilComplete();
      EXPECT_THAT(delegate.request_status(), IsOk());
    }
  }

  for (int priority = 0; priority < net::NUM_PRIORITIES; ++priority) {
    histograms.ExpectTotalCount("Net.HttpJob.TotalTimeSuccess.Priority" +
                                    base::NumberToString(priority),
                                priority + 1);
  }
}

TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestHttpJobRecordsTrustAnchorHistograms) {
  SSLSocketDataProvider ssl_socket_data(net::ASYNC, net::OK);
  ssl_socket_data.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  // Simulate a certificate chain issued by "C=US, O=Google Trust Services LLC,
  // CN=GTS Root R4". This publicly-trusted root was chosen as it was included
  // in 2017 and is not anticipated to be removed from all supported platforms
  // for a few decades.
  // Note: The actual cert in |cert| does not matter for this testing.
  SHA256HashValue leaf_hash = {{0}};
  SHA256HashValue intermediate_hash = {{1}};
  SHA256HashValue root_hash = {
      {0x98, 0x47, 0xe5, 0x65, 0x3e, 0x5e, 0x9e, 0x84, 0x75, 0x16, 0xe5,
       0xcb, 0x81, 0x86, 0x06, 0xaa, 0x75, 0x44, 0xa1, 0x9b, 0xe6, 0x7f,
       0xd7, 0x36, 0x6d, 0x50, 0x69, 0x88, 0xe8, 0xd8, 0x43, 0x47}};
  ssl_socket_data.ssl_info.public_key_hashes.push_back(HashValue(leaf_hash));
  ssl_socket_data.ssl_info.public_key_hashes.push_back(
      HashValue(intermediate_hash));
  ssl_socket_data.ssl_info.public_key_hashes.push_back(HashValue(root_hash));

  const base::HistogramBase::Sample kGTSRootR4HistogramID = 486;

  socket_factory_.AddSSLSocketDataProvider(&ssl_socket_data);

  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};
  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);

  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kTrustAnchorRequestHistogram, 0);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request = context_->CreateRequest(
      GURL("https://www.example.com/"), DEFAULT_PRIORITY, &delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS);
  request->Start();
  delegate.RunUntilComplete();
  EXPECT_THAT(delegate.request_status(), IsOk());

  histograms.ExpectTotalCount(kTrustAnchorRequestHistogram, 1);
  histograms.ExpectUniqueSample(kTrustAnchorRequestHistogram,
                                kGTSRootR4HistogramID, 1);
}

TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestHttpJobDoesNotRecordTrustAnchorHistogramsWhenNoNetworkLoad) {
  SSLSocketDataProvider ssl_socket_data(net::ASYNC, net::OK);
  ssl_socket_data.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  // Simulate a request loaded from a non-network source, such as a disk
  // cache.
  ssl_socket_data.ssl_info.public_key_hashes.clear();

  socket_factory_.AddSSLSocketDataProvider(&ssl_socket_data);

  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};
  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);

  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kTrustAnchorRequestHistogram, 0);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request = context_->CreateRequest(
      GURL("https://www.example.com/"), DEFAULT_PRIORITY, &delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS);
  request->Start();
  delegate.RunUntilComplete();
  EXPECT_THAT(delegate.request_status(), IsOk());

  histograms.ExpectTotalCount(kTrustAnchorRequestHistogram, 0);
}

TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestHttpJobRecordsMostSpecificTrustAnchorHistograms) {
  SSLSocketDataProvider ssl_socket_data(net::ASYNC, net::OK);
  ssl_socket_data.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  // Simulate a certificate chain issued by "C=US, O=Google Trust Services LLC,
  // CN=GTS Root R4". This publicly-trusted root was chosen as it was included
  // in 2017 and is not anticipated to be removed from all supported platforms
  // for a few decades.
  // Note: The actual cert in |cert| does not matter for this testing.
  SHA256HashValue leaf_hash = {{0}};
  SHA256HashValue intermediate_hash = {{1}};
  SHA256HashValue gts_root_r3_hash = {
      {0x41, 0x79, 0xed, 0xd9, 0x81, 0xef, 0x74, 0x74, 0x77, 0xb4, 0x96,
       0x26, 0x40, 0x8a, 0xf4, 0x3d, 0xaa, 0x2c, 0xa7, 0xab, 0x7f, 0x9e,
       0x08, 0x2c, 0x10, 0x60, 0xf8, 0x40, 0x96, 0x77, 0x43, 0x48}};
  SHA256HashValue gts_root_r4_hash = {
      {0x98, 0x47, 0xe5, 0x65, 0x3e, 0x5e, 0x9e, 0x84, 0x75, 0x16, 0xe5,
       0xcb, 0x81, 0x86, 0x06, 0xaa, 0x75, 0x44, 0xa1, 0x9b, 0xe6, 0x7f,
       0xd7, 0x36, 0x6d, 0x50, 0x69, 0x88, 0xe8, 0xd8, 0x43, 0x47}};
  ssl_socket_data.ssl_info.public_key_hashes.push_back(HashValue(leaf_hash));
  ssl_socket_data.ssl_info.public_key_hashes.push_back(
      HashValue(intermediate_hash));
  ssl_socket_data.ssl_info.public_key_hashes.push_back(
      HashValue(gts_root_r3_hash));
  ssl_socket_data.ssl_info.public_key_hashes.push_back(
      HashValue(gts_root_r4_hash));

  const base::HistogramBase::Sample kGTSRootR3HistogramID = 485;

  socket_factory_.AddSSLSocketDataProvider(&ssl_socket_data);

  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};
  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);

  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kTrustAnchorRequestHistogram, 0);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request = context_->CreateRequest(
      GURL("https://www.example.com/"), DEFAULT_PRIORITY, &delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS);
  request->Start();
  delegate.RunUntilComplete();
  EXPECT_THAT(delegate.request_status(), IsOk());

  histograms.ExpectTotalCount(kTrustAnchorRequestHistogram, 1);
  histograms.ExpectUniqueSample(kTrustAnchorRequestHistogram,
                                kGTSRootR3HistogramID, 1);
}

TEST_F(URLRequestHttpJobWithMockSocketsTest, EncodingAdvertisementOnRange) {
  MockWrite writes[] = {
      MockWrite("GET / HTTP/1.1\r\n"
                "Host: www.example.com\r\n"
                "Connection: keep-alive\r\n"
                "User-Agent: \r\n"
                "Accept-Encoding: identity\r\n"
                "Accept-Language: en-us,fr\r\n"
                "Range: bytes=0-1023\r\n\r\n")};

  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Accept-Ranges: bytes\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};

  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  // Make the extra header to trigger the change in "Accepted-Encoding"
  HttpRequestHeaders headers;
  headers.SetHeader("Range", "bytes=0-1023");
  request->SetExtraRequestHeaders(headers);

  request->Start();
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(12, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes), request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), request->GetTotalReceivedBytes());
}

TEST_F(URLRequestHttpJobWithMockSocketsTest, RangeRequestOverrideEncoding) {
  MockWrite writes[] = {
      MockWrite("GET / HTTP/1.1\r\n"
                "Host: www.example.com\r\n"
                "Connection: keep-alive\r\n"
                "Accept-Encoding: gzip, deflate\r\n"
                "User-Agent: \r\n"
                "Accept-Language: en-us,fr\r\n"
                "Range: bytes=0-1023\r\n\r\n")};

  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Accept-Ranges: bytes\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};

  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  // Explicitly set "Accept-Encoding" to make sure it's not overridden by
  // AddExtraHeaders
  HttpRequestHeaders headers;
  headers.SetHeader("Accept-Encoding", "gzip, deflate");
  headers.SetHeader("Range", "bytes=0-1023");
  request->SetExtraRequestHeaders(headers);

  request->Start();
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(12, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes), request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), request->GetTotalReceivedBytes());
}

TEST_F(URLRequestHttpJobTest, TestCancelWhileReadingCookies) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->SetCookieStore(std::make_unique<DelayedCookieMonster>());
  auto context = context_builder->Build();

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                             &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  request->Cancel();
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.request_status(), IsError(ERR_ABORTED));
}

// Make sure that SetPriority actually sets the URLRequestHttpJob's
// priority, before start.  Other tests handle the after start case.
TEST_F(URLRequestHttpJobTest, SetPriorityBasic) {
  auto job = std::make_unique<TestURLRequestHttpJob>(req_.get());
  EXPECT_EQ(DEFAULT_PRIORITY, job->priority());

  job->SetPriority(LOWEST);
  EXPECT_EQ(LOWEST, job->priority());

  job->SetPriority(LOW);
  EXPECT_EQ(LOW, job->priority());
}

// Make sure that URLRequestHttpJob passes on its priority to its
// transaction on start.
TEST_F(URLRequestHttpJobTest, SetTransactionPriorityOnStart) {
  TestScopedURLInterceptor interceptor(
      req_->url(), std::make_unique<TestURLRequestHttpJob>(req_.get()));
  req_->SetPriority(LOW);

  EXPECT_FALSE(network_layer().last_transaction());

  req_->Start();

  ASSERT_TRUE(network_layer().last_transaction());
  EXPECT_EQ(LOW, network_layer().last_transaction()->priority());
}

// Make sure that URLRequestHttpJob passes on its priority updates to
// its transaction.
TEST_F(URLRequestHttpJobTest, SetTransactionPriority) {
  TestScopedURLInterceptor interceptor(
      req_->url(), std::make_unique<TestURLRequestHttpJob>(req_.get()));
  req_->SetPriority(LOW);
  req_->Start();
  ASSERT_TRUE(network_layer().last_transaction());
  EXPECT_EQ(LOW, network_layer().last_transaction()->priority());

  req_->SetPriority(HIGHEST);
  EXPECT_EQ(HIGHEST, network_layer().last_transaction()->priority());
}

TEST_F(URLRequestHttpJobTest, HSTSInternalRedirectTest) {
  // Setup HSTS state.
  context_->transport_security_state()->AddHSTS(
      "upgrade.test", base::Time::Now() + base::Seconds(10), true);
  ASSERT_TRUE(
      context_->transport_security_state()->ShouldUpgradeToSSL("upgrade.test"));
  ASSERT_FALSE(context_->transport_security_state()->ShouldUpgradeToSSL(
      "no-upgrade.test"));

  struct TestCase {
    const char* url;
    bool upgrade_expected;
    const char* url_expected;
  } cases[] = {
    {"http://upgrade.test/", true, "https://upgrade.test/"},
    {"http://upgrade.test:123/", true, "https://upgrade.test:123/"},
    {"http://no-upgrade.test/", false, "http://no-upgrade.test/"},
    {"http://no-upgrade.test:123/", false, "http://no-upgrade.test:123/"},
#if BUILDFLAG(ENABLE_WEBSOCKETS)
    {"ws://upgrade.test/", true, "wss://upgrade.test/"},
    {"ws://upgrade.test:123/", true, "wss://upgrade.test:123/"},
    {"ws://no-upgrade.test/", false, "ws://no-upgrade.test/"},
    {"ws://no-upgrade.test:123/", false, "ws://no-upgrade.test:123/"},
#endif  // BUILDFLAG(ENABLE_WEBSOCKETS)
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(test.url);

    GURL url = GURL(test.url);
    // This is needed to bypass logic that rejects using URLRequests directly
    // for WebSocket requests.
    bool is_for_websockets = url.SchemeIsWSOrWSS();

    TestDelegate d;
    TestNetworkDelegate network_delegate;
    std::unique_ptr<URLRequest> r(context_->CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS,
        is_for_websockets));

    net_log_observer_.Clear();
    r->Start();
    d.RunUntilComplete();

    if (test.upgrade_expected) {
      auto entries = net_log_observer_.GetEntriesWithType(
          net::NetLogEventType::URL_REQUEST_REDIRECT_JOB);
      int redirects = entries.size();
      for (const auto& entry : entries) {
        EXPECT_EQ("HSTS", GetStringValueFromParams(entry, "reason"));
      }
      EXPECT_EQ(1, redirects);
      EXPECT_EQ(1, d.received_redirect_count());
      EXPECT_EQ(2u, r->url_chain().size());
    } else {
      EXPECT_EQ(0, d.received_redirect_count());
      EXPECT_EQ(1u, r->url_chain().size());
    }
    EXPECT_EQ(GURL(test.url_expected), r->url());
  }
}

TEST_F(URLRequestHttpJobTest, ShouldBypassHSTS) {
  // Setup HSTS state.
  context_->transport_security_state()->AddHSTS(
      "upgrade.test", base::Time::Now() + base::Seconds(30), true);
  ASSERT_TRUE(
      context_->transport_security_state()->ShouldUpgradeToSSL("upgrade.test"));

  struct TestCase {
    const char* url;
    bool bypass_hsts;
    const char* url_expected;
  } cases[] = {
    {"http://upgrade.test/example.crl", true,
     "http://upgrade.test/example.crl"},
    // This test ensures that the HSTS check and upgrade happens prior to cache
    // and socket pool checks
    {"http://upgrade.test/example.crl", false,
     "https://upgrade.test/example.crl"},
    {"http://upgrade.test", false, "https://upgrade.test"},
    {"http://upgrade.test:1080", false, "https://upgrade.test:1080"},
#if BUILDFLAG(ENABLE_WEBSOCKETS)
    {"ws://upgrade.test/example.crl", true, "ws://upgrade.test/example.crl"},
    {"ws://upgrade.test/example.crl", false, "wss://upgrade.test/example.crl"},
    {"ws://upgrade.test", false, "wss://upgrade.test"},
    {"ws://upgrade.test:1080", false, "wss://upgrade.test:1080"},
#endif  // BUILDFLAG(ENABLE_WEBSOCKETS)
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(test.url);

    GURL url = GURL(test.url);
    // This is needed to bypass logic that rejects using URLRequests directly
    // for WebSocket requests.
    bool is_for_websockets = url.SchemeIsWSOrWSS();

    TestDelegate d;
    TestNetworkDelegate network_delegate;
    std::unique_ptr<URLRequest> r(context_->CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS,
        is_for_websockets));
    if (test.bypass_hsts) {
      r->SetLoadFlags(net::LOAD_SHOULD_BYPASS_HSTS);
      r->set_allow_credentials(false);
    }

    net_log_observer_.Clear();
    r->Start();
    d.RunUntilComplete();

    if (test.bypass_hsts) {
      EXPECT_EQ(0, d.received_redirect_count());
      EXPECT_EQ(1u, r->url_chain().size());
    } else {
      auto entries = net_log_observer_.GetEntriesWithType(
          net::NetLogEventType::URL_REQUEST_REDIRECT_JOB);
      int redirects = entries.size();
      for (const auto& entry : entries) {
        EXPECT_EQ("HSTS", GetStringValueFromParams(entry, "reason"));
      }
      EXPECT_EQ(1, redirects);
      EXPECT_EQ(1, d.received_redirect_count());
      EXPECT_EQ(2u, r->url_chain().size());
    }
    EXPECT_EQ(GURL(test.url_expected), r->url());
  }
}

#if BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)

class URLRequestHttpJobWithMockSocketsDeviceBoundSessionServiceTest
    : public TestWithTaskEnvironment {
 protected:
  URLRequestHttpJobWithMockSocketsDeviceBoundSessionServiceTest() {
    auto context_builder = CreateTestURLRequestContextBuilder();
    context_builder->set_client_socket_factory_for_testing(&socket_factory_);
    context_builder->set_device_bound_session_service(
        std::make_unique<
            testing::StrictMock<device_bound_sessions::SessionServiceMock>>());
    context_ = context_builder->Build();
    request_ = context_->CreateRequest(GURL("http://www.example.com"),
                                       DEFAULT_PRIORITY, &delegate_,
                                       TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  device_bound_sessions::SessionServiceMock& GetMockService() {
    return *static_cast<device_bound_sessions::SessionServiceMock*>(
        context_->device_bound_session_service());
  }

  MockClientSocketFactory socket_factory_;
  std::unique_ptr<URLRequestContext> context_;
  TestDelegate delegate_;
  std::unique_ptr<URLRequest> request_;
};

TEST_F(URLRequestHttpJobWithMockSocketsDeviceBoundSessionServiceTest,
       ShouldRespondToDeviceBoundSessionHeader) {
  const MockWrite writes[] = {
      MockWrite("GET / HTTP/1.1\r\n"
                "Host: www.example.com\r\n"
                "Connection: keep-alive\r\n"
                "User-Agent: \r\n"
                "Accept-Encoding: gzip, deflate\r\n"
                "Accept-Language: en-us,fr\r\n\r\n")};

  const MockRead reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n"
               "Accept-Ranges: bytes\r\n"
               "Sec-Session-Registration: (ES256);path=\"new\";"
               "challenge=\"test\"\r\n"
               "Content-Length: 12\r\n\r\n"),
      MockRead("Test Content")};

  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);

  request_->Start();
  EXPECT_CALL(GetMockService(), RegisterBoundSession).Times(1);
  delegate_.RunUntilComplete();
  EXPECT_THAT(delegate_.request_status(), IsOk());
}

TEST_F(URLRequestHttpJobWithMockSocketsDeviceBoundSessionServiceTest,
       ShouldNotRespondWithoutDeviceBoundSessionHeader) {
  const MockWrite writes[] = {
      MockWrite("GET / HTTP/1.1\r\n"
                "Host: www.example.com\r\n"
                "Connection: keep-alive\r\n"
                "User-Agent: \r\n"
                "Accept-Encoding: gzip, deflate\r\n"
                "Accept-Language: en-us,fr\r\n\r\n")};

  const MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                                     "Accept-Ranges: bytes\r\n"
                                     "Content-Length: 12\r\n\r\n"),
                            MockRead("Test Content")};

  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);

  request_->Start();
  EXPECT_CALL(GetMockService(), RegisterBoundSession).Times(0);
  delegate_.RunUntilComplete();
  EXPECT_THAT(delegate_.request_status(), IsOk());
}

TEST_F(URLRequestHttpJobWithMockSocketsDeviceBoundSessionServiceTest,
       ShouldProcessDeviceBoundSessionChallengeHeader) {
  const MockWrite writes[] = {
      MockWrite("GET / HTTP/1.1\r\n"
                "Host: www.example.com\r\n"
                "Connection: keep-alive\r\n"
                "User-Agent: \r\n"
                "Accept-Encoding: gzip, deflate\r\n"
                "Accept-Language: en-us,fr\r\n\r\n")};

  const MockRead reads[] = {
      MockRead(
          "HTTP/1.1 200 OK\r\n"
          "Accept-Ranges: bytes\r\n"
          "Sec-Session-Challenge: \"session_identifier\";challenge=\"test\"\r\n"
          "Content-Length: 12\r\n\r\n"),
      MockRead("Test Content")};

  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);

  request_->Start();
  EXPECT_CALL(GetMockService(), SetChallengeForBoundSession).Times(1);
  delegate_.RunUntilComplete();
  EXPECT_THAT(delegate_.request_status(), IsOk());
}

#endif  // BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)

namespace {
std::unique_ptr<test_server::HttpResponse> HandleRequest(
    const std::string_view& content,
    const test_server::HttpRequest& request) {
  auto response = std::make_unique<test_server::BasicHttpResponse>();
  response->set_content(content);
  return std::move(response);
}
}  // namespace

// This test checks that if an HTTP connection was made for a request that has
// the should_bypass_hsts flag set to true, subsequent calls to the exact same
// URL WITHOUT should_bypass_hsts=true will be upgraded to HTTPS early
// enough in the process such that the HTTP socket connection is not re-used,
// and the request does not have a hit in the cache.
TEST_F(URLRequestHttpJobTest, ShouldBypassHSTSResponseAndConnectionNotReused) {
  constexpr std::string_view kSecureContent = "Secure: Okay Content";
  constexpr std::string_view kInsecureContent = "Insecure: Bad Content";

  auto context_builder = CreateTestURLRequestContextBuilder();
  auto context = context_builder->Build();

  // The host of all EmbeddedTestServer URLs is 127.0.0.1.
  context->transport_security_state()->AddHSTS(
      "127.0.0.1", base::Time::Now() + base::Seconds(30), true);
  ASSERT_TRUE(
      context->transport_security_state()->ShouldUpgradeToSSL("127.0.0.1"));

  GURL::Replacements replace_scheme;
  replace_scheme.SetSchemeStr("https");
  GURL insecure_url;
  GURL secure_url;

  int common_port = 0;

  // Create an HTTP request that is not upgraded to the should_bypass_hsts flag,
  // and ensure that the response is stored in the cache.
  {
    EmbeddedTestServer http_server(EmbeddedTestServer::TYPE_HTTP);
    http_server.AddDefaultHandlers(base::FilePath());
    http_server.RegisterRequestHandler(
        base::BindRepeating(&HandleRequest, kInsecureContent));
    ASSERT_TRUE(http_server.Start());
    common_port = http_server.port();

    insecure_url = http_server.base_url();
    ASSERT_TRUE(insecure_url.SchemeIs("http"));
    secure_url = insecure_url.ReplaceComponents(replace_scheme);
    ASSERT_TRUE(secure_url.SchemeIs("https"));

    net_log_observer_.Clear();
    TestDelegate delegate;
    std::unique_ptr<URLRequest> req(
        context->CreateRequest(insecure_url, DEFAULT_PRIORITY, &delegate,
                               TRAFFIC_ANNOTATION_FOR_TESTS));
    req->SetLoadFlags(net::LOAD_SHOULD_BYPASS_HSTS);
    req->set_allow_credentials(false);
    req->Start();
    delegate.RunUntilComplete();
    EXPECT_EQ(kInsecureContent, delegate.data_received());
    // There should be 2 cache event entries, one for beginning the read and one
    // for finishing the read.
    EXPECT_EQ(2u, net_log_observer_
                      .GetEntriesWithType(
                          net::NetLogEventType::HTTP_CACHE_ADD_TO_ENTRY)
                      .size());
    ASSERT_TRUE(http_server.ShutdownAndWaitUntilComplete());
  }
  // Test that a request with the same URL will be upgraded as long as
  // should_bypass_hsts flag is not set, and doesn't have an cache hit or
  // re-use an existing socket connection.
  {
    EmbeddedTestServer https_server(EmbeddedTestServer::TYPE_HTTPS);
    https_server.AddDefaultHandlers(base::FilePath());
    https_server.RegisterRequestHandler(
        base::BindRepeating(&HandleRequest, kSecureContent));
    ASSERT_TRUE(https_server.Start(common_port));

    TestDelegate delegate;
    std::unique_ptr<URLRequest> req(
        context->CreateRequest(insecure_url, DEFAULT_PRIORITY, &delegate,
                               TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_allow_credentials(false);
    req->Start();
    delegate.RunUntilRedirect();
    // Ensure that the new URL has an upgraded protocol. This ensures that when
    // the redirect request continues, the HTTP socket connection from before
    // will not be re-used, given that "protocol" is one of the fields used to
    // create a socket connection. Documentation here:
    // https://chromium.googlesource.com/chromium/src/+/HEAD/net/docs/life-of-a-url-request.md
    // under "Socket Pools" section.
    EXPECT_EQ(delegate.redirect_info().new_url, secure_url);
    EXPECT_TRUE(delegate.redirect_info().new_url.SchemeIs("https"));
    EXPECT_THAT(delegate.request_status(), net::ERR_IO_PENDING);

    req->FollowDeferredRedirect(std::nullopt /* removed_headers */,
                                std::nullopt /* modified_headers */);
    delegate.RunUntilComplete();
    EXPECT_EQ(kSecureContent, delegate.data_received());
    EXPECT_FALSE(req->was_cached());
    ASSERT_TRUE(https_server.ShutdownAndWaitUntilComplete());
  }
}

TEST_F(URLRequestHttpJobTest, HSTSInternalRedirectCallback) {
  EmbeddedTestServer https_test(EmbeddedTestServer::TYPE_HTTPS);
  https_test.AddDefaultHandlers(base::FilePath());
  ASSERT_TRUE(https_test.Start());

  auto context = CreateTestURLRequestContextBuilder()->Build();
  context->transport_security_state()->AddHSTS(
      "127.0.0.1", base::Time::Now() + base::Seconds(10), true);
  ASSERT_TRUE(
      context->transport_security_state()->ShouldUpgradeToSSL("127.0.0.1"));

  GURL::Replacements replace_scheme;
  replace_scheme.SetSchemeStr("http");

  {
    GURL url(
        https_test.GetURL("/echoheader").ReplaceComponents(replace_scheme));
    TestDelegate delegate;
    HttpRequestHeaders extra_headers;
    extra_headers.SetHeader("X-HSTS-Test", "1");

    HttpRawRequestHeaders raw_req_headers;

    std::unique_ptr<URLRequest> r(context->CreateRequest(
        url, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->SetExtraRequestHeaders(extra_headers);
    r->SetRequestHeadersCallback(base::BindRepeating(
        &HttpRawRequestHeaders::Assign, base::Unretained(&raw_req_headers)));

    r->Start();
    delegate.RunUntilRedirect();

    EXPECT_FALSE(raw_req_headers.headers().empty());
    std::string value;
    EXPECT_TRUE(raw_req_headers.FindHeaderForTest("X-HSTS-Test", &value));
    EXPECT_EQ("1", value);
    EXPECT_EQ("GET /echoheader HTTP/1.1\r\n", raw_req_headers.request_line());

    raw_req_headers = HttpRawRequestHeaders();

    r->FollowDeferredRedirect(std::nullopt /* removed_headers */,
                              std::nullopt /* modified_headers */);
    delegate.RunUntilComplete();

    EXPECT_FALSE(raw_req_headers.headers().empty());
  }

  {
    GURL url(https_test.GetURL("/echoheader?foo=bar")
                 .ReplaceComponents(replace_scheme));
    TestDelegate delegate;

    HttpRawRequestHeaders raw_req_headers;

    std::unique_ptr<URLRequest> r(context->CreateRequest(
        url, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->SetRequestHeadersCallback(base::BindRepeating(
        &HttpRawRequestHeaders::Assign, base::Unretained(&raw_req_headers)));

    r->Start();
    delegate.RunUntilRedirect();

    EXPECT_EQ("GET /echoheader?foo=bar HTTP/1.1\r\n",
              raw_req_headers.request_line());
  }

  {
    GURL url(
        https_test.GetURL("/echoheader#foo").ReplaceComponents(replace_scheme));
    TestDelegate delegate;

    HttpRawRequestHeaders raw_req_headers;

    std::unique_ptr<URLRequest> r(context->CreateRequest(
        url, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    r->SetRequestHeadersCallback(base::BindRepeating(
        &HttpRawRequestHeaders::Assign, base::Unretained(&raw_req_headers)));

    r->Start();
    delegate.RunUntilRedirect();

    EXPECT_EQ("GET /echoheader HTTP/1.1\r\n", raw_req_headers.request_line());
  }
}

class URLRequestHttpJobWithBrotliSupportTest : public TestWithTaskEnvironment {
 protected:
  URLRequestHttpJobWithBrotliSupportTest() {
    HttpNetworkSessionParams params;
    auto context_builder = CreateTestURLRequestContextBuilder();
    context_builder->set_enable_brotli(true);
    context_builder->set_http_network_session_params(params);
    context_builder->set_client_socket_factory_for_testing(&socket_factory_);
    context_ = context_builder->Build();
  }

  MockClientSocketFactory socket_factory_;
  std::unique_ptr<URLRequestContext> context_;
};

TEST_F(URLRequestHttpJobWithBrotliSupportTest, NoBrotliAdvertisementOverHttp) {
  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};
  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
  request->Start();
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(12, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes), request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), request->GetTotalReceivedBytes());
}

TEST_F(URLRequestHttpJobWithBrotliSupportTest, BrotliAdvertisement) {
  net::SSLSocketDataProvider ssl_socket_data_provider(net::ASYNC, net::OK);
  ssl_socket_data_provider.next_proto = kProtoHTTP11;
  ssl_socket_data_provider.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "unittest.selfsigned.der");
  ASSERT_TRUE(ssl_socket_data_provider.ssl_info.cert);
  socket_factory_.AddSSLSocketDataProvider(&ssl_socket_data_provider);

  MockWrite writes[] = {
      MockWrite("GET / HTTP/1.1\r\n"
                "Host: www.example.com\r\n"
                "Connection: keep-alive\r\n"
                "User-Agent: \r\n"
                "Accept-Encoding: gzip, deflate, br\r\n"
                "Accept-Language: en-us,fr\r\n\r\n")};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};
  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("https://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
  request->Start();
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(12, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes), request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), request->GetTotalReceivedBytes());
}

TEST_F(URLRequestHttpJobWithBrotliSupportTest, DefaultAcceptEncodingOverriden) {
  struct {
    base::flat_set<net::SourceStream::SourceType> accepted_types;
    const char* expected_request_headers;
  } kTestCases[] = {{{net::SourceStream::SourceType::TYPE_DEFLATE},
                     "GET / HTTP/1.1\r\n"
                     "Host: www.example.com\r\n"
                     "Connection: keep-alive\r\n"
                     "User-Agent: \r\n"
                     "Accept-Encoding: deflate\r\n"
                     "Accept-Language: en-us,fr\r\n\r\n"},
                    {{},
                     "GET / HTTP/1.1\r\n"
                     "Host: www.example.com\r\n"
                     "Connection: keep-alive\r\n"
                     "User-Agent: \r\n"
                     "Accept-Language: en-us,fr\r\n\r\n"},
                    {{net::SourceStream::SourceType::TYPE_GZIP},
                     "GET / HTTP/1.1\r\n"
                     "Host: www.example.com\r\n"
                     "Connection: keep-alive\r\n"
                     "User-Agent: \r\n"
                     "Accept-Encoding: gzip\r\n"
                     "Accept-Language: en-us,fr\r\n\r\n"},
                    {{net::SourceStream::SourceType::TYPE_GZIP,
                      net::SourceStream::SourceType::TYPE_DEFLATE},
                     "GET / HTTP/1.1\r\n"
                     "Host: www.example.com\r\n"
                     "Connection: keep-alive\r\n"
                     "User-Agent: \r\n"
                     "Accept-Encoding: gzip, deflate\r\n"
                     "Accept-Language: en-us,fr\r\n\r\n"},
                    {{net::SourceStream::SourceType::TYPE_BROTLI},
                     "GET / HTTP/1.1\r\n"
                     "Host: www.example.com\r\n"
                     "Connection: keep-alive\r\n"
                     "User-Agent: \r\n"
                     "Accept-Encoding: br\r\n"
                     "Accept-Language: en-us,fr\r\n\r\n"},
                    {{net::SourceStream::SourceType::TYPE_BROTLI,
                      net::SourceStream::SourceType::TYPE_GZIP,
                      net::SourceStream::SourceType::TYPE_DEFLATE},
                     "GET / HTTP/1.1\r\n"
                     "Host: www.example.com\r\n"
                     "Connection: keep-alive\r\n"
                     "User-Agent: \r\n"
                     "Accept-Encoding: gzip, deflate, br\r\n"
                     "Accept-Language: en-us,fr\r\n\r\n"}};

  for (auto test : kTestCases) {
    net::SSLSocketDataProvider ssl_socket_data_provider(net::ASYNC, net::OK);
    ssl_socket_data_provider.next_proto = kProtoHTTP11;
    ssl_socket_data_provider.ssl_info.cert =
        ImportCertFromFile(GetTestCertsDirectory(), "unittest.selfsigned.der");
    ASSERT_TRUE(ssl_socket_data_provider.ssl_info.cert);
    socket_factory_.AddSSLSocketDataProvider(&ssl_socket_data_provider);

    MockWrite writes[] = {MockWrite(test.expected_request_headers)};
    MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                                 "Content-Length: 12\r\n\r\n"),
                        MockRead("Test Content")};
    StaticSocketDataProvider socket_data(reads, writes);
    socket_factory_.AddSocketDataProvider(&socket_data);

    TestDelegate delegate;
    std::unique_ptr<URLRequest> request = context_->CreateRequest(
        GURL("https://www.example.com"), DEFAULT_PRIORITY, &delegate,
        TRAFFIC_ANNOTATION_FOR_TESTS);
    request->set_accepted_stream_types(test.accepted_types);
    request->Start();
    delegate.RunUntilComplete();
    EXPECT_THAT(delegate.request_status(), IsOk());
    socket_factory_.ResetNextMockIndexes();
  }
}

#if BUILDFLAG(IS_ANDROID)
class URLRequestHttpJobWithCheckClearTextPermittedTest
    : public TestWithTaskEnvironment {
 protected:
  URLRequestHttpJobWithCheckClearTextPermittedTest() {
    auto context_builder = CreateTestURLRequestContextBuilder();
    context_builder->SetHttpTransactionFactoryForTesting(
        std::make_unique<MockNetworkLayer>());
    context_builder->set_check_cleartext_permitted(true);
    context_builder->set_client_socket_factory_for_testing(&socket_factory_);
    context_ = context_builder->Build();
  }

  MockClientSocketFactory socket_factory_;
  std::unique_ptr<URLRequestContext> context_;
};

TEST_F(URLRequestHttpJobWithCheckClearTextPermittedTest,
       AndroidCleartextPermittedTest) {
  static constexpr struct TestCase {
    const char* url;
    bool cleartext_permitted;
    bool should_block;
    int expected_per_host_call_count;
    int expected_default_call_count;
  } kTestCases[] = {
      {"http://unblocked.test/", true, false, 1, 0},
      {"https://unblocked.test/", true, false, 0, 0},
      {"http://blocked.test/", false, true, 1, 0},
      {"https://blocked.test/", false, false, 0, 0},
      // If determining the per-host cleartext policy causes an
      // IllegalArgumentException (because the hostname is invalid),
      // the default configuration should be applied, and the
      // exception should not cause a JNI error.
      {"http://./", false, true, 1, 1},
      {"http://./", true, false, 1, 1},
      // Even if the host name would be considered invalid, https
      // schemes should not trigger cleartext policy checks.
      {"https://./", false, false, 0, 0},
  };

  JNIEnv* env = base::android::AttachCurrentThread();
  for (const TestCase& test : kTestCases) {
    Java_AndroidNetworkLibraryTestUtil_setUpSecurityPolicyForTesting(
        env, test.cleartext_permitted);

    TestDelegate delegate;
    std::unique_ptr<URLRequest> request =
        context_->CreateRequest(GURL(test.url), DEFAULT_PRIORITY, &delegate,
                                TRAFFIC_ANNOTATION_FOR_TESTS);
    request->Start();
    delegate.RunUntilComplete();

    if (test.should_block) {
      EXPECT_THAT(delegate.request_status(),
                  IsError(ERR_CLEARTEXT_NOT_PERMITTED));
    } else {
      // Should fail since there's no test server running
      EXPECT_THAT(delegate.request_status(), IsError(ERR_FAILED));
    }
    EXPECT_EQ(
        Java_AndroidNetworkLibraryTestUtil_getPerHostCleartextCheckCount(env),
        test.expected_per_host_call_count);
    EXPECT_EQ(
        Java_AndroidNetworkLibraryTestUtil_getDefaultCleartextCheckCount(env),
        test.expected_default_call_count);
  }
}
#endif

#if BUILDFLAG(ENABLE_WEBSOCKETS)

class URLRequestHttpJobWebSocketTest : public TestWithTaskEnvironment {
 protected:
  URLRequestHttpJobWebSocketTest() {
    auto context_builder = CreateTestURLRequestContextBuilder();
    context_builder->set_client_socket_factory_for_testing(&socket_factory_);
    context_ = context_builder->Build();
    req_ =
        context_->CreateRequest(GURL("ws://www.example.org"), DEFAULT_PRIORITY,
                                &delegate_, TRAFFIC_ANNOTATION_FOR_TESTS,
                                /*is_for_websockets=*/true);
  }

  std::unique_ptr<URLRequestContext> context_;
  MockClientSocketFactory socket_factory_;
  TestDelegate delegate_;
  std::unique_ptr<URLRequest> req_;
};

TEST_F(URLRequestHttpJobWebSocketTest, RejectedWithoutCreateHelper) {
  req_->Start();
  delegate_.RunUntilComplete();
  EXPECT_THAT(delegate_.request_status(), IsError(ERR_DISALLOWED_URL_SCHEME));
}

TEST_F(URLRequestHttpJobWebSocketTest, CreateHelperPassedThrough) {
  HttpRequestHeaders headers;
  headers.SetHeader("Connection", "Upgrade");
  headers.SetHeader("Upgrade", "websocket");
  headers.SetHeader("Origin", "http://www.example.org");
  headers.SetHeader("Sec-WebSocket-Version", "13");
  req_->SetExtraRequestHeaders(headers);

  MockWrite writes[] = {
      MockWrite("GET / HTTP/1.1\r\n"
                "Host: www.example.org\r\n"
                "Connection: Upgrade\r\n"
                "Upgrade: websocket\r\n"
                "Origin: http://www.example.org\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "User-Agent: \r\n"
                "Accept-Encoding: gzip, deflate\r\n"
                "Accept-Language: en-us,fr\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Extensions: permessage-deflate; "
                "client_max_window_bits\r\n\r\n")};

  MockRead reads[] = {
      MockRead("HTTP/1.1 101 Switching Protocols\r\n"
               "Upgrade: websocket\r\n"
               "Connection: Upgrade\r\n"
               "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n"),
      MockRead(ASYNC, 0)};

  StaticSocketDataProvider data(reads, writes);
  socket_factory_.AddSocketDataProvider(&data);

  auto websocket_stream_create_helper =
      std::make_unique<TestWebSocketHandshakeStreamCreateHelper>();

  req_->SetUserData(kWebSocketHandshakeUserDataKey,
                    std::move(websocket_stream_create_helper));
  req_->SetLoadFlags(LOAD_DISABLE_CACHE);
  req_->Start();
  delegate_.RunUntilComplete();
  EXPECT_THAT(delegate_.request_status(), IsOk());
  EXPECT_TRUE(delegate_.response_completed());

  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());
}

#endif  // BUILDFLAG(ENABLE_WEBSOCKETS)

bool SetAllCookies(CookieMonster* cm, const CookieList& list) {
  DCHECK(cm);
  ResultSavingCookieCallback<CookieAccessResult> callback;
  cm->SetAllCookiesAsync(list, callback.MakeCallback());
  callback.WaitUntilDone();
  return callback.result().status.IsInclude();
}

bool CreateAndSetCookie(CookieStore* cs,
                        const GURL& url,
                        const std::string& cookie_line) {
  auto cookie =
      CanonicalCookie::CreateForTesting(url, cookie_line, base::Time::Now());
  if (!cookie)
    return false;
  DCHECK(cs);
  ResultSavingCookieCallback<CookieAccessResult> callback;
  cs->SetCanonicalCookieAsync(std::move(cookie), url,
                              CookieOptions::MakeAllInclusive(),
                              callback.MakeCallback());
  callback.WaitUntilDone();
  return callback.result().status.IsInclude();
}

void RunRequest(URLRequestContext* context, const GURL& url) {
  TestDelegate delegate;
  std::unique_ptr<URLRequest> request = context->CreateRequest(
      url, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  // Make this a laxly same-site context to allow setting
  // SameSite=Lax-by-default cookies.
  request->set_site_for_cookies(SiteForCookies::FromUrl(url));
  request->Start();
  delegate.RunUntilComplete();
}

}  // namespace

TEST_F(URLRequestHttpJobTest, CookieSchemeRequestSchemeHistogram) {
  base::HistogramTester histograms;
  const std::string test_histogram = "Cookie.CookieSchemeRequestScheme";

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->SetCookieStore(std::make_unique<CookieMonster>(
      /*store=*/nullptr, /*net_log=*/nullptr));
  auto context = context_builder->Build();

  auto* cookie_store = static_cast<CookieMonster*>(context->cookie_store());

  // Secure set cookie marked as Unset source scheme.
  // Using port 7 because it fails the transaction without sending a request and
  // prevents a timeout due to the fake addresses. Because we only need the
  // headers to be generated (and thus the histogram filled) and not actually
  // sent this is acceptable.
  GURL nonsecure_url_for_unset1("http://unset1.example:7");
  GURL secure_url_for_unset1("https://unset1.example:7");

  // Normally the source scheme would be set by
  // CookieMonster::SetCanonicalCookie(), however we're using SetAllCookies() to
  // bypass the source scheme check in order to test the kUnset state which
  // would normally only happen during an existing cookie DB version upgrade.
  std::unique_ptr<CanonicalCookie> unset_cookie1 =
      CanonicalCookie::CreateForTesting(
          secure_url_for_unset1, "NoSourceSchemeHttps=val", base::Time::Now());
  unset_cookie1->SetSourceScheme(net::CookieSourceScheme::kUnset);

  CookieList list1 = {*unset_cookie1};
  EXPECT_TRUE(SetAllCookies(cookie_store, list1));
  RunRequest(context.get(), nonsecure_url_for_unset1);
  histograms.ExpectBucketCount(
      test_histogram,
      URLRequestHttpJob::CookieRequestScheme::kUnsetCookieScheme, 1);
  RunRequest(context.get(), secure_url_for_unset1);
  histograms.ExpectBucketCount(
      test_histogram,
      URLRequestHttpJob::CookieRequestScheme::kUnsetCookieScheme, 2);

  // Nonsecure set cookie marked as unset source scheme.
  GURL nonsecure_url_for_unset2("http://unset2.example:7");
  GURL secure_url_for_unset2("https://unset2.example:7");

  std::unique_ptr<CanonicalCookie> unset_cookie2 =
      CanonicalCookie::CreateForTesting(nonsecure_url_for_unset2,
                                        "NoSourceSchemeHttp=val",
                                        base::Time::Now());
  unset_cookie2->SetSourceScheme(net::CookieSourceScheme::kUnset);

  CookieList list2 = {*unset_cookie2};
  EXPECT_TRUE(SetAllCookies(cookie_store, list2));
  RunRequest(context.get(), nonsecure_url_for_unset2);
  histograms.ExpectBucketCount(
      test_histogram,
      URLRequestHttpJob::CookieRequestScheme::kUnsetCookieScheme, 3);
  RunRequest(context.get(), secure_url_for_unset2);
  histograms.ExpectBucketCount(
      test_histogram,
      URLRequestHttpJob::CookieRequestScheme::kUnsetCookieScheme, 4);

  // Secure set cookie with source scheme marked appropriately.
  GURL nonsecure_url_for_secure_set("http://secureset.example:7");
  GURL secure_url_for_secure_set("https://secureset.example:7");

  EXPECT_TRUE(CreateAndSetCookie(cookie_store, secure_url_for_secure_set,
                                 "SecureScheme=val"));
  RunRequest(context.get(), nonsecure_url_for_secure_set);
  histograms.ExpectBucketCount(
      test_histogram,
      URLRequestHttpJob::CookieRequestScheme::kSecureSetNonsecureRequest, 1);
  RunRequest(context.get(), secure_url_for_secure_set);
  histograms.ExpectBucketCount(
      test_histogram,
      URLRequestHttpJob::CookieRequestScheme::kSecureSetSecureRequest, 1);

  // Nonsecure set cookie with source scheme marked appropriately.
  GURL nonsecure_url_for_nonsecure_set("http://nonsecureset.example:7");
  GURL secure_url_for_nonsecure_set("https://nonsecureset.example:7");

  EXPECT_TRUE(CreateAndSetCookie(cookie_store, nonsecure_url_for_nonsecure_set,
                                 "NonSecureScheme=val"));
  RunRequest(context.get(), nonsecure_url_for_nonsecure_set);
  histograms.ExpectBucketCount(
      test_histogram,
      URLRequestHttpJob::CookieRequestScheme::kNonsecureSetNonsecureRequest, 1);
  RunRequest(context.get(), secure_url_for_nonsecure_set);
  histograms.ExpectBucketCount(
      test_histogram,
      URLRequestHttpJob::CookieRequestScheme::kNonsecureSetSecureRequest, 1);
}

// Test that cookies are annotated with the appropriate exclusion reason when
// privacy mode is enabled.
TEST_F(URLRequestHttpJobTest, PrivacyMode_ExclusionReason) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->SetCookieStore(std::make_unique<CookieMonster>(
      /*store=*/nullptr, /*net_log=*/nullptr));
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<FilteringTestNetworkDelegate>());
  auto context = context_builder->Build();

  // Set cookies.
  {
    TestDelegate d;
    GURL test_url = test_server.GetURL(
        "/set-cookie?one=1&"
        "two=2&"
        "three=3");
    std::unique_ptr<URLRequest> req =
        CreateFirstPartyRequest(*context, test_url, &d);
    req->Start();
    d.RunUntilComplete();
  }

  // Get cookies.
  network_delegate.ResetAnnotateCookiesCalledCount();
  ASSERT_EQ(0, network_delegate.annotate_cookies_called_count());
  // We want to fetch cookies from the cookie store, so we use the
  // NetworkDelegate to override the privacy mode (rather than setting it via
  // `allow_credentials`, since that skips querying the cookie store).
  network_delegate.set_force_privacy_mode(true);
  TestDelegate d;
  std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
      *context, test_server.GetURL("/echoheader?Cookie"), &d);
  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ("None", d.data_received());
  EXPECT_THAT(
      req->maybe_sent_cookies(),
      UnorderedElementsAre(
          MatchesCookieWithAccessResult(
              MatchesCookieWithNameSourceType("one", CookieSourceType::kHTTP),
              MatchesCookieAccessResult(
                  HasExactlyExclusionReasonsForTesting(
                      std::vector<CookieInclusionStatus::ExclusionReason>{
                          CookieInclusionStatus::EXCLUDE_USER_PREFERENCES}),
                  _, _, _)),
          MatchesCookieWithAccessResult(
              MatchesCookieWithNameSourceType("two", CookieSourceType::kHTTP),
              MatchesCookieAccessResult(
                  HasExactlyExclusionReasonsForTesting(
                      std::vector<CookieInclusionStatus::ExclusionReason>{
                          CookieInclusionStatus::EXCLUDE_USER_PREFERENCES}),
                  _, _, _)),
          MatchesCookieWithAccessResult(
              MatchesCookieWithNameSourceType("three", CookieSourceType::kHTTP),
              MatchesCookieAccessResult(
                  HasExactlyExclusionReasonsForTesting(
                      std::vector<CookieInclusionStatus::ExclusionReason>{
                          CookieInclusionStatus::EXCLUDE_USER_PREFERENCES}),
                  _, _, _))));

  EXPECT_EQ(0, network_delegate.annotate_cookies_called_count());
}

// Test that cookies are allowed to be selectively blocked by the network
// delegate.
TEST_F(URLRequestHttpJobTest, IndividuallyBlockedCookies) {
  HttpTestServer test_server;
  ASSERT_TRUE(test_server.Start());

  auto network_delegate = std::make_unique<FilteringTestNetworkDelegate>();
  network_delegate->set_block_get_cookies_by_name(true);
  network_delegate->SetCookieFilter("blocked_");
  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->SetCookieStore(std::make_unique<CookieMonster>(
      /*store=*/nullptr, /*net_log=*/nullptr));
  context_builder->set_network_delegate(std::move(network_delegate));
  auto context = context_builder->Build();

  // Set cookies.
  {
    TestDelegate d;
    GURL test_url = test_server.GetURL(
        "/set-cookie?blocked_one=1;SameSite=Lax;Secure&"
        "blocked_two=1;SameSite=Lax;Secure&"
        "allowed=1;SameSite=Lax;Secure");
    std::unique_ptr<URLRequest> req =
        CreateFirstPartyRequest(*context, test_url, &d);
    req->Start();
    d.RunUntilComplete();
  }

  // Get cookies.
  TestDelegate d;
  std::unique_ptr<URLRequest> req = CreateFirstPartyRequest(
      *context, test_server.GetURL("/echoheader?Cookie"), &d);
  req->Start();
  d.RunUntilComplete();

  EXPECT_EQ("allowed=1", d.data_received());
  EXPECT_THAT(
      req->maybe_sent_cookies(),
      UnorderedElementsAre(
          MatchesCookieWithAccessResult(
              MatchesCookieWithNameSourceType("blocked_one",
                                              CookieSourceType::kHTTP),
              MatchesCookieAccessResult(
                  HasExactlyExclusionReasonsForTesting(
                      std::vector<CookieInclusionStatus::ExclusionReason>{
                          CookieInclusionStatus::EXCLUDE_USER_PREFERENCES}),
                  _, _, _)),
          MatchesCookieWithAccessResult(
              MatchesCookieWithNameSourceType("blocked_two",
                                              CookieSourceType::kHTTP),
              MatchesCookieAccessResult(
                  HasExactlyExclusionReasonsForTesting(
                      std::vector<CookieInclusionStatus::ExclusionReason>{
                          CookieInclusionStatus::EXCLUDE_USER_PREFERENCES}),
                  _, _, _)),
          MatchesCookieWithAccessResult(
              MatchesCookieWithNameSourceType("allowed",
                                              CookieSourceType::kHTTP),
              MatchesCookieAccessResult(IsInclude(), _, _, _))));
}

namespace {

int content_count = 0;
std::unique_ptr<test_server::HttpResponse> IncreaseOnRequest(
    const test_server::HttpRequest& request) {
  auto http_response = std::make_unique<test_server::BasicHttpResponse>();
  http_response->set_content(base::NumberToString(content_count));
  content_count++;
  return std::move(http_response);
}

void ResetContentCount() {
  content_count = 0;
}

}  // namespace

TEST_F(URLRequestHttpJobTest, GetFirstPartySetsCacheFilterMatchInfo) {
  EmbeddedTestServer https_test(EmbeddedTestServer::TYPE_HTTPS);
  https_test.AddDefaultHandlers(base::FilePath());
  https_test.RegisterRequestHandler(base::BindRepeating(&IncreaseOnRequest));
  ASSERT_TRUE(https_test.Start());

  auto context_builder = CreateTestURLRequestContextBuilder();
  auto cookie_access_delegate = std::make_unique<TestCookieAccessDelegate>();
  TestCookieAccessDelegate* raw_cookie_access_delegate =
      cookie_access_delegate.get();
  auto cm = std::make_unique<CookieMonster>(nullptr, nullptr);
  cm->SetCookieAccessDelegate(std::move(cookie_access_delegate));
  context_builder->SetCookieStore(std::move(cm));
  auto context = context_builder->Build();

  const GURL kTestUrl = https_test.GetURL("/");
  const IsolationInfo kTestIsolationInfo =
      IsolationInfo::CreateForInternalRequest(url::Origin::Create(kTestUrl));
  {
    TestDelegate delegate;
    std::unique_ptr<URLRequest> req(context->CreateRequest(
        kTestUrl, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(kTestIsolationInfo);
    req->set_allow_credentials(false);
    req->Start();
    delegate.RunUntilComplete();
    EXPECT_EQ("0", delegate.data_received());
  }
  {  // Test using the cached response.
    TestDelegate delegate;
    std::unique_ptr<URLRequest> req(context->CreateRequest(
        kTestUrl, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->SetLoadFlags(LOAD_SKIP_CACHE_VALIDATION);
    req->set_allow_credentials(false);
    req->set_isolation_info(kTestIsolationInfo);
    req->Start();
    delegate.RunUntilComplete();
    EXPECT_EQ("0", delegate.data_received());
  }

  // Set cache filter and test cache is bypassed because the request site has a
  // matched entry in the filter and its response cache was stored before being
  // marked to clear.
  const int64_t kClearAtRunId = 3;
  const int64_t kBrowserRunId = 3;
  FirstPartySetsCacheFilter cache_filter(
      {{SchemefulSite(kTestUrl), kClearAtRunId}}, kBrowserRunId);
  raw_cookie_access_delegate->set_first_party_sets_cache_filter(
      std::move(cache_filter));
  {
    TestDelegate delegate;
    std::unique_ptr<URLRequest> req(context->CreateRequest(
        kTestUrl, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->SetLoadFlags(LOAD_SKIP_CACHE_VALIDATION);
    req->set_allow_credentials(false);
    req->set_isolation_info(kTestIsolationInfo);
    req->Start();
    delegate.RunUntilComplete();
    EXPECT_EQ("1", delegate.data_received());
  }

  ResetContentCount();
}

TEST_F(URLRequestHttpJobTest, SetPartitionedCookie) {
  EmbeddedTestServer https_test(EmbeddedTestServer::TYPE_HTTPS);
  https_test.AddDefaultHandlers(base::FilePath());
  ASSERT_TRUE(https_test.Start());

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->SetCookieStore(std::make_unique<CookieMonster>(
      /*store=*/nullptr, /*net_log=*/nullptr));
  auto context = context_builder->Build();

  const url::Origin kTopFrameOrigin =
      url::Origin::Create(GURL("https://www.toplevelsite.com"));
  const IsolationInfo kTestIsolationInfo =
      IsolationInfo::CreateForInternalRequest(kTopFrameOrigin);

  {
    TestDelegate delegate;
    std::unique_ptr<URLRequest> req(context->CreateRequest(
        https_test.GetURL(
            "/set-cookie?__Host-foo=bar;SameSite=None;Secure;Path=/"
            ";Partitioned;"),
        DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));

    req->set_isolation_info(kTestIsolationInfo);
    req->Start();
    ASSERT_TRUE(req->is_pending());
    delegate.RunUntilComplete();
  }

  {  // Test request from the same top-level site.
    TestDelegate delegate;
    std::unique_ptr<URLRequest> req(context->CreateRequest(
        https_test.GetURL("/echoheader?Cookie"), DEFAULT_PRIORITY, &delegate,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(kTestIsolationInfo);
    req->Start();
    delegate.RunUntilComplete();
    EXPECT_EQ("__Host-foo=bar", delegate.data_received());
  }

  {  // Test request from a different top-level site.
    const url::Origin kOtherTopFrameOrigin =
        url::Origin::Create(GURL("https://www.anothertoplevelsite.com"));
    const IsolationInfo kOtherTestIsolationInfo =
        IsolationInfo::CreateForInternalRequest(kOtherTopFrameOrigin);

    TestDelegate delegate;
    std::unique_ptr<URLRequest> req(context->CreateRequest(
        https_test.GetURL("/echoheader?Cookie"), DEFAULT_PRIORITY, &delegate,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(kOtherTestIsolationInfo);
    req->Start();
    delegate.RunUntilComplete();
    EXPECT_EQ("None", delegate.data_received());
  }

  {  // Test request from same top-level eTLD+1 but different scheme. Note that
     // although the top-level site is insecure, the endpoint setting/receiving
     // the cookie is always secure.
    const url::Origin kHttpTopFrameOrigin =
        url::Origin::Create(GURL("http://www.toplevelsite.com"));
    const IsolationInfo kHttpTestIsolationInfo =
        IsolationInfo::CreateForInternalRequest(kHttpTopFrameOrigin);

    TestDelegate delegate;
    std::unique_ptr<URLRequest> req(context->CreateRequest(
        https_test.GetURL("/echoheader?Cookie"), DEFAULT_PRIORITY, &delegate,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(kHttpTestIsolationInfo);
    req->Start();
    delegate.RunUntilComplete();
    EXPECT_EQ("None", delegate.data_received());
  }
}

TEST_F(URLRequestHttpJobTest, PartitionedCookiePrivacyMode) {
  EmbeddedTestServer https_test(EmbeddedTestServer::TYPE_HTTPS);
  https_test.AddDefaultHandlers(base::FilePath());
  ASSERT_TRUE(https_test.Start());

  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->SetCookieStore(
      std::make_unique<CookieMonster>(/*store=*/nullptr, /*net_log=*/nullptr));
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<FilteringTestNetworkDelegate>());
  auto context = context_builder->Build();

  const url::Origin kTopFrameOrigin =
      url::Origin::Create(GURL("https://www.toplevelsite.com"));
  const IsolationInfo kTestIsolationInfo =
      IsolationInfo::CreateForInternalRequest(kTopFrameOrigin);

  {
    // Set an unpartitioned and partitioned cookie.
    TestDelegate delegate;
    std::unique_ptr<URLRequest> req(context->CreateRequest(
        https_test.GetURL(
            "/set-cookie?__Host-partitioned=0;SameSite=None;Secure;Path=/"
            ";Partitioned;&__Host-unpartitioned=1;SameSite=None;Secure;Path=/"),
        DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(kTestIsolationInfo);
    req->Start();
    ASSERT_TRUE(req->is_pending());
    delegate.RunUntilComplete();
  }

  {  // Get both cookies when privacy mode is disabled.
    TestDelegate delegate;
    std::unique_ptr<URLRequest> req(context->CreateRequest(
        https_test.GetURL("/echoheader?Cookie"), DEFAULT_PRIORITY, &delegate,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(kTestIsolationInfo);
    req->Start();
    delegate.RunUntilComplete();
    EXPECT_EQ("__Host-partitioned=0; __Host-unpartitioned=1",
              delegate.data_received());
  }

  {  // Get cookies with privacy mode enabled and partitioned state allowed.
    network_delegate.set_force_privacy_mode(true);
    network_delegate.set_partitioned_state_allowed(true);
    network_delegate.SetCookieFilter("unpartitioned");
    network_delegate.set_block_get_cookies_by_name(true);
    TestDelegate delegate;
    std::unique_ptr<URLRequest> req(context->CreateRequest(
        https_test.GetURL("/echoheader?Cookie"), DEFAULT_PRIORITY, &delegate,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(kTestIsolationInfo);
    req->Start();
    delegate.RunUntilComplete();
    EXPECT_EQ("__Host-partitioned=0", delegate.data_received());
    auto want_exclusion_reasons =
        std::vector<CookieInclusionStatus::ExclusionReason>{};

    EXPECT_THAT(
        req->maybe_sent_cookies(),
        UnorderedElementsAre(
            MatchesCookieWithAccessResult(
                MatchesCookieWithNameSourceType("__Host-partitioned",
                                                CookieSourceType::kHTTP),
                MatchesCookieAccessResult(HasExactlyExclusionReasonsForTesting(
                                              want_exclusion_reasons),
                                          _, _, _)),
            MatchesCookieWithAccessResult(
                MatchesCookieWithNameSourceType("__Host-unpartitioned",
                                                CookieSourceType::kHTTP),
                MatchesCookieAccessResult(
                    HasExactlyExclusionReasonsForTesting(
                        std::vector<CookieInclusionStatus::ExclusionReason>{
                            CookieInclusionStatus::EXCLUDE_USER_PREFERENCES}),
                    _, _, _))));
  }

  {  // Get cookies with privacy mode enabled and partitioned state is not
     // allowed.
    network_delegate.set_force_privacy_mode(true);
    network_delegate.set_partitioned_state_allowed(false);
    TestDelegate delegate;
    std::unique_ptr<URLRequest> req(context->CreateRequest(
        https_test.GetURL("/echoheader?Cookie"), DEFAULT_PRIORITY, &delegate,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    req->set_isolation_info(kTestIsolationInfo);
    req->Start();
    delegate.RunUntilComplete();
    EXPECT_EQ("None", delegate.data_received());
    EXPECT_THAT(
        req->maybe_sent_cookies(),
        UnorderedElementsAre(
            MatchesCookieWithAccessResult(
                MatchesCookieWithNameSourceType("__Host-partitioned",
                                                CookieSourceType::kHTTP),
                MatchesCookieAccessResult(
                    HasExactlyExclusionReasonsForTesting(
                        std::vector<CookieInclusionStatus::ExclusionReason>{
                            CookieInclusionStatus::EXCLUDE_USER_PREFERENCES}),
                    _, _, _)),
            MatchesCookieWithAccessResult(
                MatchesCookieWithNameSourceType("__Host-unpartitioned",
                                                CookieSourceType::kHTTP),
                MatchesCookieAccessResult(
                    HasExactlyExclusionReasonsForTesting(
                        std::vector<CookieInclusionStatus::ExclusionReason>{
                            CookieInclusionStatus::EXCLUDE_USER_PREFERENCES}),
                    _, _, _))));
  }
}

}  // namespace net
