// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_CACHING_STORAGE_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_CACHING_STORAGE_H_

#include <algorithm>
#include <cstddef>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_storage.h"
#include "content/browser/interest_group/storage_interest_group.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "url/origin.h"

namespace content {

class StorageInterestGroups;
// SingleStorageInterestGroup ensures that pointers to values inside
// StorageInterestGroups are accompanied by a
// scoped_refptr<StorageInterestGroups> to prevent dangling pointers and
// ensures the scoped_refptr<StorageInterestGroups>  and
// raw_ptr<StorageInterestGroup> are destructed in the correct order.
class CONTENT_EXPORT SingleStorageInterestGroup {
 public:
  explicit SingleStorageInterestGroup(
      scoped_refptr<StorageInterestGroups> storage_interest_groups_for_owner,
      const StorageInterestGroup* storage_interest_group);
  SingleStorageInterestGroup(const SingleStorageInterestGroup& other);
  // Create a SingleStorageInterestGroup from scratch, including generating a
  // StorageInterestGroups featuring just `interest_group`.
  explicit SingleStorageInterestGroup(StorageInterestGroup&& interest_group);
  ~SingleStorageInterestGroup();
  SingleStorageInterestGroup& operator=(SingleStorageInterestGroup&& other) =
      default;
  const StorageInterestGroup* operator->() const;
  const StorageInterestGroup& operator*() const;

 private:
  scoped_refptr<StorageInterestGroups> storage_interest_groups_for_owner;
  raw_ptr<const StorageInterestGroup> storage_interest_group;
};

// StorageInterestGroups is needed for InterestGroupCachingStorage
// because it requires weak pointers and ref counted pointers to
// std::vector<StorageInterestGroup>.
class CONTENT_EXPORT StorageInterestGroups
    : public base::RefCounted<StorageInterestGroups> {
 public:
  explicit StorageInterestGroups(
      std::vector<StorageInterestGroup>&& interest_groups);
  StorageInterestGroups(const StorageInterestGroup& other) = delete;

  base::WeakPtr<StorageInterestGroups> GetWeakPtr();

  size_t size() { return storage_interest_groups_.size(); }

  std::vector<SingleStorageInterestGroup> GetInterestGroups() {
    std::vector<SingleStorageInterestGroup> storage_interest_groups;
    for (const StorageInterestGroup& interest_group :
         storage_interest_groups_) {
      storage_interest_groups.emplace_back(this, &interest_group);
    }
    return storage_interest_groups;
  }

  bool IsExpired() { return expiry_ < base::Time::Now(); }

 private:
  friend class RefCounted<StorageInterestGroups>;
  friend class InterestGroupCachingStorage;
  ~StorageInterestGroups();

  std::vector<StorageInterestGroup> storage_interest_groups_;
  base::Time expiry_;
  base::WeakPtrFactory<StorageInterestGroups> weak_ptr_factory_{this};
};

// InterestGroupCachingStorage controls access to the Interest Group Database
// through its owned InterestGroupStorage. InterestGroupStorage should
// not be accessed outside of this class. InterestGroupCachingStorage provides a
// pointer to in-memory values for GetInterestGroupsForOwner when available and
// invalidates the cached values when necessary (when an update to the values
// occurs).
class CONTENT_EXPORT InterestGroupCachingStorage {
 public:
  explicit InterestGroupCachingStorage(const base::FilePath& path,
                                       bool in_memory);
  ~InterestGroupCachingStorage();
  InterestGroupCachingStorage(const InterestGroupCachingStorage& other) =
      delete;
  InterestGroupCachingStorage& operator=(
      const InterestGroupCachingStorage& other) = delete;

  // Gets a list of all interest groups with their bidding information
  // associated with the provided owner. If the result is cached,
  // a pointer to the in-memory StorageInterestGroups is returned. Otherwise, it
  // is loaded fresh from the database.
  void GetInterestGroupsForOwner(
      const url::Origin& owner,
      base::OnceCallback<void(scoped_refptr<StorageInterestGroups>)> callback);

  // Joins an interest group. If the interest group does not exist, a new one
  // is created based on the provided group information. If the interest group
  // exists, the existing interest group is overwritten. In either case a join
  // record for this interest group is created.
  void JoinInterestGroup(const blink::InterestGroup& group,
                         const GURL& main_frame_joining_url,
                         base::OnceClosure callback);

  // Remove the interest group if it exists.
  void LeaveInterestGroup(const blink::InterestGroupKey& group_key,
                          const url::Origin& main_frame,
                          base::OnceClosure callback);

  // Removes all interest groups owned by `owner` joined from
  // `main_frame_origin` except `interest_groups_to_keep`, if they exist.
  void ClearOriginJoinedInterestGroups(
      const url::Origin& owner,
      const std::set<std::string>& interest_groups_to_keep,
      const url::Origin& main_frame_origin,
      base::OnceCallback<void(std::vector<std::string>)> callback);

  // Updates the interest group `name` of `owner` with the populated fields of
  // `update`.
  //
  // If it fails for any reason (e.g., the interest group does not exist, or the
  // data in `update` is not valid), returns false.
  void UpdateInterestGroup(const blink::InterestGroupKey& group_key,
                           InterestGroupUpdate update,
                           base::OnceCallback<void(bool)> callback);
  // Report that updating of the interest group with owner `owner` and name
  // `name` failed. With the exception of parse failures, the rate limit
  // duration for failed updates is shorter than for those that succeed -- for
  // successes, UpdateInterestGroup() automatically updates the rate limit
  // duration.
  void ReportUpdateFailed(const blink::InterestGroupKey& group_key,
                          bool parse_failure);
  // Adds an entry to the bidding history for these interest groups.
  void RecordInterestGroupBids(const blink::InterestGroupSet& groups);
  // Adds an entry to the win history for this interest group. `ad_json` is a
  // piece of opaque data to identify the winning ad.
  void RecordInterestGroupWin(const blink::InterestGroupKey& group_key,
                              const std::string& ad_json);
  // Records K-anonymity.
  void UpdateKAnonymity(const StorageInterestGroup::KAnonymityData& data);

  // Gets the last time that the key was reported to the k-anonymity server.
  void GetLastKAnonymityReported(
      const std::string& key,
      base::OnceCallback<void(absl::optional<base::Time>)> callback);
  // Updates the last time that the key was reported to the k-anonymity server.
  void UpdateLastKAnonymityReported(const std::string& key);

  // Gets a single interest group.
  void GetInterestGroup(
      const blink::InterestGroupKey& group_key,
      base::OnceCallback<void(absl::optional<StorageInterestGroup>)> callback);
  // Gets a list of all interest group owners. Each owner will only appear
  // once.
  void GetAllInterestGroupOwners(
      base::OnceCallback<void(std::vector<url::Origin>)> callback);

  // For a given owner, gets interest group keys along with their update urls.
  // `groups_limit` sets a limit on the maximum number of interest group keys
  // that may be returned.
  void GetInterestGroupsForUpdate(
      const url::Origin& owner,
      int groups_limit,
      base::OnceCallback<void(std::vector<InterestGroupUpdateParameter>)>
          callback);

  // Gets all KAnonymityData for ads part of the interest group specified by
  // `interest_group_key`.
  void GetKAnonymityDataForUpdate(
      const blink::InterestGroupKey& group_key,
      base::OnceCallback<void(
          const std::vector<StorageInterestGroup::KAnonymityData>&)> callback);

  // Gets a list of all interest group joining origins. Each joining origin
  // will only appear once.
  void GetAllInterestGroupJoiningOrigins(
      base::OnceCallback<void(std::vector<url::Origin>)> callback);

  void GetAllInterestGroupOwnerJoinerPairs(
      base::OnceCallback<void(std::vector<std::pair<url::Origin, url::Origin>>)>
          callback);

  void RemoveInterestGroupsMatchingOwnerAndJoiner(url::Origin owner,
                                                  url::Origin joining_origin,
                                                  base::OnceClosure callback);

  // Clear out storage for the matching owning storage key.
  void DeleteInterestGroupData(
      StoragePartition::StorageKeyMatcherFunction storage_key_matcher,
      base::OnceClosure callback);
  // Clear out all interest group storage including k-anonymity store.
  void DeleteAllInterestGroupData(base::OnceClosure callback);
  // Update the interest group priority.
  void SetInterestGroupPriority(const blink::InterestGroupKey& group_key,
                                double priority);

  // Merges `update_priority_signals_overrides` with the currently stored
  // priority signals of `group`. Doesn't take the cached overrides from the
  // caller, which may already have them, in favor of reading them from the
  // database, as the values may have been updated on disk since they were read
  // by the caller.
  void UpdateInterestGroupPriorityOverrides(
      const blink::InterestGroupKey& group_key,
      base::flat_map<std::string,
                     auction_worklet::mojom::PrioritySignalsDoublePtr>
          update_priority_signals_overrides);

  void GetLastMaintenanceTimeForTesting(
      base::RepeatingCallback<void(base::Time)> callback) const;

 private:
  // After the async call to load interest groups from storage, cache the result
  // in a StorageInterestGroups. Also make sure to call
  // any callbacks in outstanding_interest_group_for_owner_callbacks_ with a
  // pointer to the just-stored result.
  void OnLoadInterestGroupsForOwner(
      const url::Origin& owner,
      std::vector<StorageInterestGroup> interest_groups);

  void OnLoadInterestGroupsForOwnerOneCallback(
      const url::Origin& owner,
      base::OnceCallback<void(scoped_refptr<StorageInterestGroups>)> callback,
      std::vector<StorageInterestGroup> interest_groups);

  void InvalidateCachedInterestGroupsForOwner(const url::Origin& owner);
  void InvalidateAllCachedInterestGroups();

  base::SequenceBound<InterestGroupStorage> interest_group_storage_;

  std::map<url::Origin, base::WeakPtr<StorageInterestGroups>>
      cached_interest_groups_;

  std::map<url::Origin,
           base::queue<
               base::OnceCallback<void(scoped_refptr<StorageInterestGroups>)>>>
      outstanding_interest_groups_for_owner_callbacks_;

  std::set<url::Origin> outdated_outstanding_interest_group_loads_;

  base::WeakPtrFactory<InterestGroupCachingStorage> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_CACHING_STORAGE_H_
