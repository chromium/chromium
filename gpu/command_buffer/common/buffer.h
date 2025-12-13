// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_BUFFER_H_
#define GPU_COMMAND_BUFFER_COMMON_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/ref_counted.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "gpu/command_buffer/common/gpu_command_buffer_common_export.h"
namespace gpu {

class GPU_COMMAND_BUFFER_COMMON_EXPORT BufferBacking {
 public:
  virtual ~BufferBacking() = default;
  virtual const base::UnsafeSharedMemoryRegion& shared_memory_region() const;
  virtual base::UnguessableToken GetGUID() const;
  void* GetMemory() {
    return const_cast<void*>(std::as_const(*this).GetMemory());
  }
  virtual const void* GetMemory() const = 0;
  base::span<uint8_t> as_byte_span() {
    // SAFETY, this is the same as_byte_span() just without const.
    base::span<const uint8_t> tmp = std::as_const(*this).as_byte_span();
    return UNSAFE_BUFFERS(
        base::span<uint8_t>(const_cast<uint8_t*>(tmp.data()), tmp.size()));
  }
  virtual base::span<const uint8_t> as_byte_span() const = 0;
  virtual uint32_t GetSize() const = 0;
};

class GPU_COMMAND_BUFFER_COMMON_EXPORT MemoryBufferBacking
    : public BufferBacking {
 public:
  explicit MemoryBufferBacking(uint32_t size, uint32_t alignment = 0);

  MemoryBufferBacking(const MemoryBufferBacking&) = delete;
  MemoryBufferBacking& operator=(const MemoryBufferBacking&) = delete;

  ~MemoryBufferBacking() override;
  const void* GetMemory() const override;
  base::span<const uint8_t> as_byte_span() const override;
  uint32_t GetSize() const override;

 private:
  base::AlignedHeapArray<uint8_t> memory_;
};

class GPU_COMMAND_BUFFER_COMMON_EXPORT SharedMemoryBufferBacking
    : public BufferBacking {
 public:
  SharedMemoryBufferBacking(
      base::UnsafeSharedMemoryRegion shared_memory_region,
      base::WritableSharedMemoryMapping shared_memory_mapping);

  SharedMemoryBufferBacking(const SharedMemoryBufferBacking&) = delete;
  SharedMemoryBufferBacking& operator=(const SharedMemoryBufferBacking&) =
      delete;

  ~SharedMemoryBufferBacking() override;
  const base::UnsafeSharedMemoryRegion& shared_memory_region() const override;
  base::UnguessableToken GetGUID() const override;
  const void* GetMemory() const override;
  base::span<const uint8_t> as_byte_span() const override;
  uint32_t GetSize() const override;

 private:
  base::UnsafeSharedMemoryRegion shared_memory_region_;
  base::WritableSharedMemoryMapping shared_memory_mapping_;
};

// Buffer owns a piece of shared-memory of a certain size.
class GPU_COMMAND_BUFFER_COMMON_EXPORT Buffer
    : public base::RefCountedThreadSafe<Buffer> {
 public:
  explicit Buffer(std::unique_ptr<BufferBacking> backing);

  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;

  BufferBacking* backing() const { return backing_.get(); }
  void* memory() { return as_byte_span().data(); }
  const void* memory() const { return as_byte_span().data(); }
  base::span<uint8_t> as_byte_span() { return backing_->as_byte_span(); }
  base::span<const uint8_t> as_byte_span() const {
    return backing_->as_byte_span();
  }
  uint32_t size() const { return backing_->GetSize(); }

  // Returns empty span if the address overflows the memory.
  base::span<uint8_t> GetSpanData(uint32_t data_offset,
                                  uint32_t data_size) const;

  // Returns nullptr if the address overflows the memory.
  void* GetDataAddress(uint32_t data_offset, uint32_t data_size) const;

  // Returns nullptr if the address overflows the memory.
  void* GetDataAddressAndSize(uint32_t data_offset, uint32_t* data_size) const;

  // Returns the remaining size of the buffer after an offset
  uint32_t GetRemainingSize(uint32_t data_offset) const;

 private:
  friend class base::RefCountedThreadSafe<Buffer>;
  ~Buffer();

  std::unique_ptr<BufferBacking> backing_;
};

inline std::unique_ptr<BufferBacking> MakeBackingFromSharedMemory(
    base::UnsafeSharedMemoryRegion shared_memory_region,
    base::WritableSharedMemoryMapping shared_memory_mapping) {
  return std::make_unique<SharedMemoryBufferBacking>(
      std::move(shared_memory_region), std::move(shared_memory_mapping));
}
inline scoped_refptr<Buffer> MakeBufferFromSharedMemory(
    base::UnsafeSharedMemoryRegion shared_memory_region,
    base::WritableSharedMemoryMapping shared_memory_mapping) {
  return base::MakeRefCounted<Buffer>(MakeBackingFromSharedMemory(
      std::move(shared_memory_region), std::move(shared_memory_mapping)));
}

inline scoped_refptr<Buffer> MakeMemoryBuffer(uint32_t size,
                                              uint32_t alignment = 0) {
  return base::MakeRefCounted<Buffer>(
      std::make_unique<MemoryBufferBacking>(size, alignment));
}

// Generates a process unique buffer ID which can be safely used with
// GetBufferGUIDForTracing.
GPU_COMMAND_BUFFER_COMMON_EXPORT int32_t GetNextBufferId();

// Generates GUID which can be used to trace buffer using an Id.
GPU_COMMAND_BUFFER_COMMON_EXPORT base::trace_event::MemoryAllocatorDumpGuid
GetBufferGUIDForTracing(uint64_t tracing_process_id, int32_t buffer_id);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_BUFFER_H_
