// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/media_stream_track_platform.h"

#include "base/check_op.h"
#include "base/time/time.h"
#include "media/base/audio_timestamp_helper.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"

namespace blink {

// static
MediaStreamTrackPlatform* MediaStreamTrackPlatform::GetTrack(
    const WebMediaStreamTrack& track) {
  if (track.IsNull())
    return nullptr;

  MediaStreamComponent& component = *track;
  return component.GetPlatformTrack();
}

MediaStreamTrackPlatform::MediaStreamTrackPlatform(bool is_local_track)
    : is_local_track_(is_local_track) {}

MediaStreamTrackPlatform::~MediaStreamTrackPlatform() {}

MediaStreamTrackPlatform::CaptureHandle
MediaStreamTrackPlatform::GetCaptureHandle() {
  return MediaStreamTrackPlatform::CaptureHandle();
}

MediaStreamTrackPlatform::AudioFrameStats::AudioFrameStats() = default;

void MediaStreamTrackPlatform::AudioFrameStats::Update(
    const media::AudioParameters& params,
    base::TimeTicks capture_time,
    const media::AudioGlitchInfo& glitch_info) {
  base::TimeDelta current_latency = base::TimeTicks::Now() - capture_time;

  delivered_frames_ += params.frames_per_buffer();
  delivered_frames_duration_ += params.GetBufferDuration();
  glitch_frames_ += media::AudioTimestampHelper::TimeToFrames(
      glitch_info.duration, params.sample_rate());
  glitch_frames_duration_ += glitch_info.duration;
  last_latency_ = current_latency;
  MergeLatencyExtremes(current_latency, current_latency);
  interval_frames_ += params.frames_per_buffer();
  interval_frames_latency_sum_ += current_latency * params.frames_per_buffer();
}

void MediaStreamTrackPlatform::AudioFrameStats::Absorb(AudioFrameStats& from) {
  // |from| should have newer stats, so |from|'s counters should be at least as
  // high as |this|.
  CHECK_GE(from.delivered_frames_, delivered_frames_);
  CHECK_GE(from.delivered_frames_duration_, delivered_frames_duration_);
  CHECK_GE(from.glitch_frames_, glitch_frames_);
  CHECK_GE(from.glitch_frames_duration_, glitch_frames_duration_);

  // Copy the non-interval stats.
  delivered_frames_ = from.delivered_frames_;
  delivered_frames_duration_ = from.delivered_frames_duration_;
  glitch_frames_ = from.glitch_frames_;
  glitch_frames_duration_ = from.glitch_frames_duration_;
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

void MediaStreamTrackPlatform::AudioFrameStats::MergeLatencyExtremes(
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

size_t MediaStreamTrackPlatform::AudioFrameStats::DeliveredFrames() {
  return delivered_frames_;
}

base::TimeDelta
MediaStreamTrackPlatform::AudioFrameStats::DeliveredFramesDuration() {
  return delivered_frames_duration_;
}

size_t MediaStreamTrackPlatform::AudioFrameStats::TotalFrames() {
  return delivered_frames_ + glitch_frames_;
}

base::TimeDelta
MediaStreamTrackPlatform::AudioFrameStats::TotalFramesDuration() {
  return delivered_frames_duration_ + glitch_frames_duration_;
}

base::TimeDelta MediaStreamTrackPlatform::AudioFrameStats::Latency() {
  return last_latency_;
}

base::TimeDelta MediaStreamTrackPlatform::AudioFrameStats::AverageLatency() {
  return interval_frames_ > 0 ? interval_frames_latency_sum_ / interval_frames_
                              : last_latency_;
}

base::TimeDelta MediaStreamTrackPlatform::AudioFrameStats::MinimumLatency() {
  return interval_minimum_latency_;
}

base::TimeDelta MediaStreamTrackPlatform::AudioFrameStats::MaximumLatency() {
  return interval_maximum_latency_;
}

}  // namespace blink
