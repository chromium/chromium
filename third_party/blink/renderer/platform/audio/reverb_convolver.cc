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

#include "third_party/blink/renderer/platform/audio/reverb_convolver.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"

namespace blink {

std::unique_ptr<ReverbConvolver> ReverbConvolver::TryCreate(
    AudioChannel* impulse_response,
    unsigned render_slice_size,
    unsigned max_fft_size,
    size_t convolver_render_phase,
    float scale) {
  auto convolver = base::WrapUnique(new ReverbConvolver());
  if (!convolver->Initialize(impulse_response, render_slice_size, max_fft_size,
                             convolver_render_phase, scale)) {
    return nullptr;
  }
  return convolver;
}

bool ReverbConvolver::Initialize(AudioChannel* impulse_response,
                                 unsigned render_slice_size,
                                 unsigned max_fft_size,
                                 size_t convolver_render_phase,
                                 float scale) {
  impulse_response_length_ = impulse_response->length();

  uint32_t total_length = 0;
  if (!base::CheckAdd(impulse_response->length(), render_slice_size)
           .AssignIfValid(&total_length)) {
    return false;
  }

  if (!accumulation_buffer_.TryAllocate(total_length)) {
    return false;
  }

  uint32_t total_response_length = impulse_response->length();

  // The total latency is zero because the direct-convolution is used in the
  // leading portion.
  size_t reverb_total_latency = 0;

  unsigned stage_offset = 0;
  int i = 0;

  // First stage will be of size kMinFFTSize.  Each next stage will be twice as
  // big until we hit max_fft_size.
  unsigned fft_size = kMinFFTSize;
  while (stage_offset < total_response_length) {
    unsigned stage_size = fft_size / 2;

    // For the last stage, it's possible that stageOffset is such that we're
    // straddling the end of the impulse response buffer (if we use stageSize),
    // so reduce the last stage's length...
    if (stage_size + stage_offset > total_response_length) {
      stage_size = total_response_length - stage_offset;
    }

    // This "staggers" the time when each FFT happens so they don't all happen
    // at the same time
    size_t render_phase = convolver_render_phase + i * render_slice_size;

    bool use_direct_convolver = !stage_offset;

    std::unique_ptr<ReverbConvolverStage> stage =
        ReverbConvolverStage::TryCreate(
            impulse_response->Span(), reverb_total_latency, stage_offset,
            stage_size, fft_size, render_phase, render_slice_size,
            &accumulation_buffer_, scale, use_direct_convolver);

    if (!stage) {
      return false;
    }

    stages_.push_back(std::move(stage));

    stage_offset += stage_size;
    ++i;

    if (!use_direct_convolver) {
      // Figure out next FFT size
      fft_size *= 2;
    }

    if (fft_size > max_fft_size) {
      fft_size = max_fft_size;
    }
  }

  return true;
}

void ReverbConvolver::Process(const AudioChannel* source_channel,
                              AudioChannel* destination_channel,
                              uint32_t frames_to_process) {
  DCHECK(source_channel);
  DCHECK(destination_channel);
  DCHECK_GE(source_channel->length(), frames_to_process);
  DCHECK_GE(destination_channel->length(), frames_to_process);

  // Accumulate contributions from each stage
  for (auto& stage : stages_) {
    stage->Process(source_channel->Span().first(frames_to_process));
  }

  // Finally read from accumulation buffer
  accumulation_buffer_.ReadAndClear(
      destination_channel->MutableSpan().first(frames_to_process));
}

void ReverbConvolver::Reset() {
  for (auto& stage : stages_) {
    stage->Reset();
  }

  accumulation_buffer_.Reset();
}

size_t ReverbConvolver::LatencyFrames() const {
  return 0;
}

}  // namespace blink
