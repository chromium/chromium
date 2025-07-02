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

namespace arc {
class GpuArcVideoEncodeAccelerator;
}

namespace gfx {
class ClientNativePixmap;
class ClientNativePixmapFactory;
}  // namespace gfx

namespace media {
class V4L2JpegEncodeAccelerator;
class VaapiJpegEncodeAccelerator;
}  // namespace media

namespace gpu {

class GpuMemoryBufferSupport;

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

  // Creates a GpuMemoryBufferImpl from the given |handle| for VideoFrames.
  // |size| and |format| should match what was used to allocate the |handle|.
  // NOTE: DO NOT ADD ANY USAGES OF THIS METHOD.
  // TODO(crbug.com/40263579): Remove this method once all usages are
  // eliminated.
  static std::unique_ptr<GpuMemoryBufferImplNativePixmap>
  CreateFromHandleForVideoFrame(
      gfx::ClientNativePixmapFactory* client_native_pixmap_factory,
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) {
    return CreateFromHandle(client_native_pixmap_factory, std::move(handle),
                            size, format, usage, base::NullCallback());
  }

 private:
  // TODO(crbug.com/404905709): Eliminate these class' creation of GMBs and
  // remove this friending.
  friend class arc::GpuArcVideoEncodeAccelerator;
  friend class media::V4L2JpegEncodeAccelerator;
  friend class media::VaapiJpegEncodeAccelerator;
  friend class GpuMemoryBufferSupport;

  static std::unique_ptr<GpuMemoryBufferImplNativePixmap> CreateFromHandle(
      gfx::ClientNativePixmapFactory* client_native_pixmap_factory,
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      DestructionCallback callback);

  GpuMemoryBufferImplNativePixmap(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      DestructionCallback callback,
      std::unique_ptr<gfx::ClientNativePixmap> native_pixmap);

  const std::unique_ptr<gfx::ClientNativePixmap> pixmap_;
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_GPU_MEMORY_BUFFER_IMPL_NATIVE_PIXMAP_H_
