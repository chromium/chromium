// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_caching_storage.h"

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/interest_group/interest_group_storage.h"
#include "content/browser/interest_group/storage_interest_group.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace content {

StorageInterestGroups::StorageInterestGroups(
    std::vector<StorageInterestGroup>&& interest_groups)
    : storage_interest_groups_(std::move(interest_groups)) {}

StorageInterestGroups::~StorageInterestGroups() = default;

base::WeakPtr<StorageInterestGroups> StorageInterestGroups::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

InterestGroupCachingStorage::InterestGroupCachingStorage(
    const base::FilePath& path,
    bool in_memory)
    : interest_group_storage_(
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
               base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
          in_memory ? base::FilePath() : path) {}

InterestGroupCachingStorage::~InterestGroupCachingStorage() = default;

void InterestGroupCachingStorage::GetInterestGroupsForOwner(
    const url::Origin& owner,
    base::OnceCallback<void(scoped_refptr<StorageInterestGroups>)> callback) {
  auto it = cached_interest_groups_.find(owner);
  if (it != cached_interest_groups_.end() && it->second.MaybeValid()) {
    std::move(callback).Run(
        scoped_refptr<StorageInterestGroups>(it->second.get()));
    return;
  }
  interest_group_storage_
      .AsyncCall(&InterestGroupStorage::GetInterestGroupsForOwner)
      .WithArgs(owner)
      .Then(base::BindOnce(
          &InterestGroupCachingStorage::OnLoadInterestGroupsForOwner,
          weak_factory_.GetWeakPtr(), owner, std::move(callback)));
}

void InterestGroupCachingStorage::JoinInterestGroup(
    const blink::InterestGroup& group,
    const GURL& main_frame_joining_url,
    base::OnceClosure callback) {
  cached_interest_groups_.erase(group.owner);
  interest_group_storage_.AsyncCall(&InterestGroupStorage::JoinInterestGroup)
      .WithArgs(std::move(group), std::move(main_frame_joining_url))
      .Then(std::move(callback));
}

void InterestGroupCachingStorage::LeaveInterestGroup(
    const blink::InterestGroupKey& group_key,
    const url::Origin& main_frame,
    base::OnceClosure callback) {
  cached_interest_groups_.erase(group_key.owner);
  interest_group_storage_.AsyncCall(&InterestGroupStorage::LeaveInterestGroup)
      .WithArgs(group_key, main_frame)
      .Then(std::move(callback));
}

void InterestGroupCachingStorage::ClearOriginJoinedInterestGroups(
    const url::Origin& owner,
    const std::set<std::string>& interest_groups_to_keep,
    const url::Origin& main_frame_origin,
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  cached_interest_groups_.erase(owner);
  interest_group_storage_
      .AsyncCall(&InterestGroupStorage::ClearOriginJoinedInterestGroups)
      .WithArgs(std::move(owner), std::move(interest_groups_to_keep),
                std::move(main_frame_origin))
      .Then(std::move(callback));
}

void InterestGroupCachingStorage::UpdateInterestGroup(
    const blink::InterestGroupKey& group_key,
    InterestGroupUpdate update,
    base::OnceCallback<void(bool)> notify_callback) {
  cached_interest_groups_.erase(group_key.owner);
  interest_group_storage_.AsyncCall(&InterestGroupStorage::UpdateInterestGroup)
      .WithArgs(group_key, std::move(update))
      .Then(std::move(notify_callback));
}

void InterestGroupCachingStorage::ReportUpdateFailed(
    const blink::InterestGroupKey& group_key,
    bool parse_failure) {
  interest_group_storage_.AsyncCall(&InterestGroupStorage::ReportUpdateFailed)
      .WithArgs(group_key, parse_failure);
}

void InterestGroupCachingStorage::RecordInterestGroupBids(
    const blink::InterestGroupSet& groups) {
  for (const blink::InterestGroupKey& group_key : groups) {
    cached_interest_groups_.erase(group_key.owner);
  }
  interest_group_storage_
      .AsyncCall(&InterestGroupStorage::RecordInterestGroupBids)
      .WithArgs(groups);
}

void InterestGroupCachingStorage::RecordInterestGroupWin(
    const blink::InterestGroupKey& group_key,
    const std::string& ad_json) {
  cached_interest_groups_.erase(group_key.owner);
  interest_group_storage_
      .AsyncCall(&InterestGroupStorage::RecordInterestGroupWin)
      .WithArgs(group_key, std::move(ad_json));
}

void InterestGroupCachingStorage::UpdateKAnonymity(
    const StorageInterestGroup::KAnonymityData& data) {
  // We do not know the affected owners without looking them up from the
  // database or calculating k-anon keys for all the ads in the cache. Both are
  // expensive, and this function will run many times per interest group, so
  // prefer to over-delete data here.
  cached_interest_groups_.clear();
  interest_group_storage_.AsyncCall(&InterestGroupStorage::UpdateKAnonymity)
      .WithArgs(data);
}

void InterestGroupCachingStorage::GetLastKAnonymityReported(
    const std::string& key,
    base::OnceCallback<void(absl::optional<base::Time>)> callback) {
  interest_group_storage_
      .AsyncCall(&InterestGroupStorage::GetLastKAnonymityReported)
      .WithArgs(key)
      .Then(std::move(callback));
}

void InterestGroupCachingStorage::UpdateLastKAnonymityReported(
    const std::string& key) {
  // We don't need to invalidate any cached objects here because this value is
  // not loaded in GetInterestGroupsForOwner.
  interest_group_storage_
      .AsyncCall(&InterestGroupStorage::UpdateLastKAnonymityReported)
      .WithArgs(key);
}

void InterestGroupCachingStorage::GetInterestGroup(
    const blink::InterestGroupKey& group_key,
    base::OnceCallback<void(absl::optional<StorageInterestGroup>)> callback) {
  // TODO(abigailkatcoff): This function could check the cache first for the
  // group.
  interest_group_storage_.AsyncCall(&InterestGroupStorage::GetInterestGroup)
      .WithArgs(group_key)
      .Then(std::move(callback));
}

void InterestGroupCachingStorage::GetAllInterestGroupOwners(
    base::OnceCallback<void(std::vector<url::Origin>)> callback) {
  return interest_group_storage_
      .AsyncCall(&InterestGroupStorage::GetAllInterestGroupOwners)
      .Then(std::move(callback));
}

void InterestGroupCachingStorage::GetInterestGroupsForUpdate(
    const url::Origin& owner,
    int groups_limit,
    base::OnceCallback<void(std::vector<InterestGroupUpdateParameter>)>
        callback) {
  interest_group_storage_
      .AsyncCall(&InterestGroupStorage::GetInterestGroupsForUpdate)
      .WithArgs(owner, groups_limit)
      .Then(std::move(callback));
}

void InterestGroupCachingStorage::GetKAnonymityDataForUpdate(
    const blink::InterestGroupKey& group_key,
    base::OnceCallback<void(
        const std::vector<StorageInterestGroup::KAnonymityData>&)> callback) {
  interest_group_storage_
      .AsyncCall(&InterestGroupStorage::GetKAnonymityDataForUpdate)
      .WithArgs(group_key)
      .Then(std::move(callback));
}

void InterestGroupCachingStorage::GetAllInterestGroupJoiningOrigins(
    base::OnceCallback<void(std::vector<url::Origin>)> callback) {
  interest_group_storage_
      .AsyncCall(&InterestGroupStorage::GetAllInterestGroupJoiningOrigins)
      .Then(std::move(callback));
}

void InterestGroupCachingStorage::GetAllInterestGroupOwnerJoinerPairs(
    base::OnceCallback<void(std::vector<std::pair<url::Origin, url::Origin>>)>
        callback) {
  interest_group_storage_
      .AsyncCall(&InterestGroupStorage::GetAllInterestGroupOwnerJoinerPairs)
      .Then(std::move(callback));
}

void InterestGroupCachingStorage::RemoveInterestGroupsMatchingOwnerAndJoiner(
    url::Origin owner,
    url::Origin joining_origin,
    base::OnceClosure callback) {
  cached_interest_groups_.erase(owner);
  interest_group_storage_
      .AsyncCall(
          &InterestGroupStorage::RemoveInterestGroupsMatchingOwnerAndJoiner)
      .WithArgs(owner, joining_origin)
      .Then(std::move(callback));
}

void InterestGroupCachingStorage::DeleteInterestGroupData(
    StoragePartition::StorageKeyMatcherFunction storage_key_matcher,
    base::OnceClosure callback) {
  // Clear all owners because storage_key_matcher can match on joining_origin,
  // which we do not have stored in cached_interest_groups_.
  cached_interest_groups_.clear();
  interest_group_storage_
      .AsyncCall(&InterestGroupStorage::DeleteInterestGroupData)
      .WithArgs(std::move(storage_key_matcher))
      .Then(std::move(callback));
}
void InterestGroupCachingStorage::DeleteAllInterestGroupData(
    base::OnceClosure callback) {
  cached_interest_groups_.clear();
  interest_group_storage_
      .AsyncCall(&InterestGroupStorage::DeleteAllInterestGroupData)
      .Then(std::move(callback));
}
void InterestGroupCachingStorage::SetInterestGroupPriority(
    const blink::InterestGroupKey& group_key,
    double priority) {
  cached_interest_groups_.erase(group_key.owner);
  interest_group_storage_
      .AsyncCall(&InterestGroupStorage::SetInterestGroupPriority)
      .WithArgs(group_key, priority);
}

void InterestGroupCachingStorage::UpdateInterestGroupPriorityOverrides(
    const blink::InterestGroupKey& group_key,
    base::flat_map<std::string,
                   auction_worklet::mojom::PrioritySignalsDoublePtr>
        update_priority_signals_overrides) {
  cached_interest_groups_.erase(group_key.owner);
  interest_group_storage_
      .AsyncCall(&InterestGroupStorage::UpdateInterestGroupPriorityOverrides)
      .WithArgs(group_key, std::move(update_priority_signals_overrides));
}

void InterestGroupCachingStorage::GetLastMaintenanceTimeForTesting(
    base::RepeatingCallback<void(base::Time)> callback) const {
  interest_group_storage_
      .AsyncCall(&InterestGroupStorage::GetLastMaintenanceTimeForTesting)
      .Then(std::move(callback));
}

void InterestGroupCachingStorage::OnLoadInterestGroupsForOwner(
    const url::Origin& owner,
    base::OnceCallback<void(scoped_refptr<StorageInterestGroups>)> callback,
    std::vector<StorageInterestGroup> interest_groups) {
  scoped_refptr<StorageInterestGroups> interest_groups_ptr =
      base::MakeRefCounted<StorageInterestGroups>(std::move(interest_groups));
  cached_interest_groups_[owner] = interest_groups_ptr->GetWeakPtr();
  std::move(callback).Run(std::move(interest_groups_ptr));
}

}  // namespace content
