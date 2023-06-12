// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_CLIENT_CLIENT_SHARED_IMAGE_INTERFACE_H_
#define GPU_IPC_CLIENT_CLIENT_SHARED_IMAGE_INTERFACE_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
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
#if BUILDFLAG(IS_FUCHSIA)
  void RegisterSysmemBufferCollection(zx::eventpair service_handle,
                                      zx::channel sysmem_token,
                                      gfx::BufferFormat format,
                                      gfx::BufferUsage usage,
                                      bool register_with_image_pipe) override;
#endif  // BUILDFLAG(IS_FUCHSIA)
  SyncToken GenUnverifiedSyncToken() override;
  SyncToken GenVerifiedSyncToken() override;
  void WaitSyncToken(const gpu::SyncToken& sync_token) override;
  void Flush() override;
  scoped_refptr<gfx::NativePixmap> GetNativePixmap(
      const Mailbox& mailbox) override;
  Mailbox CreateSharedImage(viz::SharedImageFormat format,
                            const gfx::Size& size,
                            const gfx::ColorSpace& color_space,
                            GrSurfaceOrigin surface_origin,
                            SkAlphaType alpha_type,
                            uint32_t usage,
                            base::StringPiece debug_label,
                            gpu::SurfaceHandle surface_handle) override;
  Mailbox CreateSharedImage(viz::SharedImageFormat format,
                            const gfx::Size& size,
                            const gfx::ColorSpace& color_space,
                            GrSurfaceOrigin surface_origin,
                            SkAlphaType alpha_type,
                            uint32_t usage,
                            base::StringPiece debug_label,
                            base::span<const uint8_t> pixel_data) override;
  Mailbox CreateSharedImage(viz::SharedImageFormat format,
                            const gfx::Size& size,
                            const gfx::ColorSpace& color_space,
                            GrSurfaceOrigin surface_origin,
                            SkAlphaType alpha_type,
                            uint32_t usage,
                            base::StringPiece debug_label,
                            gfx::GpuMemoryBufferHandle buffer_handle) override;
  // NOTE: The below method is DEPRECATED for `gpu_memory_buffer` only with
  // single planar eg. RGB BufferFormats. Please use the equivalent method above
  // taking in single planar SharedImageFormat with GpuMemoryBufferHandle.
  Mailbox CreateSharedImage(gfx::GpuMemoryBuffer* gpu_memory_buffer,
                            GpuMemoryBufferManager* gpu_memory_buffer_manager,
                            gfx::BufferPlane plane,
                            const gfx::ColorSpace& color_space,
                            GrSurfaceOrigin surface_origin,
                            SkAlphaType alpha_type,
                            uint32_t usage,
                            base::StringPiece debug_label) override;
#if BUILDFLAG(IS_WIN)
  void CopyToGpuMemoryBuffer(const SyncToken& sync_token,
                             const Mailbox& mailbox) override;
#endif
  SwapChainMailboxes CreateSwapChain(viz::SharedImageFormat format,
                                     const gfx::Size& size,
                                     const gfx::ColorSpace& color_space,
                                     GrSurfaceOrigin surface_origin,
                                     SkAlphaType alpha_type,
                                     uint32_t usage) override;
  void DestroySharedImage(const SyncToken& sync_token,
                          const Mailbox& mailbox) override;
  uint32_t UsageForMailbox(const Mailbox& mailbox) override;
  void NotifyMailboxAdded(const Mailbox& mailbox, uint32_t usage) override;

  void AddReferenceToSharedImage(const SyncToken& sync_token,
                                 const Mailbox& mailbox,
                                 uint32_t usage) override;

 private:
  Mailbox AddMailbox(const Mailbox& mailbox);

  const raw_ptr<SharedImageInterfaceProxy> proxy_;

  base::Lock lock_;
  std::multiset<Mailbox> mailboxes_ GUARDED_BY(lock_);
};

}  // namespace gpu

#endif  // GPU_IPC_CLIENT_CLIENT_SHARED_IMAGE_INTERFACE_H_
