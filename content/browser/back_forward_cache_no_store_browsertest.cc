// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/back_forward_cache_browsertest.h"

#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromecast_buildflags.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
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

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
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

// Disabled for being flaky. See crbug.com/1116190.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, SubframeWithNoStoreCached) {
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

namespace {

class BackForwardCacheBrowserTestAllowCacheControlNoStore
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableFeatureAndSetParams(features::kBackForwardCache, "", "");
    EnableFeatureAndSetParams(kCacheControlNoStoreEnterBackForwardCache,
                              "level", "store-and-evict");
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

}  // namespace

// TODO(https://crbug.com/1231849): flaky on Cast Linux.
// Test that a page with cache-control:no-store enters bfcache with the flag on,
// but does not get restored and gets evicted.
// Turned off on cast for https://crbug.com/1281665 , along with others.
#if BUILDFLAG(IS_CASTOS)
#define MAYBE_PagesWithCacheControlNoStoreEnterBfcacheAndEvicted \
        DISABLED_PagesWithCacheControlNoStoreEnterBfcacheAndEvicted
#else
#define MAYBE_PagesWithCacheControlNoStoreEnterBfcacheAndEvicted \
        PagesWithCacheControlNoStoreEnterBfcacheAndEvicted
#endif  // BUILDFLAG(IS_CASTOS)
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestAllowCacheControlNoStore,
                       MAYBE_PagesWithCacheControlNoStoreEnterBfcacheAndEvicted) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/main_document");
  net::test_server::ControllableHttpResponse response2(embedded_test_server(),
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
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);

  // 2) Navigate away. |rfh_a| should enter the bfcache.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Go back. |rfh_a| should be evicted upon restoration.
  TestNavigationObserver observer2(web_contents());
  web_contents()->GetController().GoBack();
  response2.WaitForRequest();
  response2.Send(kResponseWithNoCache);
  response2.Done();
  observer2.Wait();

  ExpectNotRestored({NotRestoredReason::kCacheControlNoStore}, {}, {}, {}, {},
                    FROM_HERE);
  // Make sure that the tree result also has the same reason.
  EXPECT_THAT(GetTreeResult()->GetDocumentResult(),
              MatchesDocumentResult(
                  NotRestoredReasons(NotRestoredReason::kCacheControlNoStore),
                  BlockListedFeatures()));
}

// Test that a page with cache-control:no-store enters bfcache with the flag on,
// and if a cookie is modified while it is in bfcache via JavaScript, gets
// evicted with cookie modified marked.
// Turned off on cast for https://crbug.com/1281665 .
#if BUILDFLAG(IS_CASTOS)
#define MAYBE_PagesWithCacheControlNoStoreCookieModifiedThroughJavaScript \
        DISABLED_PagesWithCacheControlNoStoreCookieModifiedThroughJavaScript
#else
#define MAYBE_PagesWithCacheControlNoStoreCookieModifiedThroughJavaScript \
        PagesWithCacheControlNoStoreCookieModifiedThroughJavaScript
#endif  // BUILDFLAG(IS_CASTOS)
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestAllowCacheControlNoStore,
    MAYBE_PagesWithCacheControlNoStoreCookieModifiedThroughJavaScript) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/main_document");
  net::test_server::ControllableHttpResponse response2(embedded_test_server(),
                                                       "/main_document");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/main_document"));
  GURL url_a_2(embedded_test_server()->GetURL("a.com", "/title1.html"));
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
  TestNavigationObserver observer2(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->web_contents()->GetController().GoBack();
  response2.WaitForRequest();
  response2.Send(kResponseWithNoCache);
  response2.Done();
  observer2.Wait();

  EXPECT_EQ("foo=baz", EvalJs(tab_to_be_bfcached, "document.cookie"));
  ExpectNotRestored({NotRestoredReason::kCacheControlNoStoreCookieModified}, {},
                    {}, {}, {}, FROM_HERE);
  // Make sure that the tree result also has the same reason.
  EXPECT_THAT(GetTreeResult()->GetDocumentResult(),
              MatchesDocumentResult(
                  NotRestoredReasons(
                      NotRestoredReason::kCacheControlNoStoreCookieModified),
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
                      NotRestoredReason::kCacheControlNoStoreCookieModified),
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
                  NotRestoredReasons(NotRestoredReason::kCacheControlNoStore),
                  BlockListedFeatures()));
}

// Test that a page with cache-control:no-store enters bfcache with the flag on,
// and even if a cookie is modified on a different domain than the entry, the
// entry is not marked as cookie modified.
// Turned off on cast for https://crbug.com/1281665 .
//
// TODO(crbug.com/1336055): It may be possible to re-enable this test now that
// crbug.com/1229182 has been resolved. This was originally disabled due to
// flakiness on Cast Audio Linux.
#if BUILDFLAG(IS_CASTOS)
#define MAYBE_PagesWithCacheControlNoStoreCookieModifiedThroughJavaScriptOnDifferentDomain \
        DISABLED_PagesWithCacheControlNoStoreCookieModifiedThroughJavaScriptOnDifferentDomain
#else
#define MAYBE_PagesWithCacheControlNoStoreCookieModifiedThroughJavaScriptOnDifferentDomain \
        PagesWithCacheControlNoStoreCookieModifiedThroughJavaScriptOnDifferentDomain
#endif  // BUILDFLAG(IS_CASTOS)
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestAllowCacheControlNoStore,
    MAYBE_PagesWithCacheControlNoStoreCookieModifiedThroughJavaScriptOnDifferentDomain) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/main_document");
  net::test_server::ControllableHttpResponse response2(embedded_test_server(),
                                                       "/main_document");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/main_document"));
  GURL url_a_2(embedded_test_server()->GetURL("a.com", "/title1.html"));
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

  // 2) Set a normal cookie from JavaScript.
  EXPECT_TRUE(ExecJs(tab_to_be_bfcached, "document.cookie='foo=bar'"));
  EXPECT_EQ("foo=bar", EvalJs(tab_to_be_bfcached, "document.cookie"));

  // 3) Navigate away. |rfh_a| should enter bfcache.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 4) Navigate to b.com in |tab_to_modify_cookie| and modify cookie from
  // JavaScript.
  EXPECT_TRUE(NavigateToURL(tab_to_modify_cookie, url_b));
  EXPECT_TRUE(ExecJs(tab_to_modify_cookie, "document.cookie='foo=baz'"));
  EXPECT_EQ("foo=baz", EvalJs(tab_to_modify_cookie, "document.cookie"));

  // 5) Go back. |rfh_a| should be evicted upon restoration.
  TestNavigationObserver observer2(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->web_contents()->GetController().GoBack();
  response2.WaitForRequest();
  response2.Send(kResponseWithNoCache);
  response2.Done();
  observer2.Wait();
  EXPECT_EQ("foo=bar", EvalJs(tab_to_be_bfcached, "document.cookie"));

  ExpectNotRestored({NotRestoredReason::kCacheControlNoStore}, {}, {}, {}, {},
                    FROM_HERE);
  EXPECT_THAT(GetTreeResult()->GetDocumentResult(),
              MatchesDocumentResult(
                  NotRestoredReasons(NotRestoredReason::kCacheControlNoStore),
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
                  NotRestoredReasons(NotRestoredReason::kJavaScriptExecution,
                                     NotRestoredReason::kCacheControlNoStore),
                  BlockListedFeatures()));
}

// Test that a page with cache-control:no-store records other not restored
// reasons along with kCacheControlNoStore when there are other blocking reasons
// upon entering bfcache.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestAllowCacheControlNoStore,
    PagesWithCacheControlNoStoreRecordOtherReasonsUponEntrance) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/set-header?Cache-Control: no-store"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Load the document and specify no-store for the main resource.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);
  // Use blocklisted feature.
  EXPECT_TRUE(ExecJs(rfh_a.get(), "window.foo = new BroadcastChannel('foo');"));

  // 2) Navigate away. |rfh_a| should not enter bfcache.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // 3) Go back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  ExpectNotRestored(
      {NotRestoredReason::kBlocklistedFeatures,
       NotRestoredReason::kCacheControlNoStore},
      {blink::scheduler::WebSchedulerTrackedFeature::kBroadcastChannel}, {}, {},
      {}, FROM_HERE);
  EXPECT_THAT(
      GetTreeResult()->GetDocumentResult(),
      MatchesDocumentResult(
          NotRestoredReasons(NotRestoredReason::kBlocklistedFeatures,
                             NotRestoredReason::kCacheControlNoStore),
          BlockListedFeatures(blink::scheduler::WebSchedulerTrackedFeature::
                                  kBroadcastChannel)));
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
                  NotRestoredReasons(NotRestoredReason::kJavaScriptExecution,
                                     NotRestoredReason::kCacheControlNoStore),
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
}  // namespace

// TODO(crbug.com/1336055): It may be possible to re-enable this test now that
// crbug.com/1229182 has been resolved. This was originally disabled due to
// flakiness on Cast Audio Linux.
#if BUILDFLAG(IS_CASTOS)
#define MAYBE_PagesWithCacheControlNoStoreSetFromResponseHeader \
  DISABLED_PagesWithCacheControlNoStoreSetFromResponseHeader
#elif BUILDFLAG(IS_LINUX)
// Flaky: https://crbug.com/1353858
#define MAYBE_PagesWithCacheControlNoStoreSetFromResponseHeader \
  DISABLED_PagesWithCacheControlNoStoreSetFromResponseHeader
#else
#define MAYBE_PagesWithCacheControlNoStoreSetFromResponseHeader \
  PagesWithCacheControlNoStoreSetFromResponseHeader
#endif
// Test that a page with cache-control:no-store enters bfcache with the flag on,
// and if a cookie is modified while it is in bfcache via response header, gets
// evicted with cookie modified marked.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestAllowCacheControlNoStore,
    MAYBE_PagesWithCacheControlNoStoreSetFromResponseHeader) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/main_document");
  net::test_server::ControllableHttpResponse response2(embedded_test_server(),
                                                       "/main_document");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/main_document"));
  GURL url_a_2(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

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
  TestNavigationObserver observer2(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->web_contents()->GetController().GoBack();
  response2.WaitForRequest();
  // Send the response without the cookie header to avoid overwriting the
  // cookie.
  response2.Send(kResponseWithNoCache);
  response2.Done();
  observer2.Wait();
  EXPECT_EQ("foo=baz", EvalJs(tab_to_be_bfcached, "document.cookie"));
  ExpectNotRestored({NotRestoredReason::kCacheControlNoStoreCookieModified}, {},
                    {}, {}, {}, FROM_HERE);
  EXPECT_THAT(GetTreeResult()->GetDocumentResult(),
              MatchesDocumentResult(
                  NotRestoredReasons(
                      NotRestoredReason::kCacheControlNoStoreCookieModified),
                  BlockListedFeatures()));
}

// TODO(crbug.com/1336055): It may be possible to re-enable this test now that
// crbug.com/1229182 has been resolved. This was originally disabled due to
// flakiness on Cast Audio Linux.
#if BUILDFLAG(IS_CASTOS)
#define MAYBE_PagesWithCacheControlNoStoreSetFromResponseHeaderHTTPOnlyCookie \
  DISABLED_PagesWithCacheControlNoStoreSetFromResponseHeaderHTTPOnlyCookie
#else
#define MAYBE_PagesWithCacheControlNoStoreSetFromResponseHeaderHTTPOnlyCookie \
  PagesWithCacheControlNoStoreSetFromResponseHeaderHTTPOnlyCookie
#endif  // BUILDFLAG(IS_CASTOS)
// Test that a page with cache-control:no-store enters bfcache with the flag on,
// and if HTTPOnly cookie is modified while it is in bfcache, gets evicted with
// HTTPOnly cookie modified marked.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestAllowCacheControlNoStore,
    MAYBE_PagesWithCacheControlNoStoreSetFromResponseHeaderHTTPOnlyCookie) {
  // HTTPOnly cookie can be only set over HTTPS.
  CreateHttpsServer();
  net::test_server::ControllableHttpResponse response(https_server(),
                                                      "/main_document");
  net::test_server::ControllableHttpResponse response2(https_server(),
                                                       "/main_document2");
  net::test_server::ControllableHttpResponse response3(https_server(),
                                                       "/main_document");
  ASSERT_TRUE(https_server()->Start());

  GURL url_a(https_server()->GetURL("a.com", "/main_document"));
  GURL url_a_2(https_server()->GetURL("a.com", "/main_document2"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

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
  TestNavigationObserver observer3(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->web_contents()->GetController().GoBack();
  response3.WaitForRequest();
  response3.Send(kResponseWithNoCacheWithHTTPOnlyCookie);
  response3.Done();
  observer3.Wait();
  ExpectNotRestored(
      {NotRestoredReason::kCacheControlNoStoreHTTPOnlyCookieModified}, {}, {},
      {}, {}, FROM_HERE);
  EXPECT_THAT(
      GetTreeResult()->GetDocumentResult(),
      MatchesDocumentResult(
          NotRestoredReasons(
              NotRestoredReason::kCacheControlNoStoreHTTPOnlyCookieModified),
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
                                                      "/main_document");
  net::test_server::ControllableHttpResponse response2(https_server(),
                                                       "/main_document2");
  net::test_server::ControllableHttpResponse response3(https_server(),
                                                       "/main_document");
  net::test_server::ControllableHttpResponse response4(https_server(),
                                                       "/main_document");
  ASSERT_TRUE(https_server()->Start());

  GURL url_a(https_server()->GetURL("a.com", "/main_document"));
  GURL url_a_2(https_server()->GetURL("a.com", "/main_document2"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

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
              NotRestoredReason::kCacheControlNoStoreHTTPOnlyCookieModified),
          BlockListedFeatures()));

  RenderFrameHostImplWrapper rfh_a_2(current_frame_host());
  rfh_a_2->GetBackForwardCacheMetrics()->SetObserverForTesting(this);

  // 5) Navigate away to b.com. |rfh_a_2| should enter bfcache again.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_b));
  EXPECT_TRUE(rfh_a_2->IsInBackForwardCache());

  // 6) Navigate back to a.com. This time the cookie change has to be reset and
  // gets evicted with a different reason.
  TestNavigationObserver observer4(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->web_contents()->GetController().GoBack();
  response4.WaitForRequest();
  response4.Send(kResponseWithNoCache);
  response4.Done();
  observer4.Wait();
  ExpectNotRestored({NotRestoredReason::kCacheControlNoStore}, {}, {}, {}, {},
                    FROM_HERE);
  EXPECT_THAT(GetTreeResult()->GetDocumentResult(),
              MatchesDocumentResult(
                  NotRestoredReasons(NotRestoredReason::kCacheControlNoStore),
                  BlockListedFeatures()));
}

namespace {
// Causes a fetch using the "Authorization" header to start and complete in the
// target frame.
void UseAuthorizationHeader(const ToRenderFrameHost& execution_target,
                            GURL url) {
  ASSERT_EQ(42, EvalJs(execution_target, JsReplace(
                                             R"(
      fetch($1, {headers: {Authorization: 'foo'}})
          .then(p   => {
              // Ensure that we drain the pipe to avoid blocking on network
              // activity.
              p.text();
              return 42;
          })
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

// Test that a page without CCNS that makes a fetch with the "Authorization"
// header does not log the header.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       AuthorizationHeaderNotLogged) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Load the document.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // Do a fetch with the "Authorization" header in the main frame.
  UseAuthorizationHeader(shell(), url_a);

  // Navigate away.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // Check that the document is cached.
  ASSERT_TRUE(rfh_a->IsInBackForwardCache());

  // Go back and check that it was restored.
  ASSERT_TRUE(HistoryGoBack(shell()->web_contents()));
  ExpectRestored(FROM_HERE);
}

// Test that a page with CCNS that makes a fetch with the "Authorization" header
// logs the header.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       AuthorizationHeaderLoggedMainFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a_no_store(embedded_test_server()->GetURL(
      "a.com", "/set-header?Cache-Control: no-store"));
  GURL url_a_2(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Load the document and specify no-store for the main resource.
  ASSERT_TRUE(NavigateToURL(shell(), url_a_no_store));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // Do a fetch with the "Authorization" header in the main frame.
  UseAuthorizationHeader(shell(), url_a_2);

  // Navigate away.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // Wait until the first document has been destroyed.
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // Go back and check that it was not restored.
  ASSERT_TRUE(HistoryGoBack(shell()->web_contents()));
  ExpectNotRestored({NotRestoredReason::kBlocklistedFeatures},
                    {BlocklistedFeature::kMainResourceHasCacheControlNoStore,
                     BlocklistedFeature::kAuthorizationHeader},
                    {}, {}, {}, FROM_HERE);
}

// Test that a page with CCNS that makes a fetch with the "Authorization" header
// in a same-as-root-origin subframe of a cross-origin subframe logs the header.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       AuthorizationHeaderSameOriginSubFrameLogged) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a_no_store(embedded_test_server()->GetURL(
      "a.com", "/set-header?Cache-Control: no-store"));
  GURL url_a_2(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Load the document and specify no-store for the main resource.
  ASSERT_TRUE(NavigateToURL(shell(), url_a_no_store));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // Create a cross-origin iframe with same-as-root-origin iframe inside that
  // and do a fetch with the "Authorization" header in that grand-child iframe.
  CreateIframe(rfh_a.get(), url_b);
  CreateIframe(DescendantRenderFrameHostImplAt(rfh_a.get(), {0}), url_a_2);

  UseAuthorizationHeader(DescendantRenderFrameHostImplAt(rfh_a.get(), {0, 0}),
                         url_a_2);

  // Navigate away.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // Wait until the first document has been destroyed.
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // Go back and check that it was not cached and that both reasons are present.
  ASSERT_TRUE(HistoryGoBack(shell()->web_contents()));
  ExpectNotRestored({NotRestoredReason::kBlocklistedFeatures},
                    {BlocklistedFeature::kMainResourceHasCacheControlNoStore,
                     BlocklistedFeature::kAuthorizationHeader},
                    {}, {}, {}, FROM_HERE);
}

// Test that a page with CCNS that makes a fetch with the "Authorization" header
// in a same-origin subframe logs the header in the correct place in the tree of
// reasons.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       AuthorizationHeaderSubFrameTree) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a_no_store(embedded_test_server()->GetURL(
      "a.com", "/set-header?Cache-Control: no-store"));
  GURL url_a_2(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Load the document and specify no-store for the main resource.
  ASSERT_TRUE(NavigateToURL(shell(), url_a_no_store));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // Create a same-origin iframe do a fetch with the "Authorization" header.
  CreateIframe(rfh_a.get(), url_a_2);

  UseAuthorizationHeader(DescendantRenderFrameHostImplAt(rfh_a.get(), {0}),
                         url_a_2);

  // Navigate away.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // Wait until the first document has been destroyed.
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // Go back and check that it was not cached and that both reasons are present.
  ASSERT_TRUE(HistoryGoBack(shell()->web_contents()));
  ExpectNotRestored({NotRestoredReason::kBlocklistedFeatures},
                    {BlocklistedFeature::kMainResourceHasCacheControlNoStore,
                     BlocklistedFeature::kAuthorizationHeader},
                    {}, {}, {}, FROM_HERE);

  auto subframe_result = MatchesNotRestoredReasons(
      blink::mojom::BFCacheBlocked::kYes,
      MatchesSameOriginDetails(
          /*id=*/"", /*name=*/"", /*src=*/url_a_2.spec(),
          /*url=*/url_a_2.spec(),
          /*reasons=*/{"AuthorizationHeader"},
          /*children=*/{}));
  EXPECT_THAT(current_frame_host()->NotRestoredReasonsForTesting(),
              MatchesNotRestoredReasons(
                  blink::mojom::BFCacheBlocked::kYes,
                  MatchesSameOriginDetails(
                      /*id=*/"", /*name=*/"", /*src=*/"",
                      /*url=*/url_a_no_store.spec(),
                      /*reasons=*/{"MainResourceHasCacheControlNoStore"},
                      /*children=*/
                      {subframe_result})));
}

// Test that a page with CCNS that makes a fetch with the "Authorization" header
// in a cross-origin subframe does not log the header.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       AuthorizationHeaderCrossOriginSubFrameNotLogged) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a_no_store(embedded_test_server()->GetURL(
      "a.com", "/set-header?Cache-Control: no-store"));
  GURL url_a_2(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Load the document and specify no-store for the main resource.
  ASSERT_TRUE(NavigateToURL(shell(), url_a_no_store));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // Create an same-origin iframe and do a fetch with the "Authorization" header
  // in that iframe.
  CreateIframe(rfh_a.get(), url_b);

  UseAuthorizationHeader(DescendantRenderFrameHostImplAt(rfh_a.get(), {0}),
                         url_b);

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

class BackForwardCacheBrowserTestRestoreCacheControlNoStoreUnlessCookieChange
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableFeatureAndSetParams(features::kBackForwardCache, "", "");
    EnableFeatureAndSetParams(kCacheControlNoStoreEnterBackForwardCache,
                              "level", "restore-unless-cookie-change");
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

// Test that a page with cache-control:no-store enters bfcache with the flag on,
// and gets restored if cookies do not change.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestRestoreCacheControlNoStoreUnlessCookieChange,
    PagesWithCacheControlNoStoreRestoreFromBackForwardCache) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/main_document");
  net::test_server::ControllableHttpResponse response2(embedded_test_server(),
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

// Test that a page with CCNS that makes a fetch with the "Authorization" header
// is blocked even when CCNS pages are allowed to be restored.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestRestoreCacheControlNoStoreUnlessCookieChange,
    AuthorizationHeaderBlocks) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a_no_store(embedded_test_server()->GetURL(
      "a.com", "/set-header?Cache-Control: no-store"));
  GURL url_a_2(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Load the document and specify no-store for the main resource.
  ASSERT_TRUE(NavigateToURL(shell(), url_a_no_store));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // Do a fetch with the "Authorization" header in the main frame.
  UseAuthorizationHeader(shell(), url_a_2);

  // Navigate away.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // Wait until the first document has been destroyed.
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // Go back and check that it was not restored.
  ASSERT_TRUE(HistoryGoBack(shell()->web_contents()));
  ExpectNotRestored({NotRestoredReason::kBlocklistedFeatures},
                    {BlocklistedFeature::kAuthorizationHeader}, {}, {}, {},
                    FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestRestoreCacheControlNoStoreUnlessCookieChange,
    PagesWithCacheControlNoStoreEvictedIfCookieChange) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/main_document");
  net::test_server::ControllableHttpResponse response2(embedded_test_server(),
                                                       "/main_document");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/main_document"));
  GURL url_a_2(embedded_test_server()->GetURL("a.com", "/title1.html"));
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
  TestNavigationObserver observer2(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->web_contents()->GetController().GoBack();
  response2.WaitForRequest();
  response2.Send(kResponseWithNoCache);
  response2.Done();
  observer2.Wait();

  EXPECT_EQ("foo=baz", EvalJs(tab_to_be_bfcached, "document.cookie"));
  ExpectNotRestored({NotRestoredReason::kCacheControlNoStoreCookieModified}, {},
                    {}, {}, {}, FROM_HERE);
  EXPECT_THAT(GetTreeResult()->GetDocumentResult(),
              MatchesDocumentResult(
                  NotRestoredReasons(
                      NotRestoredReason::kCacheControlNoStoreCookieModified),
                  BlockListedFeatures()));
}

// TODO(https://crbug.com/1231849): flaky on Cast Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_PagesWithCacheControlNoStoreEvictedWithBothCookieReasons \
  DISABLED_PagesWithCacheControlNoStoreEvictedWithBothCookieReasons
#else
#define MAYBE_PagesWithCacheControlNoStoreEvictedWithBothCookieReasons \
  PagesWithCacheControlNoStoreEvictedWithBothCookieReasons
#endif

// Test that a page with cache-control:no-store enters bfcache with the flag on,
// and gets evicted with both JavaScript and HTTPOnly cookie changes. Only
// HTTPOnly cookie reason should be recorded.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestRestoreCacheControlNoStoreUnlessCookieChange,
    MAYBE_PagesWithCacheControlNoStoreEvictedWithBothCookieReasons) {
  CreateHttpsServer();
  net::test_server::ControllableHttpResponse response(https_server(),
                                                      "/main_document");
  net::test_server::ControllableHttpResponse response2(https_server(),
                                                       "/main_document2");
  net::test_server::ControllableHttpResponse response3(https_server(),
                                                       "/main_document");
  ASSERT_TRUE(https_server()->Start());

  GURL url_a(https_server()->GetURL("a.com", "/main_document"));
  GURL url_a_2(https_server()->GetURL("a.com", "/main_document2"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

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
  TestNavigationObserver observer3(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->web_contents()->GetController().GoBack();
  response3.WaitForRequest();
  response3.Send(kResponseWithNoCacheWithHTTPOnlyCookie);
  response3.Done();
  observer3.Wait();
  ExpectNotRestored(
      {NotRestoredReason::kCacheControlNoStoreHTTPOnlyCookieModified}, {}, {},
      {}, {}, FROM_HERE);
  EXPECT_THAT(
      GetTreeResult()->GetDocumentResult(),
      MatchesDocumentResult(
          NotRestoredReasons(
              NotRestoredReason::kCacheControlNoStoreHTTPOnlyCookieModified),
          BlockListedFeatures()));
}

class BackForwardCacheBrowserTestRestoreUnlessHTTPOnlyCookieChange
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableFeatureAndSetParams(features::kBackForwardCache, "", "");
    EnableFeatureAndSetParams(kCacheControlNoStoreEnterBackForwardCache,
                              "level",
                              "restore-unless-http-only-cookie-change");
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

// TODO(https://crbug.com/1231849): flaky on Cast Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_NoCacheControlNoStoreButHTTPOnlyCookieChange \
  DISABLED_NoCacheControlNoStoreButHTTPOnlyCookieChange
#else
#define MAYBE_NoCacheControlNoStoreButHTTPOnlyCookieChange \
  NoCacheControlNoStoreButHTTPOnlyCookieChange
#endif
// Test that a page without cache-control:no-store can enter BackForwardCache
// and gets restored if HTTPOnly Cookie changes.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestRestoreUnlessHTTPOnlyCookieChange,
    MAYBE_NoCacheControlNoStoreButHTTPOnlyCookieChange) {
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

// TODO(https://crbug.com/1231849): flaky on Cast Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_PagesWithCacheControlNoStoreNotEvictedIfNormalCookieChange \
  DISABLED_PagesWithCacheControlNoStoreNotEvictedIfNormalCookieChange
#else
#define MAYBE_PagesWithCacheControlNoStoreNotEvictedIfNormalCookieChange \
  PagesWithCacheControlNoStoreNotEvictedIfNormalCookieChange
#endif
// Test that a page with cache-control:no-store enters bfcache with the flag on,
// and does not get evicted if normal cookies change.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestRestoreUnlessHTTPOnlyCookieChange,
    MAYBE_PagesWithCacheControlNoStoreNotEvictedIfNormalCookieChange) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/set-header?Cache-Control: no-store"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  Shell* tab_to_be_bfcached = shell();
  Shell* tab_to_modify_cookie = CreateBrowser();

  // 1) Load the document and specify no-store for the main resource.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // 2) Set a normal cookie from JavaScript.
  EXPECT_TRUE(ExecJs(tab_to_be_bfcached, "document.cookie='foo=bar'"));
  EXPECT_EQ("foo=bar", EvalJs(tab_to_be_bfcached, "document.cookie"));

  // 3) Navigate away. |rfh_a| should enter bfcache.
  EXPECT_TRUE(NavigateToURL(tab_to_be_bfcached, url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 4) Navigate to a.com in |tab_to_modify_cookie| and modify cookie from
  // JavaScript.
  EXPECT_TRUE(NavigateToURL(tab_to_modify_cookie, url_a));
  EXPECT_EQ("foo=bar", EvalJs(tab_to_modify_cookie, "document.cookie"));
  EXPECT_TRUE(ExecJs(tab_to_modify_cookie, "document.cookie='foo=baz'"));
  EXPECT_EQ("foo=baz", EvalJs(tab_to_modify_cookie, "document.cookie"));

  // 5) Go back. |rfh_a| should be restored from bfcache.
  ASSERT_TRUE(HistoryGoBack(tab_to_be_bfcached->web_contents()));

  EXPECT_EQ("foo=baz", EvalJs(tab_to_be_bfcached, "document.cookie"));
  ExpectRestored(FROM_HERE);
}

// TODO(https://crbug.com/1231849): flaky on Cast Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_PagesWithCacheControlNoStoreEvictedIfHTTPOnlyCookieChange \
  DISABLED_PagesWithCacheControlNoStoreEvictedIfHTTPOnlyCookieChange
#else
#define MAYBE_PagesWithCacheControlNoStoreEvictedIfHTTPOnlyCookieChange \
  PagesWithCacheControlNoStoreEvictedIfHTTPOnlyCookieChange
#endif
// Test that a page with cache-control:no-store enters bfcache with the flag on,
// and gets evicted if HTTPOnly cookie changes.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestRestoreCacheControlNoStoreUnlessCookieChange,
    MAYBE_PagesWithCacheControlNoStoreEvictedIfHTTPOnlyCookieChange) {
  CreateHttpsServer();
  net::test_server::ControllableHttpResponse response(https_server(),
                                                      "/main_document");
  net::test_server::ControllableHttpResponse response2(https_server(),
                                                       "/main_document2");
  net::test_server::ControllableHttpResponse response3(https_server(),
                                                       "/main_document");
  ASSERT_TRUE(https_server()->Start());

  GURL url_a(https_server()->GetURL("a.com", "/main_document"));
  GURL url_a_2(https_server()->GetURL("a.com", "/main_document2"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

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

  // 3) Navigate to a.com in |tab_to_modify_cookie| and modify HTTPOnly cookie
  // from the response.
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
  response3.Send(kResponseWithNoCacheWithHTTPOnlyCookie);
  response3.Done();
  observer3.Wait();
  ExpectNotRestored(
      {NotRestoredReason::kCacheControlNoStoreHTTPOnlyCookieModified}, {}, {},
      {}, {}, FROM_HERE);
  EXPECT_THAT(
      GetTreeResult()->GetDocumentResult(),
      MatchesDocumentResult(
          NotRestoredReasons(
              NotRestoredReason::kCacheControlNoStoreHTTPOnlyCookieModified),
          BlockListedFeatures()));
}

// TODO(https://crbug.com/1231849): flaky on Cast Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_PagesWithCacheControlNoStoreEvictedIfJSAndHTTPOnlyCookieChange \
  DISABLED_PagesWithCacheControlNoStoreEvictedIfJSAndHTTPOnlyCookieChange
#else
#define MAYBE_PagesWithCacheControlNoStoreEvictedIfJSAndHTTPOnlyCookieChange \
  PagesWithCacheControlNoStoreEvictedIfJSAndHTTPOnlyCookieChange
#endif
// Test that a page with cache-control:no-store enters bfcache with the flag on,
// and gets evicted if HTTPOnly cookie changes.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestRestoreCacheControlNoStoreUnlessCookieChange,
    MAYBE_PagesWithCacheControlNoStoreEvictedIfJSAndHTTPOnlyCookieChange) {
  CreateHttpsServer();
  net::test_server::ControllableHttpResponse response(https_server(),
                                                      "/main_document");
  net::test_server::ControllableHttpResponse response2(https_server(),
                                                       "/main_document2");
  net::test_server::ControllableHttpResponse response3(https_server(),
                                                       "/main_document");
  ASSERT_TRUE(https_server()->Start());

  GURL url_a(https_server()->GetURL("a.com", "/main_document"));
  GURL url_a_2(https_server()->GetURL("a.com", "/main_document2"));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

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
  TestNavigationObserver observer3(tab_to_be_bfcached->web_contents());
  tab_to_be_bfcached->web_contents()->GetController().GoBack();
  response3.WaitForRequest();
  response3.Send(kResponseWithNoCacheWithHTTPOnlyCookie);
  response3.Done();
  observer3.Wait();
  ExpectNotRestored(
      {NotRestoredReason::kCacheControlNoStoreHTTPOnlyCookieModified}, {}, {},
      {}, {}, FROM_HERE);
  EXPECT_THAT(
      GetTreeResult()->GetDocumentResult(),
      MatchesDocumentResult(
          NotRestoredReasons(
              NotRestoredReason::kCacheControlNoStoreHTTPOnlyCookieModified),
          BlockListedFeatures()));
}

}  // namespace content
