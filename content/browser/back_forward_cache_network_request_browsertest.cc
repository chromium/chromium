// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "content/browser/back_forward_cache_browsertest.h"

#include "base/task/single_thread_task_runner.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "third_party/blink/public/common/features.h"

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
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a.get());

  // Use "fetch" immediately before being frozen.
  EXPECT_TRUE(ExecJs(rfh_a.get(), R"(
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
// TODO(crbug.com/40937269): Disabled due to flakiness.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DISABLED_FetchRedirectedWhileStoring) {
  net::test_server::ControllableHttpResponse fetch_response(
      embedded_test_server(), "/fetch");
  net::test_server::ControllableHttpResponse fetch2_response(
      embedded_test_server(), "/fetch2");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // Trigger a fetch.
  ExecuteScriptAsync(rfh_a.get(), "my_fetch = fetch('/fetch');");

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
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // 3) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kNetworkRequestRedirected}, {}, {}, {},
                    {}, FROM_HERE);
}

// Eviction is triggered when a keepalive fetch request gets redirected while
// the page is in back-forward cache.
// TODO(crbug.com/40724916): We should not trigger eviction on redirects
// of keepalive fetches.
// TODO(crbug.com/40874525): Disabled for flakiness.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DISABLED_KeepAliveFetchRedirectedWhileStoring) {
  net::test_server::ControllableHttpResponse fetch_response(
      embedded_test_server(), "/fetch");
  net::test_server::ControllableHttpResponse fetch2_response(
      embedded_test_server(), "/fetch2");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a.get());

  // Trigger a keepalive fetch.
  ExecuteScriptAsync(rfh_a.get(),
                     "my_fetch = fetch('/fetch', { keepalive: true });");

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
  // TODO(crbug.com/40724916): We should not trigger eviction on
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

class BackForwardCacheDrainedAsBytesConsumerTest
    : public BackForwardCacheBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  static constexpr int kMaxBufferedBytesPerProcess = 10000;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableFeatureAndSetParams(
        blink::features::kAllowDatapipeDrainedAsBytesConsumerInBFCache, "", "");
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kLoadingTasksUnfreezable,
          {{"max_buffered_bytes_per_process",
            base::NumberToString(kMaxBufferedBytesPerProcess)}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests the case when the header was received before the page is frozen,
// but parts of the response body is received when the page is frozen.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheDrainedAsBytesConsumerTest,
    PageWithDrainedDatapipeRequestsForFetchShouldBeEvictedOrNot) {
  net::test_server::ControllableHttpResponse fetch_response(
      embedded_test_server(), "/fetch");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  // Call fetch before navigating away.
  EXPECT_TRUE(ExecJs(current_frame_host(), R"(
    var fetch_response_promise = my_fetch = fetch('/fetch').then(response => {
        return response.text();
    });
  )"));
  // Send response header and a piece of the body before navigating away.
  fetch_response.WaitForRequest();
  fetch_response.Send(net::HTTP_OK, "text/plain");
  fetch_response.Send("hello");

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  fetch_response.Send("world");
  fetch_response.Done();

  // 3) Go back to A.
  // Note that we cannot reliably wait for the datapipe to be drained as bytes
  // consumer, so sometimes this is not testing the case in question, but the
  // page will be restored in either way, and the test will not be flaky.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectRestored(FROM_HERE);
  // Ensure that the fetch response is complete, having both of the parts from
  // before entering back/forward cache and after.
  EXPECT_EQ("helloworld",
            content::EvalJs(current_frame_host(), "fetch_response_promise"));
}

// If too much data is processed while in bfcache, evict the entry.
// TODO(crbug.com/325558875): Flaky on Mac and ChromeOS bots.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_PageWithDrainedDatapipeAsBytesConsumerCannotProcessTooMuchData \
  DISABLED_PageWithDrainedDatapipeAsBytesConsumerCannotProcessTooMuchData
#else
#define MAYBE_PageWithDrainedDatapipeAsBytesConsumerCannotProcessTooMuchData \
  PageWithDrainedDatapipeAsBytesConsumerCannotProcessTooMuchData
#endif
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheDrainedAsBytesConsumerTest,
    MAYBE_PageWithDrainedDatapipeAsBytesConsumerCannotProcessTooMuchData) {
  net::test_server::ControllableHttpResponse fetch_response(
      embedded_test_server(), "/fetch");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // Call fetch before navigating away.
  EXPECT_TRUE(ExecJs(rfh_a.get(), R"(
    var fetch_response_promise = my_fetch = fetch('/fetch').then(response => {
        return response.text();
    });
  )"));
  // Send response header and a piece of the body before navigating away.
  fetch_response.WaitForRequest();
  fetch_response.Send(net::HTTP_OK, "text/plain");
  fetch_response.Send("start sending body");

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  ASSERT_TRUE(rfh_a->IsInBackForwardCache());

  // Complete the response after navigating away.
  // Data over the limit is processed, so the bfcache entry should be evicted.
  std::string body(kMaxBufferedBytesPerProcess * 10, '*');
  fetch_response.Send(body);
  fetch_response.Done();
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // 3) Go back to A.
  // Note that we cannot reliably wait for the datapipe to be drained as bytes
  // consumer, so sometimes this is not testing the case in question.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kNetworkExceedsBufferLimit}, {}, {}, {},
                    {}, FROM_HERE);
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

enum class BackgroundResourceFetchTestCase {
  kBackgroundResourceFetchEnabled,
  kBackgroundResourceFetchDisabled,
};

class BackForwardCacheNetworkLimitBrowserTest
    : public BackForwardCacheBrowserTest,
      public testing::WithParamInterface<BackgroundResourceFetchTestCase> {
 public:
  const int kMaxBufferedBytesPerProcess = 10000;
  const base::TimeDelta kGracePeriodToFinishLoading = base::Seconds(5);

  void SetUpCommandLine(base::CommandLine* command_line) override {
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kLoadingTasksUnfreezable,
          {{"max_buffered_bytes_per_process",
            base::NumberToString(kMaxBufferedBytesPerProcess)},
           {"grace_period_to_finish_loading_in_seconds",
            base::NumberToString(kGracePeriodToFinishLoading.InSeconds())}}}},
        {});
    if (IsBackgroundResourceFetchEnabled()) {
      feature_background_resource_fetch_.InitAndEnableFeature(
          blink::features::kBackgroundResourceFetch);
    }
  }

 private:
  bool IsBackgroundResourceFetchEnabled() const {
    return GetParam() ==
           BackgroundResourceFetchTestCase::kBackgroundResourceFetchEnabled;
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedFeatureList feature_background_resource_fetch_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    BackForwardCacheNetworkLimitBrowserTest,
    testing::ValuesIn(
        {BackgroundResourceFetchTestCase::kBackgroundResourceFetchEnabled,
         BackgroundResourceFetchTestCase::kBackgroundResourceFetchDisabled}),
    [](const testing::TestParamInfo<BackgroundResourceFetchTestCase>& info) {
      switch (info.param) {
        case (BackgroundResourceFetchTestCase::kBackgroundResourceFetchEnabled):
          return "BackgroundResourceFetchEnabled";
        case (
            BackgroundResourceFetchTestCase::kBackgroundResourceFetchDisabled):
          return "BackgroundResourceFetchDisabled";
      }
    });

IN_PROC_BROWSER_TEST_P(
    BackForwardCacheNetworkLimitBrowserTest,
    PageWithDrainedDatapipeRequestsForScriptStreamerShouldBeEvictedIfStreamedTooMuch) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/small_script.js");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/empty.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/empty.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_1(current_frame_host());

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
  // Page A is now in BFCache.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  // Complete the response after navigating away.
  std::string body(kMaxBufferedBytesPerProcess + 1, '*');
  response.Send(body);
  response.Done();
  // Page A should be evicted from BFCache, we wait for the deletion to
  // complete.
  ASSERT_TRUE(rfh_1.WaitUntilRenderFrameDeleted());

  // 3) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kNetworkExceedsBufferLimit}, {}, {}, {},
                    {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_P(BackForwardCacheNetworkLimitBrowserTest,
                       ImageStillLoading_ResponseStartedWhileFrozen) {
  net::test_server::ControllableHttpResponse image_response(
      embedded_test_server(), "/image.png");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with an image with src == "image.png".
  RenderFrameHostImplWrapper rfh_1(NavigateToPageWithImage(
      embedded_test_server()->GetURL("a.com", "/title1.html")));
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
  EXPECT_EQ("error", EvalJs(rfh_1.get(), "image_load_status"));
}

IN_PROC_BROWSER_TEST_P(
    BackForwardCacheNetworkLimitBrowserTest,
    ImageStillLoading_ResponseStartedWhileRestoring_DoNotTriggerEviction) {
  net::test_server::ControllableHttpResponse image_response(
      embedded_test_server(), "/image.png");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with an image with src == "image.png".
  GURL url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  RenderFrameHostImplWrapper rfh_1(NavigateToPageWithImage(url));

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

#if BUILDFLAG(IS_MAC)
#define MAYBE_ImageStillLoading_ResponseStartedWhileFrozen_ExceedsPerProcessBytesLimit \
  DISABLED_ImageStillLoading_ResponseStartedWhileFrozen_ExceedsPerProcessBytesLimit
#else
#define MAYBE_ImageStillLoading_ResponseStartedWhileFrozen_ExceedsPerProcessBytesLimit \
  ImageStillLoading_ResponseStartedWhileFrozen_ExceedsPerProcessBytesLimit
#endif
IN_PROC_BROWSER_TEST_P(
    BackForwardCacheNetworkLimitBrowserTest,
    MAYBE_ImageStillLoading_ResponseStartedWhileFrozen_ExceedsPerProcessBytesLimit) {
  net::test_server::ControllableHttpResponse image1_response(
      embedded_test_server(), "/image1.png");
  net::test_server::ControllableHttpResponse image2_response(
      embedded_test_server(), "/image2.png");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with 2 images.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  RenderFrameHostImplWrapper rfh_1(current_frame_host());
  // Wait for the document to load DOM to ensure that kLoading is not
  // one of the reasons why the document wasn't cached.
  ASSERT_TRUE(WaitForDOMContentLoaded(rfh_1.get()));

  EXPECT_TRUE(ExecJs(rfh_1.get(), R"(
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
  ASSERT_TRUE(rfh_1.WaitUntilRenderFrameDeleted());

  // 3) Go back to the first page. We should not restore the page from the
  // back-forward cache.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kNetworkExceedsBufferLimit}, {}, {}, {},
                    {}, FROM_HERE);
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_ImageStillLoading_ResponseStartedWhileFrozen_ExceedsPerProcessBytesLimit_SameSiteSubframe \
  DISABLED_ImageStillLoading_ResponseStartedWhileFrozen_ExceedsPerProcessBytesLimit_SameSiteSubframe
#else
#define MAYBE_ImageStillLoading_ResponseStartedWhileFrozen_ExceedsPerProcessBytesLimit_SameSiteSubframe \
  ImageStillLoading_ResponseStartedWhileFrozen_ExceedsPerProcessBytesLimit_SameSiteSubframe
#endif
IN_PROC_BROWSER_TEST_P(
    BackForwardCacheNetworkLimitBrowserTest,
    MAYBE_ImageStillLoading_ResponseStartedWhileFrozen_ExceedsPerProcessBytesLimit_SameSiteSubframe) {
  net::test_server::ControllableHttpResponse image1_response(
      embedded_test_server(), "/image1.png");
  net::test_server::ControllableHttpResponse image2_response(
      embedded_test_server(), "/image2.png");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate main frame to a page with 1 image.
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         "a.com", "/page_with_iframe.html")));
  RenderFrameHostImplWrapper main_rfh(current_frame_host());
  // Wait for the document to load DOM to ensure that kLoading is not
  // one of the reasons why the document wasn't cached.
  ASSERT_TRUE(WaitForDOMContentLoaded(main_rfh.get()));

  EXPECT_TRUE(ExecJs(main_rfh.get(), R"(
      var image1 = document.createElement("img");
      image1.src = "image1.png";
      document.body.appendChild(image1);
      var image1_load_status = new Promise((resolve, reject) => {
        image1.onload = () => { resolve("loaded"); }
        image1.onerror = () => { resolve("error"); }
      });
    )"));

  // 2) Add 1 image to the subframe.
  RenderFrameHostImplWrapper subframe_rfh(
      main_rfh->child_at(0)->current_frame_host());

  // First, wait for the subframe document to load DOM to ensure that kLoading
  // is not one of the reasons why the document wasn't cached.
  EXPECT_TRUE(WaitForDOMContentLoaded(subframe_rfh.get()));

  EXPECT_TRUE(ExecJs(subframe_rfh.get(), R"(
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
  ASSERT_TRUE(main_rfh.WaitUntilRenderFrameDeleted());
  ASSERT_TRUE(subframe_rfh.WaitUntilRenderFrameDeleted());

  // 3) Go back to the first page. We should not restore the page from the
  // back-forward cache.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kNetworkExceedsBufferLimit}, {}, {}, {},
                    {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_P(
    BackForwardCacheNetworkLimitBrowserTest,
    ImageStillLoading_ResponseStartedWhileFrozen_ExceedsPerProcessBytesLimit_ResetOnRestore) {
  net::test_server::ControllableHttpResponse image1_response(
      embedded_test_server(), "/image.png");
  net::test_server::ControllableHttpResponse image2_response(
      embedded_test_server(), "/image2.png");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with an image with src == "image.png".
  RenderFrameHostImplWrapper rfh_1(NavigateToPageWithImage(
      embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Wait for the image request, but don't send anything yet.
  image1_response.WaitForRequest();

  // 2) Navigate away on the main frame.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title2.html")));
  RenderFrameHostImplWrapper rfh_2(current_frame_host());
  ASSERT_TRUE(WaitForDOMContentLoaded(rfh_2.get()));

  // The first page was still loading images when we navigated away, but it's
  // still eligible for back-forward cache.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  // 3) Add 1 image to the second page.
  EXPECT_TRUE(ExecJs(rfh_2.get(), R"(
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

IN_PROC_BROWSER_TEST_P(
    BackForwardCacheNetworkLimitBrowserTest,
    ImageStillLoading_ResponseStartedWhileFrozen_ExceedsPerProcessBytesLimit_ResetOnDetach) {
  net::test_server::ControllableHttpResponse image1_response(
      embedded_test_server(), "/image.png");
  net::test_server::ControllableHttpResponse image2_response(
      embedded_test_server(), "/image2.png");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with an image with src == "image.png".
  RenderFrameHostImplWrapper rfh_1(NavigateToPageWithImage(
      embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Wait for the image request, but don't send anything yet.
  image1_response.WaitForRequest();

  // 2) Navigate away on the main frame.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title2.html")));
  RenderFrameHostImplWrapper rfh_2(current_frame_host());
  ASSERT_TRUE(WaitForDOMContentLoaded(rfh_2.get()));

  // The first page was still loading images when we navigated away, but it's
  // still eligible for back-forward cache.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  // 3) Add 1 image to the second page.
  EXPECT_TRUE(ExecJs(rfh_2.get(), R"(
      var image2 = document.createElement("img");
      image2.src = "image2.png";
      document.body.appendChild(image2);
      var image2_load_status = new Promise((resolve, reject) => {
        image2.onload = () => { resolve("loaded"); }
        image2.onerror = () => { resolve("error"); }
      });
    )"));
  image2_response.WaitForRequest();

  // Start sending an image response that's larger than the per-process and
  // per-request buffer limit, causing the page to get evicted from the
  // back-forward cache.
  std::string body(kMaxBufferedBytesPerProcess + 1, '*');
  image1_response.Send(net::HTTP_OK, "image/png");
  image1_response.Send(body);
  image1_response.Done();
  ASSERT_TRUE(rfh_1.WaitUntilRenderFrameDeleted());

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
  EXPECT_EQ("error", EvalJs(rfh_2.get(), "image2_load_status"));
}

IN_PROC_BROWSER_TEST_P(BackForwardCacheNetworkLimitBrowserTest,
                       ImageStillLoading_ResponseStartedWhileFrozen_Timeout) {
  net::test_server::ControllableHttpResponse image_response(
      embedded_test_server(), "/image.png");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with an image with src == "image.png".
  RenderFrameHostImplWrapper rfh_1(NavigateToPageWithImage(
      embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Wait for the image request, but don't send anything yet.
  image_response.WaitForRequest();

  // 2) Navigate away.
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title2.html")));
  // The page was still loading when we navigated away, but it's still eligible
  // for back-forward cache.
  ASSERT_TRUE(rfh_1->IsInBackForwardCache());

  // Start sending the image response while in the back-forward cache, but never
  // finish the request. Eventually the page will get deleted due to network
  // request timeout.
  image_response.Send(net::HTTP_OK, "image/png");
  ASSERT_TRUE(rfh_1.WaitUntilRenderFrameDeleted());

  // 3) Go back to the first page. We should not restore the page from the
  // back-forward cache.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kNetworkRequestTimeout}, {}, {}, {}, {},
                    FROM_HERE);
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_ImageStillLoading_ResponseStartedBeforeFreezing_ExceedsPerProcessBytesLimit \
  DISABLED_ImageStillLoading_ResponseStartedBeforeFreezing_ExceedsPerProcessBytesLimit
#else
#define MAYBE_ImageStillLoading_ResponseStartedBeforeFreezing_ExceedsPerProcessBytesLimit \
  ImageStillLoading_ResponseStartedBeforeFreezing_ExceedsPerProcessBytesLimit
#endif
IN_PROC_BROWSER_TEST_P(
    BackForwardCacheNetworkLimitBrowserTest,
    MAYBE_ImageStillLoading_ResponseStartedBeforeFreezing_ExceedsPerProcessBytesLimit) {
  net::test_server::ControllableHttpResponse image1_response(
      embedded_test_server(), "/image1.png");
  net::test_server::ControllableHttpResponse image2_response(
      embedded_test_server(), "/image2.png");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page with 2 images.
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  RenderFrameHostImplWrapper rfh_1(current_frame_host());
  // Wait for the document to load DOM to ensure that kLoading is not
  // one of the reasons why the document wasn't cached.
  ASSERT_TRUE(WaitForDOMContentLoaded(rfh_1.get()));

  ASSERT_TRUE(ExecJs(rfh_1.get(), R"(
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
  ASSERT_TRUE(ExecJs(rfh_1.get(), "var foo = 42;"));

  // 2) Navigate away.
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title2.html")));
  // The page was still loading when we navigated away, but it's still eligible
  // for back-forward cache.
  ASSERT_TRUE(rfh_1.get()->IsInBackForwardCache());

  // Send the image response body while in the back-forward cache. The body size
  // of the responses individually is less than the per-process limit, but
  // together they surpass the per-process limit.
  const int image_body_size = kMaxBufferedBytesPerProcess / 2 + 1;
  std::string body(image_body_size, '*');
  image1_response.Send(body);
  image1_response.Done();
  image2_response.Send(body);
  image2_response.Done();
  ASSERT_TRUE(rfh_1.WaitUntilRenderFrameDeleted());

  // 3) Go back to the first page. We should not restore the page from the
  // back-forward cache.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kNetworkExceedsBufferLimit}, {}, {}, {},
                    {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_P(BackForwardCacheNetworkLimitBrowserTest,
                       TimeoutNotTriggeredAfterDone) {
  net::test_server::ControllableHttpResponse image_response(
      embedded_test_server(), "/image.png");
  ASSERT_TRUE(embedded_test_server()->Start());
  // 1) Navigate to a page with an image with src == "image.png".
  RenderFrameHostImplWrapper rfh_1(NavigateToPageWithImage(
      embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Wait for the image request, but don't send anything yet.
  image_response.WaitForRequest();

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title2.html")));
  // The page was still loading when we navigated away, but it's still eligible
  // for back-forward cache.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  // Start sending the image response while in the back-forward cache and finish
  // the request before the active request timeout hits.
  image_response.Send(net::HTTP_OK, "image/png");
  image_response.Send(" ");
  image_response.Done();

  // Make sure enough time passed to trigger network request eviction if the
  // load above didn't finish.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      kGracePeriodToFinishLoading + base::Seconds(1));
  run_loop.Run();

  // Ensure that the page is still in bfcache.
  EXPECT_FALSE(rfh_1.IsDestroyed());
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());

  // 3) Go back to the first page. We should restore the page from the
  // back-forward cache.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectRestored(FROM_HERE);
}

IN_PROC_BROWSER_TEST_P(
    BackForwardCacheNetworkLimitBrowserTest,
    TimeoutNotTriggeredAfterDone_ResponseStartedBeforeFreezing) {
  net::test_server::ControllableHttpResponse image_response(
      embedded_test_server(), "/image.png");
  ASSERT_TRUE(embedded_test_server()->Start());
  // 1) Navigate to a page with an image with src == "image.png".
  RenderFrameHostImplWrapper rfh_1(NavigateToPageWithImage(
      embedded_test_server()->GetURL("a.com", "/title1.html")));

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

  // Finish the request before the active request timeout hits.
  image_response.Done();

  // Make sure enough time passed to trigger network request eviction if the
  // load above didn't finish.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      kGracePeriodToFinishLoading + base::Seconds(1));
  run_loop.Run();

  // Ensure that the page is still in bfcache.
  EXPECT_FALSE(rfh_1.IsDestroyed());
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
  RenderFrameHostImplWrapper rfh_1(NavigateToPageWithImage(
      embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Start sending response before the page gets in the back-forward cache.
  image_response.WaitForRequest();
  image_response.Send(net::HTTP_OK, "image/png");
  image_response.Send(" ");
  // Run some script to ensure the renderer processed its pending tasks.
  EXPECT_TRUE(ExecJs(rfh_1.get(), "var foo = 42;"));

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
  EXPECT_EQ("error", EvalJs(rfh_1.get(), "image_load_status"));
}

class BackForwardCacheBrowserTestWithDisallowJavaScriptExecution
    : public BackForwardCacheBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
    feature_list_.InitAndEnableFeature(
        blink::features::kBackForwardCacheDWCOnJavaScriptExecution);
    DCHECK(base::FeatureList::IsEnabled(
        blink::features::kBackForwardCacheDWCOnJavaScriptExecution));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestWithDisallowJavaScriptExecution,
    EvictWillNotTriggerReadystatechange) {
  net::test_server::ControllableHttpResponse image_response(
      embedded_test_server(), "/back_forward_cache/image.png");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/back_forward_cache/page_with_non_existing_image.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  shell()->LoadURL(url_a);
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  // Start sending response before the page gets in the back-forward cache, so
  // that the readystate of the document is interactive instead of complete.
  image_response.WaitForRequest();
  image_response.Send(net::HTTP_OK, "image/png");
  image_response.Send(" ");
  ASSERT_TRUE(WaitForDOMContentLoaded(rfh_a.get()));
  // Add event listener and make sure that the readystate is set to interactive.
  ASSERT_EQ("interactive", EvalJs(rfh_a.get(), "interactivePromise"));

  // 2) Navigate to B. Use |LoadURL()| and |TestNavigationManager| instead of
  // |NavigateToURL()| because the first navigation to a.com has not been
  // complete yet because of in-flight image request.
  TestNavigationManager nav_manager(web_contents(), url_b);
  shell()->LoadURL(url_b);
  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Evict entry A. This will change the readystate to complete as part of
  // document detach, but the readystatechange event is queued instead of being
  // fired synchronously.
  DisableBFCacheForRFHForTesting(rfh_a->GetGlobalId());
  EXPECT_TRUE(rfh_a->is_evicted_from_back_forward_cache());

  // 4.) Go back. Expect that readystatechange event has not been fired, and
  // DumpWithoutCrashing is not hit.
  TestNavigationManager nav_manager_2(web_contents(), url_a);
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(nav_manager_2.WaitForNavigationFinished());
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kDisableForRenderFrameHostCalled},
                    {}, {}, {RenderFrameHostDisabledForTestingReason()}, {},
                    FROM_HERE);
}

class BackForwardCacheWithKeepaliveSupportBrowserTest
    : public BackForwardCacheBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableFeatureAndSetParams(
        blink::features::kBackForwardCacheWithKeepaliveRequest, "", "");

    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

// With the feature, keepalive doesn't prevent the page from entering into the
// bfcache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheWithKeepaliveSupportBrowserTest,
                       KeepAliveFetch) {
  net::test_server::ControllableHttpResponse fetch_response(
      embedded_test_server(), "/fetch");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // Trigger a keepalive fetch.
  ExecuteScriptAsync(rfh_a.get(),
                     "my_fetch = fetch('/fetch', { keepalive: true });");

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Respond the fetch with a redirect.
  fetch_response.WaitForRequest();

  // 3) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectRestored(FROM_HERE);
}

}  // namespace content
