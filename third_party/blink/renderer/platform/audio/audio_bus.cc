/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/audio/audio_bus.h"

#include <assert.h>
#include <math.h>
#include <algorithm>
#include <memory>
#include <utility>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_audio_bus.h"
#include "third_party/blink/renderer/platform/audio/audio_file_reader.h"
#include "third_party/blink/renderer/platform/audio/denormal_disabler.h"
#include "third_party/blink/renderer/platform/audio/sinc_resampler.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "ui/base/resource/scale_factor.h"

namespace blink {

using vector_math::Vadd;
using vector_math::Vsma;

const unsigned kMaxBusChannels = 32;

scoped_refptr<AudioBus> AudioBus::Create(unsigned number_of_channels,
                                         uint32_t length,
                                         bool allocate) {
  DCHECK_LE(number_of_channels, kMaxBusChannels);
  if (number_of_channels > kMaxBusChannels)
    return nullptr;

  return base::AdoptRef(new AudioBus(number_of_channels, length, allocate));
}

AudioBus::AudioBus(unsigned number_of_channels, uint32_t length, bool allocate)
    : length_(length), sample_rate_(0) {
  channels_.ReserveInitialCapacity(number_of_channels);

  for (unsigned i = 0; i < number_of_channels; ++i) {
    std::unique_ptr<AudioChannel> channel =
        allocate ? std::make_unique<AudioChannel>(length)
                 : std::make_unique<AudioChannel>(nullptr, length);
    channels_.push_back(std::move(channel));
  }

  layout_ = kLayoutCanonical;  // for now this is the only layout we define
}

void AudioBus::SetChannelMemory(unsigned channel_index,
                                float* storage,
                                uint32_t length) {
  if (channel_index < channels_.size()) {
    Channel(channel_index)->Set(storage, length);
    // FIXME: verify that this length matches all the other channel lengths
    length_ = length;
  }
}

void AudioBus::ResizeSmaller(uint32_t new_length) {
  DCHECK_LE(new_length, length_);
  if (new_length <= length_)
    length_ = new_length;

  for (unsigned i = 0; i < channels_.size(); ++i)
    channels_[i]->ResizeSmaller(new_length);
}

void AudioBus::Zero() {
  for (unsigned i = 0; i < channels_.size(); ++i)
    channels_[i]->Zero();
}

AudioChannel* AudioBus::ChannelByType(unsigned channel_type) {
  // For now we only support canonical channel layouts...
  if (layout_ != kLayoutCanonical)
    return nullptr;

  switch (NumberOfChannels()) {
    case 1:  // mono
      if (channel_type == kChannelMono || channel_type == kChannelLeft)
        return Channel(0);
      return nullptr;

    case 2:  // stereo
      switch (channel_type) {
        case kChannelLeft:
          return Channel(0);
        case kChannelRight:
          return Channel(1);
        default:
          return nullptr;
      }

    case 4:  // quad
      switch (channel_type) {
        case kChannelLeft:
          return Channel(0);
        case kChannelRight:
          return Channel(1);
        case kChannelSurroundLeft:
          return Channel(2);
        case kChannelSurroundRight:
          return Channel(3);
        default:
          return nullptr;
      }

    case 5:  // 5.0
      switch (channel_type) {
        case kChannelLeft:
          return Channel(0);
        case kChannelRight:
          return Channel(1);
        case kChannelCenter:
          return Channel(2);
        case kChannelSurroundLeft:
          return Channel(3);
        case kChannelSurroundRight:
          return Channel(4);
        default:
          return nullptr;
      }

    case 6:  // 5.1
      switch (channel_type) {
        case kChannelLeft:
          return Channel(0);
        case kChannelRight:
          return Channel(1);
        case kChannelCenter:
          return Channel(2);
        case kChannelLFE:
          return Channel(3);
        case kChannelSurroundLeft:
          return Channel(4);
        case kChannelSurroundRight:
          return Channel(5);
        default:
          return nullptr;
      }
  }

  NOTREACHED();
  return nullptr;
}

const AudioChannel* AudioBus::ChannelByType(unsigned type) const {
  return const_cast<AudioBus*>(this)->ChannelByType(type);
}

// Returns true if the channel count and frame-size match.
bool AudioBus::TopologyMatches(const AudioBus& bus) const {
  if (NumberOfChannels() != bus.NumberOfChannels())
    return false;  // channel mismatch

  // Make sure source bus has enough frames.
  if (length() > bus.length())
    return false;  // frame-size mismatch

  return true;
}

scoped_refptr<AudioBus> AudioBus::CreateBufferFromRange(
    const AudioBus* source_buffer,
    unsigned start_frame,
    unsigned end_frame) {
  uint32_t number_of_source_frames = source_buffer->length();
  unsigned number_of_channels = source_buffer->NumberOfChannels();

  // Sanity checking
  bool is_range_safe =
      start_frame < end_frame && end_frame <= number_of_source_frames;
  DCHECK(is_range_safe);
  if (!is_range_safe)
    return nullptr;

  uint32_t range_length = end_frame - start_frame;

  scoped_refptr<AudioBus> audio_bus = Create(number_of_channels, range_length);
  audio_bus->SetSampleRate(source_buffer->SampleRate());

  for (unsigned i = 0; i < number_of_channels; ++i)
    audio_bus->Channel(i)->CopyFromRange(source_buffer->Channel(i), start_frame,
                                         end_frame);

  return audio_bus;
}

float AudioBus::MaxAbsValue() const {
  float max = 0.0f;
  for (unsigned i = 0; i < NumberOfChannels(); ++i) {
    const AudioChannel* channel = this->Channel(i);
    max = std::max(max, channel->MaxAbsValue());
  }

  return max;
}

void AudioBus::Normalize() {
  float max = MaxAbsValue();
  if (max)
    Scale(1.0f / max);
}

void AudioBus::Scale(float scale) {
  for (unsigned i = 0; i < NumberOfChannels(); ++i)
    Channel(i)->Scale(scale);
}

void AudioBus::CopyFrom(const AudioBus& source_bus,
                        ChannelInterpretation channel_interpretation) {
  if (&source_bus == this)
    return;

  // Copying bus is equivalent to zeroing and then summing.
  Zero();
  SumFrom(source_bus, channel_interpretation);
}

void AudioBus::SumFrom(const AudioBus& source_bus,
                       ChannelInterpretation channel_interpretation) {
  if (&source_bus == this)
    return;

  unsigned number_of_source_channels = source_bus.NumberOfChannels();
  unsigned number_of_destination_channels = NumberOfChannels();

  // If the channel numbers are equal, perform channels-wise summing.
  if (number_of_source_channels == number_of_destination_channels) {
    for (unsigned i = 0; i < number_of_source_channels; ++i)
      Channel(i)->SumFrom(source_bus.Channel(i));

    return;
  }

  // Otherwise perform up/down-mix or the discrete transfer based on the
  // number of channels and the channel interpretation.
  switch (channel_interpretation) {
    case kSpeakers:
      if (number_of_source_channels < number_of_destination_channels)
        SumFromByUpMixing(source_bus);
      else
        SumFromByDownMixing(source_bus);
      break;
    case kDiscrete:
      DiscreteSumFrom(source_bus);
      break;
  }
}

void AudioBus::DiscreteSumFrom(const AudioBus& source_bus) {
  unsigned number_of_source_channels = source_bus.NumberOfChannels();
  unsigned number_of_destination_channels = NumberOfChannels();

  if (number_of_destination_channels < number_of_source_channels) {
    // Down-mix by summing channels and dropping the remaining.
    for (unsigned i = 0; i < number_of_destination_channels; ++i)
      Channel(i)->SumFrom(source_bus.Channel(i));
  } else if (number_of_destination_channels > number_of_source_channels) {
    // Up-mix by summing as many channels as we have.
    for (unsigned i = 0; i < number_of_source_channels; ++i)
      Channel(i)->SumFrom(source_bus.Channel(i));
  }
}

void AudioBus::SumFromByUpMixing(const AudioBus& source_bus) {
  unsigned number_of_source_channels = source_bus.NumberOfChannels();
  unsigned number_of_destination_channels = NumberOfChannels();

  if ((number_of_source_channels == 1 && number_of_destination_channels == 2) ||
      (number_of_source_channels == 1 && number_of_destination_channels == 4)) {
    // Up-mixing: 1 -> 2, 1 -> 4
    //   output.L = input
    //   output.R = input
    //   output.SL = 0 (in the case of 1 -> 4)
    //   output.SR = 0 (in the case of 1 -> 4)
    const AudioChannel* source_l = source_bus.ChannelByType(kChannelLeft);
    ChannelByType(kChannelLeft)->SumFrom(source_l);
    ChannelByType(kChannelRight)->SumFrom(source_l);
  } else if (number_of_source_channels == 1 &&
             number_of_destination_channels == 6) {
    // Up-mixing: 1 -> 5.1
    //   output.L = 0
    //   output.R = 0
    //   output.C = input (put in center channel)
    //   output.LFE = 0
    //   output.SL = 0
    //   output.SR = 0
    ChannelByType(kChannelCenter)
        ->SumFrom(source_bus.ChannelByType(kChannelLeft));
  } else if ((number_of_source_channels == 2 &&
              number_of_destination_channels == 4) ||
             (number_of_source_channels == 2 &&
              number_of_destination_channels == 6)) {
    // Up-mixing: 2 -> 4, 2 -> 5.1
    //   output.L = input.L
    //   output.R = input.R
    //   output.C = 0 (in the case of 2 -> 5.1)
    //   output.LFE = 0 (in the case of 2 -> 5.1)
    //   output.SL = 0
    //   output.SR = 0
    ChannelByType(kChannelLeft)
        ->SumFrom(source_bus.ChannelByType(kChannelLeft));
    ChannelByType(kChannelRight)
        ->SumFrom(source_bus.ChannelByType(kChannelRight));
  } else if (number_of_source_channels == 4 &&
             number_of_destination_channels == 6) {
    // Up-mixing: 4 -> 5.1
    //   output.L = input.L
    //   output.R = input.R
    //   output.C = 0
    //   output.LFE = 0
    //   output.SL = input.SL
    //   output.SR = input.SR
    ChannelByType(kChannelLeft)
        ->SumFrom(source_bus.ChannelByType(kChannelLeft));
    ChannelByType(kChannelRight)
        ->SumFrom(source_bus.ChannelByType(kChannelRight));
    ChannelByType(kChannelSurroundLeft)
        ->SumFrom(source_bus.ChannelByType(kChannelSurroundLeft));
    ChannelByType(kChannelSurroundRight)
        ->SumFrom(source_bus.ChannelByType(kChannelSurroundRight));
  } else {
    // All other cases, fall back to the discrete sum. This will silence the
    // excessive channels.
    DiscreteSumFrom(source_bus);
  }
}

void AudioBus::SumFromByDownMixing(const AudioBus& source_bus) {
  unsigned number_of_source_channels = source_bus.NumberOfChannels();
  unsigned number_of_destination_channels = NumberOfChannels();

  if (number_of_source_channels == 2 && number_of_destination_channels == 1) {
    // Down-mixing: 2 -> 1
    //   output = 0.5 * (input.L + input.R)
    const float* source_l = source_bus.ChannelByType(kChannelLeft)->Data();
    const float* source_r = source_bus.ChannelByType(kChannelRight)->Data();

    float* destination = ChannelByType(kChannelLeft)->MutableData();
    float scale = 0.5;

    Vsma(source_l, 1, &scale, destination, 1, length());
    Vsma(source_r, 1, &scale, destination, 1, length());
  } else if (number_of_source_channels == 4 &&
             number_of_destination_channels == 1) {
    // Down-mixing: 4 -> 1
    //   output = 0.25 * (input.L + input.R + input.SL + input.SR)
    const float* source_l = source_bus.ChannelByType(kChannelLeft)->Data();
    const float* source_r = source_bus.ChannelByType(kChannelRight)->Data();
    const float* source_sl =
        source_bus.ChannelByType(kChannelSurroundLeft)->Data();
    const float* source_sr =
        source_bus.ChannelByType(kChannelSurroundRight)->Data();

    float* destination = ChannelByType(kChannelLeft)->MutableData();
    float scale = 0.25;

    Vsma(source_l, 1, &scale, destination, 1, length());
    Vsma(source_r, 1, &scale, destination, 1, length());
    Vsma(source_sl, 1, &scale, destination, 1, length());
    Vsma(source_sr, 1, &scale, destination, 1, length());
  } else if (number_of_source_channels == 6 &&
             number_of_destination_channels == 1) {
    // Down-mixing: 5.1 -> 1
    //   output = sqrt(1/2) * (input.L + input.R) + input.C
    //            + 0.5 * (input.SL + input.SR)
    const float* source_l = source_bus.ChannelByType(kChannelLeft)->Data();
    const float* source_r = source_bus.ChannelByType(kChannelRight)->Data();
    const float* source_c = source_bus.ChannelByType(kChannelCenter)->Data();
    const float* source_sl =
        source_bus.ChannelByType(kChannelSurroundLeft)->Data();
    const float* source_sr =
        source_bus.ChannelByType(kChannelSurroundRight)->Data();

    float* destination = ChannelByType(kChannelLeft)->MutableData();
    float scale_sqrt_half = sqrtf(0.5);
    float scale_half = 0.5;

    Vsma(source_l, 1, &scale_sqrt_half, destination, 1, length());
    Vsma(source_r, 1, &scale_sqrt_half, destination, 1, length());
    Vadd(source_c, 1, destination, 1, destination, 1, length());
    Vsma(source_sl, 1, &scale_half, destination, 1, length());
    Vsma(source_sr, 1, &scale_half, destination, 1, length());
  } else if (number_of_source_channels == 4 &&
             number_of_destination_channels == 2) {
    // Down-mixing: 4 -> 2
    //   output.L = 0.5 * (input.L + input.SL)
    //   output.R = 0.5 * (input.R + input.SR)
    const float* source_l = source_bus.ChannelByType(kChannelLeft)->Data();
    const float* source_r = source_bus.ChannelByType(kChannelRight)->Data();
    const float* source_sl =
        source_bus.ChannelByType(kChannelSurroundLeft)->Data();
    const float* source_sr =
        source_bus.ChannelByType(kChannelSurroundRight)->Data();

    float* destination_l = ChannelByType(kChannelLeft)->MutableData();
    float* destination_r = ChannelByType(kChannelRight)->MutableData();
    float scale_half = 0.5;

    Vsma(source_l, 1, &scale_half, destination_l, 1, length());
    Vsma(source_sl, 1, &scale_half, destination_l, 1, length());
    Vsma(source_r, 1, &scale_half, destination_r, 1, length());
    Vsma(source_sr, 1, &scale_half, destination_r, 1, length());
  } else if (number_of_source_channels == 6 &&
             number_of_destination_channels == 2) {
    // Down-mixing: 5.1 -> 2
    //   output.L = input.L + sqrt(1/2) * (input.C + input.SL)
    //   output.R = input.R + sqrt(1/2) * (input.C + input.SR)
    const float* source_l = source_bus.ChannelByType(kChannelLeft)->Data();
    const float* source_r = source_bus.ChannelByType(kChannelRight)->Data();
    const float* source_c = source_bus.ChannelByType(kChannelCenter)->Data();
    const float* source_sl =
        source_bus.ChannelByType(kChannelSurroundLeft)->Data();
    const float* source_sr =
        source_bus.ChannelByType(kChannelSurroundRight)->Data();

    float* destination_l = ChannelByType(kChannelLeft)->MutableData();
    float* destination_r = ChannelByType(kChannelRight)->MutableData();
    float scale_sqrt_half = sqrtf(0.5);

    Vadd(source_l, 1, destination_l, 1, destination_l, 1, length());
    Vsma(source_c, 1, &scale_sqrt_half, destination_l, 1, length());
    Vsma(source_sl, 1, &scale_sqrt_half, destination_l, 1, length());
    Vadd(source_r, 1, destination_r, 1, destination_r, 1, length());
    Vsma(source_c, 1, &scale_sqrt_half, destination_r, 1, length());
    Vsma(source_sr, 1, &scale_sqrt_half, destination_r, 1, length());
  } else if (number_of_source_channels == 6 &&
             number_of_destination_channels == 4) {
    // Down-mixing: 5.1 -> 4
    //   output.L = input.L + sqrt(1/2) * input.C
    //   output.R = input.R + sqrt(1/2) * input.C
    //   output.SL = input.SL
    //   output.SR = input.SR
    const float* source_l = source_bus.ChannelByType(kChannelLeft)->Data();
    const float* source_r = source_bus.ChannelByType(kChannelRight)->Data();
    const float* source_c = source_bus.ChannelByType(kChannelCenter)->Data();

    float* destination_l = ChannelByType(kChannelLeft)->MutableData();
    float* destination_r = ChannelByType(kChannelRight)->MutableData();
    float scale_sqrt_half = sqrtf(0.5);

    Vadd(source_l, 1, destination_l, 1, destination_l, 1, length());
    Vsma(source_c, 1, &scale_sqrt_half, destination_l, 1, length());
    Vadd(source_r, 1, destination_r, 1, destination_r, 1, length());
    Vsma(source_c, 1, &scale_sqrt_half, destination_r, 1, length());
    Channel(2)->SumFrom(source_bus.Channel(4));
    Channel(3)->SumFrom(source_bus.Channel(5));
  } else {
    // All other cases, fall back to the discrete sum. This will perform
    // channel-wise sum until the destination channels run out.
    DiscreteSumFrom(source_bus);
  }
}

void AudioBus::CopyWithGainFrom(const AudioBus& source_bus, float gain) {
  if (!TopologyMatches(source_bus)) {
    NOTREACHED();
    Zero();
    return;
  }

  if (source_bus.IsSilent()) {
    Zero();
    return;
  }

  unsigned number_of_channels = this->NumberOfChannels();
  DCHECK_LE(number_of_channels, kMaxBusChannels);
  if (number_of_channels > kMaxBusChannels)
    return;

  // If it is copying from the same bus and no need to change gain, just return.
  if (this == &source_bus && gain == 1)
    return;

  const float* sources[kMaxBusChannels];
  float* destinations[kMaxBusChannels];

  for (unsigned i = 0; i < number_of_channels; ++i) {
    sources[i] = source_bus.Channel(i)->Data();
    destinations[i] = Channel(i)->MutableData();
  }

  unsigned frames_to_process = length();

  // Handle gains of 0 and 1 (exactly) specially.
  if (gain == 1) {
    for (unsigned channel_index = 0; channel_index < number_of_channels;
         ++channel_index) {
      memcpy(destinations[channel_index], sources[channel_index],
             frames_to_process * sizeof(*destinations[channel_index]));
    }
  } else if (gain == 0) {
    for (unsigned channel_index = 0; channel_index < number_of_channels;
         ++channel_index) {
      memset(destinations[channel_index], 0,
             frames_to_process * sizeof(*destinations[channel_index]));
    }
  } else {
    for (unsigned channel_index = 0; channel_index < number_of_channels;
         ++channel_index) {
      vector_math::Vsmul(sources[channel_index], 1, &gain,
                         destinations[channel_index], 1, frames_to_process);
    }
  }
}

void AudioBus::CopyWithSampleAccurateGainValuesFrom(
    const AudioBus& source_bus,
    float* gain_values,
    unsigned number_of_gain_values) {
  // Make sure we're processing from the same type of bus.
  // We *are* able to process from mono -> stereo
  if (source_bus.NumberOfChannels() != 1 && !TopologyMatches(source_bus)) {
    NOTREACHED();
    return;
  }

  if (!gain_values || number_of_gain_values > source_bus.length()) {
    NOTREACHED();
    return;
  }

  if (source_bus.length() == number_of_gain_values &&
      source_bus.length() == length() && source_bus.IsSilent()) {
    Zero();
    return;
  }

  // We handle both the 1 -> N and N -> N case here.
  const float* source = source_bus.Channel(0)->Data();
  for (unsigned channel_index = 0; channel_index < NumberOfChannels();
       ++channel_index) {
    if (source_bus.NumberOfChannels() == NumberOfChannels())
      source = source_bus.Channel(channel_index)->Data();
    float* destination = Channel(channel_index)->MutableData();
    vector_math::Vmul(source, 1, gain_values, 1, destination, 1,
                      number_of_gain_values);
  }
}

scoped_refptr<AudioBus> AudioBus::CreateBySampleRateConverting(
    const AudioBus* source_bus,
    bool mix_to_mono,
    double new_sample_rate) {
  // sourceBus's sample-rate must be known.
  DCHECK(source_bus);
  DCHECK(source_bus->SampleRate());
  if (!source_bus || !source_bus->SampleRate())
    return nullptr;

  double source_sample_rate = source_bus->SampleRate();
  double destination_sample_rate = new_sample_rate;
  double sample_rate_ratio = source_sample_rate / destination_sample_rate;
  unsigned number_of_source_channels = source_bus->NumberOfChannels();

  if (number_of_source_channels == 1)
    mix_to_mono = false;  // already mono

  if (source_sample_rate == destination_sample_rate) {
    // No sample-rate conversion is necessary.
    if (mix_to_mono)
      return AudioBus::CreateByMixingToMono(source_bus);

    // Return exact copy.
    return AudioBus::CreateBufferFromRange(source_bus, 0, source_bus->length());
  }

  if (source_bus->IsSilent()) {
    scoped_refptr<AudioBus> silent_bus = Create(
        number_of_source_channels, source_bus->length() / sample_rate_ratio);
    silent_bus->SetSampleRate(new_sample_rate);
    return silent_bus;
  }

  // First, mix to mono (if necessary) then sample-rate convert.
  const AudioBus* resampler_source_bus;
  scoped_refptr<AudioBus> mixed_mono_bus;
  if (mix_to_mono) {
    mixed_mono_bus = AudioBus::CreateByMixingToMono(source_bus);
    resampler_source_bus = mixed_mono_bus.get();
  } else {
    // Directly resample without down-mixing.
    resampler_source_bus = source_bus;
  }

  // Calculate destination length based on the sample-rates.
  int source_length = resampler_source_bus->length();
  int destination_length = source_length / sample_rate_ratio;

  // Create destination bus with same number of channels.
  unsigned number_of_destination_channels =
      resampler_source_bus->NumberOfChannels();
  scoped_refptr<AudioBus> destination_bus =
      Create(number_of_destination_channels, destination_length);

  // Sample-rate convert each channel.
  for (unsigned i = 0; i < number_of_destination_channels; ++i) {
    const float* source = resampler_source_bus->Channel(i)->Data();
    float* destination = destination_bus->Channel(i)->MutableData();

    SincResampler resampler(sample_rate_ratio);
    resampler.Process(source, destination, source_length);
  }

  destination_bus->ClearSilentFlag();
  destination_bus->SetSampleRate(new_sample_rate);
  return destination_bus;
}

scoped_refptr<AudioBus> AudioBus::CreateByMixingToMono(
    const AudioBus* source_bus) {
  if (source_bus->IsSilent())
    return Create(1, source_bus->length());

  switch (source_bus->NumberOfChannels()) {
    case 1:
      // Simply create an exact copy.
      return AudioBus::CreateBufferFromRange(source_bus, 0,
                                             source_bus->length());
    case 2: {
      unsigned n = source_bus->length();
      scoped_refptr<AudioBus> destination_bus = Create(1, n);

      const float* source_l = source_bus->Channel(0)->Data();
      const float* source_r = source_bus->Channel(1)->Data();
      float* destination = destination_bus->Channel(0)->MutableData();

      // Do the mono mixdown.
      for (unsigned i = 0; i < n; ++i)
        destination[i] = (source_l[i] + source_r[i]) / 2;

      destination_bus->ClearSilentFlag();
      destination_bus->SetSampleRate(source_bus->SampleRate());
      return destination_bus;
    }
  }

  NOTREACHED();
  return nullptr;
}

bool AudioBus::IsSilent() const {
  for (size_t i = 0; i < channels_.size(); ++i) {
    if (!channels_[i]->IsSilent())
      return false;
  }
  return true;
}

void AudioBus::ClearSilentFlag() {
  for (size_t i = 0; i < channels_.size(); ++i)
    channels_[i]->ClearSilentFlag();
}

scoped_refptr<AudioBus> DecodeAudioFileData(const char* data, size_t size) {
  WebAudioBus web_audio_bus;
  if (Platform::Current()->DecodeAudioFileData(&web_audio_bus, data, size))
    return web_audio_bus.Release();
  return nullptr;
}

scoped_refptr<AudioBus> AudioBus::GetDataResource(int resource_id,
                                                  float sample_rate) {
  const WebData& resource = Platform::Current()->GetDataResource(resource_id);
  if (resource.IsEmpty())
    return nullptr;

  // Currently, the only client of this method is caching the result -- so
  // it's reasonable to (potentially) pay a one-time flat access cost.
  // If this becomes problematic, we'll have the refactor DecodeAudioFileData
  // to take WebData and use segmented access.
  SharedBuffer::DeprecatedFlatData flat_data(
      resource.operator scoped_refptr<SharedBuffer>());
  scoped_refptr<AudioBus> audio_bus =
      DecodeAudioFileData(flat_data.Data(), flat_data.size());

  if (!audio_bus.get())
    return nullptr;

  // If the bus is already at the requested sample-rate then return as is.
  if (audio_bus->SampleRate() == sample_rate)
    return audio_bus;

  return AudioBus::CreateBySampleRateConverting(audio_bus.get(), false,
                                                sample_rate);
}

scoped_refptr<AudioBus> CreateBusFromInMemoryAudioFile(const void* data,
                                                       size_t data_size,
                                                       bool mix_to_mono,
                                                       float sample_rate) {
  scoped_refptr<AudioBus> audio_bus =
      DecodeAudioFileData(static_cast<const char*>(data), data_size);
  if (!audio_bus.get())
    return nullptr;

  // If the bus needs no conversion then return as is.
  if ((!mix_to_mono || audio_bus->NumberOfChannels() == 1) &&
      audio_bus->SampleRate() == sample_rate)
    return audio_bus;

  return AudioBus::CreateBySampleRateConverting(audio_bus.get(), mix_to_mono,
                                                sample_rate);
}

}  // namespace blink
