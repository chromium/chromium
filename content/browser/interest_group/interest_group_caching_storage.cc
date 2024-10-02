// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_caching_storage.h"

#include <algorithm>
#include <cstdint>

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "content/browser/interest_group/bidding_and_auction_server_key_fetcher.h"
#include "content/browser/interest_group/interest_group_features.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/interest_group/interest_group_storage.h"
#include "content/browser/interest_group/storage_interest_group.h"
#include "content/common/features.h"
#include "content/public/common/content_features.h"
#include "url/origin.h"

namespace {

std::optional<content::SingleStorageInterestGroup>
ConvertOptionalGroupToSingleStorageInterestGroup(
    std::optional<content::StorageInterestGroup> possible_group) {
  if (possible_group) {
    return content::SingleStorageInterestGroup(
        std::move(possible_group.value()));
  }
  return std::nullopt;
}

}  // namespace
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

std::optional<SingleStorageInterestGroup> StorageInterestGroups::FindGroup(
    std::string_view name) {
  for (const StorageInterestGroup& interest_group : storage_interest_groups_) {
    if (interest_group.interest_group.name == name) {
      SingleStorageInterestGroup output(this, &interest_group);
      return output;
    }
  }
  return std::nullopt;
}

StorageInterestGroups::~StorageInterestGroups() = default;

base::WeakPtr<StorageInterestGroups> StorageInterestGroups::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

InterestGroupCachingStorage::CachedOriginsInfo::CachedOriginsInfo() = default;

InterestGroupCachingStorage::CachedOriginsInfo::CachedOriginsInfo(
    const blink::InterestGroup& group)
    : interest_group_name(group.name), expiry(group.expiry) {
  if (group.trusted_bidding_signals_url.has_value()) {
    url::Origin signals_origin =
        url::Origin::Create(group.trusted_bidding_signals_url.value());
    if (signals_origin != group.owner) {
      bidding_signals_origin = std::move(signals_origin);
    }
  }
}

InterestGroupCachingStorage::CachedOriginsInfo::CachedOriginsInfo(
    InterestGroupCachingStorage::CachedOriginsInfo&& other) = default;

InterestGroupCachingStorage::CachedOriginsInfo&
InterestGroupCachingStorage::CachedOriginsInfo::operator=(
    InterestGroupCachingStorage::CachedOriginsInfo&& other) = default;

InterestGroupCachingStorage::CachedOriginsInfo::~CachedOriginsInfo() = default;

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
                                 OnLoadInterestGroupsForOwnerNoCachingIGs,
                             weak_factory_.GetWeakPtr(), owner,
                             std::move(callback)));
    return;
  }

  // If there is a cache hit, use the in-memory object.
  auto cached_groups_it = cached_interest_groups_.find(owner);
  if (cached_groups_it != cached_interest_groups_.end()) {
    scoped_refptr<StorageInterestGroups> groups =
        cached_groups_it->second.get();
    if (groups && !groups->IsExpired()) {
      StartTimerForInterestGroupHold(owner, groups);
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::move(groups)));

      base::UmaHistogramBoolean("Ads.InterestGroup.Auction.LoadGroupsCacheHit",
                                true);
      return;
    }
  }
  base::UmaHistogramBoolean("Ads.InterestGroup.Auction.LoadGroupsCacheHit",
                            false);

  // If there is no cache hit, run
  // InterestGroupStorage::GetInterestGroupsForOwner if there are no
  // outstanding calls or if the cache has been invalidated since an
  // outstanding call. Otherwise, allow the callback to use the result of an
  // outstanding call.
  base::queue<base::OnceCallback<void(scoped_refptr<StorageInterestGroups>)>>&
      callback_queue = interest_groups_sequenced_callbacks_[std::make_pair(
          owner, valid_interest_group_versions_[owner])];

  if (callback_queue.empty()) {
    interest_group_storage_
        .AsyncCall(&InterestGroupStorage::GetInterestGroupsForOwner)
        .WithArgs(owner)
        .Then(base::BindOnce(
            &InterestGroupCachingStorage::OnLoadInterestGroupsForOwner,
            weak_factory_.GetWeakPtr(), owner,
            valid_interest_group_versions_[owner]));
    base::UmaHistogramBoolean(
        "Ads.InterestGroup.Auction.LoadGroupsUseInProgressLoad", false);
  } else {
    base::UmaHistogramBoolean(
        "Ads.InterestGroup.Auction.LoadGroupsUseInProgressLoad", true);
  }

  callback_queue.push(std::move(callback));
}

bool InterestGroupCachingStorage::GetCachedOwnerAndSignalsOrigins(
    const url::Origin& owner,
    std::optional<url::Origin>& signals_origin) {
  auto it = cached_owners_and_signals_origins_.find(owner);
  if (it == cached_owners_and_signals_origins_.end()) {
    return false;
  }
  if (it->second.expiry < base::Time::Now()) {
    cached_owners_and_signals_origins_.erase(it);
    return false;
  }
  signals_origin = it->second.bidding_signals_origin;
  return true;
}

void InterestGroupCachingStorage::JoinInterestGroup(
    const blink::InterestGroup& group,
    const GURL& main_frame_joining_url,
    base::OnceCallback<void(std::optional<InterestGroupKanonUpdateParameter>)>
        callback) {
  InvalidateCachedInterestGroupsForOwner(group.owner);
  interest_group_storage_.AsyncCall(&InterestGroupStorage::JoinInterestGroup)
      .WithArgs(std::move(group), std::move(main_frame_joining_url))
      .Then(base::BindOnce(&InterestGroupCachingStorage::OnJoinInterestGroup,
                           weak_factory_.GetWeakPtr(), group.owner,
                           CachedOriginsInfo(group), std::move(callback)));
}

void InterestGroupCachingStorage::LeaveInterestGroup(
    const blink::InterestGroupKey& group_key,
    const url::Origin& main_frame,
    base::OnceClosure callback) {
  InvalidateCachedInterestGroupsForOwner(group_key.owner);
  auto it = cached_owners_and_signals_origins_.find(group_key.owner);
  if (it != cached_owners_and_signals_origins_.end() &&
      (it->second.interest_group_name == group_key.name ||
       it->second.expiry < base::Time::Now())) {
    cached_owners_and_signals_origins_.erase(it);
  }
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
  auto it = cached_owners_and_signals_origins_.find(owner);
  if (it != cached_owners_and_signals_origins_.end() &&
      (!interest_groups_to_keep.contains(it->second.interest_group_name) ||
       it->second.expiry < base::Time::Now())) {
    cached_owners_and_signals_origins_.erase(it);
  }
  interest_group_storage_
      .AsyncCall(&InterestGroupStorage::ClearOriginJoinedInterestGroups)
      .WithArgs(std::move(owner), std::move(interest_groups_to_keep),
                std::move(main_frame_origin))
      .Then(std::move(callback));
}

void InterestGroupCachingStorage::UpdateInterestGroup(
    const blink::InterestGroupKey& group_key,
    InterestGroupUpdate update,
    base::OnceCallback<void(std::optional<InterestGroupKanonUpdateParameter>)>
        callback) {
  InvalidateCachedInterestGroupsForOwner(group_key.owner);
  if (update.trusted_bidding_signals_url) {
    auto it = cached_owners_and_signals_origins_.find(group_key.owner);
    if (it != cached_owners_and_signals_origins_.end() &&
        ((it->second.interest_group_name == group_key.name &&
          it->second.bidding_signals_origin !=
              url::Origin::Create(*update.trusted_bidding_signals_url)) ||
         it->second.expiry < base::Time::Now())) {
      // Instead of modifying the existing cache entry, erase it in case the
      // update is invalid or doesn't succeed.
      cached_owners_and_signals_origins_.erase(it);
    }
  }
  interest_group_storage_.AsyncCall(&InterestGroupStorage::UpdateInterestGroup)
      .WithArgs(group_key, std::move(update))
      .Then(std::move(callback));
}

void InterestGroupCachingStorage::AllowUpdateIfOlderThan(
    blink::InterestGroupKey group_key,
    base::TimeDelta update_if_older_than) {
  interest_group_storage_
      .AsyncCall(&InterestGroupStorage::AllowUpdateIfOlderThan)
      .WithArgs(std::move(group_key), update_if_older_than);
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
    MarkOutstandingInterestGroupLoadResultOutdated(owner);
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

void InterestGroupCachingStorage::RecordDebugReportLockout(
    base::Time last_report_sent_time) {
  interest_group_storage_
      .AsyncCall(&InterestGroupStorage::RecordDebugReportLockout)
      .WithArgs(last_report_sent_time);
}

void InterestGroupCachingStorage::RecordDebugReportCooldown(
    const url::Origin& origin,
    base::Time cooldown_start,
    DebugReportCooldownType cooldown_type) {
  interest_group_storage_
      .AsyncCall(&InterestGroupStorage::RecordDebugReportCooldown)
      .WithArgs(origin, cooldown_start, cooldown_type);
}

void InterestGroupCachingStorage::UpdateKAnonymity(
    const blink::InterestGroupKey& interest_group_key,
    const std::vector<std::string>& positive_hashed_keys,
    const base::Time update_time,
    bool replace_existing_values) {
  // We do not know the affected owners without looking them up from the
  // database or calculating k-anon keys for all the ads in the cache. Both are
  // expensive, and this function will run many times per interest group, so
  // prefer to over-delete data here.
  InvalidateAllCachedInterestGroups();
  interest_group_storage_.AsyncCall(&InterestGroupStorage::UpdateKAnonymity)
      .WithArgs(interest_group_key, positive_hashed_keys, update_time,
                replace_existing_values);
}

void InterestGroupCachingStorage::GetLastKAnonymityReported(
    const std::string& hashed_key,
    base::OnceCallback<void(std::optional<base::Time>)> callback) {
  interest_group_storage_
      .AsyncCall(&InterestGroupStorage::GetLastKAnonymityReported)
      .WithArgs(hashed_key)
      .Then(std::move(callback));
}

void InterestGroupCachingStorage::UpdateLastKAnonymityReported(
    const std::string& hashed_key) {
  // We don't need to invalidate any cached objects here because this value is
  // not loaded in GetInterestGroupsForOwner.
  interest_group_storage_
      .AsyncCall(&InterestGroupStorage::UpdateLastKAnonymityReported)
      .WithArgs(hashed_key);
}

void InterestGroupCachingStorage::GetInterestGroup(
    const blink::InterestGroupKey& group_key,
    base::OnceCallback<void(std::optional<SingleStorageInterestGroup>)>
        callback) {
  if (base::FeatureList::IsEnabled(features::kFledgeUseInterestGroupCache)) {
    auto cached_groups_it = cached_interest_groups_.find(group_key.owner);
    if (cached_groups_it != cached_interest_groups_.end()) {
      scoped_refptr<StorageInterestGroups> groups =
          cached_groups_it->second.get();
      if (groups) {
        std::optional<SingleStorageInterestGroup> output =
            groups->FindGroup(group_key.name);
        if (output &&
            output.value()->interest_group.expiry < base::Time::Now()) {
          output.reset();
        }
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), std::move(output)));
        base::UmaHistogramBoolean("Ads.InterestGroup.GetInterestGroupCacheHit",
                                  true);
        return;
      }
    }
    base::UmaHistogramBoolean("Ads.InterestGroup.GetInterestGroupCacheHit",
                              false);
  }
  interest_group_storage_.AsyncCall(&InterestGroupStorage::GetInterestGroup)
      .WithArgs(group_key)
      .Then(base::BindOnce(&ConvertOptionalGroupToSingleStorageInterestGroup)
                .Then(std::move(callback)));
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

void InterestGroupCachingStorage::GetDebugReportLockout(
    base::OnceCallback<void(std::optional<base::Time>)> callback) {
  return interest_group_storage_
      .AsyncCall(&InterestGroupStorage::GetDebugReportLockout)
      .Then(std::move(callback));
}

void InterestGroupCachingStorage::GetDebugReportLockoutAndCooldowns(
    base::flat_set<url::Origin> origins,
    base::OnceCallback<void(std::optional<DebugReportLockoutAndCooldowns>)>
        callback) {
  return interest_group_storage_
      .AsyncCall(&InterestGroupStorage::GetDebugReportLockoutAndCooldowns)
      .WithArgs(std::move(origins))
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
  cached_owners_and_signals_origins_.clear();
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

void InterestGroupCachingStorage::SetBiddingAndAuctionServerKeys(
    const url::Origin& coordinator,
    const std::vector<BiddingAndAuctionServerKey>& keys,
    base::Time expiration) {
  interest_group_storage_
      .AsyncCall(&InterestGroupStorage::SetBiddingAndAuctionServerKeys)
      .WithArgs(coordinator, keys, expiration);
}
void InterestGroupCachingStorage::GetBiddingAndAuctionServerKeys(
    const url::Origin& coordinator,
    base::OnceCallback<
        void(std::pair<base::Time, std::vector<BiddingAndAuctionServerKey>>)>
        callback) {
  interest_group_storage_
      .AsyncCall(&InterestGroupStorage::GetBiddingAndAuctionServerKeys)
      .WithArgs(coordinator)
      .Then(std::move(callback));
}

void InterestGroupCachingStorage::GetLastMaintenanceTimeForTesting(
    base::RepeatingCallback<void(base::Time)> callback) const {
  interest_group_storage_
      .AsyncCall(&InterestGroupStorage::GetLastMaintenanceTimeForTesting)
      .Then(std::move(callback));
}

void InterestGroupCachingStorage::OnLoadInterestGroupsForOwnerNoCachingIGs(
    const url::Origin& owner,
    base::OnceCallback<void(scoped_refptr<StorageInterestGroups>)> callback,
    std::vector<StorageInterestGroup> interest_groups) {
  UpdateCachedOriginsIfEnabled(owner, interest_groups);
  scoped_refptr<StorageInterestGroups> interest_groups_ptr =
      base::MakeRefCounted<StorageInterestGroups>(std::move(interest_groups));
  std::move(callback).Run(std::move(interest_groups_ptr));
}

void InterestGroupCachingStorage::OnJoinInterestGroup(
    const url::Origin& owner,
    CachedOriginsInfo cached_origins_info,
    base::OnceCallback<void(std::optional<InterestGroupKanonUpdateParameter>)>
        callback,
    std::optional<InterestGroupKanonUpdateParameter> update) {
  if (update) {
    auto it = cached_owners_and_signals_origins_.find(owner);
    if (it != cached_owners_and_signals_origins_.end()) {
      if (it->second.interest_group_name ==
              cached_origins_info.interest_group_name ||
          it->second.expiry < cached_origins_info.expiry) {
        it->second = std::move(cached_origins_info);
      }
    } else {
      cached_owners_and_signals_origins_[owner] =
          std::move(cached_origins_info);
    }
  }
  std::move(callback).Run(std::move(update));
}

void InterestGroupCachingStorage::OnLoadInterestGroupsForOwner(
    const url::Origin& owner,
    uint32_t version,
    std::vector<StorageInterestGroup> interest_groups) {
  auto outstanding_callbacks_it =
      interest_groups_sequenced_callbacks_.find(std::make_pair(owner, version));
  if (outstanding_callbacks_it == interest_groups_sequenced_callbacks_.end()) {
    return;
  }

  UpdateCachedOriginsIfEnabled(owner, interest_groups);

  scoped_refptr<StorageInterestGroups> interest_groups_ptr =
      base::MakeRefCounted<StorageInterestGroups>(std::move(interest_groups));

  // Cache the result only if it's still valid.
  if (version == valid_interest_group_versions_[owner]) {
    cached_interest_groups_[owner] = interest_groups_ptr->GetWeakPtr();
    StartTimerForInterestGroupHold(owner, interest_groups_ptr);
  }

  while (!outstanding_callbacks_it->second.empty()) {
    std::move(outstanding_callbacks_it->second.front())
        .Run(interest_groups_ptr);
    outstanding_callbacks_it->second.pop();
  }
  interest_groups_sequenced_callbacks_.erase(outstanding_callbacks_it);
  if (interest_groups_sequenced_callbacks_.empty()) {
    // Reset the versions so that we don't need to have all owners in memory.
    valid_interest_group_versions_.clear();
  }
}

void InterestGroupCachingStorage::InvalidateCachedInterestGroupsForOwner(
    const url::Origin& owner) {
  cached_interest_groups_.erase(owner);
  MarkOutstandingInterestGroupLoadResultOutdated(owner);
  timed_holds_of_interest_groups_.erase(owner);
}

void InterestGroupCachingStorage::InvalidateAllCachedInterestGroups() {
  cached_interest_groups_.clear();
  timed_holds_of_interest_groups_.clear();
  for (const auto& [owner_version, _] : interest_groups_sequenced_callbacks_) {
    MarkOutstandingInterestGroupLoadResultOutdated(owner_version.first);
  }
}

void InterestGroupCachingStorage::
    MarkOutstandingInterestGroupLoadResultOutdated(const url::Origin& owner) {
  auto it = valid_interest_group_versions_.find(owner);
  if (it != valid_interest_group_versions_.end()) {
    it->second = it->second + 1;
  }
}

void InterestGroupCachingStorage::StartTimerForInterestGroupHold(
    const url::Origin& owner,
    scoped_refptr<StorageInterestGroups> groups) {
  // Get the existing timer if it exists or create a new one if not.
  std::unique_ptr<base::OneShotTimer>& timer =
      timed_holds_of_interest_groups_
          .insert(std::make_pair(owner, std::make_unique<base::OneShotTimer>()))
          .first->second;
  if (timer->IsRunning()) {
    timer->Stop();
  }
  timer->Start(
      FROM_HERE, kMinimumCacheHoldTime,
      base::BindOnce(
          &InterestGroupCachingStorage::OnMinimumCacheHoldTimeCompleted,
          weak_factory_.GetWeakPtr(), owner, groups));
}

void InterestGroupCachingStorage::UpdateCachedOriginsIfEnabled(
    const url::Origin& owner,
    const std::vector<StorageInterestGroup>& interest_groups) {
  if (!base::FeatureList::IsEnabled(features::kFledgeUsePreconnectCache)) {
    return;
  }
  if (interest_groups.empty()) {
    cached_owners_and_signals_origins_.erase(owner);
    return;
  }
  CachedOriginsInfo cached_origins_info;
  for (const StorageInterestGroup& group : interest_groups) {
    if (group.interest_group.expiry > cached_origins_info.expiry) {
      cached_origins_info = CachedOriginsInfo(group.interest_group);
    }
  }
  cached_owners_and_signals_origins_[owner] = std::move(cached_origins_info);
}

}  // namespace content
