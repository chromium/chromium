// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class BackForwardCacheMetricsTest : public RenderViewHostImplTestHarness,
                                    public WebContentsObserver {
 public:
  BackForwardCacheMetricsTest() {}

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    WebContents* web_contents = RenderViewHostImplTestHarness::web_contents();
    ASSERT_TRUE(web_contents);  // The WebContents should be created by now.
    WebContentsObserver::Observe(web_contents);

    // Ensure that the time is non-null.
    clock_.Advance(base::TimeDelta::FromMilliseconds(5));
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
  clock_.Advance(base::TimeDelta::FromMilliseconds(0b1));
  NavigationSimulator::NavigateAndCommitFromDocument(url2, main_test_rfh());
  clock_.Advance(base::TimeDelta::FromMilliseconds(0b10));
  NavigationSimulator::NavigateAndCommitFromDocument(url3, main_test_rfh());
  clock_.Advance(base::TimeDelta::FromMilliseconds(0b100));
  NavigationSimulator::GoBack(contents());
  clock_.Advance(base::TimeDelta::FromMilliseconds(0b1000));
  NavigationSimulator::GoBack(contents());
  clock_.Advance(base::TimeDelta::FromMilliseconds(0b10000));
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
  clock_.Advance(base::TimeDelta::FromHours(5) +
                 base::TimeDelta::FromMilliseconds(50));
  NavigationSimulator::GoBack(contents());

  ASSERT_EQ(navigation_ids_.size(), static_cast<size_t>(3));
  ukm::SourceId id3 = ToSourceId(navigation_ids_[2]);

  std::string time_away = "TimeSinceNavigatedAwayFromDocument";

  // The original interval of 5h + 50ms is clamped to just 5h.
  EXPECT_THAT(
      recorder_.GetEntries("HistoryNavigation", {time_away}),
      testing::ElementsAre(UkmEntry{
          id3, {{time_away, base::TimeDelta::FromHours(5).InMilliseconds()}}}));
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
    clock_.Advance(base::TimeDelta::FromMilliseconds(0b1));
    simulator->Commit();
  }

  clock_.Advance(base::TimeDelta::FromMilliseconds(0b10));

  {
    auto simulator =
        NavigationSimulator::CreateRendererInitiated(url2, main_test_rfh());
    simulator->Start();
    clock_.Advance(base::TimeDelta::FromMilliseconds(0b100));
    simulator->Commit();
  }

  clock_.Advance(base::TimeDelta::FromMilliseconds(0b1000));

  {
    auto simulator =
        NavigationSimulator::CreateHistoryNavigation(-1, contents());
    simulator->Start();
    clock_.Advance(base::TimeDelta::FromMilliseconds(0b10000));
    simulator->Commit();
  }

  ASSERT_EQ(navigation_ids_.size(), static_cast<size_t>(3));
  ukm::SourceId id3 = ToSourceId(navigation_ids_[2]);

  std::string time_away = "TimeSinceNavigatedAwayFromDocument";

  EXPECT_THAT(recorder_.GetEntries("HistoryNavigation", {time_away}),
              testing::ElementsAre(UkmEntry{id3, {{time_away, 0b1000}}}));
}

TEST_F(BackForwardCacheMetricsTest, TimeRecordedWhenRendererIsKilled) {
  // Need to enable back-forward cache to make sure a page is put into the
  // cache.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kBackForwardCache}, {});

  base::HistogramTester histogram_tester;

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  // Go to foo1.
  NavigationSimulator::NavigateAndCommitFromDocument(url1, main_test_rfh());
  clock_.Advance(base::TimeDelta::FromMilliseconds(0b1));
  TestRenderFrameHost* old_main_frame_host = main_test_rfh();

  // Go to foo2. Foo1 will be in the back-forward cache.
  NavigationSimulator::NavigateAndCommitFromDocument(url2, main_test_rfh());
  clock_.Advance(base::TimeDelta::FromMilliseconds(0b10));

  // Kill the renderer.
  old_main_frame_host->GetProcess()->SimulateRenderProcessExit(
      base::TERMINATION_STATUS_PROCESS_WAS_KILLED, 1);
  clock_.Advance(base::TimeDelta::FromMilliseconds(0b100));

  NavigationSimulator::GoBack(contents());
  clock_.Advance(base::TimeDelta::FromMilliseconds(0b1000));

  const char kTimeUntilProcessKilled[] =
      "BackForwardCache.Eviction.TimeUntilProcessKilled";

  // The expected recorded time is between when the last navigation happened and
  // when the renderer is killed.
  EXPECT_THAT(histogram_tester.GetAllSamples(kTimeUntilProcessKilled),
              testing::ElementsAre(base::Bucket(0b10, 1)));
}

}  // namespace content
