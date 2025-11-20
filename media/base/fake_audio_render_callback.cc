// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/fake_audio_render_callback.h"

#include <algorithm>
#include <numbers>

#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_timestamp_helper.h"

namespace media {

FakeAudioRenderCallback::FakeAudioRenderCallback(double step, int sample_rate)
    : half_fill_(false),
      step_(step),
      last_delay_(base::TimeDelta::Max()),
      last_channel_count_(-1),
      volume_(1),
      sample_rate_(sample_rate) {
  reset();
}

FakeAudioRenderCallback::~FakeAudioRenderCallback() = default;

int FakeAudioRenderCallback::Render(base::TimeDelta delay,
                                    base::TimeTicks delay_timestamp,
                                    const AudioGlitchInfo& glitch_info,
                                    AudioBus* audio_bus) {
  cumulative_glitch_info_ += glitch_info;
  return RenderInternal(audio_bus, delay, volume_);
}

double FakeAudioRenderCallback::ProvideInput(
    AudioBus* audio_bus,
    uint32_t frames_delayed,
    const AudioGlitchInfo& glitch_info) {
  cumulative_glitch_info_ += glitch_info;
  // Volume should only be applied by the caller to ProvideInput, so don't bake
  // it into the rendered audio.
  auto delay = AudioTimestampHelper::FramesToTime(frames_delayed, sample_rate_);
  RenderInternal(audio_bus, delay, 1.0);
  return volume_;
}

int FakeAudioRenderCallback::RenderInternal(AudioBus* audio_bus,
                                            base::TimeDelta delay,
                                            double volume) {
  DCHECK_LE(delay, base::TimeDelta::Max());
  last_delay_ = delay;
  last_channel_count_ = audio_bus->channels();

  size_t number_of_frames = static_cast<size_t>(audio_bus->frames());
  if (half_fill_)
    number_of_frames /= 2;

  auto first_channel = audio_bus->channel_span(0);

  // Fill first channel with a sine wave.
  for (size_t i = 0; i < number_of_frames; ++i) {
    first_channel[i] = sin(2 * std::numbers::pi * (x_ + step_ * i)) * volume;
  }
  x_ += number_of_frames * step_;

  // Matches the AudioRenderer::InputFadeInHelper implementation.
  if (needs_fade_in_) {
    constexpr base::TimeDelta kFadeInDuration = base::Milliseconds(5);

    const int fade_in_frames =
        std::min(static_cast<int>(AudioTimestampHelper::TimeToFrames(
                     kFadeInDuration, sample_rate_)),
                 audio_bus->frames());

    for (int i = 0; i < fade_in_frames; ++i) {
      first_channel[i] = first_channel[i] * i / fade_in_frames;
    }

    needs_fade_in_ = false;
  }

  // Copy first channel into the rest of the channels.
  auto first_channel_data = first_channel.first(number_of_frames);
  for (int i = 1; i < audio_bus->channels(); ++i) {
    audio_bus->channel_span(i)
        .first(number_of_frames)
        .copy_from_nonoverlapping(first_channel_data);
  }

  return number_of_frames;
}

}  // namespace media
