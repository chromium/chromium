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

#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/dynamics_compressor.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

DynamicsCompressor::DynamicsCompressor(float sample_rate,
                                       unsigned number_of_channels)
    : number_of_channels_(number_of_channels),
      sample_rate_(sample_rate),
      compressor_(sample_rate, number_of_channels) {
  // Uninitialized state - for parameter recalculation.
  last_filter_stage_ratio_ = -1;
  last_anchor_ = -1;
  last_filter_stage_gain_ = -1;

  SetNumberOfChannels(number_of_channels);
  InitializeParameters();
}

void DynamicsCompressor::SetParameterValue(unsigned parameter_id, float value) {
  DCHECK_LT(parameter_id, static_cast<unsigned>(kParamLast));
  parameters_[parameter_id] = value;
}

void DynamicsCompressor::InitializeParameters() {
  // Initializes compressor to default values.

  parameters_[kParamThreshold] = -24;    // dB
  parameters_[kParamKnee] = 30;          // dB
  parameters_[kParamRatio] = 12;         // unit-less
  parameters_[kParamAttack] = 0.003f;    // seconds
  parameters_[kParamRelease] = 0.250f;   // seconds
  parameters_[kParamPreDelay] = 0.006f;  // seconds

  // Release zone values 0 -> 1.
  parameters_[kParamReleaseZone1] = 0.09f;
  parameters_[kParamReleaseZone2] = 0.16f;
  parameters_[kParamReleaseZone3] = 0.42f;
  parameters_[kParamReleaseZone4] = 0.98f;

  parameters_[kParamFilterStageGain] = 4.4f;  // dB
  parameters_[kParamFilterStageRatio] = 2;
  parameters_[kParamFilterAnchor] = 15000 / Nyquist();

  parameters_[kParamPostGain] = 0;   // dB
  parameters_[kParamReduction] = 0;  // dB

  // Linear crossfade (0 -> 1).
  parameters_[kParamEffectBlend] = 1;
}

float DynamicsCompressor::ParameterValue(unsigned parameter_id) {
  DCHECK_LT(parameter_id, static_cast<unsigned>(kParamLast));
  return parameters_[parameter_id];
}

void DynamicsCompressor::Process(const AudioBus* source_bus,
                                 AudioBus* destination_bus,
                                 unsigned frames_to_process) {
  // Though numberOfChannels is retrived from destinationBus, we still name it
  // numberOfChannels instead of numberOfDestinationChannels.  It's because we
  // internally match sourceChannels's size to destinationBus by channel
  // up/down mix. Thus we need numberOfChannels
  // to do the loop work for both m_sourceChannels and m_destinationChannels.

  unsigned number_of_channels = destination_bus->NumberOfChannels();
  unsigned number_of_source_channels = source_bus->NumberOfChannels();

  DCHECK_EQ(number_of_channels, number_of_channels_);
  DCHECK(number_of_source_channels);

  switch (number_of_channels) {
    case 2:  // stereo
      source_channels_[0] = source_bus->Channel(0)->Data();

      if (number_of_source_channels > 1)
        source_channels_[1] = source_bus->Channel(1)->Data();
      else
        // Simply duplicate mono channel input data to right channel for stereo
        // processing.
        source_channels_[1] = source_channels_[0];

      break;
    default:
      // FIXME : support other number of channels.
      NOTREACHED();
      destination_bus->Zero();
      return;
  }

  for (unsigned i = 0; i < number_of_channels; ++i)
    destination_channels_[i] = destination_bus->Channel(i)->MutableData();

  float filter_stage_gain = ParameterValue(kParamFilterStageGain);
  float filter_stage_ratio = ParameterValue(kParamFilterStageRatio);
  float anchor = ParameterValue(kParamFilterAnchor);

  if (filter_stage_gain != last_filter_stage_gain_ ||
      filter_stage_ratio != last_filter_stage_ratio_ ||
      anchor != last_anchor_) {
    last_filter_stage_gain_ = filter_stage_gain;
    last_filter_stage_ratio_ = filter_stage_ratio;
    last_anchor_ = anchor;
  }

  float db_threshold = ParameterValue(kParamThreshold);
  float db_knee = ParameterValue(kParamKnee);
  float ratio = ParameterValue(kParamRatio);
  float attack_time = ParameterValue(kParamAttack);
  float release_time = ParameterValue(kParamRelease);
  float pre_delay_time = ParameterValue(kParamPreDelay);

  // This is effectively a master volume on the compressed signal
  // (pre-blending).
  float db_post_gain = ParameterValue(kParamPostGain);

  // Linear blending value from dry to completely processed (0 -> 1)
  // 0 means the signal is completely unprocessed.
  // 1 mixes in only the compressed signal.
  float effect_blend = ParameterValue(kParamEffectBlend);

  float release_zone1 = ParameterValue(kParamReleaseZone1);
  float release_zone2 = ParameterValue(kParamReleaseZone2);
  float release_zone3 = ParameterValue(kParamReleaseZone3);
  float release_zone4 = ParameterValue(kParamReleaseZone4);

  // Apply compression to the source signal.
  compressor_.Process(source_channels_.get(), destination_channels_.get(),
                      number_of_channels, frames_to_process,

                      db_threshold, db_knee, ratio, attack_time, release_time,
                      pre_delay_time, db_post_gain, effect_blend,

                      release_zone1, release_zone2, release_zone3,
                      release_zone4);

  // Update the compression amount.
  SetParameterValue(kParamReduction, compressor_.MeteringGain());
}

void DynamicsCompressor::Reset() {
  last_filter_stage_ratio_ = -1;  // for recalc
  last_anchor_ = -1;
  last_filter_stage_gain_ = -1;

  compressor_.Reset();
}

void DynamicsCompressor::SetNumberOfChannels(unsigned number_of_channels) {
  source_channels_ = std::make_unique<const float* []>(number_of_channels);
  destination_channels_ = std::make_unique<float* []>(number_of_channels);

  compressor_.SetNumberOfChannels(number_of_channels);
  number_of_channels_ = number_of_channels;
}

double DynamicsCompressor::TailTime() const {
  return compressor_.TailTime();
}

}  // namespace blink
