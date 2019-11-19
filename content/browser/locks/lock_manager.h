// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOCKS_LOCK_CONTEXT_H_
#define CONTENT_BROWSER_LOCKS_LOCK_CONTEXT_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/locks/lock_manager.mojom.h"
#include "url/origin.h"

namespace content {

// One instance of this exists per StoragePartition, and services multiple
// child processes/origins. An instance must only be used on the sequence
// it was created on.
class LockManager : public base::RefCountedThreadSafe<LockManager>,
                    public blink::mojom::LockManager {
 public:
  LockManager();

  // Binds |receiver| to this LockManager. |receiver| belongs to a frame or
  // worker at |origin| hosted by |render_process_id|. If it belongs to a frame,
  // |render_frame_id| identifies it, otherwise it is MSG_ROUTING_NONE.
  void BindReceiver(int render_process_id,
                    int render_frame_id,
                    const url::Origin& origin,
                    mojo::PendingReceiver<blink::mojom::LockManager> receiver);

  // Request a lock. When the lock is acquired, |callback| will be invoked with
  // a LockHandle.
  void RequestLock(const std::string& name,
                   blink::mojom::LockMode mode,
                   WaitMode wait,
                   mojo::PendingAssociatedRemote<blink::mojom::LockRequest>
                       request) override;

  // Called by a LockHandle's implementation when destructed.
  void ReleaseLock(const url::Origin& origin, int64_t lock_id);

  // Called to request a snapshot of the current lock state for an origin.
  void QueryState(QueryStateCallback callback) override;

 protected:
  friend class base::RefCountedThreadSafe<LockManager>;
  ~LockManager() override;

 private:
  // Internal representation of a lock request or held lock.
  class Lock;

  // State for a particular origin.
  class OriginState;

  // Describes a frame or a worker.
  struct ExecutionContext {
    // The identifier of the process hosting this frame or worker.
    int render_process_id;

    // The frame identifier, or MSG_ROUTING_NONE if this describes a worker
    // (this means that dedicated/shared/service workers are not distinguished).
    int render_frame_id;

    // Returns true if this is a worker.
    bool IsWorker() const;
  };

  // Comparator to use ExecutionContext in a map.
  struct ExecutionContextComparator {
    bool operator()(const ExecutionContext& left,
                    const ExecutionContext& right) const;
  };

  // State for each client held in |receivers_|.
  struct ReceiverState {
    std::string client_id;

    // ExecutionContext owning this receiver.
    ExecutionContext execution_context;

    // Origin of the frame or worker owning this receiver.
    url::Origin origin;
  };

  // Mints a monotonically increasing identifier. Used both for lock requests
  // and granted locks as keys in ordered maps.
  int64_t NextLockId();

  // Increments/decrements the number of locks held by the frame described by
  // |execution_context|. No-ops if |execution_context| describes a worker.
  void IncrementLocksHeldByFrame(const ExecutionContext& execution_context);
  void DecrementLocksHeldByFrame(const ExecutionContext& execution_context);

  mojo::ReceiverSet<blink::mojom::LockManager, ReceiverState> receivers_;

  int64_t next_lock_id_ = 0;
  std::map<url::Origin, OriginState> origins_;

  // Number of Locks held per frame.
  std::map<ExecutionContext, int, ExecutionContextComparator>
      num_locks_held_by_frame_{ExecutionContextComparator()};

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<LockManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LockManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOCKS_LOCK_CONTEXT_H
