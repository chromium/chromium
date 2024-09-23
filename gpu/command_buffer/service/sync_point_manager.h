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
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/atomic_flag.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
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

// The cause of fence sync releases.
enum class ReleaseCause {
  // Releases done by clients explicitly during task execution.
  kExplicitClientRelease,
  // Releases done automatically at task completion, according to task info
  // specified by clients.
  kTaskCompletionRelease,
  // Releases done forcefully to resolve invalid waits.
  kForceRelease
};

class GPU_EXPORT SyncPointOrderData
    : public base::RefCountedThreadSafe<SyncPointOrderData> {
 public:
  SyncPointOrderData(const SyncPointOrderData&) = delete;
  SyncPointOrderData& operator=(const SyncPointOrderData&) = delete;

  // Helper function that calls SyncPointManager::DestroySyncPointOrderData.
  void Destroy() LOCKS_EXCLUDED(lock_);

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
  typedef std::
      priority_queue<OrderFence, std::vector<OrderFence>, std::greater<>>
          OrderFenceQueue;

  SyncPointOrderData(SyncPointManager* sync_point_manager,
                     SequenceId seqeunce_id);

  ~SyncPointOrderData();

  // Called by SyncPointManager after it has removed this SyncPointerOrderData
  // from its order_data_map_.
  void DestroyInternal() LOCKS_EXCLUDED(lock_);

  // Returns callback_id for created OrderFence on success, 0 on failure.
  uint64_t ValidateReleaseOrderNumber(
      scoped_refptr<SyncPointClientState> client_state,
      uint32_t wait_order_num,
      uint64_t fence_release) LOCKS_EXCLUDED(lock_);

  const raw_ptr<SyncPointManager> sync_point_manager_;

  const SequenceId sequence_id_;

  uint64_t current_callback_id_ GUARDED_BY(lock_) = 0;

  // Non thread-safe functions need to be called from a single thread.
  base::ThreadChecker processing_thread_checker_;

  // Current IPC order number being processed (only used on processing thread).
  uint32_t current_order_num_ = 0;

  // Whether or not the current order number is being processed or paused.
  bool paused_ = false;

  mutable base::Lock lock_;

  bool destroyed_ GUARDED_BY(lock_) = false;

  // Last finished IPC order number.
  uint32_t processed_order_num_ GUARDED_BY(lock_) = 0;

  // Last unprocessed order number. Updated in GenerateUnprocessedOrderNumber.
  uint32_t last_unprocessed_order_num_ GUARDED_BY(lock_) = 0;

  // Queue of unprocessed order numbers. Order numbers are enqueued in
  // GenerateUnprocessedOrderNumber, and dequeued in
  // FinishProcessingOrderNumber.
  base::queue<uint32_t> unprocessed_order_nums_ GUARDED_BY(lock_);

  // This variable is only used when graph-based validation is disabled.
  //
  // In situations where we are waiting on fence syncs that do not exist, we
  // validate by making sure the order number does not pass the order number
  // which the wait command was issued. If the order number reaches the
  // wait command's, we should automatically release up to the expected
  // release count. Note that this also releases other lower release counts,
  // so a single misbehaved fence sync is enough to invalidate/signal all
  // previous fence syncs. All order numbers (n) in order_fence_queue_ must
  // follow the invariant:
  //   unprocessed_order_nums_.front() < n <= unprocessed_order_nums_.back().
  OrderFenceQueue order_fence_queue_ GUARDED_BY(lock_);
};

class GPU_EXPORT SyncPointClientState
    : public base::RefCountedThreadSafe<SyncPointClientState> {
 public:
  SyncPointClientState(const SyncPointClientState&) = delete;
  SyncPointClientState& operator=(const SyncPointClientState&) = delete;

  // Calls SyncPointManager::DestroySyncPointClientState.
  void Destroy() LOCKS_EXCLUDED(fence_sync_lock_);

  CommandBufferNamespace namespace_id() const { return namespace_id_; }
  CommandBufferId command_buffer_id() const { return command_buffer_id_; }
  SequenceId sequence_id() const { return order_data_->sequence_id(); }

  // This behaves similarly to SyncPointManager::Wait but uses the order data
  // to guarantee no deadlocks with other clients. Must be called on order
  // number processing thread.
  bool Wait(const SyncToken& sync_token, base::OnceClosure callback)
      LOCKS_EXCLUDED(fence_sync_lock_);

  // Like Wait but runs the callback on the given task runner's thread. Must be
  // called on order number processing thread.
  // TODO(elgarawany): Rename this method to instead make it explicit that the
  // callback is going to run on |task_runner|.
  bool WaitNonThreadSafe(
      const SyncToken& sync_token,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::OnceClosure callback) LOCKS_EXCLUDED(fence_sync_lock_);

  // Release fence sync and run queued callbacks. Must be called on order number
  // processing thread.
  void ReleaseFenceSync(uint64_t release) LOCKS_EXCLUDED(fence_sync_lock_);

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
                              std::greater<>>
      ReleaseCallbackQueue;

  SyncPointClientState(SyncPointManager* sync_point_manager,
                       scoped_refptr<SyncPointOrderData> order_data,
                       CommandBufferNamespace namespace_id,
                       CommandBufferId command_buffer_id);

  ~SyncPointClientState();

  std::vector<base::OnceClosure> DestroyAndReturnCallbacks()
      LOCKS_EXCLUDED(fence_sync_lock_);

  // Returns true if fence sync has been released.
  bool IsFenceSyncReleased(uint64_t release) LOCKS_EXCLUDED(fence_sync_lock_);

  // Queues the callback to be called if the release is valid. If the release
  // is invalid this function will return False and the callback will never
  // be called.
  bool WaitForRelease(uint64_t release,
                      uint32_t wait_order_num,
                      base::OnceClosure callback)
      LOCKS_EXCLUDED(fence_sync_lock_);

  // Does not release the fence sync, but releases callbacks waiting on that
  // fence sync.
  void EnsureWaitReleased(uint64_t release, uint64_t callback_id)
      LOCKS_EXCLUDED(fence_sync_lock_);

  void EnsureFenceSyncReleased(uint64_t release, ReleaseCause cause)
      LOCKS_EXCLUDED(fence_sync_lock_);

  // Sync point manager is guaranteed to exist in the lifetime of the client.
  const raw_ptr<SyncPointManager> sync_point_manager_;

  // Global order data where releases will originate from.
  const scoped_refptr<SyncPointOrderData> order_data_;

  // Unique namespace/client id pair for this sync point client.
  const CommandBufferNamespace namespace_id_;
  const CommandBufferId command_buffer_id_;

  // Protects fence_sync_release_, fence_callback_queue_.
  base::Lock fence_sync_lock_;

  base::AtomicFlag destroyed_;

  // Current fence sync release that has been signaled.
  uint64_t fence_sync_release_ GUARDED_BY(fence_sync_lock_) = 0;

  // The fence sync release that has been signaled by clients, including both
  // ReleaseCause::kExplicitClientRelease and
  // ReleaseCause::kTaskCompletionRelease.
  // It is always true that
  // `client_fence_sync_release_` <= `fence_sync_release_`.
  // This variable is used to check that clients don't submit out of order
  // releases.
  uint64_t client_fence_sync_release_ GUARDED_BY(fence_sync_lock_) = 0;

  // In well defined fence sync operations, fence syncs are released in order
  // so simply having a priority queue for callbacks is enough.
  ReleaseCallbackQueue release_callback_queue_ GUARDED_BY(fence_sync_lock_);
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
  bool IsSyncTokenReleased(const SyncToken& sync_token) LOCKS_EXCLUDED(lock_);

  // Returns the sequence ID that will release this sync token.
  SequenceId GetSyncTokenReleaseSequenceId(const SyncToken& sync_token)
      LOCKS_EXCLUDED(lock_);

  // Returns the global last processed order number.
  uint32_t GetProcessedOrderNum() const LOCKS_EXCLUDED(lock_);

  // // Returns the global last unprocessed order number.
  uint32_t GetUnprocessedOrderNum() const LOCKS_EXCLUDED(lock_);

  // If the wait is valid (sync token hasn't been processed or command buffer
  // does not exist), the callback is queued to run when the sync point is
  // released. If the wait is invalid, the callback is NOT run. The callback
  // runs on the thread the sync point is released. Clients should use
  // SyncPointClient::Wait because that uses order data to prevent deadlocks.
  bool Wait(const SyncToken& sync_token,
            SequenceId sequence_id,
            uint32_t wait_order_num,
            base::OnceClosure callback) LOCKS_EXCLUDED(lock_);

  // Like Wait but runs the callback on the given task runner's thread.
  // TODO(elgarawany): Rename this method to instead make it explicit that the
  // callback is going to run on |task_runner|.
  bool WaitNonThreadSafe(
      const SyncToken& sync_token,
      SequenceId sequence_id,
      uint32_t wait_order_num,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::OnceClosure callback) LOCKS_EXCLUDED(lock_);

  // Used by SyncPointOrderData.
  uint32_t GenerateOrderNumber();

  // Is called by SyncPointOrderData::Destroy to remove `order_data` from
  // client_state_map_.
  void RemoveSyncPointOrderData(scoped_refptr<SyncPointOrderData> order_data)
      LOCKS_EXCLUDED(lock_);

  // Grabs any remaining callbacks in |client_state|'s release queue, destroys
  // |client_state|, then runs those remaining callbacks.
  void DestroySyncPointClientState(
      scoped_refptr<SyncPointClientState> client_state)
      LOCKS_EXCLUDED(lock_, client_state->fence_sync_lock_);

  // Ensures release count reaches `release`.
  void EnsureFenceSyncReleased(const SyncToken& release, ReleaseCause cause)
      LOCKS_EXCLUDED(lock_);

  // Whether to rely on gpu::TaskGraph (instead of SyncPointOrderData) to
  // perform sync point validation.
  bool graph_validation_enabled() const { return graph_validation_enabled_; }

  // There are debugging fatal logs to ensure that clients don't submit
  // out-of-order releases. Tests that would like to explicitly test such
  // invalid release sequences should use this flag to suppress those fatal
  // logs.
  //
  // This method doesn't handle multi-thread access. Caller should set the flag
  // early when no one is accessing this class from multiple threads.
  void set_suppress_fatal_log_for_testing() {
    suppress_fatal_log_for_testing_ = true;
  }

  bool suppress_fatal_log_for_testing() const {
    return suppress_fatal_log_for_testing_;
  }

 private:
  using ClientStateMap =
      base::flat_map<CommandBufferId, scoped_refptr<SyncPointClientState>>;

  using OrderDataMap =
      base::flat_map<SequenceId, scoped_refptr<SyncPointOrderData>>;

  scoped_refptr<SyncPointOrderData> GetSyncPointOrderData(
      SequenceId sequence_id) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  scoped_refptr<SyncPointClientState> GetSyncPointClientState(
      CommandBufferNamespace namespace_id,
      CommandBufferId command_buffer_id) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Internal version of GetSyncTokenReleaseSequenceId that requires lock to be
  // acquired.
  SequenceId GetSyncTokenReleaseSequenceIdInternal(const SyncToken& sync_token)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Order number is global for all clients.
  base::AtomicSequenceNumber order_num_generator_;

  // The following are protected by |lock_|.
  // Map of command buffer id to client state for each namespace.
  ClientStateMap client_state_maps_[NUM_COMMAND_BUFFER_NAMESPACES] GUARDED_BY(
      lock_);

  // Map of sequence id to order data.
  OrderDataMap order_data_map_ GUARDED_BY(lock_);

  SequenceId::Generator sequence_id_generator_ GUARDED_BY(lock_);

  mutable base::Lock lock_;

  const bool graph_validation_enabled_ = false;

  bool suppress_fatal_log_for_testing_ = false;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SYNC_POINT_MANAGER_H_
