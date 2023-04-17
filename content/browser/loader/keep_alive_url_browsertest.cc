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
constexpr char kKeepAliveRedirectedEndpoint[] = "/beacon-redirected";

constexpr char kKeepAliveResponse[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "\r\n";
constexpr char kKeepAliveRedirectToSameOriginResponse[] =
    "HTTP/1.1 301 Moved Permanently\r\n"
    "Location: /beacon-redirected\r\n"
    "\r\n";
constexpr char kKeepAliveRedirectToUnSafeResponse[] =
    "HTTP/1.1 301 Moved Permanently\r\n"
    "Location: chrome://settings\r\n"
    "\r\n";
constexpr char kKeepAliveRedirectToViolatingCSPResponse[] =
    "HTTP/1.1 301 Moved Permanently\r\n"
    "Location: http://b.com/beacon-redirected\r\n"
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

// A helper to manage responding to multiple fetch keepalive requests in batch.
class KeepAliveRequestsHandler {
 public:
  KeepAliveRequestsHandler(net::EmbeddedTestServer* server,
                           const std::vector<std::string>& relative_urls) {
    for (const auto& relative_url : relative_urls) {
      controllers_.emplace_back(
          std::make_unique<net::test_server::ControllableHttpResponse>(
              server, relative_url));
    }
  }
  // Not Copyable.
  KeepAliveRequestsHandler(const KeepAliveRequestsHandler&) = delete;
  KeepAliveRequestsHandler& operator=(const KeepAliveRequestsHandler&) = delete;

  net::test_server::ControllableHttpResponse& operator[](size_t i) {
    return *controllers_[i];
  }

  void WaitForAllRequests() {
    for (auto& controller : controllers_) {
      controller->WaitForRequest();
    }
  }

  void Send(const std::string& bytes) {
    for (auto& controller : controllers_) {
      controller->Send(bytes);
    }
  }

  void Done() {
    for (auto& controller : controllers_) {
      controller->Done();
    }
  }

 private:
  std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
      controllers_;
};

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

    // TODO(crbug.com/1356128): Update `embedded_test_server()` to support
    // serving contents + modified headers.
    ContentBrowserTest::SetUpOnMainThread();
  }

  void RegisterRequestsHandler(const std::vector<std::string>& relative_urls) {
    requests_handler_ = std::make_unique<KeepAliveRequestsHandler>(
        embedded_test_server(), relative_urls);
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
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
  KeepAliveRequestsHandler& requests_handler() { return *requests_handler_; }
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
  std::unique_ptr<KeepAliveRequestsHandler> requests_handler_;
  scoped_refptr<KeepAliveURLLoadersTestObserver> loaders_observer_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    KeepAliveURLBrowserTest,
    ::testing::Values(net::HttpRequestHeaders::kGetMethod/*,
                      net::HttpRequestHeaders::kPostMethod*/),
    [](const testing::TestParamInfo<KeepAliveURLBrowserTest::ParamType>& info) {
      return info.param;
    });

IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest, OneRequest) {
  const std::string method = GetParam();
  RegisterRequestsHandler({kKeepAliveEndpoint});
  ASSERT_TRUE(NavigateToURL(web_contents(), GetKeepalivePageURL(method)));
  // Ensure the keepalive request is sent, but delay response.
  requests_handler().WaitForAllRequests();
  ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);

  // End the keepalive request by sending back response.
  requests_handler().Send(kKeepAliveResponse);
  requests_handler().Done();

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
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
#define MAYBE_TwoConcurrentRequestsPerHost DISABLED_TwoConcurrentRequestsPerHost
#else
#define MAYBE_TwoConcurrentRequestsPerHost TwoConcurrentRequestsPerHost
#endif
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest,
                       MAYBE_TwoConcurrentRequestsPerHost) {
  const std::string method = GetParam();
  const size_t num_requests = 2;
  RegisterRequestsHandler({kKeepAliveEndpoint, kKeepAliveEndpoint});
  ASSERT_TRUE(
      NavigateToURL(web_contents(), GetKeepalivePageURL(method, num_requests)));
  // Ensure all keepalive requests are sent, but delay responses.
  requests_handler().WaitForAllRequests();
  ASSERT_EQ(loader_service()->NumLoadersForTesting(), num_requests);

  // End the keepalive request by sending back responses.
  requests_handler().Send(kKeepAliveResponse);
  requests_handler().Done();

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
  RegisterRequestsHandler({kKeepAliveEndpoint});
  ASSERT_TRUE(NavigateToURL(web_contents(), GetKeepalivePageURL(method)));
  RenderFrameHostImplWrapper rfh_1(current_frame_host());
  // Ensure the current page can be unloaded instead of being cached.
  DisableBackForwardCache(web_contents());
  // Ensure the keepalive request is sent before leaving the current page.
  requests_handler().WaitForAllRequests();
  ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);

  // Navigate to cross-origin page.
  ASSERT_TRUE(NavigateToURL(web_contents(), GetCrossOriginPageURL()));
  // Ensure the previous page has been unloaded.
  ASSERT_TRUE(rfh_1.WaitUntilRenderFrameDeleted());
  // The disconnected loader is still pending to receive response.
  ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);
  ASSERT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 1u);

  // End the keepalive request by sending back response.
  requests_handler().Send(kKeepAliveResponse);
  requests_handler().Done();

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
  RegisterRequestsHandler({kKeepAliveEndpoint});
  ASSERT_TRUE(NavigateToURL(web_contents(), GetKeepalivePageURL(method)));
  RenderFrameHostImplWrapper rfh_1(current_frame_host());
  // Ensure the keepalive request is sent before leaving the current page.
  requests_handler().WaitForAllRequests();
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
  requests_handler().Send(kKeepAliveResponse);
  // The response is immediately forwarded to the in-BackForwardCache renderer.
  loaders_observer().WaitForTotalOnReceiveResponseForwarded(1);
  // Go back to `rfh_1`.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  // The response should be processed in renderer. Hence resolving Promise.
  TitleWatcher watcher(web_contents(), kPromiseResolvedPageTitle);
  EXPECT_EQ(watcher.WaitAndGetTitle(), kPromiseResolvedPageTitle);
  requests_handler().Done();
  loaders_observer().WaitForTotalOnCompleteForwarded({net::OK});
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);
}

// Delays handling redirect for a keepalive ping until after the page making the
// keepalive ping has been unloaded. The browser must ensure the redirect is
// verified and properly processed by the browser.
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest,
                       ReceiveRedirectAfterPageUnload) {
  const std::string method = GetParam();
  RegisterRequestsHandler({kKeepAliveEndpoint, kKeepAliveRedirectedEndpoint});

  ASSERT_TRUE(NavigateToURL(web_contents(), GetKeepalivePageURL(method)));
  RenderFrameHostImplWrapper rfh_1(current_frame_host());
  // Ensure the current page can be unloaded instead of being cached.
  DisableBackForwardCache(web_contents());
  // Ensure the keepalive request is sent before leaving the current page.
  requests_handler()[0].WaitForRequest();
  ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);

  // Navigate to cross-origin page.
  ASSERT_TRUE(NavigateToURL(web_contents(), GetCrossOriginPageURL()));
  // Ensure the previous page has been unloaded.
  ASSERT_TRUE(rfh_1.WaitUntilRenderFrameDeleted());
  // The disconnected loader is still pending to receive response.
  ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);
  ASSERT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 1u);

  // Send back response to redirect.
  requests_handler()[0].Send(kKeepAliveRedirectToSameOriginResponse);
  requests_handler()[0].Done();
  // The in-browser logic should process the redirect.
  loaders_observer().WaitForTotalOnReceiveRedirectProcessed(1);

  // The redirect request should be processed in browser and gets sent.
  requests_handler()[1].WaitForRequest();
  // End the keepalive request by sending back final response.
  requests_handler()[1].Send(kKeepAliveResponse);
  requests_handler()[1].Done();

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
  RegisterRequestsHandler({kKeepAliveEndpoint});

  ASSERT_TRUE(NavigateToURL(web_contents(), GetKeepalivePageURL(method)));
  RenderFrameHostImplWrapper rfh_1(current_frame_host());
  // Ensure the current page can be unloaded instead of being cached.
  DisableBackForwardCache(web_contents());
  // Ensure the keepalive request is sent before leaving the current page.
  requests_handler()[0].WaitForRequest();
  ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);

  // Navigate to cross-origin page.
  ASSERT_TRUE(NavigateToURL(web_contents(), GetCrossOriginPageURL()));
  // Ensure the previous page has been unloaded.
  ASSERT_TRUE(rfh_1.WaitUntilRenderFrameDeleted());
  // The disconnected loader is still pending to receive response.
  ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);
  ASSERT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 1u);

  // Send back response to redirect to unsafe target.
  requests_handler()[0].Send(kKeepAliveRedirectToUnSafeResponse);
  requests_handler()[0].Done();

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
  RegisterRequestsHandler({kKeepAliveEndpoint});

  ASSERT_TRUE(NavigateToURL(
      web_contents(),
      GetKeepalivePageURL(method, /*num_requests=*/1, /*set_csp=*/true)));
  RenderFrameHostImplWrapper rfh_1(current_frame_host());
  // Ensure the current page can be unloaded instead of being cached.
  DisableBackForwardCache(web_contents());
  // Ensure the keepalive request is sent before leaving the current page.
  requests_handler()[0].WaitForRequest();
  ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);

  // Navigate to cross-origin page.
  ASSERT_TRUE(NavigateToURL(web_contents(), GetCrossOriginPageURL()));
  // Ensure the previous page has been unloaded.
  ASSERT_TRUE(rfh_1.WaitUntilRenderFrameDeleted());
  // The disconnected loader is still pending to receive response.
  ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);
  ASSERT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 1u);

  // Send back response to redirect to the target that violates CSP.
  requests_handler()[0].Send(kKeepAliveRedirectToViolatingCSPResponse);
  requests_handler()[0].Done();

  // The redirect doesn't match CSP source from the 1st page, so the loader is
  // terminated.
  loaders_observer().WaitForTotalOnCompleteProcessed({net::ERR_BLOCKED_BY_CSP});
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);
}

}  // namespace content
