// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <strstream>
#include <utility>

#include "base/containers/contains.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page/content_to_visible_time_reporter.h"
#include "ui/gfx/presentation_feedback.h"

namespace blink {

constexpr char kWebContentsUnOccludedHistogram[] =
    "Aura.WebContentsWindowUnOccludedTime";
constexpr char kBfcacheRestoreHistogram[] =
    "BackForwardCache.Restore.NavigationToFirstPaint";

constexpr base::TimeDelta kDuration = base::Milliseconds(42);
constexpr base::TimeDelta kOtherDuration = base::Milliseconds(4242);

// Combinations of tab states that log different histogram suffixes.
struct TabStateParams {
  bool has_saved_frames;
  bool destination_is_loaded;
  const char* duration_histogram;
  const char* incomplete_duration_histogram;
  const char* result_histogram;
};

constexpr TabStateParams kTabStatesToTest[] = {
    // WithSavedFrames
    {.has_saved_frames = true,
     .destination_is_loaded = true,
     .duration_histogram = "Browser.Tabs.TotalSwitchDuration.WithSavedFrames",
     .incomplete_duration_histogram =
         "Browser.Tabs.TotalIncompleteSwitchDuration.WithSavedFrames",
     .result_histogram = "Browser.Tabs.TabSwitchResult.WithSavedFrames"},
    // NoSavedFrames_Loaded
    {.has_saved_frames = false,
     .destination_is_loaded = true,
     .duration_histogram =
         "Browser.Tabs.TotalSwitchDuration.NoSavedFrames_Loaded",
     .incomplete_duration_histogram =
         "Browser.Tabs.TotalIncompleteSwitchDuration.NoSavedFrames_Loaded",
     .result_histogram = "Browser.Tabs.TabSwitchResult.NoSavedFrames_Loaded"},
    // NoSavedFrames_NotLoaded
    {.has_saved_frames = false,
     .destination_is_loaded = false,
     .duration_histogram =
         "Browser.Tabs.TotalSwitchDuration.NoSavedFrames_NotLoaded",
     .incomplete_duration_histogram =
         "Browser.Tabs.TotalIncompleteSwitchDuration.NoSavedFrames_"
         "NotLoaded",
     .result_histogram =
         "Browser.Tabs.TabSwitchResult.NoSavedFrames_NotLoaded"},
};

class ContentToVisibleTimeReporterTest
    : public ::testing::TestWithParam<TabStateParams> {
 protected:
  ContentToVisibleTimeReporterTest() : tab_state_(GetParam()) {
    // Expect all histograms to be empty.
    ExpectHistogramsEmptyExcept({});
  }

  void ExpectHistogramsEmptyExcept(
      std::vector<const char*> histograms_with_values) {
    constexpr const char* kAllHistograms[] = {
        "Browser.Tabs.TotalSwitchDuration.WithSavedFrames",
        "Browser.Tabs.TotalSwitchDuration.NoSavedFrames_Loaded",
        "Browser.Tabs.TotalSwitchDuration.NoSavedFrames_NotLoaded",
        "Browser.Tabs.TotalIncompleteSwitchDuration.WithSavedFrames",
        "Browser.Tabs.TotalIncompleteSwitchDuration.NoSavedFrames_Loaded",
        "Browser.Tabs.TotalIncompleteSwitchDuration.NoSavedFrames_NotLoaded",
        "Browser.Tabs.TabSwitchResult.WithSavedFrames",
        "Browser.Tabs.TabSwitchResult.NoSavedFrames_Loaded",
        "Browser.Tabs.TabSwitchResult.NoSavedFrames_NotLoaded",
        kWebContentsUnOccludedHistogram,
        kBfcacheRestoreHistogram};
    for (const char* histogram : kAllHistograms) {
      if (!base::Contains(histograms_with_values, histogram))
        ExpectTotalSamples(histogram, 0);
    }
  }

  void ExpectTotalSamples(const char* histogram_name, int expected_count) {
    SCOPED_TRACE(base::StringPrintf("Expect %d samples in %s.", expected_count,
                                    histogram_name));
    EXPECT_EQ(static_cast<int>(
                  histogram_tester_.GetAllSamples(histogram_name).size()),
              expected_count);
  }

  void ExpectTimeBucketCount(const char* histogram_name,
                             base::TimeDelta value,
                             int count) {
    histogram_tester_.ExpectTimeBucketCount(histogram_name, value, count);
  }

  void ExpectResultBucketCount(
      const char* histogram_name,
      ContentToVisibleTimeReporter::TabSwitchResult value,
      int count) {
    histogram_tester_.ExpectBucketCount(histogram_name, value, count);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ContentToVisibleTimeReporter tab_switch_time_recorder_;
  base::HistogramTester histogram_tester_;
  TabStateParams tab_state_;
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
          /* show_reason_tab_switching */ true,
          /* show_reason_unoccluded */ false,
          /* show_reason_bfcache_restore */ false));
  const auto end = start + kDuration;
  auto presentation_feedback = gfx::PresentationFeedback(
      end, end - start, gfx::PresentationFeedback::Flags::kHWCompletion);
  std::move(callback).Run(presentation_feedback);

  ExpectHistogramsEmptyExcept(
      {tab_state_.duration_histogram, tab_state_.result_histogram});

  // Duration.
  ExpectTotalSamples(tab_state_.duration_histogram, 1);
  ExpectTimeBucketCount(tab_state_.duration_histogram, kDuration, 1);

  // Result.
  ExpectTotalSamples(tab_state_.result_histogram, 1);
  ExpectResultBucketCount(
      tab_state_.result_histogram,
      ContentToVisibleTimeReporter::TabSwitchResult::kSuccess, 1);
}

// A failure should be reported if gfx::PresentationFeedback contains the
// kFailure flag.
TEST_P(ContentToVisibleTimeReporterTest, PresentationFailure) {
  const auto start = base::TimeTicks::Now();
  auto callback = tab_switch_time_recorder_.TabWasShown(
      tab_state_.has_saved_frames,
      blink::mojom::RecordContentToVisibleTimeRequest::New(
          start, tab_state_.destination_is_loaded,
          /* show_reason_tab_switching */ true,
          /* show_reason_unoccluded */ false,
          /* show_reason_bfcache_restore */ false));
  std::move(callback).Run(gfx::PresentationFeedback::Failure());

  ExpectHistogramsEmptyExcept({tab_state_.result_histogram});

  // Result (no duration is recorded on presentation failure).
  ExpectTotalSamples(tab_state_.result_histogram, 1);
  ExpectResultBucketCount(
      tab_state_.result_histogram,
      ContentToVisibleTimeReporter::TabSwitchResult::kPresentationFailure, 1);
}

// An incomplete tab switch is reported when no frame is shown before a tab is
// hidden.
TEST_P(ContentToVisibleTimeReporterTest, HideBeforePresentFrame) {
  const auto start1 = base::TimeTicks::Now();
  auto callback1 = tab_switch_time_recorder_.TabWasShown(
      tab_state_.has_saved_frames,
      blink::mojom::RecordContentToVisibleTimeRequest::New(
          start1, tab_state_.destination_is_loaded,
          /* show_reason_tab_switching */ true,
          /* show_reason_unoccluded */ false,
          /* show_reason_bfcache_restore */ false));

  task_environment_.FastForwardBy(kDuration);
  tab_switch_time_recorder_.TabWasHidden();

  ExpectHistogramsEmptyExcept(
      {tab_state_.result_histogram, tab_state_.incomplete_duration_histogram});

  // Duration.
  ExpectTotalSamples(tab_state_.incomplete_duration_histogram, 1);
  ExpectTimeBucketCount(tab_state_.incomplete_duration_histogram, kDuration, 1);

  // Result.
  ExpectTotalSamples(tab_state_.result_histogram, 1);
  ExpectResultBucketCount(
      tab_state_.result_histogram,
      ContentToVisibleTimeReporter::TabSwitchResult::kIncomplete, 1);

  const auto start2 = base::TimeTicks::Now();
  auto callback2 = tab_switch_time_recorder_.TabWasShown(
      tab_state_.has_saved_frames,
      blink::mojom::RecordContentToVisibleTimeRequest::New(
          start2, tab_state_.destination_is_loaded,
          /* show_reason_tab_switching */ true,
          /* show_reason_unoccluded */ false,
          /* show_reason_bfcache_restore */ false));
  const auto end2 = start2 + kOtherDuration;
  auto presentation_feedback = gfx::PresentationFeedback(
      end2, end2 - start2, gfx::PresentationFeedback::Flags::kHWCompletion);
  std::move(callback2).Run(presentation_feedback);

  ExpectHistogramsEmptyExcept({tab_state_.duration_histogram,
                               tab_state_.result_histogram,
                               tab_state_.incomplete_duration_histogram});

  // Duration.
  ExpectTotalSamples(tab_state_.incomplete_duration_histogram, 1);
  ExpectTimeBucketCount(tab_state_.incomplete_duration_histogram, kDuration, 1);
  ExpectTotalSamples(tab_state_.duration_histogram, 1);
  ExpectTimeBucketCount(tab_state_.duration_histogram, kOtherDuration, 1);

  // Result.
  ExpectTotalSamples(tab_state_.result_histogram, 2);
  ExpectResultBucketCount(
      tab_state_.result_histogram,
      ContentToVisibleTimeReporter::TabSwitchResult::kIncomplete, 1);
  ExpectResultBucketCount(
      tab_state_.result_histogram,
      ContentToVisibleTimeReporter::TabSwitchResult::kSuccess, 1);
}

// Time is properly recorded to histogram when we have unoccluded event.
TEST_P(ContentToVisibleTimeReporterTest, UnoccludedTimeIsRecorded) {
  const auto start = base::TimeTicks::Now();
  auto callback = tab_switch_time_recorder_.TabWasShown(
      tab_state_.has_saved_frames,
      blink::mojom::RecordContentToVisibleTimeRequest::New(
          start, tab_state_.destination_is_loaded,
          /* show_reason_tab_switching */ false,
          /* show_reason_unoccluded */ true,
          /* show_reason_bfcache_restore */ false));
  const auto end = start + kDuration;
  auto presentation_feedback = gfx::PresentationFeedback(
      end, end - start, gfx::PresentationFeedback::Flags::kHWCompletion);
  std::move(callback).Run(presentation_feedback);

  ExpectHistogramsEmptyExcept({kWebContentsUnOccludedHistogram});

  // UnOccluded.
  ExpectTotalSamples(kWebContentsUnOccludedHistogram, 1);
  ExpectTimeBucketCount(kWebContentsUnOccludedHistogram, kDuration, 1);
}

// Time is properly recorded to histogram when we have unoccluded event
// and some other events too.
TEST_P(ContentToVisibleTimeReporterTest,
       TimeIsRecordedWithSavedFramesPlusUnoccludedTimeIsRecorded) {
  const auto start = base::TimeTicks::Now();
  auto callback = tab_switch_time_recorder_.TabWasShown(
      tab_state_.has_saved_frames,
      blink::mojom::RecordContentToVisibleTimeRequest::New(
          start, tab_state_.destination_is_loaded,
          /* show_reason_tab_switching */ true,
          /* show_reason_unoccluded */ true,
          /* show_reason_bfcache_restore */ false));
  const auto end = start + kDuration;
  auto presentation_feedback = gfx::PresentationFeedback(
      end, end - start, gfx::PresentationFeedback::Flags::kHWCompletion);
  std::move(callback).Run(presentation_feedback);

  ExpectHistogramsEmptyExcept({tab_state_.duration_histogram,
                               tab_state_.result_histogram,
                               kWebContentsUnOccludedHistogram});

  // Duration.
  ExpectTotalSamples(tab_state_.duration_histogram, 1);
  ExpectTimeBucketCount(tab_state_.duration_histogram, kDuration, 1);

  // Result.
  ExpectTotalSamples(tab_state_.result_histogram, 1);
  ExpectResultBucketCount(
      tab_state_.result_histogram,
      ContentToVisibleTimeReporter::TabSwitchResult::kSuccess, 1);

  // UnOccluded.
  ExpectTotalSamples(kWebContentsUnOccludedHistogram, 1);
  ExpectTimeBucketCount(kWebContentsUnOccludedHistogram, kDuration, 1);
}

// Time is properly recorded to histogram when we have bfcache restore event.
TEST_P(ContentToVisibleTimeReporterTest, BfcacheRestoreTimeIsRecorded) {
  const auto start = base::TimeTicks::Now();
  auto callback = tab_switch_time_recorder_.TabWasShown(
      tab_state_.has_saved_frames,
      blink::mojom::RecordContentToVisibleTimeRequest::New(
          start, tab_state_.destination_is_loaded,
          /* show_reason_tab_switching */ false,
          /* show_reason_unoccluded */ false,
          /* show_reason_bfcache_restore */ true));
  const auto end = start + kDuration;
  auto presentation_feedback = gfx::PresentationFeedback(
      end, end - start, gfx::PresentationFeedback::Flags::kHWCompletion);
  std::move(callback).Run(presentation_feedback);

  ExpectHistogramsEmptyExcept({kBfcacheRestoreHistogram});

  // Bfcache restore.
  ExpectTotalSamples(kBfcacheRestoreHistogram, 1);
  ExpectTimeBucketCount(kBfcacheRestoreHistogram, kDuration, 1);
}

}  // namespace blink
