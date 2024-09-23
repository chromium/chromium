// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sanitizer_buildflags.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/test_utils.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"

namespace content {

namespace {
const char kBackForwardCachePageWithFormStorableHistogramName[] =
    "BackForwardCache.PageWithForm.Storable";
const char kBackForwardCachePageWithFormRestoreResultHistogramName[] =
    "BackForwardCache.PageWithForm.RestoreResult";

class BackForwardCacheWebContentsDelegate : public WebContentsDelegate {
 public:
  BackForwardCacheWebContentsDelegate() = default;

  bool IsBackForwardCacheSupported(WebContents& web_contents) override {
    return true;
  }
};

}  // namespace

class BackForwardCacheMetricsTest : public RenderViewHostImplTestHarness,
                                    public WebContentsObserver {
 public:
  BackForwardCacheMetricsTest() {}

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    WebContents* web_contents = RenderViewHostImplTestHarness::web_contents();
    ASSERT_TRUE(web_contents);  // The WebContents should be created by now.
    WebContentsObserver::Observe(web_contents);
    web_contents->SetDelegate(&web_contents_delegate_);

    // Ensure that the time is non-null.
    clock_.Advance(base::Milliseconds(5));
    BackForwardCacheMetrics::OverrideTimeForTesting(&clock_);
  }

  void TearDown() override {
    BackForwardCacheMetrics::OverrideTimeForTesting(nullptr);
    RenderViewHostImplTestHarness::TearDown();
  }

  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    navigation_ids_.push_back(navigation_handle->GetNavigationId());
  }

 protected:
  ukm::TestAutoSetUkmRecorder recorder_;
  base::HistogramTester histogram_tester_;

  BackForwardCacheWebContentsDelegate web_contents_delegate_;

  base::SimpleTestTickClock clock_;

  std::vector<int64_t> navigation_ids_;
};

using UkmEntry = ukm::TestUkmRecorder::HumanReadableUkmEntry;

ukm::SourceId ToSourceId(int64_t navigation_id) {
  return ukm::ConvertToSourceId(navigation_id,
                                ukm::SourceIdType::NAVIGATION_ID);
}

TEST_F(BackForwardCacheMetricsTest, HistoryNavigationUKM) {
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");
  const GURL url3("http://foo3");

  // Advance clock by 2^N milliseconds (and spell out the intervals in binary)
  // to ensure that each pair is easily distinguished.

  NavigationSimulator::NavigateAndCommitFromDocument(url1, main_test_rfh());
  clock_.Advance(base::Milliseconds(0b1));
  NavigationSimulator::NavigateAndCommitFromDocument(url2, main_test_rfh());
  clock_.Advance(base::Milliseconds(0b10));
  NavigationSimulator::NavigateAndCommitFromDocument(url3, main_test_rfh());
  clock_.Advance(base::Milliseconds(0b100));
  NavigationSimulator::GoBack(contents());
  clock_.Advance(base::Milliseconds(0b1000));
  NavigationSimulator::GoBack(contents());
  clock_.Advance(base::Milliseconds(0b10000));
  NavigationSimulator::GoForward(contents());

  ASSERT_EQ(navigation_ids_.size(), static_cast<size_t>(6));
  ukm::SourceId id1 = ToSourceId(navigation_ids_[0]);
  ukm::SourceId id2 = ToSourceId(navigation_ids_[1]);
  ukm::SourceId id4 = ToSourceId(navigation_ids_[3]);
  ukm::SourceId id5 = ToSourceId(navigation_ids_[4]);
  ukm::SourceId id6 = ToSourceId(navigation_ids_[5]);

  // Navigations 4 and 5 are back navigations.
  // Navigation 6 is a forward navigation.

  std::string last_navigation_id =
      "LastCommittedCrossDocumentNavigationSourceIdForTheSameDocument";
  std::string time_away = "TimeSinceNavigatedAwayFromDocument";

  EXPECT_THAT(
      recorder_.GetEntries("HistoryNavigation",
                           {last_navigation_id, time_away}),
      testing::ElementsAre(
          UkmEntry{id4, {{last_navigation_id, id2}, {time_away, 0b100}}},
          UkmEntry{id5, {{last_navigation_id, id1}, {time_away, 0b1110}}},
          UkmEntry{id6, {{last_navigation_id, id4}, {time_away, 0b10000}}}));
}

TEST_F(BackForwardCacheMetricsTest, LongDurationsAreClamped) {
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  NavigationSimulator::NavigateAndCommitFromDocument(url1, main_test_rfh());
  NavigationSimulator::NavigateAndCommitFromDocument(url2, main_test_rfh());
  clock_.Advance(base::Hours(5) + base::Milliseconds(50));
  NavigationSimulator::GoBack(contents());

  ASSERT_EQ(navigation_ids_.size(), static_cast<size_t>(3));
  ukm::SourceId id3 = ToSourceId(navigation_ids_[2]);

  std::string time_away = "TimeSinceNavigatedAwayFromDocument";

  // The original interval of 5h + 50ms is clamped to just 5h.
  EXPECT_THAT(recorder_.GetEntries("HistoryNavigation", {time_away}),
              testing::ElementsAre(UkmEntry{
                  id3, {{time_away, base::Hours(5).InMilliseconds()}}}));
}

TEST_F(BackForwardCacheMetricsTest, TimeRecordedAtStart) {
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  // Advance clock by 2^N milliseconds (and spell out the intervals in binary)
  // to ensure that each pair is easily distinguished.

  {
    auto simulator =
        NavigationSimulator::CreateRendererInitiated(url1, main_test_rfh());
    simulator->Start();
    clock_.Advance(base::Milliseconds(0b1));
    simulator->Commit();
  }

  clock_.Advance(base::Milliseconds(0b10));

  {
    auto simulator =
        NavigationSimulator::CreateRendererInitiated(url2, main_test_rfh());
    simulator->Start();
    clock_.Advance(base::Milliseconds(0b100));
    simulator->Commit();
  }

  clock_.Advance(base::Milliseconds(0b1000));

  {
    auto simulator = NavigationSimulator::CreateHistoryNavigation(
        -1, contents(), false /* is_renderer_initiated */);
    simulator->Start();
    clock_.Advance(base::Milliseconds(0b10000));
    simulator->Commit();
  }

  ASSERT_EQ(navigation_ids_.size(), static_cast<size_t>(3));
  ukm::SourceId id3 = ToSourceId(navigation_ids_[2]);

  std::string time_away = "TimeSinceNavigatedAwayFromDocument";

  EXPECT_THAT(recorder_.GetEntries("HistoryNavigation", {time_away}),
              testing::ElementsAre(UkmEntry{id3, {{time_away, 0b1000}}}));
}

// TODO(crbug.com/40200059): Flaky under TSan.
#if BUILDFLAG(USING_SANITIZER)
#define MAYBE_TimeRecordedWhenRendererIsKilled DISABLED_TimeRecordedWhenRendererIsKilled
#else
#define MAYBE_TimeRecordedWhenRendererIsKilled TimeRecordedWhenRendererIsKilled
#endif
TEST_F(BackForwardCacheMetricsTest, MAYBE_TimeRecordedWhenRendererIsKilled) {
  // Need to enable back-forward cache to make sure a page is put into the
  // cache.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      GetBasicBackForwardCacheFeatureForTesting(),
      GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  base::HistogramTester histogram_tester;

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  // Go to foo1.
  NavigationSimulator::NavigateAndCommitFromDocument(url1, main_test_rfh());
  clock_.Advance(base::Milliseconds(0b1));
  TestRenderFrameHost* old_main_frame_host = main_test_rfh();

  // Go to foo2. Foo1 will be in the back-forward cache.
  NavigationSimulator::NavigateAndCommitFromDocument(url2, main_test_rfh());
  clock_.Advance(base::Milliseconds(0b10));

  // Kill the renderer.
  old_main_frame_host->GetProcess()->SimulateRenderProcessExit(
      base::TERMINATION_STATUS_PROCESS_WAS_KILLED, 1);
  clock_.Advance(base::Milliseconds(0b100));

  NavigationSimulator::GoBack(contents());
  clock_.Advance(base::Milliseconds(0b1000));

  const char kTimeUntilProcessKilled[] =
      "BackForwardCache.Eviction.TimeUntilProcessKilled";

  // The expected recorded time is between when the last navigation happened and
  // when the renderer is killed.
  EXPECT_THAT(histogram_tester.GetAllSamples(kTimeUntilProcessKilled),
              testing::ElementsAre(base::Bucket(0b10, 1)));
}

// Test that |GetDisallowedFeatures()| and |GetAllowedFeatures()| cover all the
// blocklisted features.
TEST_F(BackForwardCacheMetricsTest, AllFeaturesCovered) {
  // Features that were removed from the enum must have their int value listed
  // here because ::All() will still include them.
  std::unordered_set<uint64_t> removed_features{
      /* WebSchedulerTrackedFeature::kPageShowEventListener =*/6,
      /* WebSchedulerTrackedFeature::kPageHideEventListener =*/7,
      /* WebSchedulerTrackedFeature::kBeforeUnloadEventListener =*/8,
      /* WebSchedulerTrackedFeature::kUnloadEventListener =*/9,
      /* WebSchedulerTrackedFeature::kFreezeEventListener =*/10,
      /* WebSchedulerTrackedFeature::kResumeEventListener =*/11,
      /* WebSchedulerTrackedFeature::kDedicatedWorkerOrWorklet =*/14,
      /* WebSchedulerTrackedFeature::kServiceWorkerControlledPage =*/16,
      /* WebSchedulerTrackedFeature::kOutstandingIndexedDBTransaction =*/17,
      /* WebSchedulerTrackedFeature::kHasScriptableFramesInMultipleTabs =*/18,
      /* WebSchedulerTrackedFeature::kRequestedGeolocationPermission =*/19,
      /* WebSchedulerTrackedFeature::kRequestedNotificationsPermission =*/20,
      /* WebSchedulerTrackedFeature::kIndexedDBConnection =*/28,
      /* WebSchedulerTrackedFeature::kWebGL =*/29,
      /* WebSchedulerTrackedFeature::kWebVR =*/30,
      /* WebSchedulerTrackedFeature::kWakeLock =*/35,
      /* WebSchedulerTrackedFeature::kWebFileSystem =*/39,
      /* WebSchedulerTrackedFeature::kAppBanner =*/42,
      /* WebSchedulerTrackedFeature::kPortal =*/46,
      /* WebSchedulerTrackedFeature::kSpeechSynthesis =*/50,
      /* WebSchedulerTrackedFeature::kMediaSessionImplOnServiceCreated =*/56};

  for (BackForwardCacheImpl::CacheControlNoStoreContext ccns_context :
       {BackForwardCacheImpl::kInCCNSContext,
        BackForwardCacheImpl::kNotInCCNSContext}) {
    // Combine the result of |GetDisallowedFeatures()| and
    // |GetAllowedFeatures()|.
    std::unordered_set<uint64_t> combined_features;
    auto disallowed_features = BackForwardCacheImpl::GetDisallowedFeatures(
        BackForwardCacheImpl::RequestedFeatures::kAll, ccns_context);
    auto allowed_features = BackForwardCacheImpl::GetAllowedFeatures(
        BackForwardCacheImpl::RequestedFeatures::kAll, ccns_context);
    ASSERT_TRUE(Intersection(disallowed_features, allowed_features).empty());
    for (auto feature : Union(disallowed_features, allowed_features)) {
      combined_features.emplace(static_cast<uint64_t>(feature));
    }
    // Add the removed features to the list.
    for (auto feature : removed_features) {
      ASSERT_FALSE(combined_features.contains(feature));
      combined_features.emplace(feature);
    }
    // Make a list of all the WebSchedulerTrackedFeatures indices.
    std::unordered_set<uint64_t> all_features;
    for (auto feature : blink::scheduler::WebSchedulerTrackedFeatures::All()) {
      all_features.emplace(static_cast<uint64_t>(feature));
    }
    SCOPED_TRACE(ccns_context == BackForwardCacheImpl::kInCCNSContext
                     ? "InCCNSContext"
                     : "NotInCCNSContext");
    EXPECT_THAT(combined_features,
                testing::UnorderedElementsAreArray(all_features));
  }
}

TEST_F(BackForwardCacheMetricsTest, PageWithFormsMetricsStoredRecorded) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      GetBasicBackForwardCacheFeatureForTesting(),
      GetDefaultDisabledBackForwardCacheFeaturesForTesting());

  const GURL url_with_form("http://foo1");
  const GURL url_without_form("http://foo2");

  // Set has form associated for the first URL.
  NavigationSimulator::NavigateAndCommitFromDocument(url_with_form,
                                                     main_test_rfh());
  BackForwardCache::SetHadFormDataAssociated(main_test_rfh()->GetPage());

  // Navigating away from |url_with_form| will put the page into BFCache.
  RenderFrameHostWrapper old_rfh(main_test_rfh());
  NavigationSimulator::NavigateAndCommitFromDocument(url_without_form,
                                                     main_test_rfh());
  EXPECT_EQ(old_rfh->GetLifecycleState(),
            RenderFrameHost::LifecycleState::kInBackForwardCache);
  histogram_tester_.ExpectBucketCount(
      kBackForwardCachePageWithFormStorableHistogramName,
      BackForwardCacheMetrics::PageWithFormStorable::kPageSeen, 1);
  histogram_tester_.ExpectBucketCount(
      kBackForwardCachePageWithFormStorableHistogramName,
      BackForwardCacheMetrics::PageWithFormStorable::kPageStored, 1);

  // Navigate back will restore the page from the BFCache.
  NavigationSimulator::GoBack(contents());
  EXPECT_EQ(old_rfh->GetLifecycleState(),
            RenderFrameHost::LifecycleState::kActive);
  histogram_tester_.ExpectBucketCount(
      kBackForwardCachePageWithFormRestoreResultHistogramName,
      BackForwardCacheMetrics::HistoryNavigationOutcome::kRestored, 1);
}

TEST_F(BackForwardCacheMetricsTest,
       PageWithFormsMetricsRecordedForSameSiteNavigation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      GetBasicBackForwardCacheFeatureForTesting(),
      GetDefaultDisabledBackForwardCacheFeaturesForTesting());

  const GURL url_with_form("http://foo1");
  const GURL url_with_form_with_handle("http://foo1#title");

  // Set has form associated for the first URL.
  NavigationSimulator::NavigateAndCommitFromDocument(url_with_form,
                                                     main_test_rfh());
  BackForwardCache::SetHadFormDataAssociated(main_test_rfh()->GetPage());

  NavigationSimulator::NavigateAndCommitFromDocument(url_with_form_with_handle,
                                                     main_test_rfh());

  // Record PageSeen without stored in cache.
  histogram_tester_.ExpectBucketCount(
      kBackForwardCachePageWithFormStorableHistogramName,
      BackForwardCacheMetrics::PageWithFormStorable::kPageSeen, 1);
  histogram_tester_.ExpectBucketCount(
      kBackForwardCachePageWithFormStorableHistogramName,
      BackForwardCacheMetrics::PageWithFormStorable::kPageStored, 0);
}

TEST_F(BackForwardCacheMetricsTest, PageWithFormsMetricsNotRestoreRecorded) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      GetBasicBackForwardCacheFeatureForTesting(),
      GetDefaultDisabledBackForwardCacheFeaturesForTesting());

  const GURL url_with_form("http://foo1");
  const GURL url_without_form("http://foo2");

  // Set has form associated for the first URL.
  NavigationSimulator::NavigateAndCommitFromDocument(url_with_form,
                                                     main_test_rfh());
  BackForwardCache::SetHadFormDataAssociated(main_test_rfh()->GetPage());

  // Navigating into another URL and go back and page can't be restored because
  // we flush the cache.
  RenderFrameHostWrapper old_rfh(main_test_rfh());
  NavigationSimulator::NavigateAndCommitFromDocument(url_without_form,
                                                     main_test_rfh());
  contents()->GetController().GetBackForwardCache().Flush();
  NavigationSimulator::GoBack(contents());
  EXPECT_TRUE(old_rfh.WaitUntilRenderFrameDeleted());
  histogram_tester_.ExpectBucketCount(
      kBackForwardCachePageWithFormRestoreResultHistogramName,
      BackForwardCacheMetrics::HistoryNavigationOutcome::kNotRestored, 1);
}

TEST_F(BackForwardCacheMetricsTest, PageWithFormsMetricsNotStore) {
  DisableBackForwardCacheForTesting(contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  const GURL url_with_form("http://foo1");
  const GURL url_without_form("http://foo2");

  // Set has form associated for the first URL.
  NavigationSimulator::NavigateAndCommitFromDocument(url_with_form,
                                                     main_test_rfh());
  BackForwardCache::SetHadFormDataAssociated(main_test_rfh()->GetPage());

  // Navigating away from |url_with_form| won't put the page into BFCache since
  // cache is disabled.
  NavigationSimulator::NavigateAndCommitFromDocument(url_without_form,
                                                     main_test_rfh());

  histogram_tester_.ExpectBucketCount(
      kBackForwardCachePageWithFormStorableHistogramName,
      BackForwardCacheMetrics::PageWithFormStorable::kPageSeen, 1);
  histogram_tester_.ExpectBucketCount(
      kBackForwardCachePageWithFormStorableHistogramName,
      BackForwardCacheMetrics::PageWithFormStorable::kPageStored, 0);
}
}  // namespace content
