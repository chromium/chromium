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

  size_t buffer_length = BufferLengthForDelay(max_delay_time, sample_rate);
  DCHECK(buffer_length);

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

void AudioDelayDSPKernel::CalculateSampleAccurateValues(float*, uint32_t) {
  NOTREACHED();
}

double AudioDelayDSPKernel::DelayTime(float sample_rate) {
  return desired_delay_frames_ / sample_rate;
}

void AudioDelayDSPKernel::Process(const float* source,
                                  float* destination,
                                  uint32_t frames_to_process) {
  size_t buffer_length = buffer_.size();
  float* buffer = buffer_.Data();

  DCHECK(buffer_length);
  DCHECK(source);
  DCHECK(destination);

  float sample_rate = this->SampleRate();
  double max_time = MaxDelayTime();

  if (HasSampleAccurateValues()) {
    float* delay_times = delay_times_.Data();
    CalculateSampleAccurateValues(delay_times, frames_to_process);

    int w_index = write_index_;

    for (unsigned i = 0; i < frames_to_process; ++i) {
      double delay_time = delay_times[i];
      // TODO(crbug.com/1013345): Don't need this if that bug is fixed
      if (std::isnan(delay_time))
        delay_time = max_time;

      double desired_delay_frames = delay_time * sample_rate;

      double read_position = w_index + buffer_length - desired_delay_frames;
      if (read_position >= buffer_length)
        read_position -= buffer_length;

      // Linearly interpolate in-between delay times.
      int read_index1 = static_cast<int>(read_position);
      int read_index2 = read_index1 + 1;
      if (read_index2 >= static_cast<int>(buffer_length))
        read_index2 -= buffer_length;

      double interpolation_factor = read_position - read_index1;

      buffer[w_index] = *source++;

      ++w_index;
      if (w_index >= static_cast<int>(buffer_length))
        w_index -= buffer_length;

      float sample1 = buffer[read_index1];
      float sample2 = buffer[read_index2];

      double output =
          (1 - interpolation_factor) * sample1 + interpolation_factor * sample2;

      *destination++ = static_cast<float>(output);
    }

    write_index_ = w_index;
  } else {
    // This is basically the same as above, but optimized for the case where the
    // delay time is constant for the current render.
    //
    // TODO(crbug.com/1012198): There are still some further optimizations that
    // could be done.  interp_factor could be a float to eliminate several
    // conversions between floats and doubles.  It might be possible to get rid
    // of the wrapping if the buffer were longer.  This may aslo allow
    // |write_index_| to be different from |read_index1| or |read_index2| which
    // simplifies the loop a bit.

    double delay_time = this->DelayTime(sample_rate);
    // Make sure the delay time is in a valid range.
    delay_time = clampTo(delay_time, 0.0, max_time);
    double desired_delay_frames = delay_time * sample_rate;
    double read_position = write_index_ + buffer_length - desired_delay_frames;
    if (read_position >= buffer_length)
      read_position -= buffer_length;

    // Linearly interpolate in-between delay times.  |read_index1| and
    // |read_index2| are the indices of the frames to be used for
    // interpolation.
    int read_index1 = static_cast<int>(read_position);
    int read_index2 = (read_index1 + 1) % buffer_length;
    double interp_factor = read_position - read_index1;

    int w_index = write_index_;

    for (unsigned i = 0; i < frames_to_process; ++i) {
      // Copy the latest sample into the buffer.  Needed because
      // w_index could be the same as read_index1 or read_index2.
      buffer[w_index] = *source++;
      float sample1 = buffer[read_index1];
      float sample2 = buffer[read_index2];

      // Update the indices and wrap them to the beginning of the buffer if
      // needed.
      ++w_index;
      ++read_index1;
      ++read_index2;
      if (w_index >= static_cast<int>(buffer_length))
        w_index -= buffer_length;
      if (read_index1 >= static_cast<int>(buffer_length))
        read_index1 -= buffer_length;
      if (read_index2 >= static_cast<int>(buffer_length))
        read_index2 -= buffer_length;

      // Linearly interpolate between samples.
      *destination++ = (1 - interp_factor) * sample1 + interp_factor * sample2;
    }

    write_index_ = w_index;
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
