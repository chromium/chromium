// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/pac_file_fetcher_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/filename_util.h"
#include "net/base/load_flags.h"
#include "net/base/network_delegate_impl.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/ct_policy_enforcer.h"
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
#include "net/quic/quic_context.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/simple_connection_listener.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_job_factory_impl.h"
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
  base::string16 text;
};

// A non-mock URL request which can access http:// and file:// urls, in the case
// the tests were built with file support.
class RequestContext : public URLRequestContext {
 public:
  RequestContext() : storage_(this) {
    ProxyConfig no_proxy;
    storage_.set_host_resolver(std::make_unique<MockHostResolver>());
    storage_.set_cert_verifier(std::make_unique<MockCertVerifier>());
    storage_.set_transport_security_state(
        std::make_unique<TransportSecurityState>());
    storage_.set_cert_transparency_verifier(
        std::make_unique<MultiLogCTVerifier>());
    storage_.set_ct_policy_enforcer(
        std::make_unique<DefaultCTPolicyEnforcer>());
    storage_.set_proxy_resolution_service(ProxyResolutionService::CreateFixed(
        ProxyConfigWithAnnotation(no_proxy, TRAFFIC_ANNOTATION_FOR_TESTS)));
    storage_.set_ssl_config_service(
        std::make_unique<SSLConfigServiceDefaults>());
    storage_.set_http_server_properties(
        std::make_unique<HttpServerProperties>());
    storage_.set_quic_context(std::make_unique<QuicContext>());

    HttpNetworkSession::Context session_context;
    session_context.host_resolver = host_resolver();
    session_context.cert_verifier = cert_verifier();
    session_context.transport_security_state = transport_security_state();
    session_context.cert_transparency_verifier = cert_transparency_verifier();
    session_context.ct_policy_enforcer = ct_policy_enforcer();
    session_context.proxy_resolution_service = proxy_resolution_service();
    session_context.ssl_config_service = ssl_config_service();
    session_context.http_server_properties = http_server_properties();
    session_context.quic_context = quic_context();
    storage_.set_http_network_session(std::make_unique<HttpNetworkSession>(
        HttpNetworkSession::Params(), session_context));
    storage_.set_http_transaction_factory(std::make_unique<HttpCache>(
        storage_.http_network_session(), HttpCache::DefaultBackend::InMemory(0),
        false));
    std::unique_ptr<URLRequestJobFactoryImpl> job_factory =
        std::make_unique<URLRequestJobFactoryImpl>();
    storage_.set_job_factory(std::move(job_factory));
  }

  ~RequestContext() override { AssertNoURLRequests(); }

 private:
  URLRequestContextStorage storage_;
};

// Get a file:// url relative to net/data/proxy/pac_file_fetcher_unittest.
GURL GetTestFileUrl(const std::string& relpath) {
  base::FilePath path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
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
  ~BasicNetworkDelegate() override = default;

 private:
  int OnBeforeURLRequest(URLRequest* request,
                         CompletionOnceCallback callback,
                         GURL* new_url) override {
    EXPECT_TRUE(request->load_flags() & LOAD_DISABLE_CERT_NETWORK_FETCHES);
    return OK;
  }

  int OnBeforeStartTransaction(URLRequest* request,
                               CompletionOnceCallback callback,
                               HttpRequestHeaders* headers) override {
    return OK;
  }

  int OnHeadersReceived(
      URLRequest* request,
      CompletionOnceCallback callback,
      const HttpResponseHeaders* original_response_headers,
      scoped_refptr<HttpResponseHeaders>* override_response_headers,
      const net::IPEndPoint& endpoint,
      base::Optional<GURL>* preserve_fragment_on_redirect_url) override {
    return OK;
  }

  void OnBeforeRedirect(URLRequest* request,
                        const GURL& new_location) override {}

  void OnResponseStarted(URLRequest* request, int net_error) override {}

  void OnCompleted(URLRequest* request, bool started, int net_error) override {}

  void OnURLRequestDestroyed(URLRequest* request) override {}

  void OnPACScriptError(int line_number, const base::string16& error) override {
  }

  bool OnCanGetCookies(const URLRequest& request,
                       const CookieList& cookie_list,
                       bool allowed_from_caller) override {
    return allowed_from_caller;
  }

  bool OnCanSetCookie(const URLRequest& request,
                      const net::CanonicalCookie& cookie,
                      CookieOptions* options,
                      bool allowed_from_caller) override {
    return allowed_from_caller;
  }

  DISALLOW_COPY_AND_ASSIGN(BasicNetworkDelegate);
};

class PacFileFetcherImplTest : public PlatformTest, public WithTaskEnvironment {
 public:
  PacFileFetcherImplTest() {
    test_server_.AddDefaultHandlers(base::FilePath(kDocRoot));
    context_.set_network_delegate(&network_delegate_);
  }

 protected:
  EmbeddedTestServer test_server_;
  BasicNetworkDelegate network_delegate_;
  RequestContext context_;
};

TEST_F(PacFileFetcherImplTest, FileUrlNotAllowed) {
  auto pac_fetcher = PacFileFetcherImpl::Create(&context_);

  // Fetch a file that exists, however the PacFileFetcherImpl does not allow use
  // of file://.
  base::string16 text;
  TestCompletionCallback callback;
  int result =
      pac_fetcher->Fetch(GetTestFileUrl("pac.txt"), &text, callback.callback(),
                         TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_THAT(result, IsError(ERR_DISALLOWED_URL_SCHEME));
}

// Redirect to file URLs are not allowed.
TEST_F(PacFileFetcherImplTest, RedirectToFileUrl) {
  ASSERT_TRUE(test_server_.Start());

  auto pac_fetcher = PacFileFetcherImpl::Create(&context_);

  GURL url(test_server_.GetURL("/redirect-to-file"));

  base::string16 text;
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

  auto pac_fetcher = PacFileFetcherImpl::Create(&context_);

  {  // Fetch a PAC with mime type "text/plain"
    GURL url(test_server_.GetURL("/pac.txt"));
    base::string16 text;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                    TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_THAT(result, IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());
    EXPECT_EQ(ASCIIToUTF16("-pac.txt-\n"), text);
  }
  {  // Fetch a PAC with mime type "text/html"
    GURL url(test_server_.GetURL("/pac.html"));
    base::string16 text;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                    TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_THAT(result, IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());
    EXPECT_EQ(ASCIIToUTF16("-pac.html-\n"), text);
  }
  {  // Fetch a PAC with mime type "application/x-ns-proxy-autoconfig"
    GURL url(test_server_.GetURL("/pac.nsproxy"));
    base::string16 text;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                    TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_THAT(result, IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());
    EXPECT_EQ(ASCIIToUTF16("-pac.nsproxy-\n"), text);
  }
}

TEST_F(PacFileFetcherImplTest, HttpStatusCode) {
  ASSERT_TRUE(test_server_.Start());

  auto pac_fetcher = PacFileFetcherImpl::Create(&context_);

  {  // Fetch a PAC which gives a 500 -- FAIL
    GURL url(test_server_.GetURL("/500.pac"));
    base::string16 text;
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
    base::string16 text;
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

  auto pac_fetcher = PacFileFetcherImpl::Create(&context_);

  // Fetch PAC scripts via HTTP with a Content-Disposition header -- should
  // have no effect.
  GURL url(test_server_.GetURL("/downloadable.pac"));
  base::string16 text;
  TestCompletionCallback callback;
  int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                  TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_THAT(result, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_EQ(ASCIIToUTF16("-downloadable.pac-\n"), text);
}

// Verifies that PAC scripts are not being cached.
TEST_F(PacFileFetcherImplTest, NoCache) {
  ASSERT_TRUE(test_server_.Start());

  auto pac_fetcher = PacFileFetcherImpl::Create(&context_);

  // Fetch a PAC script whose HTTP headers make it cacheable for 1 hour.
  GURL url(test_server_.GetURL("/cacheable_1hr.pac"));
  {
    base::string16 text;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                    TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_THAT(result, IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());
    EXPECT_EQ(ASCIIToUTF16("-cacheable_1hr.pac-\n"), text);
  }

  // Kill the HTTP server.
  ASSERT_TRUE(test_server_.ShutdownAndWaitUntilComplete());

  // Try to fetch the file again. Since the server is not running anymore, the
  // call should fail, thus indicating that the file was not fetched from the
  // local cache.
  {
    base::string16 text;
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

  auto pac_fetcher = PacFileFetcherImpl::Create(&context_);

  // Set the maximum response size to 50 bytes.
  int prev_size = pac_fetcher->SetSizeConstraint(50);

  // Try fetching URL that is 101 bytes large. We should abort the request
  // after 50 bytes have been read, and fail with a too large error.
  GURL url = test_server_.GetURL("/large-pac.nsproxy");
  base::string16 text;
  TestCompletionCallback callback;
  int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                  TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_THAT(result, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_FILE_TOO_BIG));
  EXPECT_TRUE(text.empty());

  // Restore the original size bound.
  pac_fetcher->SetSizeConstraint(prev_size);

  {  // Make sure we can still fetch regular URLs.
    GURL url(test_server_.GetURL("/pac.nsproxy"));
    base::string16 text;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                    TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_THAT(result, IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());
    EXPECT_EQ(ASCIIToUTF16("-pac.nsproxy-\n"), text);
  }
}

// The PacFileFetcher should be able to handle responses with an empty body.
TEST_F(PacFileFetcherImplTest, Empty) {
  ASSERT_TRUE(test_server_.Start());

  auto pac_fetcher = PacFileFetcherImpl::Create(&context_);

  GURL url(test_server_.GetURL("/empty"));
  base::string16 text;
  TestCompletionCallback callback;
  int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                  TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_THAT(result, IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_EQ(0u, text.size());
}

TEST_F(PacFileFetcherImplTest, Hang) {
  ASSERT_TRUE(test_server_.Start());

  auto pac_fetcher = PacFileFetcherImpl::Create(&context_);

  // Set the timeout period to 0.5 seconds.
  base::TimeDelta prev_timeout =
      pac_fetcher->SetTimeoutConstraint(base::TimeDelta::FromMilliseconds(500));

  // Try fetching a URL which takes 1.2 seconds. We should abort the request
  // after 500 ms, and fail with a timeout error.
  {
    GURL url(test_server_.GetURL("/slow/proxy.pac?1.2"));
    base::string16 text;
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
    base::string16 text;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                    TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_THAT(result, IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());
    EXPECT_EQ(ASCIIToUTF16("-pac.nsproxy-\n"), text);
  }
}

// The PacFileFetcher should decode any content-codings
// (like gzip, bzip, etc.), and apply any charset conversions to yield
// UTF8.
TEST_F(PacFileFetcherImplTest, Encodings) {
  ASSERT_TRUE(test_server_.Start());

  auto pac_fetcher = PacFileFetcherImpl::Create(&context_);

  // Test a response that is gzip-encoded -- should get inflated.
  {
    GURL url(test_server_.GetURL("/gzipped_pac"));
    base::string16 text;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                    TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_THAT(result, IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());
    EXPECT_EQ(ASCIIToUTF16("This data was gzipped.\n"), text);
  }

  // Test a response that was served as UTF-16 (BE). It should
  // be converted to UTF8.
  {
    GURL url(test_server_.GetURL("/utf16be_pac"));
    base::string16 text;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                    TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_THAT(result, IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());
    EXPECT_EQ(ASCIIToUTF16("This was encoded as UTF-16BE.\n"), text);
  }

  // Test a response that lacks a charset, however starts with a UTF8 BOM.
  {
    GURL url(test_server_.GetURL("/utf8_bom"));
    base::string16 text;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                    TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_THAT(result, IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback.WaitForResult(), IsOk());
    EXPECT_EQ(ASCIIToUTF16("/* UTF8 */\n"), text);
  }
}

TEST_F(PacFileFetcherImplTest, DataURLs) {
  auto pac_fetcher = PacFileFetcherImpl::Create(&context_);

  const char kEncodedUrl[] =
      "data:application/x-ns-proxy-autoconfig;base64,ZnVuY3Rpb24gRmluZFByb3h5R"
      "m9yVVJMKHVybCwgaG9zdCkgewogIGlmIChob3N0ID09ICdmb29iYXIuY29tJykKICAgIHJl"
      "dHVybiAnUFJPWFkgYmxhY2tob2xlOjgwJzsKICByZXR1cm4gJ0RJUkVDVCc7Cn0=";
  const char kPacScript[] =
      "function FindProxyForURL(url, host) {\n"
      "  if (host == 'foobar.com')\n"
      "    return 'PROXY blackhole:80';\n"
      "  return 'DIRECT';\n"
      "}";

  // Test fetching a "data:"-url containing a base64 encoded PAC script.
  {
    GURL url(kEncodedUrl);
    base::string16 text;
    TestCompletionCallback callback;
    int result = pac_fetcher->Fetch(url, &text, callback.callback(),
                                    TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_THAT(result, IsOk());
    EXPECT_EQ(ASCIIToUTF16(kPacScript), text);
  }

  const char kEncodedUrlBroken[] =
      "data:application/x-ns-proxy-autoconfig;base64,ZnVuY3Rpb24gRmluZFByb3h5R";

  // Test a broken "data:"-url containing a base64 encoded PAC script.
  {
    GURL url(kEncodedUrlBroken);
    base::string16 text;
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

  std::vector<std::unique_ptr<PacFileFetcherImpl>> pac_fetchers;

  TestCompletionCallback callback;
  base::string16 text;
  for (int i = 0; i < num_requests; i++) {
    auto pac_fetcher = PacFileFetcherImpl::Create(&context_);
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

  auto pac_fetcher = PacFileFetcherImpl::Create(&context_);
  base::string16 text;
  TestCompletionCallback callback;
  int result =
      pac_fetcher->Fetch(test_server_.GetURL("/hung"), &text,
                         callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_THAT(result, IsError(ERR_IO_PENDING));
  EXPECT_EQ(1u, context_.url_requests()->size());

  pac_fetcher->OnShutdown();
  EXPECT_EQ(0u, context_.url_requests()->size());
  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_CONTEXT_SHUT_DOWN));

  // Make sure there's no asynchronous completion notification.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, context_.url_requests()->size());
  EXPECT_FALSE(callback.have_result());

  result =
      pac_fetcher->Fetch(test_server_.GetURL("/hung"), &text,
                         callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_THAT(result, IsError(ERR_CONTEXT_SHUT_DOWN));
}

TEST_F(PacFileFetcherImplTest, OnShutdownWithNoLiveRequest) {
  ASSERT_TRUE(test_server_.Start());

  auto pac_fetcher = PacFileFetcherImpl::Create(&context_);
  pac_fetcher->OnShutdown();

  base::string16 text;
  TestCompletionCallback callback;
  int result =
      pac_fetcher->Fetch(test_server_.GetURL("/hung"), &text,
                         callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_THAT(result, IsError(ERR_CONTEXT_SHUT_DOWN));
  EXPECT_EQ(0u, context_.url_requests()->size());
}

}  // namespace

}  // namespace net
