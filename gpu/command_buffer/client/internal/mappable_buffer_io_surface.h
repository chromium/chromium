// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_INTERNAL_MAPPABLE_BUFFER_IO_SURFACE_H_
#define GPU_COMMAND_BUFFER_CLIENT_INTERNAL_MAPPABLE_BUFFER_IO_SURFACE_H_

#include <IOSurface/IOSurfaceRef.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/apple/scoped_cftyperef.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/gpu_command_buffer_client_export.h"
#include "gpu/command_buffer/client/internal/mappable_buffer.h"
#include "ui/gfx/color_space.h"

namespace gpu {

class ClientSharedImage;

// Implementation of MappableBuffer based on IO surfaces.
class GPU_COMMAND_BUFFER_CLIENT_EXPORT MappableBufferIOSurface
    : public MappableBuffer {
 public:
  MappableBufferIOSurface(const MappableBufferIOSurface&) = delete;
  MappableBufferIOSurface& operator=(const MappableBufferIOSurface&) = delete;

  ~MappableBufferIOSurface() override;

  static constexpr gfx::GpuMemoryBufferType kBufferType =
      gfx::IO_SURFACE_BUFFER;

  static std::unique_ptr<MappableBufferIOSurface> CreateFromHandleForTesting(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      viz::SharedImageFormat format,
      gfx::BufferUsage usage);

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

 private:
  friend ClientSharedImage;

  static std::unique_ptr<MappableBufferIOSurface> CreateFromHandle(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      viz::SharedImageFormat format,
      bool is_read_only_cpu_usage);

  static std::unique_ptr<MappableBufferIOSurface> CreateFromHandleImpl(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      viz::SharedImageFormat format,
      int32_t lock_flags);

  MappableBufferIOSurface(const gfx::Size& size,
                          viz::SharedImageFormat format,
                          gfx::GpuMemoryBufferHandle handle,
                          uint32_t lock_flags);

  void AssertMapped();

  const gfx::Size size_;
  const viz::SharedImageFormat format_;
  gfx::GpuMemoryBufferHandle handle_;
  [[maybe_unused]] const uint32_t lock_flags_;

  // Note: This lock must be held throughout the entirety of the Map() and
  // Unmap() operations to avoid corrupt mutation across multiple threads.
  base::Lock map_lock_;
  uint32_t map_count_ GUARDED_BY(map_lock_) = 0u;

#if BUILDFLAG(IS_IOS)
  // On iOS, we can't use IOKit to access IOSurfaces in the renderer process, so
  // we share the memory segment backing the IOSurface as shared memory which is
  // then mapped in the renderer process.
  base::WritableSharedMemoryMapping shared_memory_mapping_;
#endif
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_INTERNAL_MAPPABLE_BUFFER_IO_SURFACE_H_
