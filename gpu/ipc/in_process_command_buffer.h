// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_IN_PROCESS_COMMAND_BUFFER_H_
#define GPU_IPC_IN_PROCESS_COMMAND_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "components/viz/common/display/update_vsync_parameters_callback.h"
#include "components/viz/common/gpu/gpu_vsync_callback.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/command_buffer/service/gr_cache_controller.h"
#include "gpu/command_buffer/service/program_cache.h"
#include "gpu/command_buffer/service/service_discardable_manager.h"
#include "gpu/command_buffer/service/service_transfer_cache.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/command_buffer_task_executor.h"
#include "gpu/ipc/gl_in_process_context_export.h"
#include "gpu/ipc/service/context_url.h"
#include "gpu/ipc/service/display_context.h"
#include "gpu/ipc/service/image_transport_surface_delegate.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gpu_preference.h"

namespace gl {
class GLContext;
class GLShareGroup;
}

namespace gfx {
struct GpuFenceHandle;
class Size;
}

namespace gpu {
class SharedContextState;
class GpuChannelManagerDelegate;
class GpuProcessActivityFlags;
class GpuMemoryBufferManager;
class ImageFactory;
class SharedImageFactory;
class SharedImageInterface;
class SyncPointClientState;
struct ContextCreationAttribs;
struct SwapBuffersCompleteParams;

namespace raster {
class GrShaderCache;
}

// This class provides a thread-safe interface to the global GPU service (for
// example GPU thread) when being run in single process mode.
// However, the behavior for accessing one context (i.e. one instance of this
// class) from different client threads is undefined. See
// ui::InProcessContextProvider for an example of how to define multi-threading
// semantics.
class GL_IN_PROCESS_CONTEXT_EXPORT InProcessCommandBuffer
    : public CommandBuffer,
      public GpuControl,
      public CommandBufferServiceClient,
      public DecoderClient,
      public ImageTransportSurfaceDelegate,
      public DisplayContext {
 public:
  InProcessCommandBuffer(CommandBufferTaskExecutor* task_executor,
                         const GURL& active_url);
  ~InProcessCommandBuffer() override;

  // If |surface| is not null, use it directly; in this case, the command
  // buffer gpu thread must be the same as the client thread. Otherwise create
  // a new GLSurface.
  // |gpu_channel_manager_delegate| should be non-null when the command buffer
  // is used in the GPU process for compositor to gpu thread communication.
  gpu::ContextResult Initialize(
      scoped_refptr<gl::GLSurface> surface,
      bool is_offscreen,
      SurfaceHandle surface_handle,
      const ContextCreationAttribs& attribs,
      GpuMemoryBufferManager* gpu_memory_buffer_manager,
      ImageFactory* image_factory,
      GpuChannelManagerDelegate* gpu_channel_manager_delegate,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      gpu::raster::GrShaderCache* gr_shader_cache,
      GpuProcessActivityFlags* activity_flags);

  // CommandBuffer implementation (called on client thread):
  State GetLastState() override;
  void Flush(int32_t put_offset) override;
  void OrderingBarrier(int32_t put_offset) override;
  State WaitForTokenInRange(int32_t start, int32_t end) override;
  State WaitForGetOffsetInRange(uint32_t set_get_buffer_count,
                                int32_t start,
                                int32_t end) override;
  void SetGetBuffer(int32_t shm_id) override;
  scoped_refptr<Buffer> CreateTransferBuffer(uint32_t size,
                                             int32_t* id) override;
  void DestroyTransferBuffer(int32_t id) override;

  // GpuControl implementation (called on client thread):
  void SetGpuControlClient(GpuControlClient*) override;
  // GetCapabilities() can be called on any thread.
  const Capabilities& GetCapabilities() const override;
  int32_t CreateImage(ClientBuffer buffer,
                      size_t width,
                      size_t height) override;
  void DestroyImage(int32_t id) override;
  void SignalQuery(uint32_t query_id, base::OnceClosure callback) override;
  void CreateGpuFence(uint32_t gpu_fence_id, ClientGpuFence source) override;
  void GetGpuFence(uint32_t gpu_fence_id,
                   base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)>
                       callback) override;
  void SetLock(base::Lock*) override;
  void EnsureWorkVisible() override;
  CommandBufferNamespace GetNamespaceID() const override;
  CommandBufferId GetCommandBufferID() const override;
  void FlushPendingWork() override;
  uint64_t GenerateFenceSyncRelease() override;
  bool IsFenceSyncReleased(uint64_t release) override;
  void SignalSyncToken(const SyncToken& sync_token,
                       base::OnceClosure callback) override;
  void WaitSyncToken(const SyncToken& sync_token) override;
  bool CanWaitUnverifiedSyncToken(const SyncToken& sync_token) override;
  void SetDisplayTransform(gfx::OverlayTransform transform) override;

  // CommandBufferServiceClient implementation (called on gpu thread):
  CommandBatchProcessedResult OnCommandBatchProcessed() override;
  void OnParseError() override;

  // DisplayContext implementation (called on gpu thread):
  void MarkContextLost() override;

  // DecoderClient implementation (called on gpu thread):
  void OnConsoleMessage(int32_t id, const std::string& message) override;
  void CacheShader(const std::string& key, const std::string& shader) override;
  void OnFenceSyncRelease(uint64_t release) override;
  void OnDescheduleUntilFinished() override;
  void OnRescheduleAfterFinished() override;
  void OnSwapBuffers(uint64_t swap_id, uint32_t flags) override;
  void ScheduleGrContextCleanup() override;
  void HandleReturnData(base::span<const uint8_t> data) override;

// ImageTransportSurfaceDelegate implementation:
#if defined(OS_WIN)
  void DidCreateAcceleratedSurfaceChildWindow(
      SurfaceHandle parent_window,
      SurfaceHandle child_window) override;
#endif
  void DidSwapBuffersComplete(SwapBuffersCompleteParams params) override;
  const gles2::FeatureInfo* GetFeatureInfo() const override;
  const GpuPreferences& GetGpuPreferences() const override;
  void BufferPresented(const gfx::PresentationFeedback& feedback) override;
  viz::GpuVSyncCallback GetGpuVSyncCallback() override;
  base::TimeDelta GetGpuBlockedTimeSinceLastSwap() override;

  // Upstream this function to GpuControl if needs arise. Can be called on any
  // thread.
  const GpuFeatureInfo& GetGpuFeatureInfo() const;

  void SetUpdateVSyncParametersCallback(
      viz::UpdateVSyncParametersCallback callback);

  void SetGpuVSyncCallback(viz::GpuVSyncCallback callback);
  void SetGpuVSyncEnabled(bool enabled);

  void SetGpuVSyncEnabledOnThread(bool enabled);

  gpu::ServiceTransferCache* GetTransferCacheForTest() const;
  int GetRasterDecoderIdForTest() const;

  CommandBufferTaskExecutor* service_for_testing() const {
    return task_executor_;
  }

  gpu::SharedImageInterface* GetSharedImageInterface() const;

  // Provides a callback that can be used to preserve the back buffer for the
  // GLSurface associated with the command buffer, even after the command buffer
  // has been destroyed. The back buffer is evicted once the callback is
  // dispatched.
  // Note that the caller is responsible for ensuring that the |task_executor|
  // and |surface_handle| provided in Initialize outlive this callback.
  base::ScopedClosureRunner GetCacheBackBufferCb();

 private:
  class SharedImageInterface;

  struct InitializeOnGpuThreadParams {
    SurfaceHandle surface_handle;
    const ContextCreationAttribs& attribs;
    Capabilities* capabilities;  // Ouptut.
    ImageFactory* image_factory;
    gpu::raster::GrShaderCache* gr_shader_cache;
    GpuProcessActivityFlags* activity_flags;

    InitializeOnGpuThreadParams(SurfaceHandle surface_handle,
                                const ContextCreationAttribs& attribs,
                                Capabilities* capabilities,
                                ImageFactory* image_factory,
                                gpu::raster::GrShaderCache* gr_shader_cache,
                                GpuProcessActivityFlags* activity_flags)
        : surface_handle(surface_handle),
          attribs(attribs),
          capabilities(capabilities),
          image_factory(image_factory),
          gr_shader_cache(gr_shader_cache),
          activity_flags(activity_flags) {}
  };

  // Initialize() and Destroy() are called on the client thread, but post tasks
  // to the gpu thread to perform the actual initialization or destruction.
  gpu::ContextResult InitializeOnGpuThread(
      const InitializeOnGpuThreadParams& params);
  void Destroy();
  bool DestroyOnGpuThread();

  // Flush up to put_offset. If execution is deferred either by yielding, or due
  // to a sync token wait, HasUnprocessedCommandsOnGpuThread() returns true.
  void FlushOnGpuThread(int32_t put_offset,
                        const std::vector<SyncToken>& sync_token_fences);
  bool HasUnprocessedCommandsOnGpuThread();
  void UpdateLastStateOnGpuThread();

  void ScheduleDelayedWorkOnGpuThread();

  bool MakeCurrent();

  base::Optional<gles2::ProgramCache::ScopedCacheUse> CreateCacheUse();

  // Client callbacks are posted back to |origin_task_runner_|, or run
  // synchronously if there's no task runner or message loop.
  void PostOrRunClientCallback(base::OnceClosure callback);
  base::OnceClosure WrapClientCallback(base::OnceClosure callback);

  void RunTaskOnGpuThread(base::OnceClosure task);
  void ScheduleGpuTask(
      base::OnceClosure task,
      std::vector<SyncToken> sync_token_fences = std::vector<SyncToken>());
  void ContinueGpuTask(base::OnceClosure task);

  void SignalSyncTokenOnGpuThread(const SyncToken& sync_token,
                                  base::OnceClosure callback);
  void SignalQueryOnGpuThread(unsigned query_id, base::OnceClosure callback);

  void RegisterTransferBufferOnGpuThread(int32_t id,
                                         scoped_refptr<Buffer> buffer);
  void DestroyTransferBufferOnGpuThread(int32_t id);

  void CreateImageOnGpuThread(int32_t id,
                              gfx::GpuMemoryBufferHandle handle,
                              const gfx::Size& size,
                              gfx::BufferFormat format,
                              uint64_t fence_sync);
  void DestroyImageOnGpuThread(int32_t id);

  void SetGetBufferOnGpuThread(int32_t shm_id, base::WaitableEvent* completion);

  void CreateGpuFenceOnGpuThread(uint32_t gpu_fence_id,
                                 const gfx::GpuFenceHandle& handle);
  void GetGpuFenceOnGpuThread(
      uint32_t gpu_fence_id,
      base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)> callback);
  void LazyCreateSharedImageFactory();
  void CreateSharedImageOnGpuThread(const Mailbox& mailbox,
                                    viz::ResourceFormat format,
                                    const gfx::Size& size,
                                    const gfx::ColorSpace& color_space,
                                    uint32_t usage,
                                    const SyncToken& sync_token);
  void CreateSharedImageWithDataOnGpuThread(const Mailbox& mailbox,
                                            viz::ResourceFormat format,
                                            const gfx::Size& size,
                                            const gfx::ColorSpace& color_space,
                                            uint32_t usage,
                                            const SyncToken& sync_token,
                                            std::vector<uint8_t> pixel_data);
  void CreateGMBSharedImageOnGpuThread(const Mailbox& mailbox,
                                       gfx::GpuMemoryBufferHandle handle,
                                       gfx::BufferFormat format,
                                       const gfx::Size& size,
                                       const gfx::ColorSpace& color_space,
                                       uint32_t usage,
                                       const SyncToken& sync_token);
  void UpdateSharedImageOnGpuThread(const Mailbox& mailbox,
                                    const SyncToken& sync_token);
  void DestroySharedImageOnGpuThread(const Mailbox& mailbox);
  void SetDisplayTransformOnGpuThread(gfx::OverlayTransform transform);

  // Sets |active_url_| as the active GPU process URL. Should be called on GPU
  // thread only.
  void UpdateActiveUrl();

  // Callbacks on the gpu thread.
  void PerformDelayedWorkOnGpuThread();

  // Callback implementations on the client thread.
  void OnContextLost();
  void DidSwapBuffersCompleteOnOriginThread(SwapBuffersCompleteParams params);
  void BufferPresentedOnOriginThread(uint64_t swap_id,
                                     uint32_t flags,
                                     const gfx::PresentationFeedback& feedback);

  void HandleReturnDataOnOriginThread(std::vector<uint8_t> data);
  void HandleGpuVSyncOnOriginThread(base::TimeTicks vsync_time,
                                    base::TimeDelta vsync_interval);

  const CommandBufferId command_buffer_id_;
  const ContextUrl active_url_;

  bool is_offscreen_ = false;

  // Members accessed on the gpu thread (possibly with the exception of
  // creation):
  bool use_virtualized_gl_context_ = false;
  raster::GrShaderCache* gr_shader_cache_ = nullptr;
  scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner_;
  std::unique_ptr<CommandBufferService> command_buffer_;
  std::unique_ptr<DecoderContext> decoder_;
  base::Optional<raster::GrCacheController> gr_cache_controller_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<SyncPointClientState> sync_point_client_state_;
  scoped_refptr<SyncPointClientState> shared_image_client_state_;
  std::unique_ptr<SharedImageFactory> shared_image_factory_;

  // Used to throttle PerformDelayedWorkOnGpuThread.
  bool delayed_work_pending_ = false;
  ImageFactory* image_factory_ = nullptr;
  GpuChannelManagerDelegate* gpu_channel_manager_delegate_ = nullptr;
  // Sequence checker for tasks that run on the gpu "thread".
  SEQUENCE_CHECKER(gpu_sequence_checker_);

  // Members accessed on the client thread:
  GpuControlClient* gpu_control_client_ = nullptr;
#if DCHECK_IS_ON()
  bool context_lost_ = false;
#endif
  State last_state_;
  base::Lock last_state_lock_;
  int32_t last_put_offset_ = -1;
  Capabilities capabilities_;
  GpuMemoryBufferManager* gpu_memory_buffer_manager_ = nullptr;
  uint64_t next_fence_sync_release_ = 1;
  std::vector<SyncToken> next_flush_sync_token_fences_;
  // Sequence checker for client sequence used for initialization, destruction,
  // callbacks, such as context loss, and methods which provide such callbacks,
  // such as SignalSyncToken.
  SEQUENCE_CHECKER(client_sequence_checker_);

  // Accessed on both threads:
  base::WaitableEvent flush_event_;
  CommandBufferTaskExecutor* const task_executor_;
  std::unique_ptr<gpu::SingleTaskSequence> task_sequence_;
  std::unique_ptr<SharedImageInterface> shared_image_interface_;

  // The group of contexts that share namespaces with this context.
  scoped_refptr<gles2::ContextGroup> context_group_;

  scoped_refptr<gl::GLShareGroup> gl_share_group_;
  base::WaitableEvent fence_sync_wait_event_;

  // Callbacks on client thread.
  viz::UpdateVSyncParametersCallback update_vsync_parameters_callback_;
  viz::GpuVSyncCallback gpu_vsync_callback_;

  // Params pushed each time we call OnSwapBuffers, and popped when a buffer
  // is presented or a swap completed.
  struct SwapBufferParams {
    uint64_t swap_id;
    uint32_t flags;
  };
  base::circular_deque<SwapBufferParams> pending_presented_params_;
  base::circular_deque<SwapBufferParams> pending_swap_completed_params_;

  scoped_refptr<SharedContextState> context_state_;

  base::WeakPtr<InProcessCommandBuffer> client_thread_weak_ptr_;

  // Don't use |client_thread_weak_ptr_factory_| on GPU thread.  Use the cached
  // |client_thread_weak_ptr_| instead.
  base::WeakPtrFactory<InProcessCommandBuffer> client_thread_weak_ptr_factory_{
      this};
  base::WeakPtrFactory<InProcessCommandBuffer> gpu_thread_weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(InProcessCommandBuffer);
};

}  // namespace gpu

#endif  // GPU_IPC_IN_PROCESS_COMMAND_BUFFER_H_
