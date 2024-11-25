// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_LOCK_MANAGER_H_
#define CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_LOCK_MANAGER_H_

#include "base/memory/raw_ref.h"
#include "content/browser/locks/lock_manager.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "services/network/public/mojom/shared_storage.mojom-forward.h"

namespace content {

class StoragePartitionImpl;

// Manages the shared storage locks and the pending modifier methods that are
// protected by these locks. It also provides a convenient proxy method for
// invoking the shared storage modifier method that may or may not require
// acquiring a lock.
class CONTENT_EXPORT SharedStorageLockManager
    : public blink::mojom::LockRequest {
 public:
  using SharedStorageUpdateCallback =
      base::OnceCallback<void(const std::string&)>;

  enum AccessScope {
    kWindow,
    kSharedStorageWorklet,
    kHeader,
  };

  explicit SharedStorageLockManager(StoragePartitionImpl& storage_partition);
  ~SharedStorageLockManager() override;

  // Invokes `lock_manager_.BindReceiver()` with the given parameters.
  void BindLockManager(
      const url::Origin& shared_storage_origin,
      mojo::PendingReceiver<blink::mojom::LockManager> receiver);

  // Invokes the provided shared storage modifier method and handles the
  // necessary locking. If the method requires a lock, this function will
  // acquire it via `lock_manager_` and will update the state of
  // `lock_request_receivers_` accordingly.
  void SharedStorageUpdate(
      network::mojom::SharedStorageModifierMethodWithOptionsPtr
          method_with_options,
      const url::Origin& shared_storage_origin,
      AccessScope scope,
      FrameTreeNodeId main_frame_id,
      SharedStorageUpdateCallback callback);

  // blink::mojom::LockRequest
  void Granted(mojo::PendingAssociatedRemote<blink::mojom::LockHandle>
                   pending_handle) override;
  void Failed() override;

 private:
  // Represents an origin for use as a lock group ID.
  // This wraps the serialized origin and provides a trivial `is_null()` method
  // to satisfy the requirements of the `LockManager` template class.
  struct OriginLockGroupId {
    explicit OriginLockGroupId(const url::Origin& origin)
        : origin(origin.Serialize()) {}

    bool is_null() const { return false; }

    bool operator<(const OriginLockGroupId& other) const {
      return origin < other.origin;
    }

    std::string origin;
  };

  // State for each client held in `lock_request_receivers_`.
  struct LockRequestReceiverState {
    LockRequestReceiverState(
        base::OnceClosure lock_granted_callback,
        mojo::Remote<blink::mojom::LockManager> lock_manager);
    LockRequestReceiverState();
    LockRequestReceiverState(const LockRequestReceiverState& other) = delete;
    LockRequestReceiverState(LockRequestReceiverState&& other);
    ~LockRequestReceiverState();

    base::OnceClosure lock_granted_callback;
    mojo::Remote<blink::mojom::LockManager> lock_manager;
  };

  void OnReadyToHandleUpdate(
      network::mojom::SharedStorageModifierMethodPtr method,
      url::Origin shared_storage_origin,
      AccessScope scope,
      FrameTreeNodeId main_frame_id,
      SharedStorageUpdateCallback callback);

  // `storage_partition_` indirectly owns `this`, and thus outlives `this`.
  raw_ref<StoragePartitionImpl> storage_partition_;

  // Manages shared storage locks.
  LockManager<OriginLockGroupId> lock_manager_;

  // Manages LockRequest receivers and their associated state.
  mojo::AssociatedReceiverSet<blink::mojom::LockRequest,
                              LockRequestReceiverState>
      lock_request_receivers_;

  base::WeakPtrFactory<SharedStorageLockManager> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_LOCK_MANAGER_H_
