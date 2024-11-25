// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_lock_manager.h"

#include "components/services/storage/shared_storage/shared_storage_database.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/browser/shared_storage/shared_storage_runtime_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "services/network/public/mojom/shared_storage.mojom.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

using AccessScope = SharedStorageLockManager::AccessScope;
using AccessType =
    SharedStorageRuntimeManager::SharedStorageObserverInterface::AccessType;
using OperationResult = storage::SharedStorageManager::OperationResult;

AccessType GetAccessType(
    const network::mojom::SharedStorageModifierMethodPtr& method,
    AccessScope scope) {
  switch (method->which()) {
    case network::mojom::SharedStorageModifierMethod::Tag::kSetMethod: {
      switch (scope) {
        case AccessScope::kWindow:
          return AccessType::kDocumentSet;
        case AccessScope::kSharedStorageWorklet:
          return AccessType::kWorkletSet;
        case AccessScope::kHeader:
          return AccessType::kHeaderSet;
      }
      break;
    }
    case network::mojom::SharedStorageModifierMethod::Tag::kAppendMethod: {
      switch (scope) {
        case AccessScope::kWindow:
          return AccessType::kDocumentAppend;
        case AccessScope::kSharedStorageWorklet:
          return AccessType::kWorkletAppend;
        case AccessScope::kHeader:
          return AccessType::kHeaderAppend;
      }
      break;
    }
    case network::mojom::SharedStorageModifierMethod::Tag::kDeleteMethod: {
      switch (scope) {
        case AccessScope::kWindow:
          return AccessType::kDocumentDelete;
        case AccessScope::kSharedStorageWorklet:
          return AccessType::kWorkletDelete;
        case AccessScope::kHeader:
          return AccessType::kHeaderDelete;
      }
      break;
    }
    case network::mojom::SharedStorageModifierMethod::Tag::kClearMethod: {
      switch (scope) {
        case AccessScope::kWindow:
          return AccessType::kDocumentClear;
        case AccessScope::kSharedStorageWorklet:
          return AccessType::kWorkletClear;
        case AccessScope::kHeader:
          return AccessType::kHeaderClear;
      }
      break;
    }
  }

  NOTREACHED();
}

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
    FrameTreeNodeId main_frame_id,
    SharedStorageUpdateCallback callback) {
  auto ready_to_handle_update_callback = base::BindOnce(
      &SharedStorageLockManager::OnReadyToHandleUpdate,
      weak_ptr_factory_.GetWeakPtr(), std::move(method_with_options->method),
      shared_storage_origin, scope, main_frame_id, std::move(callback));

  const std::optional<std::string>& with_lock = method_with_options->with_lock;

  if (!with_lock ||
      !base::FeatureList::IsEnabled(blink::features::kSharedStorageWebLocks)) {
    std::move(ready_to_handle_update_callback).Run();
    return;
  }

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
      {/*lock_granted_callback=*/std::move(ready_to_handle_update_callback),
       std::move(lock_manager)});

  LockRequestReceiverState* context = lock_request_receivers_.GetContext(id);
  context->lock_manager->RequestLock(with_lock.value(),
                                     blink::mojom::LockMode::EXCLUSIVE,
                                     blink::mojom::LockManager::WaitMode::WAIT,
                                     std::move(lock_request_remote));
}

void SharedStorageLockManager::Granted(
    mojo::PendingAssociatedRemote<blink::mojom::LockHandle> pending_handle) {
  mojo::AssociatedRemote<blink::mojom::LockHandle> handle;
  handle.Bind(std::move(pending_handle));

  std::move(lock_request_receivers_.current_context().lock_granted_callback)
      .Run();

  // `handle` is destroyed here. This triggers the following events:
  // 1. The lock is released on the back end.
  // 2. The corresponding receiver is removed from `lock_request_receivers_`.
  // Since the relevant Mojo services reside in the browser process, these
  // should complete synchronously.
}

void SharedStorageLockManager::Failed() {
  // With WaitMode::WAIT, `Failed()` will never trigger.
  NOTREACHED();
}

SharedStorageLockManager::LockRequestReceiverState::LockRequestReceiverState(
    base::OnceClosure lock_granted_callback,
    mojo::Remote<blink::mojom::LockManager> lock_manager)
    : lock_granted_callback(std::move(lock_granted_callback)),
      lock_manager(std::move(lock_manager)) {}
SharedStorageLockManager::LockRequestReceiverState::LockRequestReceiverState() =
    default;
SharedStorageLockManager::LockRequestReceiverState::LockRequestReceiverState(
    LockRequestReceiverState&& other) = default;
SharedStorageLockManager::LockRequestReceiverState::
    ~LockRequestReceiverState() = default;

void SharedStorageLockManager::OnReadyToHandleUpdate(
    network::mojom::SharedStorageModifierMethodPtr method,
    url::Origin shared_storage_origin,
    AccessScope scope,
    FrameTreeNodeId main_frame_id,
    SharedStorageUpdateCallback callback) {
  AccessType access_type = GetAccessType(method, scope);

  switch (method->which()) {
    case network::mojom::SharedStorageModifierMethod::Tag::kSetMethod: {
      network::mojom::SharedStorageSetMethodPtr& set_method =
          method->get_set_method();

      storage::SharedStorageDatabase::SetBehavior set_behavior =
          set_method->ignore_if_present
              ? storage::SharedStorageDatabase::SetBehavior::kIgnoreIfPresent
              : storage::SharedStorageDatabase::SetBehavior::kDefault;

      storage_partition_->GetSharedStorageRuntimeManager()
          ->NotifySharedStorageAccessed(
              access_type, main_frame_id, shared_storage_origin.Serialize(),
              SharedStorageEventParams::CreateForSet(
                  base::UTF16ToUTF8(set_method->key),
                  base::UTF16ToUTF8(set_method->value),
                  set_method->ignore_if_present));

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

      storage_partition_->GetSharedStorageRuntimeManager()
          ->NotifySharedStorageAccessed(
              access_type, main_frame_id, shared_storage_origin.Serialize(),
              SharedStorageEventParams::CreateForAppend(
                  base::UTF16ToUTF8(append_method->key),
                  base::UTF16ToUTF8(append_method->value)));

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

      storage_partition_->GetSharedStorageRuntimeManager()
          ->NotifySharedStorageAccessed(
              access_type, main_frame_id, shared_storage_origin.Serialize(),
              SharedStorageEventParams::CreateForGetOrDelete(
                  base::UTF16ToUTF8(delete_method->key)));

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
      storage_partition_->GetSharedStorageRuntimeManager()
          ->NotifySharedStorageAccessed(
              access_type, main_frame_id, shared_storage_origin.Serialize(),
              SharedStorageEventParams::CreateDefault());

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
}

}  // namespace content
