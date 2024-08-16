// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_INTERFACE_IN_PROCESS_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_INTERFACE_IN_PROCESS_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/ipc/common/gpu_memory_buffer_handle_info.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace base {
class WaitableEvent;
}

namespace gpu {
class SharedContextState;
class SharedImageFactory;
class SharedImageManager;
class SingleTaskSequence;
class SyncPointClientState;
class DisplayCompositorMemoryAndTaskControllerOnGpu;
class SyncPointManager;
struct GpuPreferences;
class GpuDriverBugWorkarounds;
struct GpuFeatureInfo;
struct SyncToken;

// This is an implementation of the SharedImageInterface to be used on the viz
// compositor thread. This class also implements the corresponding parts
// happening on gpu thread.
class GPU_GLES2_EXPORT SharedImageInterfaceInProcess
    : public SharedImageInterface {
 public:
  // Specifies which thread owns this class.
  enum class OwnerThread { kGpu, kCompositor };

  // The callers must guarantee that the instances passed via pointers are kept
  // alive for as long as the instance of this class is alive. This can be
  // achieved by ensuring that the ownership of the created
  // SharedImageInterfaceInProcess is the same as the ownership of the passed in
  // pointers.
  SharedImageInterfaceInProcess(
      SingleTaskSequence* task_sequence,
      DisplayCompositorMemoryAndTaskControllerOnGpu* display_controller);
  // The callers must guarantee that the instances passed via pointers are kept
  // alive for as long as the instance of this class is alive. This can be
  // achieved by ensuring that the ownership of the created
  // SharedImageInterfaceInProcess is the same as the ownership of the passed in
  // pointers.
  // The owner thread parameter specifies which thread owns this class. Note
  // that parts of the initialization and destruction must take place on the
  // gpu thread, so this enum determines whether a ScheduleTask is required in
  // order to complete construction/destruction.
  SharedImageInterfaceInProcess(
      SingleTaskSequence* task_sequence,
      SyncPointManager* sync_point_manager,
      const GpuPreferences& gpu_preferences,
      const GpuDriverBugWorkarounds& gpu_workarounds,
      const GpuFeatureInfo& gpu_feature_info,
      gpu::SharedContextState* context_state,
      SharedImageManager* shared_image_manager,
      bool is_for_display_compositor,
      OwnerThread owner_thread = OwnerThread::kCompositor);

  SharedImageInterfaceInProcess(const SharedImageInterfaceInProcess&) = delete;
  SharedImageInterfaceInProcess& operator=(
      const SharedImageInterfaceInProcess&) = delete;

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
                                        SharedImageUsageSet usage) override;
  void PresentSwapChain(const SyncToken& sync_token,
                        const Mailbox& mailbox) override;
#if BUILDFLAG(IS_FUCHSIA)
  void RegisterSysmemBufferCollection(zx::eventpair service_handle,
                                      zx::channel sysmem_token,
                                      const viz::SharedImageFormat& format,
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
  ~SharedImageInterfaceInProcess() override;

 private:
  // Parameters needed to be passed in to set up the class on the GPU.
  // Needed since we cannot pass refcounted instances (e.g.
  // gpu::SharedContextState) to base::BindOnce as raw pointers.
  struct SetUpOnGpuParams;

  void SetUpOnGpu(std::unique_ptr<SetUpOnGpuParams> params);
  void DestroyOnGpu(base::WaitableEvent* completion);

  SyncToken MakeSyncToken(uint64_t release_id) {
    return SyncToken(CommandBufferNamespace::IN_PROCESS, command_buffer_id_,
                     release_id);
  }

  void ScheduleGpuTask(base::OnceClosure task,
                       std::vector<SyncToken> sync_token_fences);

  // Only called on the gpu thread.
  bool MakeContextCurrent(bool needs_gl = false);
  bool LazyCreateSharedImageFactory();
  // The "OnGpuThread" version of the methods accept a std::string for
  // debug_label so it can be safely passed (copied) between threads without
  // UAF.
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
                                    const SyncToken& sync_token);
  void CreateSharedImageWithDataOnGpuThread(const Mailbox& mailbox,
                                            SharedImageInfo si_info,
                                            const SyncToken& sync_token,
                                            std::vector<uint8_t> pixel_data);
  void CreateSharedImageWithBufferUsageOnGpuThread(
      const Mailbox& mailbox,
      SharedImageInfo si_info,
      SurfaceHandle surface_handle,
      gfx::BufferUsage buffer_usage,
      const SyncToken& sync_token);
  void CreateSharedImageWithBufferOnGpuThread(
      const Mailbox& mailbox,
      SharedImageInfo si_info,
      gfx::GpuMemoryBufferHandle buffer_handle,
      const SyncToken& sync_token);
  void UpdateSharedImageOnGpuThread(const Mailbox& mailbox,
                                    const SyncToken& sync_token);
  void DestroySharedImageOnGpuThread(const Mailbox& mailbox);
  void WaitSyncTokenOnGpuThread(const SyncToken& sync_token);
  void WrapTaskWithGpuUrl(base::OnceClosure task);
  void GetCapabilitiesOnGpu(base::WaitableEvent* completion,
                            SharedImageCapabilities* out_capabilities);

  GpuMemoryBufferHandleInfo GetGpuMemoryBufferHandleInfo(
      const Mailbox& mailbox);

  // Used to schedule work on the gpu thread. This is a raw pointer for now
  // since the ownership of SingleTaskSequence would be the same as the
  // SharedImageInterfaceInProcess.
  raw_ptr<SingleTaskSequence> task_sequence_;
  const CommandBufferId command_buffer_id_;

  base::OnceCallback<std::unique_ptr<SharedImageFactory>()> create_factory_;

  // Sequence checker for tasks that run on the gpu "thread".
  SEQUENCE_CHECKER(gpu_sequence_checker_);

  // Accessed on any thread.
  base::Lock lock_;
  uint64_t next_fence_sync_release_ GUARDED_BY(lock_) = 1;

  // Accessed on compositor thread.
  // This is used to get NativePixmap, and is only used when SharedImageManager
  // is thread safe.
  raw_ptr<SharedImageManager> shared_image_manager_;

  // Accessed on GPU thread.
  scoped_refptr<SharedContextState> context_state_;
  // This is a raw pointer for now since the ownership of SyncPointManager would
  // be the same as the SharedImageInterfaceInProcess.
  raw_ptr<SyncPointManager> sync_point_manager_;
  scoped_refptr<SyncPointClientState> sync_point_client_state_;
  std::unique_ptr<SharedImageFactory> shared_image_factory_;
  std::unique_ptr<SharedImageCapabilities> shared_image_capabilities_;
  // Specifies which thread owns this object.
  const OwnerThread owner_thread_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_INTERFACE_IN_PROCESS_H_
