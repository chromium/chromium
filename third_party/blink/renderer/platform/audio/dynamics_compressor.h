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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_DYNAMICS_COMPRESSOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_DYNAMICS_COMPRESSOR_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/renderer/platform/audio/audio_array.h"
#include "third_party/blink/renderer/platform/audio/dynamics_compressor_kernel.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class AudioBus;

// DynamicsCompressor implements a flexible audio dynamics compression effect
// such as is commonly used in musical production and game audio. It lowers the
// volume of the loudest parts of the signal and raises the volume of the
// softest parts, making the sound richer, fuller, and more controlled.

class PLATFORM_EXPORT DynamicsCompressor {
  USING_FAST_MALLOC(DynamicsCompressor);

 public:
  enum {
    kParamThreshold,
    kParamKnee,
    kParamRatio,
    kParamAttack,
    kParamRelease,
    kParamPreDelay,
    kParamReleaseZone1,
    kParamReleaseZone2,
    kParamReleaseZone3,
    kParamReleaseZone4,
    kParamPostGain,
    kParamFilterStageGain,
    kParamFilterStageRatio,
    kParamFilterAnchor,
    kParamEffectBlend,
    kParamReduction,
    kParamLast
  };

  DynamicsCompressor(float sample_rate, unsigned number_of_channels);

  void Process(const AudioBus* source_bus,
               AudioBus* destination_bus,
               unsigned frames_to_process);
  void Reset();
  void SetNumberOfChannels(unsigned);

  void SetParameterValue(unsigned parameter_id, float value);
  float ParameterValue(unsigned parameter_id);

  float SampleRate() const { return sample_rate_; }
  float Nyquist() const { return sample_rate_ / 2; }

  double TailTime() const;
  double LatencyTime() const {
    return compressor_.LatencyFrames() / static_cast<double>(SampleRate());
  }
  bool RequiresTailProcessing() const {
    // Always return true even if the tail time and latency might both be zero.
    return true;
  }

 protected:
  unsigned number_of_channels_;

  // m_parameters holds the tweakable compressor parameters.
  float parameters_[kParamLast];
  void InitializeParameters();

  float sample_rate_;

  // Emphasis filter controls.
  float last_filter_stage_ratio_;
  float last_anchor_;
  float last_filter_stage_gain_;

  std::unique_ptr<const float* []> source_channels_;
  std::unique_ptr<float* []> destination_channels_;

  // The core compressor.
  DynamicsCompressorKernel compressor_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DynamicsCompressor);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_DYNAMICS_COMPRESSOR_H_
