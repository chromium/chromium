// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_INTERFACE_IN_PROCESS_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_INTERFACE_IN_PROCESS_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/gpu_gles2_export.h"
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
  SharedImageInterfaceInProcess(SingleTaskSequence* task_sequence,
                                SyncPointManager* sync_point_manager,
                                const GpuPreferences& gpu_preferences,
                                const GpuDriverBugWorkarounds& gpu_workarounds,
                                const GpuFeatureInfo& gpu_feature_info,
                                gpu::SharedContextState* context_state,
                                SharedImageManager* shared_image_manager,
                                bool is_for_display_compositor);

  SharedImageInterfaceInProcess(const SharedImageInterfaceInProcess&) = delete;
  SharedImageInterfaceInProcess& operator=(
      const SharedImageInterfaceInProcess&) = delete;

  ~SharedImageInterfaceInProcess() override;

  // The |SharedImageInterface| keeps ownership of the image until
  // |DestroySharedImage| is called or the interface itself is destroyed (e.g.
  // the GPU channel is lost).
  Mailbox CreateSharedImage(viz::SharedImageFormat format,
                            const gfx::Size& size,
                            const gfx::ColorSpace& color_space,
                            GrSurfaceOrigin surface_origin,
                            SkAlphaType alpha_type,
                            uint32_t usage,
                            base::StringPiece debug_label,
                            gpu::SurfaceHandle surface_handle) override;

  // Same behavior as the above, except that this version takes |pixel_data|
  // which is used to populate the SharedImage.  |pixel_data| should have the
  // same format which would be passed to glTexImage2D to populate a similarly
  // specified texture.
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

  // |usage| is a combination of |SharedImageUsage| bits that describes which
  // API(s) the image will be used with. Format and size are derived from the
  // GpuMemoryBuffer. |gpu_memory_buffer_manager| is the manager that created
  // |gpu_memory_buffer|. If the |gpu_memory_buffer| was created on the client
  // side (for NATIVE_PIXMAP or ANDROID_HARDWARE_BUFFER types only), without a
  // GpuMemoryBufferManager, |gpu_memory_buffer_manager| can be nullptr.
  // If valid, |color_space| will be applied to the shared
  // image (possibly overwriting the one set on the GpuMemoryBuffer).
  // The |SharedImageInterface| keeps ownership of the image until
  // |DestroySharedImage| is called or the interface itself is destroyed (e.g.
  // the GPU channel is lost).
  Mailbox CreateSharedImage(gfx::GpuMemoryBuffer* gpu_memory_buffer,
                            GpuMemoryBufferManager* gpu_memory_buffer_manager,
                            gfx::BufferPlane plane,
                            const gfx::ColorSpace& color_space,
                            GrSurfaceOrigin surface_origin,
                            SkAlphaType alpha_type,
                            uint32_t usage,
                            base::StringPiece debug_label) override;

  // Updates a shared image after its GpuMemoryBuffer (if any) was modified on
  // the CPU or through external devices, after |sync_token| has been released.
  void UpdateSharedImage(const SyncToken& sync_token,
                         const Mailbox& mailbox) override;

  // Updates a shared image after its GpuMemoryBuffer (if any) was modified on
  // the CPU or through external devices, after |sync_token| has been released.
  // If |acquire_fence| is not null, the fence is inserted in the GPU command
  // stream and a server side wait is issued before any GPU command referring
  // to this shared imaged is executed on the GPU.
  void UpdateSharedImage(const SyncToken& sync_token,
                         std::unique_ptr<gfx::GpuFence> acquire_fence,
                         const Mailbox& mailbox) override;

  // Destroys the shared image, unregistering its mailbox, after |sync_token|
  // has been released. After this call, the mailbox can't be used to reference
  // the image any more, however if the image was imported into other APIs,
  // those may keep a reference to the underlying data.
  void DestroySharedImage(const SyncToken& sync_token,
                          const Mailbox& mailbox) override;

  void AddReferenceToSharedImage(const SyncToken& sync_token,
                                 const Mailbox& mailbox,
                                 uint32_t usage) override;

  // Creates a swap chain. Not reached in this implementation.
  SwapChainMailboxes CreateSwapChain(viz::SharedImageFormat format,
                                     const gfx::Size& size,
                                     const gfx::ColorSpace& color_space,
                                     GrSurfaceOrigin surface_origin,
                                     SkAlphaType alpha_type,
                                     uint32_t usage) override;

  // Swaps front and back buffer of a swap chain. Not reached in this
  // implementation.
  void PresentSwapChain(const SyncToken& sync_token,
                        const Mailbox& mailbox) override;

#if BUILDFLAG(IS_FUCHSIA)
  // Registers a sysmem buffer collection. Not reached in this implementation.
  void RegisterSysmemBufferCollection(zx::eventpair service_handle,
                                      zx::channel sysmem_token,
                                      gfx::BufferFormat format,
                                      gfx::BufferUsage usage,
                                      bool register_with_image_pipe) override;
#endif  // BUILDFLAG(IS_FUCHSIA)

  // Generates an unverified SyncToken that is released after all previous
  // commands on this interface have executed on the service side.
  SyncToken GenUnverifiedSyncToken() override;

  // Generates a verified SyncToken that is released after all previous
  // commands on this interface have executed on the service side.
  SyncToken GenVerifiedSyncToken() override;

  void WaitSyncToken(const SyncToken& sync_token) override;

  // Flush the SharedImageInterface, issuing any deferred IPCs.
  void Flush() override;

  scoped_refptr<gfx::NativePixmap> GetNativePixmap(
      const gpu::Mailbox& mailbox) override;

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
  void CreateSharedImageOnGpuThread(const Mailbox& mailbox,
                                    viz::SharedImageFormat format,
                                    gpu::SurfaceHandle surface_handle,
                                    const gfx::Size& size,
                                    const gfx::ColorSpace& color_space,
                                    GrSurfaceOrigin surface_origin,
                                    SkAlphaType alpha_type,
                                    uint32_t usage,
                                    std::string debug_label,
                                    const SyncToken& sync_token);
  void CreateSharedImageWithDataOnGpuThread(const Mailbox& mailbox,
                                            viz::SharedImageFormat format,
                                            const gfx::Size& size,
                                            const gfx::ColorSpace& color_space,
                                            GrSurfaceOrigin surface_origin,
                                            SkAlphaType alpha_type,
                                            uint32_t usage,
                                            std::string debug_label,
                                            const SyncToken& sync_token,
                                            std::vector<uint8_t> pixel_data);

  void CreateGMBSharedImageOnGpuThread(const Mailbox& mailbox,
                                       gfx::GpuMemoryBufferHandle handle,
                                       gfx::BufferFormat format,
                                       gfx::BufferPlane plane,
                                       const gfx::Size& size,
                                       const gfx::ColorSpace& color_space,
                                       GrSurfaceOrigin surface_origin,
                                       SkAlphaType alpha_type,
                                       uint32_t usage,
                                       std::string debug_label,
                                       const SyncToken& sync_token);
  void UpdateSharedImageOnGpuThread(const Mailbox& mailbox,
                                    const SyncToken& sync_token);
  void DestroySharedImageOnGpuThread(const Mailbox& mailbox);
  void WaitSyncTokenOnGpuThread(const SyncToken& sync_token);
  void WrapTaskWithGpuUrl(base::OnceClosure task);

  // Used to schedule work on the gpu thread. This is a raw pointer for now
  // since the ownership of SingleTaskSequence would be the same as the
  // SharedImageInterfaceInProcess.
  raw_ptr<SingleTaskSequence> task_sequence_;
  const CommandBufferId command_buffer_id_;

  base::OnceCallback<std::unique_ptr<SharedImageFactory>()> create_factory_;

  // Sequence checker for tasks that run on the gpu "thread".
  SEQUENCE_CHECKER(gpu_sequence_checker_);

  // Accessed on any thread. release_id_lock_ protects access to
  // next_fence_sync_release_.
  base::Lock lock_;
  uint64_t next_fence_sync_release_ = 1;

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
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_INTERFACE_IN_PROCESS_H_
