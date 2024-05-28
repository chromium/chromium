// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_DEVICE_STATS_REPORTER_H_
#define MEDIA_AUDIO_AUDIO_DEVICE_STATS_REPORTER_H_

#include <list>
#include <string>
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_latency.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_export.h"

namespace media {

// Uploads audio UMA stats at the AudioOutputDevice/AudioInputDevice level.
// Uploads Short stats on destruction, for streams shorter than 1000 callbacks.
// For streams of at least 1000 callbacks it uploads Interval stats every 1000
// callbacks and drops any unuploaded stats on destruction.
class MEDIA_EXPORT AudioDeviceStatsReporter {
 public:
  enum class Type { kInput, kOutput };

  AudioDeviceStatsReporter(const AudioParameters& params, Type type);
  AudioDeviceStatsReporter(const AudioDeviceStatsReporter&) = delete;
  AudioDeviceStatsReporter& operator=(const AudioDeviceStatsReporter&) = delete;
  virtual ~AudioDeviceStatsReporter();

  // In the case when type is kOutput, this be called by AudioOutputDevice
  // every time it pulls more data using
  // AudioRendererSink::RenderCallback::Render(). In the case of kInput, it
  // should be called by AudioInputDevice every time it pushes data using
  // AudioCapturerSource::CaptureCallback::Capture(). Uploads Interval stats
  // every 1000 callbacks.
  void ReportCallback(base::TimeDelta delay,
                      const media::AudioGlitchInfo& glitch_info);

 private:
  // Indicates what period samples are aggregated over. kShort means entire
  // streams of less than 1000 callbacks, kIntervals means exactly 1000
  // callbacks.
  enum class SamplingPeriod { kShort, kIntervals };

  struct Stats {
    int callback_count = 0;
    int glitch_count = 0;
    base::TimeDelta glitch_duration;
  };

  // Logs data aggregated over intervals.
  using AggregateLogCallback =
      base::RepeatingCallback<void(int value, SamplingPeriod sampling_period)>;

  // Logs data on every callback.
  using RealtimeLogCallback = base::RepeatingCallback<void(int value)>;

  static AggregateLogCallback CreateAggregateCallback(
      const std::string& stat_name,
      media::AudioLatency::Type latency,
      int max_value,
      size_t bucket_count,
      Type type);

  static RealtimeLogCallback CreateRealtimeCallback(
      const std::string& stat_name,
      media::AudioLatency::Type latency,
      int max_value,
      size_t bucket_count,
      Type type);

  void UploadStats(const Stats& stats, SamplingPeriod sampling_period);

  Stats stats_;

  // The duration that a single callback covers.
  const base::TimeDelta callback_duration_;

  // Callback functions for writing to the histograms.
  const RealtimeLogCallback delay_log_callback_;
  const AggregateLogCallback glitch_count_log_callback_;
  const AggregateLogCallback glitch_duration_log_callback_;

  // Whether the stream is shorter than 1000 callbacks.
  bool stream_is_short_ = true;

  // Whether or not we have received and discarded the data from the first
  // callback. For input, this is always true, as the first callback should not
  // be discarded.
  bool discarded_first_callback_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_DEVICE_STATS_REPORTER_H_
