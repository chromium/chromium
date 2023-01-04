// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_CLIENT_SHARED_IMAGE_INTERFACE_PROXY_H_
#define GPU_IPC_CLIENT_SHARED_IMAGE_INTERFACE_PROXY_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/buffer.h"

namespace viz {
class SharedImageFormat;
}

namespace gpu {
class GpuChannelHost;

// Proxy that sends commands over GPU channel IPCs for managing shared images.
class SharedImageInterfaceProxy {
 public:
  explicit SharedImageInterfaceProxy(GpuChannelHost* host, int32_t route_id);
  ~SharedImageInterfaceProxy();
  Mailbox CreateSharedImage(viz::SharedImageFormat format,
                            const gfx::Size& size,
                            const gfx::ColorSpace& color_space,
                            GrSurfaceOrigin surface_origin,
                            SkAlphaType alpha_type,
                            uint32_t usage);
  Mailbox CreateSharedImage(viz::SharedImageFormat format,
                            const gfx::Size& size,
                            const gfx::ColorSpace& color_space,
                            GrSurfaceOrigin surface_origin,
                            SkAlphaType alpha_type,
                            uint32_t usage,
                            base::span<const uint8_t> pixel_data);
  Mailbox CreateSharedImage(viz::SharedImageFormat format,
                            const gfx::Size& size,
                            const gfx::ColorSpace& color_space,
                            GrSurfaceOrigin surface_origin,
                            SkAlphaType alpha_type,
                            uint32_t usage,
                            gfx::GpuMemoryBufferHandle handle);
  Mailbox CreateSharedImage(gfx::BufferFormat format,
                            gfx::BufferPlane plane,
                            const gfx::Size& size,
                            const gfx::ColorSpace& color_space,
                            GrSurfaceOrigin surface_origin,
                            SkAlphaType alpha_type,
                            uint32_t usage,
                            gfx::GpuMemoryBufferHandle buffer_handle);

#if BUILDFLAG(IS_WIN)
  void CopyToGpuMemoryBuffer(const SyncToken& sync_token,
                             const Mailbox& mailbox);
#endif  // BUILDFLAG(IS_WIN)

  void UpdateSharedImage(const SyncToken& sync_token, const Mailbox& mailbox);
  void UpdateSharedImage(const SyncToken& sync_token,
                         std::unique_ptr<gfx::GpuFence> acquire_fence,
                         const Mailbox& mailbox);

  void DestroySharedImage(const SyncToken& sync_token, const Mailbox& mailbox);
  SyncToken GenVerifiedSyncToken();
  SyncToken GenUnverifiedSyncToken();
  void WaitSyncToken(const SyncToken& sync_token);
  void Flush();

  SharedImageInterface::SwapChainMailboxes CreateSwapChain(
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage);
  void PresentSwapChain(const SyncToken& sync_token, const Mailbox& mailbox);

#if BUILDFLAG(IS_FUCHSIA)
  void RegisterSysmemBufferCollection(zx::eventpair service_handle,
                                      zx::channel sysmem_token,
                                      gfx::BufferFormat format,
                                      gfx::BufferUsage usage,
                                      bool register_with_image_pipe);
#endif  // BUILDFLAG(IS_FUCHSIA)

  scoped_refptr<gfx::NativePixmap> GetNativePixmap(const gpu::Mailbox& mailbox);

  uint32_t UsageForMailbox(const Mailbox& mailbox);
  void NotifyMailboxAdded(const Mailbox& mailbox, uint32_t usage);

 private:
  bool GetSHMForPixelData(base::span<const uint8_t> pixel_data,
                          size_t* shm_offset,
                          bool* done_with_shm) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  void AddMailbox(const Mailbox& mailbox, uint32_t usage)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  const raw_ptr<GpuChannelHost> host_;
  const int32_t route_id_;
  base::Lock lock_;
  uint32_t next_release_id_ GUARDED_BY(lock_) = 0;
  uint32_t last_flush_id_ GUARDED_BY(lock_) = 0;

  // A buffer used to upload initial data during SharedImage creation.
  base::MappedReadOnlyRegion upload_buffer_ GUARDED_BY(lock_);
  // The offset into |upload_buffer_| at which data is no longer used.
  size_t upload_buffer_offset_ GUARDED_BY(lock_) = 0;

  base::flat_map<Mailbox, uint32_t> mailbox_to_usage_ GUARDED_BY(lock_);
};

}  // namespace gpu

#endif  // GPU_IPC_CLIENT_SHARED_IMAGE_INTERFACE_PROXY_H_
