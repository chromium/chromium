// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/input_glitch_counter.h"

#include <memory>
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace audio {
namespace {

const std::string kDroppedData10sIntervals =
    "Media.AudioCapturerDroppedData10sIntervals";
const std::string kMissedReadDeadline10sIntervals =
    "Media.AudioCapturerMissedReadDeadline10sIntervals";
const std::string kDroppedDataBelow10s =
    "Media.AudioCapturerDroppedDataBelow10s";
const std::string kMissedReadDeadlineBelow10s =
    "Media.AudioCapturerMissedReadDeadlineBelow10s";

class InputGlitchCounterTest : public ::testing::Test {
 public:
  InputGlitchCounterTest() = default;

  InputGlitchCounterTest(const InputGlitchCounterTest&) = delete;
  InputGlitchCounterTest& operator=(const InputGlitchCounterTest&) = delete;

  base::HistogramTester histogram_tester_;

  std::unique_ptr<InputGlitchCounter> input_glitch_counter_ =
      std::make_unique<InputGlitchCounter>(
          base::BindLambdaForTesting([](const std::string& s) {}));
};

TEST_F(InputGlitchCounterTest, IntervalHistograms) {
  for (int i = 0; i < 1000; i++) {
    // Miss 5 of the first 1000 deadlines.
    input_glitch_counter_->ReportMissedReadDeadline(i % (1000 / 5) == 0);
    // Drop 2 of the first 1000 data blocks.
    input_glitch_counter_->ReportDroppedData(i % (1000 / 2) == 0);
  }
  histogram_tester_.ExpectUniqueSample(kDroppedData10sIntervals, 2, 1);
  histogram_tester_.ExpectUniqueSample(kMissedReadDeadline10sIntervals, 5, 1);

  for (int i = 0; i < 1000; i++) {
    // Miss 10 of the second 1000 deadlines.
    input_glitch_counter_->ReportMissedReadDeadline(i % (1000 / 10) == 0);
    // Drop 4 of the second 1000 data blocks.
    input_glitch_counter_->ReportDroppedData(i % (1000 / 4) == 0);
  }

  histogram_tester_.ExpectTotalCount(kDroppedData10sIntervals, 2);
  histogram_tester_.ExpectBucketCount(kDroppedData10sIntervals, 4, 1);
  histogram_tester_.ExpectTotalCount(kMissedReadDeadline10sIntervals, 2);
  histogram_tester_.ExpectBucketCount(kMissedReadDeadline10sIntervals, 10, 1);

  // Since the stream lasted for longer than 10 seconds, we do not expect it to
  // upload data for the histograms for less than 10 seconds.
  input_glitch_counter_.reset();
  histogram_tester_.ExpectTotalCount(kDroppedDataBelow10s, 0);
  histogram_tester_.ExpectTotalCount(kMissedReadDeadlineBelow10s, 0);
}

TEST_F(InputGlitchCounterTest, Below10sHistograms) {
  for (int i = 0; i < 100; i++) {
    // Miss 5 of the first 100 deadlines.
    input_glitch_counter_->ReportMissedReadDeadline(i % (100 / 5) == 0);
    // Drop 2 of the first 100 data blocks.
    input_glitch_counter_->ReportDroppedData(i % (100 / 2) == 0);
  }

  // Report 30 seconds of trailing errors. We expect these not to be reflected
  // in the histograms.
  for (int i = 0; i < 3000; i++) {
    input_glitch_counter_->ReportDroppedData(true);
    input_glitch_counter_->ReportMissedReadDeadline(true);
  }

  // This histograms should be uploaded upon destruction of
  // input_glitch_counter_.
  histogram_tester_.ExpectTotalCount(kDroppedDataBelow10s, 0);
  histogram_tester_.ExpectTotalCount(kMissedReadDeadlineBelow10s, 0);
  input_glitch_counter_.reset();
  histogram_tester_.ExpectUniqueSample(kDroppedDataBelow10s, 2, 1);
  histogram_tester_.ExpectUniqueSample(kMissedReadDeadlineBelow10s, 5, 1);

  // Since the stream was shorter than 10 seconds, no 10 second interval data
  // should be logged.
  histogram_tester_.ExpectTotalCount(kDroppedData10sIntervals, 0);
  histogram_tester_.ExpectTotalCount(kMissedReadDeadline10sIntervals, 0);
}

TEST_F(InputGlitchCounterTest, BinaryGlitchMetricTrue) {
  input_glitch_counter_->ReportDroppedData(true);
  input_glitch_counter_->ReportDroppedData(false);
  input_glitch_counter_.reset();
  histogram_tester_.ExpectUniqueSample("Media.AudioCapturerAudioGlitches", 1,
                                       1);
}

TEST_F(InputGlitchCounterTest, BinaryGlitchMetricFalse) {
  // This drop will be considered trailing.
  input_glitch_counter_->ReportDroppedData(true);
  input_glitch_counter_.reset();
  histogram_tester_.ExpectUniqueSample("Media.AudioCapturerAudioGlitches", 0,
                                       1);
}

}  // namespace
}  // namespace audio
