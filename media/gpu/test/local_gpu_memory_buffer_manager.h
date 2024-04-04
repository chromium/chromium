// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_LOCAL_GPU_MEMORY_BUFFER_MANAGER_H_
#define MEDIA_GPU_TEST_LOCAL_GPU_MEMORY_BUFFER_MANAGER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/linux/scoped_gbm_device.h"

namespace gfx {
class GpuMemoryBuffer;
struct NativePixmapHandle;
class Size;
}  // namespace gfx

namespace media {

// A local, as opposed to the default IPC-based, implementation of
// gfx::GpuMemoryBufferManager which interacts with the DRM render node device
// directly. The LocalGpuMemoryBufferManager is only for testing purposes and
// should not be used in production.
class MEDIA_GPU_EXPORT LocalGpuMemoryBufferManager
    : public gpu::GpuMemoryBufferManager {
 public:
  LocalGpuMemoryBufferManager();

  LocalGpuMemoryBufferManager(const LocalGpuMemoryBufferManager&) = delete;
  LocalGpuMemoryBufferManager& operator=(const LocalGpuMemoryBufferManager&) =
      delete;

  // gpu::GpuMemoryBufferManager implementation
  ~LocalGpuMemoryBufferManager() override;
  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gpu::SurfaceHandle surface_handle,
      base::WaitableEvent* shutdown_event) override;
  void CopyGpuMemoryBufferAsync(
      gfx::GpuMemoryBufferHandle buffer_handle,
      base::UnsafeSharedMemoryRegion memory_region,
      base::OnceCallback<void(bool)> callback) override;
  bool CopyGpuMemoryBufferSync(
      gfx::GpuMemoryBufferHandle buffer_handle,
      base::UnsafeSharedMemoryRegion memory_region) override;

  // Imports a DmaBuf as a GpuMemoryBuffer to be able to map it. The
  // GBM_BO_USE_SW_READ_OFTEN usage is specified so that the user of the
  // returned GpuMemoryBuffer is guaranteed to have a linear view when mapping
  // it.
  std::unique_ptr<gfx::GpuMemoryBuffer> ImportDmaBuf(
      const gfx::NativePixmapHandle& handle,
      const gfx::Size& size,
      gfx::BufferFormat format);

  // Returns true if the combination of |format| and |usage| is supported by
  // CreateGpuMemoryBuffer().
  bool IsFormatAndUsageSupported(gfx::BufferFormat format,
                                 gfx::BufferUsage usage);

 private:
  ui::ScopedGbmDevice gbm_device_;
};

}  // namespace media

#endif  // MEDIA_GPU_TEST_LOCAL_GPU_MEMORY_BUFFER_MANAGER_H_
