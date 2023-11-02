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

#include "base/location.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

const int kInputBufferSize = 8 * 16384;

// We only process the leading portion of the impulse response in the real-time
// thread.  We don't exceed this length.  It turns out then, that the
// background thread has about 278msec of scheduling slop.  Empirically, this
// has been found to be a good compromise between giving enough time for
// scheduling slop, while still minimizing the amount of processing done in the
// primary (high-priority) thread.  This was found to be a good value on Mac OS
// X, and may work well on other platforms as well, assuming the very rough
// scheduling latencies are similar on these time-scales.  Of course, this code
// may need to be tuned for individual platforms if this assumption is found to
// be incorrect.
const size_t kRealtimeFrameLimit = 8192 + 4096;  // ~278msec @ 44.1KHz

const unsigned kMinFFTSize = 128;
const unsigned kMaxRealtimeFFTSize = 2048;

ReverbConvolver::ReverbConvolver(AudioChannel* impulse_response,
                                 unsigned render_slice_size,
                                 unsigned max_fft_size,
                                 size_t convolver_render_phase,
                                 bool use_background_threads,
                                 float scale)
    : impulse_response_length_(impulse_response->length()),
      accumulation_buffer_(impulse_response->length() + render_slice_size),
      input_buffer_(kInputBufferSize),
      min_fft_size_(
          kMinFFTSize),  // First stage will have this size - successive
                         // stages will double in size each time
      max_fft_size_(max_fft_size)  // until we hit m_maxFFTSize
{
  // If we are using background threads then don't exceed this FFT size for the
  // stages which run in the real-time thread.  This avoids having only one or
  // two large stages (size 16384 or so) at the end which take a lot of time
  // every several processing slices.  This way we amortize the cost over more
  // processing slices.
  max_realtime_fft_size_ = kMaxRealtimeFFTSize;

  const float* response = impulse_response->Data();
  uint32_t total_response_length = impulse_response->length();

  // The total latency is zero because the direct-convolution is used in the
  // leading portion.
  size_t reverb_total_latency = 0;

  unsigned stage_offset = 0;
  int i = 0;
  unsigned fft_size = min_fft_size_;
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
        std::make_unique<ReverbConvolverStage>(
            response, total_response_length, reverb_total_latency, stage_offset,
            stage_size, fft_size, render_phase, render_slice_size,
            &accumulation_buffer_, scale, use_direct_convolver);

    bool is_background_stage = false;

    if (use_background_threads && stage_offset > kRealtimeFrameLimit) {
      background_stages_.push_back(std::move(stage));
      is_background_stage = true;
    } else {
      stages_.push_back(std::move(stage));
    }

    stage_offset += stage_size;
    ++i;

    if (!use_direct_convolver) {
      // Figure out next FFT size
      fft_size *= 2;
    }

    if (use_background_threads && !is_background_stage &&
        fft_size > max_realtime_fft_size_) {
      fft_size = max_realtime_fft_size_;
    }
    if (fft_size > max_fft_size_) {
      fft_size = max_fft_size_;
    }
  }

  // Start up background thread
  // FIXME: would be better to up the thread priority here.  It doesn't need to
  // be real-time, but higher than the default...
  if (use_background_threads && background_stages_.size() > 0) {
    background_thread_ = NonMainThread::CreateThread(
        ThreadCreationParams(ThreadType::kReverbConvolutionBackgroundThread));
  }
}

ReverbConvolver::~ReverbConvolver() {
  // Wait for background thread to stop
  background_thread_.reset();
}

void ReverbConvolver::ProcessInBackground() {
  // Process all of the stages until their read indices reach the input buffer's
  // write index
  size_t write_index = input_buffer_.WriteIndex();

  // Even though it doesn't seem like every stage needs to maintain its own
  // version of readIndex we do this in case we want to run in more than one
  // background thread.
  // FIXME: do better to detect buffer overrun...
  while (background_stages_[0]->InputReadIndex() != write_index) {
    // The ReverbConvolverStages need to process in amounts which evenly divide
    // half the FFT size
    const int kSliceSize = kMinFFTSize / 2;

    // Accumulate contributions from each stage
    for (auto& background_stage : background_stages_) {
      background_stage->ProcessInBackground(this, kSliceSize);
    }
  }
}

void ReverbConvolver::Process(const AudioChannel* source_channel,
                              AudioChannel* destination_channel,
                              uint32_t frames_to_process) {
  DCHECK(source_channel);
  DCHECK(destination_channel);
  DCHECK_GE(source_channel->length(), frames_to_process);
  DCHECK_GE(destination_channel->length(), frames_to_process);

  const float* source = source_channel->Data();
  float* destination = destination_channel->MutableData();
  DCHECK(source);
  DCHECK(destination);

  // Feed input buffer (read by all threads)
  input_buffer_.Write(source, frames_to_process);

  // Accumulate contributions from each stage
  for (auto& stage : stages_) {
    stage->Process(source, frames_to_process);
  }

  // Finally read from accumulation buffer
  accumulation_buffer_.ReadAndClear(destination, frames_to_process);

  // Now that we've buffered more input, post another task to the background
  // thread.
  if (background_thread_) {
    PostCrossThreadTask(
        *background_thread_->GetTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(&ReverbConvolver::ProcessInBackground,
                            CrossThreadUnretained(this)));
  }
}

void ReverbConvolver::Reset() {
  for (auto& stage : stages_) {
    stage->Reset();
  }

  for (auto& background_stage : background_stages_) {
    background_stage->Reset();
  }

  accumulation_buffer_.Reset();
  input_buffer_.Reset();
}

size_t ReverbConvolver::LatencyFrames() const {
  return 0;
}

}  // namespace blink
