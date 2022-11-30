// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_glitch_info.h"

#include <utility>

namespace media {

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
