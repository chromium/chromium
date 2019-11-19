// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_CLIENT_COMMAND_BUFFER_PROXY_IMPL_H_
#define GPU_IPC_CLIENT_COMMAND_BUFFER_PROXY_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/command_buffer_shared.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ipc/ipc_listener.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gpu_preference.h"

struct GPUCommandBufferConsoleMessage;
class GURL;

namespace gfx {
struct GpuFenceHandle;
struct PresentationFeedback;
}

namespace gpu {
struct ContextCreationAttribs;
struct Mailbox;
struct SwapBuffersCompleteParams;
struct SyncToken;
}

namespace gpu {
class GpuChannelHost;
class GpuMemoryBufferManager;

// Client side proxy that forwards messages synchronously to a
// CommandBufferStub.
class GPU_EXPORT CommandBufferProxyImpl : public gpu::CommandBuffer,
                                          public gpu::GpuControl,
                                          public IPC::Listener {
 public:
  class DeletionObserver {
   public:
    // Called during the destruction of the CommandBufferProxyImpl.
    virtual void OnWillDeleteImpl() = 0;

   protected:
    virtual ~DeletionObserver() = default;
  };

  CommandBufferProxyImpl(
      scoped_refptr<GpuChannelHost> channel,
      GpuMemoryBufferManager* gpu_memory_buffer_manager,
      int32_t stream_id,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~CommandBufferProxyImpl() override;

  // Connect to a command buffer in the GPU process.
  ContextResult Initialize(gpu::SurfaceHandle surface_handle,
                           CommandBufferProxyImpl* share_group,
                           gpu::SchedulingPriority stream_priority,
                           const gpu::ContextCreationAttribs& attribs,
                           const GURL& active_url);

  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelError() override;

  // CommandBuffer implementation:
  State GetLastState() override;
  void Flush(int32_t put_offset) override;
  void OrderingBarrier(int32_t put_offset) override;
  State WaitForTokenInRange(int32_t start, int32_t end) override;
  State WaitForGetOffsetInRange(uint32_t set_get_buffer_count,
                                int32_t start,
                                int32_t end) override;
  void SetGetBuffer(int32_t shm_id) override;
  scoped_refptr<gpu::Buffer> CreateTransferBuffer(uint32_t size,
                                                  int32_t* id) override;
  void DestroyTransferBuffer(int32_t id) override;

  // gpu::GpuControl implementation:
  void SetGpuControlClient(GpuControlClient* client) override;
  const gpu::Capabilities& GetCapabilities() const override;
  int32_t CreateImage(ClientBuffer buffer,
                      size_t width,
                      size_t height) override;
  void DestroyImage(int32_t id) override;
  void SignalQuery(uint32_t query, base::OnceClosure callback) override;
  void CreateGpuFence(uint32_t gpu_fence_id, ClientGpuFence source) override;
  void GetGpuFence(uint32_t gpu_fence_id,
                   base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)>
                       callback) override;
  void SetDisplayTransform(gfx::OverlayTransform transform) override;

  void SetLock(base::Lock* lock) override;
  void EnsureWorkVisible() override;
  gpu::CommandBufferNamespace GetNamespaceID() const override;
  gpu::CommandBufferId GetCommandBufferID() const override;
  void FlushPendingWork() override;
  uint64_t GenerateFenceSyncRelease() override;
  bool IsFenceSyncReleased(uint64_t release) override;
  void SignalSyncToken(const gpu::SyncToken& sync_token,
                       base::OnceClosure callback) override;
  void WaitSyncToken(const gpu::SyncToken& sync_token) override;
  bool CanWaitUnverifiedSyncToken(const gpu::SyncToken& sync_token) override;
  void TakeFrontBuffer(const gpu::Mailbox& mailbox);
  void ReturnFrontBuffer(const gpu::Mailbox& mailbox,
                         const gpu::SyncToken& sync_token,
                         bool is_lost);

  void AddDeletionObserver(DeletionObserver* observer);
  void RemoveDeletionObserver(DeletionObserver* observer);

  bool EnsureBackbuffer();

  using UpdateVSyncParametersCallback =
      base::RepeatingCallback<void(base::TimeTicks timebase,
                                   base::TimeDelta interval)>;
  void SetUpdateVSyncParametersCallback(
      const UpdateVSyncParametersCallback& callback);

  int32_t route_id() const { return route_id_; }

  const scoped_refptr<GpuChannelHost>& channel() const { return channel_; }

  const base::UnsafeSharedMemoryRegion& GetSharedStateRegion() const {
    return shared_state_shm_;
  }

 private:
  typedef std::map<int32_t, scoped_refptr<gpu::Buffer>> TransferBufferMap;
  typedef std::unordered_map<uint32_t, base::OnceClosure> SignalTaskMap;

  void CheckLock() {
    if (lock_) {
      lock_->AssertAcquired();
    } else {
      DCHECK(lockless_thread_checker_.CalledOnValidThread());
    }
  }

  void OrderingBarrierHelper(int32_t put_offset);

  // Send an IPC message over the GPU channel. This is private to fully
  // encapsulate the channel; all callers of this function must explicitly
  // verify that the context has not been lost.
  bool Send(IPC::Message* msg);

  std::pair<base::UnsafeSharedMemoryRegion, base::WritableSharedMemoryMapping>
  AllocateAndMapSharedMemory(size_t size);

  // Message handlers:
  void OnDestroyed(gpu::error::ContextLostReason reason,
                   gpu::error::Error error);
  void OnConsoleMessage(const GPUCommandBufferConsoleMessage& message);
  void OnGpuSwitched(gl::GpuPreference active_gpu_heuristic);
  void OnSignalAck(uint32_t id, const CommandBuffer::State& state);
  void OnSwapBuffersCompleted(const SwapBuffersCompleteParams& params);
  void OnBufferPresented(uint64_t swap_id,
                         const gfx::PresentationFeedback& feedback);
  void OnGetGpuFenceHandleComplete(uint32_t gpu_fence_id,
                                   const gfx::GpuFenceHandle&);
  void OnReturnData(const std::vector<uint8_t>& data);

  // Try to read an updated copy of the state from shared memory, and calls
  // OnGpuStateError() if the new state has an error.
  void TryUpdateState();
  // Like above but calls the error handler and disconnects channel by posting
  // a task.
  void TryUpdateStateThreadSafe();
  // Like the above but does not call the error event handler if the new state
  // has an error.
  void TryUpdateStateDontReportError();
  // Sets the state, and calls OnGpuStateError() if the new state has an error.
  void SetStateFromMessageReply(const CommandBuffer::State& state);

  // Loses the context after we received an invalid reply from the GPU
  // process.
  void OnGpuSyncReplyError();

  // Loses the context when receiving a message from the GPU process.
  void OnGpuAsyncMessageError(gpu::error::ContextLostReason reason,
                              gpu::error::Error error);

  // Loses the context after we receive an error state from the GPU process.
  void OnGpuStateError();

  // Sets an error on the last_state_ and loses the context due to client-side
  // errors.
  void OnClientError(gpu::error::Error error);

  // Helper methods, don't call these directly.
  void DisconnectChannelInFreshCallStack();
  void LockAndDisconnectChannel();
  void DisconnectChannel();

  // The shared memory area used to update state.
  gpu::CommandBufferSharedState* shared_state() const;

  // The shared memory region used to update state.
  base::UnsafeSharedMemoryRegion shared_state_shm_;
  base::WritableSharedMemoryMapping shared_state_mapping_;

  // The last cached state received from the service.
  State last_state_;

  // Lock to access shared state e.g. sync token release count across multiple
  // threads. This allows tracking command buffer progress from another thread.
  base::Lock last_state_lock_;

  // There should be a lock_ if this is going to be used across multiple
  // threads, or we guarantee it is used by a single thread by using a thread
  // checker if no lock_ is set.
  base::Lock* lock_ = nullptr;
  base::ThreadChecker lockless_thread_checker_;

  // Client that wants to listen for important events on the GpuControl.
  gpu::GpuControlClient* gpu_control_client_ = nullptr;

  // Unowned list of DeletionObservers.
  base::ObserverList<DeletionObserver>::Unchecked deletion_observers_;

  scoped_refptr<GpuChannelHost> channel_;
  GpuMemoryBufferManager* gpu_memory_buffer_manager_;
  bool disconnected_ = false;
  const int channel_id_;
  const int32_t route_id_;
  const int32_t stream_id_;
  const gpu::CommandBufferId command_buffer_id_;
  uint32_t last_flush_id_ = 0;
  int32_t last_put_offset_ = -1;
  bool has_buffer_ = false;

  // Next generated fence sync.
  uint64_t next_fence_sync_release_ = 1;

  // Sync token waits that haven't been flushed yet.
  std::vector<SyncToken> pending_sync_token_fences_;

  // Tasks to be invoked in SignalSyncPoint responses.
  uint32_t next_signal_id_ = 0;
  SignalTaskMap signal_tasks_;

  gpu::Capabilities capabilities_;

  UpdateVSyncParametersCallback update_vsync_parameters_completion_callback_;

  using GetGpuFenceTaskMap =
      base::flat_map<uint32_t,
                     base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)>>;
  GetGpuFenceTaskMap get_gpu_fence_tasks_;

  scoped_refptr<base::SingleThreadTaskRunner> callback_thread_;
  base::WeakPtrFactory<CommandBufferProxyImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CommandBufferProxyImpl);
};

}  // namespace gpu

#endif  // GPU_IPC_CLIENT_COMMAND_BUFFER_PROXY_IMPL_H_
