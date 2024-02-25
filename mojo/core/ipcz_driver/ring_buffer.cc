// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ipcz_driver/ring_buffer.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/numerics/safe_math.h"
#include "base/ranges/algorithm.h"
#include "mojo/core/ipcz_driver/shared_buffer_mapping.h"

namespace mojo::core::ipcz_driver {

RingBuffer::RingBuffer(scoped_refptr<SharedBufferMapping> mapping)
    : mapping_(std::move(mapping)) {}

RingBuffer::~RingBuffer() = default;

size_t RingBuffer::Write(base::span<const uint8_t> source) {
  auto bytes = MapRange(ComplementRange(data_range_));
  const size_t first_chunk_size = std::min(source.size(), bytes.first.size());
  const size_t second_chunk_size =
      std::min(source.size() - first_chunk_size, bytes.second.size());
  base::ranges::copy(source.first(first_chunk_size), bytes.first.data());
  base::ranges::copy(source.subspan(first_chunk_size, second_chunk_size),
                     bytes.second.data());

  const size_t write_size = first_chunk_size + second_chunk_size;
  bool ok = ExtendDataRange(write_size);
  DCHECK(ok);
  return write_size;
}

bool RingBuffer::WriteAll(base::span<const uint8_t> source) {
  if (source.size() > available_capacity()) {
    return false;
  }

  const size_t n = Write(source);
  DCHECK_EQ(n, source.size());
  return true;
}

size_t RingBuffer::Read(base::span<uint8_t> target) {
  const size_t n = Peek(target);
  bool ok = DiscardAll(n);
  DCHECK(ok);
  return n;
}

bool RingBuffer::ReadAll(base::span<uint8_t> target) {
  if (target.size() > data_size()) {
    return false;
  }

  const size_t n = Read(target);
  DCHECK_EQ(n, target.size());
  return true;
}

size_t RingBuffer::Peek(base::span<uint8_t> target) {
  auto bytes = MapRange(data_range_);
  const size_t first_chunk_size = std::min(target.size(), bytes.first.size());
  const size_t second_chunk_size =
      std::min(target.size() - first_chunk_size, bytes.second.size());
  memcpy(target.data(), bytes.first.data(), first_chunk_size);
  memcpy(target.subspan(first_chunk_size).data(), bytes.second.data(),
         second_chunk_size);
  return first_chunk_size + second_chunk_size;
}

bool RingBuffer::PeekAll(base::span<uint8_t> target) {
  if (target.size() > data_size()) {
    return false;
  }

  const size_t n = Peek(target);
  DCHECK_EQ(n, target.size());
  return true;
}

size_t RingBuffer::Discard(size_t n) {
  const size_t num_bytes = std::min(n, data_size());
  bool ok = DiscardAll(num_bytes);
  DCHECK(ok);
  return num_bytes;
}

bool RingBuffer::DiscardAll(size_t n) {
  if (n > data_size()) {
    return false;
  }

  size_t new_offset;
  if (!base::CheckAdd(data_range_.offset, n).AssignIfValid(&new_offset)) {
    return false;
  }

  data_range_ = {.offset = new_offset % capacity(), .size = data_size() - n};
  return true;
}

bool RingBuffer::ExtendDataRange(size_t n) {
  if (n > available_capacity()) {
    return false;
  }
  data_range_.size += n;
  return true;
}

void RingBuffer::Serialize(SerializedState& state) {
  state.offset = base::checked_cast<uint32_t>(data_range_.offset);
  state.size = base::checked_cast<uint32_t>(data_range_.size);
}

bool RingBuffer::Deserialize(const SerializedState& state) {
  const uint32_t data_offset = state.offset;
  const uint32_t data_size = state.size;
  if (data_offset >= capacity() || data_size > capacity()) {
    return false;
  }

  data_range_ = {.offset = data_offset, .size = data_size};
  return true;
}

RingBuffer::SplitBytes RingBuffer::MapRange(
    const RingBuffer::Range& range) const {
  DCHECK_LE(range.offset, capacity());
  const size_t first_chunk_size =
      std::min(range.size, capacity() - range.offset);
  return {
      mapping().bytes().subspan(range.offset).first(first_chunk_size),
      mapping().bytes().first(range.size - first_chunk_size),
  };
}

RingBuffer::Range RingBuffer::ComplementRange(const Range& range) const {
  const size_t buffer_size = capacity();
  DCHECK_LE(range.offset, buffer_size);
  DCHECK_LE(range.size, buffer_size);
  const size_t end = range.offset + range.size;
  return {
      .offset = end >= buffer_size ? end - buffer_size : end,
      .size = buffer_size - range.size,
  };
}

base::span<uint8_t> RingBuffer::GetAvailableCapacityView() const {
  return MapRange(ComplementRange(data_range_)).first;
}

base::span<const uint8_t> RingBuffer::GetReadableDataView() const {
  return MapRange(data_range_).first;
}

}  // namespace mojo::core::ipcz_driver
