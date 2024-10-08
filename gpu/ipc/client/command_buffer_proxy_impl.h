// Copyright 2012 The Chromium Authors
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
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/command_buffer_shared.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "gpu/ipc/common/surface_handle.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/shared_associated_remote.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gpu_preference.h"

class GURL;

namespace base {
class HistogramBase;
}

namespace gfx {
struct GpuFenceHandle;
}

namespace gpu {
struct ContextCreationAttribs;
struct Mailbox;
struct SyncToken;
}

namespace gpu {
class GpuChannelHost;
class GpuMemoryBufferManager;

// Client side proxy that forwards messages synchronously to a
// CommandBufferStub.
class GPU_EXPORT CommandBufferProxyImpl : public gpu::CommandBuffer,
                                          public gpu::GpuControl,
                                          public mojom::CommandBufferClient {
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
      int32_t stream_id,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      base::SharedMemoryMapper* transfer_buffer_mapper = nullptr);

  CommandBufferProxyImpl(const CommandBufferProxyImpl&) = delete;
  CommandBufferProxyImpl& operator=(const CommandBufferProxyImpl&) = delete;

  ~CommandBufferProxyImpl() override;

  // Connect to a command buffer in the GPU process.
  ContextResult Initialize(gpu::SurfaceHandle surface_handle,
                           CommandBufferProxyImpl* share_group,
                           gpu::SchedulingPriority stream_priority,
                           const gpu::ContextCreationAttribs& attribs,
                           const GURL& active_url);

  void OnDisconnect();

  // CommandBuffer implementation:
  State GetLastState() override;
  void Flush(int32_t put_offset) override;
  void OrderingBarrier(int32_t put_offset) override;
  State WaitForTokenInRange(int32_t start, int32_t end) override;
  State WaitForGetOffsetInRange(uint32_t set_get_buffer_count,
                                int32_t start,
                                int32_t end) override;
  void SetGetBuffer(int32_t shm_id) override;
  scoped_refptr<gpu::Buffer> CreateTransferBuffer(
      uint32_t size,
      int32_t* id,
      uint32_t alignment = 0,
      TransferBufferAllocationOption option =
          TransferBufferAllocationOption::kLoseContextOnOOM) override;
  void DestroyTransferBuffer(int32_t id) override;
  void ForceLostContext(error::ContextLostReason reason) override;

  // gpu::GpuControl implementation:
  void SetGpuControlClient(GpuControlClient* client) override;
  const gpu::Capabilities& GetCapabilities() const override;
  const gpu::GLCapabilities& GetGLCapabilities() const override;
  void SignalQuery(uint32_t query, base::OnceClosure callback) override;
  void CancelAllQueries() override;
  void CreateGpuFence(uint32_t gpu_fence_id, ClientGpuFence source) override;
  void GetGpuFence(uint32_t gpu_fence_id,
                   base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)>
                       callback) override;

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
  void SetDefaultFramebufferSharedImage(const gpu::Mailbox& mailbox,
                                        const gpu::SyncToken& sync_token,
                                        int samples_count,
                                        bool preserve,
                                        bool needs_depth,
                                        bool needs_stencil);
  void AddDeletionObserver(DeletionObserver* observer);
  void RemoveDeletionObserver(DeletionObserver* observer);

  int32_t route_id() const { return route_id_; }

  const scoped_refptr<GpuChannelHost>& channel() const { return channel_; }

  mojom::GpuChannel& GetGpuChannel() const {
    return channel()->GetGpuChannel();
  }

  const base::UnsafeSharedMemoryRegion& GetSharedStateRegion() const {
    return shared_state_shm_;
  }

  // Used in cases where fence sync releases are not directly generated from
  // this class itself.
  void UpdateLastFenceSyncRelease(uint64_t release_count);

 private:
  typedef std::map<int32_t, scoped_refptr<gpu::Buffer>> TransferBufferMap;
  typedef std::unordered_map<uint32_t, base::OnceClosure> SignalTaskMap;

  void CheckLock() {
    if (lock_) {
      lock_->AssertAcquired();
    } else {
      DCHECK_CALLED_ON_VALID_SEQUENCE(lockless_sequence_checker_);
    }
  }

  void OrderingBarrierHelper(int32_t put_offset);

  std::pair<base::UnsafeSharedMemoryRegion, base::WritableSharedMemoryMapping>
  AllocateAndMapSharedMemory(size_t size,
                             base::SharedMemoryMapper* mapper = nullptr);

  // mojom::CommandBufferClient:
  void OnConsoleMessage(const std::string& message) override;
  void OnGpuSwitched(gl::GpuPreference active_gpu_heuristic) override;
  void OnDestroyed(gpu::error::ContextLostReason reason,
                   gpu::error::Error error) override;
  void OnReturnData(const std::vector<uint8_t>& data) override;
  void OnSignalAck(uint32_t id, const CommandBuffer::State& state) override;

  void OnGetGpuFenceHandleComplete(
      uint32_t gpu_fence_id,
      base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)> callback,
      gfx::GpuFenceHandle);

  // Try to read an updated copy of the state from shared memory, and calls
  // OnGpuStateError() if the new state has an error.
  void TryUpdateState() EXCLUSIVE_LOCKS_REQUIRED(last_state_lock_);
  // Like above but calls the error handler and disconnects channel by posting
  // a task.
  void TryUpdateStateThreadSafe() EXCLUSIVE_LOCKS_REQUIRED(last_state_lock_);
  // Like the above but does not call the error event handler if the new state
  // has an error.
  void TryUpdateStateDontReportError()
      EXCLUSIVE_LOCKS_REQUIRED(last_state_lock_);
  // Sets the state, and calls OnGpuStateError() if the new state has an error.
  void SetStateFromMessageReply(const CommandBuffer::State& state)
      EXCLUSIVE_LOCKS_REQUIRED(last_state_lock_);

  // Loses the context after we received an invalid reply from the GPU
  // process.
  void OnGpuSyncReplyError() EXCLUSIVE_LOCKS_REQUIRED(last_state_lock_);

  // Loses the context when receiving a message from the GPU process.
  void OnGpuAsyncMessageError(gpu::error::ContextLostReason reason,
                              gpu::error::Error error)
      EXCLUSIVE_LOCKS_REQUIRED(last_state_lock_);

  // Loses the context after we receive an error state from the GPU process.
  void OnGpuStateError() EXCLUSIVE_LOCKS_REQUIRED(last_state_lock_);

  // Sets an error on the last_state_ and loses the context due to client-side
  // errors.
  void OnClientError(gpu::error::Error error)
      EXCLUSIVE_LOCKS_REQUIRED(last_state_lock_);

  // Helper methods, don't call these directly.
  void DisconnectChannelInFreshCallStack()
      EXCLUSIVE_LOCKS_REQUIRED(last_state_lock_);
  void LockAndDisconnectChannel();
  void DisconnectChannel();

  // The shared memory area used to update state.
  gpu::CommandBufferSharedState* shared_state() const;

  base::HistogramBase* GetUMAHistogramEnsureWorkVisibleDuration();

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
  raw_ptr<base::Lock> lock_ = nullptr;
  base::SequenceChecker lockless_sequence_checker_;

  // Client that wants to listen for important events on the GpuControl.
  raw_ptr<gpu::GpuControlClient> gpu_control_client_ = nullptr;

  // Unowned list of DeletionObservers.
  base::ObserverList<DeletionObserver>::Unchecked deletion_observers_;

  scoped_refptr<GpuChannelHost> channel_;
  bool disconnected_ = false;
  const int channel_id_;
  const int32_t route_id_;
  const int32_t stream_id_;
  const gpu::CommandBufferId command_buffer_id_;
  uint32_t last_flush_id_ = 0;
  int32_t last_put_offset_ = -1;
  bool has_buffer_ = false;

  mojo::SharedAssociatedRemote<mojom::CommandBuffer> command_buffer_;
  mojo::AssociatedReceiver<mojom::CommandBufferClient> client_receiver_{this};

  // Last generated fence sync.
  uint64_t last_fence_sync_release_ = 0;

  // Sync token waits that haven't been flushed yet.
  std::vector<SyncToken> pending_sync_token_fences_;

  // Tasks to be invoked in SignalSyncPoint responses.
  uint32_t next_signal_id_ = 0;
  SignalTaskMap signal_tasks_;

  gpu::Capabilities capabilities_;
  gpu::GLCapabilities gl_capabilities_;

  // Cache pointer to EnsureWorkVisibleDuration custom UMA histogram.
  raw_ptr<base::HistogramBase> uma_histogram_ensure_work_visible_duration_ =
      nullptr;

  scoped_refptr<base::SequencedTaskRunner> callback_thread_;

  // Optional shared memory mapper to use when creating transfer buffers.
  // TODO(crbug.com/40837434) remove this member and instead let callers of
  // CreateTransferBuffer specify the mapper to use so that only the buffers
  // used for WebGPU ArrayBuffers use a non-default mapper.
  raw_ptr<base::SharedMemoryMapper> transfer_buffer_mapper_;

  base::WeakPtrFactory<CommandBufferProxyImpl> weak_ptr_factory_{this};
};

}  // namespace gpu

#endif  // GPU_IPC_CLIENT_COMMAND_BUFFER_PROXY_IMPL_H_
