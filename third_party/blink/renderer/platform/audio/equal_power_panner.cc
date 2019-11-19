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

#include <algorithm>
#include <cmath>
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

EqualPowerPanner::EqualPowerPanner(float sample_rate)
    : Panner(kPanningModelEqualPower) {}

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

  unsigned number_of_input_channels = input_bus->NumberOfChannels();

  DCHECK(output_bus);
  DCHECK_EQ(output_bus->NumberOfChannels(), 2u);
  DCHECK_LE(frames_to_process, output_bus->length());

  const float* source_l = input_bus->Channel(0)->Data();
  const float* source_r =
      number_of_input_channels > 1 ? input_bus->Channel(1)->Data() : source_l;
  float* destination_l =
      output_bus->ChannelByType(AudioBus::kChannelLeft)->MutableData();
  float* destination_r =
      output_bus->ChannelByType(AudioBus::kChannelRight)->MutableData();

  if (!source_l || !source_r || !destination_l || !destination_r)
    return;

  // Clamp azimuth to allowed range of -180 -> +180.
  azimuth = clampTo(azimuth, -180.0, 180.0);

  // Alias the azimuth ranges behind us to in front of us:
  // -90 -> -180 to -90 -> 0 and 90 -> 180 to 90 -> 0
  if (azimuth < -90)
    azimuth = -180 - azimuth;
  else if (azimuth > 90)
    azimuth = 180 - azimuth;

  double desired_pan_position;
  double desired_gain_l;
  double desired_gain_r;

  if (number_of_input_channels == 1) {  // For mono source case.
    // Pan smoothly from left to right with azimuth going from -90 -> +90
    // degrees.
    desired_pan_position = (azimuth + 90) / 180;
  } else {               // For stereo source case.
    if (azimuth <= 0) {  // from -90 -> 0
      // sourceL -> destL and "equal-power pan" sourceR as in mono case
      // by transforming the "azimuth" value from -90 -> 0 degrees into the
      // range -90 -> +90.
      desired_pan_position = (azimuth + 90) / 90;
    } else {  // from 0 -> +90
      // sourceR -> destR and "equal-power pan" sourceL as in mono case
      // by transforming the "azimuth" value from 0 -> +90 degrees into the
      // range -90 -> +90.
      desired_pan_position = azimuth / 90;
    }
  }

  desired_gain_l = std::cos(kPiOverTwoDouble * desired_pan_position);
  desired_gain_r = std::sin(kPiOverTwoDouble * desired_pan_position);

  int n = frames_to_process;

  if (number_of_input_channels == 1) {  // For mono source case.
    while (n--) {
      float input_l = *source_l++;

      *destination_l++ = static_cast<float>(input_l * desired_gain_l);
      *destination_r++ = static_cast<float>(input_l * desired_gain_r);
    }
  } else {               // For stereo source case.
    if (azimuth <= 0) {  // from -90 -> 0
      while (n--) {
        float input_l = *source_l++;
        float input_r = *source_r++;

        *destination_l++ =
            static_cast<float>(input_l + input_r * desired_gain_l);
        *destination_r++ = static_cast<float>(input_r * desired_gain_r);
      }
    } else {  // from 0 -> +90
      while (n--) {
        float input_l = *source_l++;
        float input_r = *source_r++;

        *destination_l++ = static_cast<float>(input_l * desired_gain_l);
        *destination_r++ =
            static_cast<float>(input_r + input_l * desired_gain_r);
      }
    }
  }
}

void EqualPowerPanner::CalculateDesiredGain(double& desired_gain_l,
                                            double& desired_gain_r,
                                            double azimuth,
                                            int number_of_input_channels) {
  // Clamp azimuth to allowed range of -180 -> +180.
  azimuth = clampTo(azimuth, -180.0, 180.0);

  // Alias the azimuth ranges behind us to in front of us:
  // -90 -> -180 to -90 -> 0 and 90 -> 180 to 90 -> 0
  if (azimuth < -90)
    azimuth = -180 - azimuth;
  else if (azimuth > 90)
    azimuth = 180 - azimuth;

  double desired_pan_position;

  if (number_of_input_channels == 1) {  // For mono source case.
    // Pan smoothly from left to right with azimuth going from -90 -> +90
    // degrees.
    desired_pan_position = (azimuth + 90) / 180;
  } else {               // For stereo source case.
    if (azimuth <= 0) {  // from -90 -> 0
      // sourceL -> destL and "equal-power pan" sourceR as in mono case
      // by transforming the "azimuth" value from -90 -> 0 degrees into the
      // range -90 -> +90.
      desired_pan_position = (azimuth + 90) / 90;
    } else {  // from 0 -> +90
      // sourceR -> destR and "equal-power pan" sourceL as in mono case
      // by transforming the "azimuth" value from 0 -> +90 degrees into the
      // range -90 -> +90.
      desired_pan_position = azimuth / 90;
    }
  }

  desired_gain_l = std::cos(kPiOverTwoDouble * desired_pan_position);
  desired_gain_r = std::sin(kPiOverTwoDouble * desired_pan_position);
}

void EqualPowerPanner::PanWithSampleAccurateValues(
    double* azimuth,
    double* /*elevation*/,
    const AudioBus* input_bus,
    AudioBus* output_bus,
    uint32_t frames_to_process,
    AudioBus::ChannelInterpretation) {
  DCHECK(input_bus);
  DCHECK_LE(frames_to_process, input_bus->length());
  DCHECK_GE(input_bus->NumberOfChannels(), 1u);
  DCHECK_LE(input_bus->NumberOfChannels(), 2u);

  unsigned number_of_input_channels = input_bus->NumberOfChannels();

  DCHECK(output_bus);
  DCHECK_EQ(output_bus->NumberOfChannels(), 2u);
  DCHECK_LE(frames_to_process, output_bus->length());

  const float* source_l = input_bus->Channel(0)->Data();
  const float* source_r =
      number_of_input_channels > 1 ? input_bus->Channel(1)->Data() : source_l;
  float* destination_l =
      output_bus->ChannelByType(AudioBus::kChannelLeft)->MutableData();
  float* destination_r =
      output_bus->ChannelByType(AudioBus::kChannelRight)->MutableData();

  DCHECK(source_l);
  DCHECK(source_r);
  DCHECK(destination_l);
  DCHECK(destination_r);

  int n = frames_to_process;

  if (number_of_input_channels == 1) {  // For mono source case.
    for (int k = 0; k < n; ++k) {
      double desired_gain_l;
      double desired_gain_r;
      float input_l = *source_l++;

      CalculateDesiredGain(desired_gain_l, desired_gain_r, azimuth[k],
                           number_of_input_channels);
      *destination_l++ = static_cast<float>(input_l * desired_gain_l);
      *destination_r++ = static_cast<float>(input_l * desired_gain_r);
    }
  } else {  // For stereo source case.
    for (int k = 0; k < n; ++k) {
      double desired_gain_l;
      double desired_gain_r;

      CalculateDesiredGain(desired_gain_l, desired_gain_r, azimuth[k],
                           number_of_input_channels);
      if (azimuth[k] <= 0) {  // from -90 -> 0
        float input_l = *source_l++;
        float input_r = *source_r++;
        *destination_l++ =
            static_cast<float>(input_l + input_r * desired_gain_l);
        *destination_r++ = static_cast<float>(input_r * desired_gain_r);
      } else {  // from 0 -> +90
        float input_l = *source_l++;
        float input_r = *source_r++;
        *destination_l++ = static_cast<float>(input_l * desired_gain_l);
        *destination_r++ =
            static_cast<float>(input_r + input_l * desired_gain_r);
      }
    }
  }
}

}  // namespace blink
