// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/back_forward_cache_browsertest.h"

#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/controllable_http_response.h"

// This file contains back-/forward-cache tests for fetching from the network.
// It was forked from
// https://source.chromium.org/chromium/chromium/src/+/main:content/browser/back_forward_cache_browsertest.cc;drc=748acc7b301b489567691500c558c5fde8cfd538
//
// When adding tests please also add WPTs. See
// third_party/blink/web_tests/external/wpt/html/browsers/browsing-the-web/back-forward-cache/README.md

namespace content {

using NotRestoredReason = BackForwardCacheMetrics::NotRestoredReason;

// When loading task is unfreezable with the feature flag
// kLoadingTaskUnfreezable, a page will keep processing the in-flight network
// requests while the page is frozen in BackForwardCache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, FetchWhileStoring) {
  net::test_server::ControllableHttpResponse fetch_response(
      embedded_test_server(), "/fetch");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // Use "fetch" immediately before being frozen.
  EXPECT_TRUE(ExecJs(rfh_a, R"(
    document.addEventListener('freeze', event => {
      my_fetch = fetch('/fetch', { keepalive: true});
    });
  )"));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  fetch_response.WaitForRequest();
  fetch_response.Send(net::HTTP_OK, "text/html", "TheResponse");
  fetch_response.Done();
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(delete_observer_rfh_a.deleted());

  // 3) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectRestored(FROM_HERE);
}

// Eviction is triggered when a normal fetch request gets redirected while the
// page is in back-forward cache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       FetchRedirectedWhileStoring) {
  net::test_server::ControllableHttpResponse fetch_response(
      embedded_test_server(), "/fetch");
  net::test_server::ControllableHttpResponse fetch2_response(
      embedded_test_server(), "/fetch2");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // Trigger a fetch.
  ExecuteScriptAsync(rfh_a, "my_fetch = fetch('/fetch');");

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // Page A is initially stored in the back-forward cache.
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Respond the fetch with a redirect.
  fetch_response.WaitForRequest();
  fetch_response.Send(
      "HTTP/1.1 302 Moved Temporarily\r\n"
      "Location: /fetch2");
  fetch_response.Done();

  // Ensure that the request to /fetch2 was never sent (because the page is
  // immediately evicted) by checking after 3 seconds.
  base::RunLoop loop;
  base::OneShotTimer timer;
  timer.Start(FROM_HERE, base::Seconds(3), loop.QuitClosure());
  loop.Run();
  EXPECT_EQ(nullptr, fetch2_response.http_request());

  // Page A should be evicted from the back-forward cache.
  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kNetworkRequestRedirected}, {}, {}, {},
                    {}, FROM_HERE);
}

// Eviction is triggered when a keepalive fetch request gets redirected while
// the page is in back-forward cache.
// TODO(https://crbug.com/1137682): We should not trigger eviction on redirects
// of keepalive fetches.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       KeepAliveFetchRedirectedWhileStoring) {
  net::test_server::ControllableHttpResponse fetch_response(
      embedded_test_server(), "/fetch");
  net::test_server::ControllableHttpResponse fetch2_response(
      embedded_test_server(), "/fetch2");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // Trigger a keepalive fetch.
  ExecuteScriptAsync(rfh_a, "my_fetch = fetch('/fetch', { keepalive: true });");

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // Page A is initially stored in the back-forward cache.
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Respond the fetch with a redirect.
  fetch_response.WaitForRequest();
  fetch_response.Send(
      "HTTP/1.1 302 Moved Temporarily\r\n"
      "Location: /fetch2");
  fetch_response.Done();

  // Ensure that the request to /fetch2 was never sent (because the page is
  // immediately evicted) by checking after 3 seconds.
  // TODO(https://crbug.com/1137682): We should not trigger eviction on
  // redirects of keepalive fetches and the redirect request should be sent.
  base::RunLoop loop;
  base::OneShotTimer timer;
  timer.Start(FROM_HERE, base::Seconds(3), loop.QuitClosure());
  loop.Run();
  EXPECT_EQ(nullptr, fetch2_response.http_request());

  // Page A should be evicted from the back-forward cache.
  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kNetworkRequestRedirected}, {}, {}, {},
                    {}, FROM_HERE);
}

// Tests the case when the header was received before the page is frozen,
// but parts of the response body is received when the page is frozen.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       PageWithDrainedDatapipeRequestsForFetchShouldBeEvicted) {
  net::test_server::ControllableHttpResponse fetch_response(
      embedded_test_server(), "/fetch");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // Call fetch before navigating away.
  EXPECT_TRUE(ExecJs(rfh_a, R"(
    var fetch_response_promise = my_fetch = fetch('/fetch').then(response => {
        return response.text();
    });
  )"));
  // Send response header and a piece of the body before navigating away.
  fetch_response.WaitForRequest();
  fetch_response.Send(net::HTTP_OK, "text/plain");
  fetch_response.Send("body");

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored(
      {NotRestoredReason::kNetworkRequestDatapipeDrainedAsBytesConsumer}, {},
      {}, {}, {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    PageWithDrainedDatapipeRequestsForScriptStreamerShouldNotBeEvicted) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/small_script.js");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/empty.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/empty.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  // Append the script tag.
  EXPECT_TRUE(ExecJs(shell(), R"(
    var script = document.createElement('script');
    script.src = 'small_script.js'
    document.body.appendChild(script);
  )"));

  response.WaitForRequest();
  // Send the small_script.js but not complete, so that the datapipe is passed
  // to ScriptStreamer upon bfcache entrance.
  const char kHttpResponseHeader[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n";
  response.Send(kHttpResponseHeader);
  response.Send("alert('more than 4 bytes');");

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  // Complete the response after navigating away.
  response.Send("alert('more than 4 bytes');");
  response.Done();

  // 3) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectRestored(FROM_HERE);
}

// TODO(crbug.com/1236190) Disabled for flaky failures on various configs.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    DISABLED_PageWithDrainedDatapipeRequestsForScriptStreamerShouldBeEvictedIfStreamedTooMuch) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/small_script.js");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/empty.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/empty.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  // Append the script tag.
  EXPECT_TRUE(ExecJs(shell(), R"(
    var script = document.createElement('script');
    script.src = 'small_script.js'
    document.body.appendChild(script);
  )"));

  response.WaitForRequest();
  // Send the small_script.js but not complete, so that the datapipe is passed
  // to ScriptStreamer upon bfcache entrance.
  const char kHttpResponseHeader[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n";
  response.Send(kHttpResponseHeader);
  response.Send("alert('more than 4 bytes');");

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // Complete the response after navigating away.
  std::string body(kMaxBufferedBytesPerProcess + 1, '*');
  response.Send(body);
  response.Done();

  // 3) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kNetworkExceedsBufferLimit}, {}, {}, {},
                    {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       ImageStillLoading_ResponseStartedWhileFrozen) {
  net::test_server::ControllableHttpResponse image_response(
      embedded_test_server(), "/image.png");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with an image with src == "image.png".
  RenderFrameHostImpl* rfh_1 = NavigateToPageWithImage(
      embedded_test_server()->GetURL("a.com", "/title1.html"));
  image_response.WaitForRequest();

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title2.html")));
  // The page was still loading when we navigated away, but it's still eligible
  // for back-forward cache.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  // Start sending the image body while in the back-forward cache.
  image_response.Send(net::HTTP_OK, "image/png");
  image_response.Send("image_body");
  image_response.Done();

  // 3) Go back to the first page. We should restore the page from the
  // back-forward cache.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectRestored(FROM_HERE);

  // Wait until the deferred body is processed. Since it's not a valid image
  // value, we'll get the "error" event.
  EXPECT_EQ("error", EvalJs(rfh_1, "image_load_status"));
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    ImageStillLoading_ResponseStartedWhileRestoring_DoNotTriggerEviction) {
  net::test_server::ControllableHttpResponse image_response(
      embedded_test_server(), "/image.png");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with an image with src == "image.png".
  GURL url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  RenderFrameHostImpl* rfh_1 = NavigateToPageWithImage(url);

  // Wait for the image request, but don't send anything yet.
  image_response.WaitForRequest();

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title2.html")));
  // The page was still loading when we navigated away, but it's still eligible
  // for back-forward cache.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  // 3) Go back to the first page using TestActivationManager so that we split
  // the navigation into stages.
  TestActivationManager restore_activation_manager(shell()->web_contents(),
                                                   url);
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(restore_activation_manager.WaitForBeforeChecks());

  // Before we try to commit the navigation, BFCache will defer to wait
  // asynchronously for renderers to reply that they've unfrozen. Finish the
  // image response in that time.
  restore_activation_manager.ResumeActivation();
  auto* navigation_request =
      NavigationRequest::From(restore_activation_manager.GetNavigationHandle());
  ASSERT_TRUE(
      navigation_request->IsCommitDeferringConditionDeferredForTesting());
  ASSERT_FALSE(restore_activation_manager.is_paused());
  ASSERT_FALSE(navigation_request->HasCommitted());

  image_response.Send(net::HTTP_OK, "image/png");
  std::string body(kMaxBufferedBytesPerProcess + 1, '*');
  image_response.Send(body);
  image_response.Done();

  // Finish the navigation.
  restore_activation_manager.WaitForNavigationFinished();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ExpectRestored(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    ImageStillLoading_ResponseStartedWhileFrozen_ExceedsPerProcessBytesLimit) {
  net::test_server::ControllableHttpResponse image1_response(
      embedded_test_server(), "/image1.png");
  net::test_server::ControllableHttpResponse image2_response(
      embedded_test_server(), "/image2.png");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with 2 images.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  RenderFrameHostImpl* rfh_1 = current_frame_host();
  // Wait for the document to load DOM to ensure that kLoading is not
  // one of the reasons why the document wasn't cached.
  ASSERT_TRUE(WaitForDOMContentLoaded(rfh_1));

  EXPECT_TRUE(ExecJs(rfh_1, R"(
      var image1 = document.createElement("img");
      image1.src = "image1.png";
      document.body.appendChild(image1);
      var image2 = document.createElement("img");
      image2.src = "image2.png";
      document.body.appendChild(image1);

      var image1_load_status = new Promise((resolve, reject) => {
        image1.onload = () => { resolve("loaded"); }
        image1.onerror = () => { resolve("error"); }
      });

      var image2_load_status = new Promise((resolve, reject) => {
        image2.onload = () => { resolve("loaded"); }
        image2.onerror = () => { resolve("error"); }
      });
    )"));

  // Wait for the image requests, but don't send anything yet.
  image1_response.WaitForRequest();
  image2_response.WaitForRequest();

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title2.html")));
  // The page was still loading when we navigated away, but it's still eligible
  // for back-forward cache.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  RenderFrameDeletedObserver delete_observer(rfh_1);
  // Start sending the image responses while in the back-forward cache. The
  // body size of the responses individually is less than the per-process limit,
  // but together they surpass the per-process limit.
  const int image_body_size = kMaxBufferedBytesPerProcess / 2 + 1;
  std::string body(image_body_size, '*');
  image1_response.Send(net::HTTP_OK, "image/png");
  image1_response.Send(body);
  image1_response.Done();
  image2_response.Send(net::HTTP_OK, "image/png");
  image2_response.Send(body);
  image2_response.Done();
  delete_observer.WaitUntilDeleted();

  // 3) Go back to the first page. We should not restore the page from the
  // back-forward cache.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kNetworkExceedsBufferLimit}, {}, {}, {},
                    {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    ImageStillLoading_ResponseStartedWhileFrozen_ExceedsPerProcessBytesLimit_SameSiteSubframe) {
  net::test_server::ControllableHttpResponse image1_response(
      embedded_test_server(), "/image1.png");
  net::test_server::ControllableHttpResponse image2_response(
      embedded_test_server(), "/image2.png");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate main frame to a page with 1 image.
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         "a.com", "/page_with_iframe.html")));
  RenderFrameHostImpl* main_rfh = current_frame_host();
  // Wait for the document to load DOM to ensure that kLoading is not
  // one of the reasons why the document wasn't cached.
  ASSERT_TRUE(WaitForDOMContentLoaded(main_rfh));

  EXPECT_TRUE(ExecJs(main_rfh, R"(
      var image1 = document.createElement("img");
      image1.src = "image1.png";
      document.body.appendChild(image1);
      var image1_load_status = new Promise((resolve, reject) => {
        image1.onload = () => { resolve("loaded"); }
        image1.onerror = () => { resolve("error"); }
      });
    )"));

  // 2) Add 1 image to the subframe.
  RenderFrameHostImpl* subframe_rfh =
      main_rfh->child_at(0)->current_frame_host();

  // First, wait for the subframe document to load DOM to ensure that kLoading
  // is not one of the reasons why the document wasn't cached.
  EXPECT_TRUE(WaitForDOMContentLoaded(subframe_rfh));

  EXPECT_TRUE(ExecJs(subframe_rfh, R"(
      var image2 = document.createElement("img");
      image2.src = "image2.png";
      document.body.appendChild(image2);
      var image2_load_status = new Promise((resolve, reject) => {
        image2.onload = () => { resolve("loaded"); }
        image2.onerror = () => { resolve("error"); }
      });
    )"));

  // Wait for the image requests, but don't send anything yet.
  image1_response.WaitForRequest();
  image2_response.WaitForRequest();

  // 3) Navigate away on the main frame.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title2.html")));
  // The page was still loading images when we navigated away, but it's still
  // eligible for back-forward cache.
  EXPECT_TRUE(main_rfh->IsInBackForwardCache());
  EXPECT_TRUE(subframe_rfh->IsInBackForwardCache());

  RenderFrameDeletedObserver delete_observer_1(main_rfh);
  RenderFrameDeletedObserver delete_observer_2(subframe_rfh);
  // Start sending the image responses while in the back-forward cache. The
  // body size of the responses individually is less than the per-process limit,
  // but together they surpass the per-process limit since both the main frame
  // and the subframe are put in the same renderer process (because they're
  // same-site).
  const int image_body_size = kMaxBufferedBytesPerProcess / 2 + 1;
  std::string body(image_body_size, '*');
  image1_response.Send(net::HTTP_OK, "image/png");
  image1_response.Send(body);
  image1_response.Done();
  image2_response.Send(net::HTTP_OK, "image/png");
  image2_response.Send(body);
  image2_response.Done();
  delete_observer_1.WaitUntilDeleted();
  delete_observer_2.WaitUntilDeleted();

  // 3) Go back to the first page. We should not restore the page from the
  // back-forward cache.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kNetworkExceedsBufferLimit}, {}, {}, {},
                    {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    ImageStillLoading_ResponseStartedWhileFrozen_ExceedsPerProcessBytesLimit_ResetOnRestore) {
  net::test_server::ControllableHttpResponse image1_response(
      embedded_test_server(), "/image.png");
  net::test_server::ControllableHttpResponse image2_response(
      embedded_test_server(), "/image2.png");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with an image with src == "image.png".
  RenderFrameHostImpl* rfh_1 = NavigateToPageWithImage(
      embedded_test_server()->GetURL("a.com", "/title1.html"));

  // Wait for the image request, but don't send anything yet.
  image1_response.WaitForRequest();

  // 2) Navigate away on the main frame.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title2.html")));
  RenderFrameHostImpl* rfh_2 = current_frame_host();
  ASSERT_TRUE(WaitForDOMContentLoaded(rfh_2));

  // The first page was still loading images when we navigated away, but it's
  // still eligible for back-forward cache.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  // 3) Add 1 image to the second page.
  EXPECT_TRUE(ExecJs(rfh_2, R"(
      var image2 = document.createElement("img");
      image2.src = "image2.png";
      document.body.appendChild(image2);
      var image2_load_status = new Promise((resolve, reject) => {
        image2.onload = () => { resolve("loaded"); }
        image2.onerror = () => { resolve("error"); }
      });
    )"));
  image2_response.WaitForRequest();

  // Start sending the image response for the first page while in the
  // back-forward cache. The body size of the response is half of the
  // per-process limit.
  const int image_body_size = kMaxBufferedBytesPerProcess / 2 + 1;
  std::string body(image_body_size, '*');
  image1_response.Send(net::HTTP_OK, "image/png");
  image1_response.Send(body);
  image1_response.Done();

  // 4) Go back to the first page. We should restore the page from the
  // back-forward cache.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectRestored(FROM_HERE);

  // The second page was still loading images when we navigated away, but it's
  // still eligible for back-forward cache.
  EXPECT_TRUE(rfh_2->IsInBackForwardCache());

  // Start sending the image response for the second page's image request.
  // The second page should still stay in the back-forward cache since the
  // per-process buffer limit is reset back to 0 after the first page gets
  // restored from the back-forward cache, so we wouldn't go over the
  // per-process buffer limit even when the total body size buffered during the
  // lifetime of the test actually exceeds the per-process buffer limit.
  image2_response.Send(net::HTTP_OK, "image/png");
  image2_response.Send(body);
  image2_response.Done();

  EXPECT_TRUE(rfh_2->IsInBackForwardCache());

  // 5) Go forward. We should restore the second page from the back-forward
  // cache.
  ASSERT_TRUE(HistoryGoForward(web_contents()));
  ExpectRestored(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    ImageStillLoading_ResponseStartedWhileFrozen_ExceedsPerProcessBytesLimit_ResetOnDetach) {
  net::test_server::ControllableHttpResponse image1_response(
      embedded_test_server(), "/image.png");
  net::test_server::ControllableHttpResponse image2_response(
      embedded_test_server(), "/image2.png");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with an image with src == "image.png".
  RenderFrameHostImpl* rfh_1 = NavigateToPageWithImage(
      embedded_test_server()->GetURL("a.com", "/title1.html"));

  // Wait for the image request, but don't send anything yet.
  image1_response.WaitForRequest();

  // 2) Navigate away on the main frame.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title2.html")));
  RenderFrameHostImpl* rfh_2 = current_frame_host();
  ASSERT_TRUE(WaitForDOMContentLoaded(rfh_2));

  // The first page was still loading images when we navigated away, but it's
  // still eligible for back-forward cache.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  // 3) Add 1 image to the second page.
  EXPECT_TRUE(ExecJs(rfh_2, R"(
      var image2 = document.createElement("img");
      image2.src = "image2.png";
      document.body.appendChild(image2);
      var image2_load_status = new Promise((resolve, reject) => {
        image2.onload = () => { resolve("loaded"); }
        image2.onerror = () => { resolve("error"); }
      });
    )"));
  image2_response.WaitForRequest();

  RenderFrameDeletedObserver delete_observer_1(rfh_1);
  // Start sending an image response that's larger than the per-process and
  // per-request buffer limit, causing the page to get evicted from the
  // back-forward cache.
  std::string body(kMaxBufferedBytesPerProcess + 1, '*');
  image1_response.Send(net::HTTP_OK, "image/png");
  image1_response.Send(body);
  image1_response.Done();
  delete_observer_1.WaitUntilDeleted();

  // 4) Go back to the first page. We should not restore the page from the
  // back-forward cache.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kNetworkExceedsBufferLimit}, {}, {}, {},
                    {}, FROM_HERE);

  // The second page was still loading images when we navigated away, but it's
  // still eligible for back-forward cache.
  EXPECT_TRUE(rfh_2->IsInBackForwardCache());

  // Start sending a small image response for the second page's image request.
  // The second page should still stay in the back-forward cache since the
  // per-process buffer limit is reset back to 0 after the first page gets
  // evicted and deleted
  image2_response.Send(net::HTTP_OK, "image/png");
  image2_response.Send("*");
  image2_response.Done();

  EXPECT_TRUE(rfh_2->IsInBackForwardCache());

  // 5) Go forward. We should restore the second page from the back-forward
  // cache.
  ASSERT_TRUE(HistoryGoForward(web_contents()));
  ExpectRestored(FROM_HERE);

  // Wait until the deferred body is processed. Since it's not a valid image
  // value, we'll get the "error" event.
  EXPECT_EQ("error", EvalJs(rfh_2, "image2_load_status"));
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       ImageStillLoading_ResponseStartedWhileFrozen_Timeout) {
  net::test_server::ControllableHttpResponse image_response(
      embedded_test_server(), "/image.png");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with an image with src == "image.png".
  RenderFrameHostImpl* rfh_1 = NavigateToPageWithImage(
      embedded_test_server()->GetURL("a.com", "/title1.html"));

  // Wait for the image request, but don't send anything yet.
  image_response.WaitForRequest();

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title2.html")));
  // The page was still loading when we navigated away, but it's still eligible
  // for back-forward cache.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  RenderFrameDeletedObserver delete_observer(rfh_1);
  // Start sending the image response while in the back-forward cache, but never
  // finish the request. Eventually the page will get deleted due to network
  // request timeout.
  image_response.Send(net::HTTP_OK, "image/png");
  delete_observer.WaitUntilDeleted();

  // 3) Go back to the first page. We should not restore the page from the
  // back-forward cache.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kNetworkRequestTimeout}, {}, {}, {}, {},
                    FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    ImageStillLoading_ResponseStartedBeforeFreezing_ExceedsPerProcessBytesLimit) {
  net::test_server::ControllableHttpResponse image1_response(
      embedded_test_server(), "/image1.png");
  net::test_server::ControllableHttpResponse image2_response(
      embedded_test_server(), "/image2.png");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with 2 images.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  RenderFrameHostImpl* rfh_1 = current_frame_host();
  // Wait for the document to load DOM to ensure that kLoading is not
  // one of the reasons why the document wasn't cached.
  ASSERT_TRUE(WaitForDOMContentLoaded(rfh_1));

  EXPECT_TRUE(ExecJs(rfh_1, R"(
      var image1 = document.createElement("img");
      image1.src = "image1.png";
      document.body.appendChild(image1);
      var image2 = document.createElement("img");
      image2.src = "image2.png";
      document.body.appendChild(image1);

      var image1_load_status = new Promise((resolve, reject) => {
        image1.onload = () => { resolve("loaded"); }
        image1.onerror = () => { resolve("error"); }
      });

      var image2_load_status = new Promise((resolve, reject) => {
        image2.onload = () => { resolve("loaded"); }
        image2.onerror = () => { resolve("error"); }
      });
    )"));

  // Wait for the image requests, but don't send anything yet.

  // Start sending response before the page gets in the back-forward cache.
  image1_response.WaitForRequest();
  image1_response.Send(net::HTTP_OK, "image/png");
  image1_response.Send(" ");
  image2_response.WaitForRequest();
  image2_response.Send(net::HTTP_OK, "image/png");
  image2_response.Send(" ");
  // Run some script to ensure the renderer processed its pending tasks.
  EXPECT_TRUE(ExecJs(rfh_1, "var foo = 42;"));

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title2.html")));
  // The page was still loading when we navigated away, but it's still eligible
  // for back-forward cache.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  RenderFrameDeletedObserver delete_observer(rfh_1);
  // Send the image response body while in the back-forward cache. The body size
  // of the responses individually is less than the per-process limit, but
  // together they surpass the per-process limit.
  const int image_body_size = kMaxBufferedBytesPerProcess / 2 + 1;
  std::string body(image_body_size, '*');
  image1_response.Send(body);
  image1_response.Done();
  image2_response.Send(body);
  image2_response.Done();
  delete_observer.WaitUntilDeleted();

  // 3) Go back to the first page. We should not restore the page from the
  // back-forward cache.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kNetworkExceedsBufferLimit}, {}, {}, {},
                    {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       TimeoutNotTriggeredAfterDone) {
  net::test_server::ControllableHttpResponse image_response(
      embedded_test_server(), "/image.png");
  ASSERT_TRUE(embedded_test_server()->Start());
  // 1) Navigate to a page with an image with src == "image.png".
  RenderFrameHostImpl* rfh_1 = NavigateToPageWithImage(
      embedded_test_server()->GetURL("a.com", "/title1.html"));

  // Wait for the image request, but don't send anything yet.
  image_response.WaitForRequest();

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title2.html")));
  // The page was still loading when we navigated away, but it's still eligible
  // for back-forward cache.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  RenderFrameDeletedObserver delete_observer(rfh_1);
  // Start sending the image response while in the back-forward cache and finish
  // the request before the active request timeout hits.
  image_response.Send(net::HTTP_OK, "image/png");
  image_response.Send(" ");
  image_response.Done();

  // Make sure enough time passed to trigger network request eviction if the
  // load above didn't finish.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      kGracePeriodToFinishLoading + base::Seconds(1));
  run_loop.Run();

  // Ensure that the page is still in bfcache.
  EXPECT_FALSE(delete_observer.deleted());
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  // 3) Go back to the first page. We should restore the page from the
  // back-forward cache.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectRestored(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    TimeoutNotTriggeredAfterDone_ResponseStartedBeforeFreezing) {
  net::test_server::ControllableHttpResponse image_response(
      embedded_test_server(), "/image.png");
  ASSERT_TRUE(embedded_test_server()->Start());
  // 1) Navigate to a page with an image with src == "image.png".
  RenderFrameHostImpl* rfh_1 = NavigateToPageWithImage(
      embedded_test_server()->GetURL("a.com", "/title1.html"));

  // Start sending response before the page gets in the back-forward cache.
  image_response.WaitForRequest();
  image_response.Send(net::HTTP_OK, "image/png");
  image_response.Send(" ");

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title2.html")));
  // The page was still loading when we navigated away, but it's still eligible
  // for back-forward cache.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  RenderFrameDeletedObserver delete_observer(rfh_1);
  // Finish the request before the active request timeout hits.
  image_response.Done();

  // Make sure enough time passed to trigger network request eviction if the
  // load above didn't finish.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      kGracePeriodToFinishLoading + base::Seconds(1));
  run_loop.Run();

  // Ensure that the page is still in bfcache.
  EXPECT_FALSE(delete_observer.deleted());
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  // 3) Go back to the first page. We should restore the page from the
  // back-forward cache.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectRestored(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       ImageStillLoading_ResponseStartedBeforeFreezing) {
  net::test_server::ControllableHttpResponse image_response(
      embedded_test_server(), "/image.png");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with an image with src == "image.png".
  RenderFrameHostImpl* rfh_1 = NavigateToPageWithImage(
      embedded_test_server()->GetURL("a.com", "/title1.html"));

  // Start sending response before the page gets in the back-forward cache.
  image_response.WaitForRequest();
  image_response.Send(net::HTTP_OK, "image/png");
  image_response.Send(" ");
  // Run some script to ensure the renderer processed its pending tasks.
  EXPECT_TRUE(ExecJs(rfh_1, "var foo = 42;"));

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title2.html")));
  // The page was still loading when we navigated away, but it's still eligible
  // for back-forward cache.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  // Send body while in the back-forward cache.
  image_response.Send("image_body");
  image_response.Done();

  // 3) Go back to the first page. We should restore the page from the
  // back-forward cache.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectRestored(FROM_HERE);

  // Wait until the deferred body is processed. Since it's not a valid image
  // value, we'll get the "error" event.
  EXPECT_EQ("error", EvalJs(rfh_1, "image_load_status"));
}

}  // namespace content
