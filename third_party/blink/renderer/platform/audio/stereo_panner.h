// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_STEREO_PANNER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_STEREO_PANNER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class AudioBus;

// Implement the equal-power panning algorithm for mono or stereo input. See:
// https://webaudio.github.io/web-audio-api/#Spatialzation-equal-power-panning

class PLATFORM_EXPORT StereoPanner {
  USING_FAST_MALLOC(StereoPanner);

 public:
  explicit StereoPanner(float sample_rate);
  ~StereoPanner() = default;

  void PanWithSampleAccurateValues(const AudioBus* input_bus,
                                   AudioBus* output_bus,
                                   const float* pan_values,
                                   uint32_t frames_to_process);
  void PanToTargetValue(const AudioBus* input_bus,
                        AudioBus* output_bus,
                        float pan_value,
                        uint32_t frames_to_process);

 private:
  DISALLOW_COPY_AND_ASSIGN(StereoPanner);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_STEREO_PANNER_H_
