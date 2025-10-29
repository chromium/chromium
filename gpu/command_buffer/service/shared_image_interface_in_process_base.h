// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_INTERFACE_IN_PROCESS_BASE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_INTERFACE_IN_PROCESS_BASE_H_

#include "base/sequence_checker.h"
#include "base/synchronization/waitable_event.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
class SharedImageFactory;

// Common base implementation for in-process `SharedImageInterface`
// Note: `SharedImageInterface` requires //gpu OWNERS approval for new
// implementations by use of a private constructor; this subclass does not
// enforce the same visibility restrictions on its own subclasses, but does
// mark all non-trivial overrides `final` to ensure subclasses restrict
// themselves to expected extension points; `override`-qualified methods here
// are generally implemented as `NOTREACHED()`.
class GPU_GLES2_EXPORT SharedImageInterfaceInProcessBase
    : public SharedImageInterface {
 public:
  // Generate the next sequence token in the process's sequence
  SyncToken GenNextSyncToken();

  // Include default-args overloads from superclass.
  using SharedImageInterface::CreateSharedImage;

  // SharedImageInterface:
  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      gpu::SurfaceHandle surface_handle,
      std::optional<SharedImagePoolId> pool_id) final;
  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      base::span<const uint8_t> pixel_data) final;
  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      SurfaceHandle surface_handle,
      gfx::BufferUsage buffer_usage,
      std::optional<SharedImagePoolId> pool_id) override;
  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      gpu::SurfaceHandle surface_handle,
      gfx::BufferUsage buffer_usage,
      gfx::GpuMemoryBufferHandle buffer_handle) final;
  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      gfx::GpuMemoryBufferHandle buffer_handle) final;
  scoped_refptr<ClientSharedImage> CreateSharedImageForMLTensor(
      std::string debug_label,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      gpu::SharedImageUsageSet usage) override;
  scoped_refptr<ClientSharedImage> CreateSharedImageForSoftwareCompositor(
      const SharedImageInfo& si_info) final;
  void UpdateSharedImage(const SyncToken& sync_token,
                         const Mailbox& mailbox) final;
  void UpdateSharedImage(const SyncToken& sync_token,
                         std::unique_ptr<gfx::GpuFence> acquire_fence,
                         const Mailbox& mailbox) final;
  void DestroySharedImage(const SyncToken& sync_token,
                          const Mailbox& mailbox) final;
  void DestroySharedImage(
      const SyncToken& sync_token,
      scoped_refptr<ClientSharedImage> client_shared_image) final;
  scoped_refptr<ClientSharedImage> ImportSharedImage(
      ExportedSharedImage exported_shared_image) override;
#if BUILDFLAG(IS_FUCHSIA)
  void RegisterSysmemBufferCollection(zx::eventpair service_handle,
                                      zx::channel sysmem_token,
                                      const viz::SharedImageFormat& format,
                                      gfx::BufferUsage usage,
                                      bool register_with_image_pipe) override;
#endif  // BUILDFLAG(IS_FUCHSIA)
  SyncToken GenUnverifiedSyncToken() final;
  SyncToken GenVerifiedSyncToken() final;
  void VerifySyncToken(SyncToken& sync_token) final;
  bool CanVerifySyncToken(const gpu::SyncToken& sync_token) final;
  void VerifyFlush() final;
  void WaitSyncToken(const SyncToken& sync_token) final;
  const SharedImageCapabilities& GetCapabilities() final;

 protected:
  // `MakeSyncToken()` creates a sync token with `namespace_id` and
  // `command_buffer_id`; `GenCreationSyncToken()` verifies that sync token
  // or not based on `verify_creation_sync_token`. If
  // `shared_image_capabilities` is not provided to the constructor it will be
  // lazily-created on the GPU thread.
  SharedImageInterfaceInProcessBase(
      CommandBufferNamespace namespace_id,
      CommandBufferId command_buffer_id,
      bool verify_creation_sync_token,
      SharedImageCapabilities shared_image_capabilities);
  SharedImageInterfaceInProcessBase(CommandBufferNamespace namespace_id,
                                    CommandBufferId command_buffer_id,
                                    bool verify_creation_sync_token);

  ~SharedImageInterfaceInProcessBase() override;

  // Schedule the `task` on the GPU, waiting on `sync_token_fences` and
  // signalling `release` when done.
  // The GPU expects monotonically increasing release IDs of the `release` sync
  // token, generally accomplished in this class by initializing `release` from
  // `GenNextSyncTokenLocked()` and calling `ScheduleGpuTask()` in the same
  // `lock_`-guarded critical section. Given the critical-section call,
  // subclass implementations of `ScheduleGpuTask()` should perform a small and
  // bounded amount of work.
  virtual void ScheduleGpuTask(base::OnceClosure task,
                               std::vector<SyncToken> sync_token_fences,
                               const SyncToken& release) = 0;

  // Get a reference to a valid `SharedImageFactory`, null if not available.
  // This should only be called on the GPU thread.
  virtual SharedImageFactory* GetSharedImageFactoryOnGpuThread() = 0;

  // Returns `false` if cannot make context current.
  // This should only be called on the GPU thread.
  virtual bool MakeContextCurrentOnGpuThread(bool needs_gl /* = false*/) = 0;

  bool MakeContextCurrentOnGpuThread() {
    return MakeContextCurrentOnGpuThread(false);
  }

  // Mark context lost if shared image cannot be created.
  // This should only be called on the GPU thread.
  virtual void MarkContextLostOnGpuThread() = 0;

  CommandBufferId command_buffer_id() const { return command_buffer_id_; }

  // Sequence checker for tasks that run on the gpu thread.
  SEQUENCE_CHECKER(gpu_sequence_checker_);

 private:
  void CreateSharedImageOnGpuThread(const Mailbox& mailbox,
                                    SharedImageInfo si_info,
                                    gpu::SurfaceHandle surface_handle);

  void CreateSharedImageWithDataOnGpuThread(const Mailbox& mailbox,
                                            SharedImageInfo si_info,
                                            std::vector<uint8_t> pixel_data);

  void CreateSharedImageWithBufferUsageOnGpuThread(
      const Mailbox& mailbox,
      SharedImageInfo si_info,
      SurfaceHandle surface_handle,
      gfx::BufferUsage buffer_usage);

  void GetGpuMemoryBufferHandleInfoOnGpuThread(
      const Mailbox& mailbox,
      gfx::GpuMemoryBufferHandle* handle,
      gfx::BufferUsage* buffer_usage,
      base::WaitableEvent* completion);

  void CreateSharedImageWithBufferOnGpuThread(
      const Mailbox& mailbox,
      SharedImageInfo si_info,
      gfx::GpuMemoryBufferHandle buffer_handle);

  // Blocks and waits for a response from the GPU main thread
  GpuMemoryBufferHandleInfo GetGpuMemoryBufferHandleInfo(
      const Mailbox& mailbox);

  void UpdateSharedImageOnGpuThread(const Mailbox& mailbox);

  void DestroySharedImageOnGpuThread(const Mailbox& mailbox);

  void GetCapabilitiesOnGpuThread();

  SyncToken MakeSyncToken(uint64_t release_id) {
    return {namespace_id_, command_buffer_id_, release_id};
  }

  // Internal version of `GenNextSyncToken()` for when `lock_` is already held
  SyncToken GenNextSyncTokenLocked() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Generate a sync token for `CreateSharedImage()`, with verification
  // dependent on `verify_creation_sync_token_`
  SyncToken GenCreationSyncToken() {
    return verify_creation_sync_token_ ? GenVerifiedSyncToken()
                                       : GenUnverifiedSyncToken();
  }

  // Accessed on any thread.
  base::Lock lock_;
  uint64_t next_fence_sync_release_ GUARDED_BY(lock_) = 1;

  const CommandBufferNamespace namespace_id_;
  const CommandBufferId command_buffer_id_;
  const bool verify_creation_sync_token_;

  // This should only be non-default initialized at construction or from the GPU
  // thread. `shared_image_capabilities_ready_.IsSignalled()` indicates that it
  // is safe to read from.
  SharedImageCapabilities shared_image_capabilities_;
  base::WaitableEvent shared_image_capabilities_ready_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_INTERFACE_IN_PROCESS_BASE_H_
