// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/java_heap_profiler/hprof_buffer_android.h"

#include "base/logging.h"

namespace tracing {

HprofBuffer::HprofBuffer(const unsigned char* data, size_t size)
    : data_(data), size_(size) {}

uint32_t HprofBuffer::GetOneByte() {
  return GetUInt32FromBytes(1);
}

uint32_t HprofBuffer::GetTwoBytes() {
  return GetUInt32FromBytes(2);
}

uint32_t HprofBuffer::GetFourBytes() {
  return GetUInt32FromBytes(4);
}

uint64_t HprofBuffer::GetId() {
  return GetUInt64FromBytes(object_id_size_in_bytes_);
}

bool HprofBuffer::HasRemaining() {
  return data_position_ < size_;
}

void HprofBuffer::set_id_size(unsigned id_size) {
  DCHECK(id_size == 4 || id_size == 8);
  object_id_size_in_bytes_ = id_size;
}

void HprofBuffer::set_position(size_t new_position) {
  DCHECK(new_position <= size_ && new_position >= 0);
  data_position_ = new_position;
}

// Skips |delta| bytes in the buffer.
void HprofBuffer::Skip(uint32_t delta) {
  set_position(data_position_ + delta);
}

unsigned char HprofBuffer::GetByte() {
  DCHECK(HasRemaining());
  unsigned char byte = data_[data_position_];
  ++data_position_;
  return byte;
}

// Read in the next |num_bytes| as an uint32_t.
uint32_t HprofBuffer::GetUInt32FromBytes(size_t num_bytes) {
  uint32_t val = 0;
  for (size_t i = 0; i < num_bytes; ++i) {
    val = (val << 8) + GetByte();
  }
  return val;
}

// Read in the next |num_bytes| as an uint64_t.
uint64_t HprofBuffer::GetUInt64FromBytes(size_t num_bytes) {
  uint64_t val = 0;
  for (size_t i = 0; i < num_bytes; ++i) {
    val = (val << 8) + GetByte();
  }
  return val;
}
}  // namespace tracing
