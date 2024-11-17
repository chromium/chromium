// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_COMMAND_BUFFER_STUB_H_
#define GPU_IPC_SERVICE_COMMAND_BUFFER_STUB_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/stack_allocated.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "gpu/command_buffer/service/program_cache.h"
#include "gpu/command_buffer/service/scheduler_task_runner.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/command_buffer/service/task_graph.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/service/context_url.h"
#include "gpu/ipc/service/gpu_ipc_service_export.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/shared_associated_remote.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gpu_preference.h"
#include "url/gurl.h"

namespace gpu {
class DecoderContext;
class MemoryTracker;
struct SyncToken;
struct WaitForCommandState;
class GpuChannel;

// CommandBufferStub is a base class for different CommandBuffer backends
// (e.g. GLES2, Raster, WebGPU) within the GPU service. Each instance lives on
// the main thread and receives IPCs there, either dispatched to the default
// main thread TaskRunner, or a specific main-thread sequence on the GPU
// Scheduler.
//
// For every CommandBufferStub instance, there's a corresponding
// CommandBufferProxyImpl client.
class GPU_IPC_SERVICE_EXPORT CommandBufferStub
    : public CommandBufferServiceClient,
      public DecoderClient,
      public mojom::CommandBuffer {
 public:
  class DestructionObserver {
   public:
    // Called in Destroy(), before the context/surface are released.
    // If |have_context| is false, then the context cannot be made current, else
    // it already is.
    virtual void OnWillDestroyStub(bool have_context) = 0;

   protected:
    virtual ~DestructionObserver() = default;
  };

  CommandBufferStub(GpuChannel* channel,
                    const mojom::CreateCommandBufferParams& init_params,
                    CommandBufferId command_buffer_id,
                    SequenceId sequence_id,
                    int32_t stream_id,
                    int32_t route_id);

  CommandBufferStub(const CommandBufferStub&) = delete;
  CommandBufferStub& operator=(const CommandBufferStub&) = delete;

  ~CommandBufferStub() override;

  // Exposes a SequencedTaskRunner which can be used to schedule tasks in
  // sequence with this CommandBufferStub -- that is, on the same gpu::Scheduler
  // sequence. Does not support nested loops or delayed tasks.
  scoped_refptr<base::SequencedTaskRunner> task_runner() const {
    return scheduler_task_runner_;
  }

  // This must leave the GL context associated with the newly-created
  // CommandBufferStub current, so the GpuChannel can initialize
  // the gpu::Capabilities.
  virtual gpu::ContextResult Initialize(
      CommandBufferStub* share_group,
      const mojom::CreateCommandBufferParams& params,
      base::UnsafeSharedMemoryRegion shared_state_shm) = 0;

  // Establish Mojo bindings for the receiver and client endpoints.
  void BindEndpoints(
      mojo::PendingAssociatedReceiver<mojom::CommandBuffer> receiver,
      mojo::PendingAssociatedRemote<mojom::CommandBufferClient> client,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  MemoryTracker* GetMemoryTracker() const;
  virtual MemoryTracker* GetContextGroupMemoryTracker() const = 0;

  virtual base::WeakPtr<CommandBufferStub> AsWeakPtr() = 0;

  // Executes a DeferredRequest routed to this command buffer by a GpuChannel.
  void ExecuteDeferredRequest(mojom::DeferredCommandBufferRequestParams& params,
                              FenceSyncReleaseDelegate* release_delegate);

  // Instructs the CommandBuffer to wait asynchronously until the reader has
  // updated the token value to be within the [start, end] range (inclusive).
  // `callback` is invoked with the last known State once this occurs, or with
  // an invalid State if the CommandBuffer is destroyed first.
  using WaitForStateCallback =
      base::OnceCallback<void(const gpu::CommandBuffer::State&)>;
  void WaitForTokenInRange(int32_t start,
                           int32_t end,
                           WaitForStateCallback callback);

  // Instructs the CommandBuffer to wait asynchronously until the reader has
  // reached a get offset within the range [start, end] (inclusive). `callback`
  // is invoked with the last known State once this occurs, or with an invalid
  // State if the CommandBuffer is destroyed first.
  void WaitForGetOffsetInRange(uint32_t set_get_buffer_count,
                               int32_t start,
                               int32_t end,
                               WaitForStateCallback callback);

  // CommandBufferServiceClient implementation:
  CommandBatchProcessedResult OnCommandBatchProcessed() override;
  void OnParseError() override;

  // DecoderClient implementation:
  void OnConsoleMessage(int32_t id, const std::string& message) override;
  void CacheBlob(gpu::GpuDiskCacheType type,
                 const std::string& key,
                 const std::string& blob) override;
  void OnFenceSyncRelease(uint64_t release) override;
  void OnDescheduleUntilFinished() override;
  void OnRescheduleAfterFinished() override;
  void ScheduleGrContextCleanup() override;
  void HandleReturnData(base::span<const uint8_t> data) override;
  bool ShouldYield() override;

  using MemoryTrackerFactory =
      base::RepeatingCallback<std::unique_ptr<MemoryTracker>()>;

  // Overrides the way CreateMemoryTracker() uses to create a MemoryTracker.
  // This is intended for mocking the MemoryTracker in tests.
  static void SetMemoryTrackerFactoryForTesting(MemoryTrackerFactory factory);

  scoped_refptr<Buffer> GetTransferBuffer(int32_t id);
  void RegisterTransferBufferForTest(int32_t id, scoped_refptr<Buffer> buffer);

  // Whether this command buffer can currently handle IPC messages.
  bool IsScheduled();

  // Whether there are commands in the buffer that haven't been processed.
  bool HasUnprocessedCommands();

  DecoderContext* decoder_context() const { return decoder_context_.get(); }
  GpuChannel* channel() const { return channel_; }

  // Unique command buffer ID for this command buffer stub.
  CommandBufferId command_buffer_id() const { return command_buffer_id_; }

  SequenceId sequence_id() const { return sequence_id_; }

  int32_t stream_id() const { return stream_id_; }

  gl::GLSurface* surface() const { return surface_.get(); }

  ContextType context_type() const { return context_type_; }

  void AddDestructionObserver(DestructionObserver* observer);
  void RemoveDestructionObserver(DestructionObserver* observer);

  void MarkContextLost();

  scoped_refptr<gl::GLShareGroup> share_group() { return share_group_; }

 protected:
  // Scoper to help with setup and teardown boilerplate around operations which
  // may require the context to be current and which may need to process pending
  // queries or schedule other delayed work after completion. This makes the
  // context current on construction if possible.
  class ScopedContextOperation {
    STACK_ALLOCATED();

   public:
    explicit ScopedContextOperation(CommandBufferStub& stub);
    ~ScopedContextOperation();

    // Making the context current on construction may fail, in which case the
    // caller may wish to avoid doing work. This indicates whether it succeeded
    // or failed.
    bool is_context_current() const { return cache_use_.has_value(); }

   private:
    CommandBufferStub& stub_;
    bool have_context_ = false;
    std::optional<gles2::ProgramCache::ScopedCacheUse> cache_use_;
  };

  mojom::CommandBufferClient& client() { return *client_.get(); }

  // mojom::CommandBuffer:
  void SetGetBuffer(int32_t shm_id) override;
  void RegisterTransferBuffer(
      int32_t id,
      base::UnsafeSharedMemoryRegion transfer_buffer) override;
  void CreateGpuFenceFromHandle(uint32_t id,
                                gfx::GpuFenceHandle handle) override;
  void GetGpuFenceHandle(uint32_t id,
                         GetGpuFenceHandleCallback callback) override;
  void SignalSyncToken(const SyncToken& sync_token, uint32_t id) override;
  void SignalQuery(uint32_t query, uint32_t id) override;

  virtual void OnSetDefaultFramebufferSharedImage(const Mailbox& mailbox,
                                                  int samples_count,
                                                  bool preserve,
                                                  bool needs_depth,
                                                  bool needs_stencil) {}

  std::unique_ptr<MemoryTracker> CreateMemoryTracker() const;

  // Must be called during Initialize(). Takes ownership to co-ordinate
  // teardown in Destroy().
  void set_decoder_context(std::unique_ptr<DecoderContext> decoder_context) {
    decoder_context_ = std::move(decoder_context);
  }
  void CheckContextLost();

  // Sets |active_url_| as the active GPU process URL.
  void UpdateActiveUrl();

  bool MakeCurrent();

  bool offscreen() const {
#if BUILDFLAG(IS_ANDROID)
    return offscreen_;
#else
    return true;
#endif
  }

  // The lifetime of objects of this class is managed by a GpuChannel. The
  // GpuChannels destroy all the CommandBufferStubs that they own when
  // they are destroyed. So a raw pointer is safe.
  const raw_ptr<GpuChannel> channel_;

  ContextType context_type_;
  ContextUrl active_url_;

  bool initialized_;
#if BUILDFLAG(IS_ANDROID)
  const bool offscreen_;
#endif
  bool use_virtualized_gl_context_;

  std::unique_ptr<CommandBufferService> command_buffer_;

  // Have an ownership of the memory tracker used in children class. This is to
  // ensure that the memory tracker outlives the objects that uses it, for
  // example the ContextGroup referenced both in the Decoder and the
  // CommandBufferStub.
  std::unique_ptr<gpu::MemoryTracker> memory_tracker_;

  scoped_refptr<gl::GLSurface> surface_;
  ScopedSyncPointClientState scoped_sync_point_client_state_;
  scoped_refptr<gl::GLShareGroup> share_group_;

  const CommandBufferId command_buffer_id_;
  const SequenceId sequence_id_;
  const scoped_refptr<SchedulerTaskRunner> scheduler_task_runner_;
  const int32_t stream_id_;
  const int32_t route_id_;

 private:
  void Destroy();

  void CreateCacheUse(
      std::optional<gles2::ProgramCache::ScopedCacheUse>& cache_use);

  // Message handlers:
  void OnAsyncFlush(int32_t put_offset,
                    uint32_t flush_id,
                    const std::vector<SyncToken>& sync_token_fences);
  void OnDestroyTransferBuffer(int32_t id);

  void OnSignalAck(uint32_t id);

  void ReportState();

  // Poll the command buffer to execute work.
  void PollWork();
  void PerformWork();

  // Schedule processing of delayed work. This updates the time at which
  // delayed work should be processed. |process_delayed_work_time_| is
  // updated to current time + delay. Call this after processing some amount
  // of delayed work.
  void ScheduleDelayedWork(base::TimeDelta delay);

  void CheckCompleteWaits();

  // Set driver bug workarounds and disabled GL extensions to the context.
  static void SetContextGpuFeatureInfo(gl::GLContext* context,
                                       const GpuFeatureInfo& gpu_feature_info);

  static MemoryTrackerFactory GetMemoryTrackerFactory();

  // Overrides the way CreateMemoryTracker() uses to create a MemoryTracker. If
  // |factory| is base::NullCallback(), it returns the current
  // MemoryTrackerFactory (initially base::NullCallback() which
  // CreateMemoryTracker() should interpret as a signal to use the default).
  // This is intended for mocking the MemoryTracker in tests.
  static MemoryTrackerFactory SetOrGetMemoryTrackerFactory(
      MemoryTrackerFactory factory);

  std::unique_ptr<DecoderContext> decoder_context_;

  uint32_t last_flush_id_;

  base::ObserverList<DestructionObserver>::Unchecked destruction_observers_;

  base::DeadlineTimer process_delayed_work_timer_;
  uint32_t previous_processed_num_;
  base::TimeTicks last_idle_time_;

  std::unique_ptr<WaitForCommandState> wait_for_token_;
  std::unique_ptr<WaitForCommandState> wait_for_get_offset_;
  uint32_t wait_set_get_buffer_count_;

  mojo::AssociatedReceiver<mojom::CommandBuffer> receiver_{this};
  mojo::SharedAssociatedRemote<mojom::CommandBufferClient> client_;

  // Caching the `release_delegate` argument of ExecuteDeferredRequest() during
  // the call.
  raw_ptr<FenceSyncReleaseDelegate> release_delegate_ = nullptr;
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_COMMAND_BUFFER_STUB_H_
