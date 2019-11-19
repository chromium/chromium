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

#include "third_party/blink/renderer/platform/audio/audio_resampler.h"
#include "third_party/blink/renderer/platform/audio/audio_resampler_kernel.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

const size_t AudioResamplerKernel::kMaxFramesToProcess = 128;

AudioResamplerKernel::AudioResamplerKernel(AudioResampler* resampler)
    : resampler_(resampler),
      // The buffer size must be large enough to hold up to two extra sample
      // frames for the linear interpolation.
      source_buffer_(
          2 + static_cast<int>(kMaxFramesToProcess * AudioResampler::kMaxRate)),
      virtual_read_index_(0.0),
      fill_index_(0) {
  last_values_[0] = 0.0f;
  last_values_[1] = 0.0f;
}

float* AudioResamplerKernel::GetSourcePointer(
    uint32_t frames_to_process,
    size_t* number_of_source_frames_needed_p) {
  DCHECK_LE(frames_to_process, kMaxFramesToProcess);

  // Calculate the next "virtual" index.  After process() is called,
  // m_virtualReadIndex will equal this value.
  double next_fractional_index =
      virtual_read_index_ + frames_to_process * Rate();

  // Because we're linearly interpolating between the previous and next sample
  // we need to round up so we include the next sample.
  int end_index = static_cast<int>(next_fractional_index +
                                   1.0);  // round up to next integer index

  // Determine how many input frames we'll need.
  // We need to fill the buffer up to and including endIndex (so add 1) but
  // we've already buffered m_fillIndex frames from last time.
  size_t frames_needed = 1 + end_index - fill_index_;
  if (number_of_source_frames_needed_p)
    *number_of_source_frames_needed_p = frames_needed;

  // Do bounds checking for the source buffer.
  DCHECK_LT(fill_index_, source_buffer_.size());
  DCHECK_LE(fill_index_ + frames_needed, source_buffer_.size());

  return source_buffer_.Data() + fill_index_;
}

void AudioResamplerKernel::Process(float* destination,
                                   uint32_t frames_to_process) {
  DCHECK_LE(frames_to_process, kMaxFramesToProcess);

  float* source = source_buffer_.Data();

  double rate = this->Rate();
  rate = clampTo(rate, 0.0, AudioResampler::kMaxRate);

  // Start out with the previous saved values (if any).
  if (fill_index_ > 0) {
    source[0] = last_values_[0];
    source[1] = last_values_[1];
  }

  // Make a local copy.
  double virtual_read_index = virtual_read_index_;

  // Sanity check source buffer access.
  DCHECK_GT(frames_to_process, 0u);
  DCHECK_GE(virtual_read_index, 0);
  DCHECK_LT(1 + static_cast<unsigned>(virtual_read_index +
                                      (frames_to_process - 1) * rate),
            source_buffer_.size());

  // Do the linear interpolation.
  int n = frames_to_process;
  while (n--) {
    unsigned read_index = static_cast<unsigned>(virtual_read_index);
    double interpolation_factor = virtual_read_index - read_index;

    double sample1 = source[read_index];
    double sample2 = source[read_index + 1];

    double sample =
        (1.0 - interpolation_factor) * sample1 + interpolation_factor * sample2;

    *destination++ = static_cast<float>(sample);

    virtual_read_index += rate;
  }

  // Save the last two sample-frames which will later be used at the beginning
  // of the source buffer the next time around.
  int read_index = static_cast<int>(virtual_read_index);
  last_values_[0] = source[read_index];
  last_values_[1] = source[read_index + 1];
  fill_index_ = 2;

  // Wrap the virtual read index back to the start of the buffer.
  virtual_read_index -= read_index;

  // Put local copy back into member variable.
  virtual_read_index_ = virtual_read_index;
}

void AudioResamplerKernel::Reset() {
  virtual_read_index_ = 0.0;
  fill_index_ = 0;
  last_values_[0] = 0.0f;
  last_values_[1] = 0.0f;
}

double AudioResamplerKernel::Rate() const {
  return resampler_->Rate();
}

}  // namespace blink
