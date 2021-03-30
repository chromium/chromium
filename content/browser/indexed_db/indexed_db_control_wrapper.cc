// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_control_wrapper.h"

#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace content {

IndexedDBControlWrapper::IndexedDBControlWrapper(
    const base::FilePath& data_path,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    base::Clock* clock,
    mojo::PendingRemote<storage::mojom::BlobStorageContext>
        blob_storage_context,
    mojo::PendingRemote<storage::mojom::FileSystemAccessContext>
        file_system_access_context,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    scoped_refptr<base::SequencedTaskRunner> custom_task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  context_ = base::MakeRefCounted<IndexedDBContextImpl>(
      data_path, std::move(quota_manager_proxy), clock,
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

  context_->Shutdown();
  IndexedDBContextImpl::ReleaseOnIDBSequence(std::move(context_));
}

void IndexedDBControlWrapper::BindIndexedDB(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindRemoteIfNeeded();
  if (storage_policy_observer_)
    storage_policy_observer_->StartTrackingOrigin(origin);
  indexed_db_control_->BindIndexedDB(origin, std::move(receiver));
}

void IndexedDBControlWrapper::GetUsage(GetUsageCallback usage_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindRemoteIfNeeded();
  indexed_db_control_->GetUsage(std::move(usage_callback));
}

void IndexedDBControlWrapper::DeleteForOrigin(
    const url::Origin& origin,
    DeleteForOriginCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindRemoteIfNeeded();
  indexed_db_control_->DeleteForOrigin(origin, std::move(callback));
}

void IndexedDBControlWrapper::ForceClose(
    const url::Origin& origin,
    storage::mojom::ForceCloseReason reason,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindRemoteIfNeeded();
  indexed_db_control_->ForceClose(origin, reason, std::move(callback));
}

void IndexedDBControlWrapper::GetConnectionCount(
    const url::Origin& origin,
    GetConnectionCountCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindRemoteIfNeeded();
  indexed_db_control_->GetConnectionCount(origin, std::move(callback));
}

void IndexedDBControlWrapper::DownloadOriginData(
    const url::Origin& origin,
    DownloadOriginDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindRemoteIfNeeded();
  indexed_db_control_->DownloadOriginData(origin, std::move(callback));
}

void IndexedDBControlWrapper::GetAllOriginsDetails(
    GetAllOriginsDetailsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindRemoteIfNeeded();
  indexed_db_control_->GetAllOriginsDetails(std::move(callback));
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

  if (indexed_db_control_.is_bound())
    return;
  context_->IDBTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&IndexedDBContextImpl::Bind, context_,
                     indexed_db_control_.BindNewPipeAndPassReceiver()));
}

}  // namespace content
