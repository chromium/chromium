// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/fast_shared_buffer_reader.h"

namespace blink {

FastSharedBufferReader::FastSharedBufferReader(
    scoped_refptr<SegmentReader> data)
    : data_(std::move(data)), data_position_(0) {}

FastSharedBufferReader::~FastSharedBufferReader() = default;

void FastSharedBufferReader::SetData(scoped_refptr<SegmentReader> data) {
  if (data == data_) {
    return;
  }
  data_ = std::move(data);
  ClearCache();
}

void FastSharedBufferReader::ClearCache() {
  segment_ = {};
  data_position_ = 0;
}

base::span<const uint8_t> FastSharedBufferReader::GetConsecutiveData(
    size_t data_position,
    size_t length,
    base::span<uint8_t> buffer) const {
  CHECK_LE(data_position + length, data_->size());

  // Use the cached segment if it can serve the request.
  if (data_position >= data_position_ &&
      data_position + length <= data_position_ + segment_.size()) {
    return segment_.subspan(data_position - data_position_, length);
  }

  // Return a span into `data_` if the request doesn't span segments.
  GetSomeDataInternal(data_position);
  if (length <= segment_.size()) {
    return segment_.first(length);
  }

  base::span<uint8_t> dest = buffer;
  size_t remaining = length;
  while (true) {
    size_t copy = std::min(remaining, segment_.size());
    dest.take_first(copy).copy_from(segment_.first(copy));
    remaining -= copy;
    if (remaining == 0) {
      break;
    }
    // Continue reading the next segment.
    GetSomeDataInternal(data_position_ + copy);
  }
  return buffer.first(length);
}

base::span<const uint8_t> FastSharedBufferReader::GetSomeData(
    size_t data_position) const {
  GetSomeDataInternal(data_position);
  return segment_;
}

void FastSharedBufferReader::GetSomeDataInternal(size_t data_position) const {
  data_position_ = data_position;
  segment_ = data_->GetSomeData(data_position);
  DCHECK(!segment_.empty());
}

}  // namespace blink
