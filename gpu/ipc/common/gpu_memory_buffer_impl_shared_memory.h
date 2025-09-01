// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_MEMORY_BUFFER_IMPL_SHARED_MEMORY_H_
#define GPU_IPC_COMMON_GPU_MEMORY_BUFFER_IMPL_SHARED_MEMORY_H_

#include <stddef.h>

#include <memory>

#include "gpu/ipc/common/gpu_ipc_common_export.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl.h"

namespace gpu {

class ClientSharedImage;

// Implementation of GPU memory buffer based on shared memory.
class GPU_IPC_COMMON_EXPORT GpuMemoryBufferImplSharedMemory
    : public GpuMemoryBufferImpl {
 public:
  GpuMemoryBufferImplSharedMemory(const GpuMemoryBufferImplSharedMemory&) =
      delete;
  GpuMemoryBufferImplSharedMemory& operator=(
      const GpuMemoryBufferImplSharedMemory&) = delete;

  ~GpuMemoryBufferImplSharedMemory() override;

  static constexpr gfx::GpuMemoryBufferType kBufferType =
      gfx::SHARED_MEMORY_BUFFER;

  static std::unique_ptr<GpuMemoryBufferImplSharedMemory>
  CreateFromHandleForTesting(gfx::GpuMemoryBufferHandle handle,
                             const gfx::Size& size,
                             gfx::BufferFormat format,
                             gfx::BufferUsage usage) {
    return CreateFromHandle(std::move(handle), size, format, usage);
  }

  static base::OnceClosure AllocateForTesting(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gfx::GpuMemoryBufferHandle* handle);

  // Overridden from GpuMemoryBufferImpl:
  bool Map() override;
  void MapAsync(base::OnceCallback<void(bool)> callback) override;
  bool AsyncMappingIsNonBlocking() const override;
  void* memory(size_t plane) override;
  void Unmap() override;
  int stride(size_t plane) const override;
  gfx::GpuMemoryBufferType GetType() const override;
  gfx::GpuMemoryBufferHandle CloneHandle() const override;
#if BUILDFLAG(IS_WIN)
  void SetUsePreMappedMemory(bool use_premapped_memory) override {}
#endif

 private:
  friend class ClientSharedImage;

  static std::unique_ptr<GpuMemoryBufferImplSharedMemory> CreateFromHandle(
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage);

  GpuMemoryBufferImplSharedMemory(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      base::UnsafeSharedMemoryRegion shared_memory_region,
      base::WritableSharedMemoryMapping shared_memory_mapping,
      size_t offset,
      uint32_t stride);

  void AssertMapped();

  const gfx::Size size_;
  const gfx::BufferFormat format_;
  base::UnsafeSharedMemoryRegion shared_memory_region_;
  base::WritableSharedMemoryMapping shared_memory_mapping_;
  size_t offset_;
  uint32_t stride_;

  // Note: This lock must be held throughout the entirety of the Map() and
  // Unmap() operations to avoid corrupt mutation across multiple threads.
  base::Lock map_lock_;
  uint32_t map_count_ GUARDED_BY(map_lock_) = 0u;
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_GPU_MEMORY_BUFFER_IMPL_SHARED_MEMORY_H_
