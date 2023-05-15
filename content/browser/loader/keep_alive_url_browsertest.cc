// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/keep_alive_url_loader_service.h"

#include <memory>
#include <tuple>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/test/allow_check_is_test_for_testing.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/thread_annotations.h"
#include "content/browser/loader/keep_alive_url_loader.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

constexpr char16_t kPromiseResolvedPageTitle[] = u"Resolved";

constexpr char kPrimaryHost[] = "a.com";
constexpr char kSecondaryHost[] = "b.com";

constexpr char kKeepAliveEndpoint[] = "/beacon";

constexpr char k200TextResponse[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "\r\n";

// `arg` is a 2-tuple (URLLoaderCompletionStatus, int).
MATCHER(ErrorCodeEq, "match the same error code") {
  const auto& expected = std::get<1>(arg);
  const auto& got = std::get<0>(arg).error_code;
  if (got != expected) {
    *result_listener << "expected error code [" << expected << "] got [" << got
                     << "]";
    return false;
  }
  return true;
}

// Help to count the total triggering of one of methods observed by
// `KeepAliveURLLoadersTestObserver`.
// Use `WaitUntil()` to wait until this counter reaching specific value.
class AtomicCounter {
 public:
  AtomicCounter() = default;
  // Not Copyable.
  AtomicCounter(const AtomicCounter&) = delete;
  AtomicCounter& operator=(const AtomicCounter&) = delete;

  // Increments the internal counter, and stops `waiting_run_loop_` if exists.
  void Increment() {
    base::AutoLock auto_lock(lock_);
    count_++;
    if (waiting_run_loop_) {
      waiting_run_loop_->Quit();
    }
  }

  // If `count_` does not yet reach `value`, a RunLoop will be created and runs
  // until it is stopped by `Increment()`.
  void WaitUntil(size_t value) {
    {
      base::AutoLock auto_lock(lock_);
      if (count_ >= value) {
        return;
      }
    }

    {
      base::AutoLock auto_lock(lock_);
      waiting_run_loop_ = std::make_unique<base::RunLoop>(
          base::RunLoop::Type::kNestableTasksAllowed);
    }
    waiting_run_loop_->Run();

    {
      base::AutoLock auto_lock(lock_);
      waiting_run_loop_.reset();
    }
  }

 private:
  base::Lock lock_;
  size_t count_ GUARDED_BY(lock_) = 0;
  std::unique_ptr<base::RunLoop> waiting_run_loop_ = nullptr;
};

// Observes all created KeepAliveURLLoader instances' behaviors.
// KeepAliveURLLoader itself is running in browser UI thread. But there can be
// multiple instances.
class KeepAliveURLLoadersTestObserver
    : public KeepAliveURLLoader::TestObserver {
 public:
  KeepAliveURLLoadersTestObserver() = default;
  // Not Copyable.
  KeepAliveURLLoadersTestObserver(const KeepAliveURLLoadersTestObserver&) =
      delete;
  KeepAliveURLLoadersTestObserver& operator=(
      const KeepAliveURLLoadersTestObserver&) = delete;

  // Waits for `OnReceiveRedirectForwarded` to be called `total` times.
  void WaitForTotalOnReceiveRedirectForwarded(size_t total) {
    on_receive_redirect_forwarded_count_.WaitUntil(total);
  }
  // Waits for `OnReceiveRedirectProcessed` to be called `total` times.
  void WaitForTotalOnReceiveRedirectProcessed(size_t total) {
    on_receive_redirect_processed_count_.WaitUntil(total);
  }
  // Waits for `OnReceiveResponseForwarded` to be called `total` times.
  void WaitForTotalOnReceiveResponseForwarded(size_t total) {
    on_receive_response_forwarded_count_.WaitUntil(total);
  }
  // Waits for `OnReceiveResponseProcessed` to be called `total` times.
  void WaitForTotalOnReceiveResponseProcessed(size_t total) {
    on_receive_response_processed_count_.WaitUntil(total);
  }
  // Waits for `OnCompleteForwarded` to be called `error_codes.size()` times,
  // and the error codes from `on_complete_forwarded_status_` should match
  // `error_codes`.
  void WaitForTotalOnCompleteForwarded(const std::vector<int>& error_codes) {
    on_complete_forwarded_count_.WaitUntil(error_codes.size());
    EXPECT_THAT(on_complete_forwarded_status_,
                testing::Pointwise(ErrorCodeEq(), error_codes));
  }
  // Waits for `OnCompleteProcessed` to be called `error_codes.size()` times,
  // and the error codes from `on_complete_processed_status_` should match
  // `error_codes`.
  void WaitForTotalOnCompleteProcessed(const std::vector<int>& error_codes) {
    on_complete_processed_count_.WaitUntil(error_codes.size());
    EXPECT_THAT(on_complete_processed_status_,
                testing::Pointwise(ErrorCodeEq(), error_codes));
  }

 protected:
  ~KeepAliveURLLoadersTestObserver() override = default;

 private:
  // KeepAliveURLLoader::TestObserver overrides:
  void OnReceiveRedirectForwarded(KeepAliveURLLoader* loader) override {
    on_receive_redirect_forwarded_count_.Increment();
  }
  void OnReceiveRedirectProcessed(KeepAliveURLLoader* loader) override {
    on_receive_redirect_processed_count_.Increment();
  }
  void OnReceiveResponseForwarded(KeepAliveURLLoader* loader) override {
    on_receive_response_forwarded_count_.Increment();
  }
  void OnReceiveResponseProcessed(KeepAliveURLLoader* loader) override {
    on_receive_response_processed_count_.Increment();
  }
  void OnCompleteForwarded(
      KeepAliveURLLoader* loader,
      const network::URLLoaderCompletionStatus& completion_status) override {
    on_complete_forwarded_count_.Increment();
    on_complete_forwarded_status_.push_back(completion_status);
  }
  void OnCompleteProcessed(
      KeepAliveURLLoader* loader,
      const network::URLLoaderCompletionStatus& completion_status) override {
    on_complete_processed_count_.Increment();
    on_complete_processed_status_.push_back(completion_status);
  }

  AtomicCounter on_receive_redirect_forwarded_count_;
  AtomicCounter on_receive_redirect_processed_count_;
  AtomicCounter on_receive_response_forwarded_count_;
  AtomicCounter on_receive_response_processed_count_;
  AtomicCounter on_complete_forwarded_count_;
  AtomicCounter on_complete_processed_count_;
  std::vector<network::URLLoaderCompletionStatus> on_complete_forwarded_status_;
  std::vector<network::URLLoaderCompletionStatus> on_complete_processed_status_;
};

}  // namespace

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
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<std::string> {
 public:
  KeepAliveURLBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            {{blink::features::kKeepAliveInBrowserMigration, {}}}),
        GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }
  ~KeepAliveURLBrowserTest() override = default;
  // Not Copyable.
  KeepAliveURLBrowserTest(const KeepAliveURLBrowserTest&) = delete;
  KeepAliveURLBrowserTest& operator=(const KeepAliveURLBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    base::test::AllowCheckIsTestForTesting();
    loaders_observer_ = base::MakeRefCounted<KeepAliveURLLoadersTestObserver>();
    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");
    loader_service()->SetLoaderObserverForTesting(loaders_observer_);

    ContentBrowserTest::SetUpOnMainThread();
  }

 protected:
  // Navigates to a page specified by `keepalive_page_url`, which must fire a
  // fetch keepalive request.
  // The method then postpones the request handling until RFH of the page is
  // fully unloaded (by navigating to another cross-origin page).
  // After that, `response` will be sent back.
  // `keepalive_request_handler` must handle the fetch keepalive request.
  void LoadPageWithKeepaliveRequestAndSendResponseAfterUnload(
      const GURL& keepalive_page_url,
      net::test_server::ControllableHttpResponse* keepalive_request_handler,
      const std::string& response) {
    ASSERT_TRUE(NavigateToURL(web_contents(), keepalive_page_url));
    RenderFrameHostImplWrapper rfh_1(current_frame_host());
    // Ensures the current page can be unloaded instead of being cached.
    DisableBackForwardCache(web_contents());
    // Ensures the keepalive request is sent before leaving the current page.
    keepalive_request_handler->WaitForRequest();
    ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);

    // Navigates to cross-origin page to ensure the 1st page can be unloaded.
    ASSERT_TRUE(NavigateToURL(web_contents(), GetCrossOriginPageURL()));
    // Ensures the 1st page has been unloaded.
    ASSERT_TRUE(rfh_1.WaitUntilRenderFrameDeleted());
    // The disconnected loader is still pending to receive response.
    ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);
    ASSERT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 1u);

    // Sends back response to terminate in-browser request handling for the
    // pending request from 1st page.
    keepalive_request_handler->Send(response);
    keepalive_request_handler->Done();
  }

  [[nodiscard]] std::vector<
      std::unique_ptr<net::test_server::ControllableHttpResponse>>
  RegisterRequestHandlers(const std::vector<std::string>& relative_urls) {
    std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
        handlers;
    for (const auto& relative_url : relative_urls) {
      handlers.emplace_back(
          std::make_unique<net::test_server::ControllableHttpResponse>(
              embedded_test_server(), relative_url));
    }
    return handlers;
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

  GURL GetKeepalivePageURL(const std::string& method,
                           size_t num_requests = 1,
                           bool set_csp = false) const {
    return embedded_test_server()->GetURL(
        kPrimaryHost,
        base::StringPrintf(
            "/set-header-with-file/content/test/data/fetch-keepalive.html?"
            "method=%s&requests=%zu%s",
            method.c_str(), num_requests,
            set_csp
                ? "&Content-Security-Policy: connect-src 'self' http://csp.test"
                : ""));
  }
  GURL GetCrossOriginPageURL() {
    return embedded_test_server()->GetURL(kSecondaryHost, "/title2.html");
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  scoped_refptr<KeepAliveURLLoadersTestObserver> loaders_observer_;
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
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(NavigateToURL(web_contents(), GetKeepalivePageURL(method)));
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
}

// Verify keepalive request loading works given 2 concurrent requests to the
// same host.
//
// Note: Chromium allows at most 6 concurrent connections to the same host under
// HTTP 1.1 protocol, which `embedded_test_server()` uses by default.
// Exceeding this limit will hang the browser.
// TODO(crbug.com/1428502): Flaky on Fuchsia and Android.
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest,
                       DISABLED_TwoConcurrentRequestsPerHost) {
  const std::string method = GetParam();
  const size_t num_requests = 2;
  auto request_handlers =
      RegisterRequestHandlers({kKeepAliveEndpoint, kKeepAliveEndpoint});
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(
      NavigateToURL(web_contents(), GetKeepalivePageURL(method, num_requests)));
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

// Delays response to a keepalive ping until after the page making the keepalive
// ping has been unloaded. The browser must ensure the response is received and
// processed by the browser.
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest,
                       ReceiveResponseAfterPageUnload) {
  const std::string method = GetParam();
  auto request_handler =
      std::move(RegisterRequestHandlers({kKeepAliveEndpoint})[0]);
  ASSERT_TRUE(embedded_test_server()->Start());

  LoadPageWithKeepaliveRequestAndSendResponseAfterUnload(
      GetKeepalivePageURL(method), request_handler.get(), k200TextResponse);

  // The response should be processed in browser.
  loaders_observer().WaitForTotalOnReceiveResponseProcessed(1);
  // `KeepAliveURLLoader::OnComplete` may not be called, as renderer is dead.
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
}

// Delays response to a keepalive ping until after the page making the keepalive
// ping is put into BackForwardCache. The response should be processed by the
// renderer after the page is restored from BackForwardCache.
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest,
                       ReceiveResponseInBackForwardCache) {
  const std::string method = GetParam();
  auto request_handler =
      std::move(RegisterRequestHandlers({kKeepAliveEndpoint})[0]);
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(NavigateToURL(web_contents(), GetKeepalivePageURL(method)));
  RenderFrameHostImplWrapper rfh_1(current_frame_host());
  // Ensure the keepalive request is sent before leaving the current page.
  request_handler->WaitForRequest();
  ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);

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
  ASSERT_TRUE(embedded_test_server()->Start());

  // Sets up redirects according to the following redirect chain:
  // fetch("http://a.com:<port>/beacon", keepalive: true)
  // --> http://a.com:<port>/beacon-redirected
  LoadPageWithKeepaliveRequestAndSendResponseAfterUnload(
      GetKeepalivePageURL(method), request_handlers[0].get(),
      base::StringPrintf("HTTP/1.1 301 Moved Permanently\r\n"
                         "Location: %s\r\n"
                         "\r\n",
                         redirect_target));

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
  ASSERT_TRUE(embedded_test_server()->Start());

  // Sets up redirects according to the following redirect chain:
  // fetch("http://a.com:<port>/beacon", keepalive: true)
  // --> chrome://settings
  LoadPageWithKeepaliveRequestAndSendResponseAfterUnload(
      GetKeepalivePageURL(method), request_handler.get(),
      base::StringPrintf("HTTP/1.1 301 Moved Permanently\r\n"
                         "Location: %s\r\n"
                         "\r\n",
                         unsafe_redirect_target));

  // The redirect is unsafe, so the loader is terminated.
  loaders_observer().WaitForTotalOnCompleteProcessed(
      {net::ERR_UNSAFE_REDIRECT});
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);
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
  ASSERT_TRUE(embedded_test_server()->Start());

  // Sets up redirects according to the following redirect chain:
  // fetch("http://a.com:<port>/beacon", keepalive: true)
  // --> http://b.com/beacon-redirected
  LoadPageWithKeepaliveRequestAndSendResponseAfterUnload(
      GetKeepalivePageURL(method, /*num_requests=*/1, /*set_csp=*/true),
      request_handler.get(),
      base::StringPrintf("HTTP/1.1 301 Moved Permanently\r\n"
                         "Location: %s\r\n"
                         "\r\n",
                         violating_csp_redirect_target));

  // The redirect doesn't match CSP source from the 1st page, so the loader is
  // terminated.
  loaders_observer().WaitForTotalOnCompleteProcessed({net::ERR_BLOCKED_BY_CSP});
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);
}

}  // namespace content
