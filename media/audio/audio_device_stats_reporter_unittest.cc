// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_device_stats_reporter.h"
#include "base/time/time.h"

#include "base/test/metrics/histogram_tester.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_latency.h"
#include "media/base/audio_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace media {

namespace {

const std::string kRenderDelay = "Media.AudioOutputDevice.AudioServiceDelay";
const std::string kRenderDelayDifferenceShort =
    "Media.AudioOutputDevice.AudioServiceDelayDifference.Short";
const std::string kRenderDelayDifferenceIntervals =
    "Media.AudioOutputDevice.AudioServiceDelayDifference.Intervals";
const std::string kRenderGlitchCountShort =
    "Media.AudioOutputDevice.AudioServiceGlitchCount.Short";
const std::string kRenderGlitchCountIntervals =
    "Media.AudioOutputDevice.AudioServiceGlitchCount.Intervals";
const std::string kRenderGlitchDurationShort =
    "Media.AudioOutputDevice.AudioServiceGlitchDuration.Short";
const std::string kRenderGlitchDurationIntervals =
    "Media.AudioOutputDevice.AudioServiceGlitchDuration.Intervals";

}  // namespace

class AudioDeviceStatsReporterTest
    : public ::testing::TestWithParam<
          std::tuple<media::AudioLatency::LatencyType, std::string>> {
 public:
  AudioDeviceStatsReporterTest() {
    latency_type_ = std::get<0>(GetParam());
    latency_tag_ = std::get<1>(GetParam());
    params_.set_latency_tag(latency_type_);
    reporter_ = std::make_unique<AudioDeviceStatsReporter>(params_);
  }

  AudioDeviceStatsReporterTest(const AudioDeviceStatsReporterTest&) = delete;
  AudioDeviceStatsReporterTest& operator=(const AudioDeviceStatsReporterTest&) =
      delete;

  ~AudioDeviceStatsReporterTest() override = default;

 protected:
  base::HistogramTester histogram_tester_;
  AudioParameters params_ =
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      ChannelLayoutConfig::Stereo(),
                      48000,
                      480);
  AudioLatency::LatencyType latency_type_;
  std::string latency_tag_;
  std::unique_ptr<AudioDeviceStatsReporter> reporter_;
};

TEST_P(AudioDeviceStatsReporterTest, ShortStreamTest) {
  // The first callback should be ignored in the stats.
  reporter_->ReportCallback({}, {});

  for (int i = 0; i < 100; i++) {
    base::TimeDelta delay = base::Milliseconds(i % 2 ? 60 : 140);
    bool failed = i % 2;
    media::AudioGlitchInfo glitch_info{
        .duration = failed ? params_.GetBufferDuration() : base::TimeDelta(),
        .count = static_cast<unsigned int>(failed)};
    reporter_->ReportCallback(delay, glitch_info);
  }
  reporter_.reset();

  histogram_tester_.ExpectBucketCount(kRenderDelay, 60, 50);
  histogram_tester_.ExpectBucketCount(kRenderDelay + latency_tag_, 60, 50);
  histogram_tester_.ExpectBucketCount(kRenderDelay, 140, 50);
  histogram_tester_.ExpectBucketCount(kRenderDelay + latency_tag_, 140, 50);
  histogram_tester_.ExpectBucketCount(kRenderDelayDifferenceShort, 80, 1);
  histogram_tester_.ExpectBucketCount(
      kRenderDelayDifferenceShort + latency_tag_, 80, 1);
  histogram_tester_.ExpectBucketCount(kRenderGlitchCountShort, 50, 1);
  histogram_tester_.ExpectBucketCount(kRenderGlitchCountShort + latency_tag_,
                                      50, 1);
  histogram_tester_.ExpectBucketCount(kRenderGlitchDurationShort, 500, 1);
  histogram_tester_.ExpectBucketCount(kRenderGlitchDurationShort + latency_tag_,
                                      500, 1);

  histogram_tester_.ExpectTotalCount(kRenderDelayDifferenceIntervals, 0);
  histogram_tester_.ExpectTotalCount(kRenderGlitchCountIntervals, 0);
  histogram_tester_.ExpectTotalCount(kRenderGlitchDurationIntervals, 0);
}

TEST_P(AudioDeviceStatsReporterTest, LongStreamTest) {
  // The first callback should be ignored in the stats.
  reporter_->ReportCallback({}, {});

  // A complete interval where half the callbacks are glitchy.
  for (int i = 0; i < 1000; i++) {
    base::TimeDelta delay = base::Milliseconds(i % 2 ? 60 : 140);
    bool failed = i % 2;
    media::AudioGlitchInfo glitch_info{
        .duration = failed ? params_.GetBufferDuration() : base::TimeDelta(),
        .count = static_cast<unsigned int>(failed)};
    reporter_->ReportCallback(delay, glitch_info);
  }
  // A complete interval where a quarter of the callbacks are glitchy.
  for (int i = 0; i < 1000; i++) {
    base::TimeDelta delay = base::Milliseconds(i % 2 ? 10 : 190);
    bool failed = i % 4 == 0;
    media::AudioGlitchInfo glitch_info{
        .duration = failed ? params_.GetBufferDuration() : base::TimeDelta(),
        .count = static_cast<unsigned int>(failed)};
    reporter_->ReportCallback(delay, glitch_info);
  }
  // Half an interval, which will not be reflected in the interval logs, but
  // will be reflected in the delay logs.
  for (int i = 0; i < 500; i++) {
    base::TimeDelta delay = base::Milliseconds(100);
    bool failed = i % 3 == 0;
    media::AudioGlitchInfo glitch_info{
        .duration = failed ? params_.GetBufferDuration() : base::TimeDelta(),
        .count = static_cast<unsigned int>(failed)};
    reporter_->ReportCallback(delay, glitch_info);
  }

  // Data from the first interval.
  histogram_tester_.ExpectBucketCount(kRenderDelay, 60, 500);
  histogram_tester_.ExpectBucketCount(kRenderDelay + latency_tag_, 60, 500);
  histogram_tester_.ExpectBucketCount(kRenderDelay, 140, 500);
  histogram_tester_.ExpectBucketCount(kRenderDelay + latency_tag_, 140, 500);

  histogram_tester_.ExpectBucketCount(kRenderDelayDifferenceIntervals, 80, 1);
  histogram_tester_.ExpectBucketCount(
      kRenderDelayDifferenceIntervals + latency_tag_, 80, 1);
  histogram_tester_.ExpectBucketCount(kRenderGlitchCountIntervals, 500, 1);
  histogram_tester_.ExpectBucketCount(
      kRenderGlitchCountIntervals + latency_tag_, 500, 1);
  histogram_tester_.ExpectBucketCount(kRenderGlitchDurationIntervals, 500, 1);
  histogram_tester_.ExpectBucketCount(
      kRenderGlitchDurationIntervals + latency_tag_, 500, 1);

  // Data from the second interval.
  histogram_tester_.ExpectBucketCount(kRenderDelay, 10, 500);
  histogram_tester_.ExpectBucketCount(kRenderDelay + latency_tag_, 10, 500);
  histogram_tester_.ExpectBucketCount(kRenderDelay, 190, 500);
  histogram_tester_.ExpectBucketCount(kRenderDelay + latency_tag_, 190, 500);
  histogram_tester_.ExpectBucketCount(kRenderDelayDifferenceIntervals, 180, 1);
  histogram_tester_.ExpectBucketCount(
      kRenderDelayDifferenceIntervals + latency_tag_, 180, 1);
  histogram_tester_.ExpectBucketCount(kRenderGlitchCountIntervals, 250, 1);
  histogram_tester_.ExpectBucketCount(
      kRenderGlitchCountIntervals + latency_tag_, 250, 1);
  histogram_tester_.ExpectBucketCount(kRenderGlitchDurationIntervals, 250, 1);
  histogram_tester_.ExpectBucketCount(
      kRenderGlitchDurationIntervals + latency_tag_, 250, 1);

  // Data from the last, incomplete interval.
  histogram_tester_.ExpectBucketCount(kRenderDelay, 100, 500);
  histogram_tester_.ExpectBucketCount(kRenderDelay + latency_tag_, 100, 500);

  reporter_.reset();
  histogram_tester_.ExpectTotalCount(kRenderDelayDifferenceShort, 0);
  histogram_tester_.ExpectTotalCount(kRenderGlitchCountShort, 0);
  histogram_tester_.ExpectTotalCount(kRenderGlitchDurationShort, 0);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AudioDeviceStatsReporterTest,
    ::testing::Values(std::make_tuple(media::AudioLatency::LATENCY_EXACT_MS,
                                      ".LatencyExactMs"),
                      std::make_tuple(media::AudioLatency::LATENCY_INTERACTIVE,
                                      ".LatencyInteractive"),
                      std::make_tuple(media::AudioLatency::LATENCY_RTC,
                                      ".LatencyRtc"),
                      std::make_tuple(media::AudioLatency::LATENCY_PLAYBACK,
                                      ".LatencyPlayback"),
                      std::make_tuple(media::AudioLatency::LATENCY_COUNT,
                                      ".LatencyUnknown")));

}  // namespace media
