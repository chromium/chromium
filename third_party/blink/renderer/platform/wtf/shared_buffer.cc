/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

#include <cstddef>
#include <memory>

#include "base/compiler_specific.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "third_party/blink/renderer/platform/wtf/text/utf8.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace WTF {

SegmentedBuffer::Iterator& SegmentedBuffer::Iterator::operator++() {
  DCHECK(!IsEnd());
  ++segment_it_;
  Init(0);
  return *this;
}

SegmentedBuffer::Iterator::Iterator(const SegmentedBuffer* buffer)
    : segment_it_(buffer->segments_.end()), buffer_(buffer) {
  DCHECK(IsEnd());
}

SegmentedBuffer::Iterator::Iterator(Vector<Segment>::const_iterator segment_it,
                                    size_t offset,
                                    const SegmentedBuffer* buffer)
    : segment_it_(segment_it), buffer_(buffer) {
  Init(offset);
}

void SegmentedBuffer::Iterator::Init(size_t offset) {
  if (IsEnd()) {
    value_ = base::span<const char>();
    return;
  }
  value_ = base::span(segment_it_->data()).subspan(offset);
}

void SegmentedBuffer::Append(base::span<const char> data) {
  if (data.empty()) {
    return;
  }
  Append(Vector<char>(data));
}

void SegmentedBuffer::Append(Vector<char>&& vector) {
  if (vector.empty()) {
    return;
  }
  const size_t start_position = size_;
  size_ += vector.size();
  segments_.emplace_back(start_position, std::move(vector));
}

void SegmentedBuffer::Clear() {
  segments_.clear();
  size_ = 0;
}

SegmentedBuffer::Iterator SegmentedBuffer::begin() const {
  return GetIteratorAt(static_cast<size_t>(0));
}

SegmentedBuffer::Iterator SegmentedBuffer::end() const {
  return Iterator(this);
}

SegmentedBuffer::Iterator SegmentedBuffer::GetIteratorAtInternal(
    size_t position) const {
  if (position >= size()) {
    return cend();
  }
  Vector<Segment>::const_iterator it = segments_.begin();
  if (position < it->data().size()) {
    return Iterator(it, position, this);
  }
  it = std::upper_bound(it, segments_.end(), position,
                        [](const size_t& position, const Segment& segment) {
                          return position < segment.start_position();
                        });
  --it;
  return Iterator(it, position - it->start_position(), this);
}

bool SegmentedBuffer::GetBytesInternal(void* dest, size_t dest_size) const {
  if (!dest)
    return false;

  size_t offset = 0;
  for (const auto& span : *this) {
    if (offset >= dest_size)
      break;
    size_t to_be_written = std::min(span.size(), dest_size - offset);
    memcpy(static_cast<char*>(dest) + offset, span.data(), to_be_written);
    offset += to_be_written;
  }
  return offset == dest_size;
}

void SegmentedBuffer::GetMemoryDumpNameAndSize(String& dump_name,
                                               size_t& dump_size) const {
  dump_name = "/segments";
  dump_size = size_;
}

SegmentedBuffer::DeprecatedFlatData::DeprecatedFlatData(
    const SegmentedBuffer* buffer)
    : buffer_(buffer) {
  DCHECK(buffer_);
  if (buffer_->segments_.empty()) {
    data_ = nullptr;
    return;
  }
  if (buffer_->segments_.size() == 1) {
    data_ = buffer_->segments_.begin()->data().data();
    return;
  }
  flat_buffer_ = buffer_->CopyAs<Vector<char>>();
  data_ = flat_buffer_.data();
}

Vector<Vector<char>> SegmentedBuffer::TakeData() && {
  Vector<Vector<char>> result;
  result.ReserveInitialCapacity(segments_.size());
  for (auto& segment : segments_) {
    result.push_back(std::move(segment.data()));
  }
  Clear();
  return result;
}

SharedBuffer::SharedBuffer() = default;

SharedBuffer::SharedBuffer(base::span<const char> data) {
  Append(data);
}

SharedBuffer::SharedBuffer(base::span<const unsigned char> data)
    : SharedBuffer(base::as_chars(data)) {}

SharedBuffer::SharedBuffer(SegmentedBuffer&& data)
    : SegmentedBuffer(std::move(data)) {}

SharedBuffer::~SharedBuffer() = default;

scoped_refptr<SharedBuffer> SharedBuffer::Create(Vector<char>&& vector) {
  scoped_refptr<SharedBuffer> buffer = Create();
  buffer->Append(std::move(vector));
  return buffer;
}

}  // namespace WTF
