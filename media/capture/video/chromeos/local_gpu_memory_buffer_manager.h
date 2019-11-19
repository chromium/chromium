// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_LOCAL_GPU_MEMORY_BUFFER_MANAGER_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_LOCAL_GPU_MEMORY_BUFFER_MANAGER_H_

#include <memory>

#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "media/capture/capture_export.h"
#include "ui/gfx/buffer_types.h"

struct gbm_device;

namespace gfx {
class GpuMemoryBuffer;
struct NativePixmapHandle;
class Size;
}  // namespace gfx

namespace media {

// A local, as opposed to the default IPC-based, implementation of
// gfx::GpuMemoryBufferManager which interacts with the DRM render node device
// directly.  The LocalGpuMemoryBufferManager is only for testing purposes and
// should not be used in production.
//
// TODO(crbug.com/974437): consider moving this to //media/gpu/test.
class CAPTURE_EXPORT LocalGpuMemoryBufferManager
    : public gpu::GpuMemoryBufferManager {
 public:
  LocalGpuMemoryBufferManager();

  // gpu::GpuMemoryBufferManager implementation
  ~LocalGpuMemoryBufferManager() override;
  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gpu::SurfaceHandle surface_handle) override;
  void SetDestructionSyncToken(gfx::GpuMemoryBuffer* buffer,
                               const gpu::SyncToken& sync_token) override;

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
  gbm_device* gbm_device_;

  DISALLOW_COPY_AND_ASSIGN(LocalGpuMemoryBufferManager);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_LOCAL_GPU_MEMORY_BUFFER_MANAGER_H_
