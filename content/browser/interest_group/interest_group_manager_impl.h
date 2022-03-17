// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_MANAGER_IMPL_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_MANAGER_IMPL_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "content/browser/interest_group/auction_process_manager.h"
#include "content/browser/interest_group/interest_group_update_manager.h"
#include "content/browser/interest_group/storage_interest_group.h"
#include "content/common/content_export.h"
#include "content/public/browser/interest_group_manager.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom-forward.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "url/origin.h"

namespace base {

class FilePath;

}  // namespace base

namespace content {

class InterestGroupStorage;

// InterestGroupManager is a per-StoragePartition class that owns shared
// state needed to run FLEDGE auctions. It lives on the UI thread.
//
// It acts as a proxy to access an InterestGroupStorage, which lives off-thread
// as it performs blocking file IO when backed by on-disk storage.
class CONTENT_EXPORT InterestGroupManagerImpl : public InterestGroupManager {
 public:
  // Creates an interest group manager using the provided directory path for
  // persistent storage. If `in_memory` is true the path is ignored and only
  // in-memory storage is used.
  explicit InterestGroupManagerImpl(
      const base::FilePath& path,
      bool in_memory,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~InterestGroupManagerImpl() override;
  InterestGroupManagerImpl(const InterestGroupManagerImpl& other) = delete;
  InterestGroupManagerImpl& operator=(const InterestGroupManagerImpl& other) =
      delete;

  class CONTENT_EXPORT InterestGroupObserverInterface
      : public base::CheckedObserver {
   public:
    enum AccessType { kJoin, kLeave, kUpdate, kBid, kWin };
    virtual void OnInterestGroupAccessed(const base::Time& access_time,
                                         AccessType type,
                                         const std::string& owner_origin,
                                         const std::string& name) = 0;
  };

  // InterestGroupManager overrides:
  void GetAllInterestGroupJoiningOrigins(
      base::OnceCallback<void(std::vector<url::Origin>)> callback) override;

  /******** Proxy function calls to InterestGroupsStorage **********/

  // Joins an interest group. If the interest group does not exist, a new one
  // is created based on the provided group information. If the interest group
  // exists, the existing interest group is overwritten. In either case a join
  // record for this interest group is created.
  void JoinInterestGroup(blink::InterestGroup group, const GURL& joining_url);
  // Remove the interest group if it exists.
  void LeaveInterestGroup(const url::Origin& owner, const std::string& name);
  // Loads all interest groups owned by `owner`, then updates their definitions
  // by fetching their `dailyUpdateUrl`. Interest group updates that fail to
  // load or validate are skipped, but other updates will proceed.
  void UpdateInterestGroupsOfOwner(
      const url::Origin& owner,
      network::mojom::ClientSecurityStatePtr client_security_state);
  // Like UpdateInterestGroupsOfOwner(), but handles multiple interest group
  // owners.
  //
  // The list is shuffled in-place to ensure fairness.
  void UpdateInterestGroupsOfOwners(
      base::span<url::Origin> owners,
      network::mojom::ClientSecurityStatePtr client_security_state);
  // For testing *only*; changes the maximum amount of time that the update
  // process can run before it gets cancelled for taking too long.
  void set_max_update_round_duration_for_testing(base::TimeDelta delta);
  // For testing *only*; changes the maximum number of groups that can be
  // updated at the same time.
  void set_max_parallel_updates_for_testing(int max_parallel_updates);
  // Adds an entry to the bidding history for this interest group.
  void RecordInterestGroupBid(const url::Origin& owner,
                              const std::string& name);
  // Adds an entry to the win history for this interest group. `ad_json` is a
  // piece of opaque data to identify the winning ad.
  void RecordInterestGroupWin(const ::url::Origin& owner,
                              const std::string& name,
                              const std::string& ad_json);
  // Gets a single interest group.
  void GetInterestGroup(
      const url::Origin& owner,
      const std::string& name,
      base::OnceCallback<void(absl::optional<StorageInterestGroup>)> callback);
  // Gets a list of all interest group owners. Each owner will only appear
  // once.
  void GetAllInterestGroupOwners(
      base::OnceCallback<void(std::vector<url::Origin>)> callback);
  // Gets a list of all interest groups with their bidding information
  // associated with the provided owner.
  void GetInterestGroupsForOwner(
      const url::Origin& owner,
      base::OnceCallback<void(std::vector<StorageInterestGroup>)> callback);
  // Clear out storage for the matching owning origin. If the callback is empty
  // then apply to all origins.
  void DeleteInterestGroupData(
      base::RepeatingCallback<bool(const url::Origin&)> origin_matcher);
  // Get the last maintenance time from the underlying InterestGroupStorage.
  void GetLastMaintenanceTimeForTesting(
      base::RepeatingCallback<void(base::Time)> callback) const;

  AuctionProcessManager& auction_process_manager() {
    return *auction_process_manager_;
  }

  void AddInterestGroupObserver(InterestGroupObserverInterface* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveInterestGroupObserver(InterestGroupObserverInterface* observer) {
    observers_.RemoveObserver(observer);
  }

  // Allows the AuctionProcessManager to be overridden in unit tests, both to
  // allow not creating a new process, and mocking out the Mojo service
  // interface.
  void set_auction_process_manager_for_testing(
      std::unique_ptr<AuctionProcessManager> auction_process_manager) {
    auction_process_manager_ = std::move(auction_process_manager);
  }

 private:
  // InterestGroupUpdateManager calls private members to write updates to the
  // database.
  friend class InterestGroupUpdateManager;

  // Like GetInterestGroupsForOwner(), but doesn't return any interest groups
  // that are currently rate-limited for updates. Additionally, this will update
  // the `next_update_after` field such that a subsequent
  // GetInterestGroupsForUpdate() call with the same `owner` won't return
  // anything until after the success rate limit period passes.
  //
  // `groups_limit` sets a limit on the maximum number of interest groups that
  // may be returned.
  //
  // To be called only by `update_manager_`.
  void GetInterestGroupsForUpdate(
      const url::Origin& owner,
      int groups_limit,
      base::OnceCallback<void(std::vector<StorageInterestGroup>)> callback);

  // Updates the interest group of the same name based on the information in
  // the provided group. This does not update the interest group expiration
  // time or user bidding signals. Silently fails if the interest group does
  // not exist.
  //
  // To be called only by `update_manager_`.
  void UpdateInterestGroup(blink::InterestGroup group);

  // Modifies the update rate limits stored in the database, with a longer delay
  // for parse failure.
  //
  // To be called only by `update_manager_`.
  void ReportUpdateFailed(const url::Origin& owner,
                          const std::string& name,
                          bool parse_failure);
  void NotifyInterestGroupAccessed(
      InterestGroupObserverInterface::AccessType type,
      const std::string& owner_origin,
      const std::string& name);

  // Owns and manages access to the InterestGroupStorage living on a different
  // thread.
  base::SequenceBound<InterestGroupStorage> impl_;

  // Stored as pointer so that tests can override it.
  std::unique_ptr<AuctionProcessManager> auction_process_manager_;

  base::ObserverList<InterestGroupObserverInterface> observers_;

  // Manages the logic required to support UpdateInterestGroupsOfOwner().
  //
  // InterestGroupUpdateManager keeps a pointer to this InterestGroupManagerImpl
  // to make database writes via calls to GetInterestGroupsForUpdate(),
  // UpdateInterestGroup(), and ReportUpdateFailed().
  //
  // Therefore, `update_manager_` *must* be declared after fields used by those
  // methods so that updates are cancelled before those fields are destroyed.
  InterestGroupUpdateManager update_manager_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_MANAGER_IMPL_H_
