// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/buffer.h"

#include <stddef.h>
#include <stdint.h>

#include "base/atomic_sequence_num.h"
#include "base/check_op.h"
#include "base/format_macros.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_math.h"
#include "base/strings/stringprintf.h"

namespace gpu {
namespace {

// Global atomic to generate unique buffer IDs.
base::AtomicSequenceNumber g_next_buffer_id;

}  // namespace

const base::UnsafeSharedMemoryRegion& BufferBacking::shared_memory_region()
    const {
  static const base::NoDestructor<base::UnsafeSharedMemoryRegion>
      kInvalidRegion;
  return *kInvalidRegion;
}

base::UnguessableToken BufferBacking::GetGUID() const {
  return base::UnguessableToken();
}

MemoryBufferBacking::MemoryBufferBacking(uint32_t size)
    : memory_(new char[size]), size_(size) {}

MemoryBufferBacking::~MemoryBufferBacking() = default;

void* MemoryBufferBacking::GetMemory() const {
  return memory_.get();
}

uint32_t MemoryBufferBacking::GetSize() const {
  return size_;
}

SharedMemoryBufferBacking::SharedMemoryBufferBacking(
    base::UnsafeSharedMemoryRegion shared_memory_region,
    base::WritableSharedMemoryMapping shared_memory_mapping)
    : shared_memory_region_(std::move(shared_memory_region)),
      shared_memory_mapping_(std::move(shared_memory_mapping)) {
  DCHECK_EQ(shared_memory_region_.GetGUID(), shared_memory_mapping_.guid());
  DCHECK_LE(shared_memory_mapping_.size(), static_cast<size_t>(UINT32_MAX));
}

SharedMemoryBufferBacking::~SharedMemoryBufferBacking() = default;

const base::UnsafeSharedMemoryRegion&
SharedMemoryBufferBacking::shared_memory_region() const {
  return shared_memory_region_;
}

base::UnguessableToken SharedMemoryBufferBacking::GetGUID() const {
  return shared_memory_region_.GetGUID();
}

void* SharedMemoryBufferBacking::GetMemory() const {
  return shared_memory_mapping_.memory();
}

uint32_t SharedMemoryBufferBacking::GetSize() const {
  return static_cast<uint32_t>(shared_memory_mapping_.size());
}

Buffer::Buffer(std::unique_ptr<BufferBacking> backing)
    : backing_(std::move(backing)),
      memory_(backing_->GetMemory()),
      size_(backing_->GetSize()) {
  DCHECK(memory_) << "The memory must be mapped to create a Buffer";
}

Buffer::~Buffer() = default;

void* Buffer::GetDataAddress(uint32_t data_offset, uint32_t data_size) const {
  base::CheckedNumeric<uint32_t> end = data_offset;
  end += data_size;
  if (!end.IsValid() || end.ValueOrDie() > static_cast<uint32_t>(size_))
    return nullptr;
  return static_cast<uint8_t*>(memory_) + data_offset;
}

void* Buffer::GetDataAddressAndSize(uint32_t data_offset,
                                    uint32_t* data_size) const {
  if (data_offset > static_cast<uint32_t>(size_))
    return nullptr;
  *data_size = GetRemainingSize(data_offset);
  return static_cast<uint8_t*>(memory_) + data_offset;
}

uint32_t Buffer::GetRemainingSize(uint32_t data_offset) const {
  if (data_offset > static_cast<uint32_t>(size_))
    return 0;
  return static_cast<uint32_t>(size_) - data_offset;
}

int32_t GetNextBufferId() {
  // 0 is a reserved value.
  return g_next_buffer_id.GetNext() + 1;
}

base::trace_event::MemoryAllocatorDumpGuid GetBufferGUIDForTracing(
    uint64_t tracing_process_id,
    int32_t buffer_id) {
  return base::trace_event::MemoryAllocatorDumpGuid(base::StringPrintf(
      "gpu-buffer-x-process/%" PRIx64 "/%d", tracing_process_id, buffer_id));
}

} // namespace gpu
