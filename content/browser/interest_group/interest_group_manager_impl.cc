// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_manager_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/strings/strcat.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
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
    const GURL& url,
    const url::Origin& frame_origin,
    network::mojom::ClientSecurityStatePtr client_security_state) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->redirect_mode = network::mojom::RedirectMode::kError;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->request_initiator = frame_origin;
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->isolation_info =
      net::IsolationInfo::CreateTransient();
  resource_request->trusted_params->client_security_state =
      std::move(client_security_state);
  auto simple_url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), kTrafficAnnotation);
  simple_url_loader->SetTimeoutDuration(base::Seconds(30));
  return simple_url_loader;
}
}  // namespace

InterestGroupManagerImpl::ReportRequest::ReportRequest() = default;
InterestGroupManagerImpl::ReportRequest::~ReportRequest() = default;

InterestGroupManagerImpl::InterestGroupManagerImpl(
    const base::FilePath& path,
    bool in_memory,
    ProcessMode process_mode,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
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
  NotifyInterestGroupAccessed(InterestGroupObserverInterface::kJoin,
                              group.owner.Serialize(), group.name);
  impl_.AsyncCall(&InterestGroupStorage::JoinInterestGroup)
      .WithArgs(std::move(group), std::move(joining_url));
}

void InterestGroupManagerImpl::LeaveInterestGroup(
    const blink::InterestGroupKey& group_key,
    const ::url::Origin& main_frame) {
  NotifyInterestGroupAccessed(InterestGroupObserverInterface::kLeave,
                              group_key.owner.Serialize(), group_key.name);
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

void InterestGroupManagerImpl::set_max_update_round_duration_for_testing(
    base::TimeDelta delta) {
  update_manager_.set_max_update_round_duration_for_testing(delta);  // IN-TEST
}

void InterestGroupManagerImpl::set_max_parallel_updates_for_testing(
    int max_parallel_updates) {
  update_manager_.set_max_parallel_updates_for_testing(  // IN-TEST
      max_parallel_updates);
}

void InterestGroupManagerImpl::RecordInterestGroupBids(
    const blink::InterestGroupSet& group_keys) {
  for (const auto& group_key : group_keys) {
    NotifyInterestGroupAccessed(InterestGroupObserverInterface::kBid,
                                group_key.owner.Serialize(), group_key.name);
  }
  impl_.AsyncCall(&InterestGroupStorage::RecordInterestGroupBids)
      .WithArgs(group_keys);
}

void InterestGroupManagerImpl::RecordInterestGroupWin(
    const blink::InterestGroupKey& group_key,
    const std::string& ad_json) {
  NotifyInterestGroupAccessed(InterestGroupObserverInterface::kWin,
                              group_key.owner.Serialize(), group_key.name);
  impl_.AsyncCall(&InterestGroupStorage::RecordInterestGroupWin)
      .WithArgs(group_key, std::move(ad_json));
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
      .Then(std::move(callback));
}

void InterestGroupManagerImpl::DeleteInterestGroupData(
    StoragePartition::StorageKeyMatcherFunction storage_key_matcher) {
  impl_.AsyncCall(&InterestGroupStorage::DeleteInterestGroupData)
      .WithArgs(std::move(storage_key_matcher));
}

void InterestGroupManagerImpl::GetLastMaintenanceTimeForTesting(
    base::RepeatingCallback<void(base::Time)> callback) const {
  impl_.AsyncCall(&InterestGroupStorage::GetLastMaintenanceTimeForTesting)
      .Then(std::move(callback));
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
  NotifyInterestGroupAccessed(InterestGroupObserverInterface::kUpdate,
                              group_key.owner.Serialize(), group_key.name);
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

void InterestGroupManagerImpl::NotifyInterestGroupAccessed(
    InterestGroupObserverInterface::AccessType type,
    const std::string& owner_origin,
    const std::string& name) {
  // Don't bother getting the time if there are no observers.
  if (observers_.empty())
    return;
  base::Time now = base::Time::Now();
  for (InterestGroupObserverInterface& observer : observers_) {
    observer.OnInterestGroupAccessed(now, type, owner_origin, name);
  }
}

void InterestGroupManagerImpl::HandleReports(
    const std::vector<GURL>& report_urls,
    const url::Origin& frame_origin,
    network::mojom::ClientSecurityStatePtr client_security_state,
    const std::string& name,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  for (const GURL& report_url : report_urls) {
    auto report_request = std::make_unique<ReportRequest>();
    report_request->simple_url_loader = BuildSimpleUrlLoader(
        report_url, frame_origin, client_security_state.Clone());
    report_request->name = name;
    report_request->url_loader_factory = url_loader_factory;
    report_request->request_url_size_bytes = report_url.spec().size();
    report_requests_.emplace_back(std::move(report_request));
  }
}

void InterestGroupManagerImpl::EnqueueReports(
    const std::vector<GURL>& report_urls,
    const std::vector<GURL>& debug_win_report_urls,
    const std::vector<GURL>& debug_loss_report_urls,
    const url::Origin& frame_origin,
    network::mojom::ClientSecurityStatePtr client_security_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  // For memory usage reasons, purge the queue if it has no less than
  // `max_report_queue_length_` entries at the time we're about to add new
  // entries.
  if (report_requests_.size() >=
      static_cast<unsigned int>(max_report_queue_length_)) {
    report_requests_.clear();
  }

  HandleReports(std::move(report_urls), frame_origin,
                client_security_state.Clone(), "SendReportToReport",
                url_loader_factory);
  HandleReports(std::move(debug_loss_report_urls), frame_origin,
                client_security_state.Clone(), "DebugLossReport",
                url_loader_factory);
  HandleReports(std::move(debug_win_report_urls), frame_origin,
                client_security_state.Clone(), "DebugWinReport",
                url_loader_factory);
  if (!report_requests_.empty())
    SendReports();
}

void InterestGroupManagerImpl::ClearPermissionsCache() {
  permissions_checker_.ClearCache();
}

void InterestGroupManagerImpl::SendReports() {
  if (reporting_started_ == base::TimeTicks::Min()) {
    // It appears we're staring a new reporting round; mark the time we started
    // the round.
    reporting_started_ = base::TimeTicks::Now();
  }

  while (!report_requests_.empty() &&
         num_active_ < max_active_report_requests_) {
    num_active_++;
    TrySendingOneReport();
  }
}

void InterestGroupManagerImpl::TrySendingOneReport() {
  if (base::TimeTicks::Now() - reporting_started_ >
      max_reporting_round_duration_) {
    // We've been reporting for too long; delete all pending reports in the
    // queue.
    // TODO(qingxinwu): maybe add UMA metrics to learn how often this happens.
    report_requests_.clear();
    reporting_started_ = base::TimeTicks::Min();
  }

  if (report_requests_.empty()) {
    DCHECK_GT(num_active_, 0);
    num_active_--;
    if (num_active_ == 0) {
      // This reporting round is finished, there's no more work to do.
      reporting_started_ = base::TimeTicks::Min();
    }
    return;
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

  if (!report_requests_.empty()) {
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&InterestGroupManagerImpl::TrySendingOneReport,
                       weak_factory_.GetWeakPtr()),
        reporting_interval_);
    return;
  }
  num_active_--;
}

void InterestGroupManagerImpl::set_max_active_report_requests_for_testing(
    int max_active_report_requests) {
  max_active_report_requests_ = max_active_report_requests;
}

void InterestGroupManagerImpl::SetInterestGroupPriority(
    const blink::InterestGroupKey& group_key,
    double priority) {
  impl_.AsyncCall(&InterestGroupStorage::SetInterestGroupPriority)
      .WithArgs(group_key, priority);
}

void InterestGroupManagerImpl::set_max_report_queue_length_for_testing(
    int max_queue_length) {
  max_report_queue_length_ = max_queue_length;
}

void InterestGroupManagerImpl::set_max_reporting_round_duration_for_testing(
    base::TimeDelta max_reporting_round_duration) {
  max_reporting_round_duration_ = max_reporting_round_duration;
}

void InterestGroupManagerImpl::set_reporting_interval_for_testing(
    base::TimeDelta interval) {
  reporting_interval_ = interval;
}

}  // namespace content
