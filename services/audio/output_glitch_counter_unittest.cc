// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/output_glitch_counter.h"

#include <memory>
#include <string>
#include <tuple>
#include "base/check_op.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "media/base/audio_latency.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace audio {
namespace {

const std::string kMissedDeadlineIntervals =
    "Media.AudioRendererMissedDeadline3.Intervals";
const std::string kMissedDeadlineMixingIntervals =
    "Media.AudioRendererMissedDeadline3.Mixing.Intervals";
const std::string kMissedDeadlineShort =
    "Media.AudioRendererMissedDeadline3.Short";
const std::string kMissedDeadlineMixingShort =
    "Media.AudioRendererMissedDeadline3.Mixing.Short";
const std::string kAudioGlitches2 = "Media.AudioRendererAudioGlitches2";

class OutputGlitchCounterTest : public ::testing::Test {
 public:
  OutputGlitchCounterTest() = default;

  OutputGlitchCounterTest(const OutputGlitchCounterTest&) = delete;
  OutputGlitchCounterTest& operator=(const OutputGlitchCounterTest&) = delete;

  base::HistogramTester histogram_tester_;

  std::unique_ptr<OutputGlitchCounter> output_glitch_counter_ =
      std::make_unique<OutputGlitchCounter>(
          media::AudioLatency::Type::kRtc);
};

TEST_F(OutputGlitchCounterTest, IntervalHistograms) {
  // Report 500 non-mixing callbacks, 50 of which are missed.
  for (int i = 0; i < 500; i++) {
    bool is_mixing = false;
    bool missed_callback = i % (500 / 50) == 0;
    output_glitch_counter_->ReportMissedCallback(missed_callback, is_mixing);
  }
  // Report 500 mixing callbacks, 100 of which are missed. This makes 1000 total
  // callbacks.
  for (int i = 0; i < 500; i++) {
    bool is_mixing = true;
    bool missed_callback = i % (500 / 100) == 0;
    output_glitch_counter_->ReportMissedCallback(missed_callback, is_mixing);
  }
  histogram_tester_.ExpectUniqueSample(kMissedDeadlineIntervals, 150, 1);
  histogram_tester_.ExpectTotalCount(kMissedDeadlineMixingIntervals, 0);

  // Report 500 mixing callbacks, 25 of which are missed. This makes 1000
  // consecutive mixing callbacks.
  for (int i = 0; i < 500; i++) {
    bool is_mixing = true;
    bool missed_callback = i % (500 / 25) == 0;
    output_glitch_counter_->ReportMissedCallback(missed_callback, is_mixing);
  }
  histogram_tester_.ExpectUniqueSample(kMissedDeadlineIntervals, 150, 1);
  histogram_tester_.ExpectUniqueSample(kMissedDeadlineMixingIntervals, 125, 1);

  // Report 2000 mixing callbacks, none of which are mixed.
  for (int i = 0; i < 2000; i++) {
    bool is_mixing = true;
    bool missed_callback = false;
    output_glitch_counter_->ReportMissedCallback(missed_callback, is_mixing);
  }
  histogram_tester_.ExpectTotalCount(kMissedDeadlineIntervals, 3);
  histogram_tester_.ExpectBucketCount(kMissedDeadlineIntervals, 150, 1);
  histogram_tester_.ExpectBucketCount(kMissedDeadlineIntervals, 25, 1);
  histogram_tester_.ExpectBucketCount(kMissedDeadlineIntervals, 0, 1);
  histogram_tester_.ExpectTotalCount(kMissedDeadlineMixingIntervals, 3);
  histogram_tester_.ExpectBucketCount(kMissedDeadlineMixingIntervals, 125, 1);
  histogram_tester_.ExpectBucketCount(kMissedDeadlineMixingIntervals, 0, 2);

  // Since the stream was at least 1000 callbacks, we do not expect any stats
  // for short streams.
  output_glitch_counter_.reset();
  histogram_tester_.ExpectTotalCount(kMissedDeadlineShort, 0);
  histogram_tester_.ExpectTotalCount(kMissedDeadlineMixingShort, 0);
}

TEST_F(OutputGlitchCounterTest, MixingThenStopMixing) {
  // Report 500 mixing callbacks, 25 of which are missed.
  for (int i = 0; i < 500; i++) {
    bool is_mixing = true;
    bool missed_callback = i % (500 / 25) == 0;
    output_glitch_counter_->ReportMissedCallback(missed_callback, is_mixing);
  }
  // We report a non-mixing callback. Since we are no longer mixing, we should
  // upload the mixign data.
  output_glitch_counter_->ReportMissedCallback(/*missed_callback = */ false,
                                               /*is_mixing = */ false);
  histogram_tester_.ExpectTotalCount(kMissedDeadlineMixingIntervals, 0);
  histogram_tester_.ExpectUniqueSample(kMissedDeadlineMixingShort, 25, 1);

  // The sample should not be uploaded again on destruction.
  output_glitch_counter_.reset();
  histogram_tester_.ExpectUniqueSample(kMissedDeadlineMixingShort, 25, 1);
}

TEST_F(OutputGlitchCounterTest, ShortHistograms) {
  // Report 100 non-mixing callbacks, 4 of which are missed.
  for (int i = 0; i < 100; i++) {
    bool is_mixing = false;
    bool missed_callback = i % (100 / 4) == 0;
    output_glitch_counter_->ReportMissedCallback(missed_callback, is_mixing);
  }
  // Report 100 mixing callbacks, 10 of which are missed.
  for (int i = 0; i < 100; i++) {
    bool is_mixing = true;
    bool missed_callback = i % (100 / 10) == 0;
    output_glitch_counter_->ReportMissedCallback(missed_callback, is_mixing);
  }

  // Report a bunch of missed mixing callbacks. These should be considered
  // trailing and should not be reflected in the histograms.
  for (int i = 0; i < 2000; i++) {
    bool is_mixing = true;
    bool missed_callback = true;
    output_glitch_counter_->ReportMissedCallback(missed_callback, is_mixing);
  }

  // This histograms should be uploaded upon destruction of
  // output_glitch_counter_.
  histogram_tester_.ExpectTotalCount(kMissedDeadlineShort, 0);
  histogram_tester_.ExpectTotalCount(kMissedDeadlineMixingShort, 0);
  output_glitch_counter_.reset();
  histogram_tester_.ExpectUniqueSample(kMissedDeadlineShort, 14, 1);
  histogram_tester_.ExpectUniqueSample(kMissedDeadlineMixingShort, 10, 1);

  // Since the stream was shorter than 1000 callbacks, no interval data should
  // be uploaded.
  histogram_tester_.ExpectTotalCount(kMissedDeadlineIntervals, 0);
  histogram_tester_.ExpectTotalCount(kMissedDeadlineMixingIntervals, 0);
}

TEST_F(OutputGlitchCounterTest, BinaryGlitchMetricTrue) {
  output_glitch_counter_->ReportMissedCallback(/*missed_callback = */ true,
                                               /*is_mixing = */ false);
  output_glitch_counter_->ReportMissedCallback(/*missed_callback = */ false,
                                               /*is_mixing = */ false);
  output_glitch_counter_.reset();
  histogram_tester_.ExpectUniqueSample(kAudioGlitches2, 1, 1);
}

TEST_F(OutputGlitchCounterTest, BinaryGlitchMetricFalse) {
  output_glitch_counter_->ReportMissedCallback(/*missed_callback = */ false,
                                               /*is_mixing = */ false);
  // This missed callback will be considered trailing.
  output_glitch_counter_->ReportMissedCallback(/*missed_callback = */ true,
                                               /*is_mixing = */ false);
  output_glitch_counter_.reset();
  histogram_tester_.ExpectUniqueSample(kAudioGlitches2, 0, 1);
}

TEST_F(OutputGlitchCounterTest, GetLogStats) {
  // Report 100 callbacks, 25 of which are missed.
  for (int i = 0; i < 100; i++) {
    bool is_mixing = i % 3 == 0;
    bool missed_callback = i % (100 / 25) == 0;
    output_glitch_counter_->ReportMissedCallback(missed_callback, is_mixing);
  }
  output_glitch_counter_->ReportMissedCallback(true, false);

  OutputGlitchCounter::LogStats log_stats =
      output_glitch_counter_->GetLogStats();
  // The 101th call (a miss) is trailing and should not be counted.
  CHECK_EQ(log_stats.callback_count_, 100u);
  CHECK_EQ(log_stats.miss_count_, 25u);
}

class OutputGlitchCounterNamesTest
    : public ::testing::TestWithParam<
          std::tuple<media::AudioLatency::Type, std::string>> {
 public:
  OutputGlitchCounterNamesTest() = default;
  ~OutputGlitchCounterNamesTest() override = default;

  base::HistogramTester histogram_tester_;

  std::unique_ptr<OutputGlitchCounter> output_glitch_counter_;
};

TEST_P(OutputGlitchCounterNamesTest, MetricNames) {
  media::AudioLatency::Type latency_type = std::get<0>(GetParam());
  std::string suffix = std::get<1>(GetParam());

  // Test the short statistics.
  output_glitch_counter_ = std::make_unique<OutputGlitchCounter>(latency_type);

  for (int i = 0; i < 100; i++) {
    bool is_mixing = true;
    bool missed_callback = i % (100 / 10) == 0;
    output_glitch_counter_->ReportMissedCallback(missed_callback, is_mixing);
  }
  output_glitch_counter_.reset();
  histogram_tester_.ExpectUniqueSample(kMissedDeadlineShort + "." + suffix, 10,
                                       1);
  histogram_tester_.ExpectUniqueSample(
      kMissedDeadlineMixingShort + "." + suffix, 10, 1);
  histogram_tester_.ExpectUniqueSample(kAudioGlitches2 + "." + suffix, 1, 1);

  // Test the Interval statistics
  output_glitch_counter_ = std::make_unique<OutputGlitchCounter>(latency_type);

  for (int i = 0; i < 1000; i++) {
    bool is_mixing = true;
    bool missed_callback = i % (1000 / 10) == 0;
    output_glitch_counter_->ReportMissedCallback(missed_callback, is_mixing);
  }
  histogram_tester_.ExpectUniqueSample(kMissedDeadlineIntervals + "." + suffix,
                                       10, 1);
  histogram_tester_.ExpectUniqueSample(
      kMissedDeadlineMixingIntervals + "." + suffix, 10, 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    OutputGlitchCounterNamesTest,
    ::testing::Values(
        std::make_tuple(media::AudioLatency::Type::kExactMS,
                        "LatencyExactMs"),
        std::make_tuple(media::AudioLatency::Type::kInteractive,
                        "LatencyInteractive"),
        std::make_tuple(media::AudioLatency::Type::kRtc, "LatencyRtc"),
        std::make_tuple(media::AudioLatency::Type::kPlayback,
                        "LatencyPlayback"),
        std::make_tuple(media::AudioLatency::Type::kUnknown,
                        "LatencyUnknown")));

}  // namespace
}  // namespace audio
