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

#include "third_party/blink/renderer/platform/audio/reverb.h"

#include <math.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

// Empirical gain calibration tested across many impulse responses to ensure
// perceived volume is same as dry (unprocessed) signal
const float kGainCalibration = -58;
const float kGainCalibrationSampleRate = 44100;

// A minimum power value to when normalizing a silent (or very quiet) impulse
// response
const float kMinPower = 0.000125f;

static float CalculateNormalizationScale(AudioBus* response) {
  // Normalize by RMS power
  size_t number_of_channels = response->NumberOfChannels();
  size_t length = response->length();

  float power = 0;

  for (size_t i = 0; i < number_of_channels; ++i) {
    float channel_power = 0;
    vector_math::Vsvesq(response->Channel(i)->Data(), 1, &channel_power,
                        length);
    power += channel_power;
  }

  power = sqrt(power / (number_of_channels * length));

  // Protect against accidental overload
  if (std::isinf(power) || std::isnan(power) || power < kMinPower)
    power = kMinPower;

  float scale = 1 / power;

  scale *= powf(
      10, kGainCalibration *
              0.05f);  // calibrate to make perceived volume same as unprocessed

  // Scale depends on sample-rate.
  if (response->SampleRate())
    scale *= kGainCalibrationSampleRate / response->SampleRate();

  // True-stereo compensation
  if (response->NumberOfChannels() == 4)
    scale *= 0.5f;

  return scale;
}

Reverb::Reverb(AudioBus* impulse_response,
               size_t render_slice_size,
               size_t max_fft_size,
               bool use_background_threads,
               bool normalize) {
  float scale = 1;

  if (normalize) {
    scale = CalculateNormalizationScale(impulse_response);
  }

  Initialize(impulse_response, render_slice_size, max_fft_size,
             use_background_threads, scale);
}

void Reverb::Initialize(AudioBus* impulse_response_buffer,
                        size_t render_slice_size,
                        size_t max_fft_size,
                        bool use_background_threads,
                        float scale) {
  impulse_response_length_ = impulse_response_buffer->length();
  number_of_response_channels_ = impulse_response_buffer->NumberOfChannels();

  // The reverb can handle a mono impulse response and still do stereo
  // processing.
  unsigned num_convolvers = std::max(number_of_response_channels_, 2u);
  convolvers_.ReserveCapacity(num_convolvers);

  int convolver_render_phase = 0;
  for (unsigned i = 0; i < num_convolvers; ++i) {
    AudioChannel* channel = impulse_response_buffer->Channel(
        std::min(i, number_of_response_channels_ - 1));

    std::unique_ptr<ReverbConvolver> convolver =
        std::make_unique<ReverbConvolver>(channel, render_slice_size,
                                          max_fft_size, convolver_render_phase,
                                          use_background_threads, scale);
    convolvers_.push_back(std::move(convolver));

    convolver_render_phase += render_slice_size;
  }

  // For "True" stereo processing we allocate a temporary buffer to avoid
  // repeatedly allocating it in the process() method.  It can be bad to
  // allocate memory in a real-time thread.
  if (number_of_response_channels_ == 4)
    temp_buffer_ = AudioBus::Create(2, kMaxFrameSize);
}

void Reverb::Process(const AudioBus* source_bus,
                     AudioBus* destination_bus,
                     uint32_t frames_to_process) {
  // Do a fairly comprehensive sanity check.
  // If these conditions are satisfied, all of the source and destination
  // pointers will be valid for the various matrixing cases.
  DCHECK(source_bus);
  DCHECK(destination_bus);
  DCHECK_GT(source_bus->NumberOfChannels(), 0u);
  DCHECK_GT(destination_bus->NumberOfChannels(), 0u);
  DCHECK_LE(frames_to_process, kMaxFrameSize);
  DCHECK_LE(frames_to_process, source_bus->length());
  DCHECK_LE(frames_to_process, destination_bus->length());

  // For now only handle mono or stereo output
  if (destination_bus->NumberOfChannels() > 2) {
    destination_bus->Zero();
    return;
  }

  AudioChannel* destination_channel_l = destination_bus->Channel(0);
  const AudioChannel* source_channel_l = source_bus->Channel(0);

  // Handle input -> output matrixing...
  size_t num_input_channels = source_bus->NumberOfChannels();
  size_t num_output_channels = destination_bus->NumberOfChannels();
  size_t number_of_response_channels = number_of_response_channels_;

  DCHECK_LE(num_input_channels, 2ul);
  DCHECK_LE(num_output_channels, 2ul);
  DCHECK(number_of_response_channels == 1 || number_of_response_channels == 2 ||
         number_of_response_channels == 4);

  // These are the possible combinations of number inputs, response
  // channels and outputs channels that need to be supported:
  //
  //   numInputChannels:         1 or 2
  //   numberOfResponseChannels: 1, 2, or 4
  //   numOutputChannels:        1 or 2
  //
  // Not all possible combinations are valid.  numOutputChannels is
  // one only if both numInputChannels and numberOfResponseChannels are 1.
  // Otherwise numOutputChannels MUST be 2.
  //
  // The valid combinations are
  //
  //   Case     in -> resp -> out
  //   1        1 -> 1 -> 1
  //   2        1 -> 2 -> 2
  //   3        1 -> 4 -> 2
  //   4        2 -> 1 -> 2
  //   5        2 -> 2 -> 2
  //   6        2 -> 4 -> 2

  if (num_input_channels == 2 &&
      (number_of_response_channels == 1 || number_of_response_channels == 2) &&
      num_output_channels == 2) {
    // Case 4 and 5: 2 -> 2 -> 2 or 2 -> 1 -> 2.
    //
    // These can be handled in the same way because in the latter
    // case, two connvolvers are still created with the second being a
    // copy of the first.
    const AudioChannel* source_channel_r = source_bus->Channel(1);
    AudioChannel* destination_channel_r = destination_bus->Channel(1);
    convolvers_[0]->Process(source_channel_l, destination_channel_l,
                            frames_to_process);
    convolvers_[1]->Process(source_channel_r, destination_channel_r,
                            frames_to_process);
  } else if (num_input_channels == 1 && num_output_channels == 2 &&
             number_of_response_channels == 2) {
    // Case 2: 1 -> 2 -> 2
    for (int i = 0; i < 2; ++i) {
      AudioChannel* destination_channel = destination_bus->Channel(i);
      convolvers_[i]->Process(source_channel_l, destination_channel,
                              frames_to_process);
    }
  } else if (num_input_channels == 1 && number_of_response_channels == 1) {
    // Case 1: 1 -> 1 -> 1
    DCHECK_EQ(num_output_channels, 1ul);
    convolvers_[0]->Process(source_channel_l, destination_channel_l,
                            frames_to_process);
  } else if (num_input_channels == 2 && number_of_response_channels == 4 &&
             num_output_channels == 2) {
    // Case 6: 2 -> 4 -> 2 ("True" stereo)
    const AudioChannel* source_channel_r = source_bus->Channel(1);
    AudioChannel* destination_channel_r = destination_bus->Channel(1);

    AudioChannel* temp_channel_l = temp_buffer_->Channel(0);
    AudioChannel* temp_channel_r = temp_buffer_->Channel(1);

    // Process left virtual source
    convolvers_[0]->Process(source_channel_l, destination_channel_l,
                            frames_to_process);
    convolvers_[1]->Process(source_channel_l, destination_channel_r,
                            frames_to_process);

    // Process right virtual source
    convolvers_[2]->Process(source_channel_r, temp_channel_l,
                            frames_to_process);
    convolvers_[3]->Process(source_channel_r, temp_channel_r,
                            frames_to_process);

    destination_bus->SumFrom(*temp_buffer_);
  } else if (num_input_channels == 1 && number_of_response_channels == 4 &&
             num_output_channels == 2) {
    // Case 3: 1 -> 4 -> 2 (Processing mono with "True" stereo impulse
    // response) This is an inefficient use of a four-channel impulse
    // response, but we should handle the case.
    AudioChannel* destination_channel_r = destination_bus->Channel(1);

    AudioChannel* temp_channel_l = temp_buffer_->Channel(0);
    AudioChannel* temp_channel_r = temp_buffer_->Channel(1);

    // Process left virtual source
    convolvers_[0]->Process(source_channel_l, destination_channel_l,
                            frames_to_process);
    convolvers_[1]->Process(source_channel_l, destination_channel_r,
                            frames_to_process);

    // Process right virtual source
    convolvers_[2]->Process(source_channel_l, temp_channel_l,
                            frames_to_process);
    convolvers_[3]->Process(source_channel_l, temp_channel_r,
                            frames_to_process);

    destination_bus->SumFrom(*temp_buffer_);
  } else {
    NOTREACHED();
    destination_bus->Zero();
  }
}

void Reverb::Reset() {
  for (size_t i = 0; i < convolvers_.size(); ++i)
    convolvers_[i]->Reset();
}

size_t Reverb::LatencyFrames() const {
  return !convolvers_.IsEmpty() ? convolvers_.front()->LatencyFrames() : 0;
}

}  // namespace blink
