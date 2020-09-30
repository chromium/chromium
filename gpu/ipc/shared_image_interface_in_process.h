// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SHARED_IMAGE_INTERFACE_IN_PROCESS_H_
#define GPU_IPC_SHARED_IMAGE_INTERFACE_IN_PROCESS_H_

#include "build/build_config.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/ipc/in_process_command_buffer.h"

namespace gpu {
class CommandBufferTaskExecutor;
class ImageFactory;
class MailboxManager;
class MemoryTracker;
class SyncPointClientState;
struct SyncToken;
class SharedContextState;
class SharedImageFactory;
class SharedImageManager;
class SingleTaskSequence;

// This is an implementation of the SharedImageInterface to be used on viz
// compositor thread. This class also implements the corresponding parts
// happening on gpu thread.
// TODO(weiliangc): Currently this is implemented as backed by
// InProcessCommandBuffer. Add constructor for using with SkiaRenderer.
class GL_IN_PROCESS_CONTEXT_EXPORT SharedImageInterfaceInProcess
    : public SharedImageInterface {
 public:
  using CommandBufferHelper =
      InProcessCommandBuffer::SharedImageInterfaceHelper;
  SharedImageInterfaceInProcess(
      CommandBufferTaskExecutor* task_executor,
      SingleTaskSequence* task_sequence,
      CommandBufferId command_buffer_id,
      MailboxManager* mailbox_manager,
      ImageFactory* image_factory,
      MemoryTracker* memory_tracker,
      std::unique_ptr<CommandBufferHelper> command_buffer_helper);
  ~SharedImageInterfaceInProcess() override;

  // The |SharedImageInterface| keeps ownership of the image until
  // |DestroySharedImage| is called or the interface itself is destroyed (e.g.
  // the GPU channel is lost).
  Mailbox CreateSharedImage(viz::ResourceFormat format,
                            const gfx::Size& size,
                            const gfx::ColorSpace& color_space,
                            GrSurfaceOrigin surface_origin,
                            SkAlphaType alpha_type,
                            uint32_t usage,
                            gpu::SurfaceHandle surface_handle) override;

  // Same behavior as the above, except that this version takes |pixel_data|
  // which is used to populate the SharedImage.  |pixel_data| should have the
  // same format which would be passed to glTexImage2D to populate a similarly
  // specified texture.
  Mailbox CreateSharedImage(viz::ResourceFormat format,
                            const gfx::Size& size,
                            const gfx::ColorSpace& color_space,
                            GrSurfaceOrigin surface_origin,
                            SkAlphaType alpha_type,
                            uint32_t usage,
                            base::span<const uint8_t> pixel_data) override;

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
                            const gfx::ColorSpace& color_space,
                            GrSurfaceOrigin surface_origin,
                            SkAlphaType alpha_type,
                            uint32_t usage) override;

#if defined(OS_ANDROID)
  Mailbox CreateSharedImageWithAHB(const Mailbox& mailbox,
                                   uint32_t usage,
                                   const SyncToken& sync_token) override;
#endif

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

  // Creates a swap chain. Not reached in this implementation.
  SwapChainMailboxes CreateSwapChain(viz::ResourceFormat format,
                                     const gfx::Size& size,
                                     const gfx::ColorSpace& color_space,
                                     GrSurfaceOrigin surface_origin,
                                     SkAlphaType alpha_type,
                                     uint32_t usage) override;

  // Swaps front and back buffer of a swap chain. Not reached in this
  // implementation.
  void PresentSwapChain(const SyncToken& sync_token,
                        const Mailbox& mailbox) override;

#if defined(OS_FUCHSIA)
  // Registers a sysmem buffer collection. Not reached in this implementation.
  void RegisterSysmemBufferCollection(gfx::SysmemBufferCollectionId id,
                                      zx::channel token,
                                      gfx::BufferFormat format,
                                      gfx::BufferUsage usage,
                                      bool register_with_image_pipe) override;

  // Not reached in this implementation.
  void ReleaseSysmemBufferCollection(gfx::SysmemBufferCollectionId id) override;
#endif  // defined(OS_FUCHSIA)

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
  struct SharedImageFactoryInput;

  void SetUpOnGpu(CommandBufferTaskExecutor* task_executor,
                  ImageFactory* image_factory,
                  MemoryTracker* memory_tracker);
  void DestroyOnGpu(base::WaitableEvent* completion);

  SyncToken MakeSyncToken(uint64_t release_id) {
    return SyncToken(CommandBufferNamespace::IN_PROCESS, command_buffer_id_,
                     release_id);
  }

  void ScheduleGpuTask(base::OnceClosure task,
                       std::vector<SyncToken> sync_token_fences);

  // Only called on the gpu thread.
  bool MakeContextCurrent(bool needs_gl = false);
  void LazyCreateSharedImageFactory();
  void CreateSharedImageOnGpuThread(const Mailbox& mailbox,
                                    viz::ResourceFormat format,
                                    gpu::SurfaceHandle surface_handle,
                                    const gfx::Size& size,
                                    const gfx::ColorSpace& color_space,
                                    GrSurfaceOrigin surface_origin,
                                    SkAlphaType alpha_type,
                                    uint32_t usage,
                                    const SyncToken& sync_token);
  void CreateSharedImageWithDataOnGpuThread(const Mailbox& mailbox,
                                            viz::ResourceFormat format,
                                            const gfx::Size& size,
                                            const gfx::ColorSpace& color_space,
                                            GrSurfaceOrigin surface_origin,
                                            SkAlphaType alpha_type,
                                            uint32_t usage,
                                            const SyncToken& sync_token,
                                            std::vector<uint8_t> pixel_data);

  void CreateGMBSharedImageOnGpuThread(const Mailbox& mailbox,
                                       gfx::GpuMemoryBufferHandle handle,
                                       gfx::BufferFormat format,
                                       const gfx::Size& size,
                                       const gfx::ColorSpace& color_space,
                                       GrSurfaceOrigin surface_origin,
                                       SkAlphaType alpha_type,
                                       uint32_t usage,
                                       const SyncToken& sync_token);
  void UpdateSharedImageOnGpuThread(const Mailbox& mailbox,
                                    const SyncToken& sync_token);
  void DestroySharedImageOnGpuThread(const Mailbox& mailbox);
  void WaitSyncTokenOnGpuThread(const SyncToken& sync_token);
  void WrapTaskWithGpuUrl(base::OnceClosure task);
#if defined(OS_ANDROID)
  void CreateSharedImageWithAHBOnGpuThread(const Mailbox& out_mailbox,
                                           const Mailbox& in_mailbox,
                                           uint32_t usage,
                                           const SyncToken& sync_token);
#endif

  // Used to schedule work on the gpu thread. This is a raw pointer for now
  // since the ownership of SingleTaskSequence would be the same as the
  // SharedImageInterfaceInProcess.
  SingleTaskSequence* task_sequence_;
  const CommandBufferId command_buffer_id_;
  std::unique_ptr<CommandBufferHelper> command_buffer_helper_;

  base::OnceCallback<std::unique_ptr<SharedImageFactory>(
      bool enable_wrapped_sk_image)>
      create_factory_;

  // Sequence checker for tasks that run on the gpu "thread".
  SEQUENCE_CHECKER(gpu_sequence_checker_);

  // Accessed on any thread. release_id_lock_ protects access to
  // next_fence_sync_release_.
  base::Lock lock_;
  uint64_t next_fence_sync_release_ = 1;

  // Accessed on compositor thread.
  // This is used to get NativePixmap, and is only used when SharedImageManager
  // is thread safe.
  SharedImageManager* shared_image_manager_;

  // Accessed on GPU thread.
  // TODO(weiliangc): Check whether can be removed when !UsesSync().
  MailboxManager* mailbox_manager_;
  // Used to check if context is lost at destruction time.
  // TODO(weiliangc): SharedImageInterface should become active observer of
  // whether context is lost.
  SharedContextState* context_state_;
  // Created and only used by this SharedImageInterface.
  SyncPointManager* sync_point_manager_;
  scoped_refptr<SyncPointClientState> sync_point_client_state_;
  std::unique_ptr<SharedImageFactory> shared_image_factory_;

  DISALLOW_COPY_AND_ASSIGN(SharedImageInterfaceInProcess);
};

}  // namespace gpu

#endif  // GPU_IPC_SHARED_IMAGE_INTERFACE_IN_PROCESS_H_
