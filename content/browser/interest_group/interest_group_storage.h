// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_STORAGE_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_STORAGE_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// InterestGroupStorage controls access to the Interest Group Database. All
// public functions perform operations on the database and may block. This
// implementation is not thread-safe so all functions should be called from
// within the same sequence.
class CONTENT_EXPORT InterestGroupStorage {
 public:
  static constexpr base::TimeDelta kHistoryLength =
      base::TimeDelta::FromDays(30);
  static constexpr base::TimeDelta kMaintenanceInterval =
      base::TimeDelta::FromHours(1);

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
  void JoinInterestGroup(blink::mojom::InterestGroupPtr group);
  // Remove the interest group if it exists.
  void LeaveInterestGroup(const url::Origin& owner, const std::string& name);
  // Updates the interest group of the same name based on the information in
  // the provided group. This does not update the interest group expiration
  // time or user bidding signals. Silently fails if the interest group does
  // not exist.
  void UpdateInterestGroup(blink::mojom::InterestGroupPtr group);
  // Adds an entry to the bidding history for this interest group.
  void RecordInterestGroupBid(const url::Origin& owner,
                              const std::string& name);
  // Adds an entry to the win history for this interest group. `ad_json` is a
  // piece of opaque data to identify the winning ad.
  void RecordInterestGroupWin(const url::Origin& owner,
                              const std::string& name,
                              const std::string& ad_json);
  // Gets a list of all interest group owners. Each owner will only appear
  // once.
  std::vector<::url::Origin> GetAllInterestGroupOwners();
  // Gets a list of all interest groups with their bidding information
  // associated with the provided owner.
  std::vector<auction_worklet::mojom::BiddingInterestGroupPtr>
  GetInterestGroupsForOwner(const url::Origin& owner);

  // Clear out storage for the matching owning origin. If the callback is empty
  // then apply to all origins.
  void DeleteInterestGroupData(
      const base::RepeatingCallback<bool(const url::Origin&)>& origin_matcher);

  std::vector<auction_worklet::mojom::BiddingInterestGroupPtr>
  GetAllInterestGroupsUnfilteredForTesting();

 private:
  bool EnsureDBInitialized();
  bool InitializeDB();
  bool InitializeSchema();
  void PerformDBMaintenance();
  void DatabaseErrorCallback(int extended_error, sql::Statement* stmt);

  base::OneShotTimer db_maintenance_timer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<sql::Database> db_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::Time last_access_time_ GUARDED_BY_CONTEXT(sequence_checker_);
  const base::FilePath path_to_database_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_STORAGE_H_
