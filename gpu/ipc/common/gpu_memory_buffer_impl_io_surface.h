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
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl.h"
#include "ui/gfx/color_space.h"

namespace gpu {

// Implementation of GPU memory buffer based on IO surfaces.
class GPU_EXPORT GpuMemoryBufferImplIOSurface : public GpuMemoryBufferImpl {
 public:
  GpuMemoryBufferImplIOSurface(const GpuMemoryBufferImplIOSurface&) = delete;
  GpuMemoryBufferImplIOSurface& operator=(const GpuMemoryBufferImplIOSurface&) =
      delete;

  ~GpuMemoryBufferImplIOSurface() override;

  static constexpr gfx::GpuMemoryBufferType kBufferType =
      gfx::IO_SURFACE_BUFFER;

  static std::unique_ptr<GpuMemoryBufferImplIOSurface> CreateFromHandle(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      DestructionCallback callback);

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
  void SetColorSpace(const gfx::ColorSpace& color_space) override;
  gfx::GpuMemoryBufferType GetType() const override;
  gfx::GpuMemoryBufferHandle CloneHandle() const override;

 private:
  GpuMemoryBufferImplIOSurface(gfx::GpuMemoryBufferId id,
                               const gfx::Size& size,
                               gfx::BufferFormat format,
                               DestructionCallback callback,
                               IOSurfaceRef io_surface,
                               uint32_t lock_flags);

  base::apple::ScopedCFTypeRef<IOSurfaceRef> io_surface_;
  uint32_t lock_flags_;
  // Cache the color space, because re-assigning the same value can be
  // expensive.
  gfx::ColorSpace color_space_;
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_GPU_MEMORY_BUFFER_IMPL_IO_SURFACE_H_
