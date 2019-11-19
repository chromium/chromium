// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "content/common/tab_switch_time_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/presentation_feedback.h"

namespace content {

constexpr char kDurationWithSavedFramesHistogram[] =
    "Browser.Tabs.TotalSwitchDuration.WithSavedFrames";
constexpr char kDurationNoSavedFramesHistogram[] =
    "Browser.Tabs.TotalSwitchDuration.NoSavedFrames_Loaded";
constexpr char kDurationNoSavedFramesNotFrozenHistogram[] =
    "Browser.Tabs.TotalSwitchDuration.NoSavedFrames_Loaded_NotFrozen";
constexpr char kDurationNoSavedFramesFrozenHistogram[] =
    "Browser.Tabs.TotalSwitchDuration.NoSavedFrames_Loaded_Frozen";
constexpr char kDurationNoSavedFramesUnloadedHistogram[] =
    "Browser.Tabs.TotalSwitchDuration.NoSavedFrames_NotLoaded";

constexpr char kIncompleteDurationWithSavedFramesHistogram[] =
    "Browser.Tabs.TotalIncompleteSwitchDuration.WithSavedFrames";
constexpr char kIncompleteDurationNoSavedFramesHistogram[] =
    "Browser.Tabs.TotalIncompleteSwitchDuration.NoSavedFrames_Loaded";
constexpr char kIncompleteDurationNoSavedFramesNotFrozenHistogram[] =
    "Browser.Tabs.TotalIncompleteSwitchDuration.NoSavedFrames_Loaded_NotFrozen";
constexpr char kIncompleteDurationNoSavedFramesFrozenHistogram[] =
    "Browser.Tabs.TotalIncompleteSwitchDuration.NoSavedFrames_Loaded_Frozen";
constexpr char kIncompleteDurationNoSavedFramesUnloadedHistogram[] =
    "Browser.Tabs.TotalIncompleteSwitchDuration.NoSavedFrames_NotLoaded";

constexpr char kResultWithSavedFramesHistogram[] =
    "Browser.Tabs.TabSwitchResult.WithSavedFrames";
constexpr char kResultNoSavedFramesHistogram[] =
    "Browser.Tabs.TabSwitchResult.NoSavedFrames_Loaded";
constexpr char kResultNoSavedFramesNotFrozenHistogram[] =
    "Browser.Tabs.TabSwitchResult.NoSavedFrames_Loaded_NotFrozen";
constexpr char kResultNoSavedFramesFrozenHistogram[] =
    "Browser.Tabs.TabSwitchResult.NoSavedFrames_Loaded_Frozen";
constexpr char kResultNoSavedFramesUnloadedHistogram[] =
    "Browser.Tabs.TabSwitchResult.NoSavedFrames_NotLoaded";

constexpr base::TimeDelta kDuration = base::TimeDelta::FromMilliseconds(42);
constexpr base::TimeDelta kOtherDuration =
    base::TimeDelta::FromMilliseconds(4242);

class TabSwitchTimeRecorderTest : public testing::Test {
 protected:
  void SetUp() override {
    // Expect all histograms to be empty.
    ExpectHistogramsEmptyExcept({});
  }

  void ExpectHistogramsEmptyExcept(
      std::vector<const char*> histograms_with_values) {
    constexpr const char* kAllHistograms[] = {
        kDurationWithSavedFramesHistogram,
        kDurationNoSavedFramesHistogram,
        kDurationNoSavedFramesNotFrozenHistogram,
        kDurationNoSavedFramesFrozenHistogram,
        kDurationNoSavedFramesUnloadedHistogram,
        kIncompleteDurationWithSavedFramesHistogram,
        kIncompleteDurationNoSavedFramesHistogram,
        kIncompleteDurationNoSavedFramesNotFrozenHistogram,
        kIncompleteDurationNoSavedFramesFrozenHistogram,
        kIncompleteDurationNoSavedFramesUnloadedHistogram,
        kResultWithSavedFramesHistogram,
        kResultNoSavedFramesHistogram,
        kResultNoSavedFramesNotFrozenHistogram,
        kResultNoSavedFramesFrozenHistogram,
        kResultNoSavedFramesUnloadedHistogram};
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

  void ExpectResultBucketCount(const char* histogram_name,
                               TabSwitchTimeRecorder::TabSwitchResult value,
                               int count) {
    histogram_tester_.ExpectBucketCount(histogram_name, value, count);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TabSwitchTimeRecorder tab_switch_time_recorder_;
  base::HistogramTester histogram_tester_;
};

// Time is properly recorded to histogram when we have saved frames and if we
// have a proper matching TabWasShown and callback execution.
TEST_F(TabSwitchTimeRecorderTest, TimeIsRecordedWithSavedFrames) {
  const auto start = base::TimeTicks::Now();
  auto callback = tab_switch_time_recorder_.TabWasShown(
      true /* has_saved_frames */,
      {start, /* destination_is_loaded */ true,
       /* destination_is_frozen */ false},
      start);
  const auto end = start + kDuration;
  auto presentation_feedback = gfx::PresentationFeedback(
      end, end - start, gfx::PresentationFeedback::Flags::kHWCompletion);
  std::move(callback).Run(presentation_feedback);

  ExpectHistogramsEmptyExcept(
      {kDurationWithSavedFramesHistogram, kResultWithSavedFramesHistogram});

  // Duration.
  ExpectTotalSamples(kDurationWithSavedFramesHistogram, 1);
  ExpectTimeBucketCount(kDurationWithSavedFramesHistogram, kDuration, 1);

  // Result.
  ExpectTotalSamples(kResultWithSavedFramesHistogram, 1);
  ExpectResultBucketCount(kResultWithSavedFramesHistogram,
                          TabSwitchTimeRecorder::TabSwitchResult::kSuccess, 1);
}

// Time is properly recorded to histogram when we have no saved frame and if we
// have a proper matching TabWasShown and callback execution.
TEST_F(TabSwitchTimeRecorderTest, TimeIsRecordedNoSavedFrameNotFrozen) {
  const auto start = base::TimeTicks::Now();
  auto callback = tab_switch_time_recorder_.TabWasShown(
      false /* has_saved_frames */,
      {start, /* destination_is_loaded */ true,
       /* destination_is_frozen */ false},
      start);
  const auto end = start + kDuration;
  auto presentation_feedback = gfx::PresentationFeedback(
      end, end - start, gfx::PresentationFeedback::Flags::kHWCompletion);
  std::move(callback).Run(presentation_feedback);

  ExpectHistogramsEmptyExcept({kDurationNoSavedFramesHistogram,
                               kDurationNoSavedFramesNotFrozenHistogram,
                               kResultNoSavedFramesHistogram,
                               kResultNoSavedFramesNotFrozenHistogram});

  // Duration.
  ExpectTotalSamples(kDurationNoSavedFramesHistogram, 1);
  ExpectTimeBucketCount(kDurationNoSavedFramesHistogram, kDuration, 1);
  ExpectTotalSamples(kDurationNoSavedFramesNotFrozenHistogram, 1);
  ExpectTimeBucketCount(kDurationNoSavedFramesNotFrozenHistogram, kDuration, 1);

  // Result.
  ExpectTotalSamples(kResultNoSavedFramesHistogram, 1);
  ExpectResultBucketCount(kResultNoSavedFramesHistogram,
                          TabSwitchTimeRecorder::TabSwitchResult::kSuccess, 1);
  ExpectTotalSamples(kResultNoSavedFramesNotFrozenHistogram, 1);
  ExpectResultBucketCount(kResultNoSavedFramesNotFrozenHistogram,
                          TabSwitchTimeRecorder::TabSwitchResult::kSuccess, 1);
}

// Same as TimeIsRecordedNoSavedFrame but with the destination frame frozen.
TEST_F(TabSwitchTimeRecorderTest, TimeIsRecordedNoSavedFrameFrozen) {
  const auto start = base::TimeTicks::Now();
  auto callback = tab_switch_time_recorder_.TabWasShown(
      false /* has_saved_frames */,
      {start, /* destination_is_loaded */ true,
       /* destination_is_frozen */ true},
      start);
  const auto end = start + kDuration;
  auto presentation_feedback = gfx::PresentationFeedback(
      end, end - start, gfx::PresentationFeedback::Flags::kHWCompletion);
  std::move(callback).Run(presentation_feedback);

  ExpectHistogramsEmptyExcept(
      {kDurationNoSavedFramesHistogram, kDurationNoSavedFramesFrozenHistogram,
       kResultNoSavedFramesHistogram, kResultNoSavedFramesFrozenHistogram});

  // Duration.
  ExpectTotalSamples(kDurationNoSavedFramesHistogram, 1);
  ExpectTimeBucketCount(kDurationNoSavedFramesHistogram, kDuration, 1);
  ExpectTotalSamples(kDurationNoSavedFramesFrozenHistogram, 1);
  ExpectTimeBucketCount(kDurationNoSavedFramesFrozenHistogram, kDuration, 1);

  // Result.
  ExpectTotalSamples(kResultNoSavedFramesHistogram, 1);
  ExpectResultBucketCount(kResultNoSavedFramesHistogram,
                          TabSwitchTimeRecorder::TabSwitchResult::kSuccess, 1);
  ExpectTotalSamples(kResultNoSavedFramesFrozenHistogram, 1);
  ExpectResultBucketCount(kResultNoSavedFramesFrozenHistogram,
                          TabSwitchTimeRecorder::TabSwitchResult::kSuccess, 1);
}

// Same as TimeIsRecordedNoSavedFrame but with the destination frame unloaded.
TEST_F(TabSwitchTimeRecorderTest, TimeIsRecordedNoSavedFrameUnloaded) {
  const auto start = base::TimeTicks::Now();
  auto callback = tab_switch_time_recorder_.TabWasShown(
      false /* has_saved_frames */,
      {start, /* destination_is_loaded */ false,
       /* destination_is_frozen */ false},
      start);
  const auto end = start + kDuration;
  auto presentation_feedback = gfx::PresentationFeedback(
      end, end - start, gfx::PresentationFeedback::Flags::kHWCompletion);
  std::move(callback).Run(presentation_feedback);

  ExpectHistogramsEmptyExcept({kDurationNoSavedFramesUnloadedHistogram,
                               kResultNoSavedFramesUnloadedHistogram});

  // Duration.
  ExpectTotalSamples(kDurationNoSavedFramesUnloadedHistogram, 1);
  ExpectTimeBucketCount(kDurationNoSavedFramesUnloadedHistogram, kDuration, 1);

  // Result.
  ExpectTotalSamples(kResultNoSavedFramesUnloadedHistogram, 1);
  ExpectResultBucketCount(kResultNoSavedFramesUnloadedHistogram,
                          TabSwitchTimeRecorder::TabSwitchResult::kSuccess, 1);
}

// A failure should be reported if gfx::PresentationFeedback contains the
// kFailure flag.
TEST_F(TabSwitchTimeRecorderTest, PresentationFailureWithSavedFrames) {
  const auto start = base::TimeTicks::Now();
  auto callback = tab_switch_time_recorder_.TabWasShown(
      true /* has_saved_frames */,
      {start, /* destination_is_loaded */ true,
       /* destination_is_frozen */ false},
      start);
  std::move(callback).Run(gfx::PresentationFeedback::Failure());

  ExpectHistogramsEmptyExcept({kResultWithSavedFramesHistogram});

  // Result (no duration is recorded on presentation failure).
  ExpectTotalSamples(kResultWithSavedFramesHistogram, 1);
  ExpectResultBucketCount(
      kResultWithSavedFramesHistogram,
      TabSwitchTimeRecorder::TabSwitchResult::kPresentationFailure, 1);
}

// A failure should be reported if gfx::PresentationFeedback contains the
// kFailure flag.
TEST_F(TabSwitchTimeRecorderTest, PresentationFailureNoSavedFrames) {
  const auto start = base::TimeTicks::Now();
  auto callback = tab_switch_time_recorder_.TabWasShown(
      false /* has_saved_frames */,
      {start, /* destination_is_loaded */ true,
       /* destination_is_frozen */ false},
      start);
  std::move(callback).Run(gfx::PresentationFeedback::Failure());

  ExpectHistogramsEmptyExcept(
      {kResultNoSavedFramesHistogram, kResultNoSavedFramesNotFrozenHistogram});

  // Result (no duration is recorded on presentation failure).
  ExpectTotalSamples(kResultNoSavedFramesHistogram, 1);
  ExpectResultBucketCount(
      kResultNoSavedFramesHistogram,
      TabSwitchTimeRecorder::TabSwitchResult::kPresentationFailure, 1);
  ExpectTotalSamples(kResultNoSavedFramesNotFrozenHistogram, 1);
  ExpectResultBucketCount(
      kResultNoSavedFramesNotFrozenHistogram,
      TabSwitchTimeRecorder::TabSwitchResult::kPresentationFailure, 1);
}

// An incomplete tab switch is reported when no frame is shown before a tab is
// hidden.
TEST_F(TabSwitchTimeRecorderTest, HideBeforePresentFrameWithSavedFrames) {
  const auto start1 = base::TimeTicks::Now();
  auto callback1 = tab_switch_time_recorder_.TabWasShown(
      true /* has_saved_frames */,
      {start1, /* destination_is_loaded */ true,
       /* destination_is_frozen */ false},
      start1);

  task_environment_.FastForwardBy(kDuration);
  tab_switch_time_recorder_.TabWasHidden();

  ExpectHistogramsEmptyExcept({kResultWithSavedFramesHistogram,
                               kIncompleteDurationWithSavedFramesHistogram});

  // Duration.
  ExpectTotalSamples(kIncompleteDurationWithSavedFramesHistogram, 1);
  ExpectTimeBucketCount(kIncompleteDurationWithSavedFramesHistogram, kDuration,
                        1);

  // Result.
  ExpectTotalSamples(kResultWithSavedFramesHistogram, 1);
  ExpectResultBucketCount(kResultWithSavedFramesHistogram,
                          TabSwitchTimeRecorder::TabSwitchResult::kIncomplete,
                          1);

  const auto start2 = base::TimeTicks::Now();
  auto callback2 = tab_switch_time_recorder_.TabWasShown(
      true /* has_saved_frames */,
      {start2, /* destination_is_loaded */ true,
       /* destination_is_frozen */ false},
      start2);
  const auto end2 = start2 + kOtherDuration;
  auto presentation_feedback = gfx::PresentationFeedback(
      end2, end2 - start2, gfx::PresentationFeedback::Flags::kHWCompletion);
  std::move(callback2).Run(presentation_feedback);

  ExpectHistogramsEmptyExcept({kDurationWithSavedFramesHistogram,
                               kResultWithSavedFramesHistogram,
                               kIncompleteDurationWithSavedFramesHistogram});

  // Duration.
  ExpectTotalSamples(kIncompleteDurationWithSavedFramesHistogram, 1);
  ExpectTotalSamples(kDurationWithSavedFramesHistogram, 1);
  ExpectTimeBucketCount(kDurationWithSavedFramesHistogram, kOtherDuration, 1);

  // Result.
  ExpectTotalSamples(kResultWithSavedFramesHistogram, 2);
  ExpectResultBucketCount(kResultWithSavedFramesHistogram,
                          TabSwitchTimeRecorder::TabSwitchResult::kIncomplete,
                          1);
  ExpectResultBucketCount(kResultWithSavedFramesHistogram,
                          TabSwitchTimeRecorder::TabSwitchResult::kSuccess, 1);
}

// An incomplete tab switch is reported when no frame is shown before a tab is
// hidden.
TEST_F(TabSwitchTimeRecorderTest, HideBeforePresentFrameNoSavedFrames) {
  const auto start1 = base::TimeTicks::Now();
  auto callback1 = tab_switch_time_recorder_.TabWasShown(
      false /* has_saved_frames */,
      {start1, /* destination_is_loaded */ true,
       /* destination_is_frozen */ false},
      start1);

  task_environment_.FastForwardBy(kDuration);
  tab_switch_time_recorder_.TabWasHidden();

  ExpectHistogramsEmptyExcept(
      {kIncompleteDurationNoSavedFramesHistogram,
       kIncompleteDurationNoSavedFramesNotFrozenHistogram,
       kResultNoSavedFramesHistogram, kResultNoSavedFramesNotFrozenHistogram});

  // Duration.
  ExpectTotalSamples(kIncompleteDurationNoSavedFramesHistogram, 1);
  ExpectTimeBucketCount(kIncompleteDurationNoSavedFramesHistogram, kDuration,
                        1);
  ExpectTotalSamples(kIncompleteDurationNoSavedFramesNotFrozenHistogram, 1);
  ExpectTimeBucketCount(kIncompleteDurationNoSavedFramesNotFrozenHistogram,
                        kDuration, 1);

  // Result.
  ExpectTotalSamples(kResultNoSavedFramesHistogram, 1);
  ExpectResultBucketCount(kResultNoSavedFramesHistogram,
                          TabSwitchTimeRecorder::TabSwitchResult::kIncomplete,
                          1);
  ExpectTotalSamples(kResultNoSavedFramesNotFrozenHistogram, 1);
  ExpectResultBucketCount(kResultNoSavedFramesNotFrozenHistogram,
                          TabSwitchTimeRecorder::TabSwitchResult::kIncomplete,
                          1);

  const auto start2 = base::TimeTicks::Now();
  auto callback2 = tab_switch_time_recorder_.TabWasShown(
      false /* has_saved_frames */,
      {start2, /* destination_is_loaded */ true,
       /* destination_is_frozen */ false},
      start2);
  const auto end2 = start2 + kOtherDuration;

  auto presentation_feedback = gfx::PresentationFeedback(
      end2, end2 - start2, gfx::PresentationFeedback::Flags::kHWCompletion);
  std::move(callback2).Run(presentation_feedback);

  ExpectHistogramsEmptyExcept(
      {kIncompleteDurationNoSavedFramesHistogram,
       kIncompleteDurationNoSavedFramesNotFrozenHistogram,
       kDurationNoSavedFramesHistogram,
       kDurationNoSavedFramesNotFrozenHistogram, kResultNoSavedFramesHistogram,
       kResultNoSavedFramesNotFrozenHistogram});

  // Duration.
  ExpectTotalSamples(kIncompleteDurationNoSavedFramesHistogram, 1);
  ExpectTimeBucketCount(kIncompleteDurationNoSavedFramesHistogram, kDuration,
                        1);
  ExpectTotalSamples(kIncompleteDurationNoSavedFramesNotFrozenHistogram, 1);
  ExpectTimeBucketCount(kIncompleteDurationNoSavedFramesNotFrozenHistogram,
                        kDuration, 1);

  ExpectTotalSamples(kDurationNoSavedFramesHistogram, 1);
  ExpectTimeBucketCount(kDurationNoSavedFramesHistogram, kOtherDuration, 1);
  ExpectTotalSamples(kDurationNoSavedFramesNotFrozenHistogram, 1);
  ExpectTimeBucketCount(kDurationNoSavedFramesNotFrozenHistogram,
                        kOtherDuration, 1);

  // Result.
  ExpectTotalSamples(kResultNoSavedFramesHistogram, 2);
  ExpectResultBucketCount(kResultNoSavedFramesHistogram,
                          TabSwitchTimeRecorder::TabSwitchResult::kIncomplete,
                          1);
  ExpectResultBucketCount(kResultNoSavedFramesHistogram,
                          TabSwitchTimeRecorder::TabSwitchResult::kSuccess, 1);
  ExpectTotalSamples(kResultNoSavedFramesNotFrozenHistogram, 2);
  ExpectResultBucketCount(kResultNoSavedFramesNotFrozenHistogram,
                          TabSwitchTimeRecorder::TabSwitchResult::kIncomplete,
                          1);
  ExpectResultBucketCount(kResultNoSavedFramesNotFrozenHistogram,
                          TabSwitchTimeRecorder::TabSwitchResult::kSuccess, 1);
}

}  // namespace content
