// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_device_stats_reporter.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"

namespace media {
AudioDeviceStatsReporter::AudioDeviceStatsReporter(
    const AudioParameters& params,
    Type type)
    : callback_duration_(params.GetBufferDuration()),
      delay_log_callback_(CreateRealtimeCallback(
          "Delay",
          params.latency_tag(),
          /*max_value = */ 1000,  // Measured in ms. Allows us to differentiate
                                  // delays up to 1s.
          /*bucket_count = */ 50,
          type)),
      glitch_count_log_callback_(CreateAggregateCallback(
          "GlitchCount",
          params.latency_tag(),
          /*max_value = */ 1000,  // Measured in glitches per 1000 callbacks.
                                  // Unlikely to be higher than 1000.
          /*bucket_count = */ 50,
          type)),
      glitch_duration_log_callback_(CreateAggregateCallback(
          "GlitchDuration",
          params.latency_tag(),
          /*max_value = */ 1000,  // Measured in permille.
          /*bucket_count = */ 50,
          type)),
      discarded_first_callback_(
          type != Type::kOutput)  // For output, we are discarding the first
                                  // callback. Not for input.
{
  CHECK(params.IsValid());
}

void AudioDeviceStatsReporter::ReportCallback(
    base::TimeDelta delay,
    const media::AudioGlitchInfo& glitch_info) {
  // In the case of output streams, when the stream is started, the first
  // callback always contains a delay of 0 and empty glitch info. This should
  // not be included in the stats. See sync_reader.cc.
  if (!discarded_first_callback_) {
    discarded_first_callback_ = true;
    return;
  }

  delay_log_callback_.Run(delay.InMilliseconds());

  ++stats_.callback_count;
  stats_.glitch_count += glitch_info.count;
  stats_.glitch_duration += glitch_info.duration;

  if (stats_.callback_count >= 1000) {
    UploadStats(stats_, SamplingPeriod::kIntervals);
    stats_ = {};
    stream_is_short_ = false;
  }
}

AudioDeviceStatsReporter::~AudioDeviceStatsReporter() {
  if (stream_is_short_ && stats_.callback_count > 0) {
    UploadStats(stats_, SamplingPeriod::kShort);
  }
}

void AudioDeviceStatsReporter::UploadStats(const Stats& stats,
                                           SamplingPeriod sampling_period) {
  base::TimeDelta stats_duration = callback_duration_ * stats.callback_count;
  DCHECK(stats_duration.is_positive());
  int glitch_duration_permille =
      std::round(1000 * stats.glitch_duration / stats_duration);

  glitch_count_log_callback_.Run(stats.glitch_count, sampling_period);
  glitch_duration_log_callback_.Run(glitch_duration_permille, sampling_period);
}

// Used to generate callbacks for:
// Media.AudioOutputDevice.AudioServiceGlitchCount.*.*
// Media.AudioOutputDevice.AudioServiceDroppedAudio.*.*
// Media.AudioInputDevice.AudioServiceGlitchCount.*
// Media.AudioInputDevice.AudioServiceDroppedAudio.*
// |latency| is ignored for input.
AudioDeviceStatsReporter::AggregateLogCallback
AudioDeviceStatsReporter::CreateAggregateCallback(
    const std::string& stat_name,
    media::AudioLatency::Type latency,
    int max_value,
    size_t bucket_count,
    Type type) {
  std::string base_name(base::StrCat(
      {type == Type::kOutput ? "Media.AudioOutputDevice.AudioService"
                             : "Media.AudioInputDevice.AudioService",
       stat_name}));
  std::string short_name(base::StrCat({base_name, ".Short"}));
  std::string intervals_name(base::StrCat({base_name, ".Intervals"}));

  if (type == Type::kInput) {
    return base::BindRepeating(
        [](int max_value, size_t bucket_count, const std::string& short_name,
           const std::string& intervals_name, int value,
           SamplingPeriod sampling_period) {
          if (sampling_period == SamplingPeriod::kShort) {
            base::UmaHistogramCustomCounts(short_name, value, 1, max_value,
                                           bucket_count);
          } else {
            base::UmaHistogramCustomCounts(intervals_name, value, 1, max_value,
                                           bucket_count);
          }
        },
        max_value, bucket_count, std::move(short_name),
        std::move(intervals_name));
  }

  std::string short_with_latency_name(
      base::StrCat({short_name, ".", AudioLatency::ToString(latency)}));
  std::string intervals_with_latency_name(
      base::StrCat({intervals_name, ".", AudioLatency::ToString(latency)}));

  return base::BindRepeating(
      [](int max_value, size_t bucket_count, const std::string& short_name,
         const std::string& intervals_name,
         const std::string& short_with_latency_name,
         const std::string& intervals_with_latency_name, int value,
         SamplingPeriod sampling_period) {
        if (sampling_period == SamplingPeriod::kShort) {
          base::UmaHistogramCustomCounts(short_name, value, 1, max_value,
                                         bucket_count);
          base::UmaHistogramCustomCounts(short_with_latency_name, value, 1,
                                         max_value, bucket_count);
        } else {
          base::UmaHistogramCustomCounts(intervals_name, value, 1, max_value,
                                         bucket_count);
          base::UmaHistogramCustomCounts(intervals_with_latency_name, value, 1,
                                         max_value, bucket_count);
        }
      },
      max_value, bucket_count, std::move(short_name), std::move(intervals_name),
      std::move(short_with_latency_name),
      std::move(intervals_with_latency_name));
}

// Used to generate callbacks for:
// Media.AudioOutputDevice.AudioServiceDelay.*
// Media.AudioInputDevice.AudioServiceDelay.*
// |latency| is ignored for input.
AudioDeviceStatsReporter::RealtimeLogCallback
AudioDeviceStatsReporter::CreateRealtimeCallback(
    const std::string& stat_name,
    media::AudioLatency::Type latency,
    int max_value,
    size_t bucket_count,
    Type type) {
  std::string base_name(base::StrCat(
      {type == Type::kOutput ? "Media.AudioOutputDevice.AudioService"
                             : "Media.AudioInputDevice.AudioService",
       stat_name}));
  std::string base_with_latency_name(
      base::StrCat({base_name, ".", AudioLatency::ToString(latency)}));

  // Since this callback will be called on every call to ReportCallback(), we
  // pre-fetch the histograms for efficiency, like the histogram macros do. Note
  // that we cannot use the macros here because the histogram names are
  // dynamically generated, which is not allowed by the macros.
  base::HistogramBase* histogram = base::Histogram::FactoryGet(
      std::move(base_name), 1, max_value, bucket_count,
      base::HistogramBase::kUmaTargetedHistogramFlag);

  if (type == Type::kInput) {
    // Histogram pointers from FactoryGet are not owned by the caller. They are
    // never deleted, see crbug.com/79322
    return base::BindRepeating([](base::HistogramBase* histogram,
                                  int value) { histogram->Add(value); },
                               base::Unretained(histogram));
  }

  base::HistogramBase* histogram_with_latency = base::Histogram::FactoryGet(
      std::move(base_with_latency_name), 1, max_value, bucket_count,
      base::HistogramBase::kUmaTargetedHistogramFlag);

  // Histogram pointers from FactoryGet are not owned by the caller. They are
  // never deleted, see crbug.com/79322
  return base::BindRepeating(
      [](base::HistogramBase* histogram,
         base::HistogramBase* histogram_with_latency, int value) {
        histogram->Add(value);
        histogram_with_latency->Add(value);
      },
      base::Unretained(histogram), base::Unretained(histogram_with_latency));
}

}  // namespace media
