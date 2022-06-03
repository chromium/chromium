// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOCKS_LOCK_MANAGER_H_
#define CONTENT_BROWSER_LOCKS_LOCK_MANAGER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/locks/lock_manager.mojom.h"
#include "url/origin.h"

namespace content {

// One instance of this exists per StoragePartition, and services multiple
// child processes/origins. An instance must only be used on the sequence
// it was created on.
class LockManager : public blink::mojom::LockManager {
 public:
  LockManager();
  ~LockManager() override;

  LockManager(const LockManager&) = delete;
  LockManager& operator=(const LockManager&) = delete;

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

 private:
  // Internal representation of a lock request or held lock.
  class Lock;

  // State for a particular origin.
  class OriginState;

  // State for each client held in |receivers_|.
  struct ReceiverState {
    std::string client_id;

    // Origin of the frame or worker owning this receiver.
    url::Origin origin;
  };

  // Mints a monotonically increasing identifier. Used both for lock requests
  // and granted locks as keys in ordered maps.
  int64_t NextLockId();

  mojo::ReceiverSet<blink::mojom::LockManager, ReceiverState> receivers_;

  int64_t next_lock_id_ = 0;
  std::map<url::Origin, OriginState> origins_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<LockManager> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOCKS_LOCK_MANAGER_H_
