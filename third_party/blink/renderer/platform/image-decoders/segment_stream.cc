// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/segment_stream.h"
#include <utility>
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"

namespace blink {

SegmentStream::SegmentStream() = default;

SegmentStream::SegmentStream(SegmentStream&& rhs)
    : reader_(std::move(rhs.reader_)), position_(rhs.position_) {}

SegmentStream& SegmentStream::operator=(SegmentStream&& rhs) {
  reader_ = std::move(rhs.reader_);
  position_ = rhs.position_;

  return *this;
}

SegmentStream::~SegmentStream() = default;

void SegmentStream::SetReader(scoped_refptr<SegmentReader> reader) {
  reader_ = std::move(reader);
}

bool SegmentStream::IsCleared() const {
  return !reader_ || position_ > reader_->size();
}

size_t SegmentStream::read(void* buffer, size_t size) {
  if (IsCleared())
    return 0;

  size = std::min(size, reader_->size() - position_);

  size_t bytes_advanced = 0;
  if (!buffer) {  // skipping, not reading
    bytes_advanced = size;
  } else {
    bytes_advanced = peek(buffer, size);
  }

  position_ += bytes_advanced;

  return bytes_advanced;
}

size_t SegmentStream::peek(void* buffer, size_t size) const {
  if (IsCleared())
    return 0;

  size = std::min(size, reader_->size() - position_);

  size_t total_bytes_peeked = 0;
  char* buffer_as_char_ptr = reinterpret_cast<char*>(buffer);
  while (size) {
    const char* segment = nullptr;
    size_t bytes_peeked =
        reader_->GetSomeData(segment, position_ + total_bytes_peeked);
    if (!bytes_peeked)
      break;
    if (bytes_peeked > size)
      bytes_peeked = size;

    memcpy(buffer_as_char_ptr, segment, bytes_peeked);
    buffer_as_char_ptr += bytes_peeked;
    size -= bytes_peeked;
    total_bytes_peeked += bytes_peeked;
  }

  return total_bytes_peeked;
}

bool SegmentStream::isAtEnd() const {
  return !reader_ || position_ >= reader_->size();
}

bool SegmentStream::rewind() {
  position_ = 0;
  return true;
}

bool SegmentStream::seek(size_t position) {
  position_ = position;
  return true;
}

bool SegmentStream::move(long offset) {
  DCHECK_GT(offset, 0);
  position_ += offset;
  return true;
}

size_t SegmentStream::getLength() const {
  if (reader_)
    return reader_->size();

  return 0;
}

}  // namespace blink
