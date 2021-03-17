// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_CLIENT_CLIENT_SHARED_IMAGE_INTERFACE_H_
#define GPU_IPC_CLIENT_CLIENT_SHARED_IMAGE_INTERFACE_H_

#include "gpu/command_buffer/client/shared_image_interface.h"

#include "base/containers/flat_set.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "gpu/ipc/common/surface_handle.h"

namespace gpu {
class SharedImageInterfaceProxy;

// Tracks shared images created by a single context and ensures they are deleted
// if the context is lost.
class GPU_EXPORT ClientSharedImageInterface : public SharedImageInterface {
 public:
  ClientSharedImageInterface(SharedImageInterfaceProxy* proxy);
  ~ClientSharedImageInterface() override;

  // SharedImageInterface implementation.
  void UpdateSharedImage(const SyncToken& sync_token,
                         const Mailbox& mailbox) override;
  void UpdateSharedImage(const SyncToken& sync_token,
                         std::unique_ptr<gfx::GpuFence> acquire_fence,
                         const Mailbox& mailbox) override;
  void PresentSwapChain(const SyncToken& sync_token,
                        const Mailbox& mailbox) override;
#if defined(OS_FUCHSIA)
  void RegisterSysmemBufferCollection(gfx::SysmemBufferCollectionId id,
                                      zx::channel token,
                                      gfx::BufferFormat format,
                                      gfx::BufferUsage usage,
                                      bool register_with_image_pipe) override;
  void ReleaseSysmemBufferCollection(gfx::SysmemBufferCollectionId id) override;
#endif  // defined(OS_FUCHSIA)
  SyncToken GenUnverifiedSyncToken() override;
  SyncToken GenVerifiedSyncToken() override;
  void WaitSyncToken(const gpu::SyncToken& sync_token) override;
  void Flush() override;
  scoped_refptr<gfx::NativePixmap> GetNativePixmap(
      const Mailbox& mailbox) override;
  Mailbox CreateSharedImage(viz::ResourceFormat format,
                            const gfx::Size& size,
                            const gfx::ColorSpace& color_space,
                            GrSurfaceOrigin surface_origin,
                            SkAlphaType alpha_type,
                            uint32_t usage,
                            gpu::SurfaceHandle surface_handle) override;
  Mailbox CreateSharedImage(viz::ResourceFormat format,
                            const gfx::Size& size,
                            const gfx::ColorSpace& color_space,
                            GrSurfaceOrigin surface_origin,
                            SkAlphaType alpha_type,
                            uint32_t usage,
                            base::span<const uint8_t> pixel_data) override;
  Mailbox CreateSharedImage(gfx::GpuMemoryBuffer* gpu_memory_buffer,
                            GpuMemoryBufferManager* gpu_memory_buffer_manager,
                            const gfx::ColorSpace& color_space,
                            GrSurfaceOrigin surface_origin,
                            SkAlphaType alpha_type,
                            uint32_t usage) override;
#if defined(OS_ANDROID)
  Mailbox CreateSharedImageWithAHB(const Mailbox& mailbox,
                                   uint32_t usage,
                                   const SyncToken& sync_token) override;
#endif
  SwapChainMailboxes CreateSwapChain(viz::ResourceFormat format,
                                     const gfx::Size& size,
                                     const gfx::ColorSpace& color_space,
                                     GrSurfaceOrigin surface_origin,
                                     SkAlphaType alpha_type,
                                     uint32_t usage) override;
  void DestroySharedImage(const SyncToken& sync_token,
                          const Mailbox& mailbox) override;
  uint32_t UsageForMailbox(const Mailbox& mailbox) override;
  void NotifyMailboxAdded(const Mailbox& mailbox, uint32_t usage) override;

 private:
  Mailbox AddMailbox(const Mailbox& mailbox);

  SharedImageInterfaceProxy* const proxy_;

  base::Lock lock_;
  base::flat_set<Mailbox> mailboxes_ GUARDED_BY(lock_);
};

}  // namespace gpu

#endif  // GPU_IPC_CLIENT_CLIENT_SHARED_IMAGE_INTERFACE_H_
