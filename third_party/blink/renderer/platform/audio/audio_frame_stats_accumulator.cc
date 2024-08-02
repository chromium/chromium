// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/audio/audio_frame_stats_accumulator.h"

#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "media/base/audio_timestamp_helper.h"

namespace blink {

void AudioFrameStatsAccumulator::Update(
    size_t observed_frames,
    int sample_rate,
    base::TimeDelta latency,
    const media::AudioGlitchInfo& glitch_info) {
  observed_frames_ += observed_frames;
  observed_frames_duration_ += media::AudioTimestampHelper::FramesToTime(
      base::saturated_cast<int64_t>(observed_frames), sample_rate);
  glitch_frames_ += media::AudioTimestampHelper::TimeToFrames(
      glitch_info.duration, sample_rate);
  glitch_frames_duration_ += glitch_info.duration;
  glitch_event_count_ += glitch_info.count;
  last_latency_ = latency;
  MergeLatencyExtremes(latency, latency);
  interval_frames_ += observed_frames;
  interval_frames_latency_sum_ += latency * observed_frames;
}

void AudioFrameStatsAccumulator::Absorb(AudioFrameStatsAccumulator& from) {
  // |from| should have newer stats, so |from|'s counters should be at least as
  // high as |this|.
  CHECK_GE(static_cast<uint64_t>(from.observed_frames_),
           static_cast<uint64_t>(observed_frames_));
  CHECK_GE(from.observed_frames_duration_, observed_frames_duration_);
  CHECK_GE(static_cast<uint64_t>(from.glitch_frames_),
           static_cast<uint64_t>(glitch_frames_));
  CHECK_GE(from.glitch_frames_duration_, glitch_frames_duration_);
  CHECK_GE(from.glitch_event_count_, glitch_event_count_);

  // Copy the non-interval stats.
  observed_frames_ = from.observed_frames_;
  observed_frames_duration_ = from.observed_frames_duration_;
  glitch_frames_ = from.glitch_frames_;
  glitch_frames_duration_ = from.glitch_frames_duration_;
  glitch_event_count_ = from.glitch_event_count_;
  last_latency_ = from.last_latency_;

  // Add |from|'s interval stats to ours before resetting them on |from|.
  MergeLatencyExtremes(from.interval_minimum_latency_,
                       from.interval_maximum_latency_);
  interval_frames_ += from.interval_frames_;
  interval_frames_latency_sum_ += from.interval_frames_latency_sum_;

  // Reset the interval stats in the absorbed object as they have now been
  // moved. The minimum and maximum latency should be set to the last latency,
  // in accordance with the spec.
  from.interval_frames_ = 0;
  from.interval_frames_latency_sum_ = base::TimeDelta();
  from.interval_minimum_latency_ = last_latency_;
  from.interval_maximum_latency_ = last_latency_;
}

void AudioFrameStatsAccumulator::MergeLatencyExtremes(
    base::TimeDelta new_minumum,
    base::TimeDelta new_maximum) {
  // If we already have latency stats, we need to merge them.
  if (interval_frames_ > 0) {
    interval_minimum_latency_ =
        std::min(interval_minimum_latency_, new_minumum);
    interval_maximum_latency_ =
        std::max(interval_maximum_latency_, new_maximum);
  } else {
    interval_minimum_latency_ = new_minumum;
    interval_maximum_latency_ = new_maximum;
  }
}

}  // namespace blink
