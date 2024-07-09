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

#include "third_party/blink/renderer/platform/audio/reverb_input_buffer.h"

namespace blink {

ReverbInputBuffer::ReverbInputBuffer(size_t length)
    : buffer_(length), write_index_(0) {}

void ReverbInputBuffer::Write(const float* source_p, size_t number_of_frames) {
  size_t buffer_length = buffer_.size();
  size_t index = WriteIndex();
  size_t new_index = index + number_of_frames;

  CHECK_LE(new_index, buffer_length);

  memcpy(buffer_.Data() + index, source_p, sizeof(float) * number_of_frames);

  if (new_index >= buffer_length) {
    new_index = 0;
  }

  SetWriteIndex(new_index);
}

float* ReverbInputBuffer::DirectReadFrom(size_t* read_index,
                                         size_t number_of_frames) {
  uint32_t buffer_length = buffer_.size();
  DCHECK(read_index);
  DCHECK_LE(*read_index + number_of_frames, buffer_length);

  float* source_p = buffer_.Data();
  float* p = source_p + *read_index;

  // Update readIndex
  *read_index = (*read_index + number_of_frames) % buffer_length;

  return p;
}

void ReverbInputBuffer::Reset() {
  buffer_.Zero();
  write_index_ = 0;
}

}  // namespace blink
