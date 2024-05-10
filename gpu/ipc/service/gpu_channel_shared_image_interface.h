// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_CHANNEL_SHARED_IMAGE_INTERFACE_H_
#define GPU_IPC_SERVICE_GPU_CHANNEL_SHARED_IMAGE_INTERFACE_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/ipc/common/gpu_memory_buffer_handle_info.h"
#include "gpu/ipc/service/shared_image_stub.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace base {
class WaitableEvent;
}

namespace gpu {
class Scheduler;

class GPU_IPC_SERVICE_EXPORT GpuChannelSharedImageInterface
    : public SharedImageInterface {
 public:
  explicit GpuChannelSharedImageInterface(
      base::WeakPtr<SharedImageStub> shared_image_stub,
      const CommandBufferId command_buffer_id);

  GpuChannelSharedImageInterface(const GpuChannelSharedImageInterface&) =
      delete;
  GpuChannelSharedImageInterface& operator=(
      const GpuChannelSharedImageInterface&) = delete;

  // SharedImageInterface:
  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      gpu::SurfaceHandle surface_handle) override;
  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      base::span<const uint8_t> pixel_data) override;
  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      SurfaceHandle surface_handle,
      gfx::BufferUsage buffer_usage) override;
  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      gpu::SurfaceHandle surface_handle,
      gfx::BufferUsage buffer_usage,
      gfx::GpuMemoryBufferHandle buffer_handle) override;
  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      gfx::GpuMemoryBufferHandle buffer_handle) override;
  SharedImageInterface::SharedImageMapping CreateSharedImage(
      const SharedImageInfo& si_info) override;
  scoped_refptr<ClientSharedImage> CreateSharedImage(
      gfx::GpuMemoryBuffer* gpu_memory_buffer,
      GpuMemoryBufferManager* gpu_memory_buffer_manager,
      gfx::BufferPlane plane,
      const SharedImageInfo& si_info) override;
  void UpdateSharedImage(const SyncToken& sync_token,
                         const Mailbox& mailbox) override;
  void UpdateSharedImage(const SyncToken& sync_token,
                         std::unique_ptr<gfx::GpuFence> acquire_fence,
                         const Mailbox& mailbox) override;
  void DestroySharedImage(const SyncToken& sync_token,
                          const Mailbox& mailbox) override;
  void DestroySharedImage(
      const SyncToken& sync_token,
      scoped_refptr<ClientSharedImage> client_shared_image) override;
  scoped_refptr<ClientSharedImage> ImportSharedImage(
      const ExportedSharedImage& exported_shared_image) override;
  SwapChainSharedImages CreateSwapChain(viz::SharedImageFormat format,
                                        const gfx::Size& size,
                                        const gfx::ColorSpace& color_space,
                                        GrSurfaceOrigin surface_origin,
                                        SkAlphaType alpha_type,
                                        uint32_t usage) override;
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
  void VerifySyncToken(SyncToken& sync_token) override;
  void WaitSyncToken(const SyncToken& sync_token) override;
  void Flush() override;
  scoped_refptr<gfx::NativePixmap> GetNativePixmap(
      const gpu::Mailbox& mailbox) override;

  const SharedImageCapabilities& GetCapabilities() override;

 protected:
  ~GpuChannelSharedImageInterface() override;

 private:
  SyncToken MakeSyncToken(uint64_t release_id) {
    return SyncToken(CommandBufferNamespace::GPU_IO, command_buffer_id_,
                     release_id);
  }

  void ScheduleGpuTask(base::OnceClosure task,
                       std::vector<SyncToken> sync_token_fences);

  // Only called on the gpu thread.
  bool MakeContextCurrent(bool needs_gl = false);
  void ReleaseFenceSync(uint64_t release);
  void GetGpuMemoryBufferHandleInfoOnGpuThread(
      const Mailbox& mailbox,
      gfx::GpuMemoryBufferHandle* handle,
      viz::SharedImageFormat* format,
      gfx::Size* size,
      gfx::BufferUsage* buffer_usage,
      base::WaitableEvent* completion);

  void CreateSharedImageOnGpuThread(const Mailbox& mailbox,
                                    SharedImageInfo si_info,
                                    gpu::SurfaceHandle surface_handle,
                                    uint64_t release);
  void CreateSharedImageWithDataOnGpuThread(const Mailbox& mailbox,
                                            SharedImageInfo si_info,
                                            std::vector<uint8_t> pixel_data,
                                            uint64_t release);
  void CreateSharedImageWithBufferUsageOnGpuThread(
      const Mailbox& mailbox,
      SharedImageInfo si_info,
      SurfaceHandle surface_handle,
      gfx::BufferUsage buffer_usage,
      uint64_t release);
  void CreateSharedImageWithBufferOnGpuThread(
      const Mailbox& mailbox,
      SharedImageInfo si_info,
      gfx::GpuMemoryBufferHandle buffer_handle,
      uint64_t release);
  void CreateGMBSharedImageOnGpuThread(const Mailbox& mailbox,
                                       gfx::GpuMemoryBufferHandle handle,
                                       gfx::BufferFormat format,
                                       gfx::BufferPlane plane,
                                       const gfx::Size& size,
                                       SharedImageInfo si_info,
                                       uint64_t release);
  void UpdateSharedImageOnGpuThread(const Mailbox& mailbox, uint64_t release);
  void DestroySharedImageOnGpuThread(const Mailbox& mailbox);
  void DestroyClientSharedImageOnGpuThread(
      scoped_refptr<ClientSharedImage> client_shared_image);
  void WrapTaskWithGpuUrl(base::OnceClosure task);

  GpuMemoryBufferHandleInfo GetGpuMemoryBufferHandleInfo(
      const Mailbox& mailbox);

  base::WeakPtr<SharedImageStub> shared_image_stub_;

  // Sequence checker for tasks that run on the gpu "thread".
  SEQUENCE_CHECKER(gpu_sequence_checker_);

  // Accessed on any thread.
  base::Lock lock_;
  uint64_t next_fence_sync_release_ GUARDED_BY(lock_) = 1;

  const CommandBufferId command_buffer_id_;
  raw_ptr<Scheduler> scheduler_;
  const SequenceId sequence_;
  scoped_refptr<gpu::SyncPointClientState> sync_point_client_state_;
  SharedImageCapabilities shared_image_capabilities_;
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_CHANNEL_SHARED_IMAGE_INTERFACE_H_
