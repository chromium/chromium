// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_manager_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/values_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "components/cbor/diagnostic_writer.h"
#include "components/cbor/writer.h"
#include "content/browser/interest_group/interest_group_storage.h"
#include "content/browser/interest_group/interest_group_update.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"

#include "url/gurl.h"

namespace content {

namespace {
// The maximum number of active report requests at a time.
constexpr int kMaxActiveReportRequests = 5;
// The maximum number of report URLs that can be stored in `report_requests_`
// queue.
constexpr int kMaxReportQueueLength = 1000;
// The maximum amount of time allowed to fetch report requests in the queue.
constexpr base::TimeDelta kMaxReportingRoundDuration = base::Minutes(10);
// The time interval to wait before sending the next report after sending one.
constexpr base::TimeDelta kReportingInterval = base::Milliseconds(50);

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("auction_report_sender", R"(
        semantics {
          sender: "Interest group based Ad Auction report"
          description:
            "Facilitates reporting the result of an in-browser interest group "
            "based ad auction to an auction participant. "
            "See https://github.com/WICG/turtledove/blob/main/FLEDGE.md"
          trigger:
            "Requested after running a in-browser interest group based ad "
            "auction to report the auction result back to auction participants."
          data: "URL associated with an interest group or seller."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
            "These requests are controlled by a feature flag that is off by "
            "default now. When enabled, they can be disabled by the Privacy"
            " Sandbox setting."
          policy_exception_justification:
            "These requests are triggered by a website."
        })");

// Makes an uncredentialed request and creates a SimpleURLLoader for it. Returns
// the SimpleURLLoader which will be used to report the result of an in-browser
// interest group based ad auction to an auction participant.
std::unique_ptr<network::SimpleURLLoader> BuildSimpleUrlLoader(
    GURL url,
    const url::Origin& frame_origin,
    const network::mojom::ClientSecurityState& client_security_state) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = std::move(url);
  resource_request->redirect_mode = network::mojom::RedirectMode::kError;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->request_initiator = frame_origin;
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->isolation_info =
      net::IsolationInfo::CreateTransient();
  resource_request->trusted_params->client_security_state =
      client_security_state.Clone();
  auto simple_url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), kTrafficAnnotation);
  simple_url_loader->SetTimeoutDuration(base::Seconds(30));
  return simple_url_loader;
}

std::vector<InterestGroupManager::InterestGroupDataKey>
ConvertOwnerJoinerPairsToDataKeys(
    std::vector<std::pair<url::Origin, url::Origin>> owner_joiner_pairs) {
  std::vector<InterestGroupManager::InterestGroupDataKey> data_keys;
  for (auto& key : owner_joiner_pairs) {
    data_keys.emplace_back(
        InterestGroupManager::InterestGroupDataKey{key.first, key.second});
  }
  return data_keys;
}
}  // namespace

InterestGroupManagerImpl::ReportRequest::ReportRequest() = default;
InterestGroupManagerImpl::ReportRequest::~ReportRequest() = default;

InterestGroupManagerImpl::AdAuctionDataLoaderState::AdAuctionDataLoaderState() =
    default;
InterestGroupManagerImpl::AdAuctionDataLoaderState::AdAuctionDataLoaderState(
    AdAuctionDataLoaderState&& state) = default;
InterestGroupManagerImpl::AdAuctionDataLoaderState::
    ~AdAuctionDataLoaderState() = default;

InterestGroupManagerImpl::InterestGroupManagerImpl(
    const base::FilePath& path,
    bool in_memory,
    ProcessMode process_mode,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    KAnonymityServiceDelegate* k_anonymity_service)
    : impl_(base::ThreadPool::CreateSequencedTaskRunner(
                {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                 base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
            in_memory ? base::FilePath() : path),
      auction_process_manager_(
          base::WrapUnique(process_mode == ProcessMode::kDedicated
                               ? static_cast<AuctionProcessManager*>(
                                     new DedicatedAuctionProcessManager())
                               : new InRendererAuctionProcessManager())),
      update_manager_(this, std::move(url_loader_factory)),
      k_anonymity_manager_(std::make_unique<InterestGroupKAnonymityManager>(
          this,
          k_anonymity_service)),
      max_active_report_requests_(kMaxActiveReportRequests),
      max_report_queue_length_(kMaxReportQueueLength),
      reporting_interval_(kReportingInterval),
      max_reporting_round_duration_(kMaxReportingRoundDuration) {}

InterestGroupManagerImpl::~InterestGroupManagerImpl() = default;

void InterestGroupManagerImpl::GetAllInterestGroupJoiningOrigins(
    base::OnceCallback<void(std::vector<url::Origin>)> callback) {
  impl_.AsyncCall(&InterestGroupStorage::GetAllInterestGroupJoiningOrigins)
      .Then(std::move(callback));
}

void InterestGroupManagerImpl::GetAllInterestGroupDataKeys(
    base::OnceCallback<void(std::vector<InterestGroupDataKey>)> callback) {
  impl_.AsyncCall(&InterestGroupStorage::GetAllInterestGroupOwnerJoinerPairs)
      .Then(base::BindOnce(&ConvertOwnerJoinerPairsToDataKeys)
                .Then(std::move(callback)));
}

void InterestGroupManagerImpl::RemoveInterestGroupsByDataKey(
    InterestGroupDataKey data_key,
    base::OnceClosure callback) {
  impl_
      .AsyncCall(
          &InterestGroupStorage::RemoveInterestGroupsMatchingOwnerAndJoiner)
      .WithArgs(data_key.owner, data_key.joining_origin)
      .Then(std::move(callback));
}

void InterestGroupManagerImpl::CheckPermissionsAndJoinInterestGroup(
    blink::InterestGroup group,
    const GURL& joining_url,
    const url::Origin& frame_origin,
    const net::NetworkIsolationKey& network_isolation_key,
    bool report_result_only,
    network::mojom::URLLoaderFactory& url_loader_factory,
    blink::mojom::AdAuctionService::JoinInterestGroupCallback callback) {
  url::Origin interest_group_owner = group.owner;
  permissions_checker_.CheckPermissions(
      InterestGroupPermissionsChecker::Operation::kJoin, frame_origin,
      interest_group_owner, network_isolation_key, url_loader_factory,
      base::BindOnce(
          &InterestGroupManagerImpl::OnJoinInterestGroupPermissionsChecked,
          base::Unretained(this), std::move(group), joining_url,
          report_result_only, std::move(callback)));
}

void InterestGroupManagerImpl::CheckPermissionsAndLeaveInterestGroup(
    const blink::InterestGroupKey& group_key,
    const url::Origin& main_frame,
    const url::Origin& frame_origin,
    const net::NetworkIsolationKey& network_isolation_key,
    bool report_result_only,
    network::mojom::URLLoaderFactory& url_loader_factory,
    blink::mojom::AdAuctionService::LeaveInterestGroupCallback callback) {
  permissions_checker_.CheckPermissions(
      InterestGroupPermissionsChecker::Operation::kLeave, frame_origin,
      group_key.owner, network_isolation_key, url_loader_factory,
      base::BindOnce(
          &InterestGroupManagerImpl::OnLeaveInterestGroupPermissionsChecked,
          base::Unretained(this), group_key, main_frame, report_result_only,
          std::move(callback)));
}

void InterestGroupManagerImpl::JoinInterestGroup(blink::InterestGroup group,
                                                 const GURL& joining_url) {
  NotifyInterestGroupAccessed(InterestGroupObserver::kJoin, group.owner,
                              group.name);
  blink::InterestGroupKey group_key(group.owner, group.name);
  impl_.AsyncCall(&InterestGroupStorage::JoinInterestGroup)
      .WithArgs(std::move(group), std::move(joining_url));
  // This needs to happen second so that the DB row is created.
  GetInterestGroup(
      group_key,
      base::BindOnce(
          &InterestGroupManagerImpl::
              QueueKAnonymityUpdateForInterestGroupFromJoinInterestGroup,
          weak_factory_.GetWeakPtr()));
}

void InterestGroupManagerImpl::LeaveInterestGroup(
    const blink::InterestGroupKey& group_key,
    const ::url::Origin& main_frame) {
  NotifyInterestGroupAccessed(InterestGroupObserver::kLeave, group_key.owner,
                              group_key.name);
  impl_.AsyncCall(&InterestGroupStorage::LeaveInterestGroup)
      .WithArgs(group_key, main_frame);
}

void InterestGroupManagerImpl::UpdateInterestGroupsOfOwner(
    const url::Origin& owner,
    network::mojom::ClientSecurityStatePtr client_security_state) {
  update_manager_.UpdateInterestGroupsOfOwner(owner,
                                              std::move(client_security_state));
}

void InterestGroupManagerImpl::UpdateInterestGroupsOfOwners(
    base::span<url::Origin> owners,
    network::mojom::ClientSecurityStatePtr client_security_state) {
  update_manager_.UpdateInterestGroupsOfOwners(
      owners, std::move(client_security_state));
}

void InterestGroupManagerImpl::RecordInterestGroupBids(
    const blink::InterestGroupSet& group_keys) {
  if (group_keys.empty()) {
    return;
  }
  for (const auto& group_key : group_keys) {
    NotifyInterestGroupAccessed(InterestGroupObserver::kBid, group_key.owner,
                                group_key.name);
  }
  impl_.AsyncCall(&InterestGroupStorage::RecordInterestGroupBids)
      .WithArgs(group_keys);
}

void InterestGroupManagerImpl::RecordInterestGroupWin(
    const blink::InterestGroupKey& group_key,
    const std::string& ad_json) {
  NotifyInterestGroupAccessed(InterestGroupObserver::kWin, group_key.owner,
                              group_key.name);
  impl_.AsyncCall(&InterestGroupStorage::RecordInterestGroupWin)
      .WithArgs(group_key, std::move(ad_json));
}

void InterestGroupManagerImpl::RegisterAdKeysAsJoined(
    base::flat_set<std::string> keys) {
  k_anonymity_manager_->RegisterAdKeysAsJoined(std::move(keys));
}

void InterestGroupManagerImpl::GetInterestGroup(
    const url::Origin& owner,
    const std::string& name,
    base::OnceCallback<void(absl::optional<StorageInterestGroup>)> callback) {
  GetInterestGroup(blink::InterestGroupKey(owner, name), std::move(callback));
}
void InterestGroupManagerImpl::GetInterestGroup(
    const blink::InterestGroupKey& group_key,
    base::OnceCallback<void(absl::optional<StorageInterestGroup>)> callback) {
  impl_.AsyncCall(&InterestGroupStorage::GetInterestGroup)
      .WithArgs(group_key)
      .Then(std::move(callback));
}

void InterestGroupManagerImpl::GetAllInterestGroupOwners(
    base::OnceCallback<void(std::vector<url::Origin>)> callback) {
  impl_.AsyncCall(&InterestGroupStorage::GetAllInterestGroupOwners)
      .Then(std::move(callback));
}

void InterestGroupManagerImpl::GetInterestGroupsForOwner(
    const url::Origin& owner,
    base::OnceCallback<void(std::vector<StorageInterestGroup>)> callback) {
  impl_.AsyncCall(&InterestGroupStorage::GetInterestGroupsForOwner)
      .WithArgs(owner)
      .Then(
          base::BindOnce(&InterestGroupManagerImpl::OnGetInterestGroupsComplete,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
}

void InterestGroupManagerImpl::DeleteInterestGroupData(
    StoragePartition::StorageKeyMatcherFunction storage_key_matcher,
    base::OnceClosure completion_callback) {
  impl_.AsyncCall(&InterestGroupStorage::DeleteInterestGroupData)
      .WithArgs(std::move(storage_key_matcher))
      .Then(std::move(completion_callback));
}

void InterestGroupManagerImpl::DeleteAllInterestGroupData(
    base::OnceClosure completion_callback) {
  impl_.AsyncCall(&InterestGroupStorage::DeleteAllInterestGroupData)
      .Then(std::move(completion_callback));
}

void InterestGroupManagerImpl::GetLastMaintenanceTimeForTesting(
    base::RepeatingCallback<void(base::Time)> callback) const {
  impl_.AsyncCall(&InterestGroupStorage::GetLastMaintenanceTimeForTesting)
      .Then(std::move(callback));
}

void InterestGroupManagerImpl::EnqueueReports(
    ReportType report_type,
    std::vector<GURL> report_urls,
    const url::Origin& frame_origin,
    const network::mojom::ClientSecurityState& client_security_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  if (report_urls.empty()) {
    return;
  }

  // For memory usage reasons, purge the queue if it has at least
  // `max_report_queue_length_` entries at the time we're about to add new
  // entries.
  if (report_requests_.size() >=
      static_cast<unsigned int>(max_report_queue_length_)) {
    report_requests_.clear();
  }

  const char* report_type_name;
  switch (report_type) {
    case ReportType::kSendReportTo:
      report_type_name = "SendReportToReport";
      break;
    case ReportType::kDebugWin:
      report_type_name = "DebugWinReport";
      break;
    case ReportType::kDebugLoss:
      report_type_name = "DebugLossReport";
      break;
  }

  for (GURL& report_url : report_urls) {
    auto report_request = std::make_unique<ReportRequest>();
    report_request->request_url_size_bytes = report_url.spec().size();
    report_request->simple_url_loader = BuildSimpleUrlLoader(
        std::move(report_url), frame_origin, client_security_state);
    report_request->name = report_type_name;
    report_request->url_loader_factory = url_loader_factory;
    report_requests_.emplace_back(std::move(report_request));
  }

  while (!report_requests_.empty() &&
         num_active_ < max_active_report_requests_) {
    ++num_active_;
    TrySendingOneReport();
  }
}

void InterestGroupManagerImpl::SetInterestGroupPriority(
    const blink::InterestGroupKey& group_key,
    double priority) {
  impl_.AsyncCall(&InterestGroupStorage::SetInterestGroupPriority)
      .WithArgs(group_key, priority);
}

void InterestGroupManagerImpl::UpdateInterestGroupPriorityOverrides(
    const blink::InterestGroupKey& group_key,
    base::flat_map<std::string,
                   auction_worklet::mojom::PrioritySignalsDoublePtr>
        update_priority_signals_overrides) {
  impl_.AsyncCall(&InterestGroupStorage::UpdateInterestGroupPriorityOverrides)
      .WithArgs(group_key, std::move(update_priority_signals_overrides));
}

void InterestGroupManagerImpl::ClearPermissionsCache() {
  permissions_checker_.ClearCache();
}

void InterestGroupManagerImpl::QueueKAnonymityUpdateForInterestGroup(
    const StorageInterestGroup& group) {
  k_anonymity_manager_->QueryKAnonymityForInterestGroup(group);
}

void InterestGroupManagerImpl::UpdateKAnonymity(
    const StorageInterestGroup::KAnonymityData& data) {
  impl_.AsyncCall(&InterestGroupStorage::UpdateKAnonymity).WithArgs(data);
}

void InterestGroupManagerImpl::GetLastKAnonymityReported(
    const std::string& key,
    base::OnceCallback<void(absl::optional<base::Time>)> callback) {
  impl_.AsyncCall(&InterestGroupStorage::GetLastKAnonymityReported)
      .WithArgs(key)
      .Then(std::move(callback));
}

void InterestGroupManagerImpl::UpdateLastKAnonymityReported(
    const std::string& key) {
  impl_.AsyncCall(&InterestGroupStorage::UpdateLastKAnonymityReported)
      .WithArgs(key);
}

void InterestGroupManagerImpl::GetInterestGroupAdAuctionData(
    url::Origin top_level_origin,
    base::Uuid generation_id,
    base::OnceCallback<void(BiddingAndAuctionData)> callback) {
  AdAuctionDataLoaderState state;
  state.serializer.SetPublisher(top_level_origin.Serialize());
  state.serializer.SetGenerationId(std::move(generation_id));
  state.callback = std::move(callback);
  GetAllInterestGroupOwners(base::BindOnce(
      &InterestGroupManagerImpl::LoadNextInterestGroupAdAuctionData,
      weak_factory_.GetWeakPtr(), std::move(state)));
}

void InterestGroupManagerImpl::LoadNextInterestGroupAdAuctionData(
    AdAuctionDataLoaderState state,
    std::vector<url::Origin> owners) {
  if (!owners.empty()) {
    url::Origin next_owner = std::move(owners.back());
    owners.pop_back();
    GetInterestGroupsForOwner(
        next_owner,
        base::BindOnce(
            &InterestGroupManagerImpl::OnLoadedNextInterestGroupAdAuctionData,
            weak_factory_.GetWeakPtr(), std::move(state), std::move(owners),
            next_owner));
    return;
  }
  // Loading is finished.
  OnAdAuctionDataLoadComplete(std::move(state));
}

void InterestGroupManagerImpl::OnLoadedNextInterestGroupAdAuctionData(
    AdAuctionDataLoaderState state,
    std::vector<url::Origin> owners,
    url::Origin owner,
    std::vector<StorageInterestGroup> groups) {
  state.serializer.AddGroups(std::move(owner), std::move(groups));
  LoadNextInterestGroupAdAuctionData(std::move(state), std::move(owners));
}

void InterestGroupManagerImpl::OnAdAuctionDataLoadComplete(
    AdAuctionDataLoaderState state) {
  std::move(state.callback).Run(state.serializer.Build());
}

void InterestGroupManagerImpl::GetBiddingAndAuctionServerKey(
    network::mojom::URLLoaderFactory* loader,
    base::OnceCallback<void(absl::optional<BiddingAndAuctionServerKey>)>
        callback) {
  ba_key_fetcher_.GetOrFetchKey(loader, std::move(callback));
}

void InterestGroupManagerImpl::OnJoinInterestGroupPermissionsChecked(
    blink::InterestGroup group,
    const GURL& joining_url,
    bool report_result_only,
    blink::mojom::AdAuctionService::JoinInterestGroupCallback callback,
    bool can_join) {
  // Invoke callback before calling JoinInterestGroup(), which posts a task to
  // another thread. Any FLEDGE call made from the renderer will need to pass
  // through the UI thread and then bounce over the database thread, so it will
  // see the new InterestGroup, so it's not necessary to actually wait for the
  // database to be updated before invoking the callback. Waiting before
  // invoking the callback may potentially leak whether the user was previously
  // in the InterestGroup through timing differences.
  std::move(callback).Run(/*failed_well_known_check=*/!can_join);
  if (!report_result_only && can_join)
    JoinInterestGroup(std::move(group), joining_url);
}

void InterestGroupManagerImpl::OnLeaveInterestGroupPermissionsChecked(
    const blink::InterestGroupKey& group_key,
    const url::Origin& main_frame,
    bool report_result_only,
    blink::mojom::AdAuctionService::LeaveInterestGroupCallback callback,
    bool can_leave) {
  // Invoke callback before calling LeaveInterestGroup(), which posts a task to
  // another thread. Any FLEDGE call made from the renderer will need to pass
  // through the UI thread and then bounce over the database thread, so it will
  // see the new InterestGroup, so it's not necessary to actually wait for the
  // database to be updated before invoking the callback. Waiting before
  // invoking the callback may potentially leak whether the user was previously
  // in the InterestGroup through timing differences.
  std::move(callback).Run(/*failed_well_known_check=*/!can_leave);
  if (!report_result_only && can_leave)
    LeaveInterestGroup(group_key, main_frame);
}

void InterestGroupManagerImpl::GetInterestGroupsForUpdate(
    const url::Origin& owner,
    int groups_limit,
    base::OnceCallback<void(std::vector<StorageInterestGroup>)> callback) {
  impl_.AsyncCall(&InterestGroupStorage::GetInterestGroupsForUpdate)
      .WithArgs(owner, groups_limit)
      .Then(std::move(callback));
}

void InterestGroupManagerImpl::UpdateInterestGroup(
    const blink::InterestGroupKey& group_key,
    InterestGroupUpdate update,
    base::OnceCallback<void(bool)> callback) {
  NotifyInterestGroupAccessed(InterestGroupObserver::kUpdate, group_key.owner,
                              group_key.name);
  impl_.AsyncCall(&InterestGroupStorage::UpdateInterestGroup)
      .WithArgs(group_key, std::move(update))
      .Then(std::move(callback));
}

void InterestGroupManagerImpl::ReportUpdateFailed(
    const blink::InterestGroupKey& group_key,
    bool parse_failure) {
  impl_.AsyncCall(&InterestGroupStorage::ReportUpdateFailed)
      .WithArgs(group_key, parse_failure);
}

void InterestGroupManagerImpl::OnGetInterestGroupsComplete(
    base::OnceCallback<void(std::vector<StorageInterestGroup>)> callback,
    std::vector<StorageInterestGroup> groups) {
  for (const auto& group : groups) {
    NotifyInterestGroupAccessed(InterestGroupObserver::kLoaded,
                                group.interest_group.owner,
                                group.interest_group.name);
  }
  std::move(callback).Run(std::move(groups));
}

void InterestGroupManagerImpl::NotifyInterestGroupAccessed(
    InterestGroupObserver::AccessType type,
    const url::Origin& owner_origin,
    const std::string& name) {
  // Don't bother getting the time if there are no observers.
  if (observers_.empty())
    return;
  base::Time now = base::Time::Now();
  for (InterestGroupObserver& observer : observers_) {
    observer.OnInterestGroupAccessed(now, type, owner_origin, name);
  }
}

void InterestGroupManagerImpl::TrySendingOneReport() {
  DCHECK_GT(num_active_, 0);

  if (report_requests_.empty()) {
    --num_active_;
    if (num_active_ == 0)
      timeout_timer_.Stop();
    return;
  }

  if (!timeout_timer_.IsRunning()) {
    timeout_timer_.Start(
        FROM_HERE, max_reporting_round_duration_,
        base::BindOnce(&InterestGroupManagerImpl::TimeoutReports,
                       base::Unretained(this)));
  }

  std::unique_ptr<ReportRequest> report_request =
      std::move(report_requests_.front());
  report_requests_.pop_front();

  base::UmaHistogramCounts100000(
      base::StrCat(
          {"Ads.InterestGroup.Net.RequestUrlSizeBytes.", report_request->name}),
      report_request->request_url_size_bytes);
  base::UmaHistogramCounts100(
      base::StrCat(
          {"Ads.InterestGroup.Net.ResponseSizeBytes.", report_request->name}),
      0);

  network::SimpleURLLoader* simple_url_loader_ptr =
      report_request->simple_url_loader.get();
  // Pass simple_url_loader to keep it alive until the request fails or succeeds
  // to prevent cancelling the request.
  simple_url_loader_ptr->DownloadHeadersOnly(
      report_request->url_loader_factory.get(),
      base::BindOnce(&InterestGroupManagerImpl::OnOneReportSent,
                     weak_factory_.GetWeakPtr(),
                     std::move(report_request->simple_url_loader)));
}

void InterestGroupManagerImpl::OnOneReportSent(
    std::unique_ptr<network::SimpleURLLoader> simple_url_loader,
    scoped_refptr<net::HttpResponseHeaders> response_headers) {
  DCHECK_GT(num_active_, 0);

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&InterestGroupManagerImpl::TrySendingOneReport,
                     weak_factory_.GetWeakPtr()),
      reporting_interval_);
}

void InterestGroupManagerImpl::TimeoutReports() {
  // TODO(qingxinwu): maybe add UMA metrics to learn how often this happens.
  report_requests_.clear();
}

void InterestGroupManagerImpl::
    QueueKAnonymityUpdateForInterestGroupFromJoinInterestGroup(
        absl::optional<StorageInterestGroup> maybe_group) {
  // We just joined the group, so it must exist.
  // We don't need to worry about the DB size limit, since older groups
  // are removed first.
  DCHECK(maybe_group);
  if (maybe_group)
    QueueKAnonymityUpdateForInterestGroup(*maybe_group);
}

}  // namespace content
