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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_AUDIO_DELAY_DSP_KERNEL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_AUDIO_DELAY_DSP_KERNEL_H_

#include "third_party/blink/renderer/platform/audio/audio_array.h"
#include "third_party/blink/renderer/platform/audio/audio_dsp_kernel.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class PLATFORM_EXPORT AudioDelayDSPKernel : public AudioDSPKernel {
 public:
  AudioDelayDSPKernel(double max_delay_time,
                      float sample_rate,
                      unsigned render_quantum_frames);

  // Process the delay.  Basically dispatches to either ProcessKRate or
  // ProcessARate.
  void Process(const float* source,
               float* destination,
               uint32_t frames_to_process) override;

  // Handles k-rate processing
  void ProcessKRate(const float* source,
                    float* destination,
                    uint32_t frames_to_process);

  // Handles a-rate processing
  void ProcessARate(const float* source,
                    float* destination,
                    uint32_t frames_to_process);
  // Main processing loop for ProcessARate using scalar operations.  Returns the
  // new write_index.
  int ProcessARateScalar(unsigned start,
                         int w_index,
                         float* destination,
                         uint32_t frames_to_process) const;

  // Vector version of ProcessARateScalar.  Returns the number of samples
  // process by this function and the updated wirte_index_.
  std::tuple<unsigned, int> ProcessARateVector(
      float* destination,
      uint32_t frames_to_process) const;

  // Handle an NaN values in |delay_times|.  Replace NaN with |max_time|.
  void HandleNaN(float* delay_times,
                 uint32_t frames_to_process,
                 float max_time);

  void Reset() override;

  float MaxDelayTime() const { return max_delay_time_; }

  void SetDelayFrames(double number_of_frames) {
    desired_delay_frames_ = number_of_frames;
  }

  double TailTime() const override;
  double LatencyTime() const override;
  bool RequiresTailProcessing() const override;

 protected:
  AudioDelayDSPKernel(AudioDSPKernelProcessor*,
                      size_t processing_size_in_frames);

  virtual bool HasSampleAccurateValues();
  virtual void CalculateSampleAccurateValues(float* delay_times,
                                             uint32_t frames_to_process);
  virtual double DelayTime(float sample_rate);
  virtual bool IsAudioRate();

  AudioFloatArray buffer_;

  // Time is usually best kept as a double, but AudioParams are inherently
  // floats, so make this a float to keep everything consistent.
  float max_delay_time_;

  int write_index_;
  double desired_delay_frames_;

  AudioFloatArray delay_times_;

  // Temporary buffer used to hold the second sample for interpolation if
  // needed.
  AudioFloatArray temp_buffer_;

  size_t BufferLengthForDelay(double delay_time,
                              double sample_rate,
                              unsigned render_quantum_frames) const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_AUDIO_DELAY_DSP_KERNEL_H_
