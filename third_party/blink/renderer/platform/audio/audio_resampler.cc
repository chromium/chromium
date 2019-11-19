/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include <algorithm>
#include <memory>
#include "third_party/blink/renderer/platform/audio/audio_resampler.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

const double AudioResampler::kMaxRate = 8.0;

AudioResampler::AudioResampler() : rate_(1.0) {
  kernels_.push_back(std::make_unique<AudioResamplerKernel>(this));
  source_bus_ = AudioBus::Create(1, 0, false);
}

AudioResampler::AudioResampler(unsigned number_of_channels) : rate_(1.0) {
  for (unsigned i = 0; i < number_of_channels; ++i)
    kernels_.push_back(std::make_unique<AudioResamplerKernel>(this));

  source_bus_ = AudioBus::Create(number_of_channels, 0, false);
}

void AudioResampler::ConfigureChannels(unsigned number_of_channels) {
  unsigned current_size = kernels_.size();
  if (number_of_channels == current_size)
    return;  // already setup

  // First deal with adding or removing kernels.
  if (number_of_channels > current_size) {
    for (unsigned i = current_size; i < number_of_channels; ++i)
      kernels_.push_back(std::make_unique<AudioResamplerKernel>(this));
  } else
    kernels_.resize(number_of_channels);

  // Reconfigure our source bus to the new channel size.
  source_bus_ = AudioBus::Create(number_of_channels, 0, false);
}

void AudioResampler::Process(AudioSourceProvider* provider,
                             AudioBus* destination_bus,
                             uint32_t frames_to_process) {
  DCHECK(provider);

  unsigned number_of_channels = kernels_.size();

  // Make sure our configuration matches the bus we're rendering to.
  DCHECK(destination_bus);
  DCHECK_EQ(destination_bus->NumberOfChannels(), number_of_channels);

  // Setup the source bus.
  for (unsigned i = 0; i < number_of_channels; ++i) {
    // Figure out how many frames we need to get from the provider, and a
    // pointer to the buffer.
    size_t frames_needed;
    float* fill_pointer =
        kernels_[i]->GetSourcePointer(frames_to_process, &frames_needed);
    DCHECK(fill_pointer);

    source_bus_->SetChannelMemory(i, fill_pointer, frames_needed);
  }

  // Ask the provider to supply the desired number of source frames.
  provider->ProvideInput(source_bus_.get(), source_bus_->length());

  // Now that we have the source data, resample each channel into the
  // destination bus.
  // FIXME: optimize for the common stereo case where it's faster to process
  // both left/right channels in the same inner loop.
  for (unsigned i = 0; i < number_of_channels; ++i) {
    float* destination = destination_bus->Channel(i)->MutableData();
    kernels_[i]->Process(destination, frames_to_process);
  }
}

void AudioResampler::SetRate(double rate) {
  if (std::isnan(rate) || std::isinf(rate) || rate <= 0.0)
    return;

  rate_ = std::min(AudioResampler::kMaxRate, rate);
}

void AudioResampler::Reset() {
  unsigned number_of_channels = kernels_.size();
  for (unsigned i = 0; i < number_of_channels; ++i)
    kernels_[i]->Reset();
}

}  // namespace blink
