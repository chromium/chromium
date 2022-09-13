// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_MEMORY_BUFFER_IMPL_SHARED_MEMORY_H_
#define GPU_IPC_COMMON_GPU_MEMORY_BUFFER_IMPL_SHARED_MEMORY_H_

#include <stddef.h>

#include <memory>

#include "gpu/gpu_export.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl.h"

namespace gpu {

// Implementation of GPU memory buffer based on shared memory.
class GPU_EXPORT GpuMemoryBufferImplSharedMemory : public GpuMemoryBufferImpl {
 public:
  GpuMemoryBufferImplSharedMemory(const GpuMemoryBufferImplSharedMemory&) =
      delete;
  GpuMemoryBufferImplSharedMemory& operator=(
      const GpuMemoryBufferImplSharedMemory&) = delete;

  ~GpuMemoryBufferImplSharedMemory() override;

  static constexpr gfx::GpuMemoryBufferType kBufferType =
      gfx::SHARED_MEMORY_BUFFER;

  static std::unique_ptr<GpuMemoryBufferImplSharedMemory> Create(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      DestructionCallback callback);

  static gfx::GpuMemoryBufferHandle CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage);

  static std::unique_ptr<GpuMemoryBufferImplSharedMemory> CreateFromHandle(
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      DestructionCallback callback);

  static bool IsUsageSupported(gfx::BufferUsage usage);
  static bool IsConfigurationSupported(gfx::BufferFormat format,
                                       gfx::BufferUsage usage);
  static bool IsSizeValidForFormat(const gfx::Size& size,
                                   gfx::BufferFormat format);

  static base::OnceClosure AllocateForTesting(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gfx::GpuMemoryBufferHandle* handle);

  // Overridden from gfx::GpuMemoryBuffer:
  bool Map() override;
  void* memory(size_t plane) override;
  void Unmap() override;
  int stride(size_t plane) const override;
  gfx::GpuMemoryBufferType GetType() const override;
  gfx::GpuMemoryBufferHandle CloneHandle() const override;
  void OnMemoryDump(
      base::trace_event::ProcessMemoryDump* pmd,
      const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
      uint64_t tracing_process_id,
      int importance) const override;

  // Returns the shared memory GUID associated with buffer.
  base::UnguessableToken GetSharedMemoryGUID() const;

 private:
  GpuMemoryBufferImplSharedMemory(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      DestructionCallback callback,
      base::UnsafeSharedMemoryRegion shared_memory_region,
      base::WritableSharedMemoryMapping shared_memory_mapping,
      size_t offset,
      uint32_t stride);

  base::UnsafeSharedMemoryRegion shared_memory_region_;
  base::WritableSharedMemoryMapping shared_memory_mapping_;
  size_t offset_;
  uint32_t stride_;
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_GPU_MEMORY_BUFFER_IMPL_SHARED_MEMORY_H_
