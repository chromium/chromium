// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/extend.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/viz/common/frame_timing_details.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page/content_to_visible_time_reporter.h"

namespace blink {

constexpr char kBfcacheRestoreHistogram[] =
    "BackForwardCache.Restore.NavigationToFirstPaint";

constexpr base::TimeDelta kDuration = base::Milliseconds(42);
constexpr base::TimeDelta kOtherDuration = base::Milliseconds(4242);

// Combinations of tab states that log different histogram suffixes.
struct TabStateParams {
  bool has_saved_frames;
  bool destination_is_loaded;
  const char* histogram_suffix;
};

constexpr TabStateParams kTabStatesToTest[] = {
    // WithSavedFrames
    {
        .has_saved_frames = true,
        .destination_is_loaded = true,
        .histogram_suffix = "WithSavedFrames",
    },
    // NoSavedFrames_Loaded
    {
        .has_saved_frames = false,
        .destination_is_loaded = true,
        .histogram_suffix = "NoSavedFrames_Loaded",
    },
    // NoSavedFrames_NotLoaded
    {
        .has_saved_frames = false,
        .destination_is_loaded = false,
        .histogram_suffix = "NoSavedFrames_NotLoaded",
    },
};

class ContentToVisibleTimeReporterTest
    : public ::testing::TestWithParam<TabStateParams> {
 protected:
  ContentToVisibleTimeReporterTest() : tab_state_(GetParam()) {
    duration_histograms_.push_back("Browser.Tabs.TotalSwitchDuration3");
    duration_histograms_.push_back(base::StrCat(
        {"Browser.Tabs.TotalSwitchDuration3.", tab_state_.histogram_suffix}));

    incomplete_duration_histograms_.push_back(
        "Browser.Tabs.TotalIncompleteSwitchDuration3");
    incomplete_duration_histograms_.push_back(
        base::StrCat({"Browser.Tabs.TotalIncompleteSwitchDuration3.",
                      tab_state_.histogram_suffix}));

    result_histograms_.push_back("Browser.Tabs.TabSwitchResult3");
    result_histograms_.push_back(base::StrCat(
        {"Browser.Tabs.TabSwitchResult3.", tab_state_.histogram_suffix}));

    // Expect all histograms to be empty.
    ExpectHistogramsEmptyExcept({});
  }

  void ExpectHistogramsEmptyExcept(
      const std::vector<std::string>& histograms_with_values) {
    constexpr const char* kAllHistograms[] = {
        "Browser.Tabs.TotalSwitchDuration3",
        "Browser.Tabs.TotalSwitchDuration3.WithSavedFrames",
        "Browser.Tabs.TotalSwitchDuration3.NoSavedFrames_Loaded",
        "Browser.Tabs.TotalSwitchDuration3.NoSavedFrames_NotLoaded",
        "Browser.Tabs.TotalIncompleteSwitchDuration3",
        "Browser.Tabs.TotalIncompleteSwitchDuration3.WithSavedFrames",
        "Browser.Tabs.TotalIncompleteSwitchDuration3.NoSavedFrames_"
        "Loaded",
        "Browser.Tabs.TotalIncompleteSwitchDuration3.NoSavedFrames_"
        "NotLoaded",
        "Browser.Tabs.TabSwitchResult3",
        "Browser.Tabs.TabSwitchResult3.WithSavedFrames",
        "Browser.Tabs.TabSwitchResult3.NoSavedFrames_Loaded",
        "Browser.Tabs.TabSwitchResult3.NoSavedFrames_NotLoaded",
        // Non-tab switch.
        kBfcacheRestoreHistogram};
    std::vector<std::string> unexpected_histograms;
    for (const char* histogram : kAllHistograms) {
      if (!base::Contains(histograms_with_values, histogram))
        unexpected_histograms.push_back(histogram);
    }
    ExpectTotalSamples(unexpected_histograms, 0);
  }

  void ExpectTotalSamples(const std::vector<std::string>& histogram_names,
                          int expected_count) {
    for (const std::string& histogram_name : histogram_names) {
      SCOPED_TRACE(base::StringPrintf("Expect %d samples in %s.",
                                      expected_count, histogram_name.c_str()));
      EXPECT_EQ(static_cast<int>(
                    histogram_tester_.GetAllSamples(histogram_name).size()),
                expected_count);
    }
  }

  void ExpectTimeBucketCounts(const std::vector<std::string>& histogram_names,
                              base::TimeDelta value,
                              int count) {
    for (const std::string& histogram_name : histogram_names) {
      histogram_tester_.ExpectTimeBucketCount(histogram_name, value, count);
    }
  }

  void ExpectResultBucketCounts(
      const std::vector<std::string>& histogram_names,
      ContentToVisibleTimeReporter::TabSwitchResult value,
      int count) {
    for (const std::string& histogram_name : histogram_names) {
      histogram_tester_.ExpectBucketCount(histogram_name, value, count);
    }
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ContentToVisibleTimeReporter tab_switch_time_recorder_;
  base::HistogramTester histogram_tester_;
  TabStateParams tab_state_;

  // Expected histogram names to be logged for the given TabStateParams.
  std::vector<std::string> duration_histograms_;
  std::vector<std::string> incomplete_duration_histograms_;
  std::vector<std::string> result_histograms_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ContentToVisibleTimeReporterTest,
                         ::testing::ValuesIn(kTabStatesToTest));

// Time is properly recorded to histogram if we have a proper matching
// TabWasShown and callback execution.
TEST_P(ContentToVisibleTimeReporterTest, TimeIsRecorded) {
  const auto start = base::TimeTicks::Now();
  auto callback = tab_switch_time_recorder_.TabWasShown(
      tab_state_.has_saved_frames,
      blink::mojom::RecordContentToVisibleTimeRequest::New(
          start, tab_state_.destination_is_loaded,
          /*show_reason_tab_switching=*/true,
          /*show_reason_bfcache_restore=*/false,
          /*show_reason_unfold=*/false));
  const auto end = start + kDuration;
  viz::FrameTimingDetails details;
  details.presentation_feedback.timestamp = end;
  std::move(callback).Run(details);

  std::vector<std::string> expected_histograms;
  base::Extend(expected_histograms, duration_histograms_);
  base::Extend(expected_histograms, result_histograms_);
  ExpectHistogramsEmptyExcept(expected_histograms);

  // Duration.
  ExpectTotalSamples(duration_histograms_, 1);
  ExpectTimeBucketCounts(duration_histograms_, kDuration, 1);

  // Result.
  ExpectTotalSamples(result_histograms_, 1);
  ExpectResultBucketCounts(
      result_histograms_,
      ContentToVisibleTimeReporter::TabSwitchResult::kSuccess, 1);
}

// An incomplete tab switch is reported when no frame is shown before a tab is
// hidden.
TEST_P(ContentToVisibleTimeReporterTest, HideBeforePresentFrame) {
  const auto start1 = base::TimeTicks::Now();
  auto callback1 = tab_switch_time_recorder_.TabWasShown(
      tab_state_.has_saved_frames,
      blink::mojom::RecordContentToVisibleTimeRequest::New(
          start1, tab_state_.destination_is_loaded,
          /*show_reason_tab_switching=*/true,
          /*show_reason_bfcache_restore=*/false,
          /*show_reason_unfold=*/false));

  task_environment_.FastForwardBy(kDuration);
  tab_switch_time_recorder_.TabWasHidden();

  std::vector<std::string> expected_histograms;
  base::Extend(expected_histograms, result_histograms_);
  base::Extend(expected_histograms, incomplete_duration_histograms_);
  ExpectHistogramsEmptyExcept(expected_histograms);

  // Duration.
  ExpectTotalSamples(incomplete_duration_histograms_, 1);
  ExpectTimeBucketCounts(incomplete_duration_histograms_, kDuration, 1);

  // Result.
  ExpectTotalSamples(result_histograms_, 1);
  ExpectResultBucketCounts(
      result_histograms_,
      ContentToVisibleTimeReporter::TabSwitchResult::kIncomplete, 1);

  const auto start2 = base::TimeTicks::Now();
  auto callback2 = tab_switch_time_recorder_.TabWasShown(
      tab_state_.has_saved_frames,
      blink::mojom::RecordContentToVisibleTimeRequest::New(
          start2, tab_state_.destination_is_loaded,
          /*show_reason_tab_switching=*/true,
          /*show_reason_bfcache_restore=*/false,
          /*show_reason_unfold=*/false));
  const auto end2 = start2 + kOtherDuration;
  viz::FrameTimingDetails details;
  details.presentation_feedback.timestamp = end2;
  std::move(callback2).Run(details);

  // Now the tab switch completes, and adds a duration histogram.
  base::Extend(expected_histograms, duration_histograms_);
  ExpectHistogramsEmptyExcept(expected_histograms);

  // Duration.
  ExpectTotalSamples(incomplete_duration_histograms_, 1);
  ExpectTimeBucketCounts(incomplete_duration_histograms_, kDuration, 1);
  ExpectTotalSamples(duration_histograms_, 1);
  ExpectTimeBucketCounts(duration_histograms_, kOtherDuration, 1);

  // Result.
  ExpectTotalSamples(result_histograms_, 2);
  ExpectResultBucketCounts(
      result_histograms_,
      ContentToVisibleTimeReporter::TabSwitchResult::kIncomplete, 1);
  ExpectResultBucketCounts(
      result_histograms_,
      ContentToVisibleTimeReporter::TabSwitchResult::kSuccess, 1);
}

// If TabWasHidden is not called an incomplete tab switch is reported.
// TODO(crbug.com/1289266): Find and remove all cases where TabWasHidden is not
// called.
TEST_P(ContentToVisibleTimeReporterTest, MissingTabWasHidden) {
  const auto start1 = base::TimeTicks::Now();
  auto callback1 = tab_switch_time_recorder_.TabWasShown(
      tab_state_.has_saved_frames,
      blink::mojom::RecordContentToVisibleTimeRequest::New(
          start1, tab_state_.destination_is_loaded,
          /*show_reason_tab_switching=*/true,
          /*show_reason_bfcache_restore=*/false,
          /*show_reason_unfold=*/false));

  task_environment_.FastForwardBy(kDuration);

  ExpectHistogramsEmptyExcept({});

  const auto start2 = base::TimeTicks::Now();
  auto callback2 = tab_switch_time_recorder_.TabWasShown(
      tab_state_.has_saved_frames,
      blink::mojom::RecordContentToVisibleTimeRequest::New(
          start2, tab_state_.destination_is_loaded,
          /*show_reason_tab_switching=*/true,
          /*show_reason_bfcache_restore=*/false,
          /*show_reason_unfold=*/false));
  const auto end2 = start2 + kOtherDuration;
  viz::FrameTimingDetails details;
  details.presentation_feedback.timestamp = end2;
  std::move(callback2).Run(details);

  // IncompleteDuration should be logged for the first TabWasShown, and Duration
  // for the second.
  std::vector<std::string> expected_histograms;
  base::Extend(expected_histograms, duration_histograms_);
  base::Extend(expected_histograms, result_histograms_);
  base::Extend(expected_histograms, incomplete_duration_histograms_);
  ExpectHistogramsEmptyExcept(expected_histograms);

  // Duration.
  ExpectTotalSamples({incomplete_duration_histograms_}, 1);
  ExpectTimeBucketCounts({incomplete_duration_histograms_}, kDuration, 1);
  ExpectTotalSamples({duration_histograms_}, 1);
  ExpectTimeBucketCounts({duration_histograms_}, kOtherDuration, 1);

  // Result.
  ExpectTotalSamples({result_histograms_}, 2);
  ExpectResultBucketCounts(
      {result_histograms_},
      ContentToVisibleTimeReporter::TabSwitchResult::kMissedTabHide, 1);
  ExpectResultBucketCounts(
      {result_histograms_},
      ContentToVisibleTimeReporter::TabSwitchResult::kSuccess, 1);
}

// Time is properly recorded to histogram when we have bfcache restore event.
TEST_P(ContentToVisibleTimeReporterTest, BfcacheRestoreTimeIsRecorded) {
  const auto start = base::TimeTicks::Now();
  auto callback = tab_switch_time_recorder_.TabWasShown(
      tab_state_.has_saved_frames,
      blink::mojom::RecordContentToVisibleTimeRequest::New(
          start, tab_state_.destination_is_loaded,
          /*show_reason_tab_switching=*/false,
          /*show_reason_bfcache_restore=*/true,
          /*show_reason_unfold=*/false));
  const auto end = start + kDuration;
  viz::FrameTimingDetails details;
  details.presentation_feedback.timestamp = end;
  std::move(callback).Run(details);

  ExpectHistogramsEmptyExcept({kBfcacheRestoreHistogram});

  // Bfcache restore.
  ExpectTotalSamples({kBfcacheRestoreHistogram}, 1);
  ExpectTimeBucketCounts({kBfcacheRestoreHistogram}, kDuration, 1);
}

// Time is properly recorded to histogram when we have unoccluded event
// and some other events too.
TEST_P(ContentToVisibleTimeReporterTest,
       TimeIsRecordedWithSavedFramesPlusBfcacheRestoreTimeIsRecorded) {
  const auto start = base::TimeTicks::Now();
  auto callback = tab_switch_time_recorder_.TabWasShown(
      tab_state_.has_saved_frames,
      blink::mojom::RecordContentToVisibleTimeRequest::New(
          start, tab_state_.destination_is_loaded,
          /*show_reason_tab_switching=*/true,
          /*show_reason_bfcache_restore=*/true,
          /*show_reason_unfold=*/false));
  const auto end = start + kDuration;
  viz::FrameTimingDetails details;
  details.presentation_feedback.timestamp = end;
  std::move(callback).Run(details);

  std::vector<std::string> expected_histograms{kBfcacheRestoreHistogram};
  base::Extend(expected_histograms, duration_histograms_);
  base::Extend(expected_histograms, result_histograms_);
  ExpectHistogramsEmptyExcept(expected_histograms);

  // Duration.
  ExpectTotalSamples(duration_histograms_, 1);
  ExpectTimeBucketCounts(duration_histograms_, kDuration, 1);

  // Result.
  ExpectTotalSamples(result_histograms_, 1);
  ExpectResultBucketCounts(
      result_histograms_,
      ContentToVisibleTimeReporter::TabSwitchResult::kSuccess, 1);

  // Bfcache restore.
  ExpectTotalSamples({kBfcacheRestoreHistogram}, 1);
  ExpectTimeBucketCounts({kBfcacheRestoreHistogram}, kDuration, 1);
}

}  // namespace blink
