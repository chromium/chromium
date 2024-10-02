// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_manager_impl.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/to_string.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/devtools/network_service_devtools_observer.h"
#include "content/browser/interest_group/ad_auction_page_data.h"
#include "content/browser/interest_group/for_debugging_only_report_util.h"
#include "content/browser/interest_group/interest_group_caching_storage.h"
#include "content/browser/interest_group/interest_group_real_time_report_util.h"
#include "content/browser/interest_group/interest_group_storage.h"
#include "content/browser/interest_group/interest_group_update.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/services/auction_worklet/public/cpp/real_time_reporting.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "url/gurl.h"
#include "url/origin.h"

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
// Version of real time report.
constexpr int kRealTimeReportDataVersion = 1;

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

mojo::PendingRemote<network::mojom::DevToolsObserver> CreateDevtoolsObserver(
    FrameTreeNodeId frame_tree_node_id) {
  if (frame_tree_node_id) {
    FrameTreeNode* initiator_frame_tree_node =
        FrameTreeNode::GloballyFindByID(frame_tree_node_id);

    if (initiator_frame_tree_node) {
      return NetworkServiceDevToolsObserver::MakeSelfOwned(
          initiator_frame_tree_node);
    }
  }
  return mojo::PendingRemote<network::mojom::DevToolsObserver>();
}

// Creates an uncredentialed request to use for the SimpleURLLoader and
// reporting to devtools.
std::unique_ptr<network::ResourceRequest> BuildUncredentialedRequest(
    GURL url,
    const url::Origin& frame_origin,
    FrameTreeNodeId frame_tree_node_id,
    const network::mojom::ClientSecurityState& client_security_state,
    bool is_post_method) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = std::move(url);
  if (is_post_method) {
    resource_request->method = net::HttpRequestHeaders::kPostMethod;
  }
  resource_request->devtools_request_id =
      base::UnguessableToken::Create().ToString();
  resource_request->redirect_mode = network::mojom::RedirectMode::kError;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->request_initiator = frame_origin;
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->isolation_info =
      net::IsolationInfo::CreateTransient();
  resource_request->trusted_params->client_security_state =
      client_security_state.Clone();

  bool network_instrumentation_enabled = false;
  if (frame_tree_node_id) {
    FrameTreeNode* frame_tree_node =
        FrameTreeNode::GloballyFindByID(frame_tree_node_id);

    if (frame_tree_node != nullptr) {
      devtools_instrumentation::ApplyAuctionNetworkRequestOverrides(
          frame_tree_node, resource_request.get(),
          &network_instrumentation_enabled);
    }
  }
  if (network_instrumentation_enabled) {
    resource_request->enable_load_timing = true;
    resource_request->trusted_params->devtools_observer =
        CreateDevtoolsObserver(frame_tree_node_id);
  }

  return resource_request;
}

std::vector<uint8_t> BuildRealTimeReport(
    const std::vector<uint8_t>& real_time_histogram) {
  size_t num_user_buckets =
      blink::features::kFledgeRealTimeReportingNumBuckets.Get();
  size_t num_platform_buckets =
      auction_worklet::RealTimeReportingPlatformError::kNumValues;
  CHECK_EQ(real_time_histogram.size(), num_user_buckets + num_platform_buckets);

  std::vector<uint8_t> histogram_list;
  std::vector<uint8_t> platform_histogram_list;
  for (size_t i = 0; i < real_time_histogram.size(); i++) {
    if (i < num_user_buckets) {
      histogram_list.push_back(real_time_histogram[i]);
    } else {
      platform_histogram_list.push_back(real_time_histogram[i]);
    }
  }

  cbor::Value::MapValue histogram_map;
  histogram_map.emplace("length", static_cast<int64_t>(histogram_list.size()));
  histogram_map.emplace("buckets", BitPacking(std::move(histogram_list)));

  cbor::Value::MapValue platform_histogram_map;
  platform_histogram_map.emplace(
      "length", static_cast<int64_t>(platform_histogram_list.size()));
  platform_histogram_map.emplace(
      "buckets", BitPacking(std::move(platform_histogram_list)));

  cbor::Value::MapValue report;
  report.emplace("version", kRealTimeReportDataVersion);
  report.emplace("histogram", std::move(histogram_map));
  report.emplace("platformHistogram", std::move(platform_histogram_map));
  std::optional<std::vector<uint8_t>> report_cbor =
      cbor::Writer::Write(cbor::Value(std::move(report)));
  if (!report_cbor.has_value()) {
    return {};
  }
  return *report_cbor;
}

// Makes a SimpleURLLoader for a given request. Returns the SimpleURLLoader
// which will be used to report the result of an in-browser interest group based
// ad auction to an auction participant.
std::unique_ptr<network::SimpleURLLoader> BuildSimpleUrlLoader(
    std::unique_ptr<network::ResourceRequest> resource_request,
    std::optional<std::vector<uint8_t>> real_time_histogram) {
  auto simple_url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), kTrafficAnnotation);
  simple_url_loader->SetTimeoutDuration(base::Seconds(30));
  simple_url_loader->SetAllowHttpErrorResults(true);

  if (real_time_histogram.has_value()) {
    auto report = BuildRealTimeReport(*real_time_histogram);
    simple_url_loader->AttachStringForUpload(
        std::string(report.begin(), report.end()), "application/cbor");
  }

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

double GetRealTimeReportingQuota(
    std::optional<std::pair<base::TimeTicks, double>> quota,
    base::TimeTicks now,
    double max_real_time_reports,
    base::TimeDelta rate_limit_window) {
  if (!quota.has_value()) {
    return max_real_time_reports;
  }

  double recovered_quota = max_real_time_reports *
                           (now - quota->first).InMillisecondsF() /
                           rate_limit_window.InMilliseconds();
  double new_quota = quota->second + recovered_quota;
  return std::min(new_quota, max_real_time_reports);
}

}  // namespace

InterestGroupManagerImpl::ReportRequest::ReportRequest() = default;
InterestGroupManagerImpl::ReportRequest::~ReportRequest() = default;

InterestGroupManagerImpl::AdAuctionDataLoaderState::AdAuctionDataLoaderState()
    : start_time(base::TimeTicks::Now()) {}
InterestGroupManagerImpl::AdAuctionDataLoaderState::AdAuctionDataLoaderState(
    AdAuctionDataLoaderState&& state) = default;
InterestGroupManagerImpl::AdAuctionDataLoaderState::
    ~AdAuctionDataLoaderState() = default;

InterestGroupManagerImpl::InterestGroupManagerImpl(
    const base::FilePath& path,
    bool in_memory,
    ProcessMode process_mode,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    GetKAnonymityServiceDelegateCallback k_anonymity_service_callback)
    : caching_storage_(path, in_memory),
      auction_process_manager_(
          base::WrapUnique(process_mode == ProcessMode::kDedicated
                               ? static_cast<AuctionProcessManager*>(
                                     new DedicatedAuctionProcessManager())
                               : new InRendererAuctionProcessManager())),
      update_manager_(this, url_loader_factory),
      k_anonymity_manager_(std::make_unique<InterestGroupKAnonymityManager>(
          this,
          std::move(k_anonymity_service_callback))),
      max_active_report_requests_(kMaxActiveReportRequests),
      max_report_queue_length_(kMaxReportQueueLength),
      reporting_interval_(kReportingInterval),
      max_reporting_round_duration_(kMaxReportingRoundDuration),
      real_time_reporting_window_(
          blink::features::kFledgeRealTimeReportingWindow.Get()),
      max_real_time_reports_(static_cast<double>(
          blink::features::kFledgeRealTimeReportingMaxReports.Get())),
      ba_key_fetcher_(this, std::move(url_loader_factory)) {}

InterestGroupManagerImpl::~InterestGroupManagerImpl() = default;

void InterestGroupManagerImpl::GetAllInterestGroupJoiningOrigins(
    base::OnceCallback<void(std::vector<url::Origin>)> callback) {
  caching_storage_.GetAllInterestGroupJoiningOrigins(std::move(callback));
}

void InterestGroupManagerImpl::GetAllInterestGroupDataKeys(
    base::OnceCallback<void(std::vector<InterestGroupDataKey>)> callback) {
  caching_storage_.GetAllInterestGroupOwnerJoinerPairs(
      base::BindOnce(&ConvertOwnerJoinerPairsToDataKeys)
          .Then(std::move(callback)));
}

void InterestGroupManagerImpl::RemoveInterestGroupsByDataKey(
    InterestGroupDataKey data_key,
    base::OnceClosure callback) {
  caching_storage_.RemoveInterestGroupsMatchingOwnerAndJoiner(
      data_key.owner, data_key.joining_origin, std::move(callback));
}

void InterestGroupManagerImpl::CheckPermissionsAndJoinInterestGroup(
    blink::InterestGroup group,
    const GURL& joining_url,
    const url::Origin& frame_origin,
    const net::NetworkIsolationKey& network_isolation_key,
    bool report_result_only,
    network::mojom::URLLoaderFactory& url_loader_factory,
    AreReportingOriginsAttestedCallback attestation_callback,
    blink::mojom::AdAuctionService::JoinInterestGroupCallback callback) {
  url::Origin interest_group_owner = group.owner;
  permissions_checker_.CheckPermissions(
      InterestGroupPermissionsChecker::Operation::kJoin, frame_origin,
      interest_group_owner, network_isolation_key, url_loader_factory,
      base::BindOnce(
          &InterestGroupManagerImpl::OnJoinInterestGroupPermissionsChecked,
          base::Unretained(this), std::move(group), joining_url,
          report_result_only, std::move(attestation_callback),
          std::move(callback)));
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

void InterestGroupManagerImpl::
    CheckPermissionsAndClearOriginJoinedInterestGroups(
        const url::Origin& owner,
        const std::vector<std::string>& interest_groups_to_keep,
        const url::Origin& main_frame_origin,
        const url::Origin& frame_origin,
        const net::NetworkIsolationKey& network_isolation_key,
        bool report_result_only,
        network::mojom::URLLoaderFactory& url_loader_factory,
        blink::mojom::AdAuctionService::LeaveInterestGroupCallback callback) {
  permissions_checker_.CheckPermissions(
      InterestGroupPermissionsChecker::Operation::kLeave, frame_origin, owner,
      network_isolation_key, url_loader_factory,
      base::BindOnce(&InterestGroupManagerImpl::
                         OnClearOriginJoinedInterestGroupsPermissionsChecked,
                     base::Unretained(this), owner,
                     std::set<std::string>(interest_groups_to_keep.begin(),
                                           interest_groups_to_keep.end()),
                     main_frame_origin, report_result_only,
                     std::move(callback)));
}

void InterestGroupManagerImpl::JoinInterestGroup(blink::InterestGroup group,
                                                 const GURL& joining_url) {
  // Create notify callback first.
  base::OnceClosure notify_callback = CreateNotifyInterestGroupAccessedCallback(
      InterestGroupObserver::kJoin, group.owner, group.name);

  blink::InterestGroupKey group_key(group.owner, group.name);
  caching_storage_.JoinInterestGroup(
      group, joining_url,
      base::BindOnce(
          &InterestGroupManagerImpl::QueueKAnonymityUpdateForInterestGroup,
          weak_factory_.GetWeakPtr(), group_key)
          .Then(std::move(notify_callback)));
}

void InterestGroupManagerImpl::LeaveInterestGroup(
    const blink::InterestGroupKey& group_key,
    const ::url::Origin& main_frame) {
  caching_storage_.LeaveInterestGroup(
      group_key, main_frame,
      CreateNotifyInterestGroupAccessedCallback(
          InterestGroupObserver::kLeave, group_key.owner, group_key.name));
}

void InterestGroupManagerImpl::ClearOriginJoinedInterestGroups(
    const url::Origin& owner,
    std::set<std::string> interest_groups_to_keep,
    url::Origin main_frame_origin) {
  caching_storage_.ClearOriginJoinedInterestGroups(
      owner, interest_groups_to_keep, main_frame_origin,
      base::BindOnce(
          &InterestGroupManagerImpl::OnClearOriginJoinedInterestGroupsComplete,
          weak_factory_.GetWeakPtr(), owner));
}

void InterestGroupManagerImpl::OnClearOriginJoinedInterestGroupsComplete(
    const url::Origin& owner,
    std::vector<std::string> left_interest_group_names) {
  for (const auto& name : left_interest_group_names) {
    NotifyInterestGroupAccessed(
        /*devtools_auction_id=*/std::nullopt, InterestGroupObserver::kClear,
        owner, name, /*component_seller_origin=*/std::nullopt,
        /*bid=*/std::nullopt, /*bid_currency=*/std::nullopt);
  }
}

void InterestGroupManagerImpl::UpdateInterestGroupsOfOwner(
    const url::Origin& owner,
    network::mojom::ClientSecurityStatePtr client_security_state,
    AreReportingOriginsAttestedCallback callback) {
  update_manager_.UpdateInterestGroupsOfOwner(
      owner, std::move(client_security_state), std::move(callback));
}

void InterestGroupManagerImpl::UpdateInterestGroupsOfOwners(
    std::vector<url::Origin> owners,
    network::mojom::ClientSecurityStatePtr client_security_state,
    AreReportingOriginsAttestedCallback callback) {
  update_manager_.UpdateInterestGroupsOfOwners(
      owners, std::move(client_security_state), std::move(callback));
}

void InterestGroupManagerImpl::UpdateInterestGroupsOfOwnersWithDelay(
    std::vector<url::Origin> owners,
    network::mojom::ClientSecurityStatePtr client_security_state,
    AreReportingOriginsAttestedCallback callback,
    const base::TimeDelta& delay) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&InterestGroupManagerImpl::UpdateInterestGroupsOfOwners,
                     weak_factory_.GetWeakPtr(), std::move(owners),
                     std::move(client_security_state), std::move(callback)),
      delay);
}

void InterestGroupManagerImpl::AllowUpdateIfOlderThan(
    blink::InterestGroupKey group_key,
    base::TimeDelta update_if_older_than) {
  caching_storage_.AllowUpdateIfOlderThan(std::move(group_key),
                                          update_if_older_than);
}

void InterestGroupManagerImpl::RecordInterestGroupBids(
    const blink::InterestGroupSet& group_keys) {
  if (group_keys.empty()) {
    return;
  }
  caching_storage_.RecordInterestGroupBids(group_keys);
}

void InterestGroupManagerImpl::RecordInterestGroupWin(
    const blink::InterestGroupKey& group_key,
    const std::string& ad_json) {
  caching_storage_.RecordInterestGroupWin(group_key, ad_json);
}

void InterestGroupManagerImpl::RecordDebugReportLockout(
    base::Time last_report_sent_time) {
  caching_storage_.RecordDebugReportLockout(last_report_sent_time);
}

void InterestGroupManagerImpl::RecordDebugReportCooldown(
    const url::Origin& origin,
    base::Time cooldown_start,
    DebugReportCooldownType cooldown_type) {
  caching_storage_.RecordDebugReportCooldown(origin, cooldown_start,
                                             cooldown_type);
}

void InterestGroupManagerImpl::RegisterAdKeysAsJoined(
    base::flat_set<std::string> hashed_keys) {
  k_anonymity_manager_->RegisterAdKeysAsJoined(std::move(hashed_keys));
}

void InterestGroupManagerImpl::GetInterestGroup(
    const url::Origin& owner,
    const std::string& name,
    base::OnceCallback<void(std::optional<SingleStorageInterestGroup>)>
        callback) {
  GetInterestGroup(blink::InterestGroupKey(owner, name), std::move(callback));
}
void InterestGroupManagerImpl::GetInterestGroup(
    const blink::InterestGroupKey& group_key,
    base::OnceCallback<void(std::optional<SingleStorageInterestGroup>)>
        callback) {
  caching_storage_.GetInterestGroup(group_key, std::move(callback));
}

void InterestGroupManagerImpl::GetAllInterestGroupOwners(
    base::OnceCallback<void(std::vector<url::Origin>)> callback) {
  caching_storage_.GetAllInterestGroupOwners(std::move(callback));
}

void InterestGroupManagerImpl::GetInterestGroupsForOwner(
    const std::optional<std::string>& devtools_auction_id,
    const url::Origin& owner,
    base::OnceCallback<void(scoped_refptr<StorageInterestGroups>)> callback) {
  caching_storage_.GetInterestGroupsForOwner(
      owner,
      base::BindOnce(&InterestGroupManagerImpl::OnGetInterestGroupsComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     devtools_auction_id));
}

bool InterestGroupManagerImpl::GetCachedOwnerAndSignalsOrigins(
    const url::Origin& owner,
    std::optional<url::Origin>& signals_origin) {
  return caching_storage_.GetCachedOwnerAndSignalsOrigins(owner,
                                                          signals_origin);
}

void InterestGroupManagerImpl::DeleteInterestGroupData(
    StoragePartition::StorageKeyMatcherFunction storage_key_matcher,
    base::OnceClosure completion_callback) {
  caching_storage_.DeleteInterestGroupData(storage_key_matcher,
                                           std::move(completion_callback));
}

void InterestGroupManagerImpl::DeleteAllInterestGroupData(
    base::OnceClosure completion_callback) {
  caching_storage_.DeleteAllInterestGroupData(std::move(completion_callback));
}

void InterestGroupManagerImpl::GetLastMaintenanceTimeForTesting(
    base::RepeatingCallback<void(base::Time)> callback) const {
  caching_storage_.GetLastMaintenanceTimeForTesting(  // IN-TEST
      std::move(callback));
}

void InterestGroupManagerImpl::EnqueueReports(
    ReportType report_type,
    std::vector<GURL> report_urls,
    FrameTreeNodeId frame_tree_node_id,
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
    report_request->report_url = std::move(report_url);
    report_request->frame_origin = frame_origin;
    report_request->client_security_state = client_security_state;
    report_request->name = report_type_name;
    report_request->url_loader_factory = url_loader_factory;
    report_request->frame_tree_node_id = frame_tree_node_id;
    report_requests_.emplace_back(std::move(report_request));
  }

  while (!report_requests_.empty() &&
         num_active_ < max_active_report_requests_) {
    ++num_active_;
    TrySendingOneReport();
  }
}

void InterestGroupManagerImpl::EnqueueRealTimeReports(
    std::map<url::Origin, RealTimeReportingContributions> contributions,
    AdAuctionPageDataCallback ad_auction_page_data_callback,
    FrameTreeNodeId frame_tree_node_id,
    const url::Origin& frame_origin,
    const network::mojom::ClientSecurityState& client_security_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  if (contributions.empty()) {
    return;
  }

  AdAuctionPageData* ad_auction_page_data = ad_auction_page_data_callback.Run();
  if (!ad_auction_page_data) {
    // The page is destroyed. Don't enqueue the real time reports.
    return;
  }

  // For memory usage reasons, purge the queue if it has at least
  // `max_report_queue_length_` entries at the time we're about to add new
  // entries.
  if (report_requests_.size() >=
      static_cast<unsigned int>(max_report_queue_length_)) {
    report_requests_.clear();
  }

  std::map<url::Origin, std::vector<uint8_t>> histograms =
      CalculateRealTimeReportingHistograms(std::move(contributions));

  base::TimeTicks now = base::TimeTicks::Now();
  for (auto& [origin, histogram] : histograms) {
    double quota = GetRealTimeReportingQuota(
        ad_auction_page_data->GetRealTimeReportingQuota(origin), now,
        max_real_time_reports_, real_time_reporting_window_);
    if (quota < 1) {
      continue;
    }
    ad_auction_page_data->UpdateRealTimeReportingQuota(origin,
                                                       {now, quota - 1});
    auto report_request = std::make_unique<ReportRequest>();
    GURL report_url = GetRealTimeReportDestination(origin);
    report_request->request_url_size_bytes = report_url.spec().size();
    report_request->report_url = std::move(report_url);
    report_request->real_time_histogram = std::move(histogram);
    report_request->frame_origin = frame_origin;
    report_request->client_security_state = client_security_state;
    report_request->name = "RealTimeReport";
    report_request->url_loader_factory = url_loader_factory;
    report_request->frame_tree_node_id = frame_tree_node_id;
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
  caching_storage_.SetInterestGroupPriority(group_key, priority);
}

void InterestGroupManagerImpl::UpdateInterestGroupPriorityOverrides(
    const blink::InterestGroupKey& group_key,
    base::flat_map<std::string,
                   auction_worklet::mojom::PrioritySignalsDoublePtr>
        update_priority_signals_overrides) {
  caching_storage_.UpdateInterestGroupPriorityOverrides(
      group_key, std::move(update_priority_signals_overrides));
}

void InterestGroupManagerImpl::SetBiddingAndAuctionServerKeys(
    const url::Origin& coordinator,
    const std::vector<BiddingAndAuctionServerKey>& keys,
    base::Time expiration) {
  caching_storage_.SetBiddingAndAuctionServerKeys(coordinator, keys,
                                                  expiration);
}
void InterestGroupManagerImpl::GetBiddingAndAuctionServerKeys(
    const url::Origin& coordinator,
    base::OnceCallback<
        void(std::pair<base::Time, std::vector<BiddingAndAuctionServerKey>>)>
        callback) {
  caching_storage_.GetBiddingAndAuctionServerKeys(coordinator,
                                                  std::move(callback));
}

void InterestGroupManagerImpl::ClearPermissionsCache() {
  permissions_checker_.ClearCache();
}

void InterestGroupManagerImpl::QueueKAnonymityUpdateForInterestGroup(
    const blink::InterestGroupKey& group_key,
    const std::optional<InterestGroupKanonUpdateParameter> update_parameter) {
  if (update_parameter) {
    k_anonymity_manager_->QueryKAnonymityData(group_key,
                                              update_parameter.value());
  }
}

void InterestGroupManagerImpl::UpdateKAnonymity(
    const blink::InterestGroupKey& interest_group_key,
    const std::vector<std::string>& positive_hashed_keys,
    const base::Time update_time,
    bool replace_existing_values) {
  caching_storage_.UpdateKAnonymity(interest_group_key, positive_hashed_keys,
                                    update_time, replace_existing_values);
}

void InterestGroupManagerImpl::GetLastKAnonymityReported(
    const std::string& hashed_key,
    base::OnceCallback<void(std::optional<base::Time>)> callback) {
  caching_storage_.GetLastKAnonymityReported(hashed_key, std::move(callback));
}

void InterestGroupManagerImpl::UpdateLastKAnonymityReported(
    const std::string& hashed_key) {
  caching_storage_.UpdateLastKAnonymityReported(hashed_key);
}

void InterestGroupManagerImpl::GetInterestGroupAdAuctionData(
    url::Origin top_level_origin,
    base::Uuid generation_id,
    base::Time timestamp,
    blink::mojom::AuctionDataConfigPtr config,
    base::OnceCallback<void(BiddingAndAuctionData)> callback) {
  AdAuctionDataLoaderState state;
  state.serializer.SetPublisher(top_level_origin.host());
  state.serializer.SetGenerationId(std::move(generation_id));
  state.serializer.SetTimestamp(timestamp);
  state.callback = std::move(callback);
  if (config->per_buyer_configs.size() == 0) {
    state.serializer.SetConfig(std::move(config));
    GetAllInterestGroupOwners(
        base::BindOnce(&InterestGroupManagerImpl::
                           ShuffleOwnersThenLoadInterestGroupAdAuctionData,
                       weak_factory_.GetWeakPtr(), std::move(state)));
  } else {
    std::vector<url::Origin> owners;
    owners.reserve(config->per_buyer_configs.size());
    std::vector<url::Origin> sized_owners;
    for (const auto& buyer_config : config->per_buyer_configs) {
      if (buyer_config.second->target_size) {
        sized_owners.push_back(buyer_config.first);
      } else {
        owners.push_back(buyer_config.first);
      }
    }
    // Shuffle the owners. The algorithm for serializing interest groups is
    // slightly unfair in that owners that are serialize first can't take
    // advantage of space left over from when subsequent owners don't use all
    // their assigned space. Randomizing the order avoids always penalizing the
    // same owner.
    base::RandomShuffle(owners.begin(), owners.end());
    base::RandomShuffle(sized_owners.begin(), sized_owners.end());

    // Move sized owners to the end. We load groups in reverse order and then
    // serialize them in order, so this means we process the sized owners first.
    // Unsized owners share the remaining space so we want to process them last.
    std::move(sized_owners.begin(), sized_owners.end(),
              std::back_inserter(owners));

    state.serializer.SetConfig(std::move(config));
    LoadNextInterestGroupAdAuctionData(std::move(state), std::move(owners));
  }
}

void InterestGroupManagerImpl::ShuffleOwnersThenLoadInterestGroupAdAuctionData(
    AdAuctionDataLoaderState state,
    std::vector<url::Origin> owners) {
  // Shuffle the owners. The algorithm for serializing interest groups is
  // slightly unfair in that owners that are serialize first can't take
  // advantage of space left over from when subsequent owners don't use all
  // their assigned space. Randomizing the order avoids always penalizing the
  // same owner.
  base::RandomShuffle(owners.begin(), owners.end());
  LoadNextInterestGroupAdAuctionData(std::move(state), std::move(owners));
}

void InterestGroupManagerImpl::LoadNextInterestGroupAdAuctionData(
    AdAuctionDataLoaderState state,
    std::vector<url::Origin> owners) {
  if (!owners.empty()) {
    url::Origin next_owner = std::move(owners.back());
    owners.pop_back();
    // Since a single B&A blob can be associated with multiple auctions, we
    // can't link these loads to a specific one.
    GetInterestGroupsForOwner(
        /*devtools_auction_id=*/std::nullopt, next_owner,
        base::BindOnce(
            &InterestGroupManagerImpl::OnLoadedNextInterestGroupAdAuctionData,
            weak_factory_.GetWeakPtr(), std::move(state), std::move(owners),
            next_owner));
    return;
  }
  // Loading is finished.
  OnInterestGroupAdAuctionDataLoadComplete(std::move(state));
}

void InterestGroupManagerImpl::OnLoadedNextInterestGroupAdAuctionData(
    AdAuctionDataLoaderState state,
    std::vector<url::Origin> owners,
    url::Origin owner,
    scoped_refptr<StorageInterestGroups> groups) {
  state.serializer.AddGroups(std::move(owner), std::move(groups));
  LoadNextInterestGroupAdAuctionData(std::move(state), std::move(owners));
}

void InterestGroupManagerImpl::OnInterestGroupAdAuctionDataLoadComplete(
    AdAuctionDataLoaderState state) {
  if (blink::features::kFledgeEnableFilteringDebugReportStartingFrom.Get() !=
      base::Milliseconds(0)) {
    caching_storage_.GetDebugReportLockout(
        base::BindOnce(&InterestGroupManagerImpl::OnAdAuctionDataLoadComplete,
                       weak_factory_.GetWeakPtr(), std::move(state)));
  } else {
    OnAdAuctionDataLoadComplete(std::move(state), std::nullopt);
  }
}

void InterestGroupManagerImpl::OnAdAuctionDataLoadComplete(
    AdAuctionDataLoaderState state,
    std::optional<base::Time> last_report_sent_time) {
  state.serializer.SetDebugReportInLockout(
      IsInDebugReportLockout(last_report_sent_time, base::Time::Now()));
  BiddingAndAuctionData data = state.serializer.Build();
  base::UmaHistogramTimes(
      "Ads.InterestGroup.ServerAuction.AdAuctionDataLoadTime",
      base::TimeTicks::Now() - state.start_time);
  std::move(state.callback).Run(std::move(data));
}

void InterestGroupManagerImpl::GetBiddingAndAuctionServerKey(
    const std::optional<url::Origin>& coordinator,
    base::OnceCallback<void(
        base::expected<BiddingAndAuctionServerKey, std::string>)> callback) {
  ba_key_fetcher_.GetOrFetchKey(coordinator, std::move(callback));
}

void InterestGroupManagerImpl::OnJoinInterestGroupPermissionsChecked(
    blink::InterestGroup group,
    const GURL& joining_url,
    bool report_result_only,
    AreReportingOriginsAttestedCallback attestation_callback,
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

  if (!report_result_only && can_join) {
    // All ads' allowed reporting origins must be attested. Otherwise don't
    // join.
    if (group.ads) {
      for (auto& ad : *group.ads) {
        if (ad.allowed_reporting_origins) {
          // Sort and de-duplicate by passing it through a flat_set.
          ad.allowed_reporting_origins =
              base::flat_set<url::Origin>(
                  std::move(ad.allowed_reporting_origins.value()))
                  .extract();
          if (!attestation_callback.Run(ad.allowed_reporting_origins.value())) {
            return;
          }
        }
      }
    }
    JoinInterestGroup(std::move(group), joining_url);
  }
  ba_key_fetcher_.MaybePrefetchKeys();
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
  if (!report_result_only && can_leave) {
    LeaveInterestGroup(group_key, main_frame);
  }
}

void InterestGroupManagerImpl::
    OnClearOriginJoinedInterestGroupsPermissionsChecked(
        url::Origin owner,
        std::set<std::string> interest_groups_to_keep,
        url::Origin main_frame_origin,
        bool report_result_only,
        blink::mojom::AdAuctionService::LeaveInterestGroupCallback callback,
        bool can_leave) {
  // Invoke callback before calling ClearOriginJoinedInterestGroups(), which
  // posts a task to another thread. Any FLEDGE call made from the renderer will
  // need to pass through the UI thread and then bounce over the database
  // thread, so it will see the new InterestGroup, so it's not necessary to
  // actually wait for the database to be updated before invoking the callback.
  // Waiting before invoking the callback may potentially leak whether the user
  // was previously in the InterestGroup through timing differences.
  std::move(callback).Run(/*failed_well_known_check=*/!can_leave);

  if (!report_result_only && can_leave) {
    ClearOriginJoinedInterestGroups(std::move(owner),
                                    std::move(interest_groups_to_keep),
                                    std::move(main_frame_origin));
  }
}

void InterestGroupManagerImpl::GetInterestGroupsForUpdate(
    const url::Origin& owner,
    int groups_limit,
    base::OnceCallback<void(std::vector<InterestGroupUpdateParameter>)>
        callback) {
  caching_storage_.GetInterestGroupsForUpdate(owner, groups_limit,
                                              std::move(callback));
}

void InterestGroupManagerImpl::GetDebugReportLockoutAndCooldowns(
    base::flat_set<url::Origin> origins,
    base::OnceCallback<void(std::optional<DebugReportLockoutAndCooldowns>)>
        callback) {
  caching_storage_.GetDebugReportLockoutAndCooldowns(std::move(origins),
                                                     std::move(callback));
}

void InterestGroupManagerImpl::UpdateInterestGroup(
    const blink::InterestGroupKey& group_key,
    InterestGroupUpdate update,
    base::OnceCallback<void(bool)> callback) {
  caching_storage_.UpdateInterestGroup(
      group_key, std::move(update),
      base::BindOnce(&InterestGroupManagerImpl::OnUpdateComplete,
                     weak_factory_.GetWeakPtr(), group_key,
                     std::move(callback)));
}

void InterestGroupManagerImpl::OnUpdateComplete(
    const blink::InterestGroupKey& group_key,
    base::OnceCallback<void(bool)> callback,
    std::optional<InterestGroupKanonUpdateParameter> kanon_update_parameter) {
  NotifyInterestGroupAccessed(
      /*devtools_auction_id=*/std::nullopt, InterestGroupObserver::kUpdate,
      group_key.owner, group_key.name, /*component_seller_origin=*/std::nullopt,
      /*bid=*/std::nullopt,
      /*bid_currency=*/std::nullopt);
  std::move(callback).Run(kanon_update_parameter.has_value());
  QueueKAnonymityUpdateForInterestGroup(group_key,
                                        std::move(kanon_update_parameter));
}

void InterestGroupManagerImpl::ReportUpdateFailed(
    const blink::InterestGroupKey& group_key,
    bool parse_failure) {
  caching_storage_.ReportUpdateFailed(group_key, parse_failure);
}

void InterestGroupManagerImpl::OnGetInterestGroupsComplete(
    base::OnceCallback<void(scoped_refptr<StorageInterestGroups>)> callback,
    const std::optional<std::string>& devtools_auction_id,
    scoped_refptr<StorageInterestGroups> groups) {
  for (const SingleStorageInterestGroup& group : groups->GetInterestGroups()) {
    NotifyInterestGroupAccessed(
        devtools_auction_id, InterestGroupObserver::kLoaded,
        group->interest_group.owner, group->interest_group.name,
        /*component_seller_origin=*/std::nullopt,
        /*bid=*/std::nullopt, /*bid_currency=*/std::nullopt);
  }
  std::move(callback).Run(std::move(groups));
}

void InterestGroupManagerImpl::NotifyInterestGroupAccessed(
    base::optional_ref<const std::string> devtools_auction_id,
    InterestGroupObserver::AccessType type,
    const url::Origin& owner_origin,
    const std::string& name,
    base::optional_ref<const url::Origin> component_seller_origin,
    std::optional<double> bid,
    base::optional_ref<const std::string> bid_currency) {
  // Don't bother getting the time if there are no observers.
  if (observers_.empty()) {
    return;
  }
  base::Time now = base::Time::Now();
  for (InterestGroupObserver& observer : observers_) {
    observer.OnInterestGroupAccessed(
        devtools_auction_id, now, type, owner_origin, name,
        component_seller_origin, bid, bid_currency);
  }
}

void InterestGroupManagerImpl::TrySendingOneReport() {
  DCHECK_GT(num_active_, 0);

  if (report_requests_.empty()) {
    --num_active_;
    if (num_active_ == 0) {
      timeout_timer_.Stop();
    }
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

  FrameTreeNodeId frame_tree_node_id = report_request->frame_tree_node_id;

  base::UmaHistogramCounts100000(
      base::StrCat(
          {"Ads.InterestGroup.Net.RequestUrlSizeBytes.", report_request->name}),
      report_request->request_url_size_bytes);
  base::UmaHistogramCounts100(
      base::StrCat(
          {"Ads.InterestGroup.Net.ResponseSizeBytes.", report_request->name}),
      0);

  std::unique_ptr<network::ResourceRequest> resource_request =
      BuildUncredentialedRequest(
          report_request->report_url, report_request->frame_origin,
          report_request->frame_tree_node_id,
          report_request->client_security_state,
          /*is_post_method=*/report_request->real_time_histogram.has_value()
              ? true
              : false);

  std::string devtools_request_id =
      resource_request->devtools_request_id.value();

  devtools_instrumentation::OnAuctionWorkletNetworkRequestWillBeSent(
      report_request->frame_tree_node_id, *resource_request,
      base::TimeTicks::Now());

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      BuildSimpleUrlLoader(std::move(resource_request),
                           report_request->real_time_histogram);

  // Pass simple_url_loader to keep it alive until the request fails or succeeds
  // to prevent cancelling the request.
  network::SimpleURLLoader* simple_url_loader_ptr = simple_url_loader.get();
  simple_url_loader_ptr->DownloadHeadersOnly(
      report_request->url_loader_factory.get(),
      base::BindOnce(&InterestGroupManagerImpl::OnOneReportSent,
                     weak_factory_.GetWeakPtr(), std::move(simple_url_loader),
                     frame_tree_node_id, std::move(devtools_request_id)));
}

void InterestGroupManagerImpl::OnOneReportSent(
    std::unique_ptr<network::SimpleURLLoader> simple_url_loader,
    FrameTreeNodeId frame_tree_node_id,
    const std::string& devtools_request_id,
    scoped_refptr<net::HttpResponseHeaders> response_headers) {
  DCHECK_GT(num_active_, 0);

  network::URLLoaderCompletionStatus completion_status =
      network::URLLoaderCompletionStatus(simple_url_loader->NetError());

  if (simple_url_loader->CompletionStatus()) {
    completion_status = simple_url_loader->CompletionStatus().value();
  }

  if (simple_url_loader->ResponseInfo() != nullptr) {
    devtools_instrumentation::OnAuctionWorkletNetworkResponseReceived(
        frame_tree_node_id, devtools_request_id, devtools_request_id,
        simple_url_loader->GetFinalURL(), *simple_url_loader->ResponseInfo());
  }

  devtools_instrumentation::OnAuctionWorkletNetworkRequestComplete(
      frame_tree_node_id, devtools_request_id, completion_status);

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

base::OnceClosure
InterestGroupManagerImpl::CreateNotifyInterestGroupAccessedCallback(
    InterestGroupObserver::AccessType type,
    const url::Origin& owner_origin,
    const std::string& name) {
  // This is only used for join/leave, so no auction ID associated.
  DCHECK(type == InterestGroupObserver::kJoin ||
         type == InterestGroupObserver::kLeave);
  return base::BindOnce(&InterestGroupManagerImpl::NotifyInterestGroupAccessed,
                        weak_factory_.GetWeakPtr(),
                        /*devtools_auction_id=*/std::nullopt, type,
                        owner_origin, name,
                        /*component_seller_origin=*/std::nullopt,
                        /*bid=*/std::nullopt, /*bid_currency=*/std::nullopt);
}

}  // namespace content
