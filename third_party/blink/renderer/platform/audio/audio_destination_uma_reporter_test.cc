// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/renderer/platform/audio/audio_destination_uma_reporter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace blink {

namespace {
constexpr char kFifoDelay[] = "WebAudio.AudioDestination.FIFODelay";
constexpr char kTotalPlayoutDelay[] =
    "WebAudio.AudioDestination.TotalPlayoutDelay";
constexpr char kFifoUnderrunCountShort[] =
    "WebAudio.AudioDestination.FIFOUnderrunCount.Short";
constexpr char kFifoUnderrunCountIntervals[] =
    "WebAudio.AudioDestination.FIFOUnderrunCount.Intervals";
}  // namespace

class AudioDestinationUmaReporterTest
    : public ::testing::TestWithParam<
          std::tuple<WebAudioLatencyHint, std::string>> {
 public:
  AudioDestinationUmaReporterTest()
      : latency_hint_(std::get<0>(GetParam())),
        latency_tag_(std::get<1>(GetParam())) {
    uma_reporter_ =
        std::make_unique<AudioDestinationUmaReporter>(latency_hint_);
  }

  AudioDestinationUmaReporterTest(
      const AudioDestinationUmaReporterTest&) = delete;
  AudioDestinationUmaReporterTest& operator=(
      const AudioDestinationUmaReporterTest&) = delete;
  ~AudioDestinationUmaReporterTest() override = default;

 protected:
  base::HistogramTester histogram_tester_;
  WebAudioLatencyHint latency_hint_;
  std::string latency_tag_;
  std::unique_ptr<AudioDestinationUmaReporter> uma_reporter_;
};

TEST_P(AudioDestinationUmaReporterTest, BasicTest) {
  for (int i = 0; i < 1000; i++) {
    base::TimeDelta fifo_delay = base::Milliseconds(i % 2 ? 10 : 40);
    uma_reporter_->UpdateFifoDelay(fifo_delay);
    base::TimeDelta infra_delay = base::Milliseconds(i % 2 ? 1 : 10);
    uma_reporter_->UpdateTotalPlayoutDelay(fifo_delay + infra_delay);
    uma_reporter_->Report();
  }

  histogram_tester_.ExpectBucketCount(kFifoDelay, 10, 500);
  histogram_tester_.ExpectBucketCount(kFifoDelay + latency_tag_, 10, 500);
  histogram_tester_.ExpectBucketCount(kFifoDelay, 40, 500);
  histogram_tester_.ExpectBucketCount(kFifoDelay + latency_tag_, 40, 500);
  histogram_tester_.ExpectBucketCount(kTotalPlayoutDelay, 11, 500);
  histogram_tester_.ExpectBucketCount(kTotalPlayoutDelay + latency_tag_,
                                      11, 500);
  histogram_tester_.ExpectBucketCount(kTotalPlayoutDelay, 50, 500);
  histogram_tester_.ExpectBucketCount(kTotalPlayoutDelay + latency_tag_,
                                      50, 500);

  uma_reporter_.reset();
}

TEST_P(AudioDestinationUmaReporterTest, ShortStreamTest) {
  for (int i = 0; i < 100; i++) {
    if (i % 2 == 0) {
      uma_reporter_->IncreaseFifoUnderrunCount();
    }
    uma_reporter_->Report();
  }

  // Needs destruction to report a premature run.
  uma_reporter_.reset();

  histogram_tester_.ExpectBucketCount(kFifoUnderrunCountShort, 50, 1);
  histogram_tester_.ExpectBucketCount(kFifoUnderrunCountShort + latency_tag_,
                                      50, 1);
  histogram_tester_.ExpectTotalCount(kFifoUnderrunCountIntervals, 0);
}

TEST_P(AudioDestinationUmaReporterTest, LongStreamTest) {
  // A complete interval (1000 callbacks) experiencing FIFO underruns for
  // half of the interval.
  for (int i = 0; i < 1000; i++) {
    if (i % 2 == 0) {
      uma_reporter_->IncreaseFifoUnderrunCount();
    }
    uma_reporter_->Report();
  }
  // A complete interval (1000 callbacks) experiencing FIFO underruns for
  // a quarter of the interval.
  for (int i = 0; i < 1000; i++) {
    if (i % 4 == 0) {
      uma_reporter_->IncreaseFifoUnderrunCount();
    }
    uma_reporter_->Report();
  }

  histogram_tester_.ExpectBucketCount(kFifoUnderrunCountIntervals, 500, 1);
  histogram_tester_.ExpectBucketCount(
      kFifoUnderrunCountIntervals + latency_tag_, 500, 1);
  histogram_tester_.ExpectBucketCount(kFifoUnderrunCountIntervals, 250, 1);
  histogram_tester_.ExpectBucketCount(
      kFifoUnderrunCountIntervals + latency_tag_, 250, 1);

  uma_reporter_.reset();
  histogram_tester_.ExpectTotalCount(kFifoUnderrunCountShort, 0);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AudioDestinationUmaReporterTest,
    ::testing::Values(std::make_tuple(WebAudioLatencyHint::kCategoryInteractive,
                                      ".LatencyInteractive"),
                      std::make_tuple(WebAudioLatencyHint::kCategoryBalanced,
                                      ".LatencyBalanced"),
                      std::make_tuple(WebAudioLatencyHint::kCategoryPlayback,
                                      ".LatencyPlayback"),
                      std::make_tuple(WebAudioLatencyHint::kCategoryExact,
                                      ".LatencyExactMs")));

}  // namespace blink
