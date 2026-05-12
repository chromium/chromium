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

#include "third_party/blink/renderer/platform/audio/reverb_accumulation_buffer.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"

namespace blink {

ReverbAccumulationBuffer::ReverbAccumulationBuffer(uint32_t length)
    : buffer_(length), read_index_(0) {}

void ReverbAccumulationBuffer::ReadAndClear(base::span<float> destination) {
  const size_t buffer_length = buffer_.size();
  const size_t number_of_frames = destination.size();

  DCHECK_LE(read_index_, buffer_length);
  DCHECK_LE(number_of_frames, buffer_length);

  const size_t frames_available = buffer_length - read_index_;
  const size_t number_of_frames1 = std::min(number_of_frames, frames_available);
  const size_t number_of_frames2 = number_of_frames - number_of_frames1;

  base::span<float> source = buffer_.as_span();
  destination.first(number_of_frames1)
      .copy_from(source.subspan(read_index_, number_of_frames1));
  std::ranges::fill(source.subspan(read_index_, number_of_frames1), 0.0f);

  // Handle wrap-around if necessary
  if (number_of_frames2 > 0) {
    destination.subspan(number_of_frames1, number_of_frames2)
        .copy_from(source.first(number_of_frames2));
    std::ranges::fill(source.first(number_of_frames2), 0.0f);
  }

  read_index_ = (read_index_ + number_of_frames) % buffer_length;
}

void ReverbAccumulationBuffer::UpdateReadIndex(
    uint32_t* read_index,
    uint32_t number_of_frames) const {
  // Update caller's readIndex
  *read_index = (*read_index + number_of_frames) % buffer_.size();
}

uint32_t ReverbAccumulationBuffer::Accumulate(base::span<const float> source,
                                              uint32_t* read_index,
                                              size_t delay_frames) {
  const size_t number_of_frames = source.size();
  const size_t buffer_length = buffer_.size();

  const size_t write_index = (*read_index + delay_frames) % buffer_length;

  // Update caller's readIndex
  *read_index = (*read_index + number_of_frames) % buffer_length;

  const size_t frames_available = buffer_length - write_index;
  const size_t number_of_frames1 = std::min(number_of_frames, frames_available);
  const size_t number_of_frames2 = number_of_frames - number_of_frames1;

  base::span<float> destination = buffer_.as_span();

  DCHECK_LE(write_index, buffer_length);
  DCHECK_LE(number_of_frames1 + write_index, buffer_length);
  DCHECK_LE(number_of_frames2, buffer_length);

  vector_math::Vadd(source.first(number_of_frames1).data(),
                    destination.subspan(write_index, number_of_frames1).data(),
                    destination.subspan(write_index, number_of_frames1).data(),
                    number_of_frames1);

  // Handle wrap-around if necessary
  if (number_of_frames2 > 0) {
    vector_math::Vadd(
        source.subspan(number_of_frames1, number_of_frames2).data(),
        destination.first(number_of_frames2).data(),
        destination.first(number_of_frames2).data(), number_of_frames2);
  }

  return write_index;
}

void ReverbAccumulationBuffer::Reset() {
  buffer_.Zero();
  read_index_ = 0;
}

}  // namespace blink
