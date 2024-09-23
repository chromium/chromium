// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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
