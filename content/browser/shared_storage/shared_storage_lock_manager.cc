// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_lock_manager.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/services/storage/shared_storage/shared_storage_database.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/browser/shared_storage/shared_storage_runtime_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/shared_storage.mojom.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

using AccessScope = blink::SharedStorageAccessScope;
using AccessMethod =
    SharedStorageRuntimeManager::SharedStorageObserverInterface::AccessMethod;
using OperationResult = storage::SharedStorageManager::OperationResult;
using BatchUpdateResult = storage::SharedStorageManager::BatchUpdateResult;

AccessMethod GetAccessMethod(
    const network::mojom::SharedStorageModifierMethodPtr& method) {
  switch (method->which()) {
    case network::mojom::SharedStorageModifierMethod::Tag::kSetMethod:
      return AccessMethod::kSet;
    case network::mojom::SharedStorageModifierMethod::Tag::kAppendMethod:
      return AccessMethod::kAppend;
    case network::mojom::SharedStorageModifierMethod::Tag::kDeleteMethod:
      return AccessMethod::kDelete;
    case network::mojom::SharedStorageModifierMethod::Tag::kClearMethod:
      return AccessMethod::kClear;
  }

  NOTREACHED();
}

constexpr char kBatchUpdateErrorMessage[] =
    "sharedStorage.batchUpdate() failed";

}  // namespace

SharedStorageLockManager::SharedStorageLockManager(
    StoragePartitionImpl& storage_partition)
    : storage_partition_(storage_partition) {}

SharedStorageLockManager::~SharedStorageLockManager() = default;

void SharedStorageLockManager::BindLockManager(
    const url::Origin& shared_storage_origin,
    mojo::PendingReceiver<blink::mojom::LockManager> receiver) {
  lock_manager_.BindReceiver(OriginLockGroupId(shared_storage_origin),
                             std::move(receiver));
}

void SharedStorageLockManager::SharedStorageUpdate(
    network::mojom::SharedStorageModifierMethodWithOptionsPtr
        method_with_options,
    const url::Origin& shared_storage_origin,
    AccessScope scope,
    GlobalRenderFrameHostId main_frame_id,
    std::optional<int> worklet_ordinal_id,
    const base::UnguessableToken& worklet_devtools_token,
    SharedStorageUpdateCallback callback) {
  base::UmaHistogramBoolean("Storage.SharedStorage.UpdateMethod.HasLockOption",
                            !!method_with_options->with_lock);

  SharedStorageUpdateHelper(std::move(method_with_options),
                            shared_storage_origin, scope, main_frame_id,
                            worklet_ordinal_id, worklet_devtools_token,
                            std::move(callback),
                            /*legacy_batch_update_id=*/std::nullopt);
}

void SharedStorageLockManager::SharedStorageBatchUpdate(
    std::vector<network::mojom::SharedStorageModifierMethodWithOptionsPtr>
        methods_with_options,
    const std::optional<std::string>& with_lock,
    const url::Origin& shared_storage_origin,
    AccessScope scope,
    GlobalRenderFrameHostId main_frame_id,
    std::optional<int> worklet_ordinal_id,
    const base::UnguessableToken& worklet_devtools_token,
    SharedStorageUpdateCallback callback) {
  base::UmaHistogramBoolean(
      "Storage.SharedStorage.BatchUpdateMethod.HasLockOption", !!with_lock);

  auto ready_to_handle_batch_update_callback = base::BindOnce(
      &SharedStorageLockManager::OnReadyToHandleBatchUpdate,
      weak_ptr_factory_.GetWeakPtr(), std::move(methods_with_options),
      shared_storage_origin, scope, main_frame_id, worklet_ordinal_id,
      worklet_devtools_token, std::move(callback), with_lock);

  if (!with_lock ||
      !base::FeatureList::IsEnabled(blink::features::kSharedStorageWebLocks)) {
    std::move(ready_to_handle_batch_update_callback)
        .Run(mojo::AssociatedRemote<blink::mojom::LockHandle>(),
             mojo::Remote<blink::mojom::LockManager>());
    return;
  }

  RequestLock(shared_storage_origin, with_lock.value(),
              std::move(ready_to_handle_batch_update_callback));
}

void SharedStorageLockManager::Granted(
    mojo::PendingAssociatedRemote<blink::mojom::LockHandle> pending_handle) {
  mojo::AssociatedRemote<blink::mojom::LockHandle> handle;
  handle.Bind(std::move(pending_handle));

  auto& current_context = lock_request_receivers_.current_context();

  // Pass the lock state to `lock_granted_callback`. This callback will
  // determine whether to keep holding the lock (e.g., for in-progress batch
  // updates), or to release it immediately.
  std::move(current_context.lock_granted_callback)
      .Run(std::move(handle), std::move(current_context.lock_manager));
}

void SharedStorageLockManager::Failed() {
  // With WaitMode::WAIT, `Failed()` will never trigger.
  NOTREACHED();
}

SharedStorageLockManager::LockRequestReceiverState::LockRequestReceiverState(
    LockGrantedCallback lock_granted_callback,
    mojo::Remote<blink::mojom::LockManager> lock_manager)
    : lock_granted_callback(std::move(lock_granted_callback)),
      lock_manager(std::move(lock_manager)) {}

SharedStorageLockManager::LockRequestReceiverState::LockRequestReceiverState() =
    default;
SharedStorageLockManager::LockRequestReceiverState::LockRequestReceiverState(
    LockRequestReceiverState&& other) = default;
SharedStorageLockManager::LockRequestReceiverState::
    ~LockRequestReceiverState() = default;

SharedStorageLockManager::LegacyBatchUpdateState::LegacyBatchUpdateState(
    size_t pending_updates_count,
    SharedStorageUpdateCallback callback,
    mojo::AssociatedRemote<blink::mojom::LockHandle> lock_handle,
    mojo::Remote<blink::mojom::LockManager> lock_manager)
    : unstarted_updates_count(pending_updates_count),
      unfinished_updates_count(pending_updates_count),
      callback(std::move(callback)),
      lock_handle(std::move(lock_handle)),
      lock_manager(std::move(lock_manager)) {}

SharedStorageLockManager::LegacyBatchUpdateState::LegacyBatchUpdateState() =
    default;
SharedStorageLockManager::LegacyBatchUpdateState::LegacyBatchUpdateState(
    LegacyBatchUpdateState&& other) = default;
SharedStorageLockManager::LegacyBatchUpdateState::~LegacyBatchUpdateState() =
    default;

void SharedStorageLockManager::SharedStorageUpdateHelper(
    network::mojom::SharedStorageModifierMethodWithOptionsPtr
        method_with_options,
    const url::Origin& shared_storage_origin,
    AccessScope scope,
    GlobalRenderFrameHostId main_frame_id,
    std::optional<int> worklet_ordinal_id,
    const base::UnguessableToken& worklet_devtools_token,
    SharedStorageUpdateCallback callback,
    std::optional<int> legacy_batch_update_id) {
  const std::optional<std::string>& with_lock = method_with_options->with_lock;

  auto ready_to_handle_update_callback = base::BindOnce(
      &SharedStorageLockManager::OnReadyToHandleUpdate,
      weak_ptr_factory_.GetWeakPtr(), std::move(method_with_options->method),
      shared_storage_origin, scope, main_frame_id, worklet_ordinal_id,
      worklet_devtools_token, std::move(callback), with_lock,
      std::move(legacy_batch_update_id));

  if (!with_lock ||
      !base::FeatureList::IsEnabled(blink::features::kSharedStorageWebLocks)) {
    std::move(ready_to_handle_update_callback)
        .Run(mojo::AssociatedRemote<blink::mojom::LockHandle>(),
             mojo::Remote<blink::mojom::LockManager>());
    return;
  }

  RequestLock(shared_storage_origin, with_lock.value(),
              std::move(ready_to_handle_update_callback));
}

void SharedStorageLockManager::OnReadyToHandleUpdate(
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
    mojo::Remote<blink::mojom::LockManager> lock_manager) {
  NotifySharedStorageAccessed(
      method, shared_storage_origin, scope, main_frame_id, worklet_ordinal_id,
      worklet_devtools_token, with_lock, legacy_batch_update_id);

  switch (method->which()) {
    case network::mojom::SharedStorageModifierMethod::Tag::kSetMethod: {
      network::mojom::SharedStorageSetMethodPtr& set_method =
          method->get_set_method();

      storage::SharedStorageDatabase::SetBehavior set_behavior =
          set_method->ignore_if_present
              ? storage::SharedStorageDatabase::SetBehavior::kIgnoreIfPresent
              : storage::SharedStorageDatabase::SetBehavior::kDefault;

      auto completed_callback = base::BindOnce(
          [](SharedStorageUpdateCallback callback, OperationResult result) {
            if (result != OperationResult::kSet &&
                result != OperationResult::kIgnored) {
              std::move(callback).Run(
                  /*error_message=*/"sharedStorage.set() failed");
              return;
            }

            std::move(callback).Run(/*error_message=*/{});
          },
          std::move(callback));

      storage_partition_->GetSharedStorageManager()->Set(
          shared_storage_origin, set_method->key, set_method->value,
          std::move(completed_callback), set_behavior);
      break;
    }
    case network::mojom::SharedStorageModifierMethod::Tag::kAppendMethod: {
      network::mojom::SharedStorageAppendMethodPtr& append_method =
          method->get_append_method();

      auto completed_callback = base::BindOnce(
          [](SharedStorageUpdateCallback callback, OperationResult result) {
            if (result != OperationResult::kSet) {
              std::move(callback).Run(
                  /*error_message=*/"sharedStorage.append() failed");
              return;
            }

            std::move(callback).Run(/*error_message=*/{});
          },
          std::move(callback));

      storage_partition_->GetSharedStorageManager()->Append(
          shared_storage_origin, append_method->key, append_method->value,
          std::move(completed_callback));
      break;
    }
    case network::mojom::SharedStorageModifierMethod::Tag::kDeleteMethod: {
      network::mojom::SharedStorageDeleteMethodPtr& delete_method =
          method->get_delete_method();

      auto completed_callback = base::BindOnce(
          [](SharedStorageUpdateCallback callback, OperationResult result) {
            if (result != OperationResult::kSuccess) {
              std::move(callback).Run(
                  /*error_message=*/"sharedStorage.delete() failed");
              return;
            }

            std::move(callback).Run(/*error_message=*/{});
          },
          std::move(callback));

      storage_partition_->GetSharedStorageManager()->Delete(
          shared_storage_origin, delete_method->key,
          std::move(completed_callback));
      break;
    }
    case network::mojom::SharedStorageModifierMethod::Tag::kClearMethod: {
      auto completed_callback = base::BindOnce(
          [](SharedStorageUpdateCallback callback, OperationResult result) {
            if (result != OperationResult::kSuccess) {
              std::move(callback).Run(
                  /*error_message=*/"sharedStorage.clear() failed");
              return;
            }

            std::move(callback).Run(/*error_message=*/{});
          },
          std::move(callback));

      storage_partition_->GetSharedStorageManager()->Clear(
          shared_storage_origin, std::move(completed_callback));
      break;
    }
  }

  // Release any lock associated with this specific method. This also removes
  // any associated `blink::mojom::LockRequest` receiver in
  // `lock_request_receivers_`.
  lock_handle.reset();
  lock_manager.reset();

  // If this method is part of a batch update, update the batch's progress
  // and release any batch lock if this was the final method in the batch. This
  // also removes any associated `blink::mojom::LockRequest` receiver in
  // `lock_request_receivers_`.
  if (legacy_batch_update_id) {
    auto it =
        pending_legacy_batch_updates_.find(legacy_batch_update_id.value());
    CHECK(it != pending_legacy_batch_updates_.end());

    LegacyBatchUpdateState& batch_state = it->second;

    CHECK_GT(batch_state.unstarted_updates_count, 0u);

    batch_state.unstarted_updates_count--;

    if (batch_state.unstarted_updates_count == 0u) {
      batch_state.lock_handle.reset();
      batch_state.lock_manager.reset();
    }
  }
}

void SharedStorageLockManager::OnReadyToHandleBatchUpdate(
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
    mojo::Remote<blink::mojom::LockManager> lock_manager) {
  if (methods_with_options.empty()) {
    std::move(callback).Run(/*error_message=*/{});

    // Release any batch lock (i.e., `lock_handle` and `lock_manager` are
    // automatically reset here). This also removes any associated
    // `blink::mojom::LockRequest` receiver in `lock_request_receivers_`.
    return;
  }

  int batch_update_id = next_batch_update_id_++;

  storage_partition_->GetSharedStorageRuntimeManager()
      ->NotifySharedStorageAccessed(
          scope, AccessMethod::kBatchUpdate, main_frame_id,
          shared_storage_origin.Serialize(),
          SharedStorageEventParams::CreateForBatchUpdate(
              worklet_ordinal_id, worklet_devtools_token, with_lock,
              batch_update_id, methods_with_options.size()));

  if (base::FeatureList::IsEnabled(
          network::features::kSharedStorageTransactionalBatchUpdate)) {
    for (auto& method_with_options : methods_with_options) {
      auto& method = method_with_options->method;
      NotifySharedStorageAccessed(method, shared_storage_origin, scope,
                                  main_frame_id, worklet_ordinal_id,
                                  worklet_devtools_token,
                                  /*with_lock=*/std::nullopt, batch_update_id);
    }

    auto completed_callback = base::BindOnce(
        [](SharedStorageUpdateCallback callback,
           BatchUpdateResult batch_update_result) {
          if (batch_update_result.overall_result != OperationResult::kSuccess) {
            std::move(callback).Run(
                /*error_message=*/kBatchUpdateErrorMessage);
            return;
          }

          std::move(callback).Run(/*error_message=*/{});
        },
        std::move(callback));

    storage_partition_->GetSharedStorageManager()->BatchUpdate(
        shared_storage_origin, std::move(methods_with_options),
        std::move(completed_callback));

    // Release any batch lock (i.e., `lock_handle` and `lock_manager` are
    // automatically reset here). This also removes any associated
    // `blink::mojom::LockRequest` receiver in `lock_request_receivers_`.
    return;
  }

  pending_legacy_batch_updates_.emplace(
      batch_update_id,
      LegacyBatchUpdateState(
          /*pending_updates_count=*/methods_with_options.size(),
          std::move(callback), std::move(lock_handle),
          std::move(lock_manager)));

  for (auto& method_with_options : methods_with_options) {
    SharedStorageUpdateHelper(
        std::move(method_with_options), shared_storage_origin, scope,
        main_frame_id, worklet_ordinal_id, worklet_devtools_token,
        base::BindOnce(
            &SharedStorageLockManager::OnMethodWithinLegacyBatchUpdateFinished,
            weak_ptr_factory_.GetWeakPtr(), batch_update_id),
        std::make_optional(batch_update_id));
  }
}

void SharedStorageLockManager::OnMethodWithinLegacyBatchUpdateFinished(
    int legacy_batch_update_id,
    const std::string& error_message) {
  auto it = pending_legacy_batch_updates_.find(legacy_batch_update_id);
  CHECK(it != pending_legacy_batch_updates_.end());

  LegacyBatchUpdateState& batch_state = it->second;

  CHECK_GT(batch_state.unfinished_updates_count, 0u);

  if (!error_message.empty()) {
    batch_state.has_error = true;
  }

  batch_state.unfinished_updates_count--;

  // The last method within the batch has completed. Resolve the callback and
  // remove the state from `pending_legacy_batch_updates_`.
  if (batch_state.unfinished_updates_count == 0u) {
    if (batch_state.has_error) {
      std::move(batch_state.callback)
          .Run(/*error_message=*/kBatchUpdateErrorMessage);
    } else {
      std::move(batch_state.callback).Run(/*error_message=*/{});
    }

    pending_legacy_batch_updates_.erase(it);
  }
}

void SharedStorageLockManager::RequestLock(
    const url::Origin& shared_storage_origin,
    const std::string& lock_name,
    LockGrantedCallback lock_granted_callback) {
  // Request the lock via the LockManager. `this` also becomes the receiver for
  // the lock request. The pending callback is stored to the receiver's
  // associated context (`LockRequestReceiverState`) and upon `Granted()`, the
  // callback will be invoked.

  mojo::PendingRemote<blink::mojom::LockManager> pending_lock_manager;
  BindLockManager(shared_storage_origin,
                  pending_lock_manager.InitWithNewPipeAndPassReceiver());
  mojo::Remote<blink::mojom::LockManager> lock_manager(
      std::move(pending_lock_manager));

  mojo::PendingAssociatedReceiver<blink::mojom::LockRequest>
      lock_request_receiver;

  mojo::PendingAssociatedRemote<blink::mojom::LockRequest> lock_request_remote =
      lock_request_receiver.InitWithNewEndpointAndPassRemote();

  // RequestLock() can either finish synchronously and remove the receiver, or
  // it may queue a request to complete later. If the request is queued, we need
  // to keep the `lock_manager` alive in order to keep the LockRequest receiver
  // alive to receive `Granted()` eventually. Therefore, we first add the
  // receiver and the context to `lock_request_receivers_`, and then call
  // `RequestLock()`.
  mojo::ReceiverId id = lock_request_receivers_.Add(
      this, std::move(lock_request_receiver),
      {std::move(lock_granted_callback), std::move(lock_manager)});

  LockRequestReceiverState* context = lock_request_receivers_.GetContext(id);
  context->lock_manager->RequestLock(lock_name,
                                     blink::mojom::LockMode::EXCLUSIVE,
                                     blink::mojom::LockManager::WaitMode::WAIT,
                                     std::move(lock_request_remote));
}

void SharedStorageLockManager::NotifySharedStorageAccessed(
    const network::mojom::SharedStorageModifierMethodPtr& method,
    const url::Origin& shared_storage_origin,
    AccessScope scope,
    GlobalRenderFrameHostId main_frame_id,
    std::optional<int> worklet_ordinal_id,
    const base::UnguessableToken& worklet_devtools_token,
    std::optional<std::string> with_lock,
    std::optional<int> batch_update_id) {
  AccessMethod access_method = GetAccessMethod(method);

  switch (method->which()) {
    case network::mojom::SharedStorageModifierMethod::Tag::kSetMethod: {
      network::mojom::SharedStorageSetMethodPtr& set_method =
          method->get_set_method();

      storage_partition_->GetSharedStorageRuntimeManager()
          ->NotifySharedStorageAccessed(
              scope, access_method, main_frame_id,
              shared_storage_origin.Serialize(),
              SharedStorageEventParams::CreateForSet(
                  base::UTF16ToUTF8(set_method->key),
                  base::UTF16ToUTF8(set_method->value),
                  set_method->ignore_if_present, worklet_ordinal_id,
                  worklet_devtools_token, with_lock, batch_update_id));
      break;
    }
    case network::mojom::SharedStorageModifierMethod::Tag::kAppendMethod: {
      network::mojom::SharedStorageAppendMethodPtr& append_method =
          method->get_append_method();

      storage_partition_->GetSharedStorageRuntimeManager()
          ->NotifySharedStorageAccessed(
              scope, access_method, main_frame_id,
              shared_storage_origin.Serialize(),
              SharedStorageEventParams::CreateForAppend(
                  base::UTF16ToUTF8(append_method->key),
                  base::UTF16ToUTF8(append_method->value), worklet_ordinal_id,
                  worklet_devtools_token, with_lock, batch_update_id));
      break;
    }
    case network::mojom::SharedStorageModifierMethod::Tag::kDeleteMethod: {
      network::mojom::SharedStorageDeleteMethodPtr& delete_method =
          method->get_delete_method();

      storage_partition_->GetSharedStorageRuntimeManager()
          ->NotifySharedStorageAccessed(
              scope, access_method, main_frame_id,
              shared_storage_origin.Serialize(),
              SharedStorageEventParams::CreateForDelete(
                  base::UTF16ToUTF8(delete_method->key), worklet_ordinal_id,
                  worklet_devtools_token, with_lock, batch_update_id));
      break;
    }
    case network::mojom::SharedStorageModifierMethod::Tag::kClearMethod: {
      storage_partition_->GetSharedStorageRuntimeManager()
          ->NotifySharedStorageAccessed(
              scope, access_method, main_frame_id,
              shared_storage_origin.Serialize(),
              SharedStorageEventParams::CreateForClear(
                  worklet_ordinal_id, worklet_devtools_token, with_lock,
                  batch_update_id));
      break;
    }
  }
}

}  // namespace content
