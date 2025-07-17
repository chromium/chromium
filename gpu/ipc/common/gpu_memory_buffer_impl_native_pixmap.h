// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_MEMORY_BUFFER_IMPL_NATIVE_PIXMAP_H_
#define GPU_IPC_COMMON_GPU_MEMORY_BUFFER_IMPL_NATIVE_PIXMAP_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "gpu/ipc/common/gpu_ipc_common_export.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl.h"

namespace gfx {
class ClientNativePixmap;
class ClientNativePixmapFactory;
}  // namespace gfx

namespace gpu {

class ClientSharedImage;

// Implementation of GPU memory buffer based on Ozone native pixmap.
class GPU_IPC_COMMON_EXPORT GpuMemoryBufferImplNativePixmap
    : public GpuMemoryBufferImpl {
 public:
  GpuMemoryBufferImplNativePixmap(const GpuMemoryBufferImplNativePixmap&) =
      delete;
  GpuMemoryBufferImplNativePixmap& operator=(
      const GpuMemoryBufferImplNativePixmap&) = delete;

  ~GpuMemoryBufferImplNativePixmap() override;

  static constexpr gfx::GpuMemoryBufferType kBufferType = gfx::NATIVE_PIXMAP;

  static std::unique_ptr<GpuMemoryBufferImplNativePixmap>
  CreateFromHandleForTesting(
      gfx::ClientNativePixmapFactory* client_native_pixmap_factory,
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) {
    return CreateFromHandle(client_native_pixmap_factory, std::move(handle),
                            size, format, usage);
  }

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
  friend class ClientSharedImage;

  static std::unique_ptr<GpuMemoryBufferImplNativePixmap> CreateFromHandle(
      gfx::ClientNativePixmapFactory* client_native_pixmap_factory,
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage);

  GpuMemoryBufferImplNativePixmap(
      const gfx::Size& size,
      gfx::BufferFormat format,
      std::unique_ptr<gfx::ClientNativePixmap> native_pixmap);

  const std::unique_ptr<gfx::ClientNativePixmap> pixmap_;
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_GPU_MEMORY_BUFFER_IMPL_NATIVE_PIXMAP_H_
