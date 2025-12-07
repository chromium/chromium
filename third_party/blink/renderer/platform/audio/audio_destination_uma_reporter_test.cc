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

constexpr std::string_view kFifoDelayBase = "FIFODelay";
constexpr std::string_view kTotalPlayoutDelayBase = "TotalPlayoutDelay";
constexpr std::string_view kFifoUnderrunCountShortBase =
    "FIFOUnderrunCount.Short";
constexpr std::string_view kFifoUnderrunCountIntervalsBase =
    "FIFOUnderrunCount.Intervals";
constexpr std::string_view kRenderTimeRatioBase = "RenderTimeRatio";
constexpr std::string_view kRequestRenderTimeRatioBase =
    "RequestRenderTimeRatio";
constexpr std::string_view kRequestRenderGapTimeRatioBase =
    "RequestRenderGapTimeRatio";

constexpr int kFakeBufferSize = 256;
constexpr float kFakeSamplingRate = 48000;

}  // namespace

class AudioDestinationUmaReporterTest
    : public ::testing::TestWithParam<
          std::tuple<WebAudioLatencyHint, bool, std::string>> {
 public:
  AudioDestinationUmaReporterTest()
      : latency_hint_(std::get<0>(GetParam())),
        is_audio_worklet_set_(std::get<1>(GetParam())),
        latency_tag_(std::get<2>(GetParam())) {
    uma_reporter_ = std::make_unique<AudioDestinationUmaReporter>(
        latency_hint_, kFakeBufferSize, kFakeSamplingRate);
  }

  AudioDestinationUmaReporterTest(
      const AudioDestinationUmaReporterTest&) = delete;
  AudioDestinationUmaReporterTest& operator=(
      const AudioDestinationUmaReporterTest&) = delete;
  ~AudioDestinationUmaReporterTest() override = default;

  void SetUp() override {
    if (is_audio_worklet_set_) {
      uma_reporter_->UpdateMetricNameForDualThreadMode();
    }
  }

 protected:
  std::string GetExpectedHistogramName(const std::string_view base_name) {
    return "WebAudio.AudioDestination." +
           std::string(is_audio_worklet_set_ ? "DualThread."
                                             : "SingleThread.") +
           std::string(base_name);
  }

  base::HistogramTester histogram_tester_;
  WebAudioLatencyHint latency_hint_;
  bool is_audio_worklet_set_;
  std::string latency_tag_;
  std::unique_ptr<AudioDestinationUmaReporter> uma_reporter_;
};

TEST_P(AudioDestinationUmaReporterTest, BasicTest) {
  base::TimeDelta fake_callback_interval =
      base::Seconds(kFakeBufferSize / kFakeSamplingRate);
  // Simulate a render duration equal to 30% of the fake callback interval.
  base::TimeDelta fake_render_duration = fake_callback_interval * 0.3;
  // Simulate a request render duration equal to 20% of the fake callback
  // interval.
  base::TimeDelta fake_request_render_duration = fake_callback_interval * 0.2;
  // Simulate a request render gap duration equal to 10% of the fake callback
  // interval.
  base::TimeDelta fake_request_render_gap_duration =
      fake_callback_interval * 0.1;

  for (int i = 0; i < 1000; i++) {
    base::TimeDelta fifo_delay = base::Milliseconds(50);
    uma_reporter_->AddFifoDelay(fifo_delay);
    base::TimeDelta infra_delay = base::Milliseconds(20);
    uma_reporter_->AddTotalPlayoutDelay(fifo_delay + infra_delay);
    uma_reporter_->AddRenderDuration(fake_render_duration);
    uma_reporter_->AddRequestRenderDuration(fake_request_render_duration);
    uma_reporter_->AddRequestRenderGapDuration(
        fake_request_render_gap_duration);
    uma_reporter_->Report();
  }

  histogram_tester_.ExpectBucketCount(GetExpectedHistogramName(kFifoDelayBase),
                                      50, 1);
  histogram_tester_.ExpectBucketCount(
      GetExpectedHistogramName(kFifoDelayBase) + latency_tag_, 50, 1);

  histogram_tester_.ExpectBucketCount(
      GetExpectedHistogramName(kTotalPlayoutDelayBase), 70, 1);
  histogram_tester_.ExpectBucketCount(
      GetExpectedHistogramName(kTotalPlayoutDelayBase) + latency_tag_, 70, 1);

  // Due to the static_cast<int> conversion in PercentOfCallbackInterval, the
  // floating-point part of the double result is truncated (rounded down).
  // Therefore, we expect the exact ratio value minus 1 in the histograms.
  // For example, if the exact ratio is 30%, the recorded value will be 29.
  histogram_tester_.ExpectBucketCount(
      GetExpectedHistogramName(kRenderTimeRatioBase), 29, 1);
  histogram_tester_.ExpectBucketCount(
      GetExpectedHistogramName(kRenderTimeRatioBase) + latency_tag_, 29, 1);
  histogram_tester_.ExpectBucketCount(
      GetExpectedHistogramName(kRequestRenderTimeRatioBase), 19, 1);
  histogram_tester_.ExpectBucketCount(
      GetExpectedHistogramName(kRequestRenderTimeRatioBase) + latency_tag_, 19,
      1);
  histogram_tester_.ExpectBucketCount(
      GetExpectedHistogramName(kRequestRenderGapTimeRatioBase), 9, 1);
  histogram_tester_.ExpectBucketCount(
      GetExpectedHistogramName(kRequestRenderGapTimeRatioBase) + latency_tag_,
      9, 1);
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

  histogram_tester_.ExpectBucketCount(
      GetExpectedHistogramName(kFifoUnderrunCountShortBase), 50, 1);
  histogram_tester_.ExpectBucketCount(
      GetExpectedHistogramName(kFifoUnderrunCountShortBase) + latency_tag_, 50,
      1);
  histogram_tester_.ExpectTotalCount(
      GetExpectedHistogramName(kFifoUnderrunCountIntervalsBase), 0);
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

  histogram_tester_.ExpectBucketCount(
      GetExpectedHistogramName(kFifoUnderrunCountIntervalsBase), 500, 1);
  histogram_tester_.ExpectBucketCount(
      GetExpectedHistogramName(kFifoUnderrunCountIntervalsBase) + latency_tag_,
      500, 1);
  histogram_tester_.ExpectBucketCount(
      GetExpectedHistogramName(kFifoUnderrunCountIntervalsBase), 250, 1);
  histogram_tester_.ExpectBucketCount(
      GetExpectedHistogramName(kFifoUnderrunCountIntervalsBase) + latency_tag_,
      250, 1);

  uma_reporter_.reset();
  histogram_tester_.ExpectTotalCount(
      GetExpectedHistogramName(kFifoUnderrunCountShortBase), 0);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AudioDestinationUmaReporterTest,
    ::testing::Values(std::make_tuple(WebAudioLatencyHint::kCategoryInteractive,
                                      /*is_audio_worklet_set=*/false,
                                      /*latency_tag=*/".LatencyInteractive"),
                      std::make_tuple(WebAudioLatencyHint::kCategoryBalanced,
                                      /*is_audio_worklet_set=*/false,
                                      /*latency_tag=*/".LatencyBalanced"),
                      std::make_tuple(WebAudioLatencyHint::kCategoryPlayback,
                                      /*is_audio_worklet_set=*/false,
                                      /*latency_tag=*/".LatencyPlayback"),
                      std::make_tuple(WebAudioLatencyHint::kCategoryExact,
                                      /*is_audio_worklet_set=*/false,
                                      /*latency_tag=*/".LatencyExactMs"),
                      std::make_tuple(WebAudioLatencyHint::kCategoryInteractive,
                                      /*is_audio_worklet_set=*/true,
                                      /*latency_tag=*/".LatencyInteractive"),
                      std::make_tuple(WebAudioLatencyHint::kCategoryBalanced,
                                      /*is_audio_worklet_set=*/true,
                                      /*latency_tag=*/".LatencyBalanced"),
                      std::make_tuple(WebAudioLatencyHint::kCategoryPlayback,
                                      /*is_audio_worklet_set=*/true,
                                      /*latency_tag=*/".LatencyPlayback"),
                      std::make_tuple(WebAudioLatencyHint::kCategoryExact,
                                      /*is_audio_worklet_set=*/true,
                                      /*latency_tag=*/".LatencyExactMs")));

}  // namespace blink
