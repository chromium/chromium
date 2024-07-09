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

#include "third_party/blink/renderer/platform/audio/reverb_accumulation_buffer.h"

#include <algorithm>

#include "third_party/blink/renderer/platform/audio/vector_math.h"

namespace blink {

ReverbAccumulationBuffer::ReverbAccumulationBuffer(uint32_t length)
    : buffer_(length), read_index_(0), read_time_frame_(0) {}

void ReverbAccumulationBuffer::ReadAndClear(float* destination,
                                            uint32_t number_of_frames) {
  uint32_t buffer_length = buffer_.size();

  DCHECK_LE(read_index_, buffer_length);
  DCHECK_LE(number_of_frames, buffer_length);

  uint32_t frames_available = buffer_length - read_index_;
  uint32_t number_of_frames1 = std::min(number_of_frames, frames_available);
  uint32_t number_of_frames2 = number_of_frames - number_of_frames1;

  float* source = buffer_.Data();
  memcpy(destination, source + read_index_, sizeof(float) * number_of_frames1);
  memset(source + read_index_, 0, sizeof(float) * number_of_frames1);

  // Handle wrap-around if necessary
  if (number_of_frames2 > 0) {
    memcpy(destination + number_of_frames1, source,
           sizeof(float) * number_of_frames2);
    memset(source, 0, sizeof(float) * number_of_frames2);
  }

  read_index_ = (read_index_ + number_of_frames) % buffer_length;
  read_time_frame_ += number_of_frames;
}

void ReverbAccumulationBuffer::UpdateReadIndex(
    uint32_t* read_index,
    uint32_t number_of_frames) const {
  // Update caller's readIndex
  *read_index = (*read_index + number_of_frames) % buffer_.size();
}

uint32_t ReverbAccumulationBuffer::Accumulate(float* source,
                                              uint32_t number_of_frames,
                                              uint32_t* read_index,
                                              size_t delay_frames) {
  uint32_t buffer_length = buffer_.size();

  uint32_t write_index = (*read_index + delay_frames) % buffer_length;

  // Update caller's readIndex
  *read_index = (*read_index + number_of_frames) % buffer_length;

  uint32_t frames_available = buffer_length - write_index;
  uint32_t number_of_frames1 = std::min(number_of_frames, frames_available);
  uint32_t number_of_frames2 = number_of_frames - number_of_frames1;

  float* destination = buffer_.Data();

  DCHECK_LE(write_index, buffer_length);
  DCHECK_LE(number_of_frames1 + write_index, buffer_length);
  DCHECK_LE(number_of_frames2, buffer_length);

  vector_math::Vadd(source, 1, destination + write_index, 1,
                    destination + write_index, 1, number_of_frames1);

  // Handle wrap-around if necessary
  if (number_of_frames2 > 0) {
    vector_math::Vadd(source + number_of_frames1, 1, destination, 1,
                      destination, 1, number_of_frames2);
  }

  return write_index;
}

void ReverbAccumulationBuffer::Reset() {
  buffer_.Zero();
  read_index_ = 0;
  read_time_frame_ = 0;
}

}  // namespace blink
