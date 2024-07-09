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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/audio/hrtf_kernel.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "third_party/blink/renderer/platform/audio/audio_channel.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

namespace {

// Takes the input AudioChannel as an input impulse response and calculates the
// average group delay.  This represents the initial delay before the most
// energetic part of the impulse response.  The sample-frame delay is removed
// from the impulseP impulse response, and this value  is returned.  The length
// of the passed in AudioChannel must be a power of 2.
float ExtractAverageGroupDelay(AudioChannel* channel,
                               unsigned analysis_fft_size) {
  DCHECK(channel);

  float* impulse_p = channel->MutableData();

  DCHECK_GE(channel->length(), analysis_fft_size);

  // Check for power-of-2.
  DCHECK_EQ(1UL << static_cast<unsigned>(log2(analysis_fft_size)),
            analysis_fft_size);

  FFTFrame estimation_frame(analysis_fft_size);
  estimation_frame.DoFFT(impulse_p);

  const float frame_delay =
      ClampTo<float>(estimation_frame.ExtractAverageGroupDelay());
  estimation_frame.DoInverseFFT(impulse_p);

  return frame_delay;
}

}  // namespace

HRTFKernel::HRTFKernel(AudioChannel* channel,
                       unsigned fft_size,
                       float sample_rate)
    : frame_delay_(0), sample_rate_(sample_rate) {
  DCHECK(channel);

  // Determine the leading delay (average group delay) for the response.
  frame_delay_ = ExtractAverageGroupDelay(channel, fft_size / 2);

  float* impulse_response = channel->MutableData();
  const uint32_t response_length = channel->length();

  // We need to truncate to fit into 1/2 the FFT size (with zero padding) in
  // order to do proper convolution.
  // Truncate if necessary to max impulse response length allowed by FFT.
  const unsigned truncated_response_length =
      std::min(response_length, fft_size / 2);

  // Quick fade-out (apply window) at truncation point
  const unsigned number_of_fade_out_frames = static_cast<unsigned>(
      sample_rate / 4410);  // 10 sample-frames @44.1KHz sample-rate
  DCHECK_LT(number_of_fade_out_frames, truncated_response_length);
  for (unsigned i = truncated_response_length - number_of_fade_out_frames;
       i < truncated_response_length; ++i) {
    float x = 1.0f - static_cast<float>(i - (truncated_response_length -
                                             number_of_fade_out_frames)) /
                         number_of_fade_out_frames;
    impulse_response[i] *= x;
  }

  fft_frame_ = std::make_unique<FFTFrame>(fft_size);
  fft_frame_->DoPaddedFFT(impulse_response, truncated_response_length);
}

// Interpolates two kernels with x: 0 -> 1 and returns the result.
std::unique_ptr<HRTFKernel> HRTFKernel::CreateInterpolatedKernel(
    HRTFKernel* kernel1,
    HRTFKernel* kernel2,
    float x) {
  DCHECK(kernel1);
  DCHECK(kernel2);
  DCHECK_GE(x, 0.0);
  DCHECK_LT(x, 1.0);
  x = ClampTo(x, 0.0f, 1.0f);

  const float sample_rate1 = kernel1->sample_rate_;
  const float sample_rate2 = kernel2->sample_rate_;
  DCHECK_EQ(sample_rate1, sample_rate2);

  const float frame_delay =
      (1 - x) * kernel1->FrameDelay() + x * kernel2->FrameDelay();

  std::unique_ptr<FFTFrame> interpolated_frame =
      FFTFrame::CreateInterpolatedFrame(*kernel1->FftFrame(),
                                        *kernel2->FftFrame(), x);
  return std::make_unique<HRTFKernel>(std::move(interpolated_frame),
                                      frame_delay, sample_rate1);
}

}  // namespace blink
