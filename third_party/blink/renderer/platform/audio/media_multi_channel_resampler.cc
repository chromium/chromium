// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/audio/media_multi_channel_resampler.h"

#include <memory>
#include "base/bind.h"
#include "media/base/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"

namespace blink {

MediaMultiChannelResampler::MediaMultiChannelResampler(
    int channels,
    double io_sample_rate_ratio,
    size_t request_frames,
    ReadCB read_cb)
    : read_cb_(std::move(read_cb)) {
  resampler_.reset(new media::MultiChannelResampler(
      channels, io_sample_rate_ratio, request_frames,
      base::BindRepeating(&MediaMultiChannelResampler::ProvideResamplerInput,
                          base::Unretained(this))));
}

void MediaMultiChannelResampler::Resample(int frames,
                                          media::AudioBus* audio_bus) {
  resampler_->Resample(audio_bus->frames(), audio_bus);
}

void MediaMultiChannelResampler::ProvideResamplerInput(
    int resampler_frame_delay,
    media::AudioBus* dest) {
  // Create a blink::AudioBus wrapper around the memory provided by the
  // media::AudioBus.
  scoped_refptr<AudioBus> bus =
      AudioBus::Create(dest->channels(), dest->frames(), false);
  for (int i = 0; i < dest->channels(); ++i) {
    bus->SetChannelMemory(i, dest->channel(i), dest->frames());
  }
  read_cb_.Run(resampler_frame_delay, bus.get());
}

}  // namespace blink
