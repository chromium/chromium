// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_LEGACY_GPU_MEMORY_BUFFER_FOR_VIDEO_H_
#define GPU_IPC_COMMON_LEGACY_GPU_MEMORY_BUFFER_FOR_VIDEO_H_

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

namespace gpu {

class ClientSharedImage;

// Implementation of GPU memory buffer based on Ozone native pixmap for use by
// media::VideoFrame until its remaining use case is transitioned to
// MappableSharedImage.
// TODO(crbug.com/40263579): Eliminate this class.
class GPU_IPC_COMMON_EXPORT LegacyGpuMemoryBufferForVideo
    : public GpuMemoryBufferImpl {
 public:
  LegacyGpuMemoryBufferForVideo(const LegacyGpuMemoryBufferForVideo&) = delete;
  LegacyGpuMemoryBufferForVideo& operator=(
      const LegacyGpuMemoryBufferForVideo&) = delete;

  ~LegacyGpuMemoryBufferForVideo() override;

  static constexpr gfx::GpuMemoryBufferType kBufferType = gfx::NATIVE_PIXMAP;

  static std::unique_ptr<LegacyGpuMemoryBufferForVideo>
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
  static std::unique_ptr<LegacyGpuMemoryBufferForVideo>
  CreateFromHandleForVideoFrame(
      gfx::ClientNativePixmapFactory* client_native_pixmap_factory,
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) {
    return CreateFromHandle(client_native_pixmap_factory, std::move(handle),
                            size, format, usage);
  }

 private:
  // TODO(crbug.com/404905709): Eliminate this class' creation of GMBs and
  // remove this friending.
  friend class arc::GpuArcVideoEncodeAccelerator;
  friend class ClientSharedImage;

  static std::unique_ptr<LegacyGpuMemoryBufferForVideo> CreateFromHandle(
      gfx::ClientNativePixmapFactory* client_native_pixmap_factory,
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage);

  LegacyGpuMemoryBufferForVideo(
      const gfx::Size& size,
      gfx::BufferFormat format,
      std::unique_ptr<gfx::ClientNativePixmap> native_pixmap);

  const std::unique_ptr<gfx::ClientNativePixmap> pixmap_;
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_LEGACY_GPU_MEMORY_BUFFER_FOR_VIDEO_H_
