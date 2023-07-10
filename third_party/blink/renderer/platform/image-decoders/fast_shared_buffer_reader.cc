/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/image-decoders/fast_shared_buffer_reader.h"

namespace blink {

FastSharedBufferReader::FastSharedBufferReader(
    scoped_refptr<SegmentReader> data)
    : data_(std::move(data)),
      segment_(nullptr),
      segment_length_(0),
      data_position_(0) {}

FastSharedBufferReader::~FastSharedBufferReader() = default;

void FastSharedBufferReader::SetData(scoped_refptr<SegmentReader> data) {
  if (data == data_) {
    return;
  }
  data_ = std::move(data);
  ClearCache();
}

void FastSharedBufferReader::ClearCache() {
  segment_ = nullptr;
  segment_length_ = 0;
  data_position_ = 0;
}

const char* FastSharedBufferReader::GetConsecutiveData(size_t data_position,
                                                       size_t length,
                                                       char* buffer) const {
  CHECK_LE(data_position + length, data_->size());

  // Use the cached segment if it can serve the request.
  if (data_position >= data_position_ &&
      data_position + length <= data_position_ + segment_length_) {
    return segment_ + data_position - data_position_;
  }

  // Return a pointer into |data_| if the request doesn't span segments.
  GetSomeDataInternal(data_position);
  if (length <= segment_length_) {
    return segment_;
  }

  for (char* dest = buffer;;) {
    size_t copy = std::min(length, segment_length_);
    memcpy(dest, segment_, copy);
    length -= copy;
    if (!length) {
      return buffer;
    }

    // Continue reading the next segment.
    dest += copy;
    GetSomeDataInternal(data_position_ + copy);
  }
}

size_t FastSharedBufferReader::GetSomeData(const char*& some_data,
                                           size_t data_position) const {
  GetSomeDataInternal(data_position);
  some_data = segment_;
  return segment_length_;
}

void FastSharedBufferReader::GetSomeDataInternal(size_t data_position) const {
  data_position_ = data_position;
  segment_length_ = data_->GetSomeData(segment_, data_position);
  DCHECK(segment_length_);
}

}  // namespace blink
