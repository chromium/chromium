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

#include "third_party/blink/renderer/platform/audio/audio_delay_dsp_kernel.h"

#include <cmath>
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

AudioDelayDSPKernel::AudioDelayDSPKernel(AudioDSPKernelProcessor* processor,
                                         size_t processing_size_in_frames)
    : AudioDSPKernel(processor),
      write_index_(0),
      delay_times_(processing_size_in_frames) {}

AudioDelayDSPKernel::AudioDelayDSPKernel(double max_delay_time,
                                         float sample_rate)
    : AudioDSPKernel(sample_rate),
      max_delay_time_(max_delay_time),
      write_index_(0) {
  DCHECK_GT(max_delay_time, 0.0);
  DCHECK(!std::isnan(max_delay_time));
  if (max_delay_time <= 0.0 || std::isnan(max_delay_time))
    return;

  size_t buffer_length = BufferLengthForDelay(max_delay_time, sample_rate);
  DCHECK(buffer_length);
  if (!buffer_length)
    return;

  buffer_.Allocate(buffer_length);
  buffer_.Zero();
}

size_t AudioDelayDSPKernel::BufferLengthForDelay(double max_delay_time,
                                                 double sample_rate) const {
  // Compute the length of the buffer needed to handle a max delay of
  // |maxDelayTime|. One is added to handle the case where the actual delay
  // equals the maximum delay.
  return 1 + audio_utilities::TimeToSampleFrame(max_delay_time, sample_rate);
}

bool AudioDelayDSPKernel::HasSampleAccurateValues() {
  return false;
}

void AudioDelayDSPKernel::CalculateSampleAccurateValues(float*, size_t) {
  NOTREACHED();
}

double AudioDelayDSPKernel::DelayTime(float sample_rate) {
  return desired_delay_frames_ / sample_rate;
}

void AudioDelayDSPKernel::Process(const float* source,
                                  float* destination,
                                  size_t frames_to_process) {
  size_t buffer_length = buffer_.size();
  float* buffer = buffer_.Data();

  DCHECK(buffer_length);
  if (!buffer_length)
    return;

  DCHECK(source);
  DCHECK(destination);
  if (!source || !destination)
    return;

  float sample_rate = this->SampleRate();
  double delay_time = 0;
  float* delay_times = delay_times_.Data();
  double max_time = MaxDelayTime();

  bool sample_accurate = HasSampleAccurateValues();

  if (sample_accurate) {
    CalculateSampleAccurateValues(delay_times, frames_to_process);
  } else {
    delay_time = this->DelayTime(sample_rate);

    // Make sure the delay time is in a valid range.
    delay_time = clampTo(delay_time, 0.0, max_time);
  }

  for (unsigned i = 0; i < frames_to_process; ++i) {
    if (sample_accurate) {
      delay_time = delay_times[i];
      if (std::isnan(delay_time))
        delay_time = max_time;
      else
        delay_time = clampTo(delay_time, 0.0, max_time);
    }

    double desired_delay_frames = delay_time * sample_rate;

    double read_position = write_index_ + buffer_length - desired_delay_frames;
    if (read_position >= buffer_length)
      read_position -= buffer_length;

    // Linearly interpolate in-between delay times.
    int read_index1 = static_cast<int>(read_position);
    int read_index2 = (read_index1 + 1) % buffer_length;
    double interpolation_factor = read_position - read_index1;

    double input = static_cast<float>(*source++);
    buffer[write_index_] = static_cast<float>(input);
    write_index_ = (write_index_ + 1) % buffer_length;

    double sample1 = buffer[read_index1];
    double sample2 = buffer[read_index2];

    double output =
        (1.0 - interpolation_factor) * sample1 + interpolation_factor * sample2;

    *destination++ = static_cast<float>(output);
  }
}

void AudioDelayDSPKernel::Reset() {
  buffer_.Zero();
}

bool AudioDelayDSPKernel::RequiresTailProcessing() const {
  // Always return true even if the tail time and latency might both
  // be zero. This is for simplicity; most interesting delay nodes
  // have non-zero delay times anyway.  And it's ok to return true. It
  // just means the node lives a little longer than strictly
  // necessary.
  return true;
}

double AudioDelayDSPKernel::TailTime() const {
  // Account for worst case delay.
  // Don't try to track actual delay time which can change dynamically.
  return max_delay_time_;
}

double AudioDelayDSPKernel::LatencyTime() const {
  return 0;
}

}  // namespace blink
