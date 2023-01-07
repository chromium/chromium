// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SYNC_POINT_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SYNC_POINT_MANAGER_H_

#include <stdint.h>

#include <functional>
#include <memory>
#include <queue>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/gpu_export.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace gpu {

class SyncPointClient;
class SyncPointClientState;
class SyncPointManager;

class GPU_EXPORT SyncPointOrderData
    : public base::RefCountedThreadSafe<SyncPointOrderData> {
 public:
  SyncPointOrderData(const SyncPointOrderData&) = delete;
  SyncPointOrderData& operator=(const SyncPointOrderData&) = delete;

  void Destroy();

  SequenceId sequence_id() { return sequence_id_; }

  uint32_t processed_order_num() const {
    base::AutoLock auto_lock(lock_);
    return processed_order_num_;
  }

  uint32_t unprocessed_order_num() const {
    base::AutoLock auto_lock(lock_);
    return last_unprocessed_order_num_;
  }

  uint32_t current_order_num() const {
    DCHECK(processing_thread_checker_.CalledOnValidThread());
    return current_order_num_;
  }

  bool IsProcessingOrderNumber() {
    DCHECK(processing_thread_checker_.CalledOnValidThread());
    return !paused_ && current_order_num_ > processed_order_num();
  }

  uint32_t GenerateUnprocessedOrderNumber();
  void BeginProcessingOrderNumber(uint32_t order_num);
  void PauseProcessingOrderNumber(uint32_t order_num);
  void FinishProcessingOrderNumber(uint32_t order_num);

 private:
  friend class base::RefCountedThreadSafe<SyncPointOrderData>;
  friend class SyncPointManager;
  friend class SyncPointClientState;

  struct OrderFence {
    uint32_t order_num;
    uint64_t fence_release;
    scoped_refptr<SyncPointClientState> client_state;

    // ID that is unique to the particular SyncPointOrderData.
    uint64_t callback_id;

    OrderFence(uint32_t order,
               uint64_t release,
               scoped_refptr<SyncPointClientState> state,
               uint64_t callback_id);
    OrderFence(const OrderFence& other);
    ~OrderFence();

    bool operator>(const OrderFence& rhs) const {
      return std::tie(order_num, fence_release) >
             std::tie(rhs.order_num, rhs.fence_release);
    }
  };
  typedef std::priority_queue<OrderFence,
                              std::vector<OrderFence>,
                              std::greater<OrderFence>>
      OrderFenceQueue;

  SyncPointOrderData(SyncPointManager* sync_point_manager,
                     SequenceId seqeunce_id);

  ~SyncPointOrderData();

  // Returns callback_id for created OrderFence on success, 0 on failure.
  uint64_t ValidateReleaseOrderNumber(
      scoped_refptr<SyncPointClientState> client_state,
      uint32_t wait_order_num,
      uint64_t fence_release);

  const raw_ptr<SyncPointManager> sync_point_manager_;

  const SequenceId sequence_id_;

  uint64_t current_callback_id_ = 0;

  // Non thread-safe functions need to be called from a single thread.
  base::ThreadChecker processing_thread_checker_;

  // Current IPC order number being processed (only used on processing thread).
  uint32_t current_order_num_ = 0;

  // Whether or not the current order number is being processed or paused.
  bool paused_ = false;

  // This lock protects destroyed_, processed_order_num_,
  // unprocessed_order_nums_, and order_fence_queue_.
  mutable base::Lock lock_;

  bool destroyed_ = false;

  // Last finished IPC order number.
  uint32_t processed_order_num_ = 0;

  // Last unprocessed order number. Updated in GenerateUnprocessedOrderNumber.
  uint32_t last_unprocessed_order_num_ = 0;

  // Queue of unprocessed order numbers. Order numbers are enqueued in
  // GenerateUnprocessedOrderNumber, and dequeued in
  // FinishProcessingOrderNumber.
  base::queue<uint32_t> unprocessed_order_nums_;

  // In situations where we are waiting on fence syncs that do not exist, we
  // validate by making sure the order number does not pass the order number
  // which the wait command was issued. If the order number reaches the
  // wait command's, we should automatically release up to the expected
  // release count. Note that this also releases other lower release counts,
  // so a single misbehaved fence sync is enough to invalidate/signal all
  // previous fence syncs. All order numbers (n) in order_fence_queue_ must
  // follow the invariant:
  //   unprocessed_order_nums_.front() < n <= unprocessed_order_nums_.back().
  OrderFenceQueue order_fence_queue_;
};

class GPU_EXPORT SyncPointClientState
    : public base::RefCountedThreadSafe<SyncPointClientState> {
 public:
  SyncPointClientState(const SyncPointClientState&) = delete;
  SyncPointClientState& operator=(const SyncPointClientState&) = delete;

  void Destroy();

  CommandBufferNamespace namespace_id() const { return namespace_id_; }
  CommandBufferId command_buffer_id() const { return command_buffer_id_; }
  SequenceId sequence_id() const { return order_data_->sequence_id(); }

  // This behaves similarly to SyncPointManager::Wait but uses the order data
  // to guarantee no deadlocks with other clients. Must be called on order
  // number processing thread.
  bool Wait(const SyncToken& sync_token, base::OnceClosure callback);

  // Like Wait but runs the callback on the given task runner's thread. Must be
  // called on order number processing thread.
  bool WaitNonThreadSafe(
      const SyncToken& sync_token,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::OnceClosure callback);

  // Release fence sync and run queued callbacks. Must be called on order number
  // processing thread.
  void ReleaseFenceSync(uint64_t release);

 private:
  friend class base::RefCountedThreadSafe<SyncPointClientState>;
  friend class SyncPointManager;
  friend class SyncPointOrderData;

  struct ReleaseCallback {
    uint64_t release_count;
    base::OnceClosure callback_closure;
    uint64_t callback_id;

    ReleaseCallback(uint64_t release,
                    base::OnceClosure callback,
                    uint64_t callback_id);
    ReleaseCallback(ReleaseCallback&& other);
    ~ReleaseCallback();

    ReleaseCallback& operator=(ReleaseCallback&& other) = default;

    bool operator>(const ReleaseCallback& rhs) const {
      return release_count > rhs.release_count;
    }
  };
  typedef std::priority_queue<ReleaseCallback,
                              std::vector<ReleaseCallback>,
                              std::greater<ReleaseCallback>>
      ReleaseCallbackQueue;

  SyncPointClientState(SyncPointManager* sync_point_manager,
                       scoped_refptr<SyncPointOrderData> order_data,
                       CommandBufferNamespace namespace_id,
                       CommandBufferId command_buffer_id);

  ~SyncPointClientState();

  // Returns true if fence sync has been released.
  bool IsFenceSyncReleased(uint64_t release);

  // Queues the callback to be called if the release is valid. If the release
  // is invalid this function will return False and the callback will never
  // be called.
  bool WaitForRelease(uint64_t release,
                      uint32_t wait_order_num,
                      base::OnceClosure callback);

  // Does not release the fence sync, but releases callbacks waiting on that
  // fence sync.
  void EnsureWaitReleased(uint64_t release, uint64_t callback_id);

  void ReleaseFenceSyncHelper(uint64_t release);

  // Sync point manager is guaranteed to exist in the lifetime of the client.
  raw_ptr<SyncPointManager> sync_point_manager_ = nullptr;

  // Global order data where releases will originate from.
  scoped_refptr<SyncPointOrderData> order_data_;

  // Unique namespace/client id pair for this sync point client.
  const CommandBufferNamespace namespace_id_;
  const CommandBufferId command_buffer_id_;

  // Protects fence_sync_release_, fence_callback_queue_.
  base::Lock fence_sync_lock_;

  // Current fence sync release that has been signaled.
  uint64_t fence_sync_release_ = 0;

  // In well defined fence sync operations, fence syncs are released in order
  // so simply having a priority queue for callbacks is enough.
  ReleaseCallbackQueue release_callback_queue_;
};

// This class manages the sync points, which allow cross-channel
// synchronization.
class GPU_EXPORT SyncPointManager {
 public:
  SyncPointManager();

  SyncPointManager(const SyncPointManager&) = delete;
  SyncPointManager& operator=(const SyncPointManager&) = delete;

  ~SyncPointManager();

  scoped_refptr<SyncPointOrderData> CreateSyncPointOrderData();

  scoped_refptr<SyncPointClientState> CreateSyncPointClientState(
      CommandBufferNamespace namespace_id,
      CommandBufferId command_buffer_id,
      SequenceId sequence_id);

  // Returns true if the sync token has been released or if the command
  // buffer does not exist.
  bool IsSyncTokenReleased(const SyncToken& sync_token);

  // Returns the sequence ID that will release this sync token.
  SequenceId GetSyncTokenReleaseSequenceId(const SyncToken& sync_token);

  // Returns the global last processed order number.
  uint32_t GetProcessedOrderNum() const;

  // // Returns the global last unprocessed order number.
  uint32_t GetUnprocessedOrderNum() const;

  // If the wait is valid (sync token hasn't been processed or command buffer
  // does not exist), the callback is queued to run when the sync point is
  // released. If the wait is invalid, the callback is NOT run. The callback
  // runs on the thread the sync point is released. Clients should use
  // SyncPointClient::Wait because that uses order data to prevent deadlocks.
  bool Wait(const SyncToken& sync_token,
            SequenceId sequence_id,
            uint32_t wait_order_num,
            base::OnceClosure callback);

  // Like Wait but runs the callback on the given task runner's thread.
  bool WaitNonThreadSafe(
      const SyncToken& sync_token,
      SequenceId sequence_id,
      uint32_t wait_order_num,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::OnceClosure callback);

  // WaitOutOfOrder allows waiting for a sync token indefinitely, so it
  // should be used with trusted sync tokens only.
  bool WaitOutOfOrder(const SyncToken& trusted_sync_token,
                      base::OnceClosure callback);

  // Used by SyncPointOrderData.
  uint32_t GenerateOrderNumber();

  void DestroyedSyncPointOrderData(SequenceId sequence_id);

  void DestroyedSyncPointClientState(CommandBufferNamespace namespace_id,
                                     CommandBufferId command_buffer_id);

 private:
  using ClientStateMap =
      base::flat_map<CommandBufferId, scoped_refptr<SyncPointClientState>>;

  using OrderDataMap =
      base::flat_map<SequenceId, scoped_refptr<SyncPointOrderData>>;

  scoped_refptr<SyncPointOrderData> GetSyncPointOrderData(
      SequenceId sequence_id);

  scoped_refptr<SyncPointClientState> GetSyncPointClientState(
      CommandBufferNamespace namespace_id,
      CommandBufferId command_buffer_id);

  // Order number is global for all clients.
  base::AtomicSequenceNumber order_num_generator_;

  // The following are protected by |lock_|.
  // Map of command buffer id to client state for each namespace.
  ClientStateMap client_state_maps_[NUM_COMMAND_BUFFER_NAMESPACES];

  // Map of sequence id to order data.
  OrderDataMap order_data_map_;

  SequenceId::Generator sequence_id_generator_;

  mutable base::Lock lock_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SYNC_POINT_MANAGER_H_
