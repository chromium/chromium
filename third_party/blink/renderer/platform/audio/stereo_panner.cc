// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/audio/stereo_panner.h"

#include <algorithm>
#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/fdlibm/ieee754.h"

namespace blink {

// Implement equal-power panning algorithm for mono or stereo input.
// See: http://webaudio.github.io/web-audio-api/#panning-algorithm

StereoPanner::StereoPanner(float sample_rate) {}

void StereoPanner::PanWithSampleAccurateValues(const AudioBus* input_bus,
                                               AudioBus* output_bus,
                                               const float* pan_values,
                                               uint32_t frames_to_process) {
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

  if (!source_l || !source_r || !destination_l || !destination_r) {
    return;
  }

  double gain_l, gain_r, pan_radian;

  int n = frames_to_process;

  if (number_of_input_channels == 1) {  // For mono source case.
    while (n--) {
      float input_l = *source_l++;
      double pan = ClampTo(*pan_values++, -1.0, 1.0);
      // Pan from left to right [-1; 1] will be normalized as [0; 1].
      pan_radian = (pan * 0.5 + 0.5) * kPiOverTwoDouble;
      gain_l = fdlibm::cos(pan_radian);
      gain_r = fdlibm::sin(pan_radian);
      *destination_l++ = static_cast<float>(input_l * gain_l);
      *destination_r++ = static_cast<float>(input_l * gain_r);
    }
  } else {  // For stereo source case.
    while (n--) {
      float input_l = *source_l++;
      float input_r = *source_r++;
      double pan = ClampTo(*pan_values++, -1.0, 1.0);
      // Normalize [-1; 0] to [0; 1]. Do nothing when [0; 1].
      pan_radian = (pan <= 0 ? pan + 1 : pan) * kPiOverTwoDouble;
      gain_l = fdlibm::cos(pan_radian);
      gain_r = fdlibm::sin(pan_radian);
      if (pan <= 0) {
        *destination_l++ = static_cast<float>(input_l + input_r * gain_l);
        *destination_r++ = static_cast<float>(input_r * gain_r);
      } else {
        *destination_l++ = static_cast<float>(input_l * gain_l);
        *destination_r++ = static_cast<float>(input_r + input_l * gain_r);
      }
    }
  }
}

void StereoPanner::PanToTargetValue(const AudioBus* input_bus,
                                    AudioBus* output_bus,
                                    float pan_value,
                                    uint32_t frames_to_process) {
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

  if (!source_l || !source_r || !destination_l || !destination_r) {
    return;
  }

  float target_pan = ClampTo(pan_value, -1.0, 1.0);

  int n = frames_to_process;

  if (number_of_input_channels == 1) {  // For mono source case.
    // Pan from left to right [-1; 1] will be normalized as [0; 1].
    double pan_radian = (target_pan * 0.5 + 0.5) * kPiOverTwoDouble;

    double gain_l = fdlibm::cos(pan_radian);
    double gain_r = fdlibm::sin(pan_radian);

    // TODO(rtoy): This can be vectorized using vector_math::Vsmul
    while (n--) {
      float input_l = *source_l++;
      *destination_l++ = static_cast<float>(input_l * gain_l);
      *destination_r++ = static_cast<float>(input_l * gain_r);
    }
  } else {  // For stereo source case.
    // Normalize [-1; 0] to [0; 1] for the left pan position (<= 0), and
    // do nothing when [0; 1].
    double pan_radian =
        (target_pan <= 0 ? target_pan + 1 : target_pan) * kPiOverTwoDouble;

    double gain_l = fdlibm::cos(pan_radian);
    double gain_r = fdlibm::sin(pan_radian);

    // TODO(rtoy): Consider moving the if statement outside the loop
    // since |target_pan| is constant inside the loop.
    while (n--) {
      float input_l = *source_l++;
      float input_r = *source_r++;
      if (target_pan <= 0) {
        // When [-1; 0], keep left channel intact and equal-power pan the
        // right channel only.
        *destination_l++ = static_cast<float>(input_l + input_r * gain_l);
        *destination_r++ = static_cast<float>(input_r * gain_r);
      } else {
        // When [0; 1], keep right channel intact and equal-power pan the
        // left channel only.
        *destination_l++ = static_cast<float>(input_l * gain_l);
        *destination_r++ = static_cast<float>(input_r + input_l * gain_r);
      }
    }
  }
}

}  // namespace blink
