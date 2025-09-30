// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/test/scoped_feature_list.h"
#include "base/thread_annotations.h"
#include "base/timer/elapsed_timer.h"
#include "content/browser/back_forward_cache_test_util.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/prefetch_test_util.h"
#include "content/public/test/preloading_test_util.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/cookies/canonical_cookie_test_helpers.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"

namespace content {
namespace {

using net::test_server::ControllableHttpResponse;

class NavPrefetchBrowserTest : public ContentBrowserTest,
                               public BackForwardCacheMetricsTestMatcher {
 public:
  NavPrefetchBrowserTest() {
    feature_list_.InitAndEnableFeature(
        features::kPreloadingRespectUserAgentOverride);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    attempt_ukm_entry_builder_ =
        std::make_unique<test::PreloadingAttemptUkmEntryBuilder>(
            content_preloading_predictor::kSpeculationRules);

    ssl_server_.RegisterRequestMonitor(
        base::BindRepeating(&NavPrefetchBrowserTest::MonitorResourceRequest,
                            base::Unretained(this)));
    ssl_server_.AddDefaultHandlers(GetTestDataFilePath());
    ssl_server_.SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(ssl_server_.Start());
  }

  void StartPrefetch(const GURL& url) {
    auto* prefetch_document_manager =
        PrefetchDocumentManager::GetOrCreateForCurrentDocument(
            shell()->web_contents()->GetPrimaryMainFrame());
    auto candidate = blink::mojom::SpeculationCandidate::New();
    candidate->url = url;
    candidate->action = blink::mojom::SpeculationAction::kPrefetch;
    candidate->eagerness = blink::mojom::SpeculationEagerness::kImmediate;
    candidate->referrer = Referrer::SanitizeForRequest(
        url, blink::mojom::Referrer(
                 shell()->web_contents()->GetURL(),
                 network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin));
    std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
    candidates.push_back(std::move(candidate));
    prefetch_document_manager->ProcessCandidates(candidates);
  }

  void ResetSSLConfig(
      net::test_server::EmbeddedTestServer::ServerCertificate cert,
      const net::SSLServerConfig& ssl_config) {
    ASSERT_TRUE(ssl_server_.ResetSSLConfig(cert, ssl_config));
  }

  RenderFrameHostImpl& GetPrimaryMainFrameHost() {
    return static_cast<WebContentsImpl*>(shell()->web_contents())
        ->GetPrimaryPage()
        .GetMainDocument();
  }
  void MonitorResourceRequest(const net::test_server::HttpRequest& request) {
    // This should be called on `EmbeddedTestServer::io_thread_`.
    EXPECT_FALSE(BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    base::AutoLock auto_lock(lock_);
    std::string path = request.GetURL().PathForRequest();
    request_count_by_path_[path]++;
    if (auto ua_it = request.headers.find(net::HttpRequestHeaders::kUserAgent);
        ua_it != request.headers.end()) {
      request_user_agent_by_path_[path] = ua_it->second;
    }
  }

  int GetRequestCount(const GURL& url) {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    base::AutoLock auto_lock(lock_);
    return request_count_by_path_[url.PathForRequest()];
  }

  const std::string& GetLastReceivedUserAgent(const GURL& url) {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    base::AutoLock auto_lock(lock_);
    return request_user_agent_by_path_[url.PathForRequest()];
  }

  // BackForwardCacheMetricsTestMatcher implementation.
  const base::HistogramTester& histogram_tester() override {
    return histogram_tester_;
  }

  // BackForwardCacheMetricsTestMatcher implementation.
  const ukm::TestAutoSetUkmRecorder& ukm_recorder() override {
    return *ukm_recorder_;
  }

  const test::PreloadingAttemptUkmEntryBuilder&
  attempt_entry_builder() {
    return *attempt_ukm_entry_builder_;
  }

 protected:
  GURL GetUrl(const std::string& host, const std::string& path) const {
    return ssl_server_.GetURL(host, path);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::ScopedMockElapsedTimersForTest test_timer_;

  net::test_server::EmbeddedTestServer ssl_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};

  std::map<std::string, int> request_count_by_path_ GUARDED_BY(lock_);
  std::map<std::string, std::string> request_user_agent_by_path_
      GUARDED_BY(lock_);

  base::HistogramTester histogram_tester_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
  std::unique_ptr<test::PreloadingAttemptUkmEntryBuilder>
      attempt_ukm_entry_builder_;
  // Disable sampling for UKM preloading logs.
  test::PreloadingConfigOverride preloading_config_override_;

  base::test::ScopedFeatureList scoped_feature_list_;

  base::Lock lock_;
};

IN_PROC_BROWSER_TEST_F(NavPrefetchBrowserTest,
                       DoesNotHangIfCancelledWhileWaitingForHead) {
  ControllableHttpResponse response1(embedded_test_server(), "/next");
  ControllableHttpResponse response2(embedded_test_server(), "/next");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL referrer_url = embedded_test_server()->GetURL("/empty.html");
  GURL next_url = embedded_test_server()->GetURL("/next");
  ASSERT_TRUE(NavigateToURL(shell(), referrer_url));

  // Prefetch the next page and wait for that request to arrive.
  StartPrefetch(next_url);
  response1.WaitForRequest();

  // Start a navigation which may block on head (since we haven't sent it).
  RenderFrameHost* rfh = shell()->web_contents()->GetPrimaryMainFrame();
  TestFrameNavigationObserver nav_observer(rfh);
  ASSERT_TRUE(BeginNavigateToURLFromRenderer(rfh, next_url));
  ASSERT_TRUE(nav_observer.navigation_started());

  // Cancel the prefetch.
  auto* prefetch_document_manager =
      PrefetchDocumentManager::GetOrCreateForCurrentDocument(rfh);
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  prefetch_document_manager->ProcessCandidates(candidates);

  // Wait for a new request, and respond to it.
  response2.WaitForRequest();
  response2.Send(net::HTTP_OK);
  response2.Done();

  // The navigation should now succeed.
  nav_observer.Wait();
  EXPECT_EQ(nav_observer.last_committed_url(), next_url);
  EXPECT_TRUE(nav_observer.last_navigation_succeeded());
}

// TODO(crbug.com/345352974): Make it a web platform test instead.
IN_PROC_BROWSER_TEST_F(NavPrefetchBrowserTest, ServedToRedirectionChain) {
  GURL initiator_url = GetUrl("a.test", "/empty.html");
  GURL des_url = GetUrl("a.test", "/title2.html");
  GURL next_nav_url = GetUrl("b.test", "/server-redirect?" + des_url.spec());
  ASSERT_TRUE(NavigateToURL(shell(), initiator_url));

  test::TestPrefetchWatcher test_prefetch_watcher;
  StartPrefetch(des_url);
  test_prefetch_watcher.WaitUntilPrefetchResponseCompleted(
      GetPrimaryMainFrameHost().GetDocumentToken(), des_url);
  ASSERT_EQ(GetRequestCount(des_url), 1);

  TestNavigationObserver nav_observer(shell()->web_contents());
  nav_observer.set_wait_event(TestNavigationObserver::WaitEvent::kLoadStopped);
  std::ignore = ExecJs(shell()->web_contents()->GetPrimaryMainFrame(),
                       JsReplace("location = $1", next_nav_url));
  nav_observer.Wait();

  EXPECT_TRUE(test_prefetch_watcher.PrefetchUsedInLastNavigation());
  EXPECT_EQ(GetRequestCount(des_url), 1);
  ukm::SourceId ukm_source_id =
      shell()->web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();

  // Navigate primary page to flush the metrics.
  ASSERT_TRUE(NavigateToURL(shell(), initiator_url));

  test::ExpectPreloadingAttemptUkm(
      ukm_recorder(),
      {attempt_entry_builder().BuildEntry(
          ukm_source_id, PreloadingType::kPrefetch,
          PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
          PreloadingTriggeringOutcome::kSuccess,
          PreloadingFailureReason::kUnspecified,
          /*accurate=*/true,
          base::ScopedMockElapsedTimersForTest::kMockElapsedTime,
          blink::mojom::SpeculationEagerness::kImmediate)});
}

// TODO(crbug.com/345352974): Make it a web platform test instead.
IN_PROC_BROWSER_TEST_F(NavPrefetchBrowserTest, SetCookieViaHTTPResponse) {
  GURL initiator_url = GetUrl("a.test", "/empty.html");
  const std::string server_cookie = "host_cookie=1";
  GURL des_url = GetUrl("b.test", "/set-cookie?" + server_cookie);
  ASSERT_TRUE(NavigateToURL(shell(), initiator_url));

  // 1. Prefetch a resource which sets cookie.
  test::TestPrefetchWatcher test_prefetch_watcher;
  StartPrefetch(des_url);
  test_prefetch_watcher.WaitUntilPrefetchResponseCompleted(
      GetPrimaryMainFrameHost().GetDocumentToken(), des_url);
  ASSERT_EQ(GetRequestCount(des_url), 1);

  // 2. Activate the prefetched result.
  TestNavigationObserver nav_observer(shell()->web_contents());
  nav_observer.set_wait_event(TestNavigationObserver::WaitEvent::kLoadStopped);
  std::ignore = ExecJs(shell()->web_contents()->GetPrimaryMainFrame(),
                       JsReplace("location = $1", des_url));
  nav_observer.Wait();

  // 3. The cookie was written into the real cookie storage.
  EXPECT_TRUE(test_prefetch_watcher.PrefetchUsedInLastNavigation());
  EXPECT_EQ(GetRequestCount(des_url), 1);
  EXPECT_EQ(EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                   "document.cookie"), server_cookie);

  // 4. Navigate to another same-site page to confirm the cookie is persistent.
  GURL after_prefetch_url = GetUrl("b.test", "/title2.html");
  std::ignore = ExecJs(shell()->web_contents()->GetPrimaryMainFrame(),
                       JsReplace("location = $1", after_prefetch_url));
  EXPECT_EQ(EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                   "document.cookie"), server_cookie);
}

// TODO(crbug.com/345352974): Make it a web platform test instead.
IN_PROC_BROWSER_TEST_F(NavPrefetchBrowserTest,
                       NeverSetCookieForDiscardedPrefetch) {
  GURL initiator_url = GetUrl("a.test", "/empty.html");
  const std::string server_cookie = "host_cookie=1";
  GURL des_url = GetUrl("b.test", "/set-cookie?" + server_cookie);
  ASSERT_TRUE(NavigateToURL(shell(), initiator_url));

  // 1. Prefetch a resource which sets cookie.
  test::TestPrefetchWatcher test_prefetch_watcher;
  StartPrefetch(des_url);
  test_prefetch_watcher.WaitUntilPrefetchResponseCompleted(
      GetPrimaryMainFrameHost().GetDocumentToken(), des_url);
  ASSERT_EQ(GetRequestCount(des_url), 1);

  // 2. Navigate to another URL to invalidate the prefetch result.
  TestNavigationObserver nav_observer(shell()->web_contents());
  nav_observer.set_wait_event(TestNavigationObserver::WaitEvent::kLoadStopped);
  GURL after_prefetch_url = GetUrl("b.test", "/title2.html");

  std::ignore = ExecJs(shell()->web_contents()->GetPrimaryMainFrame(),
                       JsReplace("location = $1", after_prefetch_url));
  nav_observer.Wait();

  // 3. Check the cookie set by discarded prefetch response cannot affect the
  // real jar.
  EXPECT_EQ(EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                       "document.cookie").ExtractString(), "");
}

IN_PROC_BROWSER_TEST_F(
    NavPrefetchBrowserTest,
    CrossSitePrefetchNotServedWhenCookieChange_BeforeFirstServe) {
  // Perform a cross-site prefetch which sets cookie.
  const std::string server_cookie = "server_cookie=1";
  GURL initiator_url = GetUrl("a.test", "/empty.html");
  GURL des_url = GetUrl("b.test", "/set-cookie?" + server_cookie);
  ASSERT_TRUE(NavigateToURL(shell(), initiator_url));
  test::TestPrefetchWatcher test_prefetch_watcher;
  StartPrefetch(des_url);
  test_prefetch_watcher.WaitUntilPrefetchResponseCompleted(
      GetPrimaryMainFrameHost().GetDocumentToken(), des_url);
  ASSERT_EQ(GetRequestCount(des_url), 1);

  // Change a cookie for the prefetched site.
  ASSERT_TRUE(SetCookie(shell()->web_contents()->GetBrowserContext(), des_url,
                        "test=1"));

  // Try to navigate a prefetched site. Prefetch is not used because default
  // network context's cookie has been changed.
  TestNavigationObserver nav_observer(shell()->web_contents());
  nav_observer.set_wait_event(TestNavigationObserver::WaitEvent::kLoadStopped);
  std::ignore = ExecJs(shell()->web_contents()->GetPrimaryMainFrame(),
                       JsReplace("location = $1", des_url));
  nav_observer.Wait();
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), des_url);
  EXPECT_FALSE(test_prefetch_watcher.PrefetchUsedInLastNavigation());
  EXPECT_EQ(GetRequestCount(des_url), 2);
  EXPECT_THAT(GetCanonicalCookies(shell()->web_contents()->GetBrowserContext(),
                                  des_url),
              testing::UnorderedElementsAre(
                  net::MatchesCookieNameValue("server_cookie", "1"),
                  net::MatchesCookieNameValue("test", "1")));
}

IN_PROC_BROWSER_TEST_F(
    NavPrefetchBrowserTest,
    CrossSitePrefetchNotServedWhenCookieChange_AfterFirstServe) {
  if (!BackForwardCache::IsBackForwardCacheFeatureEnabled()) {
    GTEST_SKIP()
        << "This test assumes that BFCache is used when back navigation";
  }

  // Perform a cross-site prefetch which sets cookie.
  const std::string server_cookie = "server_cookie=1";
  GURL initiator_url = GetUrl("a.test", "/empty.html");
  GURL des_url = GetUrl("b.test", "/set-cookie?" + server_cookie);
  ASSERT_TRUE(NavigateToURL(shell(), initiator_url));
  test::TestPrefetchWatcher test_prefetch_watcher;
  StartPrefetch(des_url);
  test_prefetch_watcher.WaitUntilPrefetchResponseCompleted(
      GetPrimaryMainFrameHost().GetDocumentToken(), des_url);
  ASSERT_EQ(GetRequestCount(des_url), 1);

  // Activate a prefetch. Cookie is successfully copied to default network
  // context.
  {
    TestNavigationObserver nav_observer(shell()->web_contents());
    nav_observer.set_wait_event(
        TestNavigationObserver::WaitEvent::kLoadStopped);
    std::ignore = ExecJs(shell()->web_contents()->GetPrimaryMainFrame(),
                         JsReplace("location = $1", des_url));
    nav_observer.Wait();
    EXPECT_TRUE(test_prefetch_watcher.PrefetchUsedInLastNavigation());
    EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), des_url);
    EXPECT_THAT(GetCanonicalCookies(
                    shell()->web_contents()->GetBrowserContext(), des_url),
                testing::UnorderedElementsAre(
                    net::MatchesCookieNameValue("server_cookie", "1")));
  }

  // Back to the initial site. Since the document that initiated prefetch has
  // been restored from BFCache, `PrefetchDocumentManager` and the prefetch
  // should still be alive in this timing.
  {
    TestNavigationObserver observer1(shell()->web_contents());
    shell()->GoBackOrForward(-1);
    observer1.Wait();
    ASSERT_EQ(shell()->web_contents()->GetLastCommittedURL(), initiator_url);
    ExpectRestored(FROM_HERE);
  }

  // Activate a prefetch again. Prefetch is served since it can be used multiple
  // times.
  {
    TestNavigationObserver nav_observer(shell()->web_contents());
    nav_observer.set_wait_event(
        TestNavigationObserver::WaitEvent::kLoadStopped);
    std::ignore = ExecJs(shell()->web_contents()->GetPrimaryMainFrame(),
                         JsReplace("location = $1", des_url));
    nav_observer.Wait();
    EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), des_url);
    EXPECT_TRUE(test_prefetch_watcher.PrefetchUsedInLastNavigation());
    EXPECT_EQ(GetRequestCount(des_url), 1);
  }

  // Set a cookie to a prefetched site.
  ASSERT_TRUE(SetCookie(shell()->web_contents()->GetBrowserContext(), des_url,
                        "test=1"));

  // Back to the initial site and try to activate prefetch again. Prefetch is
  // not served because the cookie has been changed.
  {
    TestNavigationObserver nav_observer(shell()->web_contents());
    shell()->GoBackOrForward(-1);
    nav_observer.Wait();
    ASSERT_EQ(shell()->web_contents()->GetLastCommittedURL(), initiator_url);
    ExpectRestored(FROM_HERE);
  }
  {
    TestNavigationObserver nav_observer(shell()->web_contents());
    nav_observer.set_wait_event(
        TestNavigationObserver::WaitEvent::kLoadStopped);
    std::ignore = ExecJs(shell()->web_contents()->GetPrimaryMainFrame(),
                         JsReplace("location = $1", des_url));
    nav_observer.Wait();
    EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), des_url);
    EXPECT_FALSE(test_prefetch_watcher.PrefetchUsedInLastNavigation());
    EXPECT_EQ(GetRequestCount(des_url), 2);
    EXPECT_THAT(GetCanonicalCookies(
                    shell()->web_contents()->GetBrowserContext(), des_url),
                testing::UnorderedElementsAre(
                    net::MatchesCookieNameValue("server_cookie", "1"),
                    net::MatchesCookieNameValue("test", "1")));
  }
}

// In the tests below about auth/cert errors, we just check that the prefetches
// should fail. We expect no dialogs etc. are presented to users on such
// failures, because we don't pass URLLoaderNetworkServiceObserver for prefetch
// requests in the first place. If we should pass
// URLLoaderNetworkServiceObserver in the future (which is unlikely though), we
// would need more test coverage here (for prefetches to
// ServiceWorker-controlled URLs, check dialogs like in
// content/browser/service_worker/service_worker_auth_browsertest.cc, etc.).

// Tests that prefetch fails when auth is requested.
IN_PROC_BROWSER_TEST_F(NavPrefetchBrowserTest, AuthRequested) {
  GURL initiator_url = GetUrl("a.test", "/empty.html");
  GURL destination_url = GetUrl("a.test", "/auth-basic");
  ASSERT_TRUE(NavigateToURL(shell(), initiator_url));

  test::TestPrefetchWatcher test_prefetch_watcher;
  StartPrefetch(destination_url);
  test_prefetch_watcher.WaitUntilPrefetchResponseCompleted(
      GetPrimaryMainFrameHost().GetDocumentToken(), destination_url);
  ASSERT_EQ(GetRequestCount(destination_url), 1);

  TestNavigationObserver nav_observer(shell()->web_contents());
  nav_observer.set_wait_event(TestNavigationObserver::WaitEvent::kLoadStopped);
  std::ignore = ExecJs(shell()->web_contents()->GetPrimaryMainFrame(),
                       JsReplace("location = $1", destination_url));
  nav_observer.Wait();

  EXPECT_FALSE(test_prefetch_watcher.PrefetchUsedInLastNavigation());
  EXPECT_EQ(GetRequestCount(destination_url), 2);
  ukm::SourceId ukm_source_id =
      shell()->web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();

  // Navigate primary page to flush the metrics.
  ASSERT_TRUE(NavigateToURL(shell(), initiator_url));

  test::ExpectPreloadingAttemptUkm(
      ukm_recorder(),
      {attempt_entry_builder().BuildEntry(
          ukm_source_id, PreloadingType::kPrefetch,
          PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
          PreloadingTriggeringOutcome::kFailure,
          ToPreloadingFailureReason(PrefetchStatus::kPrefetchFailedNon2XX),
          /*accurate=*/true,
          /*ready_time=*/std::nullopt,
          blink::mojom::SpeculationEagerness::kImmediate)});
}

// Tests that prefetch fails when client cert is requested.
IN_PROC_BROWSER_TEST_F(NavPrefetchBrowserTest, ClientCertRequested) {
  GURL initiator_url = GetUrl("a.test", "/empty.html");
  GURL destination_url = GetUrl("a.test", "/title2.html");
  ASSERT_TRUE(NavigateToURL(shell(), initiator_url));

  // Reset the server's config to cause a client cert request.
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  ResetSSLConfig(net::test_server::EmbeddedTestServer::CERT_TEST_NAMES,
                 ssl_config);

  test::TestPrefetchWatcher test_prefetch_watcher;
  StartPrefetch(destination_url);
  test_prefetch_watcher.WaitUntilPrefetchResponseCompleted(
      GetPrimaryMainFrameHost().GetDocumentToken(), destination_url);

  // Reset the server's config to normal.
  ResetSSLConfig(net::test_server::EmbeddedTestServer::CERT_TEST_NAMES,
                 net::SSLServerConfig());

  TestNavigationObserver nav_observer(shell()->web_contents());
  nav_observer.set_wait_event(TestNavigationObserver::WaitEvent::kLoadStopped);
  std::ignore = ExecJs(shell()->web_contents()->GetPrimaryMainFrame(),
                       JsReplace("location = $1", destination_url));
  nav_observer.Wait();

  EXPECT_FALSE(test_prefetch_watcher.PrefetchUsedInLastNavigation());
  // Prefetch request should not be counted.
  EXPECT_EQ(GetRequestCount(destination_url), 1);
  ukm::SourceId ukm_source_id =
      shell()->web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();

  // Navigate primary page to flush the metrics.
  ASSERT_TRUE(NavigateToURL(shell(), initiator_url));

  test::ExpectPreloadingAttemptUkm(
      ukm_recorder(),
      {attempt_entry_builder().BuildEntry(
          ukm_source_id, PreloadingType::kPrefetch,
          PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
          PreloadingTriggeringOutcome::kFailure,
          ToPreloadingFailureReason(PrefetchStatus::kPrefetchFailedNetError),
          /*accurate=*/true,
          /*ready_time=*/std::nullopt,
          blink::mojom::SpeculationEagerness::kImmediate)});
}

// WebContentsDelegate to set UserAgentOverrideOption for prefetch and
// prerender.
class ScopedUserAgentOverrideTestDelegate : public WebContentsDelegate {
 public:
  explicit ScopedUserAgentOverrideTestDelegate(WebContents& web_contents)
      : web_contents_(web_contents.GetWeakPtr()) {
    web_contents_->SetDelegate(this);
  }
  ~ScopedUserAgentOverrideTestDelegate() override {
    if (web_contents_) {
      web_contents_->SetDelegate(nullptr);
    }
  }

  NavigationController::UserAgentOverrideOption
  ShouldOverrideUserAgentForPreloading(const GURL& url) override {
    return is_override_on
               ? NavigationController::UserAgentOverrideOption::UA_OVERRIDE_TRUE
               : NavigationController::UserAgentOverrideOption::
                     UA_OVERRIDE_FALSE;
  }

  void EnableOverride() { is_override_on = true; }
  void DisableOverride() { is_override_on = false; }

 private:
  bool is_override_on = false;
  base::WeakPtr<WebContents> web_contents_;
};

// Tests that prefetch fails when cert is expired.
IN_PROC_BROWSER_TEST_F(NavPrefetchBrowserTest, CertExpired) {
  GURL initiator_url = GetUrl("a.test", "/empty.html");
  GURL destination_url = GetUrl("a.test", "/title2.html");
  ASSERT_TRUE(NavigateToURL(shell(), initiator_url));

  // Reset the server's config to cause a cert error.
  ResetSSLConfig(net::test_server::EmbeddedTestServer::CERT_EXPIRED,
                 net::SSLServerConfig());

  test::TestPrefetchWatcher test_prefetch_watcher;
  StartPrefetch(destination_url);
  test_prefetch_watcher.WaitUntilPrefetchResponseCompleted(
      GetPrimaryMainFrameHost().GetDocumentToken(), destination_url);

  // Reset the server's config to normal.
  ResetSSLConfig(net::test_server::EmbeddedTestServer::CERT_TEST_NAMES,
                 net::SSLServerConfig());

  TestNavigationObserver nav_observer(shell()->web_contents());
  nav_observer.set_wait_event(TestNavigationObserver::WaitEvent::kLoadStopped);
  std::ignore = ExecJs(shell()->web_contents()->GetPrimaryMainFrame(),
                       JsReplace("location = $1", destination_url));
  nav_observer.Wait();

  EXPECT_FALSE(test_prefetch_watcher.PrefetchUsedInLastNavigation());
  // Prefetch request should not be counted.
  EXPECT_EQ(GetRequestCount(destination_url), 1);
  ukm::SourceId ukm_source_id =
      shell()->web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();

  // Navigate primary page to flush the metrics.
  ASSERT_TRUE(NavigateToURL(shell(), initiator_url));

  test::ExpectPreloadingAttemptUkm(
      ukm_recorder(),
      {attempt_entry_builder().BuildEntry(
          ukm_source_id, PreloadingType::kPrefetch,
          PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
          PreloadingTriggeringOutcome::kFailure,
          ToPreloadingFailureReason(PrefetchStatus::kPrefetchFailedNetError),
          /*accurate=*/true,
          /*ready_time=*/std::nullopt,
          blink::mojom::SpeculationEagerness::kImmediate)});
}

// Tests that WebContents-level User-Agent header overrides are used for
// prefetch requests.
IN_PROC_BROWSER_TEST_F(NavPrefetchBrowserTest,
                       RespectWebContentsUserAgentOverride) {
  GURL initiator_url = GetUrl("a.test", "/empty.html");
  GURL destination_url_1 = GetUrl("a.test", "/title1.html");
  GURL destination_url_2 = GetUrl("a.test", "/title2.html");

  ASSERT_TRUE(NavigateToURL(shell(), initiator_url));

  const std::string default_ua = GetLastReceivedUserAgent(initiator_url);

  ScopedUserAgentOverrideTestDelegate ua_override_delegate(
      *shell()->web_contents());
  test::TestPrefetchWatcher test_prefetch_watcher;

  // Scenario 1: A prefetch result has an overridden UA if it is registered and
  // enabled.
  {
    RenderFrameHostImpl* rfh = GetPrimaryMainFrameHost().GetMainFrame();

    // Set UA override and enable it for prefetch and prerender.
    shell()->web_contents()->SetUserAgentOverride(
        blink::UserAgentOverride::UserAgentOnly("fake"),
        /*override_in_new_tabs=*/true);
    ua_override_delegate.EnableOverride();

    // Prefetch a page and navigate to it.
    StartPrefetch(destination_url_1);
    test_prefetch_watcher.WaitUntilPrefetchResponseCompleted(
        GetPrimaryMainFrameHost().GetDocumentToken(), destination_url_1);

    TestFrameNavigationObserver nav_observer(rfh);
    ASSERT_TRUE(BeginNavigateToURLFromRenderer(rfh, destination_url_1));
    ASSERT_TRUE(nav_observer.navigation_started());
    nav_observer.Wait();
    ASSERT_TRUE(test_prefetch_watcher.PrefetchUsedInLastNavigation());

    // The overridden User-Agent is used.
    ASSERT_EQ(GetRequestCount(destination_url_1), 1);
    ASSERT_EQ(GetLastReceivedUserAgent(destination_url_1), "fake");
  }

  // Scenario 2: A prefetch result has a default UA if an UA override is
  // registered but disabled.
  {
    RenderFrameHostImpl* rfh = GetPrimaryMainFrameHost().GetMainFrame();

    // Disable UA override and make sure the disabled UA override is still
    // registered.
    ua_override_delegate.DisableOverride();
    ASSERT_FALSE(shell()
                     ->web_contents()
                     ->GetUserAgentOverride()
                     .ua_string_override.empty());

    // Prefetch a page and navigate to it.
    StartPrefetch(destination_url_2);
    test_prefetch_watcher.WaitUntilPrefetchResponseCompleted(
        GetPrimaryMainFrameHost().GetDocumentToken(), destination_url_2);
    TestFrameNavigationObserver nav_observer(rfh);
    ASSERT_TRUE(BeginNavigateToURLFromRenderer(rfh, destination_url_2));
    ASSERT_TRUE(nav_observer.navigation_started());
    nav_observer.Wait();

    // The overridden User-Agent is NOT used.
    ASSERT_TRUE(test_prefetch_watcher.PrefetchUsedInLastNavigation());
    EXPECT_EQ(GetRequestCount(destination_url_2), 1);
    ASSERT_EQ(GetLastReceivedUserAgent(destination_url_2), default_ua);
  }
}

}  // namespace
}  // namespace content
