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

#include "third_party/blink/renderer/platform/audio/reverb_convolver_stage.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "third_party/blink/renderer/platform/audio/reverb_accumulation_buffer.h"
#include "third_party/blink/renderer/platform/audio/reverb_convolver.h"
#include "third_party/blink/renderer/platform/audio/reverb_input_buffer.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"

namespace blink {

ReverbConvolverStage::ReverbConvolverStage(
    const float* impulse_response,
    size_t,
    size_t reverb_total_latency,
    size_t stage_offset,
    size_t stage_length,
    size_t fft_size,
    size_t render_phase,
    size_t render_slice_size,
    ReverbAccumulationBuffer* accumulation_buffer,
    float scale,
    bool direct_mode)
    : accumulation_buffer_(accumulation_buffer),
      accumulation_read_index_(0),
      input_read_index_(0),
      direct_mode_(direct_mode) {
  DCHECK(impulse_response);
  DCHECK(accumulation_buffer);

  if (!direct_mode_) {
    fft_kernel_ = std::make_unique<FFTFrame>(fft_size);
    fft_kernel_->DoPaddedFFT(impulse_response + stage_offset, stage_length);
    // Account for the normalization (if any) of the convolver.  By linearity,
    // we can scale the FFT by the factor instead of the input.  We do it this
    // way so we don't need to create a temporary for the scaled result before
    // computing the FFT.
    if (scale != 1) {
      fft_kernel_->ScaleFFT(scale);
    }
    fft_convolver_ = std::make_unique<FFTConvolver>(fft_size);
  } else {
    DCHECK(!stage_offset);
    DCHECK_LE(stage_length, fft_size / 2);

    auto direct_kernel = std::make_unique<AudioFloatArray>(fft_size / 2);
    direct_kernel->CopyToRange(impulse_response, 0, stage_length);
    // Account for the normalization (if any) of the convolver node.
    if (scale != 1) {
      vector_math::Vsmul(direct_kernel->Data(), 1, &scale,
                         direct_kernel->Data(), 1, stage_length);
    }
    direct_convolver_ = std::make_unique<DirectConvolver>(
        render_slice_size, std::move(direct_kernel));
  }
  temporary_buffer_.Allocate(render_slice_size);

  // The convolution stage at offset stageOffset needs to have a corresponding
  // delay to cancel out the offset.
  size_t total_delay = stage_offset + reverb_total_latency;

  // But, the FFT convolution itself incurs fftSize / 2 latency, so subtract
  // this out...
  size_t half_size = fft_size / 2;
  if (!direct_mode_) {
    DCHECK_GE(total_delay, half_size);
    if (total_delay >= half_size)
      total_delay -= half_size;
  }

  // We divide up the total delay, into pre and post delay sections so that we
  // can schedule at exactly the moment when the FFT will happen.  This is
  // coordinated with the other stages, so they don't all do their FFTs at the
  // same time...
  int max_pre_delay_length = std::min(half_size, total_delay);
  pre_delay_length_ = total_delay > 0 ? render_phase % max_pre_delay_length : 0;
  if (pre_delay_length_ > total_delay)
    pre_delay_length_ = 0;

  post_delay_length_ = total_delay - pre_delay_length_;
  pre_read_write_index_ = 0;
  frames_processed_ = 0;  // total frames processed so far

  size_t delay_buffer_size =
      pre_delay_length_ < fft_size ? fft_size : pre_delay_length_;
  delay_buffer_size = delay_buffer_size < render_slice_size ? render_slice_size
                                                            : delay_buffer_size;
  pre_delay_buffer_.Allocate(delay_buffer_size);
}

void ReverbConvolverStage::ProcessInBackground(ReverbConvolver* convolver,
                                               uint32_t frames_to_process) {
  ReverbInputBuffer* input_buffer = convolver->InputBuffer();
  float* source =
      input_buffer->DirectReadFrom(&input_read_index_, frames_to_process);
  Process(source, frames_to_process);
}

void ReverbConvolverStage::Process(const float* source,
                                   uint32_t frames_to_process) {
  DCHECK(source);
  if (!source)
    return;

  // Deal with pre-delay stream : note special handling of zero delay.

  const float* pre_delayed_source;
  float* pre_delayed_destination;
  float* temporary_buffer;
  bool is_temporary_buffer_safe = false;
  if (pre_delay_length_ > 0) {
    // Handles both the read case (call to process() ) and the write case
    // (memcpy() )
    bool is_pre_delay_safe =
        pre_read_write_index_ + frames_to_process <= pre_delay_buffer_.size();
    DCHECK(is_pre_delay_safe);
    if (!is_pre_delay_safe)
      return;

    is_temporary_buffer_safe = frames_to_process <= temporary_buffer_.size();

    pre_delayed_destination = pre_delay_buffer_.Data() + pre_read_write_index_;
    pre_delayed_source = pre_delayed_destination;
    temporary_buffer = temporary_buffer_.Data();
  } else {
    // Zero delay
    pre_delayed_destination = nullptr;
    pre_delayed_source = source;
    temporary_buffer = pre_delay_buffer_.Data();

    is_temporary_buffer_safe = frames_to_process <= pre_delay_buffer_.size();
  }

  DCHECK(is_temporary_buffer_safe);
  if (!is_temporary_buffer_safe)
    return;

  if (frames_processed_ < pre_delay_length_) {
    // For the first m_preDelayLength frames don't process the convolver,
    // instead simply buffer in the pre-delay.  But while buffering the
    // pre-delay, we still need to update our index.
    accumulation_buffer_->UpdateReadIndex(&accumulation_read_index_,
                                          frames_to_process);
  } else {
    // Now, run the convolution (into the delay buffer).
    // An expensive FFT will happen every fftSize / 2 frames.
    // We process in-place here...
    if (!direct_mode_)
      fft_convolver_->Process(fft_kernel_.get(), pre_delayed_source,
                              temporary_buffer, frames_to_process);
    else
      direct_convolver_->Process(pre_delayed_source, temporary_buffer,
                                 frames_to_process);

    // Now accumulate into reverb's accumulation buffer.
    accumulation_buffer_->Accumulate(temporary_buffer, frames_to_process,
                                     &accumulation_read_index_,
                                     post_delay_length_);
  }

  // Finally copy input to pre-delay.
  if (pre_delay_length_ > 0) {
    memcpy(pre_delayed_destination, source, sizeof(float) * frames_to_process);
    pre_read_write_index_ += frames_to_process;

    DCHECK_LE(pre_read_write_index_, pre_delay_length_);
    if (pre_read_write_index_ >= pre_delay_length_)
      pre_read_write_index_ = 0;
  }

  frames_processed_ += frames_to_process;
}

void ReverbConvolverStage::Reset() {
  if (!direct_mode_)
    fft_convolver_->Reset();
  else
    direct_convolver_->Reset();
  pre_delay_buffer_.Zero();
  accumulation_read_index_ = 0;
  input_read_index_ = 0;
  frames_processed_ = 0;
}

}  // namespace blink
