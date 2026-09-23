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

#include "third_party/blink/renderer/platform/audio/equal_power_panner.h"

#include <cmath>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/fdlibm/ieee754.h"

namespace blink {

EqualPowerPanner::EqualPowerPanner(float sample_rate) {
  // Initialize cache by passing reasonable default values.
  constexpr double kDefaultAzimuth = 0.0;
  constexpr int kDefaultNumberOfInputChannels = 1;
  UpdateDesiredGain(kDefaultAzimuth, kDefaultNumberOfInputChannels);
}

void EqualPowerPanner::Pan(double azimuth,
                           double /*elevation*/,
                           const AudioBus* input_bus,
                           AudioBus* output_bus,
                           uint32_t frames_to_process,
                           AudioBus::ChannelInterpretation) {
  DCHECK(input_bus);
  DCHECK_LE(frames_to_process, input_bus->length());
  DCHECK_GE(input_bus->NumberOfChannels(), 1u);
  DCHECK_LE(input_bus->NumberOfChannels(), 2u);
  DCHECK(output_bus);
  DCHECK_EQ(output_bus->NumberOfChannels(), 2u);
  DCHECK_LE(frames_to_process, output_bus->length());

  const unsigned number_of_input_channels = input_bus->NumberOfChannels();
  base::span<const float> source_l = input_bus->Channel(0)->Span();
  base::span<const float> source_r =
      number_of_input_channels > 1 ? input_bus->Channel(1)->Span() : source_l;
  base::span<float> destination_l =
      output_bus->ChannelByType(AudioBus::kChannelLeft)->MutableSpan();
  base::span<float> destination_r =
      output_bus->ChannelByType(AudioBus::kChannelRight)->MutableSpan();

  if (source_l.empty() || source_r.empty() || destination_l.empty() ||
      destination_r.empty()) {
    return;
  }

  azimuth = UpdateDesiredGain(azimuth, number_of_input_channels);

  if (number_of_input_channels == 1) {  // For mono source case.
    for (size_t i = 0; i < frames_to_process; ++i) {
      const float input_l = source_l[i];
      destination_l[i] = static_cast<float>(input_l * desired_gain_l_);
      destination_r[i] = static_cast<float>(input_l * desired_gain_r_);
    }
  } else {               // For stereo source case.
    if (azimuth <= 0) {  // from -90 -> 0
      for (size_t i = 0; i < frames_to_process; ++i) {
        const float input_l = source_l[i];
        const float input_r = source_r[i];
        destination_l[i] =
            static_cast<float>(input_l + input_r * desired_gain_l_);
        destination_r[i] = static_cast<float>(input_r * desired_gain_r_);
      }
    } else {  // from 0 -> +90
      for (size_t i = 0; i < frames_to_process; ++i) {
        const float input_l = source_l[i];
        const float input_r = source_r[i];
        destination_l[i] = static_cast<float>(input_l * desired_gain_l_);
        destination_r[i] =
            static_cast<float>(input_r + input_l * desired_gain_r_);
      }
    }
  }
}

void EqualPowerPanner::PanWithSampleAccurateValues(
    base::span<double> azimuth,
    base::span<double> /*elevation*/,
    const AudioBus* input_bus,
    AudioBus* output_bus,
    uint32_t frames_to_process,
    AudioBus::ChannelInterpretation) {
  DCHECK(input_bus);
  DCHECK_LE(frames_to_process, input_bus->length());
  DCHECK_GE(input_bus->NumberOfChannels(), 1u);
  DCHECK_LE(input_bus->NumberOfChannels(), 2u);
  DCHECK(output_bus);
  DCHECK_EQ(output_bus->NumberOfChannels(), 2u);
  DCHECK_LE(frames_to_process, output_bus->length());

  const unsigned number_of_input_channels = input_bus->NumberOfChannels();
  base::span<const float> source_l = input_bus->Channel(0)->Span();
  base::span<const float> source_r =
      number_of_input_channels > 1 ? input_bus->Channel(1)->Span() : source_l;
  base::span<float> destination_l =
      output_bus->ChannelByType(AudioBus::kChannelLeft)->MutableSpan();
  base::span<float> destination_r =
      output_bus->ChannelByType(AudioBus::kChannelRight)->MutableSpan();

  if (source_l.empty() || source_r.empty() || destination_l.empty() ||
      destination_r.empty()) {
    return;
  }

  if (number_of_input_channels == 1) {  // For mono source case.
    for (size_t i = 0; i < frames_to_process; ++i) {
      const float input_l = source_l[i];
      UpdateDesiredGain(azimuth[i], number_of_input_channels);
      destination_l[i] = static_cast<float>(input_l * desired_gain_l_);
      destination_r[i] = static_cast<float>(input_l * desired_gain_r_);
    }
  } else {  // For stereo source case.
    for (size_t i = 0; i < frames_to_process; ++i) {
      const float input_l = source_l[i];
      const float input_r = source_r[i];
      const double clamped_azimuth =
          UpdateDesiredGain(azimuth[i], number_of_input_channels);
      if (clamped_azimuth <= 0) {  // from -90 -> 0
        destination_l[i] =
            static_cast<float>(input_l + input_r * desired_gain_l_);
        destination_r[i] = static_cast<float>(input_r * desired_gain_r_);
      } else {  // from 0 -> +90
        destination_l[i] = static_cast<float>(input_l * desired_gain_l_);
        destination_r[i] =
            static_cast<float>(input_r + input_l * desired_gain_r_);
      }
    }
  }
}

double EqualPowerPanner::UpdateDesiredGain(double azimuth,
                                           int number_of_input_channels) {
  // Clamp azimuth to allowed range of -180 -> +180.
  azimuth = ClampTo(azimuth, -180.0, 180.0);

  // Alias the azimuth ranges behind us to in front of us:
  // -90 -> -180 to -90 -> 0 and 90 -> 180 to 90 -> 0
  if (azimuth < -90.0) {
    azimuth = -180.0 - azimuth;
  } else if (azimuth > 90.0) {
    azimuth = 180.0 - azimuth;
  }

  if ((cached_azimuth_ == azimuth) &&
      (cached_number_of_input_channels_ == number_of_input_channels)) {
    return azimuth;
  }
  cached_azimuth_ = azimuth;
  cached_number_of_input_channels_ = number_of_input_channels;

  double desired_pan_position;

  if (number_of_input_channels == 1) {  // For mono source case.
    // Pan smoothly from left to right with azimuth going from -90 -> +90
    // degrees.
    desired_pan_position = (azimuth + 90.0) / 180.0;
  } else {               // For stereo source case.
    if (azimuth <= 0) {  // from -90 -> 0
      // sourceL -> destL and "equal-power pan" sourceR as in mono case
      // by transforming the "azimuth" value from -90 -> 0 degrees into the
      // range -90 -> +90.
      desired_pan_position = (azimuth + 90.0) / 90.0;
    } else {  // from 0 -> +90
      // sourceR -> destR and "equal-power pan" sourceL as in mono case
      // by transforming the "azimuth" value from 0 -> +90 degrees into the
      // range -90 -> +90.
      desired_pan_position = azimuth / 90.0;
    }
  }

  desired_gain_l_ = fdlibm::cos(kPiOverTwoDouble * desired_pan_position);
  desired_gain_r_ = fdlibm::sin(kPiOverTwoDouble * desired_pan_position);
  return azimuth;
}

}  // namespace blink
