// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_http_job.h"

#include <stdint.h>

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/test/metrics/histogram_tester.h"
#include "net/base/auth.h"
#include "net/base/request_priority.h"
#include "net/cert/ct_policy_status.h"
#include "net/cookies/cookie_store_test_helpers.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_transaction_test_util.h"
#include "net/log/net_log_event_type.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_entry.h"
#include "net/log/test_net_log_util.h"
#include "net/net_buildflags.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_test_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_scoped_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "net/url_request/websocket_handshake_userdata_key.h"
#include "net/websockets/websocket_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "jni/AndroidNetworkLibraryTestUtil_jni.h"
#endif

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

using ::testing::Return;

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

const char kCTComplianceHistogramName[] =
    "Net.CertificateTransparency.RequestComplianceStatus";
const char kCTRequiredHistogramName[] =
    "Net.CertificateTransparency.CTRequiredRequestComplianceStatus";

// Inherit from URLRequestHttpJob to expose the priority and some
// other hidden functions.
class TestURLRequestHttpJob : public URLRequestHttpJob {
 public:
  explicit TestURLRequestHttpJob(URLRequest* request)
      : URLRequestHttpJob(request,
                          request->context()->network_delegate(),
                          request->context()->http_user_agent_settings()),
        use_null_source_stream_(false) {}

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
  bool use_null_source_stream_;

  DISALLOW_COPY_AND_ASSIGN(TestURLRequestHttpJob);
};

class URLRequestHttpJobSetUpSourceTest : public TestWithScopedTaskEnvironment {
 public:
  URLRequestHttpJobSetUpSourceTest() : context_(true) {
    test_job_interceptor_ = new TestJobInterceptor();
    EXPECT_TRUE(test_job_factory_.SetProtocolHandler(
        url::kHttpScheme, base::WrapUnique(test_job_interceptor_)));
    context_.set_job_factory(&test_job_factory_);
    context_.set_client_socket_factory(&socket_factory_);
    context_.Init();
  }

 protected:
  MockClientSocketFactory socket_factory_;
  // |test_job_interceptor_| is owned by |test_job_factory_|.
  TestJobInterceptor* test_job_interceptor_;
  URLRequestJobFactoryImpl test_job_factory_;

  TestURLRequestContext context_;
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
      context_.CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                             &delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
  auto job = std::make_unique<TestURLRequestHttpJob>(request.get());
  job->set_use_null_source_stream(true);
  test_job_interceptor_->set_main_intercept_job(std::move(job));
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
      context_.CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                             &delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
  auto job = std::make_unique<TestURLRequestHttpJob>(request.get());
  test_job_interceptor_->set_main_intercept_job(std::move(job));
  request->Start();

  delegate_.RunUntilComplete();
  EXPECT_EQ(OK, delegate_.request_status());
  EXPECT_EQ("Test Content", delegate_.data_received());
}

class URLRequestHttpJobWithProxy : public WithScopedTaskEnvironment {
 public:
  explicit URLRequestHttpJobWithProxy(
      std::unique_ptr<ProxyResolutionService> proxy_resolution_service)
      : proxy_resolution_service_(std::move(proxy_resolution_service)),
        context_(new TestURLRequestContext(true)) {
    context_->set_client_socket_factory(&socket_factory_);
    context_->set_network_delegate(&network_delegate_);
    context_->set_proxy_resolution_service(proxy_resolution_service_.get());
    context_->Init();
  }

  MockClientSocketFactory socket_factory_;
  TestNetworkDelegate network_delegate_;
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service_;
  std::unique_ptr<TestURLRequestContext> context_;

 private:
  DISALLOW_COPY_AND_ASSIGN(URLRequestHttpJobWithProxy);
};

// Tests that when proxy is not used, the proxy server is set correctly on the
// URLRequest.
TEST(URLRequestHttpJobWithProxy, TestFailureWithoutProxy) {
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
  EXPECT_EQ(ProxyServer::Direct(), request->proxy_server());
  EXPECT_FALSE(request->was_fetched_via_proxy());
  EXPECT_EQ(0, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes), request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), request->GetTotalReceivedBytes());
  EXPECT_EQ(CountWriteBytes(writes),
            http_job_with_proxy.network_delegate_.total_network_bytes_sent());
  EXPECT_EQ(
      CountReadBytes(reads),
      http_job_with_proxy.network_delegate_.total_network_bytes_received());
}

// Tests that when one proxy is in use and the connection to the proxy server
// fails, the proxy server is still set correctly on the URLRequest.
TEST(URLRequestHttpJobWithProxy, TestSuccessfulWithOneProxy) {
  const char kSimpleProxyGetMockWrite[] =
      "GET http://www.example.com/ HTTP/1.1\r\n"
      "Host: www.example.com\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "User-Agent: \r\n"
      "Accept-Encoding: gzip, deflate\r\n"
      "Accept-Language: en-us,fr\r\n\r\n";

  const ProxyServer proxy_server =
      ProxyServer::FromURI("http://origin.net:80", ProxyServer::SCHEME_HTTP);

  std::unique_ptr<ProxyResolutionService> proxy_resolution_service =
      ProxyResolutionService::CreateFixedFromPacResult(
          proxy_server.ToPacString(), TRAFFIC_ANNOTATION_FOR_TESTS);

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
  // When request fails due to proxy connection errors, the proxy server should
  // still be set on the |request|.
  EXPECT_EQ(proxy_server, request->proxy_server());
  EXPECT_FALSE(request->was_fetched_via_proxy());
  EXPECT_EQ(0, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes), request->GetTotalSentBytes());
  EXPECT_EQ(0, request->GetTotalReceivedBytes());
  EXPECT_EQ(CountWriteBytes(writes),
            http_job_with_proxy.network_delegate_.total_network_bytes_sent());
  EXPECT_EQ(
      0, http_job_with_proxy.network_delegate_.total_network_bytes_received());
}

// Tests that when two proxies are in use and the connection to the first proxy
// server fails, the proxy server is set correctly on the URLRequest.
TEST(URLRequestHttpJobWithProxy,
     TestContentLengthSuccessfulRequestWithTwoProxies) {
  const ProxyServer proxy_server =
      ProxyServer::FromURI("http://origin.net:80", ProxyServer::SCHEME_HTTP);

  // Connection to |proxy_server| would fail. Request should be fetched over
  // DIRECT.
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service =
      ProxyResolutionService::CreateFixedFromPacResult(
          proxy_server.ToPacString() + "; " +
              ProxyServer::Direct().ToPacString(),
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
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(ProxyServer::Direct(), request->proxy_server());
  EXPECT_FALSE(request->was_fetched_via_proxy());
  EXPECT_EQ(12, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes), request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), request->GetTotalReceivedBytes());
  EXPECT_EQ(CountWriteBytes(writes),
            http_job_with_proxy.network_delegate_.total_network_bytes_sent());
  EXPECT_EQ(
      CountReadBytes(reads),
      http_job_with_proxy.network_delegate_.total_network_bytes_received());
}

class URLRequestHttpJobTest : public TestWithScopedTaskEnvironment {
 protected:
  URLRequestHttpJobTest() : context_(true) {
    context_.set_http_transaction_factory(&network_layer_);

    // The |test_job_factory_| takes ownership of the interceptor.
    test_job_interceptor_ = new TestJobInterceptor();
    EXPECT_TRUE(test_job_factory_.SetProtocolHandler(
        url::kHttpScheme, base::WrapUnique(test_job_interceptor_)));
    context_.set_job_factory(&test_job_factory_);
    context_.set_net_log(&net_log_);
    context_.Init();

    req_ =
        context_.CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                               &delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  MockNetworkLayer network_layer_;

  // |test_job_interceptor_| is owned by |test_job_factory_|.
  TestJobInterceptor* test_job_interceptor_;
  URLRequestJobFactoryImpl test_job_factory_;

  TestURLRequestContext context_;
  TestDelegate delegate_;
  TestNetLog net_log_;
  std::unique_ptr<URLRequest> req_;
};

class URLRequestHttpJobWithMockSocketsTest
    : public TestWithScopedTaskEnvironment {
 protected:
  URLRequestHttpJobWithMockSocketsTest()
      : context_(new TestURLRequestContext(true)) {
    context_->set_client_socket_factory(&socket_factory_);
    context_->set_network_delegate(&network_delegate_);
    context_->Init();
  }

  MockClientSocketFactory socket_factory_;
  TestNetworkDelegate network_delegate_;
  std::unique_ptr<TestURLRequestContext> context_;
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
  EXPECT_EQ(CountWriteBytes(writes),
            network_delegate_.total_network_bytes_sent());
  EXPECT_EQ(CountReadBytes(reads),
            network_delegate_.total_network_bytes_received());
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
  EXPECT_EQ(CountWriteBytes(writes),
            network_delegate_.total_network_bytes_sent());
  EXPECT_EQ(CountReadBytes(reads),
            network_delegate_.total_network_bytes_received());
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
  EXPECT_EQ(CountWriteBytes(writes),
            network_delegate_.total_network_bytes_sent());
  EXPECT_EQ(CountReadBytes(reads) - 12,
            network_delegate_.total_network_bytes_received());
}

TEST_F(URLRequestHttpJobWithMockSocketsTest, TestSuccessfulCachedHeadRequest) {
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

    request->Start();
    ASSERT_TRUE(request->is_pending());
    delegate.RunUntilComplete();

    EXPECT_THAT(delegate.request_status(), IsOk());
    EXPECT_EQ(12, request->received_response_content_length());
    EXPECT_EQ(CountWriteBytes(writes), request->GetTotalSentBytes());
    EXPECT_EQ(CountReadBytes(reads), request->GetTotalReceivedBytes());
    EXPECT_EQ(CountWriteBytes(writes),
              network_delegate_.total_network_bytes_sent());
    EXPECT_EQ(CountReadBytes(reads),
              network_delegate_.total_network_bytes_received());
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
  EXPECT_EQ(CountWriteBytes(writes),
            network_delegate_.total_network_bytes_sent());
  EXPECT_EQ(CountReadBytes(reads),
            network_delegate_.total_network_bytes_received());
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
  EXPECT_EQ(CountWriteBytes(writes),
            network_delegate_.total_network_bytes_sent());
  EXPECT_EQ(CountReadBytes(reads),
            network_delegate_.total_network_bytes_received());
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
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(delegate.request_status(), IsError(ERR_ABORTED));
  EXPECT_EQ(12, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes), request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), request->GetTotalReceivedBytes());
  EXPECT_EQ(CountWriteBytes(writes),
            network_delegate_.total_network_bytes_sent());
  EXPECT_EQ(CountReadBytes(reads),
            network_delegate_.total_network_bytes_received());
}

TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestRawHeaderSizeSuccessfullRequest) {
  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};

  const std::string& response_header =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 12\r\n\r\n";
  const std::string& content_data = "Test Content";

  MockRead reads[] = {MockRead(response_header.c_str()),
                      MockRead(content_data.c_str()),
                      MockRead(net::SYNCHRONOUS, net::OK)};

  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  delegate.RunUntilComplete();

  EXPECT_EQ(net::OK, request->status().error());
  EXPECT_EQ(static_cast<int>(content_data.size()),
            request->received_response_content_length());
  EXPECT_EQ(static_cast<int>(response_header.size()),
            request->raw_header_size());
  EXPECT_EQ(CountReadBytes(reads), request->GetTotalReceivedBytes());
}

TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestRawHeaderSizeSuccessfull100ContinueRequest) {
  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};

  const std::string& continue_header = "HTTP/1.1 100 Continue\r\n\r\n";
  const std::string& response_header =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 12\r\n\r\n";
  const std::string& content_data = "Test Content";

  MockRead reads[] = {
      MockRead(continue_header.c_str()), MockRead(response_header.c_str()),
      MockRead(content_data.c_str()), MockRead(net::SYNCHRONOUS, net::OK)};

  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  delegate.RunUntilComplete();

  EXPECT_EQ(net::OK, request->status().error());
  EXPECT_EQ(static_cast<int>(content_data.size()),
            request->received_response_content_length());
  EXPECT_EQ(static_cast<int>(continue_header.size() + response_header.size()),
            request->raw_header_size());
  EXPECT_EQ(CountReadBytes(reads), request->GetTotalReceivedBytes());
}

TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestRawHeaderSizeFailureTruncatedHeaders) {
  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.0 200 OK\r\n"
                               "Content-Len"),
                      MockRead(net::SYNCHRONOUS, net::OK)};

  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  delegate.set_cancel_in_response_started(true);
  request->Start();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ERR_ABORTED, request->status().error());
  EXPECT_EQ(0, request->received_response_content_length());
  EXPECT_EQ(28, request->raw_header_size());
  EXPECT_EQ(CountReadBytes(reads), request->GetTotalReceivedBytes());
}

TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestRawHeaderSizeSuccessfullContinuiousRead) {
  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  const std::string& header_data =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 12\r\n\r\n";
  const std::string& content_data = "Test Content";
  std::string single_read_content = header_data;
  single_read_content.append(content_data);
  MockRead reads[] = {MockRead(single_read_content.c_str())};

  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context_->CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
                              &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  delegate.RunUntilComplete();

  EXPECT_EQ(net::OK, request->status().error());
  EXPECT_EQ(static_cast<int>(content_data.size()),
            request->received_response_content_length());
  EXPECT_EQ(static_cast<int>(header_data.size()), request->raw_header_size());
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
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(12, request->received_response_content_length());
  // Should not include the redirect.
  EXPECT_EQ(CountWriteBytes(final_writes), request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(final_reads), request->GetTotalReceivedBytes());
  // Should include the redirect as well as the final response.
  EXPECT_EQ(CountWriteBytes(redirect_writes) + CountWriteBytes(final_writes),
            network_delegate_.total_network_bytes_sent());
  EXPECT_EQ(CountReadBytes(redirect_reads) + CountReadBytes(final_reads),
            network_delegate_.total_network_bytes_received());
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
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(delegate.request_status(), IsError(ERR_ABORTED));
  EXPECT_EQ(0, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes), request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), request->GetTotalReceivedBytes());
  EXPECT_EQ(CountWriteBytes(writes),
            network_delegate_.total_network_bytes_sent());
  EXPECT_EQ(CountReadBytes(reads),
            network_delegate_.total_network_bytes_received());
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
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(delegate.request_status(), IsError(ERR_ABORTED));
  EXPECT_EQ(0, request->received_response_content_length());
  EXPECT_EQ(0, request->GetTotalSentBytes());
  EXPECT_EQ(0, request->GetTotalReceivedBytes());
  EXPECT_EQ(0, network_delegate_.total_network_bytes_received());
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
    histograms.ExpectTotalCount(
        "Net.HttpJob.TotalTimeSuccess.Priority" + base::IntToString(priority),
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

// Tests that the CT compliance histogram is recorded, even if CT is not
// required.
TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestHttpJobRecordsCTComplianceHistograms) {
  SSLSocketDataProvider ssl_socket_data(net::ASYNC, net::OK);
  ssl_socket_data.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  ssl_socket_data.ssl_info.is_issued_by_known_root = true;
  ssl_socket_data.ssl_info.ct_policy_compliance_required = false;
  ssl_socket_data.ssl_info.ct_policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS;

  socket_factory_.AddSSLSocketDataProvider(&ssl_socket_data);

  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};
  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);

  base::HistogramTester histograms;

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request = context_->CreateRequest(
      GURL("https://www.example.com/"), DEFAULT_PRIORITY, &delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS);
  request->Start();
  delegate.RunUntilComplete();
  EXPECT_THAT(delegate.request_status(), IsOk());

  histograms.ExpectUniqueSample(
      kCTComplianceHistogramName,
      static_cast<int32_t>(ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS),
      1);
  // CTRequiredRequestComplianceStatus should *not* have been recorded because
  // it is only recorded for requests which are required to be compliant.
  histograms.ExpectTotalCount(kCTRequiredHistogramName, 0);
}

// Tests that the CT compliance histograms are not recorded for
// locally-installed trust anchors.
TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestHttpJobDoesNotRecordCTComplianceHistogramsForLocalRoot) {
  SSLSocketDataProvider ssl_socket_data(net::ASYNC, net::OK);
  ssl_socket_data.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  ssl_socket_data.ssl_info.is_issued_by_known_root = false;
  ssl_socket_data.ssl_info.ct_policy_compliance_required = false;
  ssl_socket_data.ssl_info.ct_policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS;

  socket_factory_.AddSSLSocketDataProvider(&ssl_socket_data);

  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};
  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);

  base::HistogramTester histograms;

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request = context_->CreateRequest(
      GURL("https://www.example.com/"), DEFAULT_PRIORITY, &delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS);
  request->Start();
  delegate.RunUntilComplete();
  EXPECT_THAT(delegate.request_status(), IsOk());

  histograms.ExpectTotalCount(kCTComplianceHistogramName, 0);
  histograms.ExpectTotalCount(kCTRequiredHistogramName, 0);
}

// Tests that the CT compliance histogram is recorded when CT is required but
// not compliant.
TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestHttpJobRecordsCTRequiredHistogram) {
  SSLSocketDataProvider ssl_socket_data(net::ASYNC, net::OK);
  ssl_socket_data.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  ssl_socket_data.ssl_info.is_issued_by_known_root = true;
  ssl_socket_data.ssl_info.ct_policy_compliance_required = true;
  ssl_socket_data.ssl_info.ct_policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS;

  socket_factory_.AddSSLSocketDataProvider(&ssl_socket_data);

  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};
  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);

  base::HistogramTester histograms;

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request = context_->CreateRequest(
      GURL("https://www.example.com/"), DEFAULT_PRIORITY, &delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS);
  request->Start();
  delegate.RunUntilComplete();
  EXPECT_THAT(delegate.request_status(), IsOk());

  histograms.ExpectUniqueSample(
      kCTComplianceHistogramName,
      static_cast<int32_t>(ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS),
      1);
  histograms.ExpectUniqueSample(
      kCTRequiredHistogramName,
      static_cast<int32_t>(ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS),
      1);
}

// Tests that the CT compliance histograms are not recorded when there is an
// unrelated certificate error.
TEST_F(URLRequestHttpJobWithMockSocketsTest,
       TestHttpJobDoesNotRecordCTHistogramWithCertError) {
  SSLSocketDataProvider ssl_socket_data(net::ASYNC, net::OK);
  ssl_socket_data.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  ssl_socket_data.ssl_info.is_issued_by_known_root = true;
  ssl_socket_data.ssl_info.ct_policy_compliance_required = true;
  ssl_socket_data.ssl_info.ct_policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS;
  ssl_socket_data.ssl_info.cert_status = net::CERT_STATUS_DATE_INVALID;

  socket_factory_.AddSSLSocketDataProvider(&ssl_socket_data);

  MockWrite writes[] = {MockWrite(kSimpleGetMockWrite)};
  MockRead reads[] = {MockRead("HTTP/1.1 200 OK\r\n"
                               "Content-Length: 12\r\n\r\n"),
                      MockRead("Test Content")};
  StaticSocketDataProvider socket_data(reads, writes);
  socket_factory_.AddSocketDataProvider(&socket_data);

  base::HistogramTester histograms;

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request = context_->CreateRequest(
      GURL("https://www.example.com/"), DEFAULT_PRIORITY, &delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS);
  request->Start();
  delegate.RunUntilComplete();
  EXPECT_THAT(delegate.request_status(), IsOk());

  histograms.ExpectTotalCount(kCTComplianceHistogramName, 0);
  histograms.ExpectTotalCount(kCTRequiredHistogramName, 0);
}

TEST_F(URLRequestHttpJobTest, TestCancelWhileReadingCookies) {
  DelayedCookieMonster cookie_monster;
  TestURLRequestContext context(true);
  context.set_cookie_store(&cookie_monster);
  context.Init();

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      context.CreateRequest(GURL("http://www.example.com"), DEFAULT_PRIORITY,
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
  test_job_interceptor_->set_main_intercept_job(
      std::make_unique<TestURLRequestHttpJob>(req_.get()));
  req_->SetPriority(LOW);

  EXPECT_FALSE(network_layer_.last_transaction());

  req_->Start();

  ASSERT_TRUE(network_layer_.last_transaction());
  EXPECT_EQ(LOW, network_layer_.last_transaction()->priority());
}

// Make sure that URLRequestHttpJob passes on its priority updates to
// its transaction.
TEST_F(URLRequestHttpJobTest, SetTransactionPriority) {
  test_job_interceptor_->set_main_intercept_job(
      std::make_unique<TestURLRequestHttpJob>(req_.get()));
  req_->SetPriority(LOW);
  req_->Start();
  ASSERT_TRUE(network_layer_.last_transaction());
  EXPECT_EQ(LOW, network_layer_.last_transaction()->priority());

  req_->SetPriority(HIGHEST);
  EXPECT_EQ(HIGHEST, network_layer_.last_transaction()->priority());
}

TEST_F(URLRequestHttpJobTest, HSTSInternalRedirectTest) {
  // Setup HSTS state.
  context_.transport_security_state()->AddHSTS(
      "upgrade.test", base::Time::Now() + base::TimeDelta::FromSeconds(10),
      true);
  ASSERT_TRUE(
      context_.transport_security_state()->ShouldUpgradeToSSL("upgrade.test"));
  ASSERT_FALSE(context_.transport_security_state()->ShouldUpgradeToSSL(
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
    TestDelegate d;
    TestNetworkDelegate network_delegate;
    std::unique_ptr<URLRequest> r(context_.CreateRequest(
        GURL(test.url), DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

    net_log_.Clear();
    r->Start();
    d.RunUntilComplete();

    if (test.upgrade_expected) {
      net::TestNetLogEntry::List entries;
      net_log_.GetEntries(&entries);
      int redirects = 0;
      for (const auto& entry : entries) {
        if (entry.type == net::NetLogEventType::URL_REQUEST_REDIRECT_JOB) {
          redirects++;
          std::string value;
          EXPECT_TRUE(entry.GetStringValue("reason", &value));
          EXPECT_EQ("HSTS", value);
        }
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

class URLRequestHttpJobWithBrotliSupportTest
    : public TestWithScopedTaskEnvironment {
 protected:
  URLRequestHttpJobWithBrotliSupportTest()
      : context_(new TestURLRequestContext(true)) {
    auto params = std::make_unique<HttpNetworkSession::Params>();
    context_->set_enable_brotli(true);
    context_->set_http_network_session_params(std::move(params));
    context_->set_client_socket_factory(&socket_factory_);
    context_->Init();
  }

  MockClientSocketFactory socket_factory_;
  std::unique_ptr<TestURLRequestContext> context_;
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
  base::RunLoop().RunUntilIdle();

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
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(12, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes), request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), request->GetTotalReceivedBytes());
}

#if defined(OS_ANDROID)
TEST_F(URLRequestHttpJobTest, AndroidCleartextPermittedTest) {
  context_.set_check_cleartext_permitted(true);

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
        context_.CreateRequest(GURL(test.url), DEFAULT_PRIORITY, &delegate,
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

class URLRequestHttpJobWebSocketTest : public TestWithScopedTaskEnvironment {
 protected:
  URLRequestHttpJobWebSocketTest() : context_(true) {
    context_.set_network_delegate(&network_delegate_);
    context_.set_client_socket_factory(&socket_factory_);
    context_.Init();
    req_ =
        context_.CreateRequest(GURL("ws://www.example.org"), DEFAULT_PRIORITY,
                               &delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  // A Network Delegate is required for the WebSocketHandshakeStreamBase
  // object to be passed on to the HttpNetworkTransaction.
  TestNetworkDelegate network_delegate_;

  TestURLRequestContext context_;
  MockClientSocketFactory socket_factory_;
  TestDelegate delegate_;
  std::unique_ptr<URLRequest> req_;
};

TEST_F(URLRequestHttpJobWebSocketTest, RejectedWithoutCreateHelper) {
  req_->Start();
  base::RunLoop().RunUntilIdle();
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
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(delegate_.request_status(), IsOk());
  EXPECT_TRUE(delegate_.response_completed());

  EXPECT_TRUE(data.AllWriteDataConsumed());
  EXPECT_TRUE(data.AllReadDataConsumed());
}

#endif  // BUILDFLAG(ENABLE_WEBSOCKETS)

}  // namespace

}  // namespace net
