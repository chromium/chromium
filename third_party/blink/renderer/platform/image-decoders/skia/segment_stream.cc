// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/image-decoders/skia/segment_stream.h"

#include <utility>

#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"

namespace blink {

SegmentStream::SegmentStream(size_t reading_offset)
    : position_(reading_offset), reading_offset_(reading_offset) {}

SegmentStream::SegmentStream(SegmentStream&& rhs)
    : reader_(std::move(rhs.reader_)),
      position_(rhs.position_),
      reading_offset_(rhs.reading_offset_) {}

SegmentStream& SegmentStream::operator=(SegmentStream&& rhs) {
  reader_ = std::move(rhs.reader_);
  position_ = rhs.position_;
  reading_offset_ = rhs.reading_offset_;

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
  if (IsCleared()) {
    return 0;
  }

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
  if (IsCleared()) {
    return 0;
  }

  size = std::min(size, reader_->size() - position_);

  size_t total_bytes_peeked = 0;
  auto buffer_span = base::span(static_cast<uint8_t*>(buffer), size);
  while (!buffer_span.empty()) {
    base::span<const uint8_t> segment =
        reader_->GetSomeData(position_ + total_bytes_peeked);
    if (segment.empty()) {
      break;
    }
    if (segment.size() > buffer_span.size()) {
      segment = segment.first(buffer_span.size());
    }

    buffer_span.copy_prefix_from(segment);
    buffer_span = buffer_span.subspan(segment.size());
    total_bytes_peeked += segment.size();
  }

  return total_bytes_peeked;
}

bool SegmentStream::isAtEnd() const {
  return !reader_ || position_ >= reader_->size();
}

bool SegmentStream::rewind() {
  position_ = reading_offset_;
  return true;
}

bool SegmentStream::hasPosition() const {
  return true;
}

size_t SegmentStream::getPosition() const {
  return position_ - reading_offset_;
}

bool SegmentStream::seek(size_t position) {
  position_ = reading_offset_ + position;
  return true;
}

bool SegmentStream::move(long offset) {
  DCHECK_GT(offset, 0);
  position_ += offset;
  return true;
}

bool SegmentStream::hasLength() const {
  return true;
}

size_t SegmentStream::getLength() const {
  if (reader_) {
    return reader_->size();
  }

  return 0;
}

}  // namespace blink
