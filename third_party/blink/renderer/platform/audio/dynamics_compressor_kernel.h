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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_DYNAMICS_COMPRESSOR_KERNEL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_DYNAMICS_COMPRESSOR_KERNEL_H_

#include <memory>
#include "base/macros.h"
#include "third_party/blink/renderer/platform/audio/audio_array.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class PLATFORM_EXPORT DynamicsCompressorKernel {
  DISALLOW_NEW();

 public:
  DynamicsCompressorKernel(float sample_rate, unsigned number_of_channels);

  void SetNumberOfChannels(unsigned);

  // Performs stereo-linked compression.
  void Process(const float* source_channels[],
               float* destination_channels[],
               unsigned number_of_channels,
               unsigned frames_to_process,

               float db_threshold,
               float db_knee,
               float ratio,
               float attack_time,
               float release_time,
               float pre_delay_time,
               float db_post_gain,
               float effect_blend,

               float release_zone1,
               float release_zone2,
               float release_zone3,
               float release_zone4);

  void Reset();

  unsigned LatencyFrames() const { return last_pre_delay_frames_; }

  float SampleRate() const { return sample_rate_; }

  float MeteringGain() const { return metering_gain_; }

  double TailTime() const;

 protected:
  float sample_rate_;

  float detector_average_;
  float compressor_gain_;

  // Metering
  float metering_release_k_;
  float metering_gain_;

  // Lookahead section.
  enum { kMaxPreDelayFrames = 1024 };
  enum { kMaxPreDelayFramesMask = kMaxPreDelayFrames - 1 };
  enum {
    kDefaultPreDelayFrames = 256
  };  // setPreDelayTime() will override this initial value
  unsigned last_pre_delay_frames_;
  void SetPreDelayTime(float);

  Vector<std::unique_ptr<AudioFloatArray>> pre_delay_buffers_;
  int pre_delay_read_index_;
  int pre_delay_write_index_;

  float max_attack_compression_diff_db_;

  // Static compression curve.
  float KneeCurve(float x, float k);
  float Saturate(float x, float k);
  float SlopeAt(float x, float k);
  float KAtSlope(float desired_slope);

  float UpdateStaticCurveParameters(float db_threshold,
                                    float db_knee,
                                    float ratio);

  // Amount of input change in dB required for 1 dB of output change.
  // This applies to the portion of the curve above m_kneeThresholdDb (see
  // below).
  float ratio_;
  float slope_;  // Inverse ratio.

  // The input to output change below the threshold is linear 1:1.
  float linear_threshold_;
  float db_threshold_;

  // m_dbKnee is the number of dB above the threshold before we enter the
  // "ratio" portion of the curve.
  // m_kneeThresholdDb = m_dbThreshold + m_dbKnee
  // The portion between m_dbThreshold and m_kneeThresholdDb is the "soft knee"
  // portion of the curve which transitions smoothly from the linear portion to
  // the ratio portion.
  float db_knee_;
  float knee_threshold_;
  float knee_threshold_db_;
  float yknee_threshold_db_;

  // Internal parameter for the knee portion of the curve.
  float knee_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DynamicsCompressorKernel);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_DYNAMICS_COMPRESSOR_KERNEL_H_
