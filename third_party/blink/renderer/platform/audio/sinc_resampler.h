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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_SINC_RESAMPLER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_SINC_RESAMPLER_H_

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "third_party/blink/renderer/platform/audio/audio_array.h"
#include "third_party/blink/renderer/platform/audio/audio_source_provider.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// SincResampler is a high-quality sample-rate converter.

class SincResampler final {
  USING_FAST_MALLOC(SincResampler);

 public:
  // scaleFactor == sourceSampleRate / destinationSampleRate
  // kernelSize can be adjusted for quality (higher is better)
  // numberOfKernelOffsets is used for interpolation and is the number of
  // sub-sample kernel shifts.
  explicit SincResampler(double scale_factor,
                         unsigned kernel_size = 32,
                         unsigned number_of_kernel_offsets = 32);

  SincResampler(const SincResampler&) = delete;
  SincResampler& operator=(const SincResampler&) = delete;

  // Processes `source.size()` from `source` to produce `source.size()
  // / scale_factor_` frames in `destination`.
  void Process(base::span<const float> source, base::span<float> destination);

  // Process with input source callback function for streaming applications.
  void Process(AudioSourceProvider*, base::span<float> destination);

 protected:
  void InitializeKernel();
  void ConsumeSource(base::span<float> buffer);

  const double scale_factor_;
  const unsigned kernel_size_;
  const unsigned number_of_kernel_offsets_;

  // m_kernelStorage has m_numberOfKernelOffsets kernels back-to-back, each of
  // size m_kernelSize.  The kernel offsets are sub-sample shifts of a windowed
  // sinc() shifted from 0.0 to 1.0 sample.
  AudioFloatArray kernel_storage_;

  // m_virtualSourceIndex is an index on the source input buffer with sub-sample
  // precision.  It must be double precision to avoid drift.
  double virtual_source_index_;

  // Source is copied into this buffer for each processing pass.
  AudioFloatArray input_buffer_;

  // m_sourceProvider is used to provide the audio input stream to the
  // resampler.
  raw_ptr<AudioSourceProvider> source_provider_;

  // The buffer is primed once at the very beginning of processing.
  bool is_buffer_primed_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_SINC_RESAMPLER_H_
