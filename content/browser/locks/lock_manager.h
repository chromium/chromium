// Copyright 2017 The Chromium Authors
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
#include "components/services/storage/public/cpp/buckets/bucket_id.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/locks/lock_manager.mojom.h"
#include "url/origin.h"

namespace content {

// One instance of this exists per StoragePartition, and services multiple
// child processes/origins. An instance must only be used on the sequence
// it was created on.
class CONTENT_EXPORT LockManager : public blink::mojom::LockManager {
 public:
  LockManager();
  ~LockManager() override;

  LockManager(const LockManager&) = delete;
  LockManager& operator=(const LockManager&) = delete;

  // Binds |receiver| to this LockManager. |receiver| belongs to a frame or
  // worker at |bucket_id|.
  void BindReceiver(storage::BucketId bucket_id,
                    mojo::PendingReceiver<blink::mojom::LockManager> receiver);

  // Request a lock. When the lock is acquired, |callback| will be invoked with
  // a LockHandle.
  void RequestLock(const std::string& name,
                   blink::mojom::LockMode mode,
                   WaitMode wait,
                   mojo::PendingAssociatedRemote<blink::mojom::LockRequest>
                       request) override;

  // Called by a LockHandle's implementation when destructed.
  void ReleaseLock(storage::BucketId bucket_id, int64_t lock_id);

  // Called to request a snapshot of the current lock state for a bucket.
  void QueryState(QueryStateCallback callback) override;

 private:
  // Internal representation of a lock request or held lock.
  class Lock;

  // State for a particular bucket.
  class BucketState;

  // State for each client held in |receivers_|.
  struct ReceiverState {
    ReceiverState(std::string client_id, storage::BucketId bucket_id);
    ReceiverState();
    ReceiverState(const ReceiverState& other);
    ~ReceiverState();

    std::string client_id;

    // BucketId of the frame or worker owning this receiver.
    storage::BucketId bucket_id;
  };

  // Mints a monotonically increasing identifier. Used both for lock requests
  // and granted locks as keys in ordered maps.
  int64_t NextLockId();

  mojo::ReceiverSet<blink::mojom::LockManager, ReceiverState> receivers_;

  int64_t next_lock_id_ = 0;
  std::map<storage::BucketId, BucketState> buckets_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<LockManager> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOCKS_LOCK_MANAGER_H_
