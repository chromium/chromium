// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "content/browser/back_forward_cache_browsertest.h"
#include "content/browser/back_forward_cache_test_util.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/back_forward_cache_not_restored_reasons.mojom-blink.h"

namespace content {
using NotRestoredReason = BackForwardCacheMetrics::NotRestoredReason;
using NotRestoredReasons =
    BackForwardCacheCanStoreDocumentResult::NotRestoredReasons;

// Exists to group the tests and for test history.
class BackForwardCacheBrowserTestWithNotRestoredReasons
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableFeatureAndSetParams(kAllowCrossOriginNotRestoredReasons, "", "");
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

// NotRestoredReasons are not reported when the page is successfully restored
// from back/forward cache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithNotRestoredReasons,
                       NotReportedWhenRestored) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  // 1) Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // 3) Navigate back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectRestored(FROM_HERE);
  // Expect that NotRestoredReasons are not reported at all.
  EXPECT_TRUE(current_frame_host()->NotRestoredReasonsForTesting().is_null());
}

// NotRestoredReasons are reset after each navigation.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithNotRestoredReasons,
                       ReasonsResetForEachNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  // 1) Navigate to A and use dummy blocking feature.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  rfh_a->UseDummyStickyBackForwardCacheDisablingFeatureForTesting();
  GURL rfh_a_url = rfh_a->GetLastCommittedURL();

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImplWrapper rfh_b(current_frame_host());

  // 3) Navigate back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kBlocklistedFeatures},
                    {blink::scheduler::WebSchedulerTrackedFeature::kDummy}, {},
                    {}, {}, FROM_HERE);
  // Expect that NotRestoredReasons are reported.
  auto rfh_a_result = MatchesNotRestoredReasons(
      /*id=*/std::nullopt,
      /*name=*/std::nullopt, /*src=*/std::nullopt, /*reasons=*/
      {MatchesDetailedReason("Dummy", /*source=*/std::nullopt)},
      MatchesSameOriginDetails(
          /*url=*/rfh_a_url, /*children=*/{}));
  EXPECT_THAT(current_frame_host()->NotRestoredReasonsForTesting(),
              rfh_a_result);
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());

  // 4) Navigate forward.
  ASSERT_TRUE(HistoryGoForward(web_contents()));
  ExpectRestored(FROM_HERE);
  // Expect that NotRestoredReasons are not reported at all.
  EXPECT_TRUE(current_frame_host()->NotRestoredReasonsForTesting().is_null());
}

// Frame attributes are reported for all the frames that are reachable from
// same-origin documents. Also test that the details for cross-origin subtree
// are masked.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithNotRestoredReasons,
                       FrameAttributesAreReportedIfSameOrigin) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,b(a))"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));
  // 1) Navigate to A(A,B(A)).
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  // rfh_a_1(rfh_a_2,rfh_b(rfh_a_3))
  RenderFrameHostImplWrapper rfh_a_1(current_frame_host());
  RenderFrameHostImplWrapper rfh_a_2(
      rfh_a_1->child_at(0)->current_frame_host());
  RenderFrameHostImplWrapper rfh_b(rfh_a_1->child_at(1)->current_frame_host());
  RenderFrameHostImplWrapper rfh_a_3(rfh_b->child_at(0)->current_frame_host());
  GURL rfh_a_1_url = rfh_a_1->GetLastCommittedURL();
  GURL rfh_a_2_url = rfh_a_2->GetLastCommittedURL();
  GURL rfh_b_url = rfh_b->GetLastCommittedURL();

  rfh_a_3->UseDummyStickyBackForwardCacheDisablingFeatureForTesting();

  // cross_site_iframe_factory.html gives frames ids but they are not globally
  // unique, so replace them with unique ids so that there will be no
  // duplicates.
  EXPECT_TRUE(ExecJs(rfh_a_1.get(), R"(
    let frames = document.getElementsByTagName('iframe');
    frames[0].id = 'rfh_a_2_id';
    frames[0].name = 'rfh_a_2_name';
    frames[1].id = 'rfh_b_id';
    frames[1].name = 'rfh_b_name';
  )"));
  // 2) Navigate to C.
  ASSERT_TRUE(NavigateToURL(shell(), url_c));

  // 3) Navigate back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kBlocklistedFeatures},
                    {blink::scheduler::WebSchedulerTrackedFeature::kDummy}, {},
                    {}, {}, FROM_HERE);

  // Expect that id and name are reported for both |rfh_b| and |rfh_a_2|.
  // Note that |rfh_a_3| is masked because it's a child of |rfh_b|.
  auto rfh_b_result = MatchesNotRestoredReasons(
      /*id=*/"rfh_b_id", /*name=*/"rfh_b_name",
      /*src=*/rfh_b_url.spec(), /*reasons=*/
      {MatchesDetailedReason("masked", /*source=*/std::nullopt)},
      /*same_origin_details=*/std::nullopt);

  auto rfh_a_2_result = MatchesNotRestoredReasons(
      /*id=*/"rfh_a_2_id", /*name=*/"rfh_a_2_name",
      /*src=*/rfh_a_2_url.spec(), /*reasons=*/{},
      MatchesSameOriginDetails(
          /*url=*/rfh_a_2_url, /*children=*/{}));
  auto rfh_a_1_result = MatchesNotRestoredReasons(
      /*id=*/std::nullopt,
      /*name=*/std::nullopt, /*src=*/std::nullopt,
      /*reasons=*/{},
      MatchesSameOriginDetails(
          /*url=*/rfh_a_1_url,
          /*children=*/{rfh_a_2_result, rfh_b_result}));

  EXPECT_THAT(current_frame_host()->NotRestoredReasonsForTesting(),
              rfh_a_1_result);
}

// All the blocking reasons should be reported including subframes'.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithNotRestoredReasons,
                       AllBlockingFramesAreReported) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,a(a))"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  // 1) Navigate to A(A,A(A)) and use dummy blocking feature in the main frame
  // and subframes.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  // rfh_a_1(rfh_a_2, rfh_a_3(rfh_a_4))
  RenderFrameHostImplWrapper rfh_a_1(current_frame_host());
  RenderFrameHostImplWrapper rfh_a_2(
      rfh_a_1->child_at(0)->current_frame_host());
  RenderFrameHostImplWrapper rfh_a_3(
      rfh_a_1->child_at(1)->current_frame_host());
  RenderFrameHostImplWrapper rfh_a_4(
      rfh_a_3->child_at(0)->current_frame_host());
  GURL rfh_a_1_url = rfh_a_1->GetLastCommittedURL();
  GURL rfh_a_2_url = rfh_a_2->GetLastCommittedURL();
  GURL rfh_a_3_url = rfh_a_3->GetLastCommittedURL();
  GURL rfh_a_4_url = rfh_a_4->GetLastCommittedURL();

  rfh_a_1->UseDummyStickyBackForwardCacheDisablingFeatureForTesting();
  rfh_a_2->UseDummyStickyBackForwardCacheDisablingFeatureForTesting();
  rfh_a_4->UseDummyStickyBackForwardCacheDisablingFeatureForTesting();

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // 3) Navigate back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kBlocklistedFeatures},
                    {blink::scheduler::WebSchedulerTrackedFeature::kDummy}, {},
                    {}, {}, FROM_HERE);
  // Frames generated by cross_site_iframe_factory.html have empty names instead
  // of null.
  EXPECT_EQ(true, EvalJs(current_frame_host(),
                         "document.getElementById('child-0').name == ''"));
  auto rfh_a_2_result = MatchesNotRestoredReasons(
      /*id=*/"child-0", /*name=*/std::nullopt,
      /*src=*/rfh_a_2_url.spec(),
      /*reasons=*/
      {MatchesDetailedReason("Dummy", /*source=*/std::nullopt)},
      MatchesSameOriginDetails(
          /*url=*/rfh_a_2_url,
          /*children=*/{}));
  auto rfh_a_4_result = MatchesNotRestoredReasons(
      /*id=*/"child-0", /*name=*/std::nullopt,
      /*src=*/rfh_a_4_url.spec(),
      /*reasons=*/
      {MatchesDetailedReason("Dummy", /*source=*/std::nullopt)},
      MatchesSameOriginDetails(
          /*url=*/rfh_a_4_url,
          /*children=*/{}));
  EXPECT_EQ(true, EvalJs(current_frame_host(),
                         "document.getElementById('child-1').name == ''"));
  auto rfh_a_3_result = MatchesNotRestoredReasons(
      /*id=*/"child-1",
      /*name=*/std::nullopt,
      /*src=*/rfh_a_3_url.spec(),
      /*reasons=*/{},
      MatchesSameOriginDetails(
          /*url=*/rfh_a_3_url,
          /*children=*/{rfh_a_4_result}));
  auto rfh_a_1_result = MatchesNotRestoredReasons(
      /*id=*/std::nullopt,
      /*name=*/std::nullopt, /*src=*/std::nullopt,
      /*reasons=*/
      {MatchesDetailedReason("Dummy", /*source=*/std::nullopt)},
      MatchesSameOriginDetails(
          /*url=*/rfh_a_1_url,
          /*children=*/{rfh_a_2_result, rfh_a_3_result}));
  EXPECT_THAT(current_frame_host()->NotRestoredReasonsForTesting(),
              rfh_a_1_result);
}

// NotRestoredReasons are not reported for same document navigation.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithNotRestoredReasons,
                       NotReportedForSameDocumentNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a_1(embedded_test_server()->GetURL(
      "a.com", "/accessibility/html/a-name.html"));
  GURL url_a_2(embedded_test_server()->GetURL(
      "a.com", "/accessibility/html/a-name.html#id"));
  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a_1));
  // 2) Do a same-document navigation.
  EXPECT_TRUE(NavigateToURL(shell(), url_a_2));
  // 3) Navigate back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectOutcomeDidNotChange(FROM_HERE);
  // Expect that NotRestoredReasons are not reported at all.
  EXPECT_TRUE(current_frame_host()->NotRestoredReasonsForTesting().is_null());
}

// NotRestoredReasons are not reported for subframe navigation.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithNotRestoredReasons,
                       SubframeNavigationDoesNotRecordMetrics) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1) Navigate to A(B).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // 2) Navigate from B to C on the subframe.
  EXPECT_TRUE(NavigateFrameToURL(rfh_a->child_at(0), url_c));
  EXPECT_EQ(url_c,
            rfh_a->child_at(0)->current_frame_host()->GetLastCommittedURL());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());

  // 4) Go back from C to B on the subframe.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_TRUE(
      rfh_a->child_at(0)->current_frame_host()->GetLastCommittedURL().DomainIs(
          "b.com"));
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  ExpectOutcomeDidNotChange(FROM_HERE);
  // NotRestoredReasons are not recorded.
  EXPECT_TRUE(current_frame_host()->NotRestoredReasonsForTesting().is_null());
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithNotRestoredReasons,
                       WindowOpen) {
  if (!SiteIsolationPolicy::UseDedicatedProcessesForAllSites())
    return;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A and open a popup.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  EXPECT_EQ(1u, rfh_a->GetSiteInstance()->GetRelatedActiveContentsCount());
  OpenPopup(rfh_a.get(), url_a, "");
  EXPECT_EQ(2u, rfh_a->GetSiteInstance()->GetRelatedActiveContentsCount());
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);
  GURL rfh_a_url = rfh_a->GetLastCommittedURL();

  // 2) Navigate to B. The previous document can't enter the BackForwardCache,
  // because of the popup.
  ASSERT_TRUE(NavigateToURLFromRenderer(rfh_a.get(), url_b));
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());
  RenderFrameHostImplWrapper rfh_b(current_frame_host());
  EXPECT_EQ(2u, rfh_b->GetSiteInstance()->GetRelatedActiveContentsCount());

  // 3) Go back to A. The previous document can't enter the BackForwardCache,
  // because of the popup.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  ExpectNotRestored({NotRestoredReason::kRelatedActiveContentsExist,
                     NotRestoredReason::kBrowsingInstanceNotSwapped},
                    {},
                    {ShouldSwapBrowsingInstance::kNo_HasRelatedActiveContents},
                    {}, {}, FROM_HERE);
  // Make sure that the tree result also has the same reasons. BrowsingInstance
  // NotSwapped can only be known at commit time.
  EXPECT_THAT(
      GetTreeResult()->GetDocumentResult(),
      MatchesDocumentResult(
          NotRestoredReasons({NotRestoredReason::kRelatedActiveContentsExist,
                              NotRestoredReason::kBrowsingInstanceNotSwapped}),
          BlockListedFeatures()));

  // Both reasons are recorded and sent to the renderer.
  // TODO(crbug.com/40275090): BrowsingInstanceNotSwapped should not be reported
  // as internal-error.
  auto rfh_a_result = MatchesNotRestoredReasons(
      /*id=*/std::nullopt,
      /*name=*/std::nullopt, /*src=*/std::nullopt,
      /*reasons=*/
      {MatchesDetailedReason("non-trivial-browsing-context-group",
                             /*source=*/std::nullopt),
       MatchesDetailedReason("masked", /*source=*/std::nullopt)},
      MatchesSameOriginDetails(
          /*url=*/rfh_a_url,
          /*children=*/{}));
  EXPECT_THAT(current_frame_host()->NotRestoredReasonsForTesting(),
              rfh_a_result);
}

// Test when a server redirect happens on history navigation, causing a
// SiteInstance change and a new navigation entry. Ensure that the reasons from
// the old entry are copied to the new one and reported internally, but not to
// the API.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithNotRestoredReasons,
                       ServerRedirect) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // Navigate to a.com. This time the redirect does not happen.
  ASSERT_TRUE(NavigateToURL(web_contents(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  EXPECT_EQ(url_a, rfh_a->GetLastCommittedURL());
  // Replace the history URL to a URL that would redirect to b.com when
  // navigated to.
  std::string replace_state =
      "window.history.replaceState(null, '', '/server-redirect?" +
      url_b.spec() + "');";
  EXPECT_TRUE(ExecJs(rfh_a.get(), replace_state));

  // Navigate to c.com, and evict |rfh_a| by executing JavaScript.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  EvictByJavaScript(rfh_a.get());

  // Navigate back.
  GURL url_a_redirect(embedded_test_server()->GetURL(
      "a.com", "/server-redirect?" + url_b.spec()));
  TestNavigationManager navigation_manager(web_contents(), url_a_redirect);
  web_contents()->GetController().GoBack();

  // Wait for the navigation to start.
  EXPECT_TRUE(navigation_manager.WaitForRequestStart());
  auto* navigation_request =
      NavigationRequest::From(navigation_manager.GetNavigationHandle());
  auto reasons =
      navigation_request->commit_params().not_restored_reasons.Clone();
  // The reasons have not been reset yet.
  auto rfh_a_result = MatchesNotRestoredReasons(
      /*id=*/std::nullopt, /*name=*/std::nullopt,
      /*src=*/std::nullopt,
      /*reasons=*/
      {MatchesDetailedReason("masked", /*source=*/std::nullopt)},
      MatchesSameOriginDetails(
          /*url=*/url_a_redirect,
          /*children=*/{}));

  EXPECT_THAT(reasons, rfh_a_result);

  // Redirect happens, and now the reasons are reset.
  EXPECT_TRUE(navigation_manager.WaitForResponse());
  auto* reasons_after_redirect =
      navigation_request->commit_params().not_restored_reasons.get();
  EXPECT_THAT(reasons_after_redirect, nullptr);
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

  // Eviction reasons should be recorded internally.
  ExpectNotRestored({NotRestoredReason::kJavaScriptExecution}, {}, {}, {}, {},
                    FROM_HERE);
  // Redirect happened once.
  EXPECT_TRUE(ExecJs(current_frame_host(),
                     "performance.getEntriesByType('navigation')[0]."
                     "redirectCount == 1"));
  // Navigation type should be navigate, instead of back-forward because of the
  // redirect.
  EXPECT_TRUE(ExecJs(current_frame_host(),
                     "performance.getEntriesByType('navigation')[0]."
                     "type == 'navigate'"));
  // NotRestoredReasons are not sent to the renderer because of redirect.
  EXPECT_TRUE(ExecJs(current_frame_host(),
                     "performance.getEntriesByType('navigation')[0]."
                     "notRestoredReasons == null"));
}

// Test that after reload, NotRestoredReasons are reset.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithNotRestoredReasons,
                       Reload) {
  CreateHttpsServer();
  ASSERT_TRUE(https_server()->Start());

  GURL url_a(https_server()->GetURL("a.com", kBlockingPagePath));
  GURL url_b(https_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to a bfcache blocking page.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));

  // 3) Navigate back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  // Blocking reasons should be recorded.
  ExpectNotRestored({NotRestoredReason::kBlocklistedFeatures},
                    {kBlockingReasonEnum}, {}, {}, {}, FROM_HERE);
  // Expect that NotRestoredReasons and the blocking feature's source location
  // are reported.
  auto rfh_a_result = MatchesNotRestoredReasons(
      /*id=*/std::nullopt, /*name=*/std::nullopt,
      /*src=*/std::nullopt,
      /*reasons=*/
      {MatchesDetailedReason(kBlockingReasonString,
                             /*source=*/std::nullopt)},
      MatchesSameOriginDetails(
          /*url=*/url_a,
          /*children=*/{}));
  EXPECT_THAT(current_frame_host()->NotRestoredReasonsForTesting(),
              rfh_a_result);

  // Reload
  {
    TestNavigationObserver observer(web_contents());
    web_contents()->GetController().Reload(content::ReloadType::BYPASSING_CACHE,
                                           false);  // check_for_repost
    observer.Wait();
  }
  // Expect that NotRestoredReasons are reset to null after reload.
  EXPECT_TRUE(current_frame_host()->NotRestoredReasonsForTesting().is_null());
  EXPECT_TRUE(ExecJs(current_frame_host(),
                     "performance.getEntriesByType('navigation')[0]."
                     "type == 'reload'"));
  EXPECT_EQ(true, EvalJs(current_frame_host(),
                         "performance.getEntriesByType('navigation')[0]."
                         "notRestoredReasons === null"));
}

// Frame attributes are reported as null when they are not set.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestWithNotRestoredReasons,
                       IframesWithoutAttributes) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com",
      "/back_forward_cache/page_with_iframes_without_attributes.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  // 1) Navigate to A and use dummy blocking feature in a subframe.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  RenderFrameHostImplWrapper rfh_a_1(ChildFrameAt(current_frame_host(), 0));
  rfh_a_1->UseDummyStickyBackForwardCacheDisablingFeatureForTesting();
  GURL rfh_a_url = rfh_a->GetLastCommittedURL();
  GURL rfh_a_1_url = rfh_a_1->GetLastCommittedURL();

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImplWrapper rfh_b(current_frame_host());

  // 3) Navigate back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kBlocklistedFeatures},
                    {blink::scheduler::WebSchedulerTrackedFeature::kDummy}, {},
                    {}, {}, FROM_HERE);
  // Expect that NotRestoredReasons are reported, and all the cross-origin
  // blocked value are masked.
  auto rfh_a_1_result = MatchesNotRestoredReasons(
      /*id=*/std::nullopt, /*name=*/std::nullopt,
      /*src=*/rfh_a_1_url.spec(),
      /*reasons=*/{MatchesDetailedReason("Dummy", /*source=*/std::nullopt)},
      MatchesSameOriginDetails(
          /*url=*/rfh_a_1_url,
          /*children=*/{}));
  auto rfh_a_result = MatchesNotRestoredReasons(
      /*id=*/std::nullopt,
      /*name=*/std::nullopt, /*src=*/std::nullopt, /*reasons=*/{},
      MatchesSameOriginDetails(
          /*url=*/rfh_a_url,
          /*children=*/{rfh_a_1_result}));
  EXPECT_THAT(current_frame_host()->NotRestoredReasonsForTesting(),
              rfh_a_result);
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
}

class BackForwardCacheBrowserTestWithNotRestoredReasonsProactiveSwapOptOut
    : public BackForwardCacheBrowserTestWithNotRestoredReasons {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableFeatureAndSetParams(blink::features::kRelOpenerBcgDependencyHint, "",
                              "");
    BackForwardCacheBrowserTestWithNotRestoredReasons::SetUpCommandLine(
        command_line);
  }
};

IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestWithNotRestoredReasonsProactiveSwapOptOut,
    NavigateWithRelOpener) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url_1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  const GURL url_2(embedded_test_server()->GetURL("a.com", "/title2.html"));

  ASSERT_TRUE(NavigateToURL(shell(), url_1));
  current_frame_host()->GetBackForwardCacheMetrics()->SetObserverForTesting(
      this);

  // The document can't enter the BackForwardCache because rel=opener opts out
  // of the proactive BrowsingInstance swap.
  TestNavigationObserver nav_observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace(R"(
    let anchor = document.createElement('a');
    anchor.href = $1;
    anchor.rel = 'opener';
    anchor.text = 'Link';
    document.body.appendChild(anchor);
    anchor.click();
  )",
                                               url_2)));
  nav_observer.Wait();

  ASSERT_TRUE(HistoryGoBack(web_contents()));

  ExpectNotRestored(
      {NotRestoredReason::kBrowsingInstanceNotSwapped}, {},
      {ShouldSwapBrowsingInstance::kNo_InitiatorRequestedNoProactiveSwap}, {},
      {}, FROM_HERE);
  // Make sure that the tree result also has the same reasons. BrowsingInstance
  // NotSwapped can only be known at commit time.
  EXPECT_THAT(
      GetTreeResult()->GetDocumentResult(),
      MatchesDocumentResult(
          NotRestoredReasons({NotRestoredReason::kBrowsingInstanceNotSwapped}),
          BlockListedFeatures()));

  // The reason is recorded and sent to the renderer.
  // TODO(crbug.com/40275090): BrowsingInstanceNotSwapped should not be reported
  // as internal-error.
  std::optional<std::string> frame_attribute;
  if (!ShouldCreateNewHostForAllFrames()) {
    // Depending on whether the RenderFrameHost changed or not, the frame
    // attributes will either be std::nullopt or an empty string, due to how the
    // NotRestoredReasons object is created. Ultimately, this difference doesn't
    // really matter that much as the difference is not web-exposed.
    frame_attribute = "";
  }
  auto reasons = MatchesNotRestoredReasons(
      /*id=*/frame_attribute,
      /*name=*/frame_attribute, /*src=*/frame_attribute,
      /*reasons=*/
      {MatchesDetailedReason("masked", /*source=*/std::nullopt)},
      MatchesSameOriginDetails(
          /*url=*/url_1,
          /*children=*/{}));
  EXPECT_THAT(current_frame_host()->NotRestoredReasonsForTesting(), reasons);
}

class BackForwardCacheBrowserTestWithNotRestoredReasonsMaskCrossOrigin
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DisableFeature(kAllowCrossOriginNotRestoredReasons);
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

// NotRestoredReasons are masked for all the cross origin iframes.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestWithNotRestoredReasonsMaskCrossOrigin,
    AllCrossOriginMasked) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c),d)"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  // 1) Navigate to A and use dummy blocking feature in a cross-origin subframe.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  RenderFrameHostImplWrapper rfh_a_1(ChildFrameAt(current_frame_host(), 0));
  RenderFrameHostImplWrapper rfh_a_2(ChildFrameAt(current_frame_host(), 1));
  rfh_a_1->UseDummyStickyBackForwardCacheDisablingFeatureForTesting();
  GURL rfh_a_url = rfh_a->GetLastCommittedURL();
  GURL rfh_a_1_url = rfh_a_1->GetLastCommittedURL();
  GURL rfh_a_2_url = rfh_a_2->GetLastCommittedURL();

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImplWrapper rfh_b(current_frame_host());

  // 3) Navigate back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kBlocklistedFeatures},
                    {blink::scheduler::WebSchedulerTrackedFeature::kDummy}, {},
                    {}, {}, FROM_HERE);
  // Expect that NotRestoredReasons are reported, and all the cross-origin
  // blocked value are masked.
  auto rfh_a_1_result = MatchesNotRestoredReasons(
      /*id=*/"child-0", /*name=*/std::nullopt,
      /*src=*/rfh_a_1_url.spec(), /*reasons=*/{},
      /*same_origin_details=*/std::nullopt);
  auto rfh_a_2_result = MatchesNotRestoredReasons(
      /*id=*/"child-1", /*name=*/std::nullopt,
      /*src=*/rfh_a_2_url.spec(), /*reasons=*/{},
      /*same_origin_details=*/std::nullopt);
  auto rfh_a_result = MatchesNotRestoredReasons(
      /*id=*/std::nullopt,
      /*name=*/std::nullopt, /*src=*/std::nullopt, /*reasons=*/
      {MatchesDetailedReason("masked", /*source=*/std::nullopt)},
      MatchesSameOriginDetails(
          /*url=*/rfh_a_url,
          /*children=*/{rfh_a_1_result, rfh_a_2_result}));
  EXPECT_THAT(current_frame_host()->NotRestoredReasonsForTesting(),
              rfh_a_result);
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
}

}  // namespace content
