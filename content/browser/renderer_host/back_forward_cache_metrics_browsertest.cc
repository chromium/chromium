// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/browser/renderer_host/back_forward_cache_metrics.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"

using base::Bucket;
using testing::ElementsAre;

namespace content {

namespace {

constexpr uint64_t kPageShowFeature = static_cast<uint64_t>(
    blink::scheduler::WebSchedulerTrackedFeature::kPageShowEventListener);

constexpr uint64_t kRequestedGeolocationPermissionFeature =
    static_cast<uint64_t>(blink::scheduler::WebSchedulerTrackedFeature::
                              kRequestedGeolocationPermission);

ukm::SourceId ToSourceId(int64_t navigation_id) {
  return ukm::ConvertToSourceId(navigation_id,
                                ukm::SourceIdType::NAVIGATION_ID);
}

// Some features are present in almost all page loads (especially the ones
// which are related to the document finishing loading).
// We ignore them to make tests easier to read and write.
constexpr uint64_t kFeaturesToIgnoreMask =
    1ull << static_cast<size_t>(
        blink::scheduler::WebSchedulerTrackedFeature::kDocumentLoaded) |
    1ull << static_cast<size_t>(blink::scheduler::WebSchedulerTrackedFeature::
                                    kOutstandingNetworkRequestFetch) |
    1ull << static_cast<size_t>(blink::scheduler::WebSchedulerTrackedFeature::
                                    kOutstandingNetworkRequestXHR) |
    1ull << static_cast<size_t>(blink::scheduler::WebSchedulerTrackedFeature::
                                    kOutstandingNetworkRequestOthers);

using UkmMetrics = ukm::TestUkmRecorder::HumanReadableUkmMetrics;
using UkmEntry = ukm::TestUkmRecorder::HumanReadableUkmEntry;

}  // namespace

class BackForwardCacheMetricsBrowserTest : public ContentBrowserTest,
                                           public WebContentsObserver {
 public:
  BackForwardCacheMetricsBrowserTest() {
    geolocation_override_ =
        std::make_unique<device::ScopedGeolocationOverrider>(1.0, 1.0);
  }

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
    WebContentsObserver::Observe(shell()->web_contents());
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  void GiveItSomeTime() {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromMilliseconds(200));
    run_loop.Run();
  }

  RenderFrameHostImpl* current_frame_host() {
    return web_contents()->GetFrameTree()->root()->current_frame_host();
  }

  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    navigation_ids_.push_back(navigation_handle->GetNavigationId());
  }

  std::vector<int64_t> navigation_ids_;

 private:
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_override_;
};

IN_PROC_BROWSER_TEST_F(BackForwardCacheMetricsBrowserTest, UKM) {
  ukm::TestAutoSetUkmRecorder recorder;

  const GURL url1(
      embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html"));
  const GURL url2(embedded_test_server()->GetURL("/title1.html"));
  const char kChildFrameId[] = "child0";

  EXPECT_TRUE(NavigateToURL(shell(), url2));
  // The navigation entries are:
  // [*url2].

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  // The navigation entries are:
  // [url2, *url1(subframe)].

  EXPECT_TRUE(
      NavigateIframeToURL(shell()->web_contents(), kChildFrameId, url2));
  // The navigation entries are:
  // [url2, url1(subframe), *url1(url2)].

  EXPECT_TRUE(NavigateToURL(shell(), url2));
  // The navigation entries are:
  // [url2, url1(subframe), url1(url2), *url2].

  {
    // We are waiting for two navigations here: main frame and subframe.
    TestNavigationObserver navigation_observer(shell()->web_contents(), 2);
    shell()->GoBackOrForward(-1);
    navigation_observer.WaitForNavigationFinished();
  }
  // The navigation entries are:
  // [url2, url1(subframe), *url1(url2), url2].

  {
    TestNavigationObserver navigation_observer(shell()->web_contents());
    shell()->GoBackOrForward(-1);
    navigation_observer.WaitForNavigationFinished();
  }
  // The navigation entries are:
  // [url2, *url1(subframe), url1(url2), url2].

  {
    TestNavigationObserver navigation_observer(shell()->web_contents());
    shell()->GoBackOrForward(-1);
    navigation_observer.WaitForNavigationFinished();
  }
  // The navigation entries are:
  // [*url2, url1(subframe), url1(url2), url2].

  // There are five new navigations and four back navigations.
  // Navigations 1, 2, 5 are main frame. Navigation 3 is an initial navigation
  // in the subframe, navigation 4 is a subframe navigation.
  ASSERT_EQ(navigation_ids_.size(), static_cast<size_t>(9));
  ukm::SourceId id1 = ToSourceId(navigation_ids_[0]);
  ukm::SourceId id2 = ToSourceId(navigation_ids_[1]);
  // ukm::SourceId id3 = ToSourceId(navigation_ids_[2]);
  // ukm::SourceId id4 = ToSourceId(navigation_ids_[3]);
  // ukm::SourceId id5 = ToSourceId(navigation_ids_[4]);
  ukm::SourceId id6 = ToSourceId(navigation_ids_[5]);
  // ukm::SourceId id7 = ToSourceId(navigation_ids_[6]);
  // ukm::SourceId id8 = ToSourceId(navigation_ids_[7]);
  ukm::SourceId id9 = ToSourceId(navigation_ids_[8]);

  std::string last_navigation_id =
      "LastCommittedCrossDocumentNavigationSourceIdForTheSameDocument";

  // First back-forward navigation (#6) navigates back to navigation #3, but it
  // is the subframe navigation, so the corresponding main frame navigation is
  // #2. Navigation #7 loads the subframe for navigation #6.
  // Second back-forward navigation (#8) navigates back to navigation #2,
  // but it is subframe navigation and not reflected here. Third back-forward
  // navigation (#9) navigates back to navigation #1.
  EXPECT_THAT(recorder.GetEntries("HistoryNavigation", {last_navigation_id}),
              testing::ElementsAre(UkmEntry{id6, {{last_navigation_id, id2}}},
                                   UkmEntry{id9, {{last_navigation_id, id1}}}));
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheMetricsBrowserTest,
                       NavigatedToTheMostRecentEntry) {
  ukm::TestAutoSetUkmRecorder recorder;

  const GURL url1(
      embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html"));
  const GURL url2(embedded_test_server()->GetURL("/title1.html"));
  const char kChildFrameId[] = "child0";

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(
      NavigateIframeToURL(shell()->web_contents(), kChildFrameId, url2));
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  {
    // We are waiting for two navigations here: main frame and subframe.
    TestNavigationObserver navigation_observer(shell()->web_contents(), 2);
    shell()->GoBackOrForward(-2);
    navigation_observer.WaitForNavigationFinished();
  }

  {
    TestNavigationObserver navigation_observer(shell()->web_contents());
    shell()->GoBackOrForward(1);
    navigation_observer.WaitForNavigationFinished();
  }

  {
    TestNavigationObserver navigation_observer(shell()->web_contents());
    shell()->GoBackOrForward(1);
    navigation_observer.WaitForNavigationFinished();
  }
  // The navigation entries are:
  // [url1(subframe), url1(url2), *url2].

  std::string navigated_to_last_entry =
      "NavigatedToTheMostRecentEntryForDocument";

  // The first back navigation goes to the url1(subframe) entry, while the last
  // active entry for that document was url1(url2).
  // The second back/forward navigation is a subframe one and should be ignored.
  // The last one navigates to the actual entry.
  EXPECT_THAT(
      recorder.GetMetrics("HistoryNavigation", {navigated_to_last_entry}),
      testing::ElementsAre(UkmMetrics{{navigated_to_last_entry, false}},
                           UkmMetrics{{navigated_to_last_entry, true}}));
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheMetricsBrowserTest, CloneAndGoBack) {
  ukm::TestAutoSetUkmRecorder recorder;

  const GURL url1(embedded_test_server()->GetURL("/title1.html"));
  const GURL url2(embedded_test_server()->GetURL("/title2.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  // Clone the tab and load the page.
  std::unique_ptr<WebContents> new_tab = shell()->web_contents()->Clone();
  WebContentsImpl* new_tab_impl = static_cast<WebContentsImpl*>(new_tab.get());
  WebContentsObserver::Observe(new_tab.get());
  NavigationController& new_controller = new_tab_impl->GetController();
  {
    TestNavigationObserver clone_observer(new_tab.get());
    new_controller.LoadIfNecessary();
    clone_observer.Wait();
  }

  {
    TestNavigationObserver navigation_observer(new_tab.get());
    new_controller.GoBack();
    navigation_observer.WaitForNavigationFinished();
  }

  {
    TestNavigationObserver navigation_observer(new_tab.get());
    new_controller.GoForward();
    navigation_observer.WaitForNavigationFinished();
  }

  {
    TestNavigationObserver navigation_observer(new_tab.get());
    new_controller.GoBack();
    navigation_observer.WaitForNavigationFinished();
  }

  ASSERT_EQ(navigation_ids_.size(), static_cast<size_t>(6));
  // ukm::SourceId id1 = ToSourceId(navigation_ids_[0]);
  // ukm::SourceId id2 = ToSourceId(navigation_ids_[1]);
  ukm::SourceId id3 = ToSourceId(navigation_ids_[2]);
  ukm::SourceId id4 = ToSourceId(navigation_ids_[3]);
  ukm::SourceId id5 = ToSourceId(navigation_ids_[4]);
  ukm::SourceId id6 = ToSourceId(navigation_ids_[5]);

  std::string last_navigation_id =
      "LastCommittedCrossDocumentNavigationSourceIdForTheSameDocument";

  // First two new navigations happen in the original tab.
  // The third navigation reloads the tab for the cloned WebContents.
  // The fourth goes back, but the metrics are not recorded due to it being
  // cloned and the metrics objects missing.
  // The last two navigations, however, should have metrics.
  EXPECT_THAT(recorder.GetEntries("HistoryNavigation", {last_navigation_id}),
              testing::ElementsAre(UkmEntry{id5, {{last_navigation_id, id3}}},
                                   UkmEntry{id6, {{last_navigation_id, id4}}}));
}

// Confirms that UKMs are not recorded on reloading.
IN_PROC_BROWSER_TEST_F(BackForwardCacheMetricsBrowserTest, Reload) {
  ukm::TestAutoSetUkmRecorder recorder;

  const GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  const GURL url2(embedded_test_server()->GetURL("b.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  web_contents()->GetController().Reload(ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_TRUE(NavigateToURL(shell(), url2));
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  ASSERT_EQ(navigation_ids_.size(), static_cast<size_t>(6));
  ukm::SourceId id1 = ToSourceId(navigation_ids_[0]);
  // ukm::SourceId id2 = ToSourceId(navigation_ids_[1]);
  ukm::SourceId id3 = ToSourceId(navigation_ids_[2]);
  ukm::SourceId id4 = ToSourceId(navigation_ids_[3]);
  // ukm::SourceId id5 = ToSourceId(navigation_ids_[4]);
  ukm::SourceId id6 = ToSourceId(navigation_ids_[5]);

  // The last navigation is for reloading, and the UKM is not recorded for this
  // navigation. This also checks relaoding makes a different navigation ID.
  std::string last_navigation_id =
      "LastCommittedCrossDocumentNavigationSourceIdForTheSameDocument";
  EXPECT_THAT(recorder.GetEntries("HistoryNavigation", {last_navigation_id}),
              testing::ElementsAre(UkmEntry{id3, {{last_navigation_id, id1}}},
                                   UkmEntry{id6, {{last_navigation_id, id4}}}));
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheMetricsBrowserTest,
                       SameDocumentNavigationAndGoBackImmediately) {
  ukm::TestAutoSetUkmRecorder recorder;

  const GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  const GURL url1_foo(
      embedded_test_server()->GetURL("a.com", "/title1.html#foo"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(NavigateToURL(shell(), url1_foo));

  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  ASSERT_EQ(navigation_ids_.size(), static_cast<size_t>(3));
  std::string last_navigation_id =
      "LastCommittedCrossDocumentNavigationSourceIdForTheSameDocument";
  // Ensure that UKMs are not recorded for the same-document history
  // navigations.
  EXPECT_THAT(recorder.GetEntries("HistoryNavigation", {last_navigation_id}),
              testing::ElementsAre());
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheMetricsBrowserTest,
                       GoBackToSameDocumentNavigationEntry) {
  ukm::TestAutoSetUkmRecorder recorder;

  const GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  const GURL url1_foo(
      embedded_test_server()->GetURL("a.com", "/title1.html#foo"));
  const GURL url2(embedded_test_server()->GetURL("b.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(NavigateToURL(shell(), url1_foo));
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  ASSERT_EQ(navigation_ids_.size(), static_cast<size_t>(4));
  ukm::SourceId id1 = ToSourceId(navigation_ids_[0]);
  // ukm::SourceId id2 = ToSourceId(navigation_ids_[1]);
  // ukm::SourceId id3 = ToSourceId(navigation_ids_[2]);
  ukm::SourceId id4 = ToSourceId(navigation_ids_[3]);

  std::string last_navigation_id =
      "LastCommittedCrossDocumentNavigationSourceIdForTheSameDocument";

  // The second navigation is a same-document navigation for the page loaded by
  // the first navigation. The third navigation is a regular navigation. The
  // fourth navigation goes back.
  //
  // The back-forward navigation (#4) goes back to the navigation entry created
  // by the same-document navigation (#2), so we expect the id corresponding to
  // the previous non-same-document navigation (#1).
  EXPECT_THAT(recorder.GetEntries("HistoryNavigation", {last_navigation_id}),
              testing::ElementsAre(UkmEntry{id4, {{last_navigation_id, id1}}}));
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheMetricsBrowserTest,
                       GoBackToSameDocumentNavigationEntry2) {
  ukm::TestAutoSetUkmRecorder recorder;

  const GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  const GURL url1_foo(
      embedded_test_server()->GetURL("a.com", "/title1.html#foo"));
  const GURL url2(embedded_test_server()->GetURL("b.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(NavigateToURL(shell(), url1_foo));
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  web_contents()->GetController().GoToOffset(-2);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  ASSERT_EQ(navigation_ids_.size(), static_cast<size_t>(4));
  ukm::SourceId id1 = ToSourceId(navigation_ids_[0]);
  // ukm::SourceId id2 = ToSourceId(navigation_ids_[1]);
  // ukm::SourceId id3 = ToSourceId(navigation_ids_[2]);
  ukm::SourceId id4 = ToSourceId(navigation_ids_[3]);

  std::string last_navigation_id =
      "LastCommittedCrossDocumentNavigationSourceIdForTheSameDocument";

  // The second navigation is a same-document navigation for the page loaded by
  // the first navigation. The third navigation is a regular navigation. The
  // fourth navigation goes back.
  //
  // The back-forward navigation (#4) goes back to navigation #1, skipping a
  // navigation entry generated by same-document navigation (#2). Ensure that
  // the recorded id belongs to the navigation #1, not #2.
  EXPECT_THAT(recorder.GetEntries("HistoryNavigation", {last_navigation_id}),
              testing::ElementsAre(UkmEntry{id4, {{last_navigation_id, id1}}}));
}

namespace {

struct FeatureUsage {
  ukm::SourceId source_id;
  uint64_t main_frame_features;
  uint64_t same_origin_subframes_features;
  uint64_t cross_origin_subframes_features;
};

bool operator==(const FeatureUsage& lhs, const FeatureUsage& rhs) {
  return lhs.source_id == rhs.source_id &&
         lhs.main_frame_features == rhs.main_frame_features &&
         lhs.same_origin_subframes_features ==
             rhs.same_origin_subframes_features &&
         lhs.cross_origin_subframes_features ==
             rhs.cross_origin_subframes_features;
}

std::ostream& operator<<(std::ostream& os, const FeatureUsage& usage) {
  os << "source_id=" << usage.source_id
     << " main_frame_features=" << usage.main_frame_features
     << " same_origin_features=" << usage.same_origin_subframes_features
     << " cross_origin_features=" << usage.cross_origin_subframes_features;
  return os;
}

std::vector<FeatureUsage> GetFeatureUsageMetrics(
    ukm::TestAutoSetUkmRecorder* recorder) {
  std::vector<FeatureUsage> result;
  for (const auto& entry :
       recorder->GetEntries("HistoryNavigation",
                            {"MainFrameFeatures", "SameOriginSubframesFeatures",
                             "CrossOriginSubframesFeatures"})) {
    FeatureUsage feature_usage;
    feature_usage.source_id = entry.source_id;
    feature_usage.main_frame_features =
        entry.metrics.at("MainFrameFeatures") & ~kFeaturesToIgnoreMask;
    feature_usage.same_origin_subframes_features =
        entry.metrics.at("SameOriginSubframesFeatures") &
        ~kFeaturesToIgnoreMask;
    feature_usage.cross_origin_subframes_features =
        entry.metrics.at("CrossOriginSubframesFeatures") &
        ~kFeaturesToIgnoreMask;
    result.push_back(feature_usage);
  }
  return result;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(BackForwardCacheMetricsBrowserTest, Features_MainFrame) {
  ukm::TestAutoSetUkmRecorder recorder;

  const GURL url1(embedded_test_server()->GetURL(
      "/back_forward_cache/page_with_pageshow.html"));
  const GURL url2(embedded_test_server()->GetURL("/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  {
    TestNavigationObserver navigation_observer(shell()->web_contents());
    shell()->GoBackOrForward(-1);
    navigation_observer.WaitForNavigationFinished();
  }

  ASSERT_EQ(navigation_ids_.size(), static_cast<size_t>(3));
  // ukm::SourceId id1 = ToSourceId(navigation_ids_[0]);
  // ukm::SourceId id2 = ToSourceId(navigation_ids_[1]);
  ukm::SourceId id3 = ToSourceId(navigation_ids_[2]);

  EXPECT_THAT(
      GetFeatureUsageMetrics(&recorder),
      testing::ElementsAre(FeatureUsage{id3, 1 << kPageShowFeature, 0, 0}));
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheMetricsBrowserTest,
                       Features_MainFrame_CrossOriginNavigation) {
  ukm::TestAutoSetUkmRecorder recorder;

  const GURL url1(embedded_test_server()->GetURL(
      "/back_forward_cache/page_with_pageshow.html"));
  const GURL url2(embedded_test_server()->GetURL("bar.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  {
    TestNavigationObserver navigation_observer(shell()->web_contents());
    shell()->GoBackOrForward(-1);
    navigation_observer.WaitForNavigationFinished();
  }

  ASSERT_EQ(navigation_ids_.size(), static_cast<size_t>(3));
  // ukm::SourceId id1 = ToSourceId(navigation_ids_[0]);
  // ukm::SourceId id2 = ToSourceId(navigation_ids_[1]);
  ukm::SourceId id3 = ToSourceId(navigation_ids_[2]);

  EXPECT_THAT(
      GetFeatureUsageMetrics(&recorder),
      testing::ElementsAre(FeatureUsage{id3, 1 << kPageShowFeature, 0, 0}));
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheMetricsBrowserTest,
                       Features_SameOriginSubframes) {
  ukm::TestAutoSetUkmRecorder recorder;

  const GURL url1(embedded_test_server()->GetURL(
      "/back_forward_cache/page_with_same_origin_subframe_with_pageshow.html"));
  const GURL url2(embedded_test_server()->GetURL("/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  {
    TestNavigationObserver navigation_observer(shell()->web_contents(), 2);
    shell()->GoBackOrForward(-1);
    navigation_observer.WaitForNavigationFinished();
  }

  ASSERT_EQ(navigation_ids_.size(), static_cast<size_t>(5));
  // ukm::SourceId id1 = ToSourceId(navigation_ids_[0]);
  // ukm::SourceId id2 = ToSourceId(navigation_ids_[1]);
  // ukm::SourceId id3 = ToSourceId(navigation_ids_[2]);
  ukm::SourceId id4 = ToSourceId(navigation_ids_[3]);

  EXPECT_THAT(
      GetFeatureUsageMetrics(&recorder),
      testing::ElementsAre(FeatureUsage{id4, 0, 1 << kPageShowFeature, 0}));
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheMetricsBrowserTest,
                       Features_SameOriginSubframes_CrossOriginNavigation) {
  ukm::TestAutoSetUkmRecorder recorder;

  const GURL url1(embedded_test_server()->GetURL(
      "/back_forward_cache/page_with_same_origin_subframe_with_pageshow.html"));
  const GURL url2(embedded_test_server()->GetURL("bar.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // When the page is restored from back-forward cache, there is one navigation
  // corresponding to the bfcache restore. Whereas, when the page is reloaded,
  // there are two navigations i.e., one loading the main frame and one loading
  // the subframe.
  ASSERT_EQ(navigation_ids_.size(), IsBackForwardCacheEnabled() ? 4u : 5u);
  // ukm::SourceId id1 = ToSourceId(navigation_ids_[0]);
  // ukm::SourceId id2 = ToSourceId(navigation_ids_[1]);
  // ukm::SourceId id3 = ToSourceId(navigation_ids_[2]);
  ukm::SourceId id4 = ToSourceId(navigation_ids_[3]);

  EXPECT_THAT(
      GetFeatureUsageMetrics(&recorder),
      testing::ElementsAre(FeatureUsage{id4, 0, 1 << kPageShowFeature, 0}));
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheMetricsBrowserTest,
                       Features_CrossOriginSubframes) {
  ukm::TestAutoSetUkmRecorder recorder;

  const GURL url1(embedded_test_server()->GetURL(
      "/back_forward_cache/"
      "page_with_cross_origin_subframe_with_pageshow.html"));
  const GURL url2(embedded_test_server()->GetURL("/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  {
    TestNavigationObserver navigation_observer(shell()->web_contents(), 2);
    shell()->GoBackOrForward(-1);
    navigation_observer.WaitForNavigationFinished();
  }

  ASSERT_EQ(navigation_ids_.size(), static_cast<size_t>(5));
  // ukm::SourceId id1 = ToSourceId(navigation_ids_[0]);
  // ukm::SourceId id2 = ToSourceId(navigation_ids_[1]);
  // ukm::SourceId id3 = ToSourceId(navigation_ids_[2]);
  ukm::SourceId id4 = ToSourceId(navigation_ids_[3]);

  EXPECT_THAT(
      GetFeatureUsageMetrics(&recorder),
      testing::ElementsAre(FeatureUsage{id4, 0, 0, 1 << kPageShowFeature}));
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheMetricsBrowserTest, DedicatedWorker) {
  ukm::TestAutoSetUkmRecorder recorder;

  const GURL url(embedded_test_server()->GetURL(
      "/back_forward_cache/page_with_dedicated_worker.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_EQ(
      static_cast<WebContentsImpl*>(shell()->web_contents())
              ->GetMainFrame()
              ->scheduler_tracked_features() &
          ~kFeaturesToIgnoreMask,
      1ull << static_cast<size_t>(blink::scheduler::WebSchedulerTrackedFeature::
                                      kDedicatedWorkerOrWorklet));
}

// TODO(https://crbug.com/154571): Shared workers are not available on Android.
#if defined(OS_ANDROID)
#define MAYBE_SharedWorker DISABLED_SharedWorker
#else
#define MAYBE_SharedWorker SharedWorker
#endif
IN_PROC_BROWSER_TEST_F(BackForwardCacheMetricsBrowserTest, MAYBE_SharedWorker) {
  const GURL url(embedded_test_server()->GetURL(
      "/back_forward_cache/page_with_shared_worker.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_EQ(static_cast<WebContentsImpl*>(shell()->web_contents())
                    ->GetMainFrame()
                    ->scheduler_tracked_features() &
                ~kFeaturesToIgnoreMask,
            1ull << static_cast<uint32_t>(
                blink::scheduler::WebSchedulerTrackedFeature::kSharedWorker));
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheMetricsBrowserTest, Geolocation) {
  const GURL url1(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  RenderFrameHostImpl* main_frame = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());
  EXPECT_EQ("success", EvalJs(main_frame, R"(
    new Promise(resolve => {
      navigator.geolocation.getCurrentPosition(
        resolve.bind(this, "success"),
        resolve.bind(this, "failure"))
      });
  )"));
  EXPECT_TRUE(main_frame->scheduler_tracked_features() &
              (1 << kRequestedGeolocationPermissionFeature));
}

class RecordBackForwardCacheMetricsWithoutEnabling
    : public BackForwardCacheMetricsBrowserTest {
 public:
  RecordBackForwardCacheMetricsWithoutEnabling() {
    // Sets the allowed websites for testing.
    std::string allowed_websites =
        "https://a.allowed/back_forward_cache/, "
        "https://b.allowed/";
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kRecordBackForwardCacheMetricsWithoutEnabling,
          {{"allowed_websites", allowed_websites}}}},
        {features::kBackForwardCache});
  }

  ~RecordBackForwardCacheMetricsWithoutEnabling() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(RecordBackForwardCacheMetricsWithoutEnabling,
                       ReloadsAndHistoryNavigations) {
  base::HistogramTester histogram_tester;
  using ReloadsAndHistoryNavigations =
      BackForwardCacheMetrics::ReloadsAndHistoryNavigations;
  using ReloadsAfterHistoryNavigation =
      BackForwardCacheMetrics::ReloadsAfterHistoryNavigation;

  const char kReloadsAndHistoryNavigationsHistogram[] =
      "BackForwardCache.ReloadsAndHistoryNavigations";
  const char kReloadsAfterHistoryNavigationHistogram[] =
      "BackForwardCache.ReloadsAfterHistoryNavigation";

  EXPECT_THAT(
      histogram_tester.GetAllSamples(kReloadsAndHistoryNavigationsHistogram),
      testing::IsEmpty());
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kReloadsAfterHistoryNavigationHistogram),
      testing::IsEmpty());

  GURL url1(embedded_test_server()->GetURL(
      "a.allowed", "/back_forward_cache/allowed_path.html"));
  GURL url2(embedded_test_server()->GetURL(
      "b.disallowed", "/back_forward_cache/disallowed_path.html"));

  // 1) Navigate to url1.
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  // 2) Navigate to url2.
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  // 3) Go back to url1 and reload.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // Bucket count both "kHistoryNavigation" and
  // "kReloadAfterHistoryNavigation" should be 1 after one history
  // navigation and one reload.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kReloadsAndHistoryNavigationsHistogram),
      ElementsAre(
          Bucket(static_cast<int>(
                     ReloadsAndHistoryNavigations::kHistoryNavigation),
                 1),
          Bucket(
              static_cast<int>(
                  ReloadsAndHistoryNavigations::kReloadAfterHistoryNavigation),
              1)));

  // BackForwardCache is disabled here, the navigation is not served from
  // back-forward cache.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kReloadsAfterHistoryNavigationHistogram),
      ElementsAre(Bucket(
          static_cast<int>(
              ReloadsAfterHistoryNavigation::kNotServedFromBackForwardCache),
          1)));

  // 4) Go forward to url2 and reload.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // Bucket count both "kHistoryNavigation" and
  // "kReloadAfterHistoryNavigation" should still be 1 since the url2 is
  // not in the list of allowed_websties by
  // "kRecordBackForwardCacheMetricsWithoutEnabling" feature.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kReloadsAndHistoryNavigationsHistogram),
      ElementsAre(
          Bucket(static_cast<int>(
                     ReloadsAndHistoryNavigations::kHistoryNavigation),
                 1),
          Bucket(
              static_cast<int>(
                  ReloadsAndHistoryNavigations::kReloadAfterHistoryNavigation),
              1)));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(kReloadsAfterHistoryNavigationHistogram),
      ElementsAre(Bucket(
          static_cast<int>(
              ReloadsAfterHistoryNavigation::kNotServedFromBackForwardCache),
          1)));
}

class BackForwardCacheEnabledMetricsBrowserTest
    : public BackForwardCacheMetricsBrowserTest {
 protected:
  BackForwardCacheEnabledMetricsBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kBackForwardCache,
        {
            // Set a very long TTL before expiration (longer than the test
            // timeout) so tests that are expecting deletion don't pass when
            // they shouldn't.
            {"TimeToLiveInBackForwardCacheInSeconds", "3600"},
        });
  }

  ~BackForwardCacheEnabledMetricsBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(BackForwardCacheEnabledMetricsBrowserTest,
                       RecordReloadsAfterHistoryNavigation) {
  base::HistogramTester histogram_tester;
  using ReloadsAfterHistoryNavigation =
      BackForwardCacheMetrics::ReloadsAfterHistoryNavigation;
  const char kReloadsAfterHistoryNavigationHistogram[] =
      "BackForwardCache.ReloadsAfterHistoryNavigation";

  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  GURL url3(embedded_test_server()->GetURL("c.com", "/title3.html"));

  // 1) Navigate to url1 and reload.
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // No reloads should be recorded since it is not a history navigation.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kReloadsAfterHistoryNavigationHistogram),
      testing::IsEmpty());

  // 2) Navigate to url2.
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  // 3) Go back to url1 and reload.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // Bucket with "kServedFromBackForwardCache" should be 1 as url1 is served
  // from cache.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kReloadsAfterHistoryNavigationHistogram),
      ElementsAre(Bucket(
          static_cast<int>(
              ReloadsAfterHistoryNavigation::kServedFromBackForwardCache),
          1)));

  // 4) Go forward to url2 and reload twice.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // Bucket with "kServedFromBackForwardCache" should be 2 as only the first
  // reload after history navigation is recorded.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kReloadsAfterHistoryNavigationHistogram),
      ElementsAre(Bucket(
          static_cast<int>(
              ReloadsAfterHistoryNavigation::kServedFromBackForwardCache),
          2)));

  // 5) Go back and navigate to url3.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  RenderFrameHostImpl* rfh_url1 =
      web_contents()->GetFrameTree()->root()->current_frame_host();

  // Make url1 ineligible for caching so that when we navigate back it doesn't
  // fetch the RenderFrameHost from the back-forward cache.
  content::BackForwardCache::DisableForRenderFrameHost(
      rfh_url1, "BackForwardCacheMetricsBrowserTest");
  EXPECT_TRUE(NavigateToURL(shell(), url3));

  // 6) Go back and reload.
  // Ensure that "not served" is recorded.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // Bucket with "kNotServedFromBackForwardCache" should be 1 as url3 is not
  // served from BackForwardCache.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kReloadsAfterHistoryNavigationHistogram),
      ElementsAre(
          Bucket(static_cast<int>(ReloadsAfterHistoryNavigation::
                                      kNotServedFromBackForwardCache),
                 1),
          Bucket(
              static_cast<int>(
                  ReloadsAfterHistoryNavigation::kServedFromBackForwardCache),
              2)));
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheEnabledMetricsBrowserTest,
                       RestoreNavigationToNextPaint) {
  base::HistogramTester histogram_tester;
  const char kRestoreNavigationToNextPaintTimeHistogram[] =
      "BackForwardCache.Restore.NavigationToFirstPaint";

  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url2(embedded_test_server()->GetURL("b.com", "/title2.html"));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kRestoreNavigationToNextPaintTimeHistogram),
              testing::IsEmpty());

  // 1) Navigate to url1.
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to url2.
  EXPECT_TRUE(NavigateToURL(shell(), url2));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Go back to url1 and check if the metrics are recorded. Make sure the
  // page is restored from cache.
  shell()->web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_EQ(shell()->web_contents()->GetVisibility(), Visibility::VISIBLE);

  // Verify if the NavigationToFirstPaint metric was recorded on restore.
  do {
    FetchHistogramsFromChildProcesses();
    GiveItSomeTime();
  } while (
      histogram_tester.GetAllSamples(kRestoreNavigationToNextPaintTimeHistogram)
          .empty());
}

}  // namespace content
