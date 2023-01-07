// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/back_forward_cache_browsertest.h"

#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/web_contents/web_contents_impl.h"
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

class BackForwardCacheBrowserTestWithNotRestoredReasons
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableFeatureAndSetParams(
        blink::features::kBackForwardCacheSendNotRestoredReasons, "", "");
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  }
};

using ReasonsMatcher = testing::Matcher<
    const blink::mojom::BackForwardCacheNotRestoredReasonsPtr&>;
using SameOriginMatcher = testing::Matcher<
    const blink::mojom::SameOriginBfcacheNotRestoredDetailsPtr&>;
ReasonsMatcher BackForwardCacheBrowserTest::MatchesNotRestoredReasons(
    const testing::Matcher<bool>& blocked,
    const SameOriginMatcher* same_origin_details) {
  return testing::Pointee(testing::AllOf(
      testing::Field("blocked",
                     &blink::mojom::BackForwardCacheNotRestoredReasons::blocked,
                     blocked),
      testing::Field(
          "same_origin_details",
          &blink::mojom::BackForwardCacheNotRestoredReasons::
              same_origin_details,
          same_origin_details
              ? *same_origin_details
              : testing::Property(
                    "is_null",
                    &blink::mojom::SameOriginBfcacheNotRestoredDetailsPtr::
                        is_null,
                    true))));
}

SameOriginMatcher BackForwardCacheBrowserTest::MatchesSameOriginDetails(
    const testing::Matcher<std::string>& id,
    const testing::Matcher<std::string>& name,
    const testing::Matcher<std::string>& src,
    const testing::Matcher<std::string>& url,
    const std::vector<testing::Matcher<std::string>>& reasons,
    const std::vector<ReasonsMatcher>& children) {
  return testing::Pointee(testing::AllOf(
      testing::Field(
          "id", &blink::mojom::SameOriginBfcacheNotRestoredDetails::id, id),
      testing::Field("name",
                     &blink::mojom::SameOriginBfcacheNotRestoredDetails::name,
                     name),
      testing::Field(
          "src", &blink::mojom::SameOriginBfcacheNotRestoredDetails::src, src),
      testing::Field(
          "url", &blink::mojom::SameOriginBfcacheNotRestoredDetails::url, url),
      testing::Field(
          "reasons",
          &blink::mojom::SameOriginBfcacheNotRestoredDetails::reasons,
          testing::UnorderedElementsAreArray(reasons)),
      testing::Field(
          "children",
          &blink::mojom::SameOriginBfcacheNotRestoredDetails::children,
          testing::ElementsAreArray(children))));
}

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
  std::string rfh_a_url = rfh_a->GetLastCommittedURL().spec();

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImplWrapper rfh_b(current_frame_host());

  // 3) Navigate back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kBlocklistedFeatures},
                    {blink::scheduler::WebSchedulerTrackedFeature::kDummy}, {},
                    {}, {}, FROM_HERE);
  // Expect that NotRestoredReasons are reported.
  auto rfh_a_details = MatchesSameOriginDetails(
      /*id=*/"", /*name=*/"", /*src=*/"",
      /*url=*/rfh_a_url, /*reasons=*/{"Dummy"}, /*children=*/{});
  auto rfh_a_result = MatchesNotRestoredReasons(
      /*blocked=*/true, &rfh_a_details);
  EXPECT_THAT(current_frame_host()->NotRestoredReasonsForTesting(),
              rfh_a_result);
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());

  // 4) Navigate forward.
  ASSERT_TRUE(HistoryGoForward(web_contents()));
  ExpectRestored(FROM_HERE);
  // Expect that NotRestoredReasons are not reported at all.
  EXPECT_TRUE(current_frame_host()->NotRestoredReasonsForTesting().is_null());
}

// Frame attributes are only reported when the document is same origin with main
// document. Also test that the details for cross-origin subtree are masked.
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
  std::string rfh_a_1_url = rfh_a_1->GetLastCommittedURL().spec();
  std::string rfh_a_2_url = rfh_a_2->GetLastCommittedURL().spec();

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

  // Expect that id and name are masked for |rfh_b|, but reported for |rfh_a_2|.
  // Note that |rfh_a_3| is masked because it's a child of |rfh_b|.
  auto rfh_a_2_details = MatchesSameOriginDetails(
      /*id=*/"rfh_a_2_id", /*name=*/"rfh_a_2_name", /*src=*/rfh_a_2_url,
      /*url=*/rfh_a_2_url, /*reasons=*/{}, /*children=*/{});
  auto rfh_b_result = MatchesNotRestoredReasons(
      /*blocked=*/true, nullptr);
  auto rfh_a_2_result = MatchesNotRestoredReasons(
      /*blocked=*/false, &rfh_a_2_details);
  auto rfh_a_1_details = MatchesSameOriginDetails(
      /*id=*/"", /*name=*/"", /*src=*/"", /*url=*/rfh_a_1_url,
      /*reasons=*/{},
      /*children=*/{rfh_a_2_result, rfh_b_result});
  auto rfh_a_1_result = MatchesNotRestoredReasons(
      /*blocked=*/false, &rfh_a_1_details);

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
  std::string rfh_a_1_url = rfh_a_1->GetLastCommittedURL().spec();
  std::string rfh_a_2_url = rfh_a_2->GetLastCommittedURL().spec();
  std::string rfh_a_3_url = rfh_a_3->GetLastCommittedURL().spec();
  std::string rfh_a_4_url = rfh_a_4->GetLastCommittedURL().spec();

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

  auto rfh_a_2_details = MatchesSameOriginDetails(
      /*id=*/"child-0", /*name=*/"", /*src=*/rfh_a_2_url, /*url=*/rfh_a_2_url,
      /*reasons=*/{"Dummy"},
      /*children=*/{});
  auto rfh_a_2_result = MatchesNotRestoredReasons(
      /*blocked=*/true, &rfh_a_2_details);
  auto rfh_a_4_details = MatchesSameOriginDetails(
      /*id=*/"child-0", /*name=*/"", /*src=*/rfh_a_4_url, /*url=*/rfh_a_4_url,
      /*reasons=*/{"Dummy"},
      /*children=*/{});
  auto rfh_a_4_result = MatchesNotRestoredReasons(
      /*blocked=*/true, &rfh_a_4_details);
  auto rfh_a_3_details = MatchesSameOriginDetails(
      /*id=*/"child-1", /*name=*/"", /*src=*/rfh_a_3_url, /*url=*/rfh_a_3_url,
      /*reasons=*/{}, /*children=*/
      {rfh_a_4_result});
  auto rfh_a_3_result = MatchesNotRestoredReasons(
      /*blocked=*/false, &rfh_a_3_details);
  auto rfh_a_1_details = MatchesSameOriginDetails(
      /*id=*/"", /*name=*/"", /*src=*/"", /*url=*/rfh_a_1_url,
      /*reasons=*/{"Dummy"},
      /*children=*/{rfh_a_2_result, rfh_a_3_result});
  auto rfh_a_1_result = MatchesNotRestoredReasons(
      /*blocked=*/true, &rfh_a_1_details);
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
  std::string rfh_a_url = rfh_a->GetLastCommittedURL().spec();

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
          NotRestoredReasons(NotRestoredReason::kRelatedActiveContentsExist,
                             NotRestoredReason::kBrowsingInstanceNotSwapped),
          BlockListedFeatures()));

  // Both reasons are recorded and sent to the renderer.
  // BrowsingInstanceNotSwapped is masked as internal error.
  auto rfh_a_details = MatchesSameOriginDetails(
      /*id=*/"", /*name=*/"", /*src=*/"", /*url=*/rfh_a_url,
      /*reasons=*/{"Related active contents", "Internal error"},
      /*children=*/{});
  auto rfh_a_result = MatchesNotRestoredReasons(
      /*blocked=*/true, &rfh_a_details);
  EXPECT_THAT(current_frame_host()->NotRestoredReasonsForTesting(),
              rfh_a_result);
}

}  // namespace content
