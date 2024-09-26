// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_CLIENT_SHARED_IMAGE_INTERFACE_PROXY_H_
#define GPU_IPC_CLIENT_SHARED_IMAGE_INTERFACE_PROXY_H_

#include <unordered_map>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/buffer.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/common/gpu_memory_buffer_handle_info.h"

#if BUILDFLAG(IS_WIN)
namespace gfx {
class D3DSharedFence;
}
#endif

namespace viz {
class SharedImageFormat;
}

namespace gpu {
class GpuChannelHost;

// Proxy that sends commands over GPU channel IPCs for managing shared images.
class SharedImageInterfaceProxy {
 public:
  struct SwapChainMailboxes {
    Mailbox front_buffer;
    Mailbox back_buffer;
  };

  explicit SharedImageInterfaceProxy(
      GpuChannelHost* host,
      int32_t route_id,
      const gpu::SharedImageCapabilities& capabilities);
  ~SharedImageInterfaceProxy();

  struct SharedImageRefData {
    SharedImageRefData();
    ~SharedImageRefData();

    SharedImageRefData(SharedImageRefData&&);
    SharedImageRefData& operator=(SharedImageRefData&&);

    SharedImageRefData(const SharedImageRefData&) = delete;
    SharedImageRefData& operator=(const SharedImageRefData&) = delete;

    int ref_count = 0;
    gpu::SharedImageUsageSet usage;
    std::vector<SyncToken> destruction_sync_tokens;
  };

  Mailbox CreateSharedImage(const SharedImageInfo& si_info);
  Mailbox CreateSharedImage(SharedImageInfo& si_info,
                            gfx::BufferUsage buffer_usage,
                            gfx::GpuMemoryBufferHandle* handle_to_populate);
  Mailbox CreateSharedImage(const SharedImageInfo& si_info,
                            base::span<const uint8_t> pixel_data);
  Mailbox CreateSharedImage(const SharedImageInfo& si_info,
                            gfx::GpuMemoryBufferHandle handle);

  void CopyToGpuMemoryBuffer(const SyncToken& sync_token,
                             const Mailbox& mailbox);

#if BUILDFLAG(IS_WIN)
  void CopyToGpuMemoryBufferAsync(const SyncToken& sync_token,
                                  const Mailbox& mailbox,
                                  base::OnceCallback<void(bool)> callback);
  void UpdateSharedImage(const SyncToken& sync_token,
                         scoped_refptr<gfx::D3DSharedFence> d3d_shared_fence,
                         const Mailbox& mailbox);
  void CopyNativeGmbToSharedMemorySync(
      gfx::GpuMemoryBufferHandle buffer_handle,
      base::UnsafeSharedMemoryRegion memory_region,
      bool* status);
  void CopyNativeGmbToSharedMemoryAsync(
      gfx::GpuMemoryBufferHandle buffer_handle,
      base::UnsafeSharedMemoryRegion memory_region,
      base::OnceCallback<void(bool)> callback);
#endif  // BUILDFLAG(IS_WIN)

  void UpdateSharedImage(const SyncToken& sync_token, const Mailbox& mailbox);
  void UpdateSharedImage(const SyncToken& sync_token,
                         std::unique_ptr<gfx::GpuFence> acquire_fence,
                         const Mailbox& mailbox);

  void DestroySharedImage(const SyncToken& sync_token, const Mailbox& mailbox);
  void AddReferenceToSharedImage(const SyncToken& sync_token,
                                 const Mailbox& mailbox,
                                 gpu::SharedImageUsageSet usage);

  SyncToken GenVerifiedSyncToken();
  SyncToken GenUnverifiedSyncToken();
  void VerifySyncToken(SyncToken& sync_token);
  void WaitSyncToken(const SyncToken& sync_token);
  void Flush();

  SwapChainMailboxes CreateSwapChain(viz::SharedImageFormat format,
                                     const gfx::Size& size,
                                     const gfx::ColorSpace& color_space,
                                     GrSurfaceOrigin surface_origin,
                                     SkAlphaType alpha_type,
                                     gpu::SharedImageUsageSet usage);
  void PresentSwapChain(const SyncToken& sync_token, const Mailbox& mailbox);

#if BUILDFLAG(IS_FUCHSIA)
  void RegisterSysmemBufferCollection(zx::eventpair service_handle,
                                      zx::channel sysmem_token,
                                      const viz::SharedImageFormat& format,
                                      gfx::BufferUsage usage,
                                      bool register_with_image_pipe);
#endif  // BUILDFLAG(IS_FUCHSIA)

  scoped_refptr<gfx::NativePixmap> GetNativePixmap(const gpu::Mailbox& mailbox);

  gpu::SharedImageUsageSet UsageForMailbox(const Mailbox& mailbox);
  void NotifyMailboxAdded(const Mailbox& mailbox,
                          gpu::SharedImageUsageSet usage);

  const gpu::SharedImageCapabilities& GetCapabilities() {
    return capabilities_;
  }

 private:
  bool GetSHMForPixelData(base::span<const uint8_t> pixel_data,
                          size_t* shm_offset,
                          bool* done_with_shm) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  void AddMailbox(const Mailbox& mailbox, SharedImageUsageSet usage)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Returns true if it's first time mailbox was added.
  [[nodiscard]] bool AddMailboxOrAddReference(const Mailbox& mailbox,
                                              SharedImageUsageSet usage)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  const raw_ptr<GpuChannelHost> host_;
  const int32_t route_id_;
  base::Lock lock_;
  uint64_t next_release_id_ GUARDED_BY(lock_) = 0;
  uint32_t last_flush_id_ GUARDED_BY(lock_) = 0;

  // A buffer used to upload initial data during SharedImage creation.
  base::MappedReadOnlyRegion upload_buffer_ GUARDED_BY(lock_);
  // The offset into |upload_buffer_| at which data is no longer used.
  size_t upload_buffer_offset_ GUARDED_BY(lock_) = 0;

  base::flat_map<Mailbox, SharedImageRefData> mailbox_infos_ GUARDED_BY(lock_);

  const gpu::SharedImageCapabilities capabilities_;

#if BUILDFLAG(IS_WIN)
  base::flat_set<gfx::DXGIHandleToken> registered_fence_tokens_
      GUARDED_BY(lock_);
#endif
};

}  // namespace gpu

#endif  // GPU_IPC_CLIENT_SHARED_IMAGE_INTERFACE_PROXY_H_
