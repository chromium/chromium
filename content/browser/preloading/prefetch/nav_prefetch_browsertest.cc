// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/prefetch_test_util.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"

namespace content {
namespace {

using net::test_server::ControllableHttpResponse;

class NavPrefetchBrowserTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void StartPrefetch(const GURL& url) {
    auto* prefetch_document_manager =
        PrefetchDocumentManager::GetOrCreateForCurrentDocument(
            shell()->web_contents()->GetPrimaryMainFrame());
    auto candidate = blink::mojom::SpeculationCandidate::New();
    candidate->url = url;
    candidate->action = blink::mojom::SpeculationAction::kPrefetch;
    candidate->eagerness = blink::mojom::SpeculationEagerness::kEager;
    candidate->referrer = Referrer::SanitizeForRequest(
        url, blink::mojom::Referrer(
                 shell()->web_contents()->GetURL(),
                 network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin));
    std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
    candidates.push_back(std::move(candidate));
    prefetch_document_manager->ProcessCandidates(candidates,
                                                 /*devtools_observer=*/nullptr);
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
    request_count_by_path_[request.GetURL().PathForRequest()]++;
  }

  int GetRequestCount(const GURL& url) {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    base::AutoLock auto_lock(lock_);
    return request_count_by_path_[url.PathForRequest()];
  }

 private:
  std::map<std::string, int> request_count_by_path_ GUARDED_BY(lock_);
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
  prefetch_document_manager->ProcessCandidates(candidates,
                                               /*devtools_observer=*/nullptr);

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
  net::test_server::EmbeddedTestServer ssl_server{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};
  ssl_server.RegisterRequestMonitor(base::BindRepeating(
      &NavPrefetchBrowserTest::MonitorResourceRequest, base::Unretained(this)));
  ssl_server.AddDefaultHandlers(GetTestDataFilePath());
  ssl_server.SetSSLConfig(
      net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
  ASSERT_TRUE(ssl_server.Start());

  GURL initiator_url = ssl_server.GetURL("a.test", "/empty.html");
  GURL des_url = ssl_server.GetURL("a.test", "/title2.html");
  GURL next_nav_url =
      ssl_server.GetURL("b.test", "/server-redirect?" + des_url.spec());
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
}

// TODO(crbug.com/345352974): Make it a web platform test instead.
IN_PROC_BROWSER_TEST_F(NavPrefetchBrowserTest, SetCookieViaHTTPResponse) {
  net::test_server::EmbeddedTestServer ssl_server{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};
  ssl_server.RegisterRequestMonitor(base::BindRepeating(
      &NavPrefetchBrowserTest::MonitorResourceRequest, base::Unretained(this)));
  ssl_server.AddDefaultHandlers(GetTestDataFilePath());
  ssl_server.SetSSLConfig(
      net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
  ASSERT_TRUE(ssl_server.Start());

  GURL initiator_url = ssl_server.GetURL("a.test", "/empty.html");
  const std::string server_cookie = "host_cookie=1";
  GURL des_url = ssl_server.GetURL("b.test", "/set-cookie?" + server_cookie);
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
  GURL after_prefetch_url = ssl_server.GetURL("b.test", "/title2.html");
  std::ignore = ExecJs(shell()->web_contents()->GetPrimaryMainFrame(),
                       JsReplace("location = $1", after_prefetch_url));
  EXPECT_EQ(EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                   "document.cookie"), server_cookie);
}

// TODO(crbug.com/345352974): Make it a web platform test instead.
IN_PROC_BROWSER_TEST_F(NavPrefetchBrowserTest,
                       NeverSetCookieForDiscardedPrefetch) {
  net::test_server::EmbeddedTestServer ssl_server{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};
  ssl_server.RegisterRequestMonitor(base::BindRepeating(
      &NavPrefetchBrowserTest::MonitorResourceRequest, base::Unretained(this)));
  ssl_server.AddDefaultHandlers(GetTestDataFilePath());
  ssl_server.SetSSLConfig(
      net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
  ASSERT_TRUE(ssl_server.Start());

  GURL initiator_url = ssl_server.GetURL("a.test", "/empty.html");
  const std::string server_cookie = "host_cookie=1";
  GURL des_url = ssl_server.GetURL("b.test", "/set-cookie?" + server_cookie);
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
  GURL after_prefetch_url = ssl_server.GetURL("b.test", "/title2.html");

  std::ignore = ExecJs(shell()->web_contents()->GetPrimaryMainFrame(),
                       JsReplace("location = $1", after_prefetch_url));
  nav_observer.Wait();

  // 3. Check the cookie set by discarded prefetch response cannot affect the
  // real jar.
  EXPECT_EQ(EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                       "document.cookie").ExtractString(), "");
}

}  // namespace
}  // namespace content
