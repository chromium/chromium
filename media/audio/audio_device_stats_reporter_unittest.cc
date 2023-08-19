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
const std::string kRenderGlitchCountShort =
    "Media.AudioOutputDevice.AudioServiceGlitchCount.Short";
const std::string kRenderGlitchCountIntervals =
    "Media.AudioOutputDevice.AudioServiceGlitchCount.Intervals";
const std::string kRenderGlitchDurationShort =
    "Media.AudioOutputDevice.AudioServiceGlitchDuration.Short";
const std::string kRenderGlitchDurationIntervals =
    "Media.AudioOutputDevice.AudioServiceGlitchDuration.Intervals";

const std::string kCaptureDelay = "Media.AudioInputDevice.AudioServiceDelay";
const std::string kCaptureGlitchCountShort =
    "Media.AudioInputDevice.AudioServiceGlitchCount.Short";
const std::string kCaptureGlitchCountIntervals =
    "Media.AudioInputDevice.AudioServiceGlitchCount.Intervals";
const std::string kCaptureGlitchDurationShort =
    "Media.AudioInputDevice.AudioServiceGlitchDuration.Short";
const std::string kCaptureGlitchDurationIntervals =
    "Media.AudioInputDevice.AudioServiceGlitchDuration.Intervals";

}  // namespace

class AudioDeviceStatsReporterOutputTest
    : public ::testing::TestWithParam<
          std::tuple<media::AudioLatency::Type, std::string>> {
 public:
  AudioDeviceStatsReporterOutputTest() {
    latency_type_ = std::get<0>(GetParam());
    latency_tag_ = std::get<1>(GetParam());
    params_.set_latency_tag(latency_type_);
    reporter_ = std::make_unique<AudioDeviceStatsReporter>(
        params_, AudioDeviceStatsReporter::Type::kOutput);
  }

  AudioDeviceStatsReporterOutputTest(
      const AudioDeviceStatsReporterOutputTest&) = delete;
  AudioDeviceStatsReporterOutputTest& operator=(
      const AudioDeviceStatsReporterOutputTest&) = delete;

  ~AudioDeviceStatsReporterOutputTest() override = default;

 protected:
  base::HistogramTester histogram_tester_;
  AudioParameters params_ =
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      ChannelLayoutConfig::Stereo(),
                      48000,
                      480);
  AudioLatency::Type latency_type_;
  std::string latency_tag_;
  std::unique_ptr<AudioDeviceStatsReporter> reporter_;
};

TEST_P(AudioDeviceStatsReporterOutputTest, ShortStreamTest) {
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
  histogram_tester_.ExpectBucketCount(kRenderGlitchCountShort, 50, 1);
  histogram_tester_.ExpectBucketCount(kRenderGlitchCountShort + latency_tag_,
                                      50, 1);
  histogram_tester_.ExpectBucketCount(kRenderGlitchDurationShort, 500, 1);
  histogram_tester_.ExpectBucketCount(kRenderGlitchDurationShort + latency_tag_,
                                      500, 1);

  histogram_tester_.ExpectTotalCount(kRenderGlitchCountIntervals, 0);
  histogram_tester_.ExpectTotalCount(kRenderGlitchDurationIntervals, 0);
}

TEST_P(AudioDeviceStatsReporterOutputTest, LongStreamTest) {
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
  histogram_tester_.ExpectTotalCount(kRenderGlitchCountShort, 0);
  histogram_tester_.ExpectTotalCount(kRenderGlitchDurationShort, 0);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AudioDeviceStatsReporterOutputTest,
    ::testing::Values(
        std::make_tuple(media::AudioLatency::Type::kExactMS,
                        ".LatencyExactMs"),
        std::make_tuple(media::AudioLatency::Type::kInteractive,
                        ".LatencyInteractive"),
        std::make_tuple(media::AudioLatency::Type::kRtc, ".LatencyRtc"),
        std::make_tuple(media::AudioLatency::Type::kPlayback,
                        ".LatencyPlayback"),
        std::make_tuple(media::AudioLatency::Type::kUnknown,
                        ".LatencyUnknown")));

class AudioDeviceStatsReporterInputTest : public ::testing::Test {
 public:
  AudioDeviceStatsReporterInputTest() {
    reporter_ = std::make_unique<AudioDeviceStatsReporter>(
        params_, AudioDeviceStatsReporter::Type::kInput);
  }

  AudioDeviceStatsReporterInputTest(const AudioDeviceStatsReporterInputTest&) =
      delete;
  AudioDeviceStatsReporterInputTest& operator=(
      const AudioDeviceStatsReporterInputTest&) = delete;

  ~AudioDeviceStatsReporterInputTest() override = default;

 protected:
  base::HistogramTester histogram_tester_;
  AudioParameters params_ =
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      ChannelLayoutConfig::Stereo(),
                      48000,
                      480);
  std::unique_ptr<AudioDeviceStatsReporter> reporter_;
};

TEST_F(AudioDeviceStatsReporterInputTest, ShortStreamTest) {
  for (int i = 0; i < 100; i++) {
    base::TimeDelta delay = base::Milliseconds(i % 2 ? 60 : 140);
    bool failed = i % 2;
    media::AudioGlitchInfo glitch_info{
        .duration = failed ? params_.GetBufferDuration() : base::TimeDelta(),
        .count = static_cast<unsigned int>(failed)};
    reporter_->ReportCallback(delay, glitch_info);
  }
  reporter_.reset();

  histogram_tester_.ExpectBucketCount(kCaptureDelay, 60, 50);
  histogram_tester_.ExpectBucketCount(kCaptureDelay, 140, 50);
  histogram_tester_.ExpectBucketCount(kCaptureGlitchCountShort, 50, 1);
  histogram_tester_.ExpectBucketCount(kCaptureGlitchDurationShort, 500, 1);

  histogram_tester_.ExpectTotalCount(kCaptureGlitchCountIntervals, 0);
  histogram_tester_.ExpectTotalCount(kCaptureGlitchDurationIntervals, 0);
}

TEST_F(AudioDeviceStatsReporterInputTest, LongStreamTest) {
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
  histogram_tester_.ExpectBucketCount(kCaptureDelay, 60, 500);
  histogram_tester_.ExpectBucketCount(kCaptureDelay, 140, 500);

  histogram_tester_.ExpectBucketCount(kCaptureGlitchCountIntervals, 500, 1);
  histogram_tester_.ExpectBucketCount(kCaptureGlitchDurationIntervals, 500, 1);

  // Data from the second interval.
  histogram_tester_.ExpectBucketCount(kCaptureDelay, 10, 500);
  histogram_tester_.ExpectBucketCount(kCaptureDelay, 190, 500);
  histogram_tester_.ExpectBucketCount(kCaptureGlitchCountIntervals, 250, 1);
  histogram_tester_.ExpectBucketCount(kCaptureGlitchDurationIntervals, 250, 1);

  // Data from the last, incomplete interval.
  histogram_tester_.ExpectBucketCount(kCaptureDelay, 100, 500);

  reporter_.reset();
  histogram_tester_.ExpectTotalCount(kCaptureGlitchCountShort, 0);
  histogram_tester_.ExpectTotalCount(kCaptureGlitchDurationShort, 0);
}

}  // namespace media
