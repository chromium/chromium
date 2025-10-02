// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_LEGACY_GPU_MEMORY_BUFFER_FOR_VIDEO_H_
#define GPU_IPC_COMMON_LEGACY_GPU_MEMORY_BUFFER_FOR_VIDEO_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/synchronization/lock.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/ipc/common/gpu_ipc_common_export.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

namespace gfx {
class ClientNativePixmap;
class ClientNativePixmapFactory;
}  // namespace gfx

namespace gpu {

// Implementation of GPU memory buffer based on Ozone native pixmap for use by
// media::VideoFrame until its remaining use case is transitioned to
// MappableSharedImage.
// TODO(crbug.com/40263579): Eliminate this class.
class GPU_IPC_COMMON_EXPORT LegacyGpuMemoryBufferForVideo {
 public:
  LegacyGpuMemoryBufferForVideo(const LegacyGpuMemoryBufferForVideo&) = delete;
  LegacyGpuMemoryBufferForVideo& operator=(
      const LegacyGpuMemoryBufferForVideo&) = delete;

  ~LegacyGpuMemoryBufferForVideo();

  static constexpr gfx::GpuMemoryBufferType kBufferType = gfx::NATIVE_PIXMAP;

  bool Map();
  void* memory(size_t plane);
  base::span<uint8_t> memory_span(size_t plane);
  void Unmap();
  int stride(size_t plane) const;
  gfx::GpuMemoryBufferType GetType() const;
  gfx::GpuMemoryBufferHandle CloneHandle() const;
  gfx::Size GetSize() const { return size_; }
  viz::SharedImageFormat GetFormat() const { return format_; }

  // Creates a GpuMemoryBufferImpl from the given |handle| for VideoFrames.
  // |size| and |format| should match what was used to allocate the |handle|.
  // NOTE: DO NOT ADD ANY USAGES OF THIS METHOD.
  static std::unique_ptr<LegacyGpuMemoryBufferForVideo>
  CreateFromHandleForVideoFrame(
      gfx::ClientNativePixmapFactory* client_native_pixmap_factory,
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      viz::SharedImageFormat format,
      gfx::BufferUsage usage);

 private:
  LegacyGpuMemoryBufferForVideo(
      const gfx::Size& size,
      viz::SharedImageFormat format,
      std::unique_ptr<gfx::ClientNativePixmap> native_pixmap);

  const gfx::Size size_;
  const viz::SharedImageFormat format_;

  // Note: This lock must be held throughout the entirety of the Map() and
  // Unmap() operations to avoid corrupt mutation across multiple threads.
  base::Lock map_lock_;
  uint32_t map_count_ GUARDED_BY(map_lock_) = 0u;
  const std::unique_ptr<gfx::ClientNativePixmap> pixmap_;
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_LEGACY_GPU_MEMORY_BUFFER_FOR_VIDEO_H_
