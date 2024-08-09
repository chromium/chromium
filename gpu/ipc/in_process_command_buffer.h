// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_IN_PROCESS_COMMAND_BUFFER_H_
#define GPU_IPC_IN_PROCESS_COMMAND_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/command_buffer_task_executor.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/command_buffer/service/display_compositor_memory_and_task_controller_on_gpu.h"
#include "gpu/command_buffer/service/gpu_task_scheduler_helper.h"
#include "gpu/command_buffer/service/gr_cache_controller.h"
#include "gpu/command_buffer/service/program_cache.h"
#include "gpu/command_buffer/service/service_discardable_manager.h"
#include "gpu/command_buffer/service/service_transfer_cache.h"
#include "gpu/command_buffer/service/shared_image_interface_in_process.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/gl_in_process_context_export.h"
#include "gpu/ipc/service/context_url.h"
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
}

namespace gpu {
class SharedContextState;
class GpuProcessShmCount;
class GpuTaskSchedulerHelper;
class SharedImageInterface;
class SyncPointClientState;
struct ContextCreationAttribs;

namespace webgpu {
class WebGPUDecoder;
}

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
      public DecoderClient {
 public:
  InProcessCommandBuffer(CommandBufferTaskExecutor* task_executor,
                         const GURL& active_url);

  InProcessCommandBuffer(const InProcessCommandBuffer&) = delete;
  InProcessCommandBuffer& operator=(const InProcessCommandBuffer&) = delete;

  ~InProcessCommandBuffer() override;

  gpu::ContextResult Initialize(
      const ContextCreationAttribs& attribs,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      gpu::raster::GrShaderCache* gr_shader_cache,
      GpuProcessShmCount* use_shader_cache_shm_count);

  // CommandBuffer implementation (called on client thread):
  State GetLastState() override;
  void Flush(int32_t put_offset) override;
  void OrderingBarrier(int32_t put_offset) override;
  State WaitForTokenInRange(int32_t start, int32_t end) override;
  State WaitForGetOffsetInRange(uint32_t set_get_buffer_count,
                                int32_t start,
                                int32_t end) override;
  void SetGetBuffer(int32_t shm_id) override;
  scoped_refptr<Buffer> CreateTransferBuffer(
      uint32_t size,
      int32_t* id,
      uint32_t alignment = 0,
      TransferBufferAllocationOption option =
          TransferBufferAllocationOption::kLoseContextOnOOM) override;
  void DestroyTransferBuffer(int32_t id) override;
  void ForceLostContext(error::ContextLostReason reason) override;

  // GpuControl implementation (called on client thread):
  void SetGpuControlClient(GpuControlClient*) override;
  // GetCapabilities() can be called on any thread.
  const Capabilities& GetCapabilities() const override;
  const GLCapabilities& GetGLCapabilities() const override;
  void SignalQuery(uint32_t query_id, base::OnceClosure callback) override;
  void CancelAllQueries() override;
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

  // CommandBufferServiceClient implementation (called on gpu thread):
  CommandBatchProcessedResult OnCommandBatchProcessed() override;
  void OnParseError() override;

  // DecoderClient implementation (called on gpu thread):
  void OnConsoleMessage(int32_t id, const std::string& message) override;
  void CacheBlob(gpu::GpuDiskCacheType type,
                 const std::string& key,
                 const std::string& shader) override;
  void OnFenceSyncRelease(uint64_t release) override;
  void OnDescheduleUntilFinished() override;
  void OnRescheduleAfterFinished() override;
  void OnSwapBuffers(uint64_t swap_id, uint32_t flags) override;
  void ScheduleGrContextCleanup() override;
  void HandleReturnData(base::span<const uint8_t> data) override;
  bool ShouldYield() override;

  const gles2::FeatureInfo* GetFeatureInfo() const;

  // Upstream this function to GpuControl if needs arise. Can be called on any
  // thread.
  const GpuFeatureInfo& GetGpuFeatureInfo() const;

  gpu::ServiceTransferCache* GetTransferCacheForTest() const;
  int GetRasterDecoderIdForTest() const;
  webgpu::WebGPUDecoder* GetWebGPUDecoderForTest() const;

  CommandBufferTaskExecutor* service_for_testing() const {
    return task_executor_;
  }

  gpu::SharedImageInterface* GetSharedImageInterface() const;

 private:
  struct InitializeOnGpuThreadParams {
    const raw_ref<const ContextCreationAttribs> attribs;
    raw_ptr<Capabilities> capabilities;       // Output.
    raw_ptr<GLCapabilities> gl_capabilities;  // Output.
    raw_ptr<gpu::raster::GrShaderCache> gr_shader_cache;
    raw_ptr<GpuProcessShmCount> use_shader_cache_shm_count;

    InitializeOnGpuThreadParams(const ContextCreationAttribs& attribs,
                                Capabilities* capabilities,
                                GLCapabilities* gl_capabilities,
                                gpu::raster::GrShaderCache* gr_shader_cache,
                                GpuProcessShmCount* use_shader_cache_shm_count)
        : attribs(attribs),
          capabilities(capabilities),
          gl_capabilities(gl_capabilities),
          gr_shader_cache(gr_shader_cache),
          use_shader_cache_shm_count(use_shader_cache_shm_count) {}
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

  void CreateCacheUse(
      std::optional<gles2::ProgramCache::ScopedCacheUse>& cache_use);

  // Client callbacks are posted back to |origin_task_runner_|, or run
  // synchronously if there's no task runner or message loop.
  void PostOrRunClientCallback(base::OnceClosure callback);
  base::OnceClosure WrapClientCallback(base::OnceClosure callback);

  void RunTaskOnGpuThread(base::OnceClosure task);

  using ReportingCallback =
      base::OnceCallback<void(base::TimeTicks task_ready)>;
  void ScheduleGpuTask(
      base::OnceClosure task,
      std::vector<SyncToken> sync_token_fences = std::vector<SyncToken>(),
      ReportingCallback report_callback = ReportingCallback());
  void ContinueGpuTask(base::OnceClosure task);

  void SignalSyncTokenOnGpuThread(const SyncToken& sync_token,
                                  base::OnceClosure callback);
  void SignalQueryOnGpuThread(unsigned query_id, base::OnceClosure callback);
  void CancelAllQueriesOnGpuThread();

  void RegisterTransferBufferOnGpuThread(int32_t id,
                                         scoped_refptr<Buffer> buffer);
  void DestroyTransferBufferOnGpuThread(int32_t id);
  void ForceLostContextOnGpuThread(error::ContextLostReason reason);

  void SetGetBufferOnGpuThread(int32_t shm_id, base::WaitableEvent* completion);

  void CreateGpuFenceOnGpuThread(uint32_t gpu_fence_id,
                                 gfx::GpuFenceHandle handle);
  void GetGpuFenceOnGpuThread(
      uint32_t gpu_fence_id,
      base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)> callback);

  // Sets |active_url_| as the active GPU process URL. Should be called on GPU
  // thread only.
  void UpdateActiveUrl();

  // Callbacks on the gpu thread.
  void PerformDelayedWorkOnGpuThread();

  // Callback implementations on the client thread.
  void OnContextLost();

  void HandleReturnDataOnOriginThread(std::vector<uint8_t> data);

  const CommandBufferId command_buffer_id_;
  const ContextUrl active_url_;

  // Members accessed on the gpu thread (possibly with the exception of
  // creation):
  bool use_virtualized_gl_context_ = false;
  raw_ptr<raster::GrShaderCache> gr_shader_cache_ = nullptr;
  scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner_;
  std::unique_ptr<CommandBufferService> command_buffer_;
  std::unique_ptr<DecoderContext> decoder_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<SyncPointClientState> sync_point_client_state_;

  // Used to throttle PerformDelayedWorkOnGpuThread.
  bool delayed_work_pending_ = false;
  // Sequence checker for tasks that run on the gpu "thread".
  SEQUENCE_CHECKER(gpu_sequence_checker_);

  // Members accessed on the client thread:
  raw_ptr<GpuControlClient> gpu_control_client_ = nullptr;
#if DCHECK_IS_ON()
  bool context_lost_ = false;
#endif
  State last_state_;
  base::Lock last_state_lock_;
  int32_t last_put_offset_ = -1;
  Capabilities capabilities_;
  GLCapabilities gl_capabilities_;
  uint64_t next_fence_sync_release_ = 1;
  std::vector<SyncToken> next_flush_sync_token_fences_;
  // Sequence checker for client sequence used for initialization, destruction,
  // callbacks, such as context loss, and methods which provide such callbacks,
  // such as SignalSyncToken.
  SEQUENCE_CHECKER(client_sequence_checker_);

  // Accessed on both threads:
  base::WaitableEvent flush_event_;
  const raw_ptr<CommandBufferTaskExecutor> task_executor_;

  // If no SingleTaskSequence is passed in, create our own.
  std::unique_ptr<GpuTaskSchedulerHelper> task_scheduler_holder_;

  // Pointer to the SingleTaskSequence that actually does the scheduling.
  raw_ptr<SingleTaskSequence> task_sequence_;
  scoped_refptr<SharedImageInterfaceInProcess> shared_image_interface_;

  // The group of contexts that share namespaces with this context.
  scoped_refptr<gles2::ContextGroup> context_group_;

  scoped_refptr<gl::GLShareGroup> gl_share_group_;
  base::WaitableEvent fence_sync_wait_event_;

  scoped_refptr<SharedContextState> context_state_;

  base::WeakPtr<InProcessCommandBuffer> client_thread_weak_ptr_;

  // Don't use |client_thread_weak_ptr_factory_| on GPU thread.  Use the cached
  // |client_thread_weak_ptr_| instead.
  base::WeakPtrFactory<InProcessCommandBuffer> client_thread_weak_ptr_factory_{
      this};
  base::WeakPtrFactory<InProcessCommandBuffer> gpu_thread_weak_ptr_factory_{
      this};
};

}  // namespace gpu

#endif  // GPU_IPC_IN_PROCESS_COMMAND_BUFFER_H_
