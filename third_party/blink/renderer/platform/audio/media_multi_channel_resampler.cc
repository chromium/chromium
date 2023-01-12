// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/audio/media_multi_channel_resampler.h"

#include <memory>
#include "base/functional/bind.h"
#include "media/base/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"

namespace blink {

MediaMultiChannelResampler::MediaMultiChannelResampler(
    int channels,
    double io_sample_rate_ratio,
    uint32_t request_frames,
    ReadCB read_cb)
    : resampler_input_bus_wrapper_(media::AudioBus::CreateWrapper(channels)),
      resampler_output_bus_wrapper_(
          AudioBus::Create(channels, request_frames, false)),
      read_cb_(std::move(read_cb)) {
  resampler_ = std::make_unique<media::MultiChannelResampler>(
      channels, io_sample_rate_ratio, request_frames,
      base::BindRepeating(&MediaMultiChannelResampler::ProvideResamplerInput,
                          base::Unretained(this)));
}

void MediaMultiChannelResampler::Resample(
    int frames,
    blink::AudioBus* resampler_input_bus) {
  CHECK_EQ(static_cast<int>(resampler_input_bus->NumberOfChannels()),
            resampler_input_bus_wrapper_->channels());

  for (unsigned int i = 0; i < resampler_input_bus->NumberOfChannels(); ++i) {
    resampler_input_bus_wrapper_->SetChannelData(
        i, resampler_input_bus->Channel(i)->MutableData());
  }
  resampler_input_bus_wrapper_->set_frames(resampler_input_bus->length());
  ResampleInternal(frames, resampler_input_bus_wrapper_.get());
}

void MediaMultiChannelResampler::ResampleInternal(
    int frames,
    media::AudioBus* resampler_input_bus) {
  resampler_->Resample(frames, resampler_input_bus);
}

void MediaMultiChannelResampler::ProvideResamplerInput(
    int resampler_frame_delay,
    media::AudioBus* resampler_output_bus) {
  CHECK_EQ(static_cast<int>(resampler_output_bus_wrapper_->NumberOfChannels()),
            resampler_output_bus->channels());

  for (int i = 0; i < resampler_output_bus->channels(); ++i) {
    resampler_output_bus_wrapper_->SetChannelMemory(
        i, resampler_output_bus->channel(i), resampler_output_bus->frames());
  }
  read_cb_.Run(resampler_frame_delay, resampler_output_bus_wrapper_.get());
}

}  // namespace blink
