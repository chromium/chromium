// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_CLIENT_SHARED_IMAGE_INTERFACE_PROXY_H_
#define GPU_IPC_CLIENT_SHARED_IMAGE_INTERFACE_PROXY_H_

#include "base/memory/read_only_shared_memory_region.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/buffer.h"

namespace gpu {
class GpuChannelHost;

// Implementation of SharedImageInterface that sends commands over GPU channel
// IPCs.
class SharedImageInterfaceProxy : public SharedImageInterface {
 public:
  explicit SharedImageInterfaceProxy(GpuChannelHost* host, int32_t route_id);
  ~SharedImageInterfaceProxy() override;
  Mailbox CreateSharedImage(viz::ResourceFormat format,
                            const gfx::Size& size,
                            const gfx::ColorSpace& color_space,
                            uint32_t usage) override;
  Mailbox CreateSharedImage(viz::ResourceFormat format,
                            const gfx::Size& size,
                            const gfx::ColorSpace& color_space,
                            uint32_t usage,
                            base::span<const uint8_t> pixel_data) override;
  Mailbox CreateSharedImage(gfx::GpuMemoryBuffer* gpu_memory_buffer,
                            GpuMemoryBufferManager* gpu_memory_buffer_manager,
                            const gfx::ColorSpace& color_space,
                            uint32_t usage) override;
  void UpdateSharedImage(const SyncToken& sync_token,
                         const Mailbox& mailbox) override;
  void UpdateSharedImage(const SyncToken& sync_token,
                         std::unique_ptr<gfx::GpuFence> acquire_fence,
                         const Mailbox& mailbox) override;

  void DestroySharedImage(const SyncToken& sync_token,
                          const Mailbox& mailbox) override;
  SyncToken GenVerifiedSyncToken() override;
  SyncToken GenUnverifiedSyncToken() override;
  void Flush() override;

  SwapChainMailboxes CreateSwapChain(viz::ResourceFormat format,
                                     const gfx::Size& size,
                                     const gfx::ColorSpace& color_space,
                                     uint32_t usage) override;
  void PresentSwapChain(const SyncToken& sync_token,
                        const Mailbox& mailbox) override;

#if defined(OS_FUCHSIA)
  void RegisterSysmemBufferCollection(gfx::SysmemBufferCollectionId id,
                                      zx::channel token) override;
  void ReleaseSysmemBufferCollection(gfx::SysmemBufferCollectionId id) override;
#endif  // defined(OS_FUCHSIA)

 private:
  bool GetSHMForPixelData(base::span<const uint8_t> pixel_data,
                          size_t* shm_offset,
                          bool* done_with_shm) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  GpuChannelHost* const host_;
  const int32_t route_id_;
  base::Lock lock_;
  uint32_t next_release_id_ GUARDED_BY(lock_) = 0;
  uint32_t last_flush_id_ GUARDED_BY(lock_) = 0;

  // A buffer used to upload initial data during SharedImage creation.
  base::MappedReadOnlyRegion upload_buffer_ GUARDED_BY(lock_);
  // The offset into |upload_buffer_| at which data is no longer used.
  size_t upload_buffer_offset_ GUARDED_BY(lock_) = 0;
};

}  // namespace gpu

#endif  // GPU_IPC_CLIENT_SHARED_IMAGE_INTERFACE_PROXY_H_
