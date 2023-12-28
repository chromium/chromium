// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert_net/cert_net_fetcher_url_request.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_server_properties.h"
#include "net/http/transport_security_state.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/quic/quic_context.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/test/url_request/url_request_hanging_read_job.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using net::test::IsOk;

// TODO(eroman): Test that cookies aren't sent.

namespace net {

namespace {

const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("net/data/cert_net_fetcher_impl_unittest");

const char kMockSecureDnsHostname[] = "mock.secure.dns.check";

// Wait for the request to complete, and verify that it completed successfully
// with the indicated bytes.
void VerifySuccess(const std::string& expected_body,
                   CertNetFetcher::Request* request) {
  Error actual_error;
  std::vector<uint8_t> actual_body;
  request->WaitForResult(&actual_error, &actual_body);

  EXPECT_THAT(actual_error, IsOk());
  EXPECT_EQ(expected_body, std::string(actual_body.begin(), actual_body.end()));
}

// Wait for the request to complete, and verify that it completed with the
// indicated failure.
void VerifyFailure(Error expected_error, CertNetFetcher::Request* request) {
  Error actual_error;
  std::vector<uint8_t> actual_body;
  request->WaitForResult(&actual_error, &actual_body);

  EXPECT_EQ(expected_error, actual_error);
  EXPECT_EQ(0u, actual_body.size());
}

struct NetworkThreadState {
  std::unique_ptr<URLRequestContext> context;
  // Owned by `context`.
  raw_ptr<TestNetworkDelegate> network_delegate;
};

class CertNetFetcherURLRequestTest : public PlatformTest {
 public:
  CertNetFetcherURLRequestTest() {
    test_server_.AddDefaultHandlers(base::FilePath(kDocRoot));
    StartNetworkThread();
  }

  ~CertNetFetcherURLRequestTest() override {
    if (!network_thread_)
      return;
    network_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&CertNetFetcherURLRequestTest::TeardownOnNetworkThread,
                       base::Unretained(this)));
    network_thread_->Stop();
  }

 protected:
  CertNetFetcher* fetcher() const { return fetcher_.get(); }

  void CreateFetcherOnNetworkThread(base::WaitableEvent* done) {
    fetcher_ = base::MakeRefCounted<CertNetFetcherURLRequest>();
    fetcher_->SetURLRequestContext(state_->context.get());
    done->Signal();
  }

  void CreateFetcher() {
    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    network_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &CertNetFetcherURLRequestTest::CreateFetcherOnNetworkThread,
            base::Unretained(this), &done));
    done.Wait();
  }

  void ShutDownFetcherOnNetworkThread(base::WaitableEvent* done) {
    fetcher_->Shutdown();
    done->Signal();
  }

  void ShutDownFetcher() {
    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    network_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &CertNetFetcherURLRequestTest::ShutDownFetcherOnNetworkThread,
            base::Unretained(this), &done));
    done.Wait();
  }

  int NumCreatedRequests() {
    int count = 0;
    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    network_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&CertNetFetcherURLRequestTest::CountCreatedRequests,
                       base::Unretained(this), &count, &done));
    done.Wait();
    return count;
  }

  void StartNetworkThread() {
    // Start the network thread.
    network_thread_ = std::make_unique<base::Thread>("network thread");
    base::Thread::Options options(base::MessagePumpType::IO, 0);
    EXPECT_TRUE(network_thread_->StartWithOptions(std::move(options)));

    // Initialize the URLRequestContext (and wait till it has completed).
    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    network_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&CertNetFetcherURLRequestTest::InitOnNetworkThread,
                       base::Unretained(this), &done));
    done.Wait();
  }

  void InitOnNetworkThread(base::WaitableEvent* done) {
    state_ = std::make_unique<NetworkThreadState>();
    auto builder = CreateTestURLRequestContextBuilder();
    state_->network_delegate =
        builder->set_network_delegate(std::make_unique<TestNetworkDelegate>());
    state_->context = builder->Build();
    done->Signal();
  }

  void ResetStateOnNetworkThread(base::WaitableEvent* done) {
    state_.reset();
    done->Signal();
  }

  void ResetState() {
    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    network_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&CertNetFetcherURLRequestTest::ResetStateOnNetworkThread,
                       base::Unretained(this), &done));
    done.Wait();
  }

  void TeardownOnNetworkThread() {
    fetcher_->Shutdown();
    state_.reset();
    fetcher_ = nullptr;
  }

  void CountCreatedRequests(int* count, base::WaitableEvent* done) {
    *count = state_->network_delegate->created_requests();
    done->Signal();
  }

  EmbeddedTestServer test_server_;
  std::unique_ptr<base::Thread> network_thread_;
  scoped_refptr<CertNetFetcherURLRequest> fetcher_;

  std::unique_ptr<NetworkThreadState> state_;
};

// Installs URLRequestHangingReadJob handlers and clears them on teardown.
class CertNetFetcherURLRequestTestWithHangingReadHandler
    : public CertNetFetcherURLRequestTest,
      public WithTaskEnvironment {
 protected:
  void SetUp() override { URLRequestHangingReadJob::AddUrlHandler(); }

  void TearDown() override { URLRequestFilter::GetInstance()->ClearHandlers(); }
};

// Interceptor to check that secure DNS has been disabled.
class SecureDnsInterceptor : public net::URLRequestInterceptor {
 public:
  explicit SecureDnsInterceptor(bool* invoked_interceptor)
      : invoked_interceptor_(invoked_interceptor) {}
  ~SecureDnsInterceptor() override = default;

 private:
  // URLRequestInterceptor implementation:
  std::unique_ptr<net::URLRequestJob> MaybeInterceptRequest(
      net::URLRequest* request) const override {
    EXPECT_EQ(SecureDnsPolicy::kDisable, request->secure_dns_policy());
    *invoked_interceptor_ = true;
    return nullptr;
  }

  raw_ptr<bool> invoked_interceptor_;
};

class CertNetFetcherURLRequestTestWithSecureDnsInterceptor
    : public CertNetFetcherURLRequestTest,
      public WithTaskEnvironment {
 public:
  CertNetFetcherURLRequestTestWithSecureDnsInterceptor() = default;

  void SetUp() override {
    URLRequestFilter::GetInstance()->AddHostnameInterceptor(
        "http", kMockSecureDnsHostname,
        std::make_unique<SecureDnsInterceptor>(&invoked_interceptor_));
  }

  void TearDown() override { URLRequestFilter::GetInstance()->ClearHandlers(); }

  bool invoked_interceptor() { return invoked_interceptor_; }

 private:
  bool invoked_interceptor_ = false;
};

// Helper to start an AIA fetch using default parameters.
[[nodiscard]] std::unique_ptr<CertNetFetcher::Request> StartRequest(
    CertNetFetcher* fetcher,
    const GURL& url) {
  return fetcher->FetchCaIssuers(url, CertNetFetcher::DEFAULT,
                                 CertNetFetcher::DEFAULT);
}

// Fetch a few unique URLs using GET in parallel. Each URL has a different body
// and Content-Type.
TEST_F(CertNetFetcherURLRequestTest, ParallelFetchNoDuplicates) {
  ASSERT_TRUE(test_server_.Start());
  CreateFetcher();

  // Request a URL with Content-Type "application/pkix-cert"
  GURL url1 = test_server_.GetURL("/cert.crt");
  std::unique_ptr<CertNetFetcher::Request> request1 =
      StartRequest(fetcher(), url1);

  // Request a URL with Content-Type "application/pkix-crl"
  GURL url2 = test_server_.GetURL("/root.crl");
  std::unique_ptr<CertNetFetcher::Request> request2 =
      StartRequest(fetcher(), url2);

  // Request a URL with Content-Type "application/pkcs7-mime"
  GURL url3 = test_server_.GetURL("/certs.p7c");
  std::unique_ptr<CertNetFetcher::Request> request3 =
      StartRequest(fetcher(), url3);

  // Wait for all of the requests to complete and verify the fetch results.
  VerifySuccess("-cert.crt-\n", request1.get());
  VerifySuccess("-root.crl-\n", request2.get());
  VerifySuccess("-certs.p7c-\n", request3.get());

  EXPECT_EQ(3, NumCreatedRequests());
}

// Fetch a caIssuers URL which has an unexpected extension and Content-Type.
// The extension is .txt and the Content-Type is text/plain. Despite being
// unusual this succeeds as the extension and Content-Type are not required to
// be meaningful.
TEST_F(CertNetFetcherURLRequestTest, ContentTypeDoesntMatter) {
  ASSERT_TRUE(test_server_.Start());
  CreateFetcher();

  GURL url = test_server_.GetURL("/foo.txt");
  std::unique_ptr<CertNetFetcher::Request> request =
      StartRequest(fetcher(), url);
  VerifySuccess("-foo.txt-\n", request.get());
}

// Fetch a URLs whose HTTP response code is not 200. These are considered
// failures.
TEST_F(CertNetFetcherURLRequestTest, HttpStatusCode) {
  ASSERT_TRUE(test_server_.Start());
  CreateFetcher();

  // Response was HTTP status 404.
  {
    GURL url = test_server_.GetURL("/404.html");
    std::unique_ptr<CertNetFetcher::Request> request =
        StartRequest(fetcher(), url);
    VerifyFailure(ERR_HTTP_RESPONSE_CODE_FAILURE, request.get());
  }

  // Response was HTTP status 500.
  {
    GURL url = test_server_.GetURL("/500.html");
    std::unique_ptr<CertNetFetcher::Request> request =
        StartRequest(fetcher(), url);
    VerifyFailure(ERR_HTTP_RESPONSE_CODE_FAILURE, request.get());
  }
}

// Fetching a URL with a Content-Disposition header should have no effect.
TEST_F(CertNetFetcherURLRequestTest, ContentDisposition) {
  ASSERT_TRUE(test_server_.Start());
  CreateFetcher();

  GURL url = test_server_.GetURL("/downloadable.js");
  std::unique_ptr<CertNetFetcher::Request> request =
      StartRequest(fetcher(), url);
  VerifySuccess("-downloadable.js-\n", request.get());
}

// Verifies that a cacheable request will be served from the HTTP cache the
// second time it is requested.
TEST_F(CertNetFetcherURLRequestTest, Cache) {
  ASSERT_TRUE(test_server_.Start());

  CreateFetcher();

  // Fetch a URL whose HTTP headers make it cacheable for 1 hour.
  GURL url(test_server_.GetURL("/cacheable_1hr.crt"));
  {
    std::unique_ptr<CertNetFetcher::Request> request =
        StartRequest(fetcher(), url);
    VerifySuccess("-cacheable_1hr.crt-\n", request.get());
  }

  EXPECT_EQ(1, NumCreatedRequests());

  // Kill the HTTP server.
  ASSERT_TRUE(test_server_.ShutdownAndWaitUntilComplete());

  // Fetch again -- will fail unless served from cache.
  {
    std::unique_ptr<CertNetFetcher::Request> request =
        StartRequest(fetcher(), url);
    VerifySuccess("-cacheable_1hr.crt-\n", request.get());
  }

  EXPECT_EQ(2, NumCreatedRequests());
}

// Verify that the maximum response body constraints are enforced by fetching a
// resource that is larger than the limit.
TEST_F(CertNetFetcherURLRequestTest, TooLarge) {
  ASSERT_TRUE(test_server_.Start());

  CreateFetcher();

  // This file has a response body 12 bytes long. So setting the maximum to 11
  // bytes will cause it to fail.
  GURL url(test_server_.GetURL("/certs.p7c"));
  std::unique_ptr<CertNetFetcher::Request> request =
      fetcher()->FetchCaIssuers(url, CertNetFetcher::DEFAULT, 11);

  VerifyFailure(ERR_FILE_TOO_BIG, request.get());
}

// Set the timeout to 10 milliseconds, and try fetching a URL that takes 5
// seconds to complete. It should fail due to a timeout.
TEST_F(CertNetFetcherURLRequestTest, Hang) {
  ASSERT_TRUE(test_server_.Start());

  CreateFetcher();

  GURL url(test_server_.GetURL("/slow/certs.p7c?5"));
  std::unique_ptr<CertNetFetcher::Request> request =
      fetcher()->FetchCaIssuers(url, 10, CertNetFetcher::DEFAULT);
  VerifyFailure(ERR_TIMED_OUT, request.get());
}

// Verify that if a response is gzip-encoded it gets inflated before being
// returned to the caller.
TEST_F(CertNetFetcherURLRequestTest, Gzip) {
  ASSERT_TRUE(test_server_.Start());

  CreateFetcher();

  GURL url(test_server_.GetURL("/gzipped_crl"));
  std::unique_ptr<CertNetFetcher::Request> request =
      StartRequest(fetcher(), url);
  VerifySuccess("-gzipped_crl-\n", request.get());
}

// Try fetching an unsupported URL scheme (https).
TEST_F(CertNetFetcherURLRequestTest, HttpsNotAllowed) {
  ASSERT_TRUE(test_server_.Start());

  CreateFetcher();

  GURL url("https://foopy/foo.crt");
  std::unique_ptr<CertNetFetcher::Request> request =
      StartRequest(fetcher(), url);
  VerifyFailure(ERR_DISALLOWED_URL_SCHEME, request.get());

  // No request was created because the URL scheme was unsupported.
  EXPECT_EQ(0, NumCreatedRequests());
}

// Try fetching a URL which redirects to https.
TEST_F(CertNetFetcherURLRequestTest, RedirectToHttpsNotAllowed) {
  ASSERT_TRUE(test_server_.Start());

  CreateFetcher();

  GURL url(test_server_.GetURL("/redirect_https"));

  std::unique_ptr<CertNetFetcher::Request> request =
      StartRequest(fetcher(), url);
  VerifyFailure(ERR_DISALLOWED_URL_SCHEME, request.get());

  EXPECT_EQ(1, NumCreatedRequests());
}

// Try fetching an unsupported URL scheme (https) and then immediately
// cancelling. This is a bit special because this codepath needs to post a task.
TEST_F(CertNetFetcherURLRequestTest, CancelHttpsNotAllowed) {
  ASSERT_TRUE(test_server_.Start());

  CreateFetcher();

  GURL url("https://foopy/foo.crt");
  std::unique_ptr<CertNetFetcher::Request> request =
      StartRequest(fetcher(), url);

  // Cancel the request (May or may not have started yet, as the request is
  // running on another thread).
  request.reset();
}

// Start a few requests, and cancel one of them before running the message loop
// again.
TEST_F(CertNetFetcherURLRequestTest, CancelBeforeRunningMessageLoop) {
  ASSERT_TRUE(test_server_.Start());

  CreateFetcher();

  GURL url1 = test_server_.GetURL("/cert.crt");
  std::unique_ptr<CertNetFetcher::Request> request1 =
      StartRequest(fetcher(), url1);

  GURL url2 = test_server_.GetURL("/root.crl");
  std::unique_ptr<CertNetFetcher::Request> request2 =
      StartRequest(fetcher(), url2);

  GURL url3 = test_server_.GetURL("/certs.p7c");

  std::unique_ptr<CertNetFetcher::Request> request3 =
      StartRequest(fetcher(), url3);

  // Cancel the second request.
  request2.reset();

  // Wait for the non-cancelled requests to complete, and verify the fetch
  // results.
  VerifySuccess("-cert.crt-\n", request1.get());
  VerifySuccess("-certs.p7c-\n", request3.get());
}

// Start several requests, and cancel one of them after the first has completed.
// NOTE: The python test server is single threaded and can only service one
// request at a time. After a socket is opened by the server it waits for it to
// be completed, and any subsequent request will hang until the first socket is
// closed.
// Cancelling the first request can therefore be problematic, since if
// cancellation is done after the socket is opened but before reading/writing,
// then the socket is re-cycled and things will be stalled until the cleanup
// timer (10 seconds) closes it.
// To work around this, the last request is cancelled, and hope that the
// requests are given opened sockets in a FIFO order.
// TODO(eroman): Make this more robust.
// TODO(eroman): Rename this test.
TEST_F(CertNetFetcherURLRequestTest, CancelAfterRunningMessageLoop) {
  ASSERT_TRUE(test_server_.Start());

  CreateFetcher();

  GURL url1 = test_server_.GetURL("/cert.crt");

  std::unique_ptr<CertNetFetcher::Request> request1 =
      StartRequest(fetcher(), url1);

  GURL url2 = test_server_.GetURL("/certs.p7c");
  std::unique_ptr<CertNetFetcher::Request> request2 =
      StartRequest(fetcher(), url2);

  GURL url3("ftp://www.not.supported.com/foo");
  std::unique_ptr<CertNetFetcher::Request> request3 =
      StartRequest(fetcher(), url3);

  // Wait for the ftp request to complete (it should complete right away since
  // it doesn't even try to connect to the server).
  VerifyFailure(ERR_DISALLOWED_URL_SCHEME, request3.get());

  // Cancel the second outstanding request.
  request2.reset();

  // Wait for the first request to complete and verify the fetch result.
  VerifySuccess("-cert.crt-\n", request1.get());
}

// Fetch the same URLs in parallel and verify that only 1 request is made per
// URL.
TEST_F(CertNetFetcherURLRequestTest, ParallelFetchDuplicates) {
  ASSERT_TRUE(test_server_.Start());

  CreateFetcher();

  GURL url1 = test_server_.GetURL("/cert.crt");
  GURL url2 = test_server_.GetURL("/root.crl");

  // Issue 3 requests for url1, and 3 requests for url2
  std::unique_ptr<CertNetFetcher::Request> request1 =
      StartRequest(fetcher(), url1);

  std::unique_ptr<CertNetFetcher::Request> request2 =
      StartRequest(fetcher(), url2);

  std::unique_ptr<CertNetFetcher::Request> request3 =
      StartRequest(fetcher(), url1);

  std::unique_ptr<CertNetFetcher::Request> request4 =
      StartRequest(fetcher(), url2);

  std::unique_ptr<CertNetFetcher::Request> request5 =
      StartRequest(fetcher(), url2);

  std::unique_ptr<CertNetFetcher::Request> request6 =
      StartRequest(fetcher(), url1);

  // Cancel all but one of the requests for url1.
  request1.reset();
  request3.reset();

  // Wait for the remaining requests to finish and verify the fetch results.
  VerifySuccess("-root.crl-\n", request2.get());
  VerifySuccess("-root.crl-\n", request4.get());
  VerifySuccess("-root.crl-\n", request5.get());
  VerifySuccess("-cert.crt-\n", request6.get());

  // Verify that only 2 URLRequests were started even though 6 requests were
  // issued.
  EXPECT_EQ(2, NumCreatedRequests());
}

// Cancel a request and then start another one for the same URL.
TEST_F(CertNetFetcherURLRequestTest, CancelThenStart) {
  ASSERT_TRUE(test_server_.Start());

  CreateFetcher();

  GURL url = test_server_.GetURL("/cert.crt");

  std::unique_ptr<CertNetFetcher::Request> request1 =
      StartRequest(fetcher(), url);
  request1.reset();

  std::unique_ptr<CertNetFetcher::Request> request2 =
      StartRequest(fetcher(), url);

  std::unique_ptr<CertNetFetcher::Request> request3 =
      StartRequest(fetcher(), url);
  request3.reset();

  // All but |request2| were canceled.
  VerifySuccess("-cert.crt-\n", request2.get());
}

// Start duplicate requests and then cancel all of them.
TEST_F(CertNetFetcherURLRequestTest, CancelAll) {
  ASSERT_TRUE(test_server_.Start());

  CreateFetcher();
  std::unique_ptr<CertNetFetcher::Request> requests[3];

  GURL url = test_server_.GetURL("/cert.crt");

  for (auto& request : requests) {
    request = StartRequest(fetcher(), url);
  }

  // Cancel all the requests.
  for (auto& request : requests) {
    request.reset();
  }

  EXPECT_EQ(1, NumCreatedRequests());
}

// Tests that Requests are signalled for completion even if they are
// created after the CertNetFetcher has been shutdown.
TEST_F(CertNetFetcherURLRequestTest, RequestsAfterShutdown) {
  ASSERT_TRUE(test_server_.Start());
  CreateFetcher();
  ShutDownFetcher();

  GURL url = test_server_.GetURL("/cert.crt");
  std::unique_ptr<CertNetFetcher::Request> request =
      StartRequest(fetcher(), url);
  VerifyFailure(ERR_ABORTED, request.get());
  EXPECT_EQ(0, NumCreatedRequests());
}

// Tests that Requests are signalled for completion if the fetcher is
// shutdown and the network thread stopped before the request is
// started.
TEST_F(CertNetFetcherURLRequestTest,
       RequestAfterShutdownAndNetworkThreadStopped) {
  ASSERT_TRUE(test_server_.Start());
  CreateFetcher();
  ShutDownFetcher();
  ResetState();
  network_thread_.reset();

  GURL url = test_server_.GetURL("/cert.crt");
  std::unique_ptr<CertNetFetcher::Request> request =
      StartRequest(fetcher(), url);
  VerifyFailure(ERR_ABORTED, request.get());
}

// Tests that outstanding Requests are cancelled when Shutdown is called.
TEST_F(CertNetFetcherURLRequestTestWithHangingReadHandler,
       ShutdownCancelsRequests) {
  CreateFetcher();

  GURL url = URLRequestHangingReadJob::GetMockHttpUrl();
  std::unique_ptr<CertNetFetcher::Request> request =
      StartRequest(fetcher(), url);

  ShutDownFetcher();
  VerifyFailure(ERR_ABORTED, request.get());
}

TEST_F(CertNetFetcherURLRequestTestWithSecureDnsInterceptor,
       SecureDnsDisabled) {
  CreateFetcher();
  std::unique_ptr<net::CertNetFetcher::Request> request = StartRequest(
      fetcher(),
      GURL("http://" + std::string(kMockSecureDnsHostname) + "/cert.crt"));
  Error actual_error;
  std::vector<uint8_t> actual_body;
  request->WaitForResult(&actual_error, &actual_body);
  EXPECT_TRUE(invoked_interceptor());
}

}  // namespace

}  // namespace net
