// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_control_wrapper.h"

#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

IndexedDBControlWrapper::IndexedDBControlWrapper(
    const base::FilePath& data_path,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    mojo::PendingRemote<storage::mojom::BlobStorageContext>
        blob_storage_context,
    mojo::PendingRemote<storage::mojom::FileSystemAccessContext>
        file_system_access_context,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    scoped_refptr<base::SequencedTaskRunner> custom_task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  context_ = std::make_unique<IndexedDBContextImpl>(
      data_path, std::move(quota_manager_proxy),
      std::move(blob_storage_context), std::move(file_system_access_context),
      io_task_runner, std::move(custom_task_runner));

  if (special_storage_policy) {
    storage_policy_observer_.emplace(
        base::BindRepeating(&IndexedDBControlWrapper::ApplyPolicyUpdates,
                            weak_factory_.GetWeakPtr()),
        io_task_runner, std::move(special_storage_policy));
  }
}

IndexedDBControlWrapper::~IndexedDBControlWrapper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  IndexedDBContextImpl::Shutdown(std::move(context_));
}

void IndexedDBControlWrapper::BindIndexedDB(
    const storage::BucketLocator& bucket_locator,
    mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
        client_state_checker_remote,
    mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindRemoteIfNeeded();
  if (storage_policy_observer_) {
    // TODO(https://crbug.com/1199077): Pass the real StorageKey once
    // StoragePolicyObserver is migrated.
    storage_policy_observer_->StartTrackingOrigin(
        bucket_locator.storage_key.origin());
  }
  indexed_db_control_->BindIndexedDB(bucket_locator,
                                     std::move(client_state_checker_remote),
                                     std::move(receiver));
}

void IndexedDBControlWrapper::DeleteForStorageKey(
    const blink::StorageKey& storage_key,
    DeleteForStorageKeyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindRemoteIfNeeded();
  indexed_db_control_->DeleteForStorageKey(storage_key, std::move(callback));
}

void IndexedDBControlWrapper::ForceClose(
    storage::BucketId bucket_id,
    storage::mojom::ForceCloseReason reason,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindRemoteIfNeeded();
  indexed_db_control_->ForceClose(bucket_id, reason, std::move(callback));
}

void IndexedDBControlWrapper::GetConnectionCount(
    storage::BucketId bucket_id,
    GetConnectionCountCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindRemoteIfNeeded();
  indexed_db_control_->GetConnectionCount(bucket_id, std::move(callback));
}

void IndexedDBControlWrapper::DownloadBucketData(
    storage::BucketId bucket_id,
    DownloadBucketDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindRemoteIfNeeded();
  indexed_db_control_->DownloadBucketData(bucket_id, std::move(callback));
}

void IndexedDBControlWrapper::GetAllBucketsDetails(
    GetAllBucketsDetailsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindRemoteIfNeeded();
  indexed_db_control_->GetAllBucketsDetails(std::move(callback));
}

void IndexedDBControlWrapper::SetForceKeepSessionState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindRemoteIfNeeded();
  indexed_db_control_->SetForceKeepSessionState();
}

void IndexedDBControlWrapper::ApplyPolicyUpdates(
    std::vector<storage::mojom::StoragePolicyUpdatePtr> policy_updates) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindRemoteIfNeeded();
  indexed_db_control_->ApplyPolicyUpdates(std::move(policy_updates));
}

void IndexedDBControlWrapper::BindTestInterface(
    mojo::PendingReceiver<storage::mojom::IndexedDBControlTest> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindRemoteIfNeeded();
  indexed_db_control_->BindTestInterface(std::move(receiver));
}

void IndexedDBControlWrapper::AddObserver(
    mojo::PendingRemote<storage::mojom::IndexedDBObserver> observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindRemoteIfNeeded();
  indexed_db_control_->AddObserver(std::move(observer));
}

void IndexedDBControlWrapper::BindRemoteIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(
      !(indexed_db_control_.is_bound() && !indexed_db_control_.is_connected()))
      << "Rebinding is not supported yet.";

  if (!indexed_db_control_.is_bound()) {
    context_->BindControl(indexed_db_control_.BindNewPipeAndPassReceiver());
  }
}

}  // namespace content
