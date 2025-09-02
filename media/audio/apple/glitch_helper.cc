// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/apple/glitch_helper.h"

#include "base/check_op.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "media/base/audio_timestamp_helper.h"

namespace {
media::SystemGlitchReporter::StreamType StreamTypeFromGlitchDirection(
    media::AudioGlitchInfo::Direction direction) {
  switch (direction) {
    case media::AudioGlitchInfo::Direction::kRender:
      return media::SystemGlitchReporter::StreamType::kRender;
    case media::AudioGlitchInfo::Direction::kCapture:
      return media::SystemGlitchReporter::StreamType::kCapture;
    case media::AudioGlitchInfo::Direction::kLoopback:
      return media::SystemGlitchReporter::StreamType::kLoopback;
  }
}
}  // namespace

namespace media {

GlitchHelper::GlitchHelper(int sample_rate,
                           AudioGlitchInfo::Direction direction)
    : sample_rate_(sample_rate),
      direction_(direction),
      glitch_reporter_(StreamTypeFromGlitchDirection(direction)) {}

// This compares timestamps between two calls. We expect that a new timestamp
// should be equal to the previous one + the number of frames in the previous
// call. If the timestamp difference is too big, we interpret that as missing
// audio (i.e. glitches). Any detected glitches will be added to
// `Media.Audio.{Type}.SystemGlitchDuration` UMA histogram.
void GlitchHelper::OnFramesReceived(const AudioTimeStamp& timestamp,
                                    UInt32 frames) {
  if ((timestamp.mFlags & kAudioTimeStampSampleTimeValid) == 0) {
    // If we don't update the `last_sample_time_` we will report glitches if we
    // start to get valid timestamps again.
    last_sample_time_ = last_sample_time_ + frames;
    last_number_of_frames_ = frames;
    return;
  }

  // mSampleTime can get reset, e.g. if the sample rate is changed on the
  // speaker in the macOS settings, while capturing loopback. If the current
  // mSampleTime is lower than the last sample time, we know it was reset
  // and skip the glitch calculation. The current mSampleTime will still be
  // saved, so we can restart the glitch calculations on the next call to
  // `OnFramesReceived()`.
  if (last_sample_time_ && timestamp.mSampleTime > last_sample_time_) {
    DCHECK_NE(0U, last_number_of_frames_);
    UInt32 sample_time_diff =
        static_cast<UInt32>(timestamp.mSampleTime - last_sample_time_);
    DCHECK_GE(sample_time_diff, last_number_of_frames_);
    UInt32 lost_frames = sample_time_diff - last_number_of_frames_;
    base::TimeDelta lost_audio_duration =
        AudioTimestampHelper::FramesToTime(lost_frames, sample_rate_);
    glitch_reporter_.UpdateStats(lost_audio_duration);
    if (lost_audio_duration.is_positive()) {
      glitch_accumulator_.Add(AudioGlitchInfo::SingleBoundedSystemGlitch(
          lost_audio_duration, direction_));
    }
  }

  // Store the last sample time for use next time we get called back.
  last_sample_time_ = timestamp.mSampleTime;
  // Store the number of frames for this call, so we can estimate what the next
  // timestamp should be.
  last_number_of_frames_ = frames;
}

AudioGlitchInfo GlitchHelper::ConsumeGlitchInfo() {
  AudioGlitchInfo glitch_info = glitch_accumulator_.GetAndReset();
  glitch_info.MaybeAddTraceEvent();
  return glitch_info;
}

std::optional<std::string> GlitchHelper::LogAndReset(
    const std::string& prefix) {
  if (!HasData()) {
    return std::nullopt;  // No stats gathered to report.
  }

  SystemGlitchReporter::Stats stats =
      glitch_reporter_.GetLongTermStatsAndReset();

  std::optional<std::string> log_message =
      std::make_optional(base::StringPrintf(
          "%s: (num_glitches_detected=[%d], cumulative_audio_lost=[%llu "
          "ms], "
          "largest_glitch=[%llu ms])",
          prefix.c_str(), stats.glitches_detected,
          stats.total_glitch_duration.InMilliseconds(),
          stats.largest_glitch_duration.InMilliseconds()));

  if (stats.glitches_detected != 0) {
    DLOG(WARNING) << *log_message;
  }

  last_sample_time_ = 0;
  last_number_of_frames_ = 0;

  return log_message;
}

// False if no calls to `OnFramesReceived()` have been done, or if no
// calls to `OnFramesReceived()` have been done after a `LogAndReset()`.
bool GlitchHelper::HasData() {
  return last_sample_time_ != 0;
}

}  // namespace media
