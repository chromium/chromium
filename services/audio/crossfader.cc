// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/crossfader.h"

#include <cmath>
#include <numbers>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "media/base/audio_bus.h"

namespace audio {

namespace {

// Computes w[n] = sin^2(pi * (n + 1) / (2 * num_frames)).
base::HeapArray<float> ComputeFadeInWindow(int num_frames) {
  CHECK_GT(num_frames, 0);
  auto window = base::HeapArray<float>::Uninit(num_frames);
  const float phase_step =
      std::numbers::pi_v<float> / static_cast<float>(2 * num_frames);
  for (int i = 0; i < num_frames; ++i) {
    const float sine = std::sin(phase_step * static_cast<float>(i + 1));
    window[i] = sine * sine;
  }
  return window;
}

}  // namespace

Crossfader::Crossfader(int frames_per_buffer)
    : fade_in_window_(ComputeFadeInWindow(frames_per_buffer)) {}

Crossfader::~Crossfader() = default;

void Crossfader::Crossfade(const media::AudioBus& from,
                           const media::AudioBus& to,
                           media::AudioBus& destination) const {
  DCHECK_EQ(from.channels(), destination.channels());
  DCHECK_EQ(to.channels(), destination.channels());
  DCHECK_EQ(from.frames(), destination.frames());
  DCHECK_EQ(to.frames(), destination.frames());
  DCHECK_EQ(static_cast<size_t>(destination.frames()), fade_in_window_.size());

  // Mix complementary gains. Each sample is read before it is written, which
  // makes aliasing `destination` with `from` or `to` safe.
  for (int ch = 0; ch < destination.channels(); ++ch) {
    base::span<const float> from_channel = from.channel(ch);
    base::span<const float> to_channel = to.channel(ch);
    base::span<float> destination_channel = destination.channel(ch);
    for (size_t i = 0; i < fade_in_window_.size(); ++i) {
      const float fade_in = fade_in_window_[i];
      destination_channel[i] =
          (1.0f - fade_in) * from_channel[i] + fade_in * to_channel[i];
    }
  }
}

}  // namespace audio
