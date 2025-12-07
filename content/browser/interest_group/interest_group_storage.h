// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_STORAGE_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_STORAGE_H_

#include <optional>
#include <set>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/interest_group/for_debugging_only_report_util.h"
#include "content/browser/interest_group/interest_group_update.h"
#include "content/browser/interest_group/storage_interest_group.h"
#include "content/common/content_export.h"
#include "content/public/browser/storage_partition.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom-forward.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {
struct InterestGroup;
}
namespace network {
struct AdAuctionEventRecord;
}

namespace content {
// InterestGroupStorage controls access to the Interest Group Database. All
// public functions perform operations on the database and may block. This
// implementation is not thread-safe so all functions should be called from
// within the same sequence.
class CONTENT_EXPORT InterestGroupStorage {
 public:
  static constexpr base::TimeDelta kMaintenanceInterval = base::Hours(1);
  // Gets the default time the database waits idle before maintenance is
  // triggered.
  //
  // NOTE: This is the default value, which can be overridden by
  // CreateWithIdlePeriodForTesting().
  static constexpr base::TimeDelta kDefaultIdlePeriod = base::Seconds(30);
  // How long to store a k-anon key's last join time.
  static constexpr base::TimeDelta kAdditionalKAnonStoragePeriod =
      base::Days(1);
  // After a successful interest group update, delay the next update until
  // kUpdateSucceededBackoffPeriod time has passed.
  static constexpr base::TimeDelta kUpdateSucceededBackoffPeriod =
      base::Days(1);
  // After a failed interest group update, delay the next update until
  // kUpdateFailedBackoffPeriod time has passed.
  static constexpr base::TimeDelta kUpdateFailedBackoffPeriod = base::Hours(1);

  // Constructs an interest group storage based on a SQLite database in the
  // `path`/InterestGroups file. If the path passed in is empty, then the
  // database is instead stored in memory and not persisted to disk.
  explicit InterestGroupStorage(const base::FilePath& path);
  InterestGroupStorage(const InterestGroupStorage& other) = delete;
  InterestGroupStorage& operator=(const InterestGroupStorage& other) = delete;
  ~InterestGroupStorage();

  // Joins an interest group. If the interest group does not exist, a new one
  // is created based on the provided group information. If the interest group
  // exists, the existing interest group is overwritten. In either case a join
  // record for this interest group is created. Returns the necessary
  // information for a k-anon update if the join was successful, or nullopt if
  // not.
  std::optional<InterestGroupKanonUpdateParameter> JoinInterestGroup(
      const blink::InterestGroup& group,
      const GURL& main_frame_joining_url);
  // Remove the interest group if it exists.
  void LeaveInterestGroup(const blink::InterestGroupKey& group_key,
                          const url::Origin& main_frame);

  // Removes all interest groups owned by `owner` joined from
  // `main_frame_origin` except `interest_groups_to_keep`, if they exist.
  // Returns a (possibly empty) list of all interest groups that were cleared.
  std::vector<std::string> ClearOriginJoinedInterestGroups(
      const url::Origin& owner,
      const std::set<std::string>& interest_groups_to_keep,
      const url::Origin& main_frame_origin);

  // Gets lockout and cooldowns of `origins` for sending forDebuggingOnly
  // reports.
  std::optional<DebugReportLockoutAndCooldowns>
  GetDebugReportLockoutAndCooldowns(const base::flat_set<url::Origin>& origins);

  // Gets lockout and all cooldowns for sending forDebuggingOnly reports.
  std::optional<DebugReportLockoutAndCooldowns>
  GetDebugReportLockoutAndAllCooldowns();

  // Updates the interest group `name` of `owner` with the populated fields of
  // `update`.
  //
  // If it fails for any reason (e.g., the interest group does not exist, or the
  // data in `update` is not valid), returns nullopt. Otherwise, returns the
  // information required a k-anon update.
  std::optional<InterestGroupKanonUpdateParameter> UpdateInterestGroup(
      const blink::InterestGroupKey& group_key,
      InterestGroupUpdate update);

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
  void RecordInterestGroupBids(const blink::InterestGroupSet& group);
  // Adds an entry to the win history for this interest group. `ad_json` is a
  // piece of opaque data to identify the winning ad.
  void RecordInterestGroupWin(const blink::InterestGroupKey& group_key,
                              const std::string& ad_json);
  // Adds an entry to forDebuggingOnly report lockout table if the table is
  // empty. Otherwise replaces the existing entry.
  void RecordDebugReportLockout(base::Time starting_time,
                                base::TimeDelta duration);
  // Adds an entry to forDebuggingOnly report cooldown table for `origin` if it
  // does not exist, otherwise replaces the existing entry.
  void RecordDebugReportCooldown(const url::Origin& origin,
                                 base::Time cooldown_start,
                                 DebugReportCooldownType cooldown_type);
  // Clear out all entries for debug report cooldown information.
  void DeleteAllDebugReportCooldowns();

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
                        bool replace_existing_values);

  // Gets the last time that the key was reported to the k-anonymity server.
  std::optional<base::Time> GetLastKAnonymityReported(
      const std::string& hashed_key);
  // Updates the last time that the key was reported to the k-anonymity server.
  void UpdateLastKAnonymityReported(const std::string& hashed_key);

  // Stores the view or click data in `record` so that it may be later included
  // in view / click counts loaded for generateBid() browser signals.
  void RecordViewClick(const network::AdAuctionEventRecord& record);

  // Gets a single interest group.
  std::optional<StorageInterestGroup> GetInterestGroup(
      const blink::InterestGroupKey& group_key);
  // Gets a list of all interest group owners. Each owner will only appear
  // once.
  std::vector<url::Origin> GetAllInterestGroupOwners();
  // Gets a list of all interest groups with their bidding information
  // associated with the provided owner.
  std::vector<StorageInterestGroup> GetInterestGroupsForOwner(
      const url::Origin& owner);
  // For a given owner, gets interest group keys along with their update URLs
  // and joining origin.
  // `groups_limit` sets a limit on the maximum number of interest group keys
  // that may be returned.
  std::vector<InterestGroupUpdateParameter> GetInterestGroupsForUpdate(
      const url::Origin& owner,
      size_t groups_limit);

  // Gets a list of all interest group joining origins. Each joining origin
  // will only appear once.
  std::vector<url::Origin> GetAllInterestGroupJoiningOrigins();

  std::vector<std::pair<url::Origin, url::Origin>>
  GetAllInterestGroupOwnerJoinerPairs();

  // Set forDebuggingOnly lockout to the time until all interest groups that
  // previously joined expires.
  void SetDebugReportLockoutUntilIGExpires();

  void RemoveInterestGroupsMatchingOwnerAndJoiner(
      const url::Origin& owner,
      const url::Origin& joining_origin);

  // Clear out storage for the matching owning storage key.
  void DeleteInterestGroupData(
      StoragePartition::StorageKeyMatcherFunction storage_key_matcher,
      bool user_initiated_deletion);
  // Clear out all interest group storage including k-anonymity store.
  void DeleteAllInterestGroupData();
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

  std::vector<StorageInterestGroup> GetAllInterestGroupsUnfilteredForTesting();

  // Update B&A keys for a coordinator. This function will overwrite any
  // existing keys for the coordinator.
  void SetBiddingAndAuctionServerKeys(const url::Origin& coordinator,
                                      std::string serialized_keys,
                                      base::Time expiration);
  // Load stored B&A server keys for a coordinator along with the keys'
  // expiration.
  std::pair<base::Time, std::string> GetBiddingAndAuctionServerKeys(
      const url::Origin& coordinator);

  // Writes all of these keys to the cache, the first vector with
  // `is_kanon = true`, and the second vector with `is_kanon = false`.
  // On error, returns false; otherwise true. This function will overwrite any
  // previously cached keys.
  bool WriteHashedKAnonymityKeysToCache(
      const std::vector<std::string>& positive_hashed_keys,
      const std::vector<std::string>& negative_hashed_keys,
      base::Time fetch_time);

  struct CONTENT_EXPORT KAnonymityCacheResponse {
    // Unexpired keys found in the cache that are k-anonymous.
    std::vector<std::string> positive_hashed_keys_from_cache;

    // Includes all keys not found in the cache.
    std::vector<std::string> ids_to_query_from_server;

    KAnonymityCacheResponse(
        std::vector<std::string> _positive_hashed_keys_from_cache,
        std::vector<std::string> _ids_to_query_from_server);
    KAnonymityCacheResponse(const KAnonymityCacheResponse& other);
    KAnonymityCacheResponse& operator=(const KAnonymityCacheResponse& other);
    ~KAnonymityCacheResponse();
  };

  // Takes a vector of keys to lookup from the cache. Returns two vectors of
  // keys: the first a vector that includes those unexpired keys for which it
  // was found in the cache that that key is k-anonymous, the second a vector
  // that includes all keys not found in the cache. On error, this returns a
  // `KAnonymityCacheResponse` with empty `positive_hashed_keys_from_cache`
  // and all `keys` in `ids_to_query_from_server`.
  KAnonymityCacheResponse LoadPositiveHashedKAnonymityKeysFromCache(
      const std::vector<std::string>& keys,
      base::Time check_time);

  // Returns various resource limits, as configured by feature params.
  static size_t MaxOwnerRegularInterestGroups();
  static size_t MaxOwnerNegativeInterestGroups();
  static size_t MaxOwnerStorageSize();

  base::Time GetLastMaintenanceTimeForTesting() const;

  static int GetCurrentVersionNumberForTesting();

  // Creates an instance where the idle period is set to `idle_period` instead
  // of kDefaultIdlePeriod.
  static std::unique_ptr<InterestGroupStorage> CreateWithIdlePeriodForTesting(
      const base::FilePath& path,
      base::TimeDelta idle_period);

  // Resets the internal idle timer without performing any database operation.
  //
  // Technically, this test hook isn't necessary, since tests could just call
  // GetAllInterestGroupOwners() and throw away the result, but this method is
  // *much* more performant (due to not doing any I/O), especially when calling
  // in a loop.
  //
  // NOTE: This just calls EnsureDBInitialized() internally.
  void ResetIdleTimerForTesting();

  // Used when compacting clickiness information. Represents the raw uncompacted
  // and compacted protobuf events loaded directly from the
  // view_and_click_events table, for one of either views or clicks.
  //
  // Compacted events have a timestamp and count and are older than 1 hour,
  // whereas uncompacted events only have a timestamp.
  //
  // Exposed for use with `ComputeCompactClickinessForTesting()`.
  struct ClickinessCompactionRawEvents {
    std::string uncompacted_events;
    std::string compacted_events;
  };

  // Computes compacted form of clickiness information stored as protos in
  // the database, combining older events at lower time resolution.
  static std::optional<InterestGroupStorage::ClickinessCompactionRawEvents>
  ComputeCompactClickinessForTesting(
      base::Time now,
      const InterestGroupStorage::ClickinessCompactionRawEvents& raw);

  // Returns whether there is no entry for given
  // (provider_origin, eligible_origin) in database view/click table.
  //
  // Returns nullopt in case of an error.
  std::optional<bool> CheckViewClickCountsForProviderAndEligibleInDbForTesting(
      const url::Origin& provider_origin,
      const url::Origin& eligible_origin);

 private:
  // Private constructor that allows changing the idle period, used by
  // CreateWithIdlePeriodForTesting().
  //
  // `idle_period` may be optionally specified to override in tests the default
  // time the database waits idle before maintenance is triggered
  // (kDefaultIdlePeriod).
  InterestGroupStorage(const base::FilePath& path, base::TimeDelta idle_period);

  bool EnsureDBInitialized();
  bool InitializeDB();
  bool InitializeSchema();
  void PerformDBMaintenance();
  void DatabaseErrorCallback(int extended_error, sql::Statement* stmt);

  const base::FilePath path_to_database_;
  // Maximum number of interest groups, or interest group owners to keep in the
  // database.
  // Set by the related blink::feature parameters
  // kInterestGroupStorageMaxOwners,
  // kInterestGroupStorageMaxGroupsPerOwner, and
  // kInterestGroupStorageMaxNegativeGroupsPerOwner.
  const size_t max_owners_;
  const size_t max_owner_regular_interest_groups_;
  const size_t max_owner_negative_interest_groups_;
  const size_t max_owner_storage_size_;

  // Maximum number of operations allowed between maintenance calls.
  // Set by the related blink::feature parameter
  // kInterestGroupStorageMaxOpsBeforeMaintenance.
  const size_t max_ops_before_maintenance_;

  std::unique_ptr<sql::Database> db_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::DelayTimer db_maintenance_timer_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::Time last_access_time_ GUARDED_BY_CONTEXT(sequence_checker_) =
      base::Time::Min();
  base::Time last_maintenance_time_ GUARDED_BY_CONTEXT(sequence_checker_) =
      base::Time::Min();
  unsigned int ops_since_last_maintenance_
      GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_STORAGE_H_
