/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/audio/multi_channel_resampler.h"

#include <memory>
#include "third_party/blink/renderer/platform/audio/audio_bus.h"

namespace blink {

namespace {

// ChannelProvider provides a single channel of audio data (one channel at a
// time) for each channel of data provided to us in a multi-channel provider.

class ChannelProvider final : public AudioSourceProvider {
 public:
  ChannelProvider(AudioSourceProvider* multi_channel_provider,
                  unsigned number_of_channels)
      : multi_channel_provider_(multi_channel_provider),
        number_of_channels_(number_of_channels),
        current_channel_(0),
        frames_to_process_(0) {}

  // provideInput() will be called once for each channel, starting with the
  // first channel.  Each time it's called, it will provide the next channel of
  // data.
  void ProvideInput(AudioBus* bus, uint32_t frames_to_process) override {
    DCHECK(bus);
    DCHECK_EQ(bus->NumberOfChannels(), 1u);

    // Get the data from the multi-channel provider when the first channel asks
    // for it.  For subsequent channels, we can just dish out the channel data
    // from that (stored in m_multiChannelBus).
    if (!current_channel_) {
      frames_to_process_ = frames_to_process;
      multi_channel_bus_ =
          AudioBus::Create(number_of_channels_, frames_to_process);
      multi_channel_provider_->ProvideInput(multi_channel_bus_.get(),
                                            frames_to_process);
    }

    DCHECK(multi_channel_bus_.get());
    DCHECK_EQ(frames_to_process, frames_to_process_);

    // Copy the channel data from what we received from m_multiChannelProvider.
    DCHECK_LT(current_channel_, number_of_channels_);
    memcpy(bus->Channel(0)->MutableData(),
           multi_channel_bus_->Channel(current_channel_)->Data(),
           sizeof(float) * frames_to_process);
    ++current_channel_;
  }

 private:
  AudioSourceProvider* multi_channel_provider_;
  scoped_refptr<AudioBus> multi_channel_bus_;
  unsigned number_of_channels_;
  unsigned current_channel_;
  // Used to verify that all channels ask for the same amount.
  uint32_t frames_to_process_;
};

}  // namespace

MultiChannelResampler::MultiChannelResampler(double scale_factor,
                                             unsigned number_of_channels)
    : number_of_channels_(number_of_channels) {
  // Create each channel's resampler.
  for (unsigned channel_index = 0; channel_index < number_of_channels;
       ++channel_index)
    kernels_.push_back(std::make_unique<SincResampler>(scale_factor));
}

void MultiChannelResampler::Process(AudioSourceProvider* provider,
                                    AudioBus* destination,
                                    uint32_t frames_to_process) {
  // The provider can provide us with multi-channel audio data. But each of our
  // single-channel resamplers (kernels) below requires a provider which
  // provides a single unique channel of data.  channelProvider wraps the
  // original multi-channel provider and dishes out one channel at a time.
  ChannelProvider channel_provider(provider, number_of_channels_);

  for (unsigned channel_index = 0; channel_index < number_of_channels_;
       ++channel_index) {
    // Depending on the sample-rate scale factor, and the internal buffering
    // used in a SincResampler kernel, this call to process() will only
    // sometimes call provideInput() on the channelProvider.  However, if it
    // calls provideInput() for the first channel, then it will call it for the
    // remaining channels, since they all buffer in the same way and are
    // processing the same number of frames.
    kernels_[channel_index]->Process(
        &channel_provider, destination->Channel(channel_index)->MutableData(),
        frames_to_process);
  }
}

}  // namespace blink
