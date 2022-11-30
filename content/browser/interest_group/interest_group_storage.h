// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_STORAGE_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_STORAGE_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
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

namespace content {

// InterestGroupStorage controls access to the Interest Group Database. All
// public functions perform operations on the database and may block. This
// implementation is not thread-safe so all functions should be called from
// within the same sequence.
class CONTENT_EXPORT InterestGroupStorage {
 public:
  static constexpr base::TimeDelta kHistoryLength = base::Days(30);
  static constexpr base::TimeDelta kMaintenanceInterval = base::Hours(1);
  static constexpr base::TimeDelta kIdlePeriod = base::Seconds(30);
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
  // record for this interest group is created.
  void JoinInterestGroup(const blink::InterestGroup& group,
                         const GURL& main_frame_joining_url);
  // Remove the interest group if it exists.
  void LeaveInterestGroup(const blink::InterestGroupKey& group_key,
                          const url::Origin& main_frame);
  // Updates the interest group `name` of `owner` with the populated fields of
  // `update`.
  //
  // If it fails for any reason (e.g., the interest group does not exist, or the
  // data in `update` is not valid), returns false.
  bool UpdateInterestGroup(const blink::InterestGroupKey& group_key,
                           InterestGroupUpdate update);
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
  // Records K-anonymity.
  void UpdateKAnonymity(const StorageInterestGroup::KAnonymityData& data);

  // Gets the last time that the key was reported to the k-anonymity server.
  absl::optional<base::Time> GetLastKAnonymityReported(const std::string& key);
  // Updates the last time that the key was reported to the k-anonymity server.
  void UpdateLastKAnonymityReported(const std::string& key);

  // Gets a single interest group.
  absl::optional<StorageInterestGroup> GetInterestGroup(
      const blink::InterestGroupKey& group_key);
  // Gets a list of all interest group owners. Each owner will only appear
  // once.
  std::vector<url::Origin> GetAllInterestGroupOwners();
  // Gets a list of all interest groups with their bidding information
  // associated with the provided owner.
  std::vector<StorageInterestGroup> GetInterestGroupsForOwner(
      const url::Origin& owner);
  // Like GetInterestGroupsForOwner(), but doesn't return any interest groups
  // that are currently rate-limited for updates. Additionally, this will update
  // the `next_update_after` field such that a subsequent
  // GetInterestGroupsForUpdate() call with the same `owner` won't return
  // anything until after the success rate limit period passes.
  //
  // `groups_limit` sets a limit on the maximum number of interest groups that
  // may be returned.
  std::vector<StorageInterestGroup> GetInterestGroupsForUpdate(
      const url::Origin& owner,
      size_t groups_limit);
  // Gets a list of all interest group joining origins. Each joining origin
  // will only appear once.
  std::vector<url::Origin> GetAllInterestGroupJoiningOrigins();

  std::vector<std::pair<url::Origin, url::Origin>>
  GetAllInterestGroupOwnerJoinerPairs();
  void RemoveInterestGroupsMatchingOwnerAndJoiner(url::Origin owner,
                                                  url::Origin joining_origin);

  // Clear out storage for the matching owning storage key.
  void DeleteInterestGroupData(
      StoragePartition::StorageKeyMatcherFunction storage_key_matcher);
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

  base::Time GetLastMaintenanceTimeForTesting() const;

 private:
  bool EnsureDBInitialized();
  bool InitializeDB();
  bool InitializeSchema();
  void PerformDBMaintenance();
  void DatabaseErrorCallback(int extended_error, sql::Statement* stmt);

  const base::FilePath path_to_database_;
  // Maximum number of interest groups, or interest group owners to keep in the
  // database.
  // Set by the related blink::feature parameters kInterestGroupStorageMaxOwners
  // and kInterestGroupStorageMaxGroupsPerOwner.
  const size_t max_owners_;
  const size_t max_owner_interest_groups_;

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
