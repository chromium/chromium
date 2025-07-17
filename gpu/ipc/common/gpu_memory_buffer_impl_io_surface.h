// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_MEMORY_BUFFER_IMPL_IO_SURFACE_H_
#define GPU_IPC_COMMON_GPU_MEMORY_BUFFER_IMPL_IO_SURFACE_H_

#include <IOSurface/IOSurfaceRef.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/apple/scoped_cftyperef.h"
#include "gpu/ipc/common/gpu_ipc_common_export.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl.h"
#include "ui/gfx/color_space.h"

namespace gpu {

class ClientSharedImage;

// Implementation of GPU memory buffer based on IO surfaces.
class GPU_IPC_COMMON_EXPORT GpuMemoryBufferImplIOSurface
    : public GpuMemoryBufferImpl {
 public:
  GpuMemoryBufferImplIOSurface(const GpuMemoryBufferImplIOSurface&) = delete;
  GpuMemoryBufferImplIOSurface& operator=(const GpuMemoryBufferImplIOSurface&) =
      delete;

  ~GpuMemoryBufferImplIOSurface() override;

  static constexpr gfx::GpuMemoryBufferType kBufferType =
      gfx::IO_SURFACE_BUFFER;

  static std::unique_ptr<GpuMemoryBufferImplIOSurface>
  CreateFromHandleForTesting(const gfx::GpuMemoryBufferHandle& handle,
                             const gfx::Size& size,
                             gfx::BufferFormat format,
                             gfx::BufferUsage usage);

  static base::OnceClosure AllocateForTesting(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gfx::GpuMemoryBufferHandle* handle);

  // Overridden from GpuMemoryBufferImpl:
  bool Map() override;
  void* memory(size_t plane) override;
  void Unmap() override;
  int stride(size_t plane) const override;
  gfx::GpuMemoryBufferType GetType() const override;
  gfx::GpuMemoryBufferHandle CloneHandle() const override;

 private:
  friend ClientSharedImage;

  static std::unique_ptr<GpuMemoryBufferImplIOSurface> CreateFromHandle(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      bool is_read_only_cpu_usage);

  static std::unique_ptr<GpuMemoryBufferImplIOSurface> CreateFromHandleImpl(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      int32_t lock_flags);

  GpuMemoryBufferImplIOSurface(const gfx::Size& size,
                               gfx::BufferFormat format,
                               gfx::GpuMemoryBufferHandle handle,
                               uint32_t lock_flags);

  gfx::GpuMemoryBufferHandle handle_;
  [[maybe_unused]] const uint32_t lock_flags_;

#if BUILDFLAG(IS_IOS)
  // On iOS, we can't use IOKit to access IOSurfaces in the renderer process, so
  // we share the memory segment backing the IOSurface as shared memory which is
  // then mapped in the renderer process.
  base::WritableSharedMemoryMapping shared_memory_mapping_;
#endif
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_GPU_MEMORY_BUFFER_IMPL_IO_SURFACE_H_
