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

#include "third_party/blink/renderer/platform/audio/delay.h"

#include <cmath>

#include "base/compiler_specific.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

namespace {

void CopyToCircularBuffer(base::span<float> buffer,
                          size_t write_index,
                          base::span<const float> source,
                          size_t frames_to_process) {
  // The algorithm below depends on this being true because we don't expect to
  // have to fill the entire buffer more than once.
  DCHECK_GE(buffer.size(), frames_to_process);
  DCHECK_GE(buffer.size(), write_index);

  // Copy `frames_to_process` values from `source` to the circular buffer that
  // starts at `buffer` of length `buffer.size()`.  The copy starts at index
  // `write_index` into the buffer.
  const size_t remainder = buffer.size() - write_index;

  // Carefully handle the case where we need to wrap around to the beginning of
  // the buffer.
  const size_t first_size = std::min(frames_to_process, remainder);
  buffer.subspan(write_index, first_size).copy_from(source.first(first_size));

  if (frames_to_process > remainder) {
    const size_t second_size = frames_to_process - remainder;
    buffer.first(second_size).copy_from(source.subspan(remainder, second_size));
  }
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
std::tuple<size_t, size_t> Delay::ProcessARateVector(
    base::span<float> destination,
    size_t frames_to_process) const {
  // We don't have a vectorized version, so just do nothing and return the 0 to
  // indicate no frames processed and return the current write_index_.
  return std::make_tuple(0, write_index_);
}

void Delay::HandleNaN(base::span<float> delay_times,
                      size_t frames_to_process,
                      float max_time) {
  for (size_t k = 0; k < frames_to_process; ++k) {
    if (std::isnan(delay_times[k])) {
      delay_times[k] = max_time;
    }
  }
}
#endif

size_t Delay::ProcessARateScalar(size_t start,
                                 size_t w_index,
                                 base::span<float> destination,
                                 size_t frames_to_process) const {
  const size_t buffer_length = buffer_.size();

  DCHECK_LT(write_index_, buffer_length);

  for (unsigned i = start; i < frames_to_process; ++i) {
    double delay_time = std::fmax(delay_times_[i], 0);
    double desired_delay_frames = delay_time * sample_rate_;

    double read_position = w_index + buffer_length - desired_delay_frames;
    if (read_position >= buffer_length) {
      read_position -= buffer_length;
    }
    DCHECK_GE(read_position, 0);

    // Linearly interpolate in-between delay times.
    size_t read_index1 = static_cast<size_t>(read_position);
    DCHECK_LT(read_index1, buffer_length);
    size_t read_index2 = read_index1 + 1;
    if (read_index2 >= buffer_length) {
      read_index2 -= buffer_length;
    }
    DCHECK_LT(read_index2, buffer_length);

    float interpolation_factor = read_position - read_index1;

    float sample1 = buffer_[read_index1];
    float sample2 = buffer_[read_index2];

    ++w_index;
    if (w_index >= buffer_length) {
      w_index -= buffer_length;
    }

    destination[i] = sample1 + interpolation_factor * (sample2 - sample1);
  }

  return w_index;
}

void Delay::ProcessARate(base::span<const float> source,
                         base::span<float> destination,
                         size_t frames_to_process) {
  DCHECK_LT(write_index_, buffer_.size());

  // Any NaN's get converted to max time
  // TODO(crbug.com/1013345): Don't need this if that bug is fixed
  double max_time = MaxDelayTime();
  HandleNaN(delay_times_.as_span(), frames_to_process, max_time);

  CopyToCircularBuffer(buffer_.as_span(), write_index_, source,
                       frames_to_process);

  unsigned frames_processed;
  std::tie(frames_processed, write_index_) =
      ProcessARateVector(destination, frames_to_process);

  if (frames_processed < frames_to_process) {
    write_index_ = ProcessARateScalar(frames_processed, write_index_,
                                      destination, frames_to_process);
  }
}

void Delay::ProcessKRate(base::span<const float> source,
                         base::span<float> destination,
                         size_t frames_to_process) {
  const size_t buffer_length = buffer_.size();

  DCHECK_LT(write_index_, buffer_length);
  DCHECK_GE(buffer_length, frames_to_process);

  float sample_rate = sample_rate_;
  double max_time = MaxDelayTime();

  // This is basically the same as above, but optimized for the case where the
  // delay time is constant for the current render.

  double delay_time = DelayTime(sample_rate);
  // Make sure the delay time is in a valid range.
  delay_time = ClampTo(delay_time, 0.0, max_time);
  double desired_delay_frames = delay_time * sample_rate;
  size_t w_index = write_index_;
  double read_position = w_index + buffer_length - desired_delay_frames;

  if (read_position >= buffer_length) {
    read_position -= buffer_length;
  }
  DCHECK_GE(read_position, 0);

  // Linearly interpolate in-between delay times.  `read_index1` and
  // `read_index2` are the indices of the frames to be used for
  // interpolation.
  size_t read_index1 = static_cast<size_t>(read_position);
  float interpolation_factor = read_position - read_index1;

  // Copy data from the source into the buffer, starting at the write index.
  // The buffer is circular, so carefully handle the wrapping of the write
  // pointer.
  CopyToCircularBuffer(buffer_.as_span(), write_index_, source,
                       frames_to_process);
  w_index += frames_to_process;
  if (w_index >= buffer_length) {
    w_index -= buffer_length;
  }
  write_index_ = w_index;

  // Now copy out the samples from the buffer, starting at the read pointer,
  // carefully handling wrapping of the read pointer.
  size_t remainder = buffer_length - read_index1;

  size_t first_size = std::min(frames_to_process, remainder);
  destination.first(first_size)
      .copy_from(buffer_.as_span().subspan(read_index1, first_size));
  if (frames_to_process > remainder) {
    const size_t second_size = frames_to_process - remainder;
    destination.subspan(remainder, second_size)
        .copy_from(buffer_.as_span().first(second_size));
  }

  // If interpolation_factor = 0, we don't need to do any interpolation and
  // destination contains the desired values.  We can skip the following code.
  if (interpolation_factor != 0) {
    DCHECK_LE(frames_to_process, temp_buffer_.size());
    const size_t read_index2 = (read_index1 + 1) % buffer_length;
    remainder = buffer_length - read_index2;
    first_size = std::min(frames_to_process, remainder);
    temp_buffer_.as_span()
        .first(first_size)
        .copy_from(buffer_.as_span().subspan(read_index2, first_size));
    if (frames_to_process > remainder) {
      const size_t second_size = frames_to_process - remainder;
      temp_buffer_.as_span()
          .subspan(remainder, second_size)
          .copy_from(buffer_.as_span().first(second_size));
    }

    // Interpolate samples, where f = interpolation_factor
    //   dest[k] = dest[k] + f*(temp_buffer_[k] - dest[k]);

    // temp_buffer_[k] = temp_buffer_[k] - dest[k]
    vector_math::Vsub(temp_buffer_.Data(), destination.data(),
                      temp_buffer_.Data(), frames_to_process);

    // dest[k] = dest[k] + f*temp_buffer_[k]
    //         = dest[k] + f*(temp_buffer_[k] - dest[k]);
    //
    vector_math::Vsma(temp_buffer_.Data(), interpolation_factor,
                      destination.data(), frames_to_process);
  }
}

void Delay::Reset() {
  buffer_.Zero();
}

}  // namespace blink
