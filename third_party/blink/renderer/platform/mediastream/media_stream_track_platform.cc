// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/media_stream_track_platform.h"

#include "base/numerics/clamped_math.h"
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

void MediaStreamTrackPlatform::AudioFrameStats::Update(
    const media::AudioParameters& params,
    base::TimeTicks capture_time,
    const media::AudioGlitchInfo& glitch_info) {
  accumulator_.Update(params.frames_per_buffer(), params.sample_rate(),
                      base::TimeTicks::Now() - capture_time, glitch_info);
}

void MediaStreamTrackPlatform::AudioFrameStats::Absorb(AudioFrameStats& from) {
  accumulator_.Absorb(from.accumulator_);
}

uint64_t MediaStreamTrackPlatform::AudioFrameStats::DeliveredFrames() const {
  return accumulator_.observed_frames();
}

base::TimeDelta
MediaStreamTrackPlatform::AudioFrameStats::DeliveredFramesDuration() const {
  return accumulator_.observed_frames_duration();
}

uint64_t MediaStreamTrackPlatform::AudioFrameStats::TotalFrames() const {
  return base::MakeClampedNum(accumulator_.observed_frames()) +
         base::MakeClampedNum(accumulator_.glitch_frames());
}

base::TimeDelta MediaStreamTrackPlatform::AudioFrameStats::TotalFramesDuration()
    const {
  return accumulator_.observed_frames_duration() +
         accumulator_.glitch_frames_duration();
}

base::TimeDelta MediaStreamTrackPlatform::AudioFrameStats::Latency() const {
  return accumulator_.latency();
}

base::TimeDelta MediaStreamTrackPlatform::AudioFrameStats::AverageLatency()
    const {
  return accumulator_.average_latency();
}

base::TimeDelta MediaStreamTrackPlatform::AudioFrameStats::MinimumLatency()
    const {
  return accumulator_.min_latency();
}

base::TimeDelta MediaStreamTrackPlatform::AudioFrameStats::MaximumLatency()
    const {
  return accumulator_.max_latency();
}

}  // namespace blink
