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
  context_ = base::MakeRefCounted<IndexedDBContextImpl>(
      data_path, std::move(quota_manager_proxy),
      std::move(blob_storage_context), std::move(file_system_access_context),
      io_task_runner, std::move(custom_task_runner));

  if (special_storage_policy) {
    storage_policy_observer_.emplace(
        base::BindRepeating(
            &IndexedDBControlWrapper::OnSpecialStoragePolicyUpdated,
            base::Unretained(this)),
        io_task_runner, std::move(special_storage_policy));
  }
}

IndexedDBControlWrapper::~IndexedDBControlWrapper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  context_->Shutdown();
  IndexedDBContextImpl::ReleaseOnIDBSequence(std::move(context_));
}

storage::mojom::IndexedDBControl&
IndexedDBControlWrapper::GetIndexedDBControl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(
      !(indexed_db_control_.is_bound() && !indexed_db_control_.is_connected()))
      << "Rebinding is not supported yet.";

  if (!indexed_db_control_.is_bound()) {
    context_->IDBTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&IndexedDBContextImpl::Bind, context_,
                       indexed_db_control_.BindNewPipeAndPassReceiver()));

    if (storage_policy_observer_) {
      mojo::PendingRemote<storage::mojom::IndexedDBObserver> remote;
      receiver_.Bind(remote.InitWithNewPipeAndPassReceiver());
      indexed_db_control_->AddObserver(std::move(remote));
    }
  }

  return *indexed_db_control_;
}

void IndexedDBControlWrapper::OnIndexedDBListChanged(
    const storage::BucketLocator& bucket_locator) {
  CHECK(storage_policy_observer_);
  // TODO(https://crbug.com/1199077): Pass the real StorageKey once
  // StoragePolicyObserver is migrated.
  storage_policy_observer_->StartTrackingOrigin(
      bucket_locator.storage_key.origin());
}

void IndexedDBControlWrapper::OnSpecialStoragePolicyUpdated(
    std::vector<storage::mojom::StoragePolicyUpdatePtr> policy_updates) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetIndexedDBControl().ApplyPolicyUpdates(std::move(policy_updates));
}

}  // namespace content
