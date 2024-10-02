// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_CACHING_STORAGE_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_CACHING_STORAGE_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_storage.h"
#include "content/browser/interest_group/interest_group_update.h"
#include "content/browser/interest_group/storage_interest_group.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "url/origin.h"

namespace content {

struct DebugReportLockoutAndCooldowns;

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

  std::optional<SingleStorageInterestGroup> FindGroup(std::string_view name);

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
// occurs). It also provides cached values of the owner and bidding signals
// origins so that they can be prefetched before loading interest groups.
class CONTENT_EXPORT InterestGroupCachingStorage {
 public:
  static constexpr base::TimeDelta kMinimumCacheHoldTime = base::Seconds(10);
  struct CONTENT_EXPORT CachedOriginsInfo {
    CachedOriginsInfo();
    explicit CachedOriginsInfo(const blink::InterestGroup& group);

    CachedOriginsInfo(const CachedOriginsInfo& other) = delete;
    CachedOriginsInfo& operator=(const CachedOriginsInfo& other) = delete;
    CachedOriginsInfo(CachedOriginsInfo&& other);
    CachedOriginsInfo& operator=(CachedOriginsInfo&& other);

    ~CachedOriginsInfo();

    // The name of an owner's latest expiring interest group (of the interest
    // groups encountered by the cache via a join or load).
    std::string interest_group_name;
    // The expiry of the interest group.
    base::Time expiry = base::Time::Min();
    // The bidding signals origin of the interest group, if it's non-null and
    // different from the owner.
    std::optional<url::Origin> bidding_signals_origin;
  };

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
  // is loaded fresh from the database or the request is combined with an
  // outstanding database call (if an outstanding call exists and the cache has
  // not been invalidated since that call).
  void GetInterestGroupsForOwner(
      const url::Origin& owner,
      base::OnceCallback<void(scoped_refptr<StorageInterestGroups>)> callback);

  // For a given `owner`, return whether the owner origin and bidding signal
  // origin were cached in-memory in previous calls to
  // GetInterestGroupsForOwner and JoinInterestGroup. If the `owner` origin was
  // cached, update `signals_origin` to the one that was cached -- or set to
  // nullopt if no bidding signals origin was cached or if it would be the same
  // as the owner origin. The cache includes at most one entry per origin, and
  // may not reflect the results of interest group updates. It's intended to be
  // used for best-effort preconnecting, and should not be considered
  // authoritative. It is guaranteed not to contain interest groups that have
  // are beyond the max expiration time limit, so preconnecting should not leak
  // data the bidder would otherwise have access to, if it so desired. That is,
  // manual voluntarily removing or expiring of an interest group may not be
  // reflected in the result, but hitting the the global interest group lifetime
  // cap will be respected.
  bool GetCachedOwnerAndSignalsOrigins(
      const url::Origin& owner,
      std::optional<url::Origin>& signals_origin);

  // Joins an interest group. If the interest group does not exist, a new one
  // is created based on the provided group information. If the interest group
  // exists, the existing interest group is overwritten. In either case a join
  // record for this interest group is created. Returns the necessary
  // information for a k-anon update if the join was successful, or nullopt if
  // not.
  void JoinInterestGroup(
      const blink::InterestGroup& group,
      const GURL& main_frame_joining_url,
      base::OnceCallback<void(std::optional<InterestGroupKanonUpdateParameter>)>
          callback);

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
  // data in `update` is not valid), returns nullopt. Otherwise, returns the
  // information required a k-anon update.
  void UpdateInterestGroup(
      const blink::InterestGroupKey& group_key,
      InterestGroupUpdate update,
      base::OnceCallback<void(std::optional<InterestGroupKanonUpdateParameter>)>
          callback);
  // Allows the interest group specified by `group_key` to be updated if it was
  // last updated before `update_if_older_than`.
  void AllowUpdateIfOlderThan(blink::InterestGroupKey group_key,
                              base::TimeDelta update_if_older_than);
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
  // Adds an entry to forDebuggingOnly report lockout table if the table is
  // empty. Otherwise replaces the existing entry.
  void RecordDebugReportLockout(base::Time last_report_sent_time);
  // Adds an entry to forDebuggingOnly report cooldown table for `origin` if it
  // does not exist, otherwise replaces the existing entry.
  void RecordDebugReportCooldown(const url::Origin& origin,
                                 base::Time cooldown_start,
                                 DebugReportCooldownType cooldown_type);
  // Records a K-anonymity update for an interest group. If
  // `replace_existing_values` is true, this update will store the new
  // `update_time` and `positive_hashed_values`, replacing the interest
  // group's existing update time and keys. If `replace_existing_values` is
  // false, `positive_hashed_keys` will be added to the existing positive keys
  // without updating the stored update time.  No value is stored if
  // `update_time` is older than the `update_time` already stored in the
  // database.
  void UpdateKAnonymity(const blink::InterestGroupKey& interest_group_key,
                        const std::vector<std::string>& positive_hashed_keys,
                        const base::Time update_time,
                        bool replace_existing_values = true);

  // Gets the last time that the key was reported to the k-anonymity server.
  void GetLastKAnonymityReported(
      const std::string& hashed_key,
      base::OnceCallback<void(std::optional<base::Time>)> callback);
  // Updates the last time that the key was reported to the k-anonymity server.
  void UpdateLastKAnonymityReported(const std::string& hashed_key);

  // Gets a single interest group.
  void GetInterestGroup(
      const blink::InterestGroupKey& group_key,
      base::OnceCallback<void(std::optional<SingleStorageInterestGroup>)>
          callback);
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

  // Gets lockout for sending forDebuggingOnly reports.
  void GetDebugReportLockout(
      base::OnceCallback<void(std::optional<base::Time>)> callback);

  // Gets lockout and cooldown for sending forDebuggingOnly reports.
  void GetDebugReportLockoutAndCooldowns(
      base::flat_set<url::Origin> origins,
      base::OnceCallback<void(std::optional<DebugReportLockoutAndCooldowns>)>
          callback);

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

  // Update B&A keys for a coordinator. This function will overwrite any
  // existing keys for the coordinator.
  void SetBiddingAndAuctionServerKeys(
      const url::Origin& coordinator,
      const std::vector<BiddingAndAuctionServerKey>& keys,
      base::Time expiration);
  // Load stored B&A server keys for a coordinator along with the keys'
  // expiration.
  void GetBiddingAndAuctionServerKeys(
      const url::Origin& coordinator,
      base::OnceCallback<
          void(std::pair<base::Time, std::vector<BiddingAndAuctionServerKey>>)>
          callback);

  void GetLastMaintenanceTimeForTesting(
      base::RepeatingCallback<void(base::Time)> callback) const;

 private:
  // Once JoinInterestGroup completes successfully, maybe cache the associated
  // CachedOriginsInfo and run the callback.
  void OnJoinInterestGroup(
      const url::Origin& owner,
      CachedOriginsInfo cached_origins_info,
      base::OnceCallback<void(std::optional<InterestGroupKanonUpdateParameter>)>
          callback,
      std::optional<InterestGroupKanonUpdateParameter> update);

  // After the async call to load interest groups from storage, cache the result
  // in a StorageInterestGroups. Also call
  // callbacks in outstanding_interest_group_for_owner_callbacks_ with a
  // pointer to the just-stored result if the callbacks reference the same
  // version.
  void OnLoadInterestGroupsForOwner(
      const url::Origin& owner,
      uint32_t version,
      std::vector<StorageInterestGroup> interest_groups);

  // This callback is used once interest groups are loaded if
  // kFledgeUseInterestGroupCache is disabled. Information may still be cached
  // if kFledgeUsePreconnectCache is enabled.
  void OnLoadInterestGroupsForOwnerNoCachingIGs(
      const url::Origin& owner,
      base::OnceCallback<void(scoped_refptr<StorageInterestGroups>)> callback,
      std::vector<StorageInterestGroup> interest_groups);

  void InvalidateCachedInterestGroupsForOwner(const url::Origin& owner);
  void InvalidateAllCachedInterestGroups();

  void MarkOutstandingInterestGroupLoadResultOutdated(const url::Origin& owner);

  // Start a timer that holds a reference to `groups` so that it stays in memory
  // for a minimum amount of time (kMinimumCacheHoldTime). If such a timer
  // already exists, restart it.
  void StartTimerForInterestGroupHold(
      const url::Origin& owner,
      scoped_refptr<StorageInterestGroups> groups);

  // Callback for the timers in `timed_holds_of_interest_groups_` in
  // order to keep `groups` in memory for a minimum amount of time
  // (kMinimumCacheHoldTime). When a timer in `timed_holds_of_interest_groups_`
  // is done, make sure to delete the timer.
  void OnMinimumCacheHoldTimeCompleted(
      const url::Origin& owner,
      scoped_refptr<StorageInterestGroups> groups) {
    timed_holds_of_interest_groups_.erase(owner);
  }

  // Update `cached_owners_and_signals_origins_` for an owner's interest groups
  // if kFledgeUsePreconnectCache is enabled.
  void UpdateCachedOriginsIfEnabled(
      const url::Origin& owner,
      const std::vector<StorageInterestGroup>& interest_groups);

  base::SequenceBound<InterestGroupStorage> interest_group_storage_;

  // Used to retrieve interest groups that are still in memory (e.g. because
  // they're bidding in an auction).
  std::map<url::Origin, base::WeakPtr<StorageInterestGroups>>
      cached_interest_groups_;

  // Holds timers that have references to StorageInterestGroups so that the
  // StorageInterestGroups stay in memory for a minimum amount of time
  // (kMinimumCacheHoldTime). The timers can also be cancelled early upon cache
  // invalidation.
  std::map<url::Origin, std::unique_ptr<base::OneShotTimer>>
      timed_holds_of_interest_groups_;

  // Holds callbacks to be run once a load from the database
  // (GetInterestGroupsForOwner) is complete. Callbacks are keyed by version
  // number in addition to owner so that OnLoadInterestGroupsForOwner does not
  // load callbacks asking for a later version of the interest groups.
  std::map<std::pair<url::Origin, uint32_t>,
           base::queue<
               base::OnceCallback<void(scoped_refptr<StorageInterestGroups>)>>>
      interest_groups_sequenced_callbacks_;

  // For each owner, store the current data version for interest group results.
  // A version is incremented when an owner's interest group results are
  // invalidated. The versions are reset when
  // interest_groups_sequenced_callbacks_ becomes empty.
  std::map<url::Origin, uint32_t> valid_interest_group_versions_;

  // For each owner for which we've loaded or joined interest groups,
  // hold onto the owner origin and origin of the bidding signals url for the
  // purpose of preconnecting to them in later auctions. CachedOriginsInfo
  // tracks the latest expiring interest group that we know about to prevent
  // preconnecting to origins no longer in the database. Owners may be cleared
  // from the map if the corresponding interest group is left or expired.
  // A flat map is used because the number of interest group owners is expected
  // to be relatively small.
  base::flat_map<url::Origin, CachedOriginsInfo>
      cached_owners_and_signals_origins_;

  base::WeakPtrFactory<InterestGroupCachingStorage> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_CACHING_STORAGE_H_
