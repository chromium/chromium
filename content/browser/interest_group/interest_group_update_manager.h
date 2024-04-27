// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_UPDATE_MANAGER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_UPDATE_MANAGER_H_

#include <stddef.h>

#include <list>
#include <map>
#include <memory>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_update.h"
#include "content/browser/interest_group/storage_interest_group.h"
#include "content/common/content_export.h"
#include "net/base/isolation_info.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "url/origin.h"

namespace network {

class SimpleURLLoader;

}  // namespace network

namespace content {

class InterestGroupManagerImpl;

// Implements the interest group update functionality of
// InterestGroupManagerImpl.
//
// Updates the interest groups of each owner separately by queueing the owner
// origins whose interest group should be updated.
//
// Updates are rate-limited; interest groups don't get updated if they've been
// updated recently.
class CONTENT_EXPORT InterestGroupUpdateManager {
 public:
  using AreReportingOriginsAttestedCallback =
      base::RepeatingCallback<bool(const std::vector<url::Origin>&)>;

  // `manager` should be the InterestGroupManagerImpl that owns this
  // InterestGroupManager.
  InterestGroupUpdateManager(
      InterestGroupManagerImpl* manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~InterestGroupUpdateManager();

  // Loads all interest groups owned by `owner`, then updates their definitions
  // by fetching their `dailyUpdateUrl`. Interest group updates that fail to
  // load or validate are skipped, but other updates will proceed.
  void UpdateInterestGroupsOfOwner(
      const url::Origin& owner,
      network::mojom::ClientSecurityStatePtr client_security_state,
      AreReportingOriginsAttestedCallback callback);

  // Like UpdateInterestGroupsOfOwner(), but handles multiple interest group
  // owners.
  //
  // The list is shuffled in-place to ensure fairness.
  void UpdateInterestGroupsOfOwners(
      base::span<url::Origin> owners,
      network::mojom::ClientSecurityStatePtr client_security_state,
      AreReportingOriginsAttestedCallback callback);

  // For testing *only*; changes the maximum amount of time that the update
  // process can run before it gets cancelled for taking too long.
  void set_max_update_round_duration_for_testing(base::TimeDelta delta);

  // For testing *only*; changes the maximum number of groups that can be
  // updated at the same time.
  void set_max_parallel_updates_for_testing(int max_parallel_updates);

 private:
  using UrlLoadersList = std::list<std::unique_ptr<network::SimpleURLLoader>>;

  enum class UpdateDelayType {
    // If the update fails due to a disconnected network, the retry can happen
    // immediately.
    kNoInternet,

    // If the update failed due to another network error, it can be retried
    // after a small delay (~1 hour).
    kNetFailure,

    // If the update failed due to an issue with the response (i.e. bad JSON),
    // the interest group can only be updated again after a large delay (~1
    // day).
    kParseFailure
  };

  // A queue of interest group owners that require updating, along with the
  // ClientSecurityState that was used to request those updates and an isolation
  // info map that stores different NIKs for different joining origins.
  class OwnersToUpdate {
   public:
    OwnersToUpdate();
    ~OwnersToUpdate();

    // Returns true iff there are no more interest group owners to process.
    bool Empty() const;

    // Returns the owner origin that is currently being updated. Requires
    // !Empty().
    const url::Origin& FrontOwner() const;

    // Returns the ClientSecurityState that was used to request the current
    // owner update. Requires !Empty().
    network::mojom::ClientSecurityStatePtr FrontSecurityState() const;

    // If `owner` isn't already in the queue, enqueues `owner` for processing,
    // using `client_security_state` when fetching `owner`'s interest group
    // updates from the network, and returns true. Otherwise, returns false.
    //
    // Callers *must* call MaybeContinueUpdatingCurrentOwner() after Enqueue()
    // to ensure the that `owner` gets processed.
    bool Enqueue(const url::Origin& owner,
                 network::mojom::ClientSecurityStatePtr client_security_state);

    // Removes the current `owner` and its associated ClientSecurityState from
    // the front of the queue. Requires !Empty().
    void PopFront();

    // Get isolation info by joining origin.
    // Note that the returned pointer is not guaranteed to be valid after the
    // next time call of GetIsolationInfoByJoiningOrigin(), due to the map could
    // be re-allocated to a new address.
    net::IsolationInfo* GetIsolationInfoByJoiningOrigin(const url::Origin&);

    // Clear `joining_origin_isolation_info_map_`.
    void ClearJoiningOriginIsolationInfoMap();

    // Removes all queued interest group owners.
    void Clear();

   private:
    // The queue of owners whose interest groups need updating.
    base::circular_deque<url::Origin> owners_to_update_;

    // For each interest group owner in `owners_to_update_`, we keep the
    // ClientSecurityState that was used to make the update request.
    base::flat_map<url::Origin, network::mojom::ClientSecurityStatePtr>
        security_state_map_;

    // IsolationInfo map, keyed by `StorageInterestGroup::joining_origin`.
    // This is used to improve privacy that only the interest groups from same
    // joining origin can reuse a network isolation key.
    //
    // During the update process for all owners, it will create isolation info
    // for every joining origin and store it in this map. After the update
    // process is done, the whole map will be erased.
    std::map<url::Origin, net::IsolationInfo>
        joining_origin_isolation_info_map_;
  };

  // Processes the next set of interest groups to update.
  //
  // If existing update work is already in-progress (that is, if
  // `num_in_flight_updates_` isn't 0, or `waiting_on_db_read_` is true), quits
  // immediately to avoid duplicating update work.
  void MaybeContinueUpdatingCurrentOwner();

  // For a given owner, gets interest group keys along with their update urls.
  void GetInterestGroupsForUpdate(
      const url::Origin& owner,
      base::OnceCallback<void(std::vector<InterestGroupUpdateParameter>)>
          callback);

  // Update interest groups by batch in parallel.
  void UpdateInterestGroupByBatch(
      const url::Origin& owner,
      std::vector<InterestGroupUpdateParameter> update_parameters);

  void DidUpdateInterestGroupsOfOwnerDbLoad(
      url::Origin owner,
      std::vector<InterestGroupUpdateParameter> update_parameters);
  void DidUpdateInterestGroupsOfOwnerNetFetch(
      UrlLoadersList::iterator simple_url_loader,
      blink::InterestGroupKey group_key,
      base::TimeTicks start_time,
      std::unique_ptr<std::string> fetch_body);
  void DidUpdateInterestGroupsOfOwnerJsonParse(
      blink::InterestGroupKey group_key,
      data_decoder::DataDecoder::ValueOrError result);

  // Updates the specified interest group with the information in `update`.
  // On completion, invoked OnUpdateInterestGroupCompleted() asynchronously,
  // with a bool indicated whether or not the update succeeded.
  void UpdateInterestGroup(const blink::InterestGroupKey& group_key,
                           InterestGroupUpdate update);

  // This method finishes the current update and invokes either
  // ReportUpdateFailed() or OnOneUpdateCompleted().
  void OnUpdateInterestGroupCompleted(const blink::InterestGroupKey& group_key,
                                      bool success);

  // Called after a single interest group update finishes. Should be called
  // after any database operations (if performed) or clearing of
  // `owners_to_update_`.
  void OnOneUpdateCompleted();

  // Processes update failure, and if `delay_type` isn't kNoInternet, modifies
  // the update rate limits stored in the database.
  //
  // This method finishes the current update and calls OnOneUpdateCompleted().
  void ReportUpdateFailed(const blink::InterestGroupKey& group_key,
                          UpdateDelayType delay_type);

  // An unowned pointer to the InterestGroupManagerImpl that owns this
  // InterestGroupUpdateManager. Used as an intermediary to talk to the
  // database.
  raw_ptr<InterestGroupManagerImpl> manager_;

  // A queue of owners to update, with ClientSecurityState that was used to
  // request those update and isolation info map that is used to store different
  // NIKs for different joining origins.
  OwnersToUpdate owners_to_update_;

  // The number of interest group updates for the current interest group owner
  // (that is, owners_to_update_.front()) that are still in the process of
  // updating.
  //
  // To avoid updating the same interest group twice in succession, this should
  // only be decremented after a write to the DB, except for network
  // disconnection, where immediate retry is allowed.
  //
  // NOTE: This will be 0 while reading from the database, as at that point, we
  // don't yet know how many interest groups need to be updated for the current
  // owner. However, `waiting_on_db_read_` will be true in that situation.
  int num_in_flight_updates_ = 0;

  // Will be true while we are waiting for the database to load the set of
  // interest groups of the current owner that require updating, and false
  // otherwise. At most one interest group read may happen at a given time.
  //
  // By checking `waiting_on_db_read_` and `num_in_flight_updates_`,
  // MaybeContinueUpdatingCurrentOwner() can check if update work is already
  // ongoing, to avoid erroroneously having multiple update jobs occurring at
  // the same time. Although the core update logic in InterestGroupUpdateManager
  // occurs on the UI thread, tasks like reading from the database, network
  // fetches, and JSON parsing happen asynchronously, so errors caused by
  // interleaving are possible without such guarding.
  //
  // Note that this is not affected by whether or not we are writing to the
  // database (including any reads as part of that write), since these are
  // covered by `num_in_flight_updates_`. Although the actual write to disk
  // occurs asynchronsously, and we don't wait for completion, DB operations
  // occur in-order, so the next read should reflect the results of the previous
  // write.
  bool waiting_on_db_read_ = false;

  // The maximum amount of time that the update process can run before it gets
  // cancelled for taking too long.
  //
  // Should *only* be changed by tests.
  base::TimeDelta max_update_round_duration_;

  // The maximum number of groups that can be updated at the same time.
  //
  // Should *only* be changed by tests.
  int max_parallel_updates_;

  // The last time we started a round of updating; used to cancel long-running
  // updates.
  base::TimeTicks last_update_started_ = base::TimeTicks::Min();

  // Counter used for count how many interest groups are updated in an update
  // round. This value is only meaningful during an update round.
  int num_groups_updated_in_current_round_ = 0;

  // Used for fetching interest group update JSON over the network.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // All active network requests -- active requests will be cancelled when
  // destroyed.
  UrlLoadersList url_loaders_;

  // For checking if all allowed reporting origins are attested.
  AreReportingOriginsAttestedCallback attestation_callback_;

  // TODO(crbug.com/40172488): Do we need to test InterestGroupManager
  // destruction during update? If so, how?
  base::WeakPtrFactory<InterestGroupUpdateManager> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_UPDATE_MANAGER_H_
