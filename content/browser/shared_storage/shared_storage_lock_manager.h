// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_LOCK_MANAGER_H_
#define CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_LOCK_MANAGER_H_

#include "base/memory/raw_ref.h"
#include "content/browser/locks/lock_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "services/network/public/mojom/shared_storage.mojom-forward.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"

namespace content {

class StoragePartitionImpl;

// Manages the shared storage locks and the pending modifier methods that are
// protected by these locks. It also provides a convenient proxy method for
// invoking the shared storage modifier method that may or may not require
// acquiring a lock.
//
// NOTE: This class contains members used exclusively by the legacy batch update
//       implementation. When the transactional batch update is enabled
//       (controlled by a feature flag), these members are not used.
//       Specifically: `LegacyBatchUpdateState`,
//       `OnMethodWithinLegacyBatchUpdateFinished()`, and
//       `pending_legacy_batch_updates_`.
class CONTENT_EXPORT SharedStorageLockManager
    : public blink::mojom::LockRequest {
 public:
  using AccessScope = blink::SharedStorageAccessScope;
  using SharedStorageUpdateCallback =
      base::OnceCallback<void(const std::string&)>;
  using LockGrantedCallback =
      base::OnceCallback<void(mojo::AssociatedRemote<blink::mojom::LockHandle>,
                              mojo::Remote<blink::mojom::LockManager>)>;

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
      GlobalRenderFrameHostId main_frame_id,
      std::optional<int> worklet_ordinal_id,
      const base::UnguessableToken& worklet_devtools_token,
      SharedStorageUpdateCallback callback);

  // First, acquires the batch-level lock if requested (`with_lock` is present).
  // Then, under the batch-level lock, handles each method with any necessary
  // individual lock. Finally, releases the batch-level lock and invokes
  // `callback`.
  void SharedStorageBatchUpdate(
      std::vector<network::mojom::SharedStorageModifierMethodWithOptionsPtr>
          methods_with_options,
      const std::optional<std::string>& with_lock,
      const url::Origin& shared_storage_origin,
      AccessScope scope,
      GlobalRenderFrameHostId main_frame_id,
      std::optional<int> worklet_ordinal_id,
      const base::UnguessableToken& worklet_devtools_token,
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
        LockGrantedCallback lock_granted_callback,
        mojo::Remote<blink::mojom::LockManager> lock_manager);
    LockRequestReceiverState();
    LockRequestReceiverState(const LockRequestReceiverState& other) = delete;
    LockRequestReceiverState(LockRequestReceiverState&& other);
    ~LockRequestReceiverState();

    LockGrantedCallback lock_granted_callback;
    mojo::Remote<blink::mojom::LockManager> lock_manager;
  };

  struct LegacyBatchUpdateState {
    LegacyBatchUpdateState(
        size_t pending_updates_count,
        SharedStorageUpdateCallback callback,
        mojo::AssociatedRemote<blink::mojom::LockHandle> lock_handle,
        mojo::Remote<blink::mojom::LockManager> lock_manager);
    LegacyBatchUpdateState();
    LegacyBatchUpdateState(const LegacyBatchUpdateState& other) = delete;
    LegacyBatchUpdateState(LegacyBatchUpdateState&& other);
    ~LegacyBatchUpdateState();

    // The number of methods within the batch that haven't been sent to the
    // `SharedStorageManager` yet. This includes methods waiting for the batch
    // lock or the individual method lock.
    size_t unstarted_updates_count;

    // The number of methods within the batch that haven't received a final
    // response from the `SharedStorageManager`. This is always greater than or
    // equal to `unstarted_updates_count`.
    size_t unfinished_updates_count;

    // Indicates whether any method within the batch encountered an error during
    // processing by the `SharedStorageManager`.
    bool has_error = false;

    // The original callback provided by the client to be invoked after all
    // methods within the batch have been processed by the
    // `SharedStorageManager`.
    SharedStorageUpdateCallback callback;

    // The Mojo remote handle and lock manager used to acquire and hold the
    // batch lock. These are reset to release the lock after all methods within
    // the batch have been sent to the `SharedStorageManager`. If the lock isn't
    // needed, these will remain unbound.
    mojo::AssociatedRemote<blink::mojom::LockHandle> lock_handle;
    mojo::Remote<blink::mojom::LockManager> lock_manager;
  };

  void SharedStorageUpdateHelper(
      network::mojom::SharedStorageModifierMethodWithOptionsPtr
          method_with_options,
      const url::Origin& shared_storage_origin,
      AccessScope scope,
      GlobalRenderFrameHostId main_frame_id,
      std::optional<int> worklet_ordinal_id,
      const base::UnguessableToken& worklet_devtools_token,
      SharedStorageUpdateCallback callback,
      std::optional<int> legacy_batch_update_id);

  void OnReadyToHandleUpdate(
      network::mojom::SharedStorageModifierMethodPtr method,
      url::Origin shared_storage_origin,
      AccessScope scope,
      GlobalRenderFrameHostId main_frame_id,
      std::optional<int> worklet_ordinal_id,
      const base::UnguessableToken& worklet_devtools_token,
      SharedStorageUpdateCallback callback,
      std::optional<std::string> with_lock,
      std::optional<int> legacy_batch_update_id,
      mojo::AssociatedRemote<blink::mojom::LockHandle> lock_handle,
      mojo::Remote<blink::mojom::LockManager> lock_manager);

  void OnReadyToHandleBatchUpdate(
      std::vector<network::mojom::SharedStorageModifierMethodWithOptionsPtr>
          methods_with_options,
      url::Origin shared_storage_origin,
      AccessScope scope,
      GlobalRenderFrameHostId main_frame_id,
      std::optional<int> worklet_ordinal_id,
      const base::UnguessableToken& worklet_devtools_token,
      SharedStorageUpdateCallback callback,
      std::optional<std::string> with_lock,
      mojo::AssociatedRemote<blink::mojom::LockHandle> lock_handle,
      mojo::Remote<blink::mojom::LockManager> lock_manager);

  void OnMethodWithinLegacyBatchUpdateFinished(
      int legacy_batch_update_id,
      const std::string& error_message);

  void RequestLock(const url::Origin& shared_storage_origin,
                   const std::string& lock_name,
                   LockGrantedCallback lock_granted_callback);

  void NotifySharedStorageAccessed(
      const network::mojom::SharedStorageModifierMethodPtr& method,
      const url::Origin& shared_storage_origin,
      AccessScope scope,
      GlobalRenderFrameHostId main_frame_id,
      std::optional<int> worklet_ordinal_id,
      const base::UnguessableToken& worklet_devtools_token,
      std::optional<std::string> with_lock,
      std::optional<int> batch_update_id);

  // `storage_partition_` indirectly owns `this`, and thus outlives `this`.
  raw_ref<StoragePartitionImpl> storage_partition_;

  // Manages shared storage locks.
  LockManager<OriginLockGroupId> lock_manager_;

  // Manages LockRequest receivers and their associated state.
  mojo::AssociatedReceiverSet<blink::mojom::LockRequest,
                              LockRequestReceiverState>
      lock_request_receivers_;

  // A monotonically increasing ID assigned to each non-trivial
  // `SharedStorageBatchUpdate` (i.e., those with non-empty
  // `methods_with_options`). This ID is assigned during
  // `OnReadyToHandleBatchUpdate`, so may not correspond to the
  // original order of `SharedStorageBatchUpdate` requests.
  int next_batch_update_id_ = 0;

  // Stores the state of unfinished batch updates. The map is keyed by the batch
  // update ID.
  std::map<int, LegacyBatchUpdateState> pending_legacy_batch_updates_;

  base::WeakPtrFactory<SharedStorageLockManager> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_LOCK_MANAGER_H_
