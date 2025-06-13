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
#include "build/build_config.h"
#include "content/browser/attribution_reporting/test/mock_attribution_data_host_manager.h"
#include "content/browser/attribution_reporting/test/mock_attribution_manager.h"
#include "content/browser/back_forward_cache_test_util.h"
#include "content/browser/loader/keep_alive_request_browsertest_util.h"
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
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace content {
namespace {

using testing::Contains;
using testing::Pair;

constexpr char16_t kPromiseResolvedPageTitle[] = u"Resolved";

std::string GetConnectSrcCSPHeader(const url::Origin& origin) {
  return base::StringPrintf("Content-Security-Policy: connect-src 'self' %s",
                            origin.Serialize().c_str());
}

MATCHER(IsFrameHidden,
        base::StrCat({"Frame is", negation ? " not" : "", " hidden"})) {
  return arg->GetVisibilityState() == PageVisibilityState::kHidden;
}

}  // namespace

class FetchKeepAliveCommonTestBase : public KeepAliveRequestBrowserTestBase {
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

class KeepAliveURLAttributionReportingBrowserTest
    : public FetchKeepAliveCommonTestBase,
      public ::testing::WithParamInterface<std::string> {
 protected:
  void SetUp() override {
    // Attribution Reporting API only supports HTTPS requests.
    SetUseHttps();
    FetchKeepAliveCommonTestBase::SetUp();
  }

  void SetUpOnMainThread() override {
    auto mock_manager = std::make_unique<MockAttributionManager>();
    auto mock_data_host_manager =
        std::make_unique<MockAttributionDataHostManager>();
    mock_manager->SetDataHostManager(std::move(mock_data_host_manager));
    static_cast<StoragePartitionImpl*>(
        web_contents()->GetBrowserContext()->GetDefaultStoragePartition())
        ->OverrideAttributionManagerForTesting(std::move(mock_manager));

    FetchKeepAliveCommonTestBase::SetUpOnMainThread();
  }

  const FeaturesType& GetEnabledFeatures() override {
    static const FeaturesType enabled_features =
        GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            {{blink::features::kKeepAliveInBrowserMigration, {}},
             {blink::features::kAttributionReportingInBrowserMigration, {}}});
    return enabled_features;
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    KeepAliveURLAttributionReportingBrowserTest,
    ::testing::Values(net::HttpRequestHeaders::kGetMethod,
                      net::HttpRequestHeaders::kPostMethod),
    [](const testing::TestParamInfo<
        KeepAliveURLAttributionReportingBrowserTest::ParamType>& info) {
      return info.param;
    });

IN_PROC_BROWSER_TEST_P(KeepAliveURLAttributionReportingBrowserTest,
                       ReceiveViolatingCSPRedirect_NotForwarded) {
  const std::string method = GetParam();
  const char violating_csp_redirect_target[] =
      "http://b.test/beacon-redirected";
  auto request_handler =
      std::move(RegisterRequestHandlers({kKeepAliveEndpoint})[0]);
  ASSERT_TRUE(server()->Start());
  const GURL allowed_csp_url = server()->GetURL(kAllowedCspHost, "/");

  auto* data_host_manager = static_cast<MockAttributionDataHostManager*>(
      AttributionManager::FromWebContents(web_contents())
          ->GetDataHostManager());
  EXPECT_CALL(*data_host_manager, NotifyBackgroundRegistrationStarted).Times(1);
  EXPECT_CALL(*data_host_manager, NotifyBackgroundRegistrationData).Times(0);
  EXPECT_CALL(*data_host_manager, NotifyBackgroundRegistrationCompleted)
      .Times(1);

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
}

class KeepAliveFetchRetryBrowserTest
    : public FetchKeepAliveCommonTestBase,
      public ::testing::WithParamInterface<std::string> {
 protected:
  static constexpr int kMaxRetryCountPerLoaderForTesting = 2;
  static constexpr int kMaxRetryCountPerNetworkIsolationKeyForTesting = 3;
  static constexpr int kMaxRetryCountPerFactoryForTesting = 4;

  void SetUp() override {
    SetUseHttps();
    FetchKeepAliveCommonTestBase::SetUp();
  }

  const FeaturesType& GetEnabledFeatures() override {
    static const FeaturesType enabled_features =
        GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            {{blink::features::kFetchRetry,
              {
                  {"max_retry_count",
                   base::NumberToString(kMaxRetryCountPerLoaderForTesting)},
                  {"max_retries_per_factory",
                   base::NumberToString(kMaxRetryCountPerFactoryForTesting)},
                  {"max_retries_per_nik",
                   base::NumberToString(
                       kMaxRetryCountPerNetworkIsolationKeyForTesting)},
                  // Retry almost immediately to avoid timing out in tests.
                  {"min_retry_delta", "1ms"},
                  {"min_retry_backoff", "1.0"},
                  {"max_retry_age", "1d"},
              }}});
    return enabled_features;
  }

  void LoadPageAndTriggerFetchKeepaliveWithRetry(const GURL& fetch_url) {
    // Note that we explicitly want to trigger the fetch separately from
    // navigating to the page that contains it, because if we trigger the fetch
    // during the initial load, the IPC to create the KeepAliveURLLoader might
    // arrive earlier than the DidCommit IPC for the navigation, causing the
    // loader to not have the correct NetworkIsolationKey, etc.
    // TODO(rakina): File a bug about the race condition, which is orthogonal
    // from the fetch retry feature.
    ASSERT_TRUE(NavigateToURL(web_contents(),
                              server()->GetURL(kPrimaryHost, "/title1.html")));
    TriggerFetchKeepaliveWithRetry(fetch_url);
  }

  void TriggerFetchKeepaliveWithRetry(const GURL& fetch_url) {
    ASSERT_TRUE(ExecJs(
        web_contents(),
        JsReplace(R"(
                        window.fetchPromise = fetch($1, {
                          keepalive: true,
                          method: $2,
                          retryOptions: {
                            maxAttempts: $3,
                            retryAfterUnload: true,
                            retryNonIdempotent: true
                          }});)",
                  fetch_url, GetParam(), kMaxRetryCountPerLoaderForTesting),
        content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  }

  void ExpectFetchResolvedInJavaScript(bool result_is_ok) {
    EXPECT_EQ(result_is_ok, EvalJs(web_contents(), R"((async function() {
        try {
          let result = await window.fetchPromise;
          return result.ok;
        } catch (e) {
          // The fetch failed.
          return false;
        }
    })())"));
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    KeepAliveFetchRetryBrowserTest,
    ::testing::Values(net::HttpRequestHeaders::kGetMethod,
                      net::HttpRequestHeaders::kPostMethod),
    [](const testing::TestParamInfo<KeepAliveFetchRetryBrowserTest::ParamType>&
           info) { return info.param; });

// Test failing a load due to network changed error, then having the retry
// succeed.
IN_PROC_BROWSER_TEST_P(KeepAliveFetchRetryBrowserTest,
                       FailedRetriedThenSucceeded) {
  ASSERT_TRUE(server()->Start());
  const auto beacon_url = server()->GetURL(kPrimaryHost, kKeepAliveEndpoint);
  int request_count = 0;
  std::string initial_request_guid;
  URLLoaderInterceptor url_interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url != beacon_url) {
          return false;
        }
        request_count++;
        if (request_count == 1) {
          // Fail the first fetch with a network changed error.
          params->client->OnComplete(
              network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED));
          initial_request_guid =
              params->url_request.headers
                  .GetHeader(KeepAliveURLLoader::kRetryGuidHeader)
                  .value();
          return true;
        } else {
          //  Ensure the retry succeeds.
          EXPECT_EQ(params->url_request.headers.GetHeader(
                        KeepAliveURLLoader::kRetryAttemptsHeader),
                    "1");
          EXPECT_EQ(params->url_request.headers
                        .GetHeader(KeepAliveURLLoader::kRetryGuidHeader)
                        .value(),
                    initial_request_guid);
          URLLoaderInterceptor::WriteResponse(
              "HTTP/1.1 200 OK\n"
              "Content-type: text/html\n",
              "\r\n", params->client.get());
          return true;
        }
      }));

  LoadPageAndTriggerFetchKeepaliveWithRetry(beacon_url);

  // We fail once and then succeed on the retry.
  loaders_observer().WaitForTotalOnComplete(
      {net::ERR_NETWORK_CHANGED, net::OK});
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);

  // Only the last result (success) is forwarded to the renderer.
  loaders_observer().WaitForTotalOnCompleteForwarded({net::OK});
  ExpectFetchResolvedInJavaScript(/*result_is_ok=*/true);
  ExpectFetchKeepAliveHistogram(
      FetchKeepAliveRequestMetricType::kFetch,
      ExpectedTotalRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedStartedRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedSucceededRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedFailedRequests(/*browser=*/1, /*renderer=*/0),
      /*retried_count=*/1);
}

// Test failing a load due to HTTP 500 error. The request should not be retried.
IN_PROC_BROWSER_TEST_P(KeepAliveFetchRetryBrowserTest,
                       FailedNotRetried_HTTPError) {
  net::test_server::ControllableHttpResponse response(server(),
                                                      kKeepAliveEndpoint);
  ASSERT_TRUE(server()->Start());
  const auto beacon_url = server()->GetURL(kPrimaryHost, kKeepAliveEndpoint);
  LoadPageAndTriggerFetchKeepaliveWithRetry(beacon_url);
  // Send a HTTP 500 response. This should not be retried, as it's not a network
  // error.
  response.WaitForRequest();
  response.Send(net::HTTP_INTERNAL_SERVER_ERROR);
  response.Done();
  loaders_observer().WaitForTotalOnReceiveResponse(1);
  loaders_observer().WaitForTotalOnComplete({net::OK});
  loaders_observer().WaitForTotalOnCompleteForwarded({net::OK});
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);

  // The HTTP error is deemed as a success by the renderer (since it's not a
  // network error). Note that we don't check the failed count on the renderer
  // because sometimes the error is also counted on the renderer when loading
  // the body (this is unrelated to the retry logic at all, which is not
  // triggered in this case).
  ExpectFetchResolvedInJavaScript(/*result_is_ok=*/false);
  ExpectFetchKeepAliveHistogram(
      FetchKeepAliveRequestMetricType::kFetch,
      ExpectedTotalRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedStartedRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedSucceededRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedFailedRequests(/*browser=*/0),
      /*retried_count=*/0);
}

// Test that a failure wit ha network error that doesn't trigger a retry
IN_PROC_BROWSER_TEST_P(KeepAliveFetchRetryBrowserTest,
                       FailedNotRetried_NonRetryEligibleNetworkError) {
  ASSERT_TRUE(server()->Start());
  const auto beacon_url = server()->GetURL(kPrimaryHost, kKeepAliveEndpoint);
  // Always fail the fetch with a SSL protocol error.
  std::unique_ptr<URLLoaderInterceptor> url_interceptor =
      URLLoaderInterceptor::SetupRequestFailForURL(beacon_url,
                                                   net::ERR_SSL_PROTOCOL_ERROR);
  LoadPageAndTriggerFetchKeepaliveWithRetry(beacon_url);

  // Observe the error, which is invalid for retrying.
  loaders_observer().WaitForTotalOnComplete({net::ERR_SSL_PROTOCOL_ERROR});
  loaders_observer().WaitForTotalOnCompleteForwarded(
      {net::ERR_SSL_PROTOCOL_ERROR});
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);

  // The fetch is not retried.
  ExpectFetchResolvedInJavaScript(/*result_is_ok=*/false);
  ExpectFetchKeepAliveHistogram(
      FetchKeepAliveRequestMetricType::kFetch,
      ExpectedTotalRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedStartedRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedSucceededRequests(/*browser=*/0, /*renderer=*/0),
      ExpectedFailedRequests(/*browser=*/1, /*renderer=*/1),
      /*retried_count=*/0);
}

// Test failing a load and all the retries.
IN_PROC_BROWSER_TEST_P(KeepAliveFetchRetryBrowserTest,
                       FailedRetriedUntilMaxRetryCount) {
  ASSERT_TRUE(server()->Start());
  const auto beacon_url = server()->GetURL(kPrimaryHost, kKeepAliveEndpoint);
  // Always fail the fetch with a network changed error.
  std::unique_ptr<URLLoaderInterceptor> url_interceptor =
      URLLoaderInterceptor::SetupRequestFailForURL(beacon_url,
                                                   net::ERR_NETWORK_CHANGED);
  LoadPageAndTriggerFetchKeepaliveWithRetry(beacon_url);

  // Max retry is 2 in the test, so we'll get 3 failures in total.
  std::vector<int> errors = {net::ERR_NETWORK_CHANGED, net::ERR_NETWORK_CHANGED,
                             net::ERR_NETWORK_CHANGED};
  loaders_observer().WaitForTotalOnComplete(errors);
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);

  // Only the last failure is forwarded to the renderer.
  loaders_observer().WaitForTotalOnCompleteForwarded(
      {net::ERR_NETWORK_CHANGED});
  ExpectFetchResolvedInJavaScript(/*result_is_ok=*/false);
  ExpectFetchKeepAliveHistogram(
      FetchKeepAliveRequestMetricType::kFetch,
      ExpectedTotalRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedStartedRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedSucceededRequests(/*browser=*/0, /*renderer=*/0),
      ExpectedFailedRequests(/*browser=*/3, /*renderer=*/1),
      /*retried_count=*/kMaxRetryCountPerLoaderForTesting);

  // Trigger another fetch keepalive.
  TriggerFetchKeepaliveWithRetry(beacon_url);

  // Max retry is 2 for the loader, but since it's coming from the same
  // NetworkIsolationKey, which has a max retry of 3 attempts per
  // NetworkIsolationKey, we'll only do 1 fetch and 1 retry, totaling in 5
  // OnCompletes.
  errors.insert(errors.end(),
                {net::ERR_NETWORK_CHANGED, net::ERR_NETWORK_CHANGED});
  loaders_observer().WaitForTotalOnComplete(errors);
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);

  // The last failure is forwarded to the renderer too.
  loaders_observer().WaitForTotalOnCompleteForwarded(
      {net::ERR_NETWORK_CHANGED, net::ERR_NETWORK_CHANGED});
  ExpectFetchResolvedInJavaScript(/*result_is_ok=*/false);
  FetchHistogramsFromChildProcesses();
  ExpectFetchKeepAliveHistogram(
      FetchKeepAliveRequestMetricType::kFetch,
      ExpectedTotalRequests(/*browser=*/2, /*renderer=*/2),
      ExpectedStartedRequests(/*browser=*/2, /*renderer=*/2),
      ExpectedSucceededRequests(/*browser=*/0, /*renderer=*/0),
      ExpectedFailedRequests(/*browser=*/5, /*renderer=*/2),
      /*retried_count=*/kMaxRetryCountPerNetworkIsolationKeyForTesting);

  // Triggering another fetch won't result in any retries.
  TriggerFetchKeepaliveWithRetry(beacon_url);
  errors.push_back(net::ERR_NETWORK_CHANGED);
  loaders_observer().WaitForTotalOnComplete(errors);
  loaders_observer().WaitForTotalOnCompleteForwarded(
      {net::ERR_NETWORK_CHANGED, net::ERR_NETWORK_CHANGED,
       net::ERR_NETWORK_CHANGED});
  ExpectFetchResolvedInJavaScript(/*result_is_ok=*/false);
  FetchHistogramsFromChildProcesses();
  ExpectFetchKeepAliveHistogram(
      FetchKeepAliveRequestMetricType::kFetch,
      ExpectedTotalRequests(/*browser=*/3, /*renderer=*/3),
      ExpectedStartedRequests(/*browser=*/3, /*renderer=*/3),
      ExpectedSucceededRequests(/*browser=*/0, /*renderer=*/0),
      ExpectedFailedRequests(/*browser=*/6, /*renderer=*/3),
      /*retried_count=*/kMaxRetryCountPerNetworkIsolationKeyForTesting);

  // Navigate cross-origin to create a fetch with a different initiator
  // NetworkIsolationKey.
  ASSERT_TRUE(NavigateToURL(web_contents(), GetCrossOriginPageURL()));
  TriggerFetchKeepaliveWithRetry(beacon_url);

  // The fetch will fail, then only 1 retry will be attempted, because we hit
  // the max retry per factory (4).
  errors.insert(errors.end(),
                {net::ERR_NETWORK_CHANGED, net::ERR_NETWORK_CHANGED});
  loaders_observer().WaitForTotalOnComplete(errors);
  loaders_observer().WaitForTotalOnCompleteForwarded(
      {net::ERR_NETWORK_CHANGED, net::ERR_NETWORK_CHANGED,
       net::ERR_NETWORK_CHANGED, net::ERR_NETWORK_CHANGED});
  ExpectFetchResolvedInJavaScript(/*result_is_ok=*/false);
  FetchHistogramsFromChildProcesses();
  ExpectFetchKeepAliveHistogram(
      FetchKeepAliveRequestMetricType::kFetch,
      ExpectedTotalRequests(/*browser=*/4, /*renderer=*/4),
      ExpectedStartedRequests(/*browser=*/4, /*renderer=*/4),
      ExpectedSucceededRequests(/*browser=*/0, /*renderer=*/0),
      ExpectedFailedRequests(/*browser=*/8, /*renderer=*/4),
      /*retried_count=*/kMaxRetryCountPerFactoryForTesting);

  // Triggering another fetch won't result in any retries.
  TriggerFetchKeepaliveWithRetry(beacon_url);
  errors.push_back(net::ERR_NETWORK_CHANGED);
  loaders_observer().WaitForTotalOnComplete(errors);
  loaders_observer().WaitForTotalOnCompleteForwarded(
      {net::ERR_NETWORK_CHANGED, net::ERR_NETWORK_CHANGED,
       net::ERR_NETWORK_CHANGED, net::ERR_NETWORK_CHANGED,
       net::ERR_NETWORK_CHANGED});
  ExpectFetchResolvedInJavaScript(/*result_is_ok=*/false);
  FetchHistogramsFromChildProcesses();
  ExpectFetchKeepAliveHistogram(
      FetchKeepAliveRequestMetricType::kFetch,
      ExpectedTotalRequests(/*browser=*/5, /*renderer=*/5),
      ExpectedStartedRequests(/*browser=*/5, /*renderer=*/5),
      ExpectedSucceededRequests(/*browser=*/0, /*renderer=*/0),
      ExpectedFailedRequests(/*browser=*/9, /*renderer=*/5),
      /*retried_count=*/kMaxRetryCountPerFactoryForTesting);
}

// Test that a loader attempting retry will be deleted by clearing
// cookies/cache.
IN_PROC_BROWSER_TEST_P(KeepAliveFetchRetryBrowserTest,
                       ClearingDataClearsLoaderAttemptingRetry) {
  ASSERT_TRUE(server()->Start());
  const auto beacon_url = server()->GetURL(kPrimaryHost, kKeepAliveEndpoint);
  // Always fail the fetch with a network changed error.
  std::unique_ptr<URLLoaderInterceptor> url_interceptor =
      URLLoaderInterceptor::SetupRequestFailForURL(beacon_url,
                                                   net::ERR_NETWORK_CHANGED);
  ASSERT_TRUE(NavigateToURL(web_contents(),
                            server()->GetURL(kPrimaryHost, "/title1.html")));
  ASSERT_TRUE(ExecJs(web_contents(),
                     JsReplace(R"(
                      window.fetchPromise = fetch($1, {
                        keepalive: true,
                        retryOptions: {
                          maxAttempts: 10,
                          initialDelay: (1000 * 3600 * 24), // 1 day
                          retryNonIdempotent: true
                        }});)",
                               beacon_url),
                     content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Wait for the first load to fail.
  loaders_observer().WaitForTotalOnComplete({net::ERR_NETWORK_CHANGED});

  // The loader is now waiting for its retry attempt (which is scheduled for
  // 1 day from now).
  ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);
  ASSERT_EQ(loader_service()->NumLoadersAttemptingRetryForTesting(), 1u);

  // Simulate clearing cookies.
  base::RunLoop run_loop;
  static_cast<StoragePartitionImpl*>(
      web_contents()->GetBrowserContext()->GetDefaultStoragePartition())
      ->ClearData(StoragePartition::REMOVE_KEEPALIVE_LOADS_ATTEMPTING_RETRY,
                  StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
                  blink::StorageKey(), base::Time(), base::Time::Max(),
                  run_loop.QuitClosure());
  run_loop.Run();
  // The pending retry loader is deleted.
  ASSERT_EQ(loader_service()->NumLoadersForTesting(), 0u);
  ASSERT_EQ(loader_service()->NumLoadersAttemptingRetryForTesting(), 0u);

  // No retry happened and the failure is not forwarded to the renderer because
  // the loader is deleted while it's waiting for retry.
  ExpectFetchKeepAliveHistogram(
      FetchKeepAliveRequestMetricType::kFetch,
      ExpectedTotalRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedStartedRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedSucceededRequests(/*browser=*/0, /*renderer=*/0),
      ExpectedFailedRequests(/*browser=*/1),
      /*retried_count=*/0);
}

// TODO(crbug.com/417930271): test unload, redirects, timeout, attribution.

}  // namespace content
