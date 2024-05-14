// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/pac_file_fetcher_impl.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "net/base/features.h"
#include "net/base/filename_util.h"
#include "net/base/load_flags.h"
#include "net/base/network_delegate_impl.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/disk_cache/disk_cache.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/transport_security_state.h"
#include "net/net_buildflags.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/quic/quic_context.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/simple_connection_listener.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using net::test::IsError;
using net::test::IsOk;

using base::ASCIIToUTF16;

// TODO(eroman):
//   - Test canceling an outstanding request.
//   - Test deleting PacFileFetcher while a request is in progress.

namespace net {

namespace {

const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("net/data/pac_file_fetcher_unittest");

struct FetchResult {
  int code;
  std::u16string text;
};

// Get a file:// url relative to net/data/proxy/pac_file_fetcher_unittest.
GURL GetTestFileUrl(const std::string& relpath) {
  base::FilePath path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path);
  path = path.AppendASCII("net");
  path = path.AppendASCII("data");
  path = path.AppendASCII("pac_file_fetcher_unittest");
  GURL base_url = FilePathToFileURL(path);
  return GURL(base_url.spec() + "/" + relpath);
}

// Really simple NetworkDelegate so we can allow local file access on ChromeOS
// without introducing layering violations.  Also causes a test failure if a
// request is seen that doesn't set a load flag to bypass revocation checking.

class BasicNetworkDelegate : public NetworkDelegateImpl {
 public:
  BasicNetworkDelegate() = default;

  BasicNetworkDelegate(const BasicNetworkDelegate&) = delete;
  BasicNetworkDelegate& operator=(const BasicNetworkDelegate&) = delete;

  ~BasicNetworkDelegate() override = default;

 private:
  int OnBeforeURLRequest(URLRequest* request,
                         CompletionOnceCallback callback,
                         GURL* new_url) override {
    EXPECT_TRUE(request->load_flags() & LOAD_DISABLE_CERT_NETWORK_FETCHES);
    return OK;
  }
};

class PacFileFetcherImplTest : public PlatformTest, public WithTaskEnvironment {
 public:
  PacFileFetcherImplTest() {
    test_server_.AddDefaultHandlers(base::FilePath(kDocRoot));
    auto builder = CreateTestURLRequestContextBuilder();
    network_delegate_ =
        builder->set_network_delegate(std::make_unique<BasicNetworkDelegate>());
    context_ = builder->Build();
  }

 protected:
  EmbeddedTestServer test_server_;
  std::unique_ptr<URLRequestContext> context_;
  // Owned by `context_`.
  raw_ptr<BasicNetworkDelegate> network_delegate_;
};

TEST_F(PacFileFetcherImplTest, FileUrlNotAllowed) {
  auto pac_fetcher = PacFileFetcherImpl::Create(context_.get());

  // Fetch a file that exists, however the PacFileFetcherImpl does not allow use
  // of file://.
  std::u16string text;
  TestCompletionCallback callback;
  int result =
      pac_fetcher->Fetch(GetTestFileUrl("pac.txt"), &text, callback.callback(),
                         TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_THAT(result, IsError(ERR_DISALLOWED_URL_SCHEME));
}

// Redirect to file URLs are not allowed.
TEST_F(PacFileFetcherImplTest, RedirectToFileUrl) {
  ASSERT_TRUE(test_server_.Start());

  auto pac_fetcher = PacFileFetcherImpl::Create(context_.get());

  GURL url(test_server_.GetURL("/redirect-to-file"));

  std::u16string text;
  TestCompletionCallback callback;
  int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                  TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_THAT(result, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_UNSAFE_REDIRECT));
}

// Note that all mime types are allowed for PAC file, to be consistent
// with other browsers.
TEST_F(PacFileFetcherImplTest, HttpMimeType) {
  ASSERT_TRUE(test_server_.Start());

  auto pac_fetcher = PacFileFetcherImpl::Create(context_.get());

  {  // Fetch a PAC with mime type "text/plain"
    GURL url(test_server_.GetURL("/pac.txt"));
    std::u16string text;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                    TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_THAT(result, IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());
    EXPECT_EQ(u"-pac.txt-\n", text);
  }
  {  // Fetch a PAC with mime type "text/html"
    GURL url(test_server_.GetURL("/pac.html"));
    std::u16string text;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                    TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_THAT(result, IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());
    EXPECT_EQ(u"-pac.html-\n", text);
  }
  {  // Fetch a PAC with mime type "application/x-ns-proxy-autoconfig"
    GURL url(test_server_.GetURL("/pac.nsproxy"));
    std::u16string text;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                    TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_THAT(result, IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());
    EXPECT_EQ(u"-pac.nsproxy-\n", text);
  }
}

TEST_F(PacFileFetcherImplTest, HttpStatusCode) {
  ASSERT_TRUE(test_server_.Start());

  auto pac_fetcher = PacFileFetcherImpl::Create(context_.get());

  {  // Fetch a PAC which gives a 500 -- FAIL
    GURL url(test_server_.GetURL("/500.pac"));
    std::u16string text;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                    TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_THAT(result, IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(),
                IsError(ERR_HTTP_RESPONSE_CODE_FAILURE));
    EXPECT_TRUE(text.empty());
  }
  {  // Fetch a PAC which gives a 404 -- FAIL
    GURL url(test_server_.GetURL("/404.pac"));
    std::u16string text;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                    TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_THAT(result, IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(),
                IsError(ERR_HTTP_RESPONSE_CODE_FAILURE));
    EXPECT_TRUE(text.empty());
  }
}

TEST_F(PacFileFetcherImplTest, ContentDisposition) {
  ASSERT_TRUE(test_server_.Start());

  auto pac_fetcher = PacFileFetcherImpl::Create(context_.get());

  // Fetch PAC scripts via HTTP with a Content-Disposition header -- should
  // have no effect.
  GURL url(test_server_.GetURL("/downloadable.pac"));
  std::u16string text;
  TestCompletionCallback callback;
  int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                  TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_THAT(result, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_EQ(u"-downloadable.pac-\n", text);
}

// Verifies that fetches are made using the fetcher's IsolationInfo, by checking
// the DNS cache.
TEST_F(PacFileFetcherImplTest, IsolationInfo) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);
  const char kHost[] = "foo.test";

  ASSERT_TRUE(test_server_.Start());

  auto pac_fetcher = PacFileFetcherImpl::Create(context_.get());

  GURL url(test_server_.GetURL(kHost, "/downloadable.pac"));
  std::u16string text;
  TestCompletionCallback callback;
  int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                  TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_THAT(callback.GetResult(result), IsOk());
  EXPECT_EQ(u"-downloadable.pac-\n", text);

  // Check that the URL in kDestination is in the HostCache, with
  // the fetcher's IsolationInfo / NetworkAnonymizationKey, and no others.
  net::HostResolver::ResolveHostParameters params;
  params.source = net::HostResolverSource::LOCAL_ONLY;
  std::unique_ptr<net::HostResolver::ResolveHostRequest> host_request =
      context_->host_resolver()->CreateRequest(
          url::SchemeHostPort(url),
          pac_fetcher->isolation_info().network_anonymization_key(),
          net::NetLogWithSource(), params);
  net::TestCompletionCallback callback2;
  result = host_request->Start(callback2.callback());
  EXPECT_EQ(net::OK, callback2.GetResult(result));

  // Make sure there are no other entries in the HostCache (which would
  // potentially be associated with other NetworkIsolationKeys).
  EXPECT_EQ(1u, context_->host_resolver()->GetHostCache()->size());

  // Make sure the cache is actually returning different results based on
  // NetworkAnonymizationKey.
  host_request = context_->host_resolver()->CreateRequest(
      url::SchemeHostPort(url), NetworkAnonymizationKey(),
      net::NetLogWithSource(), params);
  net::TestCompletionCallback callback3;
  result = host_request->Start(callback3.callback());
  EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, callback3.GetResult(result));
}

// Verifies that PAC scripts are not being cached.
TEST_F(PacFileFetcherImplTest, NoCache) {
  ASSERT_TRUE(test_server_.Start());

  auto pac_fetcher = PacFileFetcherImpl::Create(context_.get());

  // Fetch a PAC script whose HTTP headers make it cacheable for 1 hour.
  GURL url(test_server_.GetURL("/cacheable_1hr.pac"));
  {
    std::u16string text;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                    TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_THAT(result, IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());
    EXPECT_EQ(u"-cacheable_1hr.pac-\n", text);
  }

  // Kill the HTTP server.
  ASSERT_TRUE(test_server_.ShutdownAndWaitUntilComplete());

  // Try to fetch the file again. Since the server is not running anymore, the
  // call should fail, thus indicating that the file was not fetched from the
  // local cache.
  {
    std::u16string text;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                    TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_THAT(result, IsError(ERR_IO_PENDING));

    // Expect any error. The exact error varies by platform.
    EXPECT_NE(OK, callback.WaitForResult());
  }
}

TEST_F(PacFileFetcherImplTest, TooLarge) {
  ASSERT_TRUE(test_server_.Start());

  auto pac_fetcher = PacFileFetcherImpl::Create(context_.get());

  {
    // Set the maximum response size to 50 bytes.
    int prev_size = pac_fetcher->SetSizeConstraint(50);

    // Try fetching URL that is 101 bytes large. We should abort the request
    // after 50 bytes have been read, and fail with a too large error.
    GURL url = test_server_.GetURL("/large-pac.nsproxy");
    std::u16string text;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                    TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_THAT(result, IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsError(ERR_FILE_TOO_BIG));
    EXPECT_TRUE(text.empty());

    // Restore the original size bound.
    pac_fetcher->SetSizeConstraint(prev_size);
  }

  {
    // Make sure we can still fetch regular URLs.
    GURL url(test_server_.GetURL("/pac.nsproxy"));
    std::u16string text;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                    TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_THAT(result, IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());
    EXPECT_EQ(u"-pac.nsproxy-\n", text);
  }
}

// The PacFileFetcher should be able to handle responses with an empty body.
TEST_F(PacFileFetcherImplTest, Empty) {
  ASSERT_TRUE(test_server_.Start());

  auto pac_fetcher = PacFileFetcherImpl::Create(context_.get());

  GURL url(test_server_.GetURL("/empty"));
  std::u16string text;
  TestCompletionCallback callback;
  int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                  TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_THAT(result, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_EQ(0u, text.size());
}

TEST_F(PacFileFetcherImplTest, Hang) {
  ASSERT_TRUE(test_server_.Start());

  auto pac_fetcher = PacFileFetcherImpl::Create(context_.get());

  // Set the timeout period to 0.5 seconds.
  base::TimeDelta prev_timeout =
      pac_fetcher->SetTimeoutConstraint(base::Milliseconds(500));

  // Try fetching a URL which takes 1.2 seconds. We should abort the request
  // after 500 ms, and fail with a timeout error.
  {
    GURL url(test_server_.GetURL("/slow/proxy.pac?1.2"));
    std::u16string text;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                    TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_THAT(result, IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsError(ERR_TIMED_OUT));
    EXPECT_TRUE(text.empty());
  }

  // Restore the original timeout period.
  pac_fetcher->SetTimeoutConstraint(prev_timeout);

  {  // Make sure we can still fetch regular URLs.
    GURL url(test_server_.GetURL("/pac.nsproxy"));
    std::u16string text;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                    TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_THAT(result, IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());
    EXPECT_EQ(u"-pac.nsproxy-\n", text);
  }
}

// The PacFileFetcher should decode any content-codings
// (like gzip, bzip, etc.), and apply any charset conversions to yield
// UTF8.
TEST_F(PacFileFetcherImplTest, Encodings) {
  ASSERT_TRUE(test_server_.Start());

  auto pac_fetcher = PacFileFetcherImpl::Create(context_.get());

  // Test a response that is gzip-encoded -- should get inflated.
  {
    GURL url(test_server_.GetURL("/gzipped_pac"));
    std::u16string text;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                    TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_THAT(result, IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());
    EXPECT_EQ(u"This data was gzipped.\n", text);
  }

  // Test a response that was served as UTF-16 (BE). It should
  // be converted to UTF8.
  {
    GURL url(test_server_.GetURL("/utf16be_pac"));
    std::u16string text;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                    TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_THAT(result, IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());
    EXPECT_EQ(u"This was encoded as UTF-16BE.\n", text);
  }

  // Test a response that lacks a charset, however starts with a UTF8 BOM.
  {
    GURL url(test_server_.GetURL("/utf8_bom"));
    std::u16string text;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                    TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_THAT(result, IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());
    EXPECT_EQ(u"/* UTF8 */\n", text);
  }
}

TEST_F(PacFileFetcherImplTest, DataURLs) {
  auto pac_fetcher = PacFileFetcherImpl::Create(context_.get());

  const char kEncodedUrl[] =
      "data:application/x-ns-proxy-autoconfig;base64,ZnVuY3Rpb24gRmluZFByb3h5R"
      "m9yVVJMKHVybCwgaG9zdCkgewogIGlmIChob3N0ID09ICdmb29iYXIuY29tJykKICAgIHJl"
      "dHVybiAnUFJPWFkgYmxhY2tob2xlOjgwJzsKICByZXR1cm4gJ0RJUkVDVCc7Cn0=";
  const char16_t kPacScript[] =
      u"function FindProxyForURL(url, host) {\n"
      u"  if (host == 'foobar.com')\n"
      u"    return 'PROXY blackhole:80';\n"
      u"  return 'DIRECT';\n"
      u"}";

  // Test fetching a "data:"-url containing a base64 encoded PAC script.
  {
    GURL url(kEncodedUrl);
    std::u16string text;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                    TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_THAT(result, IsOk());
    EXPECT_EQ(kPacScript, text);
  }

  const char kEncodedUrlBroken[] =
      "data:application/x-ns-proxy-autoconfig;base64,ZnVuY3Rpb24gRmluZFByb3h5R";

  // Test a broken "data:"-url containing a base64 encoded PAC script.
  {
    GURL url(kEncodedUrlBroken);
    std::u16string text;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                    TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_THAT(result, IsError(ERR_FAILED));
  }
}

// Makes sure that a request gets through when the socket group for the PAC URL
// is full, so PacFileFetcherImpl can use the same URLRequestContext as
// everything else.
TEST_F(PacFileFetcherImplTest, IgnoresLimits) {
  // Enough requests to exceed the per-group limit.
  int num_requests = 2 + ClientSocketPoolManager::max_sockets_per_group(
                             HttpNetworkSession::NORMAL_SOCKET_POOL);

  net::test_server::SimpleConnectionListener connection_listener(
      num_requests, net::test_server::SimpleConnectionListener::
                        FAIL_ON_ADDITIONAL_CONNECTIONS);
  test_server_.SetConnectionListener(&connection_listener);
  ASSERT_TRUE(test_server_.Start());

  std::u16string text;
  TestCompletionCallback callback;
  std::vector<std::unique_ptr<PacFileFetcherImpl>> pac_fetchers;
  for (int i = 0; i < num_requests; i++) {
    auto pac_fetcher = PacFileFetcherImpl::Create(context_.get());
    GURL url(test_server_.GetURL("/hung"));
    // Fine to use the same string and callback for all of these, as they should
    // all hang.
    int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                    TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_THAT(result, IsError(ERR_IO_PENDING));
    pac_fetchers.push_back(std::move(pac_fetcher));
  }

  connection_listener.WaitForConnections();
  // None of the callbacks should have been invoked - all jobs should still be
  // hung.
  EXPECT_FALSE(callback.have_result());

  // Need to shut down the server before |connection_listener| is destroyed.
  EXPECT_TRUE(test_server_.ShutdownAndWaitUntilComplete());
}

TEST_F(PacFileFetcherImplTest, OnShutdown) {
  ASSERT_TRUE(test_server_.Start());

  auto pac_fetcher = PacFileFetcherImpl::Create(context_.get());
  std::u16string text;
  TestCompletionCallback callback;
  int result =
      pac_fetcher->Fetch(test_server_.GetURL("/hung"), &text,
                         callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_THAT(result, IsError(ERR_IO_PENDING));
  EXPECT_EQ(1u, context_->url_requests()->size());

  pac_fetcher->OnShutdown();
  EXPECT_EQ(0u, context_->url_requests()->size());
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_CONTEXT_SHUT_DOWN));

  // Make sure there's no asynchronous completion notification.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, context_->url_requests()->size());
  EXPECT_FALSE(callback.have_result());

  result =
      pac_fetcher->Fetch(test_server_.GetURL("/hung"), &text,
                         callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_THAT(result, IsError(ERR_CONTEXT_SHUT_DOWN));
}

TEST_F(PacFileFetcherImplTest, OnShutdownWithNoLiveRequest) {
  ASSERT_TRUE(test_server_.Start());

  auto pac_fetcher = PacFileFetcherImpl::Create(context_.get());
  pac_fetcher->OnShutdown();

  std::u16string text;
  TestCompletionCallback callback;
  int result =
      pac_fetcher->Fetch(test_server_.GetURL("/hung"), &text,
                         callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_THAT(result, IsError(ERR_CONTEXT_SHUT_DOWN));
  EXPECT_EQ(0u, context_->url_requests()->size());
}

}  // namespace

}  // namespace net
