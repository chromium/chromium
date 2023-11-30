// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/shared_image_interface.h"

#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"

#if BUILDFLAG(IS_WIN)
#include "ui/gfx/win/d3d_shared_fence.h"
#endif

namespace gpu {

scoped_refptr<ClientSharedImage> SharedImageInterface::CreateSharedImage(
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::StringPiece debug_label,
    gpu::SurfaceHandle surface_handle,
    gfx::BufferUsage buffer_usage) {
  NOTREACHED();
  return base::MakeRefCounted<ClientSharedImage>(Mailbox());
}

uint32_t SharedImageInterface::UsageForMailbox(const Mailbox& mailbox) {
  return 0u;
}

void SharedImageInterface::NotifyMailboxAdded(const Mailbox& /*mailbox*/,
                                              uint32_t /*usage*/) {}

void SharedImageInterface::CopyToGpuMemoryBuffer(const SyncToken& sync_token,
                                                 const Mailbox& mailbox) {
  NOTREACHED();
}

// static
std::unique_ptr<gfx::GpuMemoryBuffer>
SharedImageInterface::CreateGpuMemoryBufferForUseByScopedMapping(
    GpuMemoryBufferHandleInfo handle_info) {
  GpuMemoryBufferSupport support;

  // Only single planar buffer formats are supported currently. Multiplanar will
  // be supported when Multiplanar SharedImages are fully implemented.
  CHECK(handle_info.format.is_single_plane());
  auto buffer = support.CreateGpuMemoryBufferImplFromHandle(
      std::move(handle_info.handle), handle_info.size,
      viz::SinglePlaneSharedImageFormatToBufferFormat(handle_info.format),
      handle_info.buffer_usage, base::DoNothing());
  if (!buffer) {
    LOG(ERROR) << "Unable to create GpuMemoryBuffer.";
  }

  return std::move(buffer);
}

#if BUILDFLAG(IS_WIN)
void SharedImageInterface::UpdateSharedImage(
    const SyncToken& sync_token,
    scoped_refptr<gfx::D3DSharedFence> d3d_shared_fence,
    const Mailbox& mailbox) {
  NOTIMPLEMENTED_LOG_ONCE();
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace gpu
