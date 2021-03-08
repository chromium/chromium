// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_control_wrapper.h"

#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace content {

// Observer for the SpecialStoragePolicy on the IO thread.
class IndexedDBControlWrapper::StoragePolicyObserver
    : public storage::SpecialStoragePolicy::Observer {
 public:
  explicit StoragePolicyObserver(
      scoped_refptr<base::SequencedTaskRunner> reply_task_runner,
      scoped_refptr<storage::SpecialStoragePolicy> storage_policy,
      base::WeakPtr<IndexedDBControlWrapper> control_wrapper)
      : reply_task_runner_(std::move(reply_task_runner)),
        storage_policy_(std::move(storage_policy)),
        control_wrapper_(std::move(control_wrapper)) {
    storage_policy_->AddObserver(this);
  }

  ~StoragePolicyObserver() override { storage_policy_->RemoveObserver(this); }

  // storage::SpecialStoragePolicy::Observer implementation:
  void OnPolicyChanged() override {
    reply_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&IndexedDBControlWrapper::OnSpecialStoragePolicyChanged,
                       control_wrapper_));
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> reply_task_runner_;
  scoped_refptr<storage::SpecialStoragePolicy> storage_policy_;

  // control_wrapper_ is bound to the reply_task_runner sequence,
  // so should not be checked other than on that sequence.
  base::WeakPtr<IndexedDBControlWrapper> control_wrapper_;
};

IndexedDBControlWrapper::IndexedDBControlWrapper(
    const base::FilePath& data_path,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    base::Clock* clock,
    mojo::PendingRemote<storage::mojom::BlobStorageContext>
        blob_storage_context,
    mojo::PendingRemote<storage::mojom::NativeFileSystemContext>
        native_file_system_context,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    scoped_refptr<base::SequencedTaskRunner> custom_task_runner)
    : special_storage_policy_(std::move(special_storage_policy)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  context_ = base::MakeRefCounted<IndexedDBContextImpl>(
      data_path, std::move(quota_manager_proxy), clock,
      std::move(blob_storage_context), std::move(native_file_system_context),
      io_task_runner, std::move(custom_task_runner));

  if (special_storage_policy_) {
    storage_policy_observer_ = base::SequenceBound<StoragePolicyObserver>(
        io_task_runner, base::SequencedTaskRunnerHandle::Get(),
        special_storage_policy_, weak_factory_.GetWeakPtr());
  }
}

IndexedDBControlWrapper::~IndexedDBControlWrapper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  IndexedDBContextImpl::ReleaseOnIDBSequence(std::move(context_));
}

void IndexedDBControlWrapper::BindIndexedDB(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindRemoteIfNeeded();
  TrackOriginPolicyState(origin);
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
    std::vector<storage::mojom::IndexedDBStoragePolicyUpdatePtr>
        policy_updates) {
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

void IndexedDBControlWrapper::OnSpecialStoragePolicyChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindRemoteIfNeeded();

  std::vector<storage::mojom::IndexedDBStoragePolicyUpdatePtr> policy_updates;
  for (auto& entry : origin_state_) {
    const GURL& origin = entry.first;
    OriginState& state = entry.second;
    state.should_purge_on_shutdown = ShouldPurgeOnShutdown(origin);

    if (state.should_purge_on_shutdown != state.will_purge_on_shutdown) {
      state.will_purge_on_shutdown = state.should_purge_on_shutdown;
      policy_updates.push_back(
          storage::mojom::IndexedDBStoragePolicyUpdate::New(
              url::Origin::Create(origin), state.should_purge_on_shutdown));
    }
  }
  if (!policy_updates.empty())
    ApplyPolicyUpdates(std::move(policy_updates));
}

void IndexedDBControlWrapper::TrackOriginPolicyState(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const GURL origin_url = GURL(origin.Serialize());
  auto it = origin_state_.find(origin_url);
  if (it == origin_state_.end())
    origin_state_[origin_url] = {};
  OnSpecialStoragePolicyChanged();
}

bool IndexedDBControlWrapper::ShouldPurgeOnShutdown(const GURL& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!special_storage_policy_)
    return false;
  if (!special_storage_policy_->IsStorageSessionOnly(origin))
    return false;
  if (special_storage_policy_->IsStorageProtected(origin))
    return false;
  return true;
}

void IndexedDBControlWrapper::BindRemoteIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(
      !(indexed_db_control_.is_bound() && !indexed_db_control_.is_connected()))
      << "Rebinding is not supported yet.";

  if (indexed_db_control_.is_bound())
    return;
  IndexedDBContextImpl* idb_context = GetIndexedDBContextInternal();
  idb_context->IDBTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&IndexedDBContextImpl::Bind,
                     base::WrapRefCounted(idb_context),
                     indexed_db_control_.BindNewPipeAndPassReceiver()));
}

}  // namespace content
