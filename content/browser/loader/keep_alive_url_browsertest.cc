// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <tuple>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/allow_check_is_test_for_testing.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/back_forward_cache_test_util.h"
#include "content/browser/loader/keep_alive_url_loader.h"
#include "content/browser/loader/keep_alive_url_loader_service.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/keep_alive_url_loader_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace content {
namespace {

using testing::Contains;
using testing::Pair;

constexpr char16_t kPromiseResolvedPageTitle[] = u"Resolved";

constexpr char kPrimaryHost[] = "a.test";
constexpr char kSecondaryHost[] = "b.test";
constexpr char kAllowedCspHost[] = "csp.test";

constexpr char kKeepAliveEndpoint[] = "/beacon";

constexpr char k200TextResponse[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "\r\n"
    "Acked!";

constexpr char k301Response[] =
    "HTTP/1.1 301 Moved Permanently\r\n"
    "Location: %s\r\n"
    "\r\n";

constexpr char kBeaconId[] = "beacon01";

constexpr char kFetchLaterEndpoint[] = "/fetch-later";

std::string GetKeepAliveEndpoint(std::optional<std::string> id = std::nullopt) {
  std::string endpoint = kKeepAliveEndpoint;
  if (id.has_value()) {
    endpoint += "?id=" + *id;
  }
  return endpoint;
}

std::string GetConnectSrcCSPHeader(const url::Origin& origin) {
  return base::StringPrintf("Content-Security-Policy: connect-src 'self' %s",
                            origin.Serialize().c_str());
}

// Encodes the given `url` using the JS method encodeURIComponent.
std::string EncodeURL(const GURL& url) {
  url::RawCanonOutputT<char> buffer;
  url::EncodeURIComponent(url.spec(), &buffer);
  return std::string(buffer.view());
}

MATCHER(IsFrameHidden,
        base::StrCat({"Frame is", negation ? " not" : "", " hidden"})) {
  return arg->GetVisibilityState() == PageVisibilityState::kHidden;
}

}  // namespace

class KeepAliveURLBrowserTestBase : public ContentBrowserTest {
 protected:
  using FeaturesType = std::vector<base::test::FeatureRefAndParams>;
  using DisabledFeaturesType = std::vector<base::test::FeatureRef>;

  KeepAliveURLBrowserTestBase()
      : https_test_server_(std::make_unique<net::EmbeddedTestServer>(
            net::EmbeddedTestServer::TYPE_HTTPS)) {}
  ~KeepAliveURLBrowserTestBase() override = default;
  // Not Copyable.
  KeepAliveURLBrowserTestBase(const KeepAliveURLBrowserTestBase&) = delete;
  KeepAliveURLBrowserTestBase& operator=(const KeepAliveURLBrowserTestBase&) =
      delete;

  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(),
                                                GetDisabledFeatures());
    ContentBrowserTest::SetUp();
  }
  virtual const FeaturesType& GetEnabledFeatures() = 0;
  virtual const DisabledFeaturesType& GetDisabledFeatures() {
    static const DisabledFeaturesType disabled_features =
        GetDefaultDisabledBackForwardCacheFeaturesForTesting();
    return disabled_features;
  }

  void SetUpOnMainThread() override {
    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");
    if (loader_service()) {
      loaders_observer_ = std::make_unique<KeepAliveURLLoadersTestObserver>(
          web_contents()->GetBrowserContext());
    }

    // Initialize an HTTPS server. Subclass may choose to use HTTPS by calling
    // `SetUseHttps()`.
    https_test_server_->AddDefaultHandlers(GetTestDataFilePath());
    https_test_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);

    histogram_tester_ = std::make_unique<base::HistogramTester>();

    ContentBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    histogram_tester_.reset();
    ContentBrowserTest::TearDownOnMainThread();
  }

 protected:
  [[nodiscard]] std::vector<
      std::unique_ptr<net::test_server::ControllableHttpResponse>>
  RegisterRequestHandlers(const std::vector<std::string>& relative_urls) {
    std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
        handlers;
    for (const auto& relative_url : relative_urls) {
      handlers.emplace_back(
          std::make_unique<net::test_server::ControllableHttpResponse>(
              server(), relative_url));
    }
    return handlers;
  }

  // Returns a cross-origin (kSecondaryHost) URL that causes the following
  // redirect chain:
  //     http(s)://b.test:<port>/no-cors-server-redirect-307?...
  // --> http(s)://b.test:<port>/server-redirect-307?...
  // --> http(s)://b.test:<port>/no-cors-server-redirect-307?...
  // --> `target_url
  GURL GetCrossOriginMultipleRedirectsURL(const GURL& target_url) const {
    const auto intermediate_url2 = server()->GetURL(
        kSecondaryHost, base::StringPrintf("/no-cors-server-redirect-307?%s",
                                           target_url.spec().c_str()));
    const auto intermediate_url1 = server()->GetURL(
        kSecondaryHost, base::StringPrintf("/server-redirect-307?%s",
                                           intermediate_url2.spec().c_str()));
    return server()->GetURL(
        kSecondaryHost, base::StringPrintf("/no-cors-server-redirect-307?%s",
                                           intermediate_url1.spec().c_str()));
  }

  // Returns a same-origin (kPrimaryHost) URL that causes the following
  // redirect chain:
  //     http(s)://a.test:<port>/server-redirect-307?...
  // --> http(s)://a.test:<port>/no-cors-server-redirect-307?...
  // --> `target_url`
  GURL GetSameOriginMultipleRedirectsURL(const GURL& target_url) const {
    const auto intermediate_url1 = server()->GetURL(
        kPrimaryHost, base::StringPrintf("/no-cors-server-redirect-307?%s",
                                         target_url.spec().c_str()));
    return server()->GetURL(
        kPrimaryHost, base::StringPrintf("/server-redirect-307?%s",
                                         intermediate_url1.spec().c_str()));
  }

  // Returns a same-origin (kPrimaryHost) URL that leads to cross-origin
  // redirect chain:
  //     http(s)://a.test:<port>/server-redirect-307?...
  // --> http(s)://b.test:<port>/no-cors-server-redirect-307?...
  // --> `target_url`
  GURL GetSameAndCrossOriginRedirectsURL(const GURL& target_url) const {
    const auto intermediate_url1 = server()->GetURL(
        kSecondaryHost, base::StringPrintf("/no-cors-server-redirect-307?%s",
                                           target_url.spec().c_str()));
    return server()->GetURL(
        kPrimaryHost, base::StringPrintf("/server-redirect-307?%s",
                                         intermediate_url1.spec().c_str()));
  }

  // Returns a same-origin (kPrimaryHost) URL that redirects to `target_url`:
  //     http(s)://a.test:<port>/server-redirect-307?...
  // --> `target_url`
  GURL GetSameOriginRedirectURL(const GURL& target_url) const {
    return server()->GetURL(kPrimaryHost,
                            base::StringPrintf("/server-redirect-307?%s",
                                               target_url.spec().c_str()));
  }

  using FetchKeepAliveRequestMetricType =
      KeepAliveURLLoader::FetchKeepAliveRequestMetricType;

  // Note: `renderer` is made optional to support the use cases where its
  // loggings happen after unloading a renderer, as the browser process might
  // not be able to fetch UMA logged by renderer in time before the latter is
  // shutting down, as described in https://crbug.com/40109064.
  // It should be made required once the bug is resolved.
  struct ExpectedRequestHistogram {
    int browser = 0;
    std::optional<int> renderer = std::nullopt;
  };
  using ExpectedTotalRequests = ExpectedRequestHistogram;
  using ExpectedStartedRequests = ExpectedRequestHistogram;
  using ExpectedSucceededRequests = ExpectedRequestHistogram;
  using ExpectedFailedRequests = ExpectedRequestHistogram;

  // A helper to assert on the number of UMA logged from both browser and
  // renderer processes.
  void ExpectFetchKeepAliveHistogram(
      const FetchKeepAliveRequestMetricType& expected_sample,
      const ExpectedTotalRequests& total,
      const ExpectedStartedRequests& started_count,
      const ExpectedSucceededRequests& succeeded_count,
      const ExpectedFailedRequests& failed_count) {
    // Collect metrics recorded in the renderer processes, if expecting any.
    for (size_t retries = 0;
         retries < 20 &&
         ((total.renderer.has_value() && *total.renderer > 0 &&
           histogram_tester()
               .GetAllSamples("FetchKeepAlive.Requests2.Total.Renderer")
               .empty()) ||
          (started_count.renderer.has_value() && *started_count.renderer > 0 &&
           histogram_tester()
               .GetAllSamples("FetchKeepAlive.Requests2.Started.Renderer")
               .empty()) ||
          (succeeded_count.renderer.has_value() &&
           *succeeded_count.renderer > 0 &&
           histogram_tester()
               .GetAllSamples("FetchKeepAlive.Requests2.Succeeded.Renderer")
               .empty()) ||
          (failed_count.renderer.has_value() && *failed_count.renderer > 0 &&
           histogram_tester()
               .GetAllSamples("FetchKeepAlive.Requests2.Failed.Renderer")
               .empty()));
         retries++) {
      FetchHistogramsFromChildProcesses();
    }

    const int renderer_sample = static_cast<int>(expected_sample);
    const int browser_sample =
        (expected_sample == FetchKeepAliveRequestMetricType::kBeacon ||
         expected_sample == FetchKeepAliveRequestMetricType::kPing ||
         expected_sample == FetchKeepAliveRequestMetricType::kAttribution)
            ? static_cast<int>(FetchKeepAliveRequestMetricType::kPing)
            : static_cast<int>(expected_sample);
    histogram_tester().ExpectUniqueSample(
        "FetchKeepAlive.Requests2.Total.Browser", browser_sample,
        total.browser);
    if (total.renderer.has_value()) {
      histogram_tester().ExpectUniqueSample(
          "FetchKeepAlive.Requests2.Total.Renderer", renderer_sample,
          *total.renderer);
    }
    histogram_tester().ExpectUniqueSample(
        "FetchKeepAlive.Requests2.Started.Browser", browser_sample,
        started_count.browser);
    if (started_count.renderer.has_value()) {
      histogram_tester().ExpectUniqueSample(
          "FetchKeepAlive.Requests2.Started.Renderer", renderer_sample,
          *started_count.renderer);
    }
    histogram_tester().ExpectUniqueSample(
        "FetchKeepAlive.Requests2.Succeeded.Browser", browser_sample,
        succeeded_count.browser);
    if (succeeded_count.renderer.has_value()) {
      histogram_tester().ExpectUniqueSample(
          "FetchKeepAlive.Requests2.Succeeded.Renderer", renderer_sample,
          *succeeded_count.renderer);
    }
    histogram_tester().ExpectUniqueSample(
        "FetchKeepAlive.Requests2.Failed.Browser", browser_sample,
        failed_count.browser);
    if (failed_count.renderer.has_value()) {
      histogram_tester().ExpectUniqueSample(
          "FetchKeepAlive.Requests2.Failed.Renderer", renderer_sample,
          *failed_count.renderer);
    }
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }
  RenderFrameHostImpl* current_frame_host() {
    return web_contents()->GetPrimaryFrameTree().root()->current_frame_host();
  }
  KeepAliveURLLoaderService* loader_service() {
    return static_cast<StoragePartitionImpl*>(
               web_contents()
                   ->GetBrowserContext()
                   ->GetDefaultStoragePartition())
        ->GetKeepAliveURLLoaderService();
  }

  void DisableBackForwardCache(WebContents* web_contents) {
    DisableBackForwardCacheForTesting(
        web_contents, BackForwardCache::TEST_REQUIRES_NO_CACHING);
  }

  KeepAliveURLLoadersTestObserver& loaders_observer() {
    return *loaders_observer_;
  }
  void SetUseHttps() { use_https_ = true; }
  net::EmbeddedTestServer* server() {
    return use_https_ ? https_test_server_.get() : embedded_test_server();
  }
  const net::EmbeddedTestServer* server() const {
    return use_https_ ? https_test_server_.get() : embedded_test_server();
  }

  const base::HistogramTester& histogram_tester() { return *histogram_tester_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<KeepAliveURLLoadersTestObserver> loaders_observer_;
  bool use_https_ = false;
  const std::unique_ptr<net::EmbeddedTestServer> https_test_server_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

class FetchKeepAliveCommonTestBase : public KeepAliveURLBrowserTestBase {
 protected:
  // Navigates to a page specified by `keepalive_page_url`, which must fire a
  // fetch keepalive request.
  // The method then postpones the request handling until RFH of the page is
  // fully unloaded (by navigating to another cross-origin page).
  // After that, `response` will be sent back.
  // `keepalive_request_handler` must handle the fetch keepalive request.
  void LoadPageWithKeepAliveRequestAndSendResponseAfterUnload(
      const GURL& keepalive_page_url,
      net::test_server::ControllableHttpResponse* keepalive_request_handler,
      const std::string& response) {
    ASSERT_TRUE(NavigateToURL(web_contents(), keepalive_page_url));
    RenderFrameHostImplWrapper rfh_1(current_frame_host());
    // Ensure the current page can be unloaded instead of being cached.
    DisableBackForwardCache(web_contents());
    // Ensure the keepalive request is sent before leaving the current page.
    keepalive_request_handler->WaitForRequest();
    if (loader_service()) {
      ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);
    }
    // Collects any potential histogram before the process is gone.
    FetchHistogramsFromChildProcesses();

    // Navigate to cross-origin page to ensure the 1st page can be unloaded.
    ASSERT_TRUE(NavigateToURL(web_contents(), GetCrossOriginPageURL()));
    ASSERT_NE(current_frame_host(), rfh_1.get());
    // Ensure the 1st page has been unloaded.
    ASSERT_TRUE(rfh_1.WaitUntilRenderFrameDeleted());
    if (loader_service()) {
      // Ensure there is still a loader pending to receive response.
      ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);
      // While the 1st page is unloaded, the disconnection may not propagate to
      // browser process in time, such that assertion on the number of
      // disconnected loader here might become flaky.
    }

    // Send back response to terminate in-browser request handling for the
    // pending request from 1st page.
    keepalive_request_handler->Send(response);
    keepalive_request_handler->Done();
  }

  // Navigates to a page specified by `keepalive_page_url`, which must fire a
  // fetch keepalive request.
  // This method ensure request handling happens. After that, `response` will be
  // sent back.
  // `keepalive_request_handler` must handle the fetch keepalive request.
  void LoadPageWithKeepAliveRequestAndSendResponse(
      const GURL& keepalive_page_url,
      net::test_server::ControllableHttpResponse* keepalive_request_handler,
      const std::string& response) {
    ASSERT_TRUE(NavigateToURL(web_contents(), keepalive_page_url));
    RenderFrameHostImplWrapper rfh_1(current_frame_host());
    // Ensure the keepalive request is sent.
    keepalive_request_handler->WaitForRequest();
    ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);
    ASSERT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);

    // Send back response to terminate in-browser request handling.
    keepalive_request_handler->Send(response);
    keepalive_request_handler->Done();
  }

  GURL GetKeepAlivePageURL(
      const std::string& method,
      size_t num_requests = 1,
      std::optional<std::string> headers = std::nullopt) const {
    std::string url = base::StringPrintf(
        "/set-header-with-file/content/test/data/fetch-keepalive.html?"
        "method=%s&requests=%zu",
        method.c_str(), num_requests);
    if (headers.has_value()) {
      url += "&" + *headers;
    }
    return server()->GetURL(kPrimaryHost, url);
  }

  GURL GetCrossOriginPageURL() {
    return server()->GetURL(kSecondaryHost, "/title2.html");
  }
};

// Contains the integration tests for loading fetch(url, {keepalive: true})
// requests via browser process that are difficult to reliably reproduce in web
// tests.
//
// Note that due to using different approach, tests to cover implementation
// before `kKeepAliveInBrowserMigration`, i.e. loading via delaying renderer
// shutdown, cannot be verified with inspecting KeepAliveURLLoaderService here
// and still live in a different file
// content/browser/renderer_host/render_process_host_browsertest.cc
class KeepAliveURLBrowserTest
    : public FetchKeepAliveCommonTestBase,
      public ::testing::WithParamInterface<std::string> {
 protected:
  const FeaturesType& GetEnabledFeatures() override {
    static const FeaturesType enabled_features =
        GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            {{blink::features::kKeepAliveInBrowserMigration, {}}});
    return enabled_features;
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    KeepAliveURLBrowserTest,
    ::testing::Values(net::HttpRequestHeaders::kGetMethod,
                      net::HttpRequestHeaders::kPostMethod),
    [](const testing::TestParamInfo<KeepAliveURLBrowserTest::ParamType>& info) {
      return info.param;
    });

IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest, OneRequest) {
  const std::string method = GetParam();
  auto request_handler =
      std::move(RegisterRequestHandlers({kKeepAliveEndpoint})[0]);
  ASSERT_TRUE(server()->Start());

  ASSERT_TRUE(NavigateToURL(web_contents(), GetKeepAlivePageURL(method)));
  // Ensure the keepalive request is sent, but delay response.
  request_handler->WaitForRequest();
  ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);

  // End the keepalive request by sending back response.
  request_handler->Send(k200TextResponse);
  request_handler->Done();

  TitleWatcher watcher(web_contents(), kPromiseResolvedPageTitle);
  EXPECT_EQ(watcher.WaitAndGetTitle(), kPromiseResolvedPageTitle);
  loaders_observer().WaitForTotalOnReceiveResponseForwarded(1);
  loaders_observer().WaitForTotalOnCompleteForwarded({net::OK});
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);
  ExpectFetchKeepAliveHistogram(
      FetchKeepAliveRequestMetricType::kFetch,
      ExpectedTotalRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedStartedRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedSucceededRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedFailedRequests(/*browser=*/0, /*renderer=*/0));
}

// Verify keepalive request loading works given 2 concurrent requests to the
// same host.
//
// Note: Chromium allows at most 6 concurrent connections to the same host under
// HTTP 1.1 protocol, which `server()` uses by default.
// Exceeding this limit will hang the browser.
// TODO(crbug.com/40262244): Flaky on Fuchsia and Android.
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest,
                       DISABLED_TwoConcurrentRequestsPerHost) {
  const std::string method = GetParam();
  const size_t num_requests = 2;
  auto request_handlers =
      RegisterRequestHandlers({kKeepAliveEndpoint, kKeepAliveEndpoint});
  ASSERT_TRUE(server()->Start());

  ASSERT_TRUE(
      NavigateToURL(web_contents(), GetKeepAlivePageURL(method, num_requests)));
  // Ensure all keepalive requests are sent, but delay responses.
  request_handlers[0]->WaitForRequest();
  request_handlers[1]->WaitForRequest();
  ASSERT_EQ(loader_service()->NumLoadersForTesting(), num_requests);

  // End the keepalive request by sending back responses.
  request_handlers[0]->Send(k200TextResponse);
  request_handlers[1]->Send(k200TextResponse);
  request_handlers[0]->Done();
  request_handlers[1]->Done();

  TitleWatcher watcher(web_contents(), kPromiseResolvedPageTitle);
  EXPECT_EQ(watcher.WaitAndGetTitle(), kPromiseResolvedPageTitle);
  loaders_observer().WaitForTotalOnReceiveResponseForwarded(2);
  loaders_observer().WaitForTotalOnCompleteForwarded({net::OK, net::OK});
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);
}

IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest, RequestWithCookie) {
  const std::string cookie = "keepaliveTestCookie=testCookieValue";
  const std::string method = GetParam();
  auto request_handler =
      std::move(RegisterRequestHandlers({kKeepAliveEndpoint})[0]);
  ASSERT_TRUE(server()->Start());

  // Navigate to an empty page first without making any requests.
  ASSERT_TRUE(NavigateToURL(web_contents(), server()->GetURL("/empty.html")));
  // Make a fetch keepalive request, expected to succeed.
  ASSERT_TRUE(ExecJs(web_contents(),
                     JsReplace(R"(
    document.cookie = $1 + '; path=/';
    fetch($2, {keepalive: true, method: $3});
  )",
                               cookie, kKeepAliveEndpoint, method),
                     content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Ensure the keepalive request is sent, but delay response.
  request_handler->WaitForRequest();
  ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);
  // End the keepalive request by sending back response.
  request_handler->Send(k200TextResponse);
  request_handler->Done();

  loaders_observer().WaitForTotalOnReceiveResponseForwarded(1);
  loaders_observer().WaitForTotalOnCompleteForwarded({net::OK});
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);
  // Expect the request to contain the cookie.
  EXPECT_THAT(request_handler->http_request()->headers,
              Contains(Pair(net::HttpRequestHeaders::kCookie, cookie)));
}

IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest,
                       RequestAfterNetworkServiceCrashes) {
  // Can't test this on bots that use an in-process network service.
  if (IsInProcessNetworkService()) {
    return;
  }

  const std::string method = GetParam();
  auto request_handler =
      std::move(RegisterRequestHandlers({kKeepAliveEndpoint})[0]);
  ASSERT_TRUE(server()->Start());

  // Navigate to an empty page first without making any requests.
  ASSERT_TRUE(NavigateToURL(web_contents(), server()->GetURL("/empty.html")));
  // Crash the NetworkService process on the page.
  SimulateNetworkServiceCrash();
  // Make a fetch keepalive request, expected to succeed.
  ASSERT_TRUE(ExecJs(web_contents(),
                     JsReplace(R"(
    fetch($1, {keepalive: true, method: $2});
  )",
                               kKeepAliveEndpoint, method),
                     content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Ensure the keepalive request is sent, but delay response.
  request_handler->WaitForRequest();
  ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);
  // End the keepalive request by sending back response.
  request_handler->Send(k200TextResponse);
  request_handler->Done();

  loaders_observer().WaitForTotalOnReceiveResponseForwarded(1);
  loaders_observer().WaitForTotalOnCompleteForwarded({net::OK});
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);
}

// TODO(crbug.com/40236167): Re-enable this test on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ReceiveResponseAfterPageUnload \
  DISABLED_ReceiveResponseAfterPageUnload
#else
#define MAYBE_ReceiveResponseAfterPageUnload ReceiveResponseAfterPageUnload
#endif
// Delays response to a keepalive ping until after the page making the keepalive
// ping has been unloaded. The browser must ensure the response is received and
// processed by the browser.
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest,
                       MAYBE_ReceiveResponseAfterPageUnload) {
  const std::string method = GetParam();
  auto request_handler =
      std::move(RegisterRequestHandlers({kKeepAliveEndpoint})[0]);
  ASSERT_TRUE(server()->Start());

  ASSERT_NO_FATAL_FAILURE(
      LoadPageWithKeepAliveRequestAndSendResponseAfterUnload(
          GetKeepAlivePageURL(method), request_handler.get(),
          k200TextResponse));

  // The response should be processed in browser.
  loaders_observer().WaitForTotalOnReceiveResponseProcessed(1);
  // `KeepAliveURLLoader::OnComplete` may not be called, as renderer is dead.
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
  ExpectFetchKeepAliveHistogram(
      FetchKeepAliveRequestMetricType::kFetch,
      ExpectedTotalRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedStartedRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedSucceededRequests(/*browser=*/1, /*renderer=*/0),
      ExpectedFailedRequests(/*browser=*/0, /*renderer=*/0));
}

// Delays response to a keepalive ping until after the page making the keepalive
// ping is put into BackForwardCache. The response should be processed by the
// renderer after the page is restored from BackForwardCache.
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest,
                       ReceiveResponseInBackForwardCache) {
  const std::string method = GetParam();
  auto request_handler =
      std::move(RegisterRequestHandlers({kKeepAliveEndpoint})[0]);
  ASSERT_TRUE(server()->Start());

  ASSERT_TRUE(NavigateToURL(web_contents(), GetKeepAlivePageURL(method)));
  RenderFrameHostImplWrapper rfh_1(current_frame_host());
  // Ensure the keepalive request is sent before leaving the current page.
  request_handler->WaitForRequest();
  ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);
  // Collects any potential histogram before the process is gone.
  FetchHistogramsFromChildProcesses();

  // Navigate to cross-origin page.
  ASSERT_TRUE(NavigateToURL(web_contents(), GetCrossOriginPageURL()));
  // Ensure the previous page has been put into BackForwardCache.
  ASSERT_EQ(rfh_1->GetLifecycleState(),
            RenderFrameHost::LifecycleState::kInBackForwardCache);
  // The loader is still pending to receive response.
  ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);
  ASSERT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);

  // Send back response.
  request_handler->Send(k200TextResponse);
  // The response is immediately forwarded to the in-BackForwardCache renderer.
  loaders_observer().WaitForTotalOnReceiveResponseForwarded(1);
  // Go back to `rfh_1`.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  // The response should be processed in renderer. Hence resolving Promise.
  TitleWatcher watcher(web_contents(), kPromiseResolvedPageTitle);
  EXPECT_EQ(watcher.WaitAndGetTitle(), kPromiseResolvedPageTitle);
  request_handler->Done();
  loaders_observer().WaitForTotalOnCompleteForwarded({net::OK});
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);
  ExpectFetchKeepAliveHistogram(
      FetchKeepAliveRequestMetricType::kFetch,
      ExpectedTotalRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedStartedRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedSucceededRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedFailedRequests(/*browser=*/0, /*renderer=*/0));
}

// Tests fetch(..., {keepalive: true}) with a cross-origin & CORS-safelisted
// request that causes a redirect chain of 4 URLs.
//
// As the mode is set to "no-cors" for CORS-safelisted requests, the redirect is
// processed without an error while the request is cross-origin.
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest, MultipleRedirectsRequest) {
  const auto beacon_endpoint =
      base::StringPrintf("%s?id=%s", kKeepAliveEndpoint, kBeaconId);
  auto request_handler =
      std::move(RegisterRequestHandlers({beacon_endpoint})[0]);
  ASSERT_TRUE(server()->Start());

  // Set up a cross-origin (kSecondaryHost) URL with CORS-safelisted
  // payload that causes multiple redirects and eventually points to a
  // cross-origin `target_url`:
  //
  //     http://b.test:<port>/no-cors-server-redirect-307?...
  // --> http://b.test:<port>/server-redirect-307?...
  // --> http://b.test:<port>/no-cors-server-redirect-307?...
  // --> `target_url
  const auto target_url = server()->GetURL(kSecondaryHost, beacon_endpoint);
  const auto beacon_url = GetCrossOriginMultipleRedirectsURL(target_url);

  // Navigate to a page that calls fetch() API and verify its response.
  ASSERT_TRUE(NavigateToURL(web_contents(),
                            server()->GetURL(kPrimaryHost, "/title1.html")));

  ASSERT_TRUE(ExecJs(web_contents(),
                     JsReplace(R"(
    fetch($1, {keepalive: true, mode: 'no-cors'});
  )",
                               beacon_url),
                     content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);

  // The in-browser logic should handle all redirects in browser first.
  loaders_observer().WaitForTotalOnReceiveRedirectProcessed(3);
  // After in-browser processing, the loader should remain alive to support
  // forwarding stored redirects/response to renderer.
  ASSERT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 1u);

  // Ensure the fetch request is sent.
  request_handler->WaitForRequest();
  // Send back response to terminate in-browser request handling.
  request_handler->Send(k200TextResponse);
  request_handler->Done();

  // All redirects and the response should be forwarded to renderer.
  loaders_observer().WaitForTotalOnReceiveRedirectForwarded(3);
  loaders_observer().WaitForTotalOnReceiveResponseForwarded(1);
  loaders_observer().WaitForTotalOnCompleteForwarded({net::OK});
  // After forwarding, the loader should all be gone.
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);
  ExpectFetchKeepAliveHistogram(
      FetchKeepAliveRequestMetricType::kFetch,
      ExpectedTotalRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedStartedRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedSucceededRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedFailedRequests(/*browser=*/0, /*renderer=*/0));
}

// Tests fetch(..., {keepalive: true}) with a cross-origin & CORS-safelisted
// request that causes a redirect chain of 3 URLs, where the cross-origin URLs
// are the 2nd URL & the 3rd URL in the chain.
//
// As the mode is set to "cors" for CORS-safelisted requests, the redirect will
// fail at the first cross-origin URL.
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest,
                       MultipleRedirectsAndFailInBetweenRequest) {
  const auto beacon_endpoint =
      base::StringPrintf("%s?id=%s", kKeepAliveEndpoint, kBeaconId);
  ASSERT_TRUE(server()->Start());

  // Set up a same-origin URL with CORS-safelisted payload that causes multiple
  // redirects and eventually points to a cross-origin `target_url`:
  //
  //     http://a.test:<port>/server-redirect-307?...
  // --> http://b.test:<port>/no-cors-server-redirect-307?... => should fail
  // --> `target_url => should not reach here
  const auto target_url = server()->GetURL(kSecondaryHost, beacon_endpoint);
  const auto beacon_url = GetSameAndCrossOriginRedirectsURL(target_url);

  // Navigate to a page that calls fetch() API and verify its response.
  ASSERT_TRUE(NavigateToURL(web_contents(),
                            server()->GetURL(kPrimaryHost, "/title1.html")));
  ASSERT_TRUE(ExecJs(web_contents(),
                     JsReplace(R"(
    fetch($1, {keepalive: true, mode: 'cors'});
  )",
                               beacon_url),
                     content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  // TODO(crbug.com/40236167): Figure out how to reliably wait for # loaders.

  // The in-browser logic should handle all redirects in browser first.
  loaders_observer().WaitForTotalOnReceiveRedirectProcessed(1);
  // After in-browser processing, the loader should remain alive to support
  // forwarding stored redirects/response to renderer.
  ASSERT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 1u);

  // No request will be sent to kKeepAliveEndpoint, as it fails at the 2nd URL.

  // All redirects should be forwarded to renderer.
  loaders_observer().WaitForTotalOnReceiveRedirectForwarded(1);
  loaders_observer().WaitForTotalOnCompleteForwarded({net::ERR_FAILED});
  // After forwarding, the loader should all be gone.
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);
  ExpectFetchKeepAliveHistogram(
      FetchKeepAliveRequestMetricType::kFetch,
      ExpectedTotalRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedStartedRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedSucceededRequests(/*browser=*/0, /*renderer=*/0),
      ExpectedFailedRequests(/*browser=*/1, /*renderer=*/1));
}

// Tests fetch(..., {keepalive: true}) with a cross-origin & CORS-safelisted
// request that causes a redirect chain of 3 URLs, where the cross-origin URL
// is the target URL (3rd URL in the chain).
//
// As the mode is set to "cors" for CORS-safelisted requests, the redirect will
// fail at the first cross-origin URL.
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest,
                       MultipleRedirectsAndFailAtLastRequest) {
  const auto beacon_endpoint =
      base::StringPrintf("%s?id=%s", kKeepAliveEndpoint, kBeaconId);
  auto request_handler =
      std::move(RegisterRequestHandlers({beacon_endpoint})[0]);
  ASSERT_TRUE(server()->Start());

  // Set up a same-origin URL with CORS-safelisted payload that causes multiple
  // redirects and eventually points to a cross-origin `target_url`:
  //
  //     http://a.test:<port>/server-redirect-307?...
  // --> http://a.test:<port>/no-cors-server-redirect-307?...
  // --> `target_url => should fail to get response
  const auto target_url = server()->GetURL(kSecondaryHost, beacon_endpoint);
  const auto beacon_url = GetSameOriginMultipleRedirectsURL(target_url);

  // Navigate to a page that calls fetch() API and verify its response.
  ASSERT_TRUE(NavigateToURL(web_contents(),
                            server()->GetURL(kPrimaryHost, "/title1.html")));
  ASSERT_TRUE(ExecJs(web_contents(),
                     JsReplace(R"(
    fetch($1, {keepalive: true, mode: 'cors'});
  )",
                               beacon_url),
                     content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);

  // The in-browser logic should handle all redirects in browser first.
  loaders_observer().WaitForTotalOnReceiveRedirectProcessed(2);
  // After in-browser processing, the loader should remain alive to support
  // forwarding stored redirects/response to renderer.
  ASSERT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 1u);

  // No request will be sent to kKeepAliveEndpoint, as it fails at the 2nd URL.
  // The redirect request should be processed in browser and gets sent.
  request_handler->WaitForRequest();
  // End the keepalive request by sending back final response.
  request_handler->Send(k200TextResponse);
  request_handler->Done();

  // All redirects should be forwarded to renderer.
  loaders_observer().WaitForTotalOnReceiveRedirectForwarded(2);
  loaders_observer().WaitForTotalOnCompleteForwarded({net::ERR_FAILED});
  // After forwarding, the loader should all be gone.
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);
  ExpectFetchKeepAliveHistogram(
      FetchKeepAliveRequestMetricType::kFetch,
      ExpectedTotalRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedStartedRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedSucceededRequests(/*browser=*/0, /*renderer=*/0),
      ExpectedFailedRequests(/*browser=*/1, /*renderer=*/1));
}

// Delays handling redirect for a keepalive ping until after the page making the
// keepalive ping has been unloaded. The browser must ensure the redirect is
// verified and properly processed by the browser.
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest,
                       ReceiveRedirectAfterPageUnload) {
  const std::string method = GetParam();
  const char redirect_target[] = "/beacon-redirected";
  auto request_handlers =
      RegisterRequestHandlers({kKeepAliveEndpoint, redirect_target});
  ASSERT_TRUE(server()->Start());

  // Sets up redirects according to the following redirect chain:
  // fetch("http://a.test:<port>/beacon", keepalive: true)
  // --> http://a.test:<port>/beacon-redirected
  ASSERT_NO_FATAL_FAILURE(
      LoadPageWithKeepAliveRequestAndSendResponseAfterUnload(
          GetKeepAlivePageURL(method), request_handlers[0].get(),
          base::StringPrintf(k301Response, redirect_target)));

  // The in-browser logic should process the redirect.
  loaders_observer().WaitForTotalOnReceiveRedirectProcessed(1);

  // The redirect request should be processed in browser and gets sent.
  request_handlers[1]->WaitForRequest();
  // End the keepalive request by sending back final response.
  request_handlers[1]->Send(k200TextResponse);
  request_handlers[1]->Done();

  // The response should be processed in browser.
  loaders_observer().WaitForTotalOnReceiveResponseProcessed(1);
  // `KeepAliveURLLoader::OnComplete` will not be called but the loader must
  // still be terminated, as renderer is dead.
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
  ExpectFetchKeepAliveHistogram(
      FetchKeepAliveRequestMetricType::kFetch,
      ExpectedTotalRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedStartedRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedSucceededRequests(/*browser=*/1, /*renderer=*/0),
      ExpectedFailedRequests(/*browser=*/0, /*renderer=*/0));
}

// Delays handling an unsafe redirect for a keepalive ping until after the page
// making the keepalive ping has been unloaded.
// The browser must ensure the unsafe redirect is not followed.
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest,
                       ReceiveUnSafeRedirectAfterPageUnload) {
  const std::string method = GetParam();
  const char unsafe_redirect_target[] = "chrome://settings";
  auto request_handler =
      std::move(RegisterRequestHandlers({kKeepAliveEndpoint})[0]);
  ASSERT_TRUE(server()->Start());

  // Set up redirects according to the following redirect chain:
  // fetch("http://a.test:<port>/beacon", keepalive: true)
  // --> chrome://settings
  ASSERT_NO_FATAL_FAILURE(
      LoadPageWithKeepAliveRequestAndSendResponseAfterUnload(
          GetKeepAlivePageURL(method), request_handler.get(),
          base::StringPrintf(k301Response, unsafe_redirect_target)));

  // The redirect is unsafe, so the loader is terminated.
  // While the 1st page is unloaded, the disconnection may not propagate to
  // browser process in time, such that calling
  // `WaitforTotalCompleteProcessed()` here might be flaky.
  loaders_observer().WaitForTotalOnComplete({net::ERR_UNSAFE_REDIRECT});
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);
  ExpectFetchKeepAliveHistogram(
      FetchKeepAliveRequestMetricType::kFetch,
      ExpectedTotalRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedStartedRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedSucceededRequests(/*browser=*/0, /*renderer=*/0),
      ExpectedFailedRequests(/*browser=*/1, /*renderer=*/0));
}

// Delays handling an violating CSP redirect for a keepalive ping until after
// the page making the keepalive ping has been unloaded.
// The browser must ensure the redirect is not followed.
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest,
                       ReceiveViolatingCSPRedirectAfterPageUnload) {
  const std::string method = GetParam();
  const char violating_csp_redirect_target[] = "http://b.com/beacon-redirected";
  auto request_handler =
      std::move(RegisterRequestHandlers({kKeepAliveEndpoint})[0]);
  ASSERT_TRUE(server()->Start());
  const GURL allowed_csp_url = server()->GetURL(kAllowedCspHost, "/");

  // Set up redirects according to the following redirect chain:
  // fetch("http://a.test:<port>/beacon", keepalive: true)
  // --> http://b.test/beacon-redirected
  ASSERT_NO_FATAL_FAILURE(
      LoadPageWithKeepAliveRequestAndSendResponseAfterUnload(
          GetKeepAlivePageURL(
              method, /*num_requests=*/1,
              GetConnectSrcCSPHeader(url::Origin::Create(allowed_csp_url))),
          request_handler.get(),
          base::StringPrintf(k301Response, violating_csp_redirect_target)));

  // The redirect doesn't match CSP source from the 1st page, so the loader is
  // terminated.
  // While the 1st page is unloaded, the disconnection may not propagate to
  // browser process in time, such that calling
  // `WaitforTotalCompleteProcessed()` here might be flaky.
  loaders_observer().WaitForTotalOnComplete({net::ERR_BLOCKED_BY_CSP});
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);
  ExpectFetchKeepAliveHistogram(
      FetchKeepAliveRequestMetricType::kFetch,
      ExpectedTotalRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedStartedRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedSucceededRequests(/*browser=*/0, /*renderer=*/0),
      ExpectedFailedRequests(/*browser=*/1, /*renderer=*/0));
}

// Verifies a redirect to mixed content target URL is not loaded.
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest, ReceiveMixedContentRedirect) {
  SetUseHttps();
  const std::string method = GetParam();
  auto request_handler =
      std::move(RegisterRequestHandlers({kKeepAliveEndpoint})[0]);
  ASSERT_TRUE(server()->Start());
  // Sets up a target URL that only has different scheme.
  // https://a.test:<port>/beacon-redirected
  std::string same_content_target =
      server()->GetURL(kPrimaryHost, "/beacon-redirected").spec();
  // http://a.test:<port>/beacon-redirected
  std::string mixed_content_target = same_content_target;
  base::ReplaceSubstringsAfterOffset(&mixed_content_target, 0, "https", "http");

  // Sets up redirects according to the following redirect chain:
  // fetch("https://a.test:<port>/beacon", keepalive: true)
  // --> http://a.test:<port>/beacon-redirected  => blocked by mixed content
  // Although it's also a CORS request, it will be blocked by mixed content
  // before reaching network service.
  ASSERT_NO_FATAL_FAILURE(LoadPageWithKeepAliveRequestAndSendResponse(
      GetKeepAlivePageURL(method), request_handler.get(),
      base::StringPrintf(k301Response, mixed_content_target.c_str())));

  // The redirect is mixed content, so the redirect is aborted.
  loaders_observer().WaitForTotalOnReceiveRedirectForwarded(1);
  loaders_observer().WaitForTotalOnReceiveResponseProcessed(0);
  // Note that the renderer terminates without waiting for error forwarded from
  // browser as it also calculates the error by itself.
  loaders_observer().WaitForTotalOnCompleteForwarded({});
  // The loader in browser is only terminated after renderer terminates its
  // loader. There is no way to wait for such disconnection mojo message
  // forwarded to browser at this moment.

  ExpectFetchKeepAliveHistogram(
      FetchKeepAliveRequestMetricType::kFetch,
      ExpectedTotalRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedStartedRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedSucceededRequests(/*browser=*/0, /*renderer=*/0),
      // TODO(crbug.com/40109064): Figure out why UMA from renderer cannot be
      // fetched by FetchHistogramsFromChildProcesses().
      ExpectedFailedRequests(/*browser=*/1));
}

// Verifies a redirect to mixed content target URL is allowed by
// KeepAliveURLLoader if the page making the fetch keepalive request has been
// unloaded, the same as pre-migration approach https://crrev.com/c/518743.
//
// Note that the current implementation in Blink & content cannot handle mixed
// content checking without the RFHI of the page that loads the request.
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest,
                       ReceiveMixedContentRedirectAfterUnload) {
  SetUseHttps();
  const std::string method = GetParam();
  auto request_handler =
      std::move(RegisterRequestHandlers({kKeepAliveEndpoint})[0]);
  auto redirected_request_handler =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/beacon-redirected");
  ASSERT_TRUE(server()->Start());
  ASSERT_TRUE(embedded_test_server()->Start());
  // Sets up a mixed content target URL that only has different scheme.
  // http://a.test:<port>/beacon-redirected
  std::string mixed_content_target =
      embedded_test_server()->GetURL(kPrimaryHost, "/beacon-redirected").spec();

  // Sets up redirects according to the following redirect chain:
  // fetch("https://a.test:<port>/beacon", keepalive: true)
  // --> http://a.test:<port>/beacon-redirected
  ASSERT_NO_FATAL_FAILURE(
      LoadPageWithKeepAliveRequestAndSendResponseAfterUnload(
          GetKeepAlivePageURL(method), request_handler.get(),
          base::StringPrintf(k301Response, mixed_content_target.c_str())));

  redirected_request_handler->WaitForRequest();
  redirected_request_handler->Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      // Necessary as this is a response to cross-origin request.
      "Access-Control-Allow-Origin: *\r\n"
      "\r\n"
      "Acked!");
  redirected_request_handler->Done();

  // The in-browser logic should process the redirect & response, as there is no
  // mixed content checking after unload.
  // TODO(crbug.com/40941240): Revisit the checks after the bug is fixed.
  loaders_observer().WaitForTotalOnReceiveRedirectProcessed(1);
  loaders_observer().WaitForTotalOnReceiveResponseProcessed(1);
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);
  ExpectFetchKeepAliveHistogram(
      FetchKeepAliveRequestMetricType::kFetch,
      ExpectedTotalRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedStartedRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedSucceededRequests(/*browser=*/1, /*renderer=*/0),
      ExpectedFailedRequests(/*browser=*/0, /*renderer=*/0));
}

// TODO(crbug.com/40236167): Re-enable this test on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ReceiveViolatingCSPRedirectInChildFrame \
  DISABLED_ReceiveViolatingCSPRedirectInChildFrame
#else
#define MAYBE_ReceiveViolatingCSPRedirectInChildFrame \
  ReceiveViolatingCSPRedirectInChildFrame
#endif
// Ensures that a keepalive request in a child frame use its RFH's data instead
// of its parent frame's:
// The main frame CSP allows `kAllowedCspHost`, while the child frame CSP does
// not. See also https://w3c.github.io/webappsec-csp/#security-inherit-csp.
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest,
                       MAYBE_ReceiveViolatingCSPRedirectInChildFrame) {
  const std::string method = GetParam();
  auto request_handler =
      std::move(RegisterRequestHandlers({GetKeepAliveEndpoint("main")})[0]);
  ASSERT_TRUE(server()->Start());
  const GURL main_target_url =
      server()->GetURL(kAllowedCspHost, GetKeepAliveEndpoint("main"));
  const GURL child_target_url =
      server()->GetURL(kAllowedCspHost, GetKeepAliveEndpoint("child"));
  const GURL main_beacon_url = GetSameOriginRedirectURL(main_target_url);
  const GURL child_beacon_url = GetSameOriginRedirectURL(child_target_url);

  // Main Page:
  // Prepares the main page that sends out a keepalive request.
  ASSERT_TRUE(NavigateToURL(
      web_contents(),
      server()->GetURL(
          kPrimaryHost,
          "/set-header-with-file/content/test/data/title1.html?" +
              GetConnectSrcCSPHeader(url::Origin::Create(main_target_url)))));
  ASSERT_TRUE(ExecJs(web_contents(),
                     JsReplace(R"(
    fetch($1, {keepalive: true, mode: 'cors'});

    let childLoaded;
    let childThrown;
    var childLoadedPromise = new Promise(resolve => childLoaded = resolve);
    var childThrownPromise = new Promise(resolve => childThrown = resolve);
    window.addEventListener('message', e => {
      if (e.data === 'loaded') {
        childLoaded(true);
      } else {
        childThrown(e.data);
      }
    });

    // Child Frame (Same-Origin):
    // Prepares the child page that also sends out a keepalive request.
    const iframe = document.createElement('iframe');
    iframe.srcdoc = `
    <meta http-equiv="Content-Security-Policy" content="connect-src 'self';">
    <script>
      fetch($2, {keepalive: true, mode: 'cors'}).catch(e =>
        window.parent.postMessage(e.message, "*")
      );
      window.parent.postMessage('loaded', "*");
    </script>
    `;
    document.body.appendChild(iframe);
  )",
                               main_beacon_url, child_beacon_url),
                     content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  ASSERT_EQ(current_frame_host()->child_count(), 1u);
  EXPECT_EQ(true, EvalJs(web_contents(), "childLoadedPromise"));
  // Redirects the keepalive request from child page to an allowed target.
  //     http://a.test:<port>/server-redirect-307?...
  // --> http://csp.test:<port>/beacon?id=child => disallowed by CSP
  EXPECT_EQ("Failed to fetch", EvalJs(web_contents(), "childThrownPromise"));

  // Only the main page is expected to sent out its keepalive request.
  request_handler->WaitForRequest();

  // Redirects the keepalive request from main page to a disallowed target.
  //     http://a.test:<port>/server-redirect-307?...
  // --> http://csp.test:<port>/beacon?id=main => allowed by CSP
  request_handler->Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      // Necessary as this is a response to cross-origin request.
      "Access-Control-Allow-Origin: *\r\n"
      "\r\n"
      "Acked!");
  request_handler->Done();

  // Only 1 redirect is expected to reach response.
  loaders_observer().WaitForTotalOnReceiveRedirectProcessed(1);
  loaders_observer().WaitForTotalOnReceiveResponse(1);
  // TODO(crbug.com/40236167): the order of calls to OnComplete is not stable.
  // Update KeepAliveURLLoadersTestObserver::WaitForTotalOnComplete to
  // accommodate this situation before asserting net::ERR_BLOCKED_BY_CSP.

  // Total 2 requests, and of them 1 fails.
  ExpectFetchKeepAliveHistogram(
      FetchKeepAliveRequestMetricType::kFetch,
      ExpectedTotalRequests(/*browser=*/2, /*renderer=*/2),
      ExpectedStartedRequests(/*browser=*/2, /*renderer=*/2),
      ExpectedSucceededRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedFailedRequests(/*browser=*/1, /*renderer=*/1));
}

// Contains the browser tests for loading fetch(url, {keepalive: true})
// requests **without** routing to browser process.
class FetchKeepAlivePreMigrationBrowserTest
    : public FetchKeepAliveCommonTestBase,
      public ::testing::WithParamInterface<std::string> {
 protected:
  const FeaturesType& GetEnabledFeatures() override {
    static const FeaturesType enabled_features =
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            {{features::kBackForwardCache, {}}});
    return enabled_features;
  }
  const DisabledFeaturesType& GetDisabledFeatures() override {
    static const DisabledFeaturesType disabled_features =
        GetDefaultDisabledBackForwardCacheFeaturesForTesting(
            {blink::features::kKeepAliveInBrowserMigration,
             blink::features::kFetchLaterAPI});
    return disabled_features;
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    FetchKeepAlivePreMigrationBrowserTest,
    ::testing::Values(net::HttpRequestHeaders::kGetMethod,
                      net::HttpRequestHeaders::kPostMethod),
    [](const testing::TestParamInfo<
        FetchKeepAlivePreMigrationBrowserTest::ParamType>& info) {
      return info.param;
    });

IN_PROC_BROWSER_TEST_P(FetchKeepAlivePreMigrationBrowserTest, OneRequest) {
  const std::string method = GetParam();
  auto request_handler =
      std::move(RegisterRequestHandlers({kKeepAliveEndpoint})[0]);
  ASSERT_TRUE(server()->Start());

  ASSERT_TRUE(NavigateToURL(web_contents(), GetKeepAlivePageURL(method)));
  // Ensure the keepalive request is sent, but delay response.
  request_handler->WaitForRequest();

  // End the keepalive request by sending back response.
  request_handler->Send(k200TextResponse);
  request_handler->Done();

  TitleWatcher watcher(web_contents(), kPromiseResolvedPageTitle);
  EXPECT_EQ(watcher.WaitAndGetTitle(), kPromiseResolvedPageTitle);
  ExpectFetchKeepAliveHistogram(
      FetchKeepAliveRequestMetricType::kFetch,
      ExpectedTotalRequests(/*browser=*/0, /*renderer=*/1),
      ExpectedStartedRequests(/*browser=*/0, /*renderer=*/1),
      ExpectedSucceededRequests(/*browser=*/0, /*renderer=*/1),
      ExpectedFailedRequests(/*browser=*/0, /*renderer=*/0));
}

IN_PROC_BROWSER_TEST_P(FetchKeepAlivePreMigrationBrowserTest,
                       ReceiveResponseAfterPageUnload) {
  const std::string method = GetParam();
  auto request_handler =
      std::move(RegisterRequestHandlers({kKeepAliveEndpoint})[0]);
  ASSERT_TRUE(server()->Start());

  ASSERT_NO_FATAL_FAILURE(
      LoadPageWithKeepAliveRequestAndSendResponseAfterUnload(
          GetKeepAlivePageURL(method), request_handler.get(),
          k200TextResponse));

  // The response should be processed in renderer.
  ExpectFetchKeepAliveHistogram(
      FetchKeepAliveRequestMetricType::kFetch,
      ExpectedTotalRequests(/*browser=*/0, /*renderer=*/1),
      ExpectedStartedRequests(/*browser=*/0, /*renderer=*/1),
      // Due to https://crbug.com/40109064, succeeded_count and failed_count
      // logging from renderer are flaky.
      ExpectedSucceededRequests(/*browser=*/0),
      // The pre-migration implementation in
      // `blink::ResourceLoader::DidResponseResponseInternal()` triggers error
      // handling if a document is detached even if the renderer has received
      // the response.
      ExpectedFailedRequests(/*browser=*/0));
}

class SendBeaconBrowserTestBase : public KeepAliveURLBrowserTestBase {
 protected:
  virtual std::string beacon_payload_type() const = 0;

  const FeaturesType& GetEnabledFeatures() override {
    static const FeaturesType enabled_features =
        GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            {{blink::features::kKeepAliveInBrowserMigration, {}}});
    return enabled_features;
  }

  GURL GetBeaconPageURL(
      const GURL& beacon_url,
      bool with_non_cors_safelisted_content,
      std::optional<int> delay_iframe_removal_ms = std::nullopt) const {
    std::vector<std::string> queries = {
        "/send-beacon-in-iframe.html?url=" + EncodeURL(beacon_url),
        "&payload_type=" + beacon_payload_type()};
    if (with_non_cors_safelisted_content) {
      // Setting the payload's content type to `application/octet-stream`, as
      // only `application/x-www-form-urlencoded`, `multipart/form-data`, and
      // `text/plain` MIME types are allowed for CORS-safelisted `content-type`
      // request header.
      // https://fetch.spec.whatwg.org/#cors-safelisted-request-header
      queries.push_back("&payload_content_type=application/octet-stream");
    }
    if (delay_iframe_removal_ms.has_value()) {
      queries.push_back(base::StringPrintf("&delay_iframe_removal_ms=%d",
                                           delay_iframe_removal_ms.value()));
    }

    return server()->GetURL(kPrimaryHost, base::StrCat(queries));
  }

  // Navigates to a page that calls `navigator.sendBeacon(beacon_url)` from a
  // programmatically created iframe. The iframe will then be removed after the
  // JS call after an optional `delay_iframe_removal_ms` interval.
  // `request_handler` must handle the final URL of the sendBeacon request.
  void LoadPageWithIframeAndSendBeacon(
      const GURL& beacon_url,
      net::test_server::ControllableHttpResponse* request_handler,
      const std::string& response,
      int expect_total_redirects,
      std::optional<int> delay_iframe_removal_ms = std::nullopt) {
    // Navigate to the page that calls sendBeacon with `beacon_url` from an
    // appended iframe.
    ASSERT_TRUE(NavigateToURL(
        web_contents(),
        GetBeaconPageURL(beacon_url,
                         /*with_non_cors_safelisted_content=*/false,
                         delay_iframe_removal_ms)));
    ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);

    // All redirects, if exist, should be processed in browser first.
    loaders_observer().WaitForTotalOnReceiveRedirectProcessed(
        expect_total_redirects);
    // After in-browser processing, the loader should remain alive to support
    // forwarding stored redirects/response to renderer. But it may or may not
    // connect to a renderer.
    EXPECT_EQ(loader_service()->NumLoadersForTesting(), 1u);

    // Ensure the sendBeacon request is sent.
    request_handler->WaitForRequest();
    // Send back final response to terminate in-browser request handling.
    request_handler->Send(response);
    request_handler->Done();

    // After in-browser redirect/response processing, the in-browser logic may
    // or may not forward redirect/response to renderer process, depending on
    // whether the renderer is still alive.
    loaders_observer().WaitForTotalOnReceiveResponse(1);
    // OnComplete may not be called if the renderer dies too early in before
    // receiving response.

    // The loader should all be gone.
    EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);
  }
};

class SendBeaconBrowserTest
    : public SendBeaconBrowserTestBase,
      public ::testing::WithParamInterface<std::string> {
 protected:
  std::string beacon_payload_type() const override { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SendBeaconBrowserTest,
    ::testing::Values("string", "arraybuffer", "form", "blob"),
    [](const testing::TestParamInfo<KeepAliveURLBrowserTest::ParamType>& info) {
      return info.param;
    });

// TODO(crbug.com/40931297): Re-enable this test on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_MultipleRedirectsRequestWithIframeRemoval \
  DISABLED_MultipleRedirectsRequestWithIframeRemoval
#else
#define MAYBE_MultipleRedirectsRequestWithIframeRemoval \
  MultipleRedirectsRequestWithIframeRemoval
#endif
// Tests navigator.sendBeacon() with a cross-origin & CORS-safelisted request
// that causes a redirect chain of 4 URLs.
//
// The JS call happens in an iframe that is removed right after the sendBeacon()
// call, so the chain of redirects & response handling must survive the iframe
// unload.
IN_PROC_BROWSER_TEST_P(SendBeaconBrowserTest,
                       MAYBE_MultipleRedirectsRequestWithIframeRemoval) {
  const auto beacon_endpoint =
      base::StringPrintf("%s?id=%s", kKeepAliveEndpoint, kBeaconId);
  auto request_handler =
      std::move(RegisterRequestHandlers({beacon_endpoint})[0]);
  ASSERT_TRUE(server()->Start());

  // Set up a cross-origin (kSecondaryHost) URL with CORS-safelisted
  // payload that causes multiple redirects.
  const auto target_url = server()->GetURL(kSecondaryHost, beacon_endpoint);
  const auto beacon_url = GetCrossOriginMultipleRedirectsURL(target_url);

  LoadPageWithIframeAndSendBeacon(beacon_url, request_handler.get(),
                                  k200TextResponse,
                                  /*expect_total_redirects=*/3);
}

// Tests navigator.sendBeacon() with a cross-origin & CORS-safelisted request
// that causes a redirect chain of 4 URLs.
//
// Unlike the `MultipleRedirectsRequestWithIframeRemoval` test case above, the
// request here is fired within an iframe that will be removed shortly
// (delayed by 0ms, roughly in the JS next event cycle).
// This is to mimic the following scenario:
//
// 1. The server returns a redirect.
// 2. In the browser process KeepAliveURLLoader::OnReceiveRedirect(),
//    forwarding_client_ is not null (as renderer/iframe still exists), so it
//    calls forwarding_client_->OnReceiveRedirect() IPC to forward to renderer.
// 3. The renderer process is somehow shut down before its
//    URLLoaderClient::OnReceiveRedirect() is finished, so the redirect chain is
//    incompleted.
// 4. KeepAliveURLLoader::OnRendererConnectionError() is triggered, and only
//    aware of forwarding_client_'s disconnection. It should take over redirect
//    chain handling.
//
// Without delaying iframe removal, renderer disconnection may happen in between
// (2) and (3).
IN_PROC_BROWSER_TEST_P(SendBeaconBrowserTest,
                       MultipleRedirectsRequestWithDelayedIframeRemoval) {
  const auto beacon_endpoint =
      base::StringPrintf("%s?id=%s", kKeepAliveEndpoint, kBeaconId);
  auto request_handler =
      std::move(RegisterRequestHandlers({beacon_endpoint})[0]);
  ASSERT_TRUE(server()->Start());

  // Set up a cross-origin (kSecondaryHost) URL with CORS-safelisted
  // payload that causes multiple redirects.
  const auto target_url = server()->GetURL(kSecondaryHost, beacon_endpoint);
  const auto beacon_url = GetCrossOriginMultipleRedirectsURL(target_url);

  LoadPageWithIframeAndSendBeacon(beacon_url, request_handler.get(),
                                  k200TextResponse,
                                  /*expect_total_redirects=*/3,
                                  /*delay_iframe_removal_ms=*/0);
}

// Tests navigator.sendBeacon() with a cross-origin & CORS-safelisted request
// that redirects from url1 to url2. The redirect is handled by a server
// endpoint (/no-cors-server-redirect-307) which does not support CORS.
// As navigator.sendBeacon() marks its request with `no-cors`, the redirect
// should succeed.
// TODO(crbug.com/40282448): Flaky on Android and Mac.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
#define MAYBE_CrossOriginAndCORSSafelistedRedirectRequest \
  DISABLED_CrossOriginAndCORSSafelistedRedirectRequest
#else
#define MAYBE_CrossOriginAndCORSSafelistedRedirectRequest \
  CrossOriginAndCORSSafelistedRedirectRequest
#endif
IN_PROC_BROWSER_TEST_P(SendBeaconBrowserTest,
                       MAYBE_CrossOriginAndCORSSafelistedRedirectRequest) {
  const auto beacon_endpoint =
      base::StringPrintf("%s?id=%s", kKeepAliveEndpoint, kBeaconId);
  auto request_handler =
      std::move(RegisterRequestHandlers({beacon_endpoint})[0]);
  ASSERT_TRUE(server()->Start());

  // Set up a cross-origin (kSecondaryHost) redirect with CORS-safelisted
  // payload according to the following redirect chain:
  // navigator.sendBeacon(
  //     "http://b.test:<port>/no-cors-server-redirect-307?...",
  //     <CORS-safelisted payload>)
  // --> http://b.test:<port>/beacon?id=beacon01
  const auto target_url = server()->GetURL(kSecondaryHost, beacon_endpoint);
  const auto beacon_url = server()->GetURL(
      kSecondaryHost, base::StringPrintf("/no-cors-server-redirect-307?%s",
                                         EncodeURL(target_url).c_str()));

  LoadPageWithIframeAndSendBeacon(beacon_url, request_handler.get(),
                                  k200TextResponse,
                                  /*expect_total_redirects=*/1);
}

class SendBeaconBlobBrowserTest : public SendBeaconBrowserTestBase {
 protected:
  std::string beacon_payload_type() const override { return "blob"; }
};

// Tests navigator.sendBeacon() with a cross-origin & non-CORS-safelisted
// request that redirects from url1 to url2. The redirect is handled by a server
// endpoint (/no-cors-server-redirect-307) which does not support CORS.
// As navigator.sendBeacon() marks its request with `no-cors`, the redirect
// should fail.
IN_PROC_BROWSER_TEST_F(SendBeaconBlobBrowserTest,
                       CrossOriginAndNonCORSSafelistedRedirectRequest) {
  const auto beacon_endpoint =
      base::StringPrintf("%s?id=%s", kKeepAliveEndpoint, kBeaconId);
  auto request_handler =
      std::move(RegisterRequestHandlers({beacon_endpoint})[0]);
  ASSERT_TRUE(server()->Start());

  // Set up a cross-origin (kSecondaryHost) redirect with non-CORS-safelisted
  // payload according to the following redirect chain:
  // navigator.sendBeacon(
  //     "http://b.test:<port>/no-cors-server-redirect-307?...",
  //     <non-CORS-safelisted payload>) => should fail here
  // --> http://b.test:<port>/beacon?id=beacon01
  const auto target_url = server()->GetURL(kSecondaryHost, beacon_endpoint);
  const auto beacon_url = server()->GetURL(
      kSecondaryHost, base::StringPrintf("/no-cors-server-redirect-307?%s",
                                         EncodeURL(target_url).c_str()));
  // Navigate to the page that calls sendBeacon with `beacon_url` from an
  // appended iframe, which will be removed shortly after calling sendBeacon().
  ASSERT_TRUE(NavigateToURL(
      web_contents(),
      GetBeaconPageURL(beacon_url, /*with_non_cors_safelisted_content=*/true)));

  // The redirect is rejected in-browser during redirect (with
  // non-CORS-safelisted payload) handling because /no-cors-server-redirect-xxx
  // doesn't support CORS. Thus, KeepAliveURLLoader::OnReceiveRedirect() is not
  // called but KeepAliveURLLoader::OnComplete().
  // Note that renderer can be gone at any point before or after the first URL
  // is loaded. So OnComplete() may or may not be forwarded.
  loaders_observer().WaitForTotalOnComplete({net::ERR_FAILED});
  EXPECT_FALSE(request_handler->has_received_request());
  // After in-browser processing, the loader should all be gone.
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);
  ExpectFetchKeepAliveHistogram(
      FetchKeepAliveRequestMetricType::kBeacon,
      ExpectedTotalRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedStartedRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedSucceededRequests(/*browser=*/0, /*renderer=*/0),
      ExpectedFailedRequests(/*browser=*/1, /*renderer=*/1));
}

// A base class to help testing JS fetchLater() API behaviors.
class FetchLaterBrowserTestBase : public KeepAliveURLBrowserTestBase {
 protected:
  void SetUp() override {
    // fetchLater() API only supports HTTPS requests.
    SetUseHttps();
    KeepAliveURLBrowserTestBase::SetUp();
  }

  bool NavigateToURL(const GURL& url) {
    previous_document_ =
        std::make_unique<RenderFrameHostImplWrapper>(current_frame_host());
    bool ret = content::NavigateToURL(web_contents(), url);
    current_document_ =
        std::make_unique<RenderFrameHostImplWrapper>(current_frame_host());
    return ret;
  }
  bool WaitUntilPreviousDocumentDeleted() {
    CHECK(previous_document_);
    // `previous_document_` might already be destroyed here.
    return previous_document_->WaitUntilRenderFrameDeleted();
  }
  // Caution: the returned document might already be killed if BFCache is not
  // working.
  RenderFrameHostImplWrapper& previous_document() {
    CHECK(previous_document_);
    CHECK(!previous_document_->IsDestroyed());
    return *previous_document_;
  }
  RenderFrameHostImplWrapper& current_document() {
    CHECK(previous_document_);
    return *current_document_;
  }

  // Navigates to an empty page, and executes `script` on it.
  void RunScript(const std::string& script) {
    ASSERT_TRUE(NavigateToURL(server()->GetURL(kPrimaryHost, "/title1.html")));
    ASSERT_TRUE(ExecJs(web_contents(), script));
    ASSERT_TRUE(WaitForLoadStop(web_contents()));
  }

  // Navigates to a page that executes `script`, and navigates to another page.
  void RunScriptAndNavigateAway(const std::string& script) {
    RunScript(script);

    // Navigate to cross-origin page to ensure the 1st page can be unloaded if
    // BackForwardCache is disabled.
    ASSERT_TRUE(
        NavigateToURL(server()->GetURL(kSecondaryHost, "/title2.html")));
    ASSERT_TRUE(WaitForLoadStop(web_contents()));
  }

  // Expects `total` number of FetchLater requests to be sent.
  // `total` must equal to the size of `request_handlers`.
  // `requester_handlers` are to wait for the FetchLater requests and to
  // respond.
  void ExpectFetchLaterRequests(
      size_t total,
      std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>&
          request_handlers) {
    SCOPED_TRACE(
        base::StringPrintf("ExpectFetchLaterRequests: %zu requests", total));
    ASSERT_EQ(total, request_handlers.size());
    EXPECT_EQ(loader_service()->NumLoadersForTesting(), total);

    for (const auto& handler : request_handlers) {
      // Waits for a FetchLater request.
      handler->WaitForRequest();
      // Sends back final response to terminate in-browser request handling.
      handler->Send(k200TextResponse);
      // Triggers OnComplete.
      handler->Done();
    }

    loaders_observer().WaitForTotalOnReceiveResponse(total);
    // TODO(crbug.com/40236167): Check NumLoadersForTesting==0 after migrating
    // to in-browser ThrottlingURLLoader. Current implementation cannot ensure
    // receiving renderer disconnection. Also need to wait for TotalOnComplete
    // by `total`, not by states.
  }

 private:
  std::unique_ptr<RenderFrameHostImplWrapper> current_document_ = nullptr;
  std::unique_ptr<RenderFrameHostImplWrapper> previous_document_ = nullptr;
};

// A type to support parameterized testing for timeout-related tests.
struct TestTimeoutType {
  std::string test_case_name;
  int32_t timeout;
};

// Tests to cover FetchLater's behaviors when BackForwardCache is off.
//
// Disables BackForwardCache such that a page is discarded right away on user
// navigating to another page.
class FetchLaterNoBackForwardCacheBrowserTest
    : public FetchLaterBrowserTestBase,
      public testing::WithParamInterface<TestTimeoutType> {
 protected:
  const FeaturesType& GetEnabledFeatures() override {
    static const FeaturesType enabled_features = {
        {blink::features::kFetchLaterAPI, {{}}}};
    return enabled_features;
  }
  const DisabledFeaturesType& GetDisabledFeatures() override {
    static const DisabledFeaturesType disabled_features = {
        features::kBackForwardCache};
    return disabled_features;
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    FetchLaterNoBackForwardCacheBrowserTest,
    testing::ValuesIn<std::vector<TestTimeoutType>>({
        {"LongTimeout", 600000},      // 10 minutes
        {"OneMinuteTimeout", 60000},  // 1 minute
    }),
    [](const testing::TestParamInfo<TestTimeoutType>& info) {
      return info.param.test_case_name;
    });

// All pending FetchLater requests should be sent after the initiator page is
// gone, no matter how much time their activateAfter has left.
// Disables BackForwardCache such that a page is discarded right away on user
// navigating to another page.
IN_PROC_BROWSER_TEST_P(FetchLaterNoBackForwardCacheBrowserTest,
                       SendOnPageDiscardBeforeActivationTimeout) {
  const std::string target_url = kFetchLaterEndpoint;
  auto request_handlers = RegisterRequestHandlers({target_url, target_url});
  ASSERT_TRUE(server()->Start());

  // Creates two FetchLater requests with various long activateAfter, which
  // should all be sent on page discard.
  RunScriptAndNavigateAway(JsReplace(R"(
    fetchLater($1, {activateAfter: $2});
    fetchLater($1, {activateAfter: $2});
  )",
                                     target_url, GetParam().timeout));
  // Ensure the 1st page has been unloaded.
  ASSERT_TRUE(WaitUntilPreviousDocumentDeleted());

  // Loaders are disconnected after the 1st page is gone.
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 2u);
  // The FetchLater requests should've been sent after the 1st page is gone.
  ExpectFetchLaterRequests(2, request_handlers);
}

class FetchLaterWithBackForwardCacheMetricsBrowserTestBase
    : public FetchLaterBrowserTestBase,
      public BackForwardCacheMetricsTestMatcher {
 protected:
  void SetUpOnMainThread() override {
    // TestAutoSetUkmRecorder's constructor requires a sequenced context.
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    FetchLaterBrowserTestBase::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    ukm_recorder_.reset();
    histogram_tester_.reset();
    FetchLaterBrowserTestBase::TearDownOnMainThread();
  }

  // `BackForwardCacheMetricsTestMatcher` implementation.
  const ukm::TestAutoSetUkmRecorder& ukm_recorder() override {
    return *ukm_recorder_;
  }
  const base::HistogramTester& histogram_tester() override {
    return *histogram_tester_;
  }

 private:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Tests to cover FetchLater's behaviors when BackForwardCache is on but does
// not come into play.
//
// Setting long `BackForwardCache TTL (1min)` so that FetchLater sending cannot
// be caused by page eviction out of BackForwardCache.
class FetchLaterNoActivationTimeoutBrowserTest
    : public FetchLaterWithBackForwardCacheMetricsBrowserTestBase {
 protected:
  const FeaturesType& GetEnabledFeatures() override {
    static const FeaturesType enabled_features = {
        {blink::features::kFetchLaterAPI, {}},
        {features::kBackForwardCache, {{}}},
        {features::kBackForwardCacheTimeToLiveControl,
         {{"time_to_live_seconds", "60"}}},
        // Forces BackForwardCache to work in low memory device.
        {features::kBackForwardCacheMemoryControls,
         {{"memory_threshold_for_back_forward_cache_in_mb", "0"}}}};
    return enabled_features;
  }
};

// A pending FetchLater request with default options should be sent after the
// initiator page is gone.
// Similar to SendOnPageDiscardBeforeActivationTimeout.
IN_PROC_BROWSER_TEST_F(FetchLaterNoActivationTimeoutBrowserTest,
                       SendOnPageDeletion) {
  const std::string target_url = kFetchLaterEndpoint;
  auto request_handlers = RegisterRequestHandlers({target_url});
  ASSERT_TRUE(server()->Start());

  // Creates a FetchLater request in an iframe, which is removed after loaded.
  ASSERT_TRUE(NavigateToURL(
      server()->GetURL(kPrimaryHost, "/page_with_blank_iframe.html")));
  ASSERT_TRUE(ExecJs(web_contents(), R"(
    var promise = new Promise(resolve => {
      window.addEventListener('message', e => {
        const iframe = document.getElementById('test_iframe');
        iframe.remove();
        resolve(e.data);
      });
    });
  )"));
  auto* iframe =
      static_cast<RenderFrameHostImpl*>(ChildFrameAt(web_contents(), 0));
  EXPECT_TRUE(ExecJs(iframe, JsReplace(R"(
      fetchLater($1);
      window.parent.postMessage(true, "*");
    )",
                                       target_url)));
  // `iframe` is removed after it calls fetchLater().
  EXPECT_EQ(true, EvalJs(web_contents(), "promise"));

  // The loader is disconnected after the 1st page is gone.
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 1u);
  // The FetchLater requests should've been sent after the 1st page is gone.
  ExpectFetchLaterRequests(1, request_handlers);
}

// A pending FetchLater request should have been sent after its page gets
// restored from BackForwardCache before getting evicted. It is because, by
// default, pending requests are all flushed on BFCache no matter
// BackgroundSync is on or not. See http://b/310541607#comment28.
IN_PROC_BROWSER_TEST_F(
    FetchLaterNoActivationTimeoutBrowserTest,
    FlushedWhenPageIsRestoredBeforeBeingEvictedFromBackForwardCache) {
  const std::string target_url = kFetchLaterEndpoint;
  auto request_handlers = RegisterRequestHandlers({target_url});
  ASSERT_TRUE(server()->Start());

  RunScriptAndNavigateAway(JsReplace(R"(
    fetchLater($1);
  )",
                                     target_url));
  ASSERT_TRUE(previous_document()->IsInBackForwardCache());
  // Navigate back to the 1st page.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  // The same page is still alive.
  ExpectRestored(FROM_HERE);
  // The FetchLater requests should've been sent.
  ExpectFetchLaterRequests(1, request_handlers);
}

// Without an activateAfter set, a pending FetchLater request should not be
// sent out during its page frozen state.
// Similar to ResetActivationTimeoutTimerOnPageResume.
IN_PROC_BROWSER_TEST_F(FetchLaterNoActivationTimeoutBrowserTest,
                       NotSendWhenPageIsResumedAfterBeingFrozen) {
  const std::string target_url = kFetchLaterEndpoint;
  ASSERT_TRUE(server()->Start());

  // Creates a FetchLater request with NO activateAfter.
  // It should be impossible to send out during page frozen.
  ASSERT_TRUE(NavigateToURL(server()->GetURL(kPrimaryHost, "/title1.html")));
  ASSERT_TRUE(ExecJs(web_contents(), JsReplace(R"(
    fetchLater($1);
  )",
                                               target_url)));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));

  // Forces to freeze the current page.
  web_contents()->WasHidden();
  web_contents()->SetPageFrozen(true);

  // The FetchLater request should not be sent.
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 1u);
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);

  // Forces to wake up the current page.
  web_contents()->WasHidden();
  web_contents()->SetPageFrozen(false);
  // The FetchLater request should not be sent.
  // TODO(crbug.com/40276121): Verify FetchLaterResult once
  // https://crrev.com/c/4820528 is submitted.
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 1u);
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
}

// Tests to cover FetchLater's activateAfter behaviors when BackForwardCache
// is on and may come into play.
//
// BackForwardCache eviction is simulated by calling
// `DisableBFCacheForRFHForTesting(previous_document())` instead of relying on
// its TTL.
class FetchLaterActivationTimeoutBrowserTest
    : public FetchLaterWithBackForwardCacheMetricsBrowserTestBase {
 protected:
  const FeaturesType& GetEnabledFeatures() override {
    static const FeaturesType enabled_features = {
        {blink::features::kFetchLaterAPI, {}},
        {features::kBackForwardCache, {{}}},
        // Sets to a long timeout, as tests below should not rely on it.
        {features::kBackForwardCacheTimeToLiveControl,
         {{"time_to_live_seconds", "60"}}},
        // Forces BackForwardCache to work in low memory device.
        {features::kBackForwardCacheMemoryControls,
         {{"memory_threshold_for_back_forward_cache_in_mb", "0"}}}};
    return enabled_features;
  }
};

// When setting activateAfter>0, a pending FetchLater request should be sent
// after around the specified time, if no navigation happens.
IN_PROC_BROWSER_TEST_F(FetchLaterActivationTimeoutBrowserTest,
                       SendOnActivationTimeout) {
  const std::string target_url = kFetchLaterEndpoint;
  auto request_handlers = RegisterRequestHandlers({target_url});
  ASSERT_TRUE(server()->Start());

  // Creates a FetchLater request with activateAfter=2s.
  // It should be sent out after 2s.
  RunScript(JsReplace(R"(
    fetchLater($1, {activateAfter: 2000});
  )",
                      target_url));
  ASSERT_FALSE(current_document().IsDestroyed());

  // The loader should still exist as the page exists.
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
  // The FetchLater request should be sent, triggered by its activateAfter.
  ExpectFetchLaterRequests(1, request_handlers);
}

// A pending FetchLater request should be sent when its page is evicted out of
// BackForwardCache.
IN_PROC_BROWSER_TEST_F(FetchLaterActivationTimeoutBrowserTest,
                       SendOnBackForwardCachedEviction) {
  const std::string target_url = kFetchLaterEndpoint;
  auto request_handlers = RegisterRequestHandlers({target_url});
  ASSERT_TRUE(server()->Start());

  // Creates a FetchLater request with long activateAfter (3min)
  RunScriptAndNavigateAway(JsReplace(R"(
    fetchLater($1, {activateAfter: 180000});
  )",
                                     target_url));
  ASSERT_TRUE(previous_document()->IsInBackForwardCache());
  // Forces evicting previous page. This will also post a task that destroys it.
  DisableBFCacheForRFHForTesting(previous_document()->GetGlobalId());
  ASSERT_TRUE(previous_document()->is_evicted_from_back_forward_cache());
  // Eviction happens immediately, but RFH deletion may be delayed.
  ASSERT_TRUE(previous_document().WaitUntilRenderFrameDeleted());

  // The loader is disconnected after the page is evicted by browser process to
  // start loading the request. However, it may happen earlier or later, so it's
  // difficult to assert the existence of the disconnected loader.

  // At the end, the FetchLater request should be sent, and the loader is
  // expected to process the response.
  ExpectFetchLaterRequests(1, request_handlers);
}

// All other send-on-BFCache behaviors are covered in
// send-on-deactivate.tentative.https.window.js

}  // namespace content
