// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/cert_verifier/cert_net_url_loader/cert_net_fetcher_url_loader.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_server_properties.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job.h"
#include "services/cert_verifier/cert_net_url_loader/cert_net_fetcher_test.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/url_loader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using net::test::IsOk;

// TODO(eroman): Test that cookies aren't sent.

namespace cert_verifier {

namespace {

const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("net/data/cert_net_fetcher_impl_unittest");

const char kMockURL[] = "http://mock.hanging.read/";

const char kMockSecureDnsHostname[] = "mock.secure.dns.check";

// Wait for the request to complete, and verify that it completed successfully
// with the indicated bytes.
void VerifySuccess(const std::string& expected_body,
                   net::CertNetFetcher::Request* request) {
  net::Error actual_error;
  std::vector<uint8_t> actual_body;
  request->WaitForResult(&actual_error, &actual_body);

  EXPECT_THAT(actual_error, IsOk());
  EXPECT_EQ(expected_body, std::string(actual_body.begin(), actual_body.end()));
}

// Wait for the request to complete, and verify that it completed with the
// indicated failure.
void VerifyFailure(net::Error expected_error,
                   net::CertNetFetcher::Request* request) {
  net::Error actual_error;
  std::vector<uint8_t> actual_body;
  request->WaitForResult(&actual_error, &actual_body);

  EXPECT_EQ(expected_error, actual_error);
  EXPECT_EQ(0u, actual_body.size());
}

class CertNetFetcherURLLoaderTest : public PlatformTest {
 public:
  CertNetFetcherURLLoaderTest() {
    test_server_.AddDefaultHandlers(base::FilePath(kDocRoot));
    StartNetworkThread();
  }

  ~CertNetFetcherURLLoaderTest() override {
    if (!creation_thread_)
      return;
    creation_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&CertNetFetcherURLLoaderTest::TeardownOnNetworkThread,
                       base::Unretained(this)));
    creation_thread_->Stop();
  }

 protected:
  net::CertNetFetcher* fetcher() const { return test_util_->fetcher().get(); }

  void SetUseHangingURLLoader() { use_hanging_url_loader_ = true; }

  void CreateFetcherOnNetworkThread(base::WaitableEvent* done) {
    // Create the CertNetFetcherTestUtil.
    if (use_hanging_url_loader_)
      test_util_ = std::make_unique<CertNetFetcherTestUtilFakeLoader>();
    else
      test_util_ = std::make_unique<CertNetFetcherTestUtilRealLoader>();

    done->Signal();
  }

  void CreateFetcher() {
    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    creation_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &CertNetFetcherURLLoaderTest::CreateFetcherOnNetworkThread,
            base::Unretained(this), &done));
    done.Wait();
  }

  void ShutdownFetcherOnNetworkThread(base::WaitableEvent* done) {
    test_util_->fetcher()->Shutdown();
    done->Signal();
  }

  void ShutdownFetcher() {
    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    creation_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &CertNetFetcherURLLoaderTest::ShutdownFetcherOnNetworkThread,
            base::Unretained(this), &done));
    done.Wait();
  }

  int NumCreatedRequests() {
    int count = 0;
    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    creation_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&CertNetFetcherURLLoaderTest::CountCreatedRequests,
                       base::Unretained(this), &count, &done));
    done.Wait();
    return count;
  }

  void StartNetworkThread() {
    // Start the network thread.
    creation_thread_ = std::make_unique<base::Thread>("network thread");
    base::Thread::Options options(base::MessagePumpType::IO, 0);
    EXPECT_TRUE(creation_thread_->StartWithOptions(std::move(options)));
  }

  void ResetTestUtilOnNetworkThread(base::WaitableEvent* done) {
    test_util_.reset();
    done->Signal();
  }

  void ResetTestUtil() {
    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    creation_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &CertNetFetcherURLLoaderTest::ResetTestUtilOnNetworkThread,
            base::Unretained(this), &done));
    done.Wait();
  }

  void ResetTestURLLoaderFactoryOnNetworkThread(base::WaitableEvent* done) {
    test_util_->ResetURLLoaderFactory();
    done->Signal();
  }

  void ResetTestURLLoaderFactory() {
    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    creation_thread_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&CertNetFetcherURLLoaderTest::
                                      ResetTestURLLoaderFactoryOnNetworkThread,
                                  base::Unretained(this), &done));
    done.Wait();
  }

  void TeardownOnNetworkThread() {
    if (!test_util_)
      return;
    test_util_->fetcher()->Shutdown();
    DCHECK(test_util_->fetcher()->HasOneRef());
    test_util_.reset();
  }

  void WaitForAlreadyPostedNetworkTasks() {
    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    creation_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&CertNetFetcherURLLoaderTest::SignalDoneOnNetworkThread,
                       base::Unretained(this), &done));
    done.Wait();
  }

  void SignalDoneOnNetworkThread(base::WaitableEvent* done) { done->Signal(); }

  void CountCreatedRequests(int* count, base::WaitableEvent* done) {
    DCHECK(!use_hanging_url_loader_);
    *count = static_cast<CertNetFetcherTestUtilRealLoader*>(test_util_.get())
                 ->shared_url_loader_factory()
                 ->num_created_loaders();
    done->Signal();
  }

  net::EmbeddedTestServer test_server_;
  std::unique_ptr<base::Thread> creation_thread_;
  std::unique_ptr<CertNetFetcherTestUtil> test_util_;

  bool use_hanging_url_loader_ = false;
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
    EXPECT_EQ(net::SecureDnsPolicy::kDisable, request->secure_dns_policy());
    *invoked_interceptor_ = true;
    return nullptr;
  }

  raw_ptr<bool> invoked_interceptor_;
};

class CertNetFetcherURLLoaderTestWithSecureDnsInterceptor
    : public CertNetFetcherURLLoaderTest,
      public net::WithTaskEnvironment {
 public:
  CertNetFetcherURLLoaderTestWithSecureDnsInterceptor()
      : invoked_interceptor_(false) {}

  void SetUp() override {
    net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
        "http", kMockSecureDnsHostname,
        std::make_unique<SecureDnsInterceptor>(&invoked_interceptor_));
  }

  void TearDown() override {
    net::URLRequestFilter::GetInstance()->ClearHandlers();
  }

  bool invoked_interceptor() { return invoked_interceptor_; }

 private:
  bool invoked_interceptor_;
};

// Helper to start an AIA fetch using default parameters.
[[nodiscard]] std::unique_ptr<net::CertNetFetcher::Request> StartRequest(
    net::CertNetFetcher* fetcher,
    const GURL& url) {
  return fetcher->FetchCaIssuers(url, net::CertNetFetcher::DEFAULT,
                                 net::CertNetFetcher::DEFAULT);
}

// Fetch a few unique URLs using GET in parallel. Each URL has a different body
// and Content-Type.
TEST_F(CertNetFetcherURLLoaderTest, ParallelFetchNoDuplicates) {
  ASSERT_TRUE(test_server_.Start());
  CreateFetcher();

  // Request a URL with Content-Type "application/pkix-cert"
  GURL url1 = test_server_.GetURL("/cert.crt");
  std::unique_ptr<net::CertNetFetcher::Request> request1 =
      StartRequest(fetcher(), url1);

  // Request a URL with Content-Type "application/pkix-crl"
  GURL url2 = test_server_.GetURL("/root.crl");
  std::unique_ptr<net::CertNetFetcher::Request> request2 =
      StartRequest(fetcher(), url2);

  // Request a URL with Content-Type "application/pkcs7-mime"
  GURL url3 = test_server_.GetURL("/certs.p7c");
  std::unique_ptr<net::CertNetFetcher::Request> request3 =
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
TEST_F(CertNetFetcherURLLoaderTest, ContentTypeDoesntMatter) {
  ASSERT_TRUE(test_server_.Start());
  CreateFetcher();

  GURL url = test_server_.GetURL("/foo.txt");
  std::unique_ptr<net::CertNetFetcher::Request> request =
      StartRequest(fetcher(), url);
  VerifySuccess("-foo.txt-\n", request.get());
}

// Fetch a URLs whose HTTP response code is not 200. These are considered
// failures.
TEST_F(CertNetFetcherURLLoaderTest, HttpStatusCode) {
  ASSERT_TRUE(test_server_.Start());
  CreateFetcher();

  // Response was HTTP status 404.
  {
    GURL url = test_server_.GetURL("/404.html");
    std::unique_ptr<net::CertNetFetcher::Request> request =
        StartRequest(fetcher(), url);
    VerifyFailure(net::ERR_HTTP_RESPONSE_CODE_FAILURE, request.get());
  }

  // Response was HTTP status 500.
  {
    GURL url = test_server_.GetURL("/500.html");
    std::unique_ptr<net::CertNetFetcher::Request> request =
        StartRequest(fetcher(), url);
    VerifyFailure(net::ERR_HTTP_RESPONSE_CODE_FAILURE, request.get());
  }
}

// Fetching a URL with a Content-Disposition header should have no effect.
TEST_F(CertNetFetcherURLLoaderTest, ContentDisposition) {
  ASSERT_TRUE(test_server_.Start());
  CreateFetcher();

  GURL url = test_server_.GetURL("/downloadable.js");
  std::unique_ptr<net::CertNetFetcher::Request> request =
      StartRequest(fetcher(), url);
  VerifySuccess("-downloadable.js-\n", request.get());
}

// Verifies that a cacheable request will be served from the HTTP cache the
// second time it is requested.
TEST_F(CertNetFetcherURLLoaderTest, Cache) {
  ASSERT_TRUE(test_server_.Start());

  CreateFetcher();

  // Fetch a URL whose HTTP headers make it cacheable for 1 hour.
  GURL url(test_server_.GetURL("/cacheable_1hr.crt"));
  {
    std::unique_ptr<net::CertNetFetcher::Request> request =
        StartRequest(fetcher(), url);
    VerifySuccess("-cacheable_1hr.crt-\n", request.get());
  }

  EXPECT_EQ(1, NumCreatedRequests());

  // Kill the HTTP server.
  ASSERT_TRUE(test_server_.ShutdownAndWaitUntilComplete());

  // Fetch again -- will fail unless served from cache.
  {
    std::unique_ptr<net::CertNetFetcher::Request> request =
        StartRequest(fetcher(), url);
    VerifySuccess("-cacheable_1hr.crt-\n", request.get());
  }

  EXPECT_EQ(2, NumCreatedRequests());
}

// Verify that the maximum response body constraints are enforced by fetching a
// resource that is larger than the limit.
TEST_F(CertNetFetcherURLLoaderTest, TooLarge) {
  ASSERT_TRUE(test_server_.Start());

  CreateFetcher();

  // This file has a response body 12 bytes long. So setting the maximum to 11
  // bytes will cause it to fail.
  GURL url(test_server_.GetURL("/certs.p7c"));
  std::unique_ptr<net::CertNetFetcher::Request> request =
      fetcher()->FetchCaIssuers(url, net::CertNetFetcher::DEFAULT, 11);

  VerifyFailure(net::ERR_INSUFFICIENT_RESOURCES, request.get());
}

// Set the timeout to 10 milliseconds, and try fetching a URL that takes 5
// seconds to complete. It should fail due to a timeout.
TEST_F(CertNetFetcherURLLoaderTest, Hang) {
  ASSERT_TRUE(test_server_.Start());

  CreateFetcher();

  GURL url(test_server_.GetURL("/slow/certs.p7c?5"));
  std::unique_ptr<net::CertNetFetcher::Request> request =
      fetcher()->FetchCaIssuers(url, 10, net::CertNetFetcher::DEFAULT);
  VerifyFailure(net::ERR_TIMED_OUT, request.get());
}

// Verify that if a response is gzip-encoded it gets inflated before being
// returned to the caller.
TEST_F(CertNetFetcherURLLoaderTest, Gzip) {
  ASSERT_TRUE(test_server_.Start());

  CreateFetcher();

  GURL url(test_server_.GetURL("/gzipped_crl"));
  std::unique_ptr<net::CertNetFetcher::Request> request =
      StartRequest(fetcher(), url);
  VerifySuccess("-gzipped_crl-\n", request.get());
}

// Try fetching an unsupported URL scheme (https).
TEST_F(CertNetFetcherURLLoaderTest, HttpsNotAllowed) {
  ASSERT_TRUE(test_server_.Start());

  CreateFetcher();

  GURL url("https://foopy/foo.crt");
  std::unique_ptr<net::CertNetFetcher::Request> request =
      StartRequest(fetcher(), url);
  VerifyFailure(net::ERR_DISALLOWED_URL_SCHEME, request.get());

  // No request was created because the URL scheme was unsupported.
  EXPECT_EQ(0, NumCreatedRequests());
}

// Try fetching a URL which redirects to https.
TEST_F(CertNetFetcherURLLoaderTest, RedirectToHttpsNotAllowed) {
  ASSERT_TRUE(test_server_.Start());

  CreateFetcher();

  GURL url(test_server_.GetURL("/redirect_https"));

  std::unique_ptr<net::CertNetFetcher::Request> request =
      StartRequest(fetcher(), url);
  VerifyFailure(net::ERR_DISALLOWED_URL_SCHEME, request.get());

  EXPECT_EQ(1, NumCreatedRequests());
}

// Try fetching an unsupported URL scheme (https) and then immediately
// cancelling. This is a bit special because this codepath needs to post a task.
TEST_F(CertNetFetcherURLLoaderTest, CancelHttpsNotAllowed) {
  ASSERT_TRUE(test_server_.Start());

  CreateFetcher();

  GURL url("https://foopy/foo.crt");
  std::unique_ptr<net::CertNetFetcher::Request> request =
      StartRequest(fetcher(), url);

  // Cancel the request (May or may not have started yet, as the request is
  // running on another thread).
  request.reset();
}

TEST_F(CertNetFetcherURLLoaderTest,
       ReconnectsAfterURLLoaderFactoryDisconnection) {
  ASSERT_TRUE(test_server_.Start());
  CreateFetcher();

  GURL cert_crt_url = test_server_.GetURL("/cert.crt");
  std::unique_ptr<net::CertNetFetcher::Request> request1 =
      StartRequest(fetcher(), cert_crt_url);

  // Reset the URLLoaderFactory. It should reconnect afterwards and successfully
  // complete the rest of the requests.
  ResetTestURLLoaderFactory();

  GURL root_url = test_server_.GetURL("/root.crl");
  std::unique_ptr<net::CertNetFetcher::Request> request2 =
      StartRequest(fetcher(), root_url);

  GURL certs_p7c_url = test_server_.GetURL("/certs.p7c");
  std::unique_ptr<net::CertNetFetcher::Request> request3 =
      StartRequest(fetcher(), certs_p7c_url);

  // Wait for all of the requests to complete and verify the fetch results.
  VerifySuccess("-root.crl-\n", request2.get());
  VerifySuccess("-certs.p7c-\n", request3.get());

  // Depending on thread timing, |request1| may have completed successfully
  // prior to the URLLoaderFactory being disconnected, or the disconnect may
  // have caused the request to fail. Because it's timing dependent, and not
  // relevant to the subsequent requests which must succeed, either state is
  // allowed.
  {
    net::Error error;
    std::vector<uint8_t> body;
    request1->WaitForResult(&error, &body);
  }

  EXPECT_LE(2, NumCreatedRequests());

  ResetTestURLLoaderFactory();

  // Requests should work even after a second reset.
  std::unique_ptr<net::CertNetFetcher::Request> request4 =
      StartRequest(fetcher(), cert_crt_url);

  std::unique_ptr<net::CertNetFetcher::Request> request5 =
      StartRequest(fetcher(), root_url);

  VerifySuccess("-cert.crt-\n", request4.get());
  VerifySuccess("-root.crl-\n", request5.get());
}

// Start a few requests, and cancel one of them before running the message loop
// again.
TEST_F(CertNetFetcherURLLoaderTest, CancelBeforeRunningMessageLoop) {
  ASSERT_TRUE(test_server_.Start());

  CreateFetcher();

  GURL url1 = test_server_.GetURL("/cert.crt");
  std::unique_ptr<net::CertNetFetcher::Request> request1 =
      StartRequest(fetcher(), url1);

  GURL url2 = test_server_.GetURL("/root.crl");
  std::unique_ptr<net::CertNetFetcher::Request> request2 =
      StartRequest(fetcher(), url2);

  GURL url3 = test_server_.GetURL("/certs.p7c");

  std::unique_ptr<net::CertNetFetcher::Request> request3 =
      StartRequest(fetcher(), url3);

  // Cancel the second request.
  request2.reset();

  // Wait for the non-cancelled requests to complete, and verify the fetch
  // results.
  VerifySuccess("-cert.crt-\n", request1.get());
  VerifySuccess("-certs.p7c-\n", request3.get());
}

// Start several requests, and cancel one of them after the first has completed.
// TODO(eroman): Rename this test.
TEST_F(CertNetFetcherURLLoaderTest, CancelAfterRunningMessageLoop) {
  ASSERT_TRUE(test_server_.Start());

  CreateFetcher();

  GURL url1 = test_server_.GetURL("/cert.crt");

  std::unique_ptr<net::CertNetFetcher::Request> request1 =
      StartRequest(fetcher(), url1);

  GURL url2 = test_server_.GetURL("/certs.p7c");
  std::unique_ptr<net::CertNetFetcher::Request> request2 =
      StartRequest(fetcher(), url2);

  GURL url3("ftp://www.not.supported.com/foo");
  std::unique_ptr<net::CertNetFetcher::Request> request3 =
      StartRequest(fetcher(), url3);

  // Wait for the ftp request to complete (it should complete right away since
  // it doesn't even try to connect to the server).
  VerifyFailure(net::ERR_DISALLOWED_URL_SCHEME, request3.get());

  // Cancel the second outstanding request.
  request2.reset();

  // Wait for the first request to complete and verify the fetch result.
  VerifySuccess("-cert.crt-\n", request1.get());
}

// Fetch the same URLs in parallel and verify that only 1 request is made per
// URL.
TEST_F(CertNetFetcherURLLoaderTest, ParallelFetchDuplicates) {
  ASSERT_TRUE(test_server_.Start());

  CreateFetcher();

  GURL url1 = test_server_.GetURL("/cert.crt");
  GURL url2 = test_server_.GetURL("/root.crl");

  // Issue 3 requests for url1, and 3 requests for url2
  std::unique_ptr<net::CertNetFetcher::Request> request1 =
      StartRequest(fetcher(), url1);

  std::unique_ptr<net::CertNetFetcher::Request> request2 =
      StartRequest(fetcher(), url2);

  std::unique_ptr<net::CertNetFetcher::Request> request3 =
      StartRequest(fetcher(), url1);

  std::unique_ptr<net::CertNetFetcher::Request> request4 =
      StartRequest(fetcher(), url2);

  std::unique_ptr<net::CertNetFetcher::Request> request5 =
      StartRequest(fetcher(), url2);

  std::unique_ptr<net::CertNetFetcher::Request> request6 =
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
TEST_F(CertNetFetcherURLLoaderTest, CancelThenStart) {
  ASSERT_TRUE(test_server_.Start());

  CreateFetcher();

  GURL url = test_server_.GetURL("/cert.crt");

  std::unique_ptr<net::CertNetFetcher::Request> request1 =
      StartRequest(fetcher(), url);
  request1.reset();

  std::unique_ptr<net::CertNetFetcher::Request> request2 =
      StartRequest(fetcher(), url);

  std::unique_ptr<net::CertNetFetcher::Request> request3 =
      StartRequest(fetcher(), url);
  request3.reset();

  // All but |request2| were canceled.
  VerifySuccess("-cert.crt-\n", request2.get());
}

// Start duplicate requests and then cancel all of them.
TEST_F(CertNetFetcherURLLoaderTest, CancelAll) {
  ASSERT_TRUE(test_server_.Start());

  CreateFetcher();
  std::unique_ptr<net::CertNetFetcher::Request> requests[3];

  GURL url = test_server_.GetURL("/cert.crt");

  for (auto& request : requests) {
    request = StartRequest(fetcher(), url);
  }

  // Cancel all the requests.
  for (auto& request : requests) {
    request.reset();
  }

  // Wait for the network thread so that all of the CreateLoaderAndStart
  // messages are handled by the network thread.
  WaitForAlreadyPostedNetworkTasks();

  EXPECT_EQ(1, NumCreatedRequests());
}

// Tests that Requests are signalled for completion even if they are
// created after the CertNetFetcher has been shutdown.
TEST_F(CertNetFetcherURLLoaderTest, RequestsAfterShutdown) {
  ASSERT_TRUE(test_server_.Start());
  CreateFetcher();
  ShutdownFetcher();

  GURL url = test_server_.GetURL("/cert.crt");
  std::unique_ptr<net::CertNetFetcher::Request> request =
      StartRequest(fetcher(), url);
  VerifyFailure(net::ERR_ABORTED, request.get());
  EXPECT_EQ(0, NumCreatedRequests());
}

// Tests that Requests are signalled for completion if the fetcher is
// shutdown and the network thread stopped before the request is
// started.
TEST_F(CertNetFetcherURLLoaderTest,
       RequestAfterShutdownAndNetworkThreadStopped) {
  ASSERT_TRUE(test_server_.Start());
  CreateFetcher();
  ShutdownFetcher();

  // Take a reference to our fetcher to keep it alive when we reset
  // |test_util_|.
  scoped_refptr<net::CertNetFetcher> fetcher_ref = fetcher();
  ResetTestUtil();
  creation_thread_.reset();

  GURL url = test_server_.GetURL("/cert.crt");
  std::unique_ptr<net::CertNetFetcher::Request> request =
      StartRequest(fetcher_ref.get(), url);
  VerifyFailure(net::ERR_ABORTED, request.get());
}

// Tests that outstanding Requests are cancelled when Shutdown is called.
TEST_F(CertNetFetcherURLLoaderTest, ShutdownCancelsRequests) {
  SetUseHangingURLLoader();
  CreateFetcher();

  GURL url = GURL(kMockURL);
  std::unique_ptr<net::CertNetFetcher::Request> request =
      StartRequest(fetcher(), url);

  ShutdownFetcher();
  VerifyFailure(net::ERR_ABORTED, request.get());
}

TEST_F(CertNetFetcherURLLoaderTestWithSecureDnsInterceptor, SecureDnsDisabled) {
  CreateFetcher();
  std::unique_ptr<net::CertNetFetcher::Request> request = StartRequest(
      fetcher(),
      GURL("http://" + std::string(kMockSecureDnsHostname) + "/cert.crt"));
  net::Error actual_error;
  std::vector<uint8_t> actual_body;
  request->WaitForResult(&actual_error, &actual_body);
  EXPECT_TRUE(invoked_interceptor());
}

}  // namespace

}  // namespace cert_verifier
