// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_INTERNAL_MAPPABLE_BUFFER_SHARED_MEMORY_H_
#define GPU_COMMAND_BUFFER_CLIENT_INTERNAL_MAPPABLE_BUFFER_SHARED_MEMORY_H_

#include <stddef.h>

#include <memory>

#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/gpu_command_buffer_client_export.h"
#include "gpu/command_buffer/client/internal/mappable_buffer.h"

namespace gpu {

class ClientSharedImage;

// Implementation of MappableBuffer based on shared memory.
class GPU_COMMAND_BUFFER_CLIENT_EXPORT MappableBufferSharedMemory
    : public MappableBuffer {
 public:
  MappableBufferSharedMemory(const MappableBufferSharedMemory&) = delete;
  MappableBufferSharedMemory& operator=(const MappableBufferSharedMemory&) =
      delete;

  ~MappableBufferSharedMemory() override;

  static constexpr gfx::GpuMemoryBufferType kBufferType =
      gfx::SHARED_MEMORY_BUFFER;

  static std::unique_ptr<MappableBufferSharedMemory> CreateFromHandleForTesting(
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      viz::SharedImageFormat format,
      gfx::BufferUsage usage) {
    return CreateFromHandle(std::move(handle), size, format);
  }

  static base::OnceClosure AllocateForTesting(
      const gfx::Size& size,
      viz::SharedImageFormat format,
      gfx::BufferUsage usage,
      gfx::GpuMemoryBufferHandle* handle);

  // Overridden from MappableBuffer:
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

  static std::unique_ptr<MappableBufferSharedMemory> CreateFromHandle(
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      viz::SharedImageFormat format);

  static std::unique_ptr<MappableBufferSharedMemory> CreateFromMapping(
      base::WritableSharedMemoryMapping shared_memory_mapping,
      const gfx::Size& size,
      viz::SharedImageFormat format);

  MappableBufferSharedMemory(
      const gfx::Size& size,
      viz::SharedImageFormat format,
      base::UnsafeSharedMemoryRegion shared_memory_region,
      base::WritableSharedMemoryMapping shared_memory_mapping,
      size_t offset,
      uint32_t stride);

  void AssertMapped();

  const gfx::Size size_;
  const viz::SharedImageFormat format_;
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

#endif  // GPU_COMMAND_BUFFER_CLIENT_INTERNAL_MAPPABLE_BUFFER_SHARED_MEMORY_H_
