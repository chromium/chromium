// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_caching_storage.h"
#include <algorithm>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/interest_group/interest_group_storage.h"
#include "content/browser/interest_group/storage_interest_group.h"
#include "content/common/features.h"
#include "url/origin.h"

namespace content {

SingleStorageInterestGroup::SingleStorageInterestGroup(
    scoped_refptr<StorageInterestGroups> storage_interest_groups_for_owner,
    const StorageInterestGroup* storage_interest_group)
    : storage_interest_groups_for_owner(storage_interest_groups_for_owner),
      storage_interest_group(storage_interest_group) {}

SingleStorageInterestGroup::SingleStorageInterestGroup(
    const SingleStorageInterestGroup& other) = default;

SingleStorageInterestGroup::SingleStorageInterestGroup(
    StorageInterestGroup&& interest_group) {
  std::vector<StorageInterestGroup> storage_interest_groups_vec;
  storage_interest_groups_vec.push_back(std::move(interest_group));
  storage_interest_groups_for_owner =
      base::MakeRefCounted<StorageInterestGroups>(
          std::move(storage_interest_groups_vec));
  storage_interest_group =
      storage_interest_groups_for_owner->GetInterestGroups()[0]
          .storage_interest_group;
}

SingleStorageInterestGroup::~SingleStorageInterestGroup() = default;

const StorageInterestGroup* SingleStorageInterestGroup::operator->() const {
  return storage_interest_group;
}

const StorageInterestGroup& SingleStorageInterestGroup::operator*() const {
  return *storage_interest_group;
}

StorageInterestGroups::StorageInterestGroups(
    std::vector<StorageInterestGroup>&& interest_groups)
    : storage_interest_groups_(std::move(interest_groups)) {
  expiry_ = base::Time::Max();
  for (const StorageInterestGroup& group : storage_interest_groups_) {
    expiry_ = std::min(expiry_, group.interest_group.expiry);
  }
}

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
  // If the cache is disabled, simply call
  // InterestGroupStorage::GetInterestGroupsForOwner on each request.
  if (!base::FeatureList::IsEnabled(features::kFledgeUseInterestGroupCache)) {
    interest_group_storage_
        .AsyncCall(&InterestGroupStorage::GetInterestGroupsForOwner)
        .WithArgs(owner)
        .Then(base::BindOnce(&InterestGroupCachingStorage::
                                 OnLoadInterestGroupsForOwnerOneCallback,
                             weak_factory_.GetWeakPtr(), owner,
                             std::move(callback)));
    return;
  }

  // If there is a cache hit, use the in-memory object.
  auto cached_groups_it = cached_interest_groups_.find(owner);
  if (cached_groups_it != cached_interest_groups_.end() &&
      cached_groups_it->second.get() &&
      !cached_groups_it->second->IsExpired()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  scoped_refptr<StorageInterestGroups>(
                                      cached_groups_it->second.get())));
    base::UmaHistogramBoolean("Ads.InterestGroup.Auction.LoadGroupsCacheHit",
                              true);
    return;
  }
  base::UmaHistogramBoolean("Ads.InterestGroup.Auction.LoadGroupsCacheHit",
                            false);

  // If there is no cache hit, run
  // InterestGroupStorage::GetInterestGroupsForOwner if there are no
  // outstanding calls or if the cache has been invalidated since an
  // outstanding call. Otherwise, allow the callback to use the result of an
  // outstanding call.
  auto outstanding_callbacks_it =
      outstanding_interest_groups_for_owner_callbacks_.find(owner);
  if (outstanding_callbacks_it ==
      outstanding_interest_groups_for_owner_callbacks_.end()) {
    outstanding_interest_groups_for_owner_callbacks_[owner].push(
        std::move(callback));
    interest_group_storage_
        .AsyncCall(&InterestGroupStorage::GetInterestGroupsForOwner)
        .WithArgs(owner)
        .Then(base::BindOnce(
            &InterestGroupCachingStorage::OnLoadInterestGroupsForOwner,
            weak_factory_.GetWeakPtr(), owner));
    base::UmaHistogramBoolean(
        "Ads.InterestGroup.Auction.LoadGroupsUseInProgressLoad", false);
  } else if (outdated_outstanding_interest_group_loads_.contains(owner)) {
    // We can't add the callback to the queue or it would get an outdated
    // result. Load a fresh result.
    interest_group_storage_
        .AsyncCall(&InterestGroupStorage::GetInterestGroupsForOwner)
        .WithArgs(owner)
        .Then(base::BindOnce(&InterestGroupCachingStorage::
                                 OnLoadInterestGroupsForOwnerOneCallback,
                             weak_factory_.GetWeakPtr(), owner,
                             std::move(callback)));
    base::UmaHistogramBoolean(
        "Ads.InterestGroup.Auction.LoadGroupsUseInProgressLoad", false);

  } else {
    outstanding_callbacks_it->second.push(std::move(callback));
    base::UmaHistogramBoolean(
        "Ads.InterestGroup.Auction.LoadGroupsUseInProgressLoad", true);
  }
}

void InterestGroupCachingStorage::JoinInterestGroup(
    const blink::InterestGroup& group,
    const GURL& main_frame_joining_url,
    base::OnceClosure callback) {
  InvalidateCachedInterestGroupsForOwner(group.owner);
  interest_group_storage_.AsyncCall(&InterestGroupStorage::JoinInterestGroup)
      .WithArgs(std::move(group), std::move(main_frame_joining_url))
      .Then(std::move(callback));
}

void InterestGroupCachingStorage::LeaveInterestGroup(
    const blink::InterestGroupKey& group_key,
    const url::Origin& main_frame,
    base::OnceClosure callback) {
  InvalidateCachedInterestGroupsForOwner(group_key.owner);
  interest_group_storage_.AsyncCall(&InterestGroupStorage::LeaveInterestGroup)
      .WithArgs(group_key, main_frame)
      .Then(std::move(callback));
}

void InterestGroupCachingStorage::ClearOriginJoinedInterestGroups(
    const url::Origin& owner,
    const std::set<std::string>& interest_groups_to_keep,
    const url::Origin& main_frame_origin,
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  InvalidateCachedInterestGroupsForOwner(owner);
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
  InvalidateCachedInterestGroupsForOwner(group_key.owner);
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
  // Update the cached objects with the new bid counts instead of invalidating
  // the cache.
  std::set<url::Origin> bidding_owners;
  for (const blink::InterestGroupKey& group_key : groups) {
    bidding_owners.emplace(group_key.owner);
  }
  for (const url::Origin& owner : bidding_owners) {
    if (outstanding_interest_groups_for_owner_callbacks_.find(owner) !=
        outstanding_interest_groups_for_owner_callbacks_.end()) {
      outdated_outstanding_interest_group_loads_.emplace(owner);
    }
    auto cached_groups_it = cached_interest_groups_.find(owner);
    if (cached_groups_it == cached_interest_groups_.end() ||
        !cached_groups_it->second.get()) {
      continue;
    }
    for (StorageInterestGroup& cached_group :
         cached_groups_it->second->storage_interest_groups_) {
      if (groups.contains(blink::InterestGroupKey(
              owner, cached_group.interest_group.name))) {
        cached_group.bidding_browser_signals->bid_count += 1;
      }
    }
  }

  interest_group_storage_
      .AsyncCall(&InterestGroupStorage::RecordInterestGroupBids)
      .WithArgs(groups);
}

void InterestGroupCachingStorage::RecordInterestGroupWin(
    const blink::InterestGroupKey& group_key,
    const std::string& ad_json) {
  InvalidateCachedInterestGroupsForOwner(group_key.owner);
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
  InvalidateAllCachedInterestGroups();
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
  InvalidateCachedInterestGroupsForOwner(owner);
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
  InvalidateAllCachedInterestGroups();
  interest_group_storage_
      .AsyncCall(&InterestGroupStorage::DeleteInterestGroupData)
      .WithArgs(std::move(storage_key_matcher))
      .Then(std::move(callback));
}
void InterestGroupCachingStorage::DeleteAllInterestGroupData(
    base::OnceClosure callback) {
  InvalidateAllCachedInterestGroups();
  interest_group_storage_
      .AsyncCall(&InterestGroupStorage::DeleteAllInterestGroupData)
      .Then(std::move(callback));
}
void InterestGroupCachingStorage::SetInterestGroupPriority(
    const blink::InterestGroupKey& group_key,
    double priority) {
  InvalidateCachedInterestGroupsForOwner(group_key.owner);
  interest_group_storage_
      .AsyncCall(&InterestGroupStorage::SetInterestGroupPriority)
      .WithArgs(group_key, priority);
}

void InterestGroupCachingStorage::UpdateInterestGroupPriorityOverrides(
    const blink::InterestGroupKey& group_key,
    base::flat_map<std::string,
                   auction_worklet::mojom::PrioritySignalsDoublePtr>
        update_priority_signals_overrides) {
  InvalidateCachedInterestGroupsForOwner(group_key.owner);
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

void InterestGroupCachingStorage::OnLoadInterestGroupsForOwnerOneCallback(
    const url::Origin& owner,
    base::OnceCallback<void(scoped_refptr<StorageInterestGroups>)> callback,
    std::vector<StorageInterestGroup> interest_groups) {
  scoped_refptr<StorageInterestGroups> interest_groups_ptr =
      base::MakeRefCounted<StorageInterestGroups>(std::move(interest_groups));
  if (base::FeatureList::IsEnabled(features::kFledgeUseInterestGroupCache)) {
    cached_interest_groups_[owner] = interest_groups_ptr->GetWeakPtr();
  }
  std::move(callback).Run(std::move(interest_groups_ptr));
}

void InterestGroupCachingStorage::OnLoadInterestGroupsForOwner(
    const url::Origin& owner,
    std::vector<StorageInterestGroup> interest_groups) {
  scoped_refptr<StorageInterestGroups> interest_groups_ptr =
      base::MakeRefCounted<StorageInterestGroups>(std::move(interest_groups));
  cached_interest_groups_[owner] = interest_groups_ptr->GetWeakPtr();

  auto outstanding_callbacks_it =
      outstanding_interest_groups_for_owner_callbacks_.find(owner);
  if (outstanding_callbacks_it ==
      outstanding_interest_groups_for_owner_callbacks_.end()) {
    return;
  }
  while (!outstanding_callbacks_it->second.empty()) {
    std::move(outstanding_callbacks_it->second.front())
        .Run(interest_groups_ptr);
    outstanding_callbacks_it->second.pop();
  }
  outstanding_interest_groups_for_owner_callbacks_.erase(owner);
  outdated_outstanding_interest_group_loads_.erase(owner);
}

void InterestGroupCachingStorage::InvalidateCachedInterestGroupsForOwner(
    const url::Origin& owner) {
  cached_interest_groups_.erase(owner);
  if (outstanding_interest_groups_for_owner_callbacks_.find(owner) !=
      outstanding_interest_groups_for_owner_callbacks_.end()) {
    outdated_outstanding_interest_group_loads_.emplace(owner);
  }
}

void InterestGroupCachingStorage::InvalidateAllCachedInterestGroups() {
  cached_interest_groups_.clear();
  for (const auto& [owner, _] :
       outstanding_interest_groups_for_owner_callbacks_) {
    outdated_outstanding_interest_group_loads_.emplace(owner);
  }
}

}  // namespace content
