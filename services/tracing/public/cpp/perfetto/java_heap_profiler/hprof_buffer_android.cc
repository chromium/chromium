// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/tracing/public/cpp/perfetto/java_heap_profiler/hprof_buffer_android.h"

#include "base/check.h"
#include "services/tracing/public/cpp/perfetto/java_heap_profiler/hprof_data_type_android.h"

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

uint32_t HprofBuffer::GetUInt32FromBytes(size_t num_bytes) {
  uint32_t val = 0;
  for (size_t i = 0; i < num_bytes; ++i) {
    val = (val << 8) + GetByte();
  }
  return val;
}

uint64_t HprofBuffer::GetUInt64FromBytes(size_t num_bytes) {
  uint64_t val = 0;
  for (size_t i = 0; i < num_bytes; ++i) {
    val = (val << 8) + GetByte();
  }
  return val;
}

bool HprofBuffer::HasRemaining() {
  return offset_ < size_;
}

void HprofBuffer::set_id_size(unsigned id_size) {
  DCHECK(id_size == 4 || id_size == 8);
  object_id_size_in_bytes_ = id_size;
}

void HprofBuffer::set_position(size_t new_position) {
  DCHECK(new_position <= size_ && new_position >= 0);
  offset_ = new_position;
}

// Skips |delta| bytes in the buffer.
void HprofBuffer::Skip(uint32_t delta) {
  set_position(offset_ + delta);
}

void HprofBuffer::SkipBytesByType(DataType type) {
  Skip(SizeOfType(type));
}

void HprofBuffer::SkipId() {
  Skip(object_id_size_in_bytes_);
}

const char* HprofBuffer::DataPosition() {
  return reinterpret_cast<const char*>((data_ + offset_).get());
}

uint32_t HprofBuffer::SizeOfType(uint32_t index) {
  uint32_t object_size = kTypeSizes[index];

  // If type is object, return id_size.
  return object_size == 0 ? object_id_size_in_bytes_ : object_size;
}

unsigned char HprofBuffer::GetByte() {
  DCHECK(HasRemaining());
  unsigned char byte = data_[offset_];
  ++offset_;
  return byte;
}

}  // namespace tracing
