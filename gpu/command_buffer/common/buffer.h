// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_BUFFER_H_
#define GPU_COMMAND_BUFFER_COMMON_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "gpu/gpu_export.h"

namespace gpu {

class GPU_EXPORT BufferBacking {
 public:
  virtual ~BufferBacking() = default;
  virtual const base::UnsafeSharedMemoryRegion& shared_memory_region() const;
  virtual base::UnguessableToken GetGUID() const;
  virtual void* GetMemory() const = 0;
  virtual uint32_t GetSize() const = 0;
};

class GPU_EXPORT MemoryBufferBacking : public BufferBacking {
 public:
  explicit MemoryBufferBacking(uint32_t size);
  ~MemoryBufferBacking() override;
  void* GetMemory() const override;
  uint32_t GetSize() const override;

 private:
  std::unique_ptr<char[]> memory_;
  uint32_t size_;
  DISALLOW_COPY_AND_ASSIGN(MemoryBufferBacking);
};


class GPU_EXPORT SharedMemoryBufferBacking : public BufferBacking {
 public:
  SharedMemoryBufferBacking(
      base::UnsafeSharedMemoryRegion shared_memory_region,
      base::WritableSharedMemoryMapping shared_memory_mapping);
  ~SharedMemoryBufferBacking() override;
  const base::UnsafeSharedMemoryRegion& shared_memory_region() const override;
  base::UnguessableToken GetGUID() const override;
  void* GetMemory() const override;
  uint32_t GetSize() const override;

 private:
  base::UnsafeSharedMemoryRegion shared_memory_region_;
  base::WritableSharedMemoryMapping shared_memory_mapping_;
  DISALLOW_COPY_AND_ASSIGN(SharedMemoryBufferBacking);
};

// Buffer owns a piece of shared-memory of a certain size.
class GPU_EXPORT Buffer : public base::RefCountedThreadSafe<Buffer> {
 public:
  explicit Buffer(std::unique_ptr<BufferBacking> backing);

  BufferBacking* backing() const { return backing_.get(); }
  void* memory() const { return memory_; }
  uint32_t size() const { return size_; }

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
  void* memory_;
  uint32_t size_;

  DISALLOW_COPY_AND_ASSIGN(Buffer);
};

static inline std::unique_ptr<BufferBacking> MakeBackingFromSharedMemory(
    base::UnsafeSharedMemoryRegion shared_memory_region,
    base::WritableSharedMemoryMapping shared_memory_mapping) {
  return std::make_unique<SharedMemoryBufferBacking>(
      std::move(shared_memory_region), std::move(shared_memory_mapping));
}
static inline scoped_refptr<Buffer> MakeBufferFromSharedMemory(
    base::UnsafeSharedMemoryRegion shared_memory_region,
    base::WritableSharedMemoryMapping shared_memory_mapping) {
  return base::MakeRefCounted<Buffer>(MakeBackingFromSharedMemory(
      std::move(shared_memory_region), std::move(shared_memory_mapping)));
}

static inline scoped_refptr<Buffer> MakeMemoryBuffer(uint32_t size) {
  return base::MakeRefCounted<Buffer>(
      std::make_unique<MemoryBufferBacking>(size));
}

// Generates a process unique buffer ID which can be safely used with
// GetBufferGUIDForTracing.
GPU_EXPORT int32_t GetNextBufferId();

// Generates GUID which can be used to trace buffer using an Id.
GPU_EXPORT base::trace_event::MemoryAllocatorDumpGuid GetBufferGUIDForTracing(
    uint64_t tracing_process_id,
    int32_t buffer_id);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_BUFFER_H_
