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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/audio/delay.h"

#include <cmath>

#include "base/notreached.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

namespace {

void CopyToCircularBuffer(float* buffer,
                          int write_index,
                          int buffer_length,
                          const float* source,
                          uint32_t frames_to_process) {
  // The algorithm below depends on this being true because we don't expect to
  // have to fill the entire buffer more than once.
  DCHECK_GE(static_cast<uint32_t>(buffer_length), frames_to_process);

  // Copy `frames_to_process` values from `source` to the circular buffer that
  // starts at `buffer` of length `buffer_length`.  The copy starts at index
  // `write_index` into the buffer.
  float* write_pointer = &buffer[write_index];
  int remainder = buffer_length - write_index;

  // Copy the sames over, carefully handling the case where we need to wrap
  // around to the beginning of the buffer.
  memcpy(write_pointer, source,
         sizeof(*write_pointer) *
             std::min(static_cast<int>(frames_to_process), remainder));
  memcpy(buffer, source + remainder,
         sizeof(*write_pointer) *
             std::max(0, static_cast<int>(frames_to_process) - remainder));
}

}  // namespace

Delay::Delay(double max_delay_time,
             float sample_rate,
             unsigned render_quantum_frames)
    : max_delay_time_(max_delay_time),
      delay_times_(render_quantum_frames),
      temp_buffer_(render_quantum_frames),
      sample_rate_(sample_rate) {
  DCHECK_GT(max_delay_time_, 0.0);
  DCHECK(std::isfinite(max_delay_time_));

  size_t buffer_length =
      BufferLengthForDelay(max_delay_time, sample_rate, render_quantum_frames);
  DCHECK(buffer_length);

  buffer_.Allocate(buffer_length);
  buffer_.Zero();
}

size_t Delay::BufferLengthForDelay(double max_delay_time,
                                   double sample_rate,
                                   unsigned render_quantum_frames) const {
  // Compute the length of the buffer needed to handle a max delay of
  // `maxDelayTime`. Add an additional render quantum frame size so we can
  // vectorize the delay processing.  The extra space is needed so that writes
  // to the buffer won't overlap reads from the buffer.
  return render_quantum_frames +
         audio_utilities::TimeToSampleFrame(max_delay_time, sample_rate,
                                            audio_utilities::kRoundUp);
}

double Delay::DelayTime(float sample_rate) {
  return desired_delay_frames_ / sample_rate;
}

#if !(defined(ARCH_CPU_X86_FAMILY) || defined(CPU_ARM_NEON))
// Default scalar versions if simd/neon are not available.
std::tuple<unsigned, int> Delay::ProcessARateVector(
    float* destination,
    uint32_t frames_to_process) const {
  // We don't have a vectorized version, so just do nothing and return the 0 to
  // indicate no frames processed and return the current write_index_.
  return std::make_tuple(0, write_index_);
}

void Delay::HandleNaN(float* delay_times,
                      uint32_t frames_to_process,
                      float max_time) {
  for (unsigned k = 0; k < frames_to_process; ++k) {
    if (std::isnan(delay_times[k])) {
      delay_times[k] = max_time;
    }
  }
}
#endif

int Delay::ProcessARateScalar(unsigned start,
                              int w_index,
                              float* destination,
                              uint32_t frames_to_process) const {
  const int buffer_length = buffer_.size();
  const float* buffer = buffer_.Data();

  DCHECK(buffer_length);
  DCHECK(destination);
  DCHECK_GE(write_index_, 0);
  DCHECK_LT(write_index_, buffer_length);

  float sample_rate = sample_rate_;
  const float* delay_times = delay_times_.Data();

  for (unsigned i = start; i < frames_to_process; ++i) {
    double delay_time = std::fmax(delay_times[i], 0);
    double desired_delay_frames = delay_time * sample_rate;

    double read_position = w_index + buffer_length - desired_delay_frames;
    if (read_position >= buffer_length) {
      read_position -= buffer_length;
    }

    // Linearly interpolate in-between delay times.
    int read_index1 = static_cast<int>(read_position);
    DCHECK_GE(read_index1, 0);
    DCHECK_LT(read_index1, buffer_length);
    int read_index2 = read_index1 + 1;
    if (read_index2 >= buffer_length) {
      read_index2 -= buffer_length;
    }
    DCHECK_GE(read_index2, 0);
    DCHECK_LT(read_index2, buffer_length);

    float interpolation_factor = read_position - read_index1;

    float sample1 = buffer[read_index1];
    float sample2 = buffer[read_index2];

    ++w_index;
    if (w_index >= buffer_length) {
      w_index -= buffer_length;
    }

    destination[i] = sample1 + interpolation_factor * (sample2 - sample1);
  }

  return w_index;
}

void Delay::ProcessARate(const float* source,
                         float* destination,
                         uint32_t frames_to_process) {
  int buffer_length = buffer_.size();
  float* buffer = buffer_.Data();

  DCHECK(buffer_length);
  DCHECK(source);
  DCHECK(destination);
  DCHECK_GE(write_index_, 0);
  DCHECK_LT(write_index_, buffer_length);

  float* delay_times = delay_times_.Data();

  // Any NaN's get converted to max time
  // TODO(crbug.com/1013345): Don't need this if that bug is fixed
  double max_time = MaxDelayTime();
  HandleNaN(delay_times, frames_to_process, max_time);

  CopyToCircularBuffer(buffer, write_index_, buffer_length, source,
                       frames_to_process);

  unsigned frames_processed;
  std::tie(frames_processed, write_index_) =
      ProcessARateVector(destination, frames_to_process);

  if (frames_processed < frames_to_process) {
    write_index_ = ProcessARateScalar(frames_processed, write_index_,
                                      destination, frames_to_process);
  }
}

void Delay::ProcessKRate(const float* source,
                         float* destination,
                         uint32_t frames_to_process) {
  int buffer_length = buffer_.size();
  float* buffer = buffer_.Data();

  DCHECK(buffer_length);
  DCHECK(source);
  DCHECK(destination);
  DCHECK_GE(write_index_, 0);
  DCHECK_LT(write_index_, buffer_length);

  float sample_rate = sample_rate_;
  double max_time = MaxDelayTime();

  // This is basically the same as above, but optimized for the case where the
  // delay time is constant for the current render.

  double delay_time = DelayTime(sample_rate);
  // Make sure the delay time is in a valid range.
  delay_time = ClampTo(delay_time, 0.0, max_time);
  double desired_delay_frames = delay_time * sample_rate;
  int w_index = write_index_;
  double read_position = w_index + buffer_length - desired_delay_frames;

  if (read_position >= buffer_length) {
    read_position -= buffer_length;
  }

  // Linearly interpolate in-between delay times.  `read_index1` and
  // `read_index2` are the indices of the frames to be used for
  // interpolation.
  int read_index1 = static_cast<int>(read_position);
  float interpolation_factor = read_position - read_index1;
  float* buffer_end = &buffer[buffer_length];
  DCHECK_GE(static_cast<unsigned>(buffer_length), frames_to_process);

  // sample1 and sample2 hold the current and next samples in the buffer.
  // These are used for interoplating the delay value.  To reduce memory
  // usage and an extra memcpy, sample1 can be the same as destination.
  float* sample1 = destination;

  // Copy data from the source into the buffer, starting at the write index.
  // The buffer is circular, so carefully handle the wrapping of the write
  // pointer.
  CopyToCircularBuffer(buffer, write_index_, buffer_length, source,
                       frames_to_process);
  w_index += frames_to_process;
  if (w_index >= buffer_length) {
    w_index -= buffer_length;
  }
  write_index_ = w_index;

  // Now copy out the samples from the buffer, starting at the read pointer,
  // carefully handling wrapping of the read pointer.
  float* read_pointer = &buffer[read_index1];

  uint32_t remainder = static_cast<uint32_t>(buffer_end - read_pointer);
  memcpy(sample1, read_pointer,
         sizeof(*sample1) * std::min(frames_to_process, remainder));
  if (frames_to_process > remainder) {
    memcpy(sample1 + remainder, buffer,
           sizeof(*sample1) * (frames_to_process - remainder));
  }

  // If interpolation_factor = 0, we don't need to do any interpolation and
  // sample1 contains the desried values.  We can skip the following code.
  if (interpolation_factor != 0) {
    DCHECK_LE(frames_to_process, temp_buffer_.size());

    int read_index2 = (read_index1 + 1) % buffer_length;
    float* sample2 = temp_buffer_.Data();

    read_pointer = &buffer[read_index2];
    remainder = static_cast<uint32_t>(buffer_end - read_pointer);
    memcpy(sample2, read_pointer,
           sizeof(*sample1) * std::min(frames_to_process, remainder));
    if (frames_to_process > remainder) {
      memcpy(sample2 + remainder, buffer,
             sizeof(*sample1) * (frames_to_process - remainder));
    }

    // Interpolate samples, where f = interpolation_factor
    //   dest[k] = sample1[k] + f*(sample2[k] - sample1[k]);

    // sample2[k] = sample2[k] - sample1[k]
    vector_math::Vsub(sample2, 1, sample1, 1, sample2, 1, frames_to_process);

    // dest[k] = dest[k] + f*sample2[k]
    //         = sample1[k] + f*(sample2[k] - sample1[k]);
    //
    vector_math::Vsma(sample2, 1, interpolation_factor, destination, 1,
                      frames_to_process);
  }
}

void Delay::Reset() {
  buffer_.Zero();
}

}  // namespace blink
