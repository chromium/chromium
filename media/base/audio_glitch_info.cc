// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_glitch_info.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"

namespace media {

std::string AudioGlitchInfo::ToString() const {
  return base::StrCat(
      {"duration (ms): ", base::NumberToString(duration.InMilliseconds()),
       ", count: ", base::NumberToString(count)});
}

void AudioGlitchInfo::MaybeAddTraceEvent() const {
  if (!count) {
    return;
  }
  TRACE_EVENT_INSTANT("audio", "Glitch!", "glitch count", count,
                      "glitch duration", duration);
}

// static
AudioGlitchInfo AudioGlitchInfo::SingleBoundedSystemGlitch(
    const base::TimeDelta duration,
    const Direction direction) {
  CHECK(duration.is_positive());
  if (direction == Direction::kRender) {
    UMA_HISTOGRAM_LONG_TIMES("Media.Audio.Render.SystemGlitchDuration",
                             duration);
  } else {
    UMA_HISTOGRAM_LONG_TIMES("Media.Audio.Capture.SystemGlitchDuration",
                             duration);
  }
  return AudioGlitchInfo{
      .duration = std::clamp(duration, base::Seconds(0), base::Seconds(1)),
      .count = 1};
}

AudioGlitchInfo& AudioGlitchInfo::operator+=(const AudioGlitchInfo& other) {
  duration += other.duration;
  count += other.count;
  return *this;
}

bool operator==(const AudioGlitchInfo& lhs, const AudioGlitchInfo& rhs) {
  return lhs.duration == rhs.duration && lhs.count == rhs.count;
}

AudioGlitchInfo::Accumulator::Accumulator() = default;
AudioGlitchInfo::Accumulator::~Accumulator() = default;

void AudioGlitchInfo::Accumulator::Add(const AudioGlitchInfo& info) {
  pending_info_ += info;
}

AudioGlitchInfo AudioGlitchInfo::Accumulator::GetAndReset() {
  AudioGlitchInfo temp = std::move(pending_info_);
  pending_info_ = {};
  return temp;
}

}  // namespace media
