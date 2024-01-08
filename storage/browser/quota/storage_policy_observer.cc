// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/storage_policy_observer.h"

#include <utility>

#include "base/task/sequenced_task_runner.h"
#include "url/origin.h"

namespace storage {

// A helper class that lives on the IO thread and registers with
// SpecialStoragePolicy.  It bounces back any OnPolicyChanged messages
// to the StoragePolicyObserver on the provided `reply_task_runner`.
class StoragePolicyObserverIOThread
    : public storage::SpecialStoragePolicy::Observer {
 public:
  StoragePolicyObserverIOThread(
      scoped_refptr<base::SequencedTaskRunner> reply_task_runner,
      scoped_refptr<storage::SpecialStoragePolicy> storage_policy,
      base::WeakPtr<StoragePolicyObserver> observer)
      : reply_task_runner_(std::move(reply_task_runner)),
        storage_policy_(std::move(storage_policy)),
        observer_(std::move(observer)) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    storage_policy_->AddObserver(this);
  }

  ~StoragePolicyObserverIOThread() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    storage_policy_->RemoveObserver(this);
  }

  // storage::SpecialStoragePolicy::Observer implementation:
  void OnPolicyChanged() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    reply_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&StoragePolicyObserver::OnPolicyChanged, observer_));
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> reply_task_runner_;
  scoped_refptr<storage::SpecialStoragePolicy> storage_policy_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtr<StoragePolicyObserver> observer_;
};

StoragePolicyObserver::StoragePolicyObserver(
    ApplyPolicyUpdatesCallback callback,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    scoped_refptr<storage::SpecialStoragePolicy> storage_policy)
    : callback_(callback), storage_policy_(std::move(storage_policy)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!storage_policy_)
    return;

  storage_policy_observer_ = base::SequenceBound<StoragePolicyObserverIOThread>(
      std::move(io_task_runner), base::SequencedTaskRunner::GetCurrentDefault(),
      storage_policy_, weak_factory_.GetWeakPtr());
}

StoragePolicyObserver::~StoragePolicyObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void StoragePolicyObserver::StartTrackingOrigin(const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StartTrackingOrigins({origin});
}

void StoragePolicyObserver::StartTrackingOrigins(
    const std::vector<url::Origin>& origins) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<std::pair<const GURL, OriginState>*> updates;
  for (const auto& origin : origins) {
    // If the origin exists, emplace fails, and its state is unchanged.
    GURL origin_url = GURL(origin.Serialize());
    auto [entry, success] =
        origin_state_.emplace(std::move(origin_url), OriginState());
    if (success) {
      updates.push_back(&*entry);
    }
  }

  if (!updates.empty()) {
    OnPolicyChangedForOrigins(updates);
  }
}

void StoragePolicyObserver::StopTrackingOrigin(const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const GURL origin_url = GURL(origin.Serialize());
  origin_state_.erase(origin_url);
}

void StoragePolicyObserver::OnPolicyChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<storage::mojom::StoragePolicyUpdatePtr> policy_updates;
  for (auto& entry : origin_state_)
    AddPolicyUpdate(&entry, &policy_updates);
  if (policy_updates.empty())
    return;
  callback_.Run(std::move(policy_updates));
}

bool StoragePolicyObserver::ShouldPurgeOnShutdownForTesting(
    const url::Origin& origin) {
  const GURL origin_url = GURL(origin.Serialize());
  return ShouldPurgeOnShutdown(origin_url);
}

bool StoragePolicyObserver::ShouldPurgeOnShutdown(const GURL& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!storage_policy_)
    return false;
  if (!storage_policy_->IsStorageSessionOnly(origin))
    return false;
  if (storage_policy_->IsStorageProtected(origin))
    return false;
  return true;
}

void StoragePolicyObserver::OnPolicyChangedForOrigins(
    const std::vector<std::pair<const GURL, OriginState>*>& updated_origins) {
  std::vector<storage::mojom::StoragePolicyUpdatePtr> policy_updates;
  for (auto* entry : updated_origins)
    AddPolicyUpdate(entry, &policy_updates);
  if (policy_updates.empty())
    return;
  callback_.Run(std::move(policy_updates));
}

void StoragePolicyObserver::AddPolicyUpdate(
    std::pair<const GURL, OriginState>* entry,
    std::vector<storage::mojom::StoragePolicyUpdatePtr>* policy_updates) {
  const GURL& origin = entry->first;
  OriginState& state = entry->second;
  state.should_purge_on_shutdown = ShouldPurgeOnShutdown(origin);

  if (state.should_purge_on_shutdown != state.will_purge_on_shutdown) {
    state.will_purge_on_shutdown = state.should_purge_on_shutdown;
    policy_updates->emplace_back(storage::mojom::StoragePolicyUpdate::New(
        url::Origin::Create(origin), state.should_purge_on_shutdown));
  }
}

}  // namespace storage
