// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_MANAGER_IMPL_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_MANAGER_IMPL_H_

#include <sys/types.h>

#include <cstddef>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "content/browser/interest_group/auction_process_manager.h"
#include "content/browser/interest_group/bidding_and_auction_serializer.h"
#include "content/browser/interest_group/bidding_and_auction_server_key_fetcher.h"
#include "content/browser/interest_group/interest_group_caching_storage.h"
#include "content/browser/interest_group/interest_group_k_anonymity_manager.h"
#include "content/browser/interest_group/interest_group_permissions_checker.h"
#include "content/browser/interest_group/interest_group_update.h"
#include "content/browser/interest_group/interest_group_update_manager.h"
#include "content/browser/interest_group/storage_interest_group.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/interest_group_manager.h"
#include "content/public/browser/k_anonymity_service_delegate.h"
#include "content/public/browser/storage_partition.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/real_time_reporting.mojom.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/ad_auction_service.mojom.h"
#include "url/origin.h"

namespace base {

class FilePath;

}  // namespace base

namespace content {

class AdAuctionPageData;
class InterestGroupStorage;
struct DebugReportLockoutAndCooldowns;

// InterestGroupManager is a per-StoragePartition class that owns shared
// state needed to run FLEDGE auctions. It lives on the UI thread.
//
// It acts as a proxy to access an InterestGroupStorage, which lives off-thread
// as it performs blocking file IO when backed by on-disk storage.
class CONTENT_EXPORT InterestGroupManagerImpl : public InterestGroupManager {
 public:
  using AreReportingOriginsAttestedCallback =
      base::RepeatingCallback<bool(const std::vector<url::Origin>&)>;
  using GetKAnonymityServiceDelegateCallback =
      InterestGroupKAnonymityManager::GetKAnonymityServiceDelegateCallback;
  using RealTimeReportingContributions =
      std::vector<auction_worklet::mojom::RealTimeReportingContributionPtr>;
  using AdAuctionPageDataCallback =
      base::RepeatingCallback<AdAuctionPageData*()>;

  // Controls how auction worklets will be run. kDedicated will use
  // fully-isolated utility processes solely for worklet. kInRenderer will
  // re-use regular renderers following the normal site isolation policy.
  enum class ProcessMode { kDedicated, kInRenderer };

  // Types of even-level reports, for use with EnqueueReports(). Currently only
  // used for histograms. Raw values are not logged directly in histograms, so
  // values do not need to be consistent across versions.
  enum class ReportType {
    kSendReportTo,
    kDebugWin,
    kDebugLoss,
  };

  // Creates an interest group manager using the provided directory path for
  // persistent storage. If `in_memory` is true the path is ignored and only
  // in-memory storage is used.
  explicit InterestGroupManagerImpl(
      const base::FilePath& path,
      bool in_memory,
      ProcessMode process_mode,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      GetKAnonymityServiceDelegateCallback k_anonymity_service_callback);
  ~InterestGroupManagerImpl() override;
  InterestGroupManagerImpl(const InterestGroupManagerImpl& other) = delete;
  InterestGroupManagerImpl& operator=(const InterestGroupManagerImpl& other) =
      delete;

  class CONTENT_EXPORT InterestGroupObserver : public base::CheckedObserver {
   public:
    enum AccessType {
      kJoin,
      kLeave,
      kUpdate,
      kLoaded,
      kBid,
      kAdditionalBid,
      kWin,
      kAdditionalBidWin,
      kClear,
      kTopLevelBid,
      kTopLevelAdditionalBid
    };

    virtual void OnInterestGroupAccessed(
        base::optional_ref<const std::string> devtools_auction_id,
        base::Time access_time,
        AccessType type,
        const url::Origin& owner_origin,
        const std::string& name,
        base::optional_ref<const url::Origin> component_seller_origin,
        std::optional<double> bid,
        base::optional_ref<const std::string> bid_currency) = 0;
  };

  // InterestGroupManager overrides:
  void GetAllInterestGroupJoiningOrigins(
      base::OnceCallback<void(std::vector<url::Origin>)> callback) override;

  void GetAllInterestGroupDataKeys(
      base::OnceCallback<void(std::vector<InterestGroupDataKey>)> callback)
      override;

  void RemoveInterestGroupsByDataKey(InterestGroupDataKey data_key,
                                     base::OnceClosure callback) override;

  /******** Proxy function calls to InterestGroupsStorage **********/

  // Checks if `frame_origin` can join the specified InterestGroup, performing
  // .well-known fetches if needed. If joining is allowed, then joins the
  // interest group.
  //
  // `network_isolation_key` must be the NetworkIsolationKey associated with
  // `url_loader_factory`.
  //
  // `url_loader_factory` is the factory for renderer frame where
  // navigator.joinAdInterestGroup() was invoked, and will be used for the
  // .well-known fetch if one is needed.
  //
  // `report_result_only`, if true, results in calling `callback` with the
  // result of the permissions check, but not actually joining the interest
  // group, regardless of success or failure. This is used to avoid leaking
  // fingerprinting information if the join operation is disallowed by the
  // browser configuration (e.g., 3P cookie blocking).
  //
  // See JoinInterestGroup() for more details on how the join operation is
  // performed.
  void CheckPermissionsAndJoinInterestGroup(
      blink::InterestGroup group,
      const GURL& joining_url,
      const url::Origin& frame_origin,
      const net::NetworkIsolationKey& network_isolation_key,
      bool report_result_only,
      network::mojom::URLLoaderFactory& url_loader_factory,
      AreReportingOriginsAttestedCallback attestation_callback,
      blink::mojom::AdAuctionService::JoinInterestGroupCallback callback);

  // Same as CheckPermissionsAndJoinInterestGroup(), except for a leave
  // operation.
  void CheckPermissionsAndLeaveInterestGroup(
      const blink::InterestGroupKey& group_key,
      const url::Origin& main_frame,
      const url::Origin& frame_origin,
      const net::NetworkIsolationKey& network_isolation_key,
      bool report_result_only,
      network::mojom::URLLoaderFactory& url_loader_factory,
      blink::mojom::AdAuctionService::LeaveInterestGroupCallback callback);

  // Much like CheckPermissionsAndJoinInterestGroup(), except for an operation
  // that leaves all interest groups previously joined from the main frame
  // origin, except those listed in `interest_groups_to_keep`.
  void CheckPermissionsAndClearOriginJoinedInterestGroups(
      const url::Origin& owner,
      const std::vector<std::string>& interest_groups_to_keep,
      const url::Origin& main_frame_origin,
      const url::Origin& frame_origin,
      const net::NetworkIsolationKey& network_isolation_key,
      bool report_result_only,
      network::mojom::URLLoaderFactory& url_loader_factory,
      blink::mojom::AdAuctionService::LeaveInterestGroupCallback callback);

  // Joins an interest group. If the interest group does not exist, a new one
  // is created based on the provided group information. If the interest group
  // exists, the existing interest group is overwritten. In either case a join
  // record for this interest group is created.
  void JoinInterestGroup(blink::InterestGroup group, const GURL& joining_url);
  // Remove the interest group if it exists.
  void LeaveInterestGroup(const blink::InterestGroupKey& group_key,
                          const url::Origin& main_frame);
  // Removes all interest groups owned by `owner` joined from
  // `main_frame_origin` except `interest_groups_to_keep`, if they exist.
  void ClearOriginJoinedInterestGroups(
      const url::Origin& owner,
      std::set<std::string> interest_groups_to_keep,
      url::Origin main_frame_origin);
  // Loads all interest groups owned by `owner`, then updates their
  // definitions by fetching their `dailyUpdateUrl`. Interest group updates
  // that fail to load or validate are skipped, but other updates will
  // proceed.
  void UpdateInterestGroupsOfOwner(
      const url::Origin& owner,
      network::mojom::ClientSecurityStatePtr client_security_state,
      AreReportingOriginsAttestedCallback callback);
  // Like UpdateInterestGroupsOfOwner(), but handles multiple interest group
  // owners.
  //
  // The list is shuffled in-place to ensure fairness.
  void UpdateInterestGroupsOfOwners(
      std::vector<url::Origin> owners,
      network::mojom::ClientSecurityStatePtr client_security_state,
      AreReportingOriginsAttestedCallback callback);

  void UpdateInterestGroupsOfOwnersWithDelay(
      std::vector<url::Origin> owners,
      network::mojom::ClientSecurityStatePtr client_security_state,
      AreReportingOriginsAttestedCallback callback,
      const base::TimeDelta& delay);

  // Allows the interest group specified by `group_key` to be updated if it was
  // last updated before `update_if_older_than`.
  void AllowUpdateIfOlderThan(blink::InterestGroupKey group_key,
                              base::TimeDelta update_if_older_than);

  // For testing *only*; changes the maximum amount of time that the update
  // process can run before it gets cancelled for taking too long.
  void set_max_update_round_duration_for_testing(base::TimeDelta delta) {
    update_manager_.set_max_update_round_duration_for_testing(
        delta);  // IN-TEST
  }

  // For testing *only*; changes the maximum number of groups that can be
  // updated at the same time.
  void set_max_parallel_updates_for_testing(int max_parallel_updates) {
    update_manager_.set_max_parallel_updates_for_testing(  // IN-TEST
        max_parallel_updates);
  }

  // Adds an entry to the bidding history for this interest group.
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

  // Reports the ad keys to the k-anonymity service. Should be called when
  // FLEDGE selects an ad.
  void RegisterAdKeysAsJoined(base::flat_set<std::string> hashed_keys);

  // Gets a single interest group.
  void GetInterestGroup(
      const blink::InterestGroupKey& group_key,
      base::OnceCallback<void(std::optional<SingleStorageInterestGroup>)>
          callback);

  // Gets a single interest group.
  void GetInterestGroup(
      const url::Origin& owner,
      const std::string& name,
      base::OnceCallback<void(std::optional<SingleStorageInterestGroup>)>
          callback);

  // Gets a list of all interest group owners. Each owner will only appear
  // once.
  void GetAllInterestGroupOwners(
      base::OnceCallback<void(std::vector<url::Origin>)> callback);

  // Gets a list of all interest groups with their bidding information
  // associated with the provided owner.
  void GetInterestGroupsForOwner(
      const std::optional<std::string>& devtools_auction_id,
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

  // Clear out storage for the matching owning storage key. If the matcher is
  // empty then apply to all storage keys.
  void DeleteInterestGroupData(
      StoragePartition::StorageKeyMatcherFunction storage_key_matcher,
      base::OnceClosure completion_callback);

  // Completely delete all interest group data, including k-anonymity data that
  // is not cleared by DeleteInterestGroupData.
  void DeleteAllInterestGroupData(base::OnceClosure completion_callback);

  // Get the last maintenance time from the underlying InterestGroupStorage.
  void GetLastMaintenanceTimeForTesting(
      base::RepeatingCallback<void(base::Time)> callback) const;

  // Enqueues reports for the specified URLs. Virtual for testing.
  virtual void EnqueueReports(
      ReportType report_type,
      std::vector<GURL> report_urls,
      FrameTreeNodeId frame_tree_node_id,
      const url::Origin& frame_origin,
      const network::mojom::ClientSecurityState& client_security_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Similar to EnqueueReports(), but enqueues real time reporting
  // contributions. Contributions will be sampled and converted to histograms by
  // calling CalculateRealTimeReportingHistograms() before added to queue.
  // Virtual for testing.
  virtual void EnqueueRealTimeReports(
      std::map<url::Origin, RealTimeReportingContributions> contributions,
      AdAuctionPageDataCallback ad_auction_page_data_callback,
      FrameTreeNodeId frame_tree_node_id,
      const url::Origin& frame_origin,
      const network::mojom::ClientSecurityState& client_security_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Update the interest group priority.
  void SetInterestGroupPriority(const blink::InterestGroupKey& group,
                                double priority);

  // Merges `update_priority_signals_overrides` with the previous priority
  // signals of `group`.
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

  // Clears the InterestGroupPermissionsChecker's cache of the results of
  // .well-known fetches.
  void ClearPermissionsCache();

  AuctionProcessManager& auction_process_manager() {
    return *auction_process_manager_;
  }

  void AddInterestGroupObserver(InterestGroupObserver* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveInterestGroupObserver(InterestGroupObserver* observer) {
    observers_.RemoveObserver(observer);
  }

  // Allows the AuctionProcessManager to be overridden in unit tests, both to
  // allow not creating a new process, and mocking out the Mojo service
  // interface.
  void set_auction_process_manager_for_testing(
      std::unique_ptr<AuctionProcessManager> auction_process_manager) {
    auction_process_manager_ = std::move(auction_process_manager);
  }

  // For testing *only*; changes the maximum number of active report requests
  // at a time.
  void set_max_active_report_requests_for_testing(
      int max_active_report_requests) {
    max_active_report_requests_ = max_active_report_requests;
  }

  // For testing *only*; changes the maximum number of report requests that can
  // be stored in `report_requests_` queue.
  void set_max_report_queue_length_for_testing(int max_queue_length) {
    max_report_queue_length_ = max_queue_length;
  }

  // For testing *only*; changes `max_reporting_round_duration_`.
  void set_max_reporting_round_duration_for_testing(
      base::TimeDelta max_reporting_round_duration) {
    max_reporting_round_duration_ = max_reporting_round_duration;
  }

  // For testing *only*; changes the time interval to wait before sending the
  // next report after sending one.
  void set_reporting_interval_for_testing(base::TimeDelta interval) {
    reporting_interval_ = interval;
  }

  // For testing *only*; changes `real_time_reporting_window_`.
  void set_real_time_reporting_window_for_testing(base::TimeDelta window) {
    real_time_reporting_window_ = window;
  }

  // For testing *only*; changes `max_real_time_reports_`.
  void set_max_real_time_reports_for_testing(int max_num_reports) {
    max_real_time_reports_ = max_num_reports;
  }

  size_t report_queue_length_for_testing() const {
    return report_requests_.size();
  }

  // Handles daily k-anonymity updates for the interest group. Triggers an
  // update request for the k-anonymity of all parts of the interest group
  // (including ads). Also reports membership in the interest group to the
  // k-anonymity of interest-group service.
  void QueueKAnonymityUpdateForInterestGroup(
      const blink::InterestGroupKey& group_key,
      const std::optional<InterestGroupKanonUpdateParameter> update_parameter);
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
                        bool initial_update);

  // Gets lockout and cooldown for sending forDebuggingOnly reports.
  void GetDebugReportLockoutAndCooldowns(
      base::flat_set<url::Origin> origins,
      base::OnceCallback<void(std::optional<DebugReportLockoutAndCooldowns>)>
          callback);

  // Gets the last time that the key was reported to the k-anonymity server.
  void GetLastKAnonymityReported(
      const std::string& hashed_key,
      base::OnceCallback<void(std::optional<base::Time>)> callback);
  // Updates the last time that the key was reported to the k-anonymity server.
  void UpdateLastKAnonymityReported(const std::string& hashed_key);

  void GetInterestGroupAdAuctionData(
      url::Origin top_level_origin,
      base::Uuid generation_id,
      base::Time timestamp,
      blink::mojom::AuctionDataConfigPtr config,
      base::OnceCallback<void(BiddingAndAuctionData)> callback);

  // Get the public key to use for the auction data. The `callback` may be
  // called synchronously if the key is already available or the coordinator is
  // not recognized.
  void GetBiddingAndAuctionServerKey(
      const std::optional<url::Origin>& coordinator,
      base::OnceCallback<void(
          base::expected<BiddingAndAuctionServerKey, std::string>)> callback);

  InterestGroupPermissionsChecker& permissions_checker_for_testing() {
    return permissions_checker_;
  }

  void set_k_anonymity_manager_for_testing(
      std::unique_ptr<InterestGroupKAnonymityManager> k_anonymity_manager) {
    k_anonymity_manager_ = std::move(k_anonymity_manager);
  }

  // Notifies an interest group has been accessed. Used for devtools interest
  // group view and testing.
  //
  // For calls that affect the database, should be invoked after the database
  // update, so that the calls are all in the same order they're applied to the
  // database (can't log them all before the database update, since for, e.g.,
  // calls to retrieve all interest groups for an owner, the name may not be
  // known until after the database has been accessed).
  void NotifyInterestGroupAccessed(
      base::optional_ref<const std::string> devtools_auction_id,
      InterestGroupObserver::AccessType type,
      const url::Origin& owner_origin,
      const std::string& name,
      base::optional_ref<const url::Origin> component_seller_origin,
      std::optional<double> bid,
      base::optional_ref<const std::string> bid_currency);

 private:
  // InterestGroupUpdateManager calls private members to write updates to the
  // database.
  friend class InterestGroupUpdateManager;

  struct ReportRequest {
    ReportRequest();
    ~ReportRequest();

    GURL report_url;
    // Real time reporting histograms to be sent in POST request's body. Null
    // for other report types.
    std::optional<std::vector<uint8_t>> real_time_histogram;

    url::Origin frame_origin;
    network::mojom::ClientSecurityState client_security_state;

    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;

    // Used for Uma histograms. These are build-time constants contained within
    // the binary, so no need for anything to own them.
    const char* name;
    int request_url_size_bytes;
    FrameTreeNodeId frame_tree_node_id;
  };

  struct AdAuctionDataLoaderState {
    AdAuctionDataLoaderState();
    ~AdAuctionDataLoaderState();
    AdAuctionDataLoaderState(AdAuctionDataLoaderState&& state);
    BiddingAndAuctionSerializer serializer;
    base::OnceCallback<void(BiddingAndAuctionData)> callback;
    base::TimeTicks start_time;
  };

  // Callbacks for CheckPermissionsAndJoinInterestGroup(),
  // CheckPermissionsAndLeaveInterestGroup(), and
  // CheckPermissionsAndClearOriginJoinedInterestGroups(), respectively. Perform
  // requested operation if the results of the permissions check allows it.
  void OnJoinInterestGroupPermissionsChecked(
      blink::InterestGroup group,
      const GURL& joining_url,
      bool report_result_only,
      AreReportingOriginsAttestedCallback attestation_callback,
      blink::mojom::AdAuctionService::JoinInterestGroupCallback callback,
      bool can_join);
  void OnLeaveInterestGroupPermissionsChecked(
      const blink::InterestGroupKey& group_key,
      const url::Origin& main_frame,
      bool report_result_only,
      blink::mojom::AdAuctionService::LeaveInterestGroupCallback callback,
      bool can_leave);
  void OnClearOriginJoinedInterestGroupsPermissionsChecked(
      url::Origin owner,
      std::set<std::string> interest_groups_to_keep,
      url::Origin main_frame_origin,
      bool report_result_only,
      blink::mojom::AdAuctionService::LeaveInterestGroupCallback callback,
      bool can_leave);
  void OnClearOriginJoinedInterestGroupsComplete(
      const url::Origin& owner,
      std::vector<std::string> left_interest_group_names);

  // Gets a list of `InterestGroupUpdateParameter` for all interest groups
  // associated with the provided owner.
  //
  // `groups_limit` sets a limit on the maximum number of interest group keys
  // that may be returned.
  //
  // To be called only by `update_manager_`.
  void GetInterestGroupsForUpdate(
      const url::Origin& owner,
      int groups_limit,
      base::OnceCallback<void(std::vector<InterestGroupUpdateParameter>)>
          callback);

  // Updates the interest group of the same name based on the information in
  // the provided group. This does not update the interest group expiration
  // time or user bidding signals.
  //
  // `callback` is invoked asynchronously on completion, with a bool indicating
  // success or failure.
  //
  // To be called only by `update_manager_`.
  void UpdateInterestGroup(const blink::InterestGroupKey& group_key,
                           InterestGroupUpdate update,
                           base::OnceCallback<void(bool)> callback);

  // Called when a call to UpdateInterestGroup() completes. Sends a notification
  // about the update and invokes `callback` with `success`. Queues a
  // k-anonymity update with `kanon_update_parameter`.
  void OnUpdateComplete(
      const blink::InterestGroupKey& group_key,
      base::OnceCallback<void(bool)> callback,
      std::optional<InterestGroupKanonUpdateParameter> kanon_update_parameter);

  // Modifies the update rate limits stored in the database, with a longer delay
  // for parse failure.
  //
  // To be called only by `update_manager_`.
  void ReportUpdateFailed(const blink::InterestGroupKey& group_key,
                          bool parse_failure);

  void OnGetInterestGroupsComplete(
      base::OnceCallback<void(scoped_refptr<StorageInterestGroups>)> callback,
      const std::optional<std::string>& devtools_auction_id,
      scoped_refptr<StorageInterestGroups> groups);

  // Dequeues and sends the first report request in `report_requests_` queue,
  // if the queue is not empty.
  void TrySendingOneReport();

  // Invoked when a report request completed.
  void OnOneReportSent(
      std::unique_ptr<network::SimpleURLLoader> simple_url_loader,
      FrameTreeNodeId frame_tree_node_id,
      const std::string& devtools_request_id,
      scoped_refptr<net::HttpResponseHeaders> response_headers);

  // Clears `report_requests_`.  Does not abort currently pending requests.
  void TimeoutReports();

  // Shuffles the owners then calls `LoadNextInterestGroupAdAuctionData()`.
  // We need the shuffle so that interest group owners are not included in the
  // auction data in the same order every time. Our serialization only solves
  // an approximation of the "knapsack" problem, so the amount of interest
  // groups we can fit for each owner may depend on the order in which
  // they are processed. Shuffling helps guarantee fairness.
  void ShuffleOwnersThenLoadInterestGroupAdAuctionData(
      AdAuctionDataLoaderState state,
      std::vector<url::Origin> owners);

  // Loads the next owner's interest group data. If there are no more owners
  // whose interest groups need to be loaded, calls OnAdAuctionDataLoadComplete.
  void LoadNextInterestGroupAdAuctionData(AdAuctionDataLoaderState state,
                                          std::vector<url::Origin> owners);

  // Serializes the loaded auction data and then calls
  // LoadNextInterestGroupAdAuctionData to continue loading.
  void OnLoadedNextInterestGroupAdAuctionData(
      AdAuctionDataLoaderState state,
      std::vector<url::Origin> owners,
      url::Origin owner,
      scoped_refptr<StorageInterestGroups> groups);

  // Invoked when loading interest groups is completed. Load debug report
  // lockout information if sampling debug report is enabled, otherwise invoke
  // `OnAdAuctionDataLoadComplete()` directly.
  void OnInterestGroupAdAuctionDataLoadComplete(AdAuctionDataLoaderState state);

  // Constructs the AuctionAdata when the load is complete and calls the
  // provided callback.
  void OnAdAuctionDataLoadComplete(
      AdAuctionDataLoaderState state,
      std::optional<base::Time> last_report_sent_time);

  // Helper to that returns bound NotifyInterestGroupAccessed() callbacks to
  // allow notifications to be sent after a database update.
  base::OnceClosure CreateNotifyInterestGroupAccessedCallback(
      InterestGroupObserver::AccessType type,
      const url::Origin& owner_origin,
      const std::string& name);

  // Controls access to the Interest Group Database through its owned
  // InterestGroupStorage. Returns cached values for GetInterestGroupsForOwner
  // when available.
  InterestGroupCachingStorage caching_storage_;

  // Stored as pointer so that tests can override it.
  std::unique_ptr<AuctionProcessManager> auction_process_manager_;

  base::ObserverList<InterestGroupObserver> observers_;

  // Manages the logic required to support UpdateInterestGroupsOfOwner().
  //
  // InterestGroupUpdateManager keeps a pointer to this InterestGroupManagerImpl
  // to make database writes via calls to GetInterestGroupsForUpdate(),
  // UpdateInterestGroup(), and ReportUpdateFailed().
  //
  // Therefore, `update_manager_` *must* be declared after fields used by those
  // methods so that updates are cancelled before those fields are destroyed.
  InterestGroupUpdateManager update_manager_;

  // Manages the logic required to support k-anonymity updates.
  //
  // InterestGroupKAnonymityManager keeps a pointer to this
  // InterestGroupManagerImpl to make database reads and writes....
  //
  // Therefore, `k_anonymity_manager_` *must* be declared after fields used by
  // those methods so that k-anonymity operations are cancelled before those
  // fields are destroyed.
  // Stored as pointer so that tests can override it.
  std::unique_ptr<InterestGroupKAnonymityManager> k_anonymity_manager_;

  // Checks if a frame can join or leave an interest group. Global so that
  // pending operations can continue after a page has been navigate away from.
  InterestGroupPermissionsChecker permissions_checker_;

  // The queue of report requests. Empty the queue if it's size is larger than
  // `max_report_queue_length` at the time of adding new entries.
  base::circular_deque<std::unique_ptr<ReportRequest>> report_requests_;

  // Current number of active report requests. Includes requests that completed
  // within the last `kReportingInterval`, each of which should have a pending
  // delayed task to invoke TrySendingOneReport().
  int num_active_ = 0;

  // The maximum number of active report requests at a time.
  //
  // Should *only* be changed by tests.
  int max_active_report_requests_;

  // The maximum number of report requests that can be stored in queue
  // `report_requests_`.
  //
  // Should *only* be changed by tests.
  int max_report_queue_length_;

  // The time interval to wait before sending the next batch of report requests
  // after sending one batch.
  //
  // Should *only* be changed by tests.
  base::TimeDelta reporting_interval_;

  // The maximum amount of time that the reporting process can run before the
  // report queue is cleared due to taking too long.
  //
  // Should *only* be changed by tests.
  base::TimeDelta max_reporting_round_duration_;

  // The number of real time reports (`max_real_time_reports_`) per reporting
  // origin per page per `real_time_reporting_window_`.
  //
  // Should *only* be changed by tests.
  base::TimeDelta real_time_reporting_window_;
  double max_real_time_reports_;

  // Used to clear all pending reports in the queue if reporting takes too long.
  // Started when sending reports starts. Stopped once all reports are sent.
  // When the timer triggers, all reports are aborted.
  //
  // The resulting behavior is that if reports are continuously being sent for
  // too long, possibly from multiple auctions, all reports are timed out.
  base::OneShotTimer timeout_timer_;

  // Used to fetch the key for encrypting the request to the bidding and auction
  // server.
  BiddingAndAuctionServerKeyFetcher ba_key_fetcher_;

  base::WeakPtrFactory<InterestGroupManagerImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_MANAGER_IMPL_H_
