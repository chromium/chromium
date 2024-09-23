// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/back_forward_cache_browsertest.h"

#include <memory>

#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromecast_buildflags.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"

// This file contains back-/forward-cache tests for the
// `Cache-control: no-store` header. It was forked from
// https://source.chromium.org/chromium/chromium/src/+/main:content/browser/back_forward_cache_browsertest.cc;drc=b339487e39ad6ae93af30fa8fcb37dc61bd138ec
//
// When adding tests please also add WPTs. See
// third_party/blink/web_tests/external/wpt/html/browsers/browsing-the-web/back-forward-cache/README.md

namespace content {

using NotRestoredReason = BackForwardCacheMetrics::NotRestoredReason;
using NotRestoredReasons =
    BackForwardCacheCanStoreDocumentResult::NotRestoredReasons;
using BlocklistedFeature = blink::scheduler::WebSchedulerTrackedFeature;

namespace {

const char kResponseWithNoCache[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Cache-Control: no-store\r\n"
    "\r\n"
    "The server speaks HTTP!";

}  // namespace

// TODO(crbug.com/40285326): This fails with the field trial testing config.
class BackForwardCacheBrowserTestNoTestingConfig
    : public BackForwardCacheBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("disable-field-trial-config");
  }
};

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestNoTestingConfig,
                       MainFrameWithNoStoreNotCached) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/main_document");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/main_document"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1. Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(web_contents());
  shell()->LoadURL(url_a);
  response.WaitForRequest();
  response.Send(kResponseWithNoCache);
  response.Done();
  observer.Wait();

  // 2. Navigate away and expect frame to be deleted.
  RenderFrameDeletedObserver delete_observer_rfh_a(current_frame_host());
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  delete_observer_rfh_a.WaitUntilDeleted();
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestNoTestingConfig,
                       SubframeWithNoStoreCached) {
  // iframe will try to load title1.html.
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/title1.html");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(web_contents());
  shell()->LoadURL(url_a);
  response.WaitForRequest();
  response.Send(kResponseWithNoCache);
  response.Done();
  observer.Wait();
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(current_frame_host());

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) Navigate back and expect everything to be restored.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_EQ(rfh_a, current_frame_host());
}

// When CCNS is present and WebSocket is used, both features should be recorded
// and the test should not hit CHECK.
// TODO(crbug.com/40241677): WebSocket server is flaky Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_CCNSAndWebSocketBothRecorded DISABLED_CCNSAndWebSocketBothRecorded
#else
#define MAYBE_CCNSAndWebSocketBothRecorded CCNSAndWebSocketBothRecorded
#endif
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestNoTestingConfig,
                       MAYBE_CCNSAndWebSocketBothRecorded) {
  net::SpawnedTestServer ws_server(net::SpawnedTestServer::TYPE_WS,
                                   net::GetWebSocketTestDataDirectory());
  ASSERT_TRUE(ws_server.Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a_no_store(embedded_test_server()->GetURL(
      "a.com", "/set-header?Cache-Control: no-store"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1. Load the document and specify no-store for the main resource.
  ASSERT_TRUE(NavigateToURL(shell(), url_a_no_store));
  RenderFrameHostWrapper rfh_a(current_frame_host());
  // Open a WebSocket.
  const char script[] = R"(
      new Promise(resolve => {
        const socket = new WebSocket($1);
        socket.addEventListener('open', () => resolve());
      });)";
  ASSERT_TRUE(
      ExecJs(rfh_a.get(),
             JsReplace(script, ws_server.GetURL("echo-with-no-extension"))));

  // 2. Navigate away and expect frame to be deleted.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3. Go back and make sure both reasons are recorded.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kBlocklistedFeatures},
                    {BlocklistedFeature::kWebSocket,
                     BlocklistedFeature::kMainResourceHasCacheControlNoStore,
                     BlocklistedFeature::kWebSocketSticky},
                    {}, {}, {}, FROM_HERE);
}

namespace {

class BackForwardCacheBrowserTestAllowCacheControlNoStore
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableFeatureAndSetParams(features::kBackForwardCache, "", "");
    EnableFeatureAndSetParams(
        features::kCacheControlNoStoreEnterBackForwardCache, "level",
        "store-and-evict");
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

}  // namespace

// Test that a page with cache-control:no-store enters bfcache with the flag on,
// but does not get restored and gets evicted.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestAllowCacheControlNoStore,
                       PagesWithCacheControlNoStoreEnterBfcacheAndEvicted) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/title1.html");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(web_contents());
  shell()->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  response.WaitForRequest();
  response.Send(kResponseWithNoCache);
  response.Done();
  observer.Wait();
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);

  // 2) Navigate away. |rfh_a| should enter the bfcache.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Go back. |rfh_a| should be evicted upon restoration.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  ExpectNotRestored({NotRestoredReason::kCacheControlNoStore}, {}, {}, {}, {},
                    FROM_HERE);
  // Make sure that the tree result also has the same reason.
  EXPECT_THAT(GetTreeResult()->GetDocumentResult(),
              MatchesDocumentResult(
                  NotRestoredReasons({NotRestoredReason::kCacheControlNoStore}),
                  BlockListedFeatures()));
}

// Test that a page with cache-control:no-store enters bfcache with the flag on,
// and if a cookie is modified while it is in bfcache via JavaScript, gets
// evicted with cookie modified marked.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestAllowCacheControlNoStore,
    PagesWithCacheControlNoStoreCookieModifiedThroughJavaScript) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/title1.html");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a_2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title3.html"));

  Shell* tab_to_be_bfcached = shell();
  Shell* tab_to_modify_cookie = CreateBrowser();

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  response.WaitForRequest();
  response.Send(kResponseWithNoCache);
  response.Done();
  observer.Wait();
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);

  // 2) Set a normal cookie from JavaScript.
  EXPECT_TRUE(ExecJs(tab_to_be_bfcached, "document.cookie='foo=bar'"));
  EXPECT_EQ("foo=bar", EvalJs(tab_to_be_bfcached, "document.cookie"));

  // 3) Navigate away. |rfh_a| should enter bfcache.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 4) Navigate to a.com in |tab_to_modify_cookie| and modify cookie from
  // JavaScript.
  EXPECT_TRUE(NavigateToURL(tab_to_modify_cookie, url_a_2));
  EXPECT_EQ("foo=bar", EvalJs(tab_to_modify_cookie, "document.cookie"));
  EXPECT_TRUE(ExecJs(tab_to_modify_cookie, "document.cookie='foo=baz'"));
  EXPECT_EQ("foo=baz", EvalJs(tab_to_modify_cookie, "document.cookie"));

  // 5) Go back. |rfh_a| should be evicted upon restoration.
  ASSERT_TRUE(HistoryGoBack(tab_to_be_bfcached->web_contents()));

  EXPECT_EQ("foo=baz", EvalJs(tab_to_be_bfcached, "document.cookie"));
  ExpectNotRestored({NotRestoredReason::kCacheControlNoStoreCookieModified}, {},
                    {}, {}, {}, FROM_HERE);
  // Make sure that the tree result also has the same reason.
  EXPECT_THAT(GetTreeResult()->GetDocumentResult(),
              MatchesDocumentResult(
                  NotRestoredReasons(
                      {NotRestoredReason::kCacheControlNoStoreCookieModified}),
                  BlockListedFeatures()));
}

// Test that a page with cache-control:no-store enters bfcache with the flag on,
// and if a cookie is modified, it gets evicted with cookie changed, but if
// navigated away again and navigated back, it gets evicted without cookie
// change marked.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestAllowCacheControlNoStore,
                       PagesWithCacheControlNoStoreCookieModifiedBackTwice) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/set-header?Cache-Control: no-store"));
  GURL url_a_2(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  Shell* tab_to_be_bfcached = shell();
  Shell* tab_to_modify_cookie = CreateBrowser();

  // 1) Load the document and specify no-store for the main resource.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);

  // 2) Set a normal cookie from JavaScript.
  EXPECT_TRUE(ExecJs(tab_to_be_bfcached, "document.cookie='foo=bar'"));
  EXPECT_EQ("foo=bar", EvalJs(tab_to_be_bfcached, "document.cookie"));

  // 3) Navigate away. |rfh_a| should enter bfcache.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 4) Navigate to a.com in |tab_to_modify_cookie| and modify cookie from
  // JavaScript.
  EXPECT_TRUE(NavigateToURL(tab_to_modify_cookie, url_a_2));
  EXPECT_EQ("foo=bar", EvalJs(tab_to_modify_cookie, "document.cookie"));
  EXPECT_TRUE(ExecJs(tab_to_modify_cookie, "document.cookie='foo=baz'"));
  EXPECT_EQ("foo=baz", EvalJs(tab_to_modify_cookie, "document.cookie"));

  // 5) Go back. |rfh_a| should be evicted upon restoration.
  ASSERT_TRUE(HistoryGoBack(tab_to_be_bfcached->web_contents()));

  EXPECT_EQ("foo=baz", EvalJs(tab_to_be_bfcached, "document.cookie"));
  ExpectNotRestored({NotRestoredReason::kCacheControlNoStoreCookieModified}, {},
                    {}, {}, {}, FROM_HERE);
  EXPECT_THAT(GetTreeResult()->GetDocumentResult(),
              MatchesDocumentResult(
                  NotRestoredReasons(
                      {NotRestoredReason::kCacheControlNoStoreCookieModified}),
                  BlockListedFeatures()));
  RenderFrameHostImplWrapper rfh_a_2(current_frame_host());
  rfh_a_2->GetBackForwardCacheMetrics()->SetObserverForTesting(this);

  // 6) Navigate away to b.com. |rfh_a_2| should enter bfcache again.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_b));
  EXPECT_TRUE(rfh_a_2->IsInBackForwardCache());

  // 7) Navigate back to a.com. This time the cookie change has to be reset and
  // gets evicted with a different reason.
  ASSERT_TRUE(HistoryGoBack(tab_to_be_bfcached->web_contents()));
  ExpectNotRestored({NotRestoredReason::kCacheControlNoStore}, {}, {}, {}, {},
                    FROM_HERE);
  EXPECT_THAT(GetTreeResult()->GetDocumentResult(),
              MatchesDocumentResult(
                  NotRestoredReasons({NotRestoredReason::kCacheControlNoStore}),
                  BlockListedFeatures()));
}

// Test that a page with cache-control:no-store enters bfcache with the flag on,
// and even if a cookie is modified on a different domain than the entry, the
// entry is not marked as cookie modified.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestAllowCacheControlNoStore,
    PagesWithCacheControlNoStoreCookieModifiedThroughJavaScriptOnDifferentDomain) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/title1.html");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a_2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title3.html"));

  Shell* tab_to_be_bfcached = shell();
  Shell* tab_to_modify_cookie = CreateBrowser();

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  response.WaitForRequest();
  response.Send(kResponseWithNoCache);
  response.Done();
  observer.Wait();
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);

  // 2) Navigate away. |rfh_a| should enter bfcache.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Navigate to b.com in |tab_to_modify_cookie| and modify cookie from
  // JavaScript.
  EXPECT_TRUE(NavigateToURL(tab_to_modify_cookie, url_b));
  EXPECT_TRUE(ExecJs(tab_to_modify_cookie, "document.cookie='foo=baz'"));
  EXPECT_EQ("foo=baz", EvalJs(tab_to_modify_cookie, "document.cookie"));

  // 4) Go back. |rfh_a| should be evicted upon restoration.
  ASSERT_TRUE(HistoryGoBack(tab_to_be_bfcached->web_contents()));

  ExpectNotRestored({NotRestoredReason::kCacheControlNoStore}, {}, {}, {}, {},
                    FROM_HERE);
  EXPECT_THAT(GetTreeResult()->GetDocumentResult(),
              MatchesDocumentResult(
                  NotRestoredReasons({NotRestoredReason::kCacheControlNoStore}),
                  BlockListedFeatures()));
}

// Test that a page with cache-control:no-store records other not restored
// reasons along with kCacheControlNoStore when eviction happens.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestAllowCacheControlNoStore,
    PagesWithCacheControlNoStoreRecordOtherReasonsWhenEvictionHappens) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/set-header?Cache-Control: no-store"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Load the document and specify no-store for the main resource.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);

  // 2) Navigate away. At this point the page should be in bfcache.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Execute JavaScript and evict the entry.
  EvictByJavaScript(rfh_a.get());

  // 4) Go back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  ExpectNotRestored({NotRestoredReason::kJavaScriptExecution,
                     NotRestoredReason::kCacheControlNoStore},
                    {}, {}, {}, {}, FROM_HERE);
  EXPECT_THAT(GetTreeResult()->GetDocumentResult(),
              MatchesDocumentResult(
                  NotRestoredReasons({NotRestoredReason::kJavaScriptExecution,
                                      NotRestoredReason::kCacheControlNoStore}),
                  BlockListedFeatures()));
}

// Test that a page with cache-control:no-store records other not restored
// reasons along with kCacheControlNoStore when there are other blocking reasons
// upon entering bfcache.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestAllowCacheControlNoStore,
    PagesWithCacheControlNoStoreRecordOtherReasonsUponEntrance) {
  ASSERT_TRUE(CreateHttpsServer()->Start());

  GURL url_a(
      https_server()->GetURL("a.com", "/set-header?Cache-Control: no-store"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

  // 1) Load the document and specify no-store for the main resource.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);
  // Use blocklisted feature.
  EXPECT_TRUE(ExecJs(rfh_a.get(), kBlockingScript));

  // 2) Navigate away. |rfh_a| should not enter bfcache.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // 3) Go back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  ExpectNotRestored({NotRestoredReason::kBlocklistedFeatures,
                     NotRestoredReason::kCacheControlNoStore},
                    {kBlockingReasonEnum}, {}, {}, {}, FROM_HERE);
  EXPECT_THAT(GetTreeResult()->GetDocumentResult(),
              MatchesDocumentResult(
                  NotRestoredReasons({NotRestoredReason::kBlocklistedFeatures,
                                      NotRestoredReason::kCacheControlNoStore}),
                  BlockListedFeatures({kBlockingReasonEnum})));
}

// Test that a page with cache-control:no-store records eviction reasons along
// with kCacheControlNoStore when the entry is evicted for other reasons.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestAllowCacheControlNoStore,
    PagesWithCacheControlNoStoreRecordOtherReasonsForEviction) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/set-header?Cache-Control: no-store"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Load the document and specify no-store for the main resource.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);

  // 2) Navigate away. |rfh_a| should enter bfcache.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Evict |rfh_a| by JavaScriptExecution.
  EvictByJavaScript(rfh_a.get());
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // 4) Go back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kJavaScriptExecution,
                     NotRestoredReason::kCacheControlNoStore},
                    {}, {}, {}, {}, FROM_HERE);
  EXPECT_THAT(GetTreeResult()->GetDocumentResult(),
              MatchesDocumentResult(
                  NotRestoredReasons({NotRestoredReason::kJavaScriptExecution,
                                      NotRestoredReason::kCacheControlNoStore}),
                  BlockListedFeatures()));
}

namespace {
const char kResponseWithNoCacheWithCookie[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Set-Cookie: foo=bar\r\n"
    "Cache-Control: no-store\r\n"
    "\r\n"
    "The server speaks HTTP!";

const char kResponseWithNoCacheWithHTTPOnlyCookie[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Set-Cookie: foo=bar; Secure; HttpOnly;\r\n"
    "Cache-Control: no-store\r\n"
    "\r\n"
    "The server speaks HTTP!";

const char kResponseWithNoCacheWithHTTPOnlyCookie2[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Set-Cookie: foo=baz; Secure; HttpOnly;\r\n"
    "Cache-Control: no-store\r\n"
    "\r\n"
    "The server speaks HTTP!";

const char kResponseWithNoCacheWithRedirectionWithHTTPOnlyCookie[] =
    "HTTP/1.1 302 Moved Temporarily\r\n"
    "Location: /redirected\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Set-Cookie: foo=baz; Secure; HttpOnly;\r\n"
    "Cache-Control: no-store\r\n"
    "\r\n";
}  // namespace

// Test that a page with cache-control:no-store enters bfcache with the flag on,
// and if a cookie is modified while it is in bfcache via response header, gets
// evicted with cookie modified marked.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestAllowCacheControlNoStore,
                       PagesWithCacheControlNoStoreSetFromResponseHeader) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/title1.html");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a_2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title3.html"));

  Shell* tab_to_be_bfcached = shell();
  Shell* tab_to_modify_cookie = CreateBrowser();

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  response.WaitForRequest();
  response.Send(kResponseWithNoCacheWithCookie);
  response.Done();
  observer.Wait();
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);
  EXPECT_EQ("foo=bar", EvalJs(tab_to_be_bfcached, "document.cookie"));

  // 2) Navigate away. |rfh_a| should enter bfcache.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Navigate to a.com in |tab_to_modify_cookie| and modify cookie from
  // JavaScript.
  EXPECT_TRUE(NavigateToURL(tab_to_modify_cookie, url_a_2));
  EXPECT_EQ("foo=bar", EvalJs(tab_to_modify_cookie, "document.cookie"));
  EXPECT_TRUE(ExecJs(tab_to_modify_cookie, "document.cookie='foo=baz'"));
  EXPECT_EQ("foo=baz", EvalJs(tab_to_modify_cookie, "document.cookie"));

  // 4) Go back. |rfh_a| should be evicted upon restoration.
  ASSERT_TRUE(HistoryGoBack(tab_to_be_bfcached->web_contents()));
  EXPECT_EQ("foo=baz", EvalJs(tab_to_be_bfcached, "document.cookie"));
  ExpectNotRestored({NotRestoredReason::kCacheControlNoStoreCookieModified}, {},
                    {}, {}, {}, FROM_HERE);
  EXPECT_THAT(GetTreeResult()->GetDocumentResult(),
              MatchesDocumentResult(
                  NotRestoredReasons(
                      {NotRestoredReason::kCacheControlNoStoreCookieModified}),
                  BlockListedFeatures()));
}

// Test that a page with cache-control:no-store enters bfcache with the flag on,
// and if HTTPOnly cookie is modified while it is in bfcache, gets evicted with
// HTTPOnly cookie modified marked.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestAllowCacheControlNoStore,
    PagesWithCacheControlNoStoreSetFromResponseHeaderHTTPOnlyCookie) {
  // HTTPOnly cookie can be only set over HTTPS.
  CreateHttpsServer();
  net::test_server::ControllableHttpResponse response(https_server(),
                                                      "/title1.html");
  net::test_server::ControllableHttpResponse response2(https_server(),
                                                       "/title2.html");
  ASSERT_TRUE(https_server()->Start());

  GURL url_a(https_server()->GetURL("a.com", "/title1.html"));
  GURL url_a_2(https_server()->GetURL("a.com", "/title2.html"));
  GURL url_b(https_server()->GetURL("b.com", "/title3.html"));

  Shell* tab_to_be_bfcached = shell();
  Shell* tab_to_modify_cookie = CreateBrowser();

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  response.WaitForRequest();
  response.Send(kResponseWithNoCacheWithHTTPOnlyCookie);
  response.Done();
  observer.Wait();
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);
  // HTTPOnly cookie should not be accessible from JavaScript.
  EXPECT_EQ("", EvalJs(tab_to_be_bfcached, "document.cookie"));

  // 2) Navigate away. |rfh_a| should enter bfcache.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Navigate to a.com in |tab_to_modify_cookie| and modify HTTPOnly cookie
  // from the response.
  TestNavigationObserver observer2(tab_to_modify_cookie->web_contents());
  tab_to_modify_cookie->LoadURL(url_a_2);
  response2.WaitForRequest();
  response2.Send(kResponseWithNoCacheWithHTTPOnlyCookie2);
  response2.Done();
  observer2.Wait();

  // 4) Go back. |rfh_a| should be evicted upon restoration.
  ASSERT_TRUE(HistoryGoBack(tab_to_be_bfcached->web_contents()));
  ExpectNotRestored(
      {NotRestoredReason::kCacheControlNoStoreHTTPOnlyCookieModified}, {}, {},
      {}, {}, FROM_HERE);
  EXPECT_THAT(
      GetTreeResult()->GetDocumentResult(),
      MatchesDocumentResult(
          NotRestoredReasons(
              {NotRestoredReason::kCacheControlNoStoreHTTPOnlyCookieModified}),
          BlockListedFeatures()));
}

// Test that a page with cache-control:no-store enters bfcache with the flag on,
// and if a HTTPOnly cookie is modified, it gets evicted with cookie changed,
// but if navigated away again and navigated back, it gets evicted without
// HTTPOnly cookie change marked.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestAllowCacheControlNoStore,
    PagesWithCacheControlNoStoreHTTPOnlyCookieModifiedBackTwice) {
  CreateHttpsServer();
  net::test_server::ControllableHttpResponse response(https_server(),
                                                      "/title1.html");
  net::test_server::ControllableHttpResponse response2(https_server(),
                                                       "/title2.html");
  net::test_server::ControllableHttpResponse response3(https_server(),
                                                       "/title1.html");
  ASSERT_TRUE(https_server()->Start());

  GURL url_a(https_server()->GetURL("a.com", "/title1.html"));
  GURL url_a_2(https_server()->GetURL("a.com", "/title2.html"));
  GURL url_b(https_server()->GetURL("b.com", "/title3.html"));

  Shell* tab_to_be_bfcached = shell();
  Shell* tab_to_modify_cookie = CreateBrowser();

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  response.WaitForRequest();
  response.Send(kResponseWithNoCacheWithHTTPOnlyCookie);
  response.Done();
  observer.Wait();
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);

  // 2) Navigate away. |rfh_a| should enter bfcache.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Navigate to a.com in |tab_to_modify_cookie| and modify cookie from
  // response header.
  TestNavigationObserver observer2(tab_to_modify_cookie->web_contents());
  tab_to_modify_cookie->LoadURL(url_a_2);
  response2.WaitForRequest();
  response2.Send(kResponseWithNoCacheWithHTTPOnlyCookie2);
  response2.Done();
  observer2.Wait();

  // 4) Go back. |rfh_a| should be evicted upon restoration.
  TestNavigationObserver observer3(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->web_contents()->GetController().GoBack();
  response3.WaitForRequest();
  response3.Send(kResponseWithNoCache);
  response3.Done();
  observer3.Wait();

  ExpectNotRestored(
      {NotRestoredReason::kCacheControlNoStoreHTTPOnlyCookieModified}, {}, {},
      {}, {}, FROM_HERE);
  EXPECT_THAT(
      GetTreeResult()->GetDocumentResult(),
      MatchesDocumentResult(
          NotRestoredReasons(
              {NotRestoredReason::kCacheControlNoStoreHTTPOnlyCookieModified}),
          BlockListedFeatures()));

  RenderFrameHostImplWrapper rfh_a_2(current_frame_host());
  rfh_a_2->GetBackForwardCacheMetrics()->SetObserverForTesting(this);

  // 5) Navigate away to b.com. |rfh_a_2| should enter bfcache again.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_b));
  EXPECT_TRUE(rfh_a_2->IsInBackForwardCache());

  // 6) Navigate back to a.com. This time the cookie change has to be reset and
  // gets evicted with a different reason.
  ASSERT_TRUE(HistoryGoBack(tab_to_be_bfcached->web_contents()));
  ExpectNotRestored({NotRestoredReason::kCacheControlNoStore}, {}, {}, {}, {},
                    FROM_HERE);
  EXPECT_THAT(GetTreeResult()->GetDocumentResult(),
              MatchesDocumentResult(
                  NotRestoredReasons({NotRestoredReason::kCacheControlNoStore}),
                  BlockListedFeatures()));
}

namespace {
// Causes a fetch request, start and complete in the target frame.
void SendFetchRequest(const ToRenderFrameHost& execution_target,
                      const GURL& url) {
  ASSERT_EQ(42, EvalJs(execution_target, JsReplace(
                                             R"(
      fetch($1)
          .then(p => {
              // Ensure that we drain the pipe to avoid blocking on network
              // activity.
              p.text();
              return 42;
          })
      )",
                                             url)));
}

// Causes an XHR, start and complete in the target frame.
void SendXhrRequest(const ToRenderFrameHost& execution_target,
                    const GURL& url) {
  ASSERT_EQ(42, EvalJs(execution_target, JsReplace(
                                             R"(
      const xhr = new XMLHttpRequest();
      xhr.open('GET', $1);
      xhr.send();
      new Promise(resolve => {
        // Use `onloadend` to ensure that the response is loaded successfully.
        xhr.onloadend = () => {resolve(42)};
      });
      )",
                                             url)));
}

// Creates an iframe in the target frame with this url. It waits until the frame
// has loaded.
void CreateIframe(const ToRenderFrameHost& execution_target, GURL url) {
  ASSERT_EQ(42, EvalJs(execution_target, JsReplace(
                                             R"(
      const iframeElement = document.createElement("iframe");
      iframeElement.src = $1;
      document.body.appendChild(iframeElement);
      new Promise(r => {
          iframeElement.onload = () => {r(42)};
      });
      )",
                                             url)));
}
}  // namespace

enum class RequestType {
  kFetch,
  kXhr,
};

// Testing the BFCache behavior when the document sends a JavaScript network
// request and receiving response with "Cache-Control: no-store" header.
class BackForwardCacheWithJsNetworkRequestReceivingCCNSResourceBrowserTest
    : public BackForwardCacheBrowserTest,
      public ::testing::WithParamInterface<RequestType> {
 public:
  // Provides meaningful param names instead of /0 and /1.
  static std::string DescribeParams(
      const ::testing::TestParamInfo<ParamType>& info) {
    switch (info.param) {
      case RequestType::kFetch:
        return "Fetch";
      case RequestType::kXhr:
        return "XHR";
    }
  }

  // TODO(crbug.com/40285326): This fails with the field trial testing config.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("disable-field-trial-config");
  }

 protected:
  // Make a JavaScript network request using the appropriate method.
  void SendJsNetworkRequest(const ToRenderFrameHost& execution_target,
                            GURL url) {
    switch (GetParam()) {
      case RequestType::kFetch:
        SendFetchRequest(execution_target, url);
        break;
      case RequestType::kXhr:
        SendXhrRequest(execution_target, url);
        break;
    }
  }
};
INSTANTIATE_TEST_SUITE_P(
    All,
    BackForwardCacheWithJsNetworkRequestReceivingCCNSResourceBrowserTest,
    ::testing::Values(RequestType::kFetch, RequestType::kXhr),
    &BackForwardCacheWithJsNetworkRequestReceivingCCNSResourceBrowserTest::
        DescribeParams);

// Test that a page without CCNS that makes a request that receives CCNS
// response does not log the
// `kJsNetworkRequestReceivedCacheControlNoStoreResource` reason.
IN_PROC_BROWSER_TEST_P(
    BackForwardCacheWithJsNetworkRequestReceivingCCNSResourceBrowserTest,
    CCNSResponseNotLogged) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL url_a_no_store(embedded_test_server()->GetURL(
      "a.com", "/set-header?Cache-Control: no-store"));

  // Load the document.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // Make a request that receives CCNS response in the main frame.
  SendJsNetworkRequest(shell(), url_a_no_store);

  // Navigate away.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // Check that the document is cached.
  ASSERT_TRUE(rfh_a->IsInBackForwardCache());

  // Go back and check that it was restored.
  ASSERT_TRUE(HistoryGoBack(shell()->web_contents()));
  ExpectRestored(FROM_HERE);
}

// Test that a page with CCNS that makes a JavaScript network request which
// receives CCNS response logs the
// `kJsNetworkRequestReceivedCacheControlNoStoreResource` reason.
IN_PROC_BROWSER_TEST_P(
    BackForwardCacheWithJsNetworkRequestReceivingCCNSResourceBrowserTest,
    CCNSResponseLoggedMainFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a_no_store(embedded_test_server()->GetURL(
      "a.com", "/set-header?Cache-Control: no-store"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Load the document and specify no-store for the main resource.
  ASSERT_TRUE(NavigateToURL(shell(), url_a_no_store));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // Make a request that receives CCNS response in the main frame.
  SendJsNetworkRequest(shell(), url_a_no_store);

  // Navigate away.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // Wait until the first document has been destroyed.
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // Go back and check that it was not restored.
  ASSERT_TRUE(HistoryGoBack(shell()->web_contents()));
  ExpectNotRestored({NotRestoredReason::kBlocklistedFeatures},
                    {BlocklistedFeature::kMainResourceHasCacheControlNoStore,
                     BlocklistedFeature::
                         kJsNetworkRequestReceivedCacheControlNoStoreResource},
                    {}, {}, {}, FROM_HERE);
}

// Test that a page with CCNS that makes a request which receives CCNS response
// in a same-as-root-origin subframe of a cross-origin subframe logs the
// `kJsNetworkRequestReceivedCacheControlNoStoreResource` reason.
IN_PROC_BROWSER_TEST_P(
    BackForwardCacheWithJsNetworkRequestReceivingCCNSResourceBrowserTest,
    CCNSResponseSameOriginSubFrameLogged) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a_no_store(embedded_test_server()->GetURL(
      "a.com", "/set-header?Cache-Control: no-store"));
  GURL url_a_2(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Load the document and specify no-store for the main resource.
  ASSERT_TRUE(NavigateToURL(shell(), url_a_no_store));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // Create a cross-origin iframe with same-as-root-origin iframe inside that
  // and make a request that receives CCNS response in that grand-child
  // iframe.
  CreateIframe(rfh_a.get(), url_b);
  CreateIframe(DescendantRenderFrameHostImplAt(rfh_a.get(), {0}),
               url_a_no_store);

  SendJsNetworkRequest(DescendantRenderFrameHostImplAt(rfh_a.get(), {0, 0}),
                       url_a_no_store);

  // Navigate away.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // Wait until the first document has been destroyed.
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // Go back and check that it was not cached and that both reasons are present.
  ASSERT_TRUE(HistoryGoBack(shell()->web_contents()));
  ExpectNotRestored({NotRestoredReason::kBlocklistedFeatures},
                    {BlocklistedFeature::kMainResourceHasCacheControlNoStore,
                     BlocklistedFeature::
                         kJsNetworkRequestReceivedCacheControlNoStoreResource},
                    {}, {}, {}, FROM_HERE);
}

// Test that a page with CCNS that makes a request which receives CCNS response
// in a same-origin subframe logs the
// `kJsNetworkRequestReceivedCacheControlNoStoreResource` reason in the correct
// place in the tree of reasons.
IN_PROC_BROWSER_TEST_P(
    BackForwardCacheWithJsNetworkRequestReceivingCCNSResourceBrowserTest,
    CCNSResponseSubFrameTree) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a_no_store(embedded_test_server()->GetURL(
      "a.com", "/set-header?Cache-Control: no-store"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Load the document and specify no-store for the main resource.
  ASSERT_TRUE(NavigateToURL(shell(), url_a_no_store));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // Create a same-origin iframe make a request that receives CCNS response.
  CreateIframe(rfh_a.get(), url_a_no_store);

  SendJsNetworkRequest(DescendantRenderFrameHostImplAt(rfh_a.get(), {0}),
                       url_a_no_store);

  // Navigate away.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // Wait until the first document has been destroyed.
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // Go back and check that it was not cached and that both reasons are present.
  ASSERT_TRUE(HistoryGoBack(shell()->web_contents()));
  ExpectNotRestored({NotRestoredReason::kBlocklistedFeatures},
                    {BlocklistedFeature::kMainResourceHasCacheControlNoStore,
                     BlocklistedFeature::
                         kJsNetworkRequestReceivedCacheControlNoStoreResource},
                    {}, {}, {}, FROM_HERE);

  auto subframe_result = MatchesNotRestoredReasons(
      /*id=*/std::nullopt, /*name=*/std::nullopt, /*src=*/url_a_no_store.spec(),
      /*reasons=*/
      {MatchesDetailedReason("response-cache-control-no-store",
                             /*source=*/std::nullopt)},
      MatchesSameOriginDetails(
          /*url=*/url_a_no_store,
          /*children=*/{}));
  EXPECT_THAT(
      current_frame_host()->NotRestoredReasonsForTesting(),
      MatchesNotRestoredReasons(
          /*id=*/std::nullopt, /*name=*/std::nullopt, /*src=*/std::nullopt,
          /*reasons=*/
          {MatchesDetailedReason("response-cache-control-no-store",
                                 /*source=*/std::nullopt)},
          MatchesSameOriginDetails(
              /*url=*/url_a_no_store,
              /*children=*/
              {subframe_result})));
}

// Test that a page with CCNS that makes a request which receives CCNS response
// in a cross-origin subframe does not log the
// `kJsNetworkRequestReceivedCacheControlNoStoreResource` reason.
IN_PROC_BROWSER_TEST_P(
    BackForwardCacheWithJsNetworkRequestReceivingCCNSResourceBrowserTest,
    CCNSResponseCrossOriginSubFrameNotLogged) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a_no_store(embedded_test_server()->GetURL(
      "a.com", "/set-header?Cache-Control: no-store"));
  GURL url_a_2(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL url_b_no_store(embedded_test_server()->GetURL(
      "b.com", "/set-header?Cache-Control: no-store"));

  // Load the document and specify no-store for the main resource.
  ASSERT_TRUE(NavigateToURL(shell(), url_a_no_store));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // Create an same-origin iframe and make a request that receives CCNS response
  // in that iframe.
  CreateIframe(rfh_a.get(), url_b_no_store);

  SendJsNetworkRequest(DescendantRenderFrameHostImplAt(rfh_a.get(), {0}),
                       url_b_no_store);

  // Navigate away.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // Wait until the first document has been destroyed.
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // Go back and check that it was not cached and that only CCNS reason is
  // present.
  ASSERT_TRUE(HistoryGoBack(shell()->web_contents()));
  ExpectNotRestored({NotRestoredReason::kBlocklistedFeatures},
                    {BlocklistedFeature::kMainResourceHasCacheControlNoStore},
                    {}, {}, {}, FROM_HERE);
}

// A subclass of `ContentBrowserTestContentBrowserClient` for testing the logic
// that checks if cookie is enabled.
class CookieDisabledContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  void SetIsCookieEnabled(bool new_value) { is_cookie_enabled_ = new_value; }

  bool CanBackForwardCachedPageReceiveCookieChanges(
      content::BrowserContext& browser_context,
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const std::optional<url::Origin>& top_frame_origin,
      const net::CookieSettingOverrides overrides) override {
    return is_cookie_enabled_;
  }

 private:
  bool is_cookie_enabled_ = true;
};
class BackForwardCacheBrowserTestRestoreCacheControlNoStoreUnlessCookieChange
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    BackForwardCacheBrowserTest::SetUpOnMainThread();
    content_browser_client_ =
        std::make_unique<CookieDisabledContentBrowserClient>();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableFeatureAndSetParams(features::kBackForwardCache, "", "");
    EnableFeatureAndSetParams(
        features::kCacheControlNoStoreEnterBackForwardCache, "level",
        "restore-unless-cookie-change");
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }

  void SetIsCookieEnabled(bool is_cookie_enabled) {
    CHECK(content_browser_client_);
    content_browser_client_->SetIsCookieEnabled(is_cookie_enabled);
  }

 private:
  std::unique_ptr<CookieDisabledContentBrowserClient> content_browser_client_;
};

// Test that a page with cache-control:no-store enters bfcache with the flag on,
// and gets restored if cookies do not change.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestRestoreCacheControlNoStoreUnlessCookieChange,
    PagesWithCacheControlNoStoreRestoreFromBackForwardCache) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/main_document");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/main_document"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(web_contents());
  shell()->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  response.WaitForRequest();
  response.Send(kResponseWithNoCache);
  response.Done();
  observer.Wait();

  // 2) Navigate away. |rfh_a| should enter the bfcache.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Go back. |rfh_a| should be restored.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectRestored(FROM_HERE);
}

// Test that a page with CCNS that makes a fetch that receives CCNS response
// is blocked even when CCNS pages are allowed to be restored. This only tests
// fetch, the blocking mechanism is the same for all kinds of requests, so if it
// works for one it will work for all.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestRestoreCacheControlNoStoreUnlessCookieChange,
    CCNSResponseBlocks) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a_no_store(embedded_test_server()->GetURL(
      "a.com", "/set-header?Cache-Control: no-store"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Load the document and specify no-store for the main resource.
  ASSERT_TRUE(NavigateToURL(shell(), url_a_no_store));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // Make a request that receives CCNS response in the main frame.
  SendFetchRequest(shell(), url_a_no_store);

  // Navigate away.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // Wait until the first document has been destroyed.
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // Go back and check that it was not restored.
  ASSERT_TRUE(HistoryGoBack(shell()->web_contents()));
  ExpectNotRestored(
      {NotRestoredReason::kBlocklistedFeatures},
      {BlocklistedFeature::kJsNetworkRequestReceivedCacheControlNoStoreResource,
       BlocklistedFeature::kMainResourceHasCacheControlNoStore},
      {}, {}, {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestRestoreCacheControlNoStoreUnlessCookieChange,
    PagesWithCacheControlNoStoreEvictedIfCookieChange) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/title1.html");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a_2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  Shell* tab_to_be_bfcached = shell();
  Shell* tab_to_modify_cookie = CreateBrowser();

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  response.WaitForRequest();
  response.Send(kResponseWithNoCache);
  response.Done();
  observer.Wait();
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);

  // 2) Navigate away. |rfh_a| should enter bfcache.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Navigate to a.com in |tab_to_modify_cookie| and modify cookie from
  // JavaScript.
  EXPECT_TRUE(NavigateToURL(tab_to_modify_cookie, url_a_2));
  EXPECT_TRUE(ExecJs(tab_to_modify_cookie, "document.cookie='foo=baz'"));
  EXPECT_EQ("foo=baz", EvalJs(tab_to_modify_cookie, "document.cookie"));

  // 4) Go back. |rfh_a| should be evicted upon restoration.
  ASSERT_TRUE(HistoryGoBack(tab_to_be_bfcached->web_contents()));

  EXPECT_EQ("foo=baz", EvalJs(tab_to_be_bfcached, "document.cookie"));
  ExpectNotRestored({NotRestoredReason::kCacheControlNoStoreCookieModified}, {},
                    {}, {}, {}, FROM_HERE);
  EXPECT_THAT(GetTreeResult()->GetDocumentResult(),
              MatchesDocumentResult(
                  NotRestoredReasons(
                      {NotRestoredReason::kCacheControlNoStoreCookieModified}),
                  BlockListedFeatures()));
}

// Test that a page with cache-control:no-store enters bfcache with the flag on,
// and gets evicted with both JavaScript and HTTPOnly cookie changes. Only
// HTTPOnly cookie reason should be recorded.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestRestoreCacheControlNoStoreUnlessCookieChange,
    PagesWithCacheControlNoStoreEvictedWithBothCookieReasons) {
  CreateHttpsServer();
  net::test_server::ControllableHttpResponse response(https_server(),
                                                      "/title1.html");
  net::test_server::ControllableHttpResponse response2(https_server(),
                                                       "/title2.html");
  ASSERT_TRUE(https_server()->Start());

  GURL url_a(https_server()->GetURL("a.com", "/title1.html"));
  GURL url_a_2(https_server()->GetURL("a.com", "/title2.html"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

  Shell* tab_to_be_bfcached = shell();
  Shell* tab_to_modify_cookie = CreateBrowser();

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  response.WaitForRequest();
  response.Send(kResponseWithNoCache);
  response.Done();
  observer.Wait();
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);

  // 2) Navigate away. |rfh_a| should enter bfcache.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  // Modify cookie from JavaScript as well.
  EXPECT_TRUE(ExecJs(tab_to_be_bfcached, "document.cookie='foo=quz'"));

  // 3) Navigate to a.com in |tab_to_modify_cookie| and modify HTTPOnly cookie
  // from the response.
  TestNavigationObserver observer2(tab_to_modify_cookie->web_contents());
  tab_to_modify_cookie->LoadURL(url_a_2);
  response2.WaitForRequest();
  response2.Send(kResponseWithNoCacheWithHTTPOnlyCookie2);
  response2.Done();
  observer2.Wait();

  // 4) Go back. |rfh_a| should be evicted upon restoration.
  ASSERT_TRUE(HistoryGoBack(tab_to_be_bfcached->web_contents()));
  ExpectNotRestored(
      {NotRestoredReason::kCacheControlNoStoreHTTPOnlyCookieModified}, {}, {},
      {}, {}, FROM_HERE);
  EXPECT_THAT(
      GetTreeResult()->GetDocumentResult(),
      MatchesDocumentResult(
          NotRestoredReasons(
              {NotRestoredReason::kCacheControlNoStoreHTTPOnlyCookieModified}),
          BlockListedFeatures()));
}

// Test that a page with cache-control:no-store gets restored if the only cookie
// modification comes from the response of the `NavigationRequest`.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestRestoreCacheControlNoStoreUnlessCookieChange,
    PagesWithCacheControlNoStoreNotBFCachedWithCookieSetInResponse) {
  CreateHttpsServer();
  net::test_server::ControllableHttpResponse response(https_server(),
                                                      "/title1.html");
  ASSERT_TRUE(https_server()->Start());

  GURL url_a(https_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

  // 1) Load the document and specify no-store for the main resource, the
  // response also sets a cookie.
  TestNavigationObserver observer(web_contents());
  shell()->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  response.WaitForRequest();
  response.Send(kResponseWithNoCacheWithCookie);
  response.Done();
  observer.Wait();
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);

  // 2) Navigate away. |rfh_a| should enter the bfcache since we only evict
  // before restoration.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Go back. |rfh_a| should be restored.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectRestored(FROM_HERE);
}

// Test that a page with `Cache-control: no-store` header gets evicted if some
// cookie is modified while the server receives the request but has not
// completed the response yet.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestRestoreCacheControlNoStoreUnlessCookieChange,
    PagesWithCacheControlNoStoreNotBFCachedWithCookieSetAfterRequestIsMade) {
  CreateHttpsServer();
  net::test_server::ControllableHttpResponse response(https_server(),
                                                      "/title1.html");
  ASSERT_TRUE(https_server()->Start());

  GURL url_a(https_server()->GetURL("a.com", "/title1.html"));
  GURL url_a_2(https_server()->GetURL("a.com", "/title2.html"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

  Shell* tab_to_be_bfcached = shell();
  Shell* tab_to_modify_cookie = CreateBrowser();

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // 2) Before the response is sent, set a cookie from another tab.
  EXPECT_TRUE(NavigateToURL(tab_to_modify_cookie, url_a_2));
  EXPECT_TRUE(ExecJs(tab_to_modify_cookie, "document.cookie='foo=bar'"));

  response.WaitForRequest();
  response.Send(kResponseWithNoCache);
  response.Done();
  observer.Wait();
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);
  EXPECT_EQ("foo=bar", EvalJs(tab_to_be_bfcached, "document.cookie"));

  // 3) Navigate away. |rfh_a| should enter the bfcache since we only evict
  // before restoration.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 4) Go back. |rfh_a| should be evicted upon restoration.
  ASSERT_TRUE(HistoryGoBack(tab_to_be_bfcached->web_contents()));
  ExpectNotRestored({NotRestoredReason::kCacheControlNoStoreCookieModified}, {},
                    {}, {}, {}, FROM_HERE);
  EXPECT_THAT(GetTreeResult()->GetDocumentResult(),
              MatchesDocumentResult(
                  NotRestoredReasons(
                      {NotRestoredReason::kCacheControlNoStoreCookieModified}),
                  BlockListedFeatures()));
}

// Test that a page with cache-control:no-store gets evicted if some cookie is
// modified before navigating away.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestRestoreCacheControlNoStoreUnlessCookieChange,
    PagesWithCacheControlNoStoreNotBFCachedWithCookieSetBeforeNavigateAway) {
  CreateHttpsServer();
  net::test_server::ControllableHttpResponse response(https_server(),
                                                      "/title1.html");
  ASSERT_TRUE(https_server()->Start());

  GURL url_a(https_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(web_contents());
  shell()->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  response.WaitForRequest();
  response.Send(kResponseWithNoCache);
  response.Done();
  observer.Wait();
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);

  // 2) Set a cookie from JavaScript.
  EXPECT_TRUE(ExecJs(web_contents(), "document.cookie='foo=bar'"));
  EXPECT_EQ("foo=bar", EvalJs(web_contents(), "document.cookie"));

  // 3) Navigate away. |rfh_a| should enter the bfcache since we only evict
  // before restoration.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 4) Go back. |rfh_a| should be evicted upon restoration.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kCacheControlNoStoreCookieModified}, {},
                    {}, {}, {}, FROM_HERE);
  EXPECT_THAT(GetTreeResult()->GetDocumentResult(),
              MatchesDocumentResult(
                  NotRestoredReasons(
                      {NotRestoredReason::kCacheControlNoStoreCookieModified}),
                  BlockListedFeatures()));
}

// Test that a page with cache-control:no-store gets evicted if some cookie is
// modified from another tab before navigating away.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestRestoreCacheControlNoStoreUnlessCookieChange,
    PagesWithCacheControlNoStoreNotBFCachedWithCookieSetFromAnotherTabBeforeNavigateAway) {
  CreateHttpsServer();
  net::test_server::ControllableHttpResponse response(https_server(),
                                                      "/title1.html");
  ASSERT_TRUE(https_server()->Start());

  GURL url_a(https_server()->GetURL("a.com", "/title1.html"));
  GURL url_a_2(https_server()->GetURL("a.com", "/title2.html"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

  Shell* tab_to_be_bfcached = shell();
  Shell* tab_to_modify_cookie = CreateBrowser();

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  response.WaitForRequest();
  response.Send(kResponseWithNoCache);
  response.Done();
  observer.Wait();
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);

  // 2) Set a cookie from another tab.
  EXPECT_TRUE(NavigateToURL(tab_to_modify_cookie, url_a_2));
  EXPECT_TRUE(ExecJs(tab_to_modify_cookie, "document.cookie='foo=bar'"));
  EXPECT_EQ("foo=bar", EvalJs(tab_to_be_bfcached, "document.cookie"));

  // 3) Navigate away. |rfh_a| should enter the bfcache since we only evict
  // before restoration.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 4) Go back. |rfh_a| should be evicted upon restoration.
  ASSERT_TRUE(HistoryGoBack(tab_to_be_bfcached->web_contents()));
  ExpectNotRestored({NotRestoredReason::kCacheControlNoStoreCookieModified}, {},
                    {}, {}, {}, FROM_HERE);
  EXPECT_THAT(GetTreeResult()->GetDocumentResult(),
              MatchesDocumentResult(
                  NotRestoredReasons(
                      {NotRestoredReason::kCacheControlNoStoreCookieModified}),
                  BlockListedFeatures()));
}

// Test that a page with cache-control:no-store gets restored if the cookie is
// modified by another tab before the navigation completes.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestRestoreCacheControlNoStoreUnlessCookieChange,
    PagesWithCacheControlNoStoreRestoredIfCookieChangeIsMadeBeforeRedirection) {
  CreateHttpsServer();
  net::test_server::ControllableHttpResponse response(https_server(),
                                                      "/main_document");
  net::test_server::ControllableHttpResponse response2(https_server(),
                                                       "/redirected");
  ASSERT_TRUE(https_server()->Start());

  GURL url_a(https_server()->GetURL("a.com", "/main_document"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

  // 1) Load the document that will be redirected to another document.
  // Both of the documents specify the cache-control:no-store, but only the
  // document before redirection sets cookie.
  TestNavigationObserver observer(web_contents());
  shell()->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  response.WaitForRequest();
  response.Send(kResponseWithNoCacheWithRedirectionWithHTTPOnlyCookie);
  response.Done();
  response2.WaitForRequest();
  response2.Send(kResponseWithNoCache);
  response2.Done();
  observer.Wait();
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);

  // 2) Navigate away. |rfh_a| should enter bfcache.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Go back. |rfh_a| should be restored from BFCache.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectRestored(FROM_HERE);
}

// Test that the cookie change information is retained after same document
// navigation.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestRestoreCacheControlNoStoreUnlessCookieChange,
    PagesWithCacheControlNoStoreNotBFCachedWithCookieSetBeforeSameDocumentNavigation) {
  CreateHttpsServer();
  net::test_server::ControllableHttpResponse response(https_server(),
                                                      "/title1.html");

  ASSERT_TRUE(https_server()->Start());

  GURL url_a(https_server()->GetURL("a.com", "/title1.html"));
  GURL url_a2(https_server()->GetURL("a.com", "/title1.html#foo"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(web_contents());
  shell()->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  response.WaitForRequest();
  response.Send(kResponseWithNoCache);
  response.Done();
  observer.Wait();
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);

  // 2) Set a cookie from JavaScript, and perform a same document navigation.
  EXPECT_TRUE(ExecJs(web_contents(), "document.cookie='foo=bar'"));
  EXPECT_EQ("foo=bar", EvalJs(web_contents(), "document.cookie"));
  EXPECT_TRUE(ExecJs(shell(), JsReplace("location = $1", url_a2.spec())));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_EQ(url_a2,
            web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL());
  EXPECT_TRUE(rfh_a->IsActive());

  // 3) Navigate away. |rfh_a| should enter the bfcache since we only evict
  // before restoration.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 4) Go back. |rfh_a| should be evicted upon restoration.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kCacheControlNoStoreCookieModified}, {},
                    {}, {}, {}, FROM_HERE);
  EXPECT_THAT(GetTreeResult()->GetDocumentResult(),
              MatchesDocumentResult(
                  NotRestoredReasons(
                      {NotRestoredReason::kCacheControlNoStoreCookieModified}),
                  BlockListedFeatures()));
}

// Test that a page with `Cache-control: no-store` header gets evicted without
// crashes if some cookie is modified immediately before the back navigation.
// TODO: this test could be potentially flaky if the notification to
// CookieChangeListener is only received after the entire back navigation
// completes. If any flaky case is reported in the future, we should fix that by
// ensuring the eviction to happen after the NavigationRequest starts to process
// response but before it finishes committing the navigation.
// See discussion from https://crrev.com/c/4408607.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestRestoreCacheControlNoStoreUnlessCookieChange,
    PagesWithCacheControlNoStoreNotBFCachedWithCookieSetImmediatelyBeforeNavigateBack) {
  CreateHttpsServer();
  net::test_server::ControllableHttpResponse response(https_server(),
                                                      "/title1.html");
  ASSERT_TRUE(https_server()->Start());

  GURL url_a(https_server()->GetURL("a.com", "/title1.html"));
  GURL url_a_2(https_server()->GetURL("a.com", "/title2.html"));

  // 1) Load the document and specify no-store for the main resource.
  {
    TestNavigationObserver observer(web_contents());
    shell()->LoadURL(url_a);
    RenderFrameHostImplWrapper rfh_a(current_frame_host());
    response.WaitForRequest();
    response.Send(kResponseWithNoCache);
    response.Done();
    observer.Wait();
    rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);
  }

  // 2) Navigate away, and set a cookie from the new page.
  EXPECT_TRUE(NavigateToURL(shell(), url_a_2));
  EXPECT_TRUE(ExecJs(shell(), "document.cookie='foo=bar'"));

  // 3) Go back. |rfh_a| should be evicted upon restoration.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kCacheControlNoStoreCookieModified}, {},
                    {}, {}, {}, FROM_HERE);
  EXPECT_THAT(GetTreeResult()->GetDocumentResult(),
              MatchesDocumentResult(
                  NotRestoredReasons(
                      {NotRestoredReason::kCacheControlNoStoreCookieModified}),
                  BlockListedFeatures()));
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestRestoreCacheControlNoStoreUnlessCookieChange,
    PagesWithCacheControlNoStoreIsNotCacheIfCookieIsDisabled) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/title1.html");
  net::test_server::ControllableHttpResponse response2(embedded_test_server(),
                                                       "/title1.html");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(shell()->web_contents());
  shell()->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  response.WaitForRequest();
  response.Send(kResponseWithNoCache);
  response.Done();
  observer.Wait();
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);

  // 2) Navigate away and back. `rfh_a` should be restored from BFCache.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  ASSERT_TRUE(HistoryGoBack(shell()->web_contents()));
  ExpectRestored(FROM_HERE);

  // 3) Disable cookies. `rfh_a` should not be stored in BFCache.
  SetIsCookieEnabled(false);
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // 4) Go back and check that the reason contains
  // `BlocklistedFeature::kMainResourceHasCacheControlNoStore`.
  TestNavigationObserver observer2(shell()->web_contents());
  shell()->web_contents()->GetController().GoBack();
  response2.WaitForRequest();
  response2.Send(kResponseWithNoCache);
  response2.Done();
  observer2.Wait();
  ExpectNotRestored({NotRestoredReason::kCacheControlNoStore,
                     NotRestoredReason::kCookieDisabled},
                    {}, {}, {}, {}, FROM_HERE);
}

class BackForwardCacheBrowserTestRestoreUnlessHTTPOnlyCookieChange
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableFeatureAndSetParams(features::kBackForwardCache, "", "");
    EnableFeatureAndSetParams(
        features::kCacheControlNoStoreEnterBackForwardCache, "level",
        "restore-unless-http-only-cookie-change");
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

// Test that a page without cache-control:no-store can enter BackForwardCache
// and gets restored if HTTPOnly Cookie changes.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestRestoreUnlessHTTPOnlyCookieChange,
    NoCacheControlNoStoreButHTTPOnlyCookieChange) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/set-header?Set-Cookie: foo=bar; Secure; HttpOnly;"));
  GURL url_a_2(embedded_test_server()->GetURL(
      "a.com", "/set-header?Set-Cookie: foo=baz; Secure; HttpOnly;"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  Shell* tab_to_be_bfcached = shell();
  Shell* tab_to_modify_cookie = CreateBrowser();

  // 1) Load the document without cache-control:no-store.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // 2) Navigate away. |rfh_a| should enter bfcache.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Navigate to a.com in |tab_to_modify_cookie| and modify HTTPOnly cookie
  // from the header.
  EXPECT_TRUE(NavigateToURL(tab_to_modify_cookie, url_a_2));

  // 4) Go back. |rfh_a| should be restored from bfcache.
  ASSERT_TRUE(HistoryGoBack(tab_to_be_bfcached->web_contents()));

  ExpectRestored(FROM_HERE);
}

// Test that a page with cache-control:no-store enters bfcache with the flag on,
// and does not get evicted if normal cookies change.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestRestoreUnlessHTTPOnlyCookieChange,
    PagesWithCacheControlNoStoreNotEvictedIfNormalCookieChange) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/set-header?Cache-Control: no-store"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  Shell* tab_to_be_bfcached = shell();
  Shell* tab_to_modify_cookie = CreateBrowser();

  // 1) Load the document and specify no-store for the main resource.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // 2) Navigate away. |rfh_a| should enter bfcache.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Navigate to a.com in |tab_to_modify_cookie| and modify cookie from
  // JavaScript.
  EXPECT_TRUE(NavigateToURL(tab_to_modify_cookie, url_a));
  EXPECT_TRUE(ExecJs(tab_to_modify_cookie, "document.cookie='foo=baz'"));
  EXPECT_EQ("foo=baz", EvalJs(tab_to_modify_cookie, "document.cookie"));

  // 4) Go back. |rfh_a| should be restored from bfcache.
  ASSERT_TRUE(HistoryGoBack(tab_to_be_bfcached->web_contents()));

  EXPECT_EQ("foo=baz", EvalJs(tab_to_be_bfcached, "document.cookie"));
  ExpectRestored(FROM_HERE);
}

// Test that a page with cache-control:no-store enters bfcache with the flag on,
// and gets evicted if HTTPOnly cookie changes.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestRestoreUnlessHTTPOnlyCookieChange,
    PagesWithCacheControlNoStoreEvictedIfHTTPOnlyCookieChange) {
  CreateHttpsServer();
  net::test_server::ControllableHttpResponse response(https_server(),
                                                      "/title1.html");
  net::test_server::ControllableHttpResponse response2(https_server(),
                                                       "/title2.html");
  ASSERT_TRUE(https_server()->Start());

  GURL url_a(https_server()->GetURL("a.com", "/title1.html"));
  GURL url_a_2(https_server()->GetURL("a.com", "/title2.html"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

  Shell* tab_to_be_bfcached = shell();
  Shell* tab_to_modify_cookie = CreateBrowser();

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  response.WaitForRequest();
  response.Send(kResponseWithNoCache);
  response.Done();
  observer.Wait();
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);

  // 2) Navigate away. |rfh_a| should enter bfcache.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Navigate to a.com in |tab_to_modify_cookie| and modify HTTPOnly cookie
  // from the response.
  TestNavigationObserver observer2(tab_to_modify_cookie->web_contents());
  tab_to_modify_cookie->LoadURL(url_a_2);
  response2.WaitForRequest();
  response2.Send(kResponseWithNoCacheWithHTTPOnlyCookie2);
  response2.Done();
  observer2.Wait();

  // 4) Go back. |rfh_a| should be evicted upon restoration.
  ASSERT_TRUE(HistoryGoBack(tab_to_be_bfcached->web_contents()));
  ExpectNotRestored(
      {NotRestoredReason::kCacheControlNoStoreHTTPOnlyCookieModified}, {}, {},
      {}, {}, FROM_HERE);
  EXPECT_THAT(
      GetTreeResult()->GetDocumentResult(),
      MatchesDocumentResult(
          NotRestoredReasons(
              {NotRestoredReason::kCacheControlNoStoreHTTPOnlyCookieModified}),
          BlockListedFeatures()));
}

// Test that a page with cache-control:no-store enters bfcache with the flag on,
// and gets evicted if HTTPOnly cookie changes.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestRestoreUnlessHTTPOnlyCookieChange,
    PagesWithCacheControlNoStoreEvictedIfJSAndHTTPOnlyCookieChange) {
  CreateHttpsServer();
  net::test_server::ControllableHttpResponse response(https_server(),
                                                      "/title1.html");
  net::test_server::ControllableHttpResponse response2(https_server(),
                                                       "/title2.html");
  ASSERT_TRUE(https_server()->Start());

  GURL url_a(https_server()->GetURL("a.com", "/title1.html"));
  GURL url_a_2(https_server()->GetURL("a.com", "/title2.html"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

  Shell* tab_to_be_bfcached = shell();
  Shell* tab_to_modify_cookie = CreateBrowser();

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  response.WaitForRequest();
  response.Send(kResponseWithNoCache);
  response.Done();
  observer.Wait();
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);

  // 2) Navigate away. |rfh_a| should enter bfcache.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  // Modify cookie from JavaScript as well.
  EXPECT_TRUE(ExecJs(tab_to_be_bfcached, "document.cookie='foo=quz'"));

  // 3) Navigate to a.com in |tab_to_modify_cookie| and modify HTTPOnly cookie
  // from the response.
  TestNavigationObserver observer2(tab_to_modify_cookie->web_contents());
  tab_to_modify_cookie->LoadURL(url_a_2);
  response2.WaitForRequest();
  response2.Send(kResponseWithNoCacheWithHTTPOnlyCookie2);
  response2.Done();
  observer2.Wait();

  // 4) Go back. |rfh_a| should be evicted upon restoration.
  ASSERT_TRUE(HistoryGoBack(tab_to_be_bfcached->web_contents()));
  ExpectNotRestored(
      {NotRestoredReason::kCacheControlNoStoreHTTPOnlyCookieModified}, {}, {},
      {}, {}, FROM_HERE);
  EXPECT_THAT(
      GetTreeResult()->GetDocumentResult(),
      MatchesDocumentResult(
          NotRestoredReasons(
              {NotRestoredReason::kCacheControlNoStoreHTTPOnlyCookieModified}),
          BlockListedFeatures()));
}

// Test that a page with cache-control:no-store gets restored if the HTTPOnly
// cookie modification comes from the response of the `NavigationRequest`.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestRestoreUnlessHTTPOnlyCookieChange,
    PagesWithCacheControlNoStoreNotBFCachedWithHTTPOnlyCookieSetInResponse) {
  CreateHttpsServer();
  net::test_server::ControllableHttpResponse response(https_server(),
                                                      "/title1.html");
  ASSERT_TRUE(https_server()->Start());

  GURL url_a(https_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

  // 1) Load the document and specify no-store for the main resource, the
  // response also sets a cookie.
  TestNavigationObserver observer(web_contents());
  shell()->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  response.WaitForRequest();
  response.Send(kResponseWithNoCacheWithHTTPOnlyCookie);
  response.Done();
  observer.Wait();
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);

  // 2) Navigate away. |rfh_a| should enter the bfcache since we only evict
  // before restoration.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Go back. |rfh_a| should be restored.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectRestored(FROM_HERE);
}

// Test that a page with cache-control:no-store gets evicted if some HTTPOnly
// cookie is modified from another tab before navigating away.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestRestoreUnlessHTTPOnlyCookieChange,
    PagesWithCacheControlNoStoreNotBFCachedWithHTTPOnlyCookieSetFromAnotherTabBeforeNavigateAway) {
  CreateHttpsServer();
  net::test_server::ControllableHttpResponse response(https_server(),
                                                      "/title1.html");
  net::test_server::ControllableHttpResponse response2(https_server(),
                                                       "/title2.html");
  ASSERT_TRUE(https_server()->Start());

  GURL url_a(https_server()->GetURL("a.com", "/title1.html"));
  GURL url_a_2(https_server()->GetURL("a.com", "/title2.html"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

  Shell* tab_to_be_bfcached = shell();
  Shell* tab_to_modify_cookie = CreateBrowser();

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  response.WaitForRequest();
  response.Send(kResponseWithNoCache);
  response.Done();
  observer.Wait();
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);

  // 2) Set an HTTPOnly cookie from another tab.
  TestNavigationObserver observer2(tab_to_modify_cookie->web_contents());
  tab_to_modify_cookie->LoadURL(url_a_2);
  response2.WaitForRequest();
  response2.Send(kResponseWithNoCacheWithHTTPOnlyCookie);
  response2.Done();
  observer2.Wait();

  // 3) Navigate away. |rfh_a| should enter the bfcache since we only evict
  // before restoration.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 4) Go back. |rfh_a| should be evicted upon restoration.
  ASSERT_TRUE(HistoryGoBack(tab_to_be_bfcached->web_contents()));
  ExpectNotRestored(
      {NotRestoredReason::kCacheControlNoStoreHTTPOnlyCookieModified}, {}, {},
      {}, {}, FROM_HERE);
  EXPECT_THAT(
      GetTreeResult()->GetDocumentResult(),
      MatchesDocumentResult(
          NotRestoredReasons(
              {NotRestoredReason::kCacheControlNoStoreHTTPOnlyCookieModified}),
          BlockListedFeatures()));
}

// Test that the cookie change information is retained after same document
// navigation.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestRestoreUnlessHTTPOnlyCookieChange,
    PagesWithCacheControlNoStoreNotBFCachedWithHTTPOnlyCookieSetBeforeSameDocumentNavigation) {
  CreateHttpsServer();
  net::test_server::ControllableHttpResponse response(https_server(),
                                                      "/title1.html");
  net::test_server::ControllableHttpResponse response2(https_server(),
                                                       "/title1.html");

  ASSERT_TRUE(https_server()->Start());

  GURL url_a(https_server()->GetURL("a.com", "/title1.html"));
  GURL url_a2(https_server()->GetURL("a.com", "/title1.html#foo"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

  Shell* tab_to_be_bfcached = shell();
  Shell* tab_to_modify_cookie = CreateBrowser();

  // 1) Load the document and specify no-store for the main resource.
  TestNavigationObserver observer(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  response.WaitForRequest();
  response.Send(kResponseWithNoCache);
  response.Done();
  observer.Wait();
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);

  // 2) Modify the HTTPOnly cookie from another tab.
  TestNavigationObserver observer2(tab_to_modify_cookie->web_contents());
  tab_to_modify_cookie->LoadURL(url_a);
  response2.WaitForRequest();
  response2.Send(kResponseWithNoCacheWithHTTPOnlyCookie);
  response2.Done();
  observer2.Wait();

  // 3) Perform a same document navigation.
  EXPECT_TRUE(ExecJs(shell(), JsReplace("location = $1", url_a2.spec())));
  EXPECT_TRUE(WaitForLoadStop(tab_to_be_bfcached->web_contents()));
  EXPECT_EQ(url_a2, tab_to_be_bfcached->web_contents()
                        ->GetPrimaryMainFrame()
                        ->GetLastCommittedURL());
  EXPECT_TRUE(rfh_a->IsActive());

  // 4) Navigate away. |rfh_a| should enter the bfcache since we only evict
  // before restoration.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 5) Go back. |rfh_a| should be evicted upon restoration.
  ASSERT_TRUE(HistoryGoBack(tab_to_be_bfcached->web_contents()));
  ExpectNotRestored(
      {NotRestoredReason::kCacheControlNoStoreHTTPOnlyCookieModified}, {}, {},
      {}, {}, FROM_HERE);
  EXPECT_THAT(
      GetTreeResult()->GetDocumentResult(),
      MatchesDocumentResult(
          NotRestoredReasons(
              {NotRestoredReason::kCacheControlNoStoreHTTPOnlyCookieModified}),
          BlockListedFeatures()));
}

}  // namespace content
