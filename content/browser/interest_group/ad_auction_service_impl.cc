// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/interest_group/ad_auction_service_impl.h"

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/trace_event/named_trigger.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "components/aggregation_service/aggregation_coordinator_utils.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "content/browser/interest_group/ad_auction_document_data.h"
#include "content/browser/interest_group/ad_auction_page_data.h"
#include "content/browser/interest_group/ad_auction_result_metrics.h"
#include "content/browser/interest_group/auction_runner.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/browser/interest_group/interest_group_features.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/loader/reconnectable_url_loader_factory.h"
#include "content/browser/loader/url_loader_factory_utils.h"
#include "content/browser/private_aggregation/private_aggregation_manager.h"
#include "content/browser/renderer_host/page_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/cookie_deprecation_label_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/network_anonymization_key.h"
#include "net/http/http_response_headers.h"
#include "net/third_party/quiche/src/quiche/oblivious_http/oblivious_http_client.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/common/permissions_policy/policy_helper_public.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace content {

namespace {

constexpr base::TimeDelta kMaxExpiry = base::Days(30);

bool IsAdRequestValid(const blink::mojom::AdRequestConfig& config) {
  // The ad_request_url origin has to be HTTPS.
  if (config.ad_request_url.scheme() != url::kHttpsScheme) {
    return false;
  }

  // At least one adProperties is required to request potential ads.
  if (config.ad_properties.size() <= 0) {
    return false;
  }

  // If a fallback source is specified it must be HTTPS.
  if (config.fallback_source &&
      (config.fallback_source->scheme() != url::kHttpsScheme)) {
    return false;
  }

  return true;
}

// This function is used as a callback to verify
// `InterestGroup::Ad::allowed_reporting_origins` are attested. These origins
// are specified as part of the ads during `joinAdInterestGroup()` and
// `updateAdInterestGroups()`. They receive reporting beacons sent by
// `reportEvent()` when reporting to custom urls.
bool AreAllowedReportingOriginsAttested(
    BrowserContext* browser_context,
    const std::vector<url::Origin>& origins) {
  for (const auto& origin : origins) {
    if (!GetContentClient()
             ->browser()
             ->IsPrivacySandboxReportingDestinationAttested(
                 browser_context, origin,
                 PrivacySandboxInvokingAPI::kProtectedAudience)) {
      return false;
    }
  }
  return true;
}

// Returns true if changing the default permission policy for `feature` from
// EnableForAll to EnableForSelf would disable `feature` for frame, and so a
// warning should be displayed when a call relies on EnableForAll. This method
// assumes the permission is enabled with the current EnableForAll policy, so it
// only needs to check if every cross-origin frame up to the root frame allows
// `feature` for the child frame's origin.
bool ShouldWarnAboutPermissionPolicyDefault(
    RenderFrameHostImpl& frame,
    blink::mojom::PermissionsPolicyFeature feature) {
  RenderFrameHostImpl* parent = frame.GetParent();
  if (!parent) {
    return false;
  }
  const auto container_policy =
      frame.browsing_context_state()->effective_frame_policy().container_policy;
  const url::Origin& frame_origin = frame.GetLastCommittedOrigin();
  if (parent->GetLastCommittedOrigin() != frame_origin) {
    bool found_match = false;
    for (const auto& declaration : container_policy) {
      if (declaration.feature == feature) {
        auto allowlist =
            blink::PermissionsPolicy::Allowlist::FromDeclaration(declaration);
        if (!allowlist.Contains(frame_origin)) {
          return true;
        }
        found_match = true;
      }
    }
    if (!found_match) {
      return true;
    }
  }
  return ShouldWarnAboutPermissionPolicyDefault(*parent, feature);
}

void RecordBaDataConstructionResultMetric(size_t data_size,
                                          base::TimeTicks start_time) {
  // Request sizes only increase by factors of two so we only need to sample
  // the powers of two. The maximum of 1 GB size is much larger than it should
  // ever be.
  base::UmaHistogramCustomCounts(/*name=*/"Ads.InterestGroup.BaDataSize2",
                                 /*sample=*/data_size, /*min=*/1,
                                 /*exclusive_max=*/1 << 30, /*buckets=*/30);

  base::UmaHistogramTimes(/*name=*/"Ads.InterestGroup.BaDataConstructionTime2",
                          /*sample=*/base::TimeTicks::Now() - start_time);
}

}  // namespace

AdAuctionServiceImpl::BiddingAndAuctionDataConstructionState::
    BiddingAndAuctionDataConstructionState()
    : start_time(base::TimeTicks::Now()),
      request_id(base::Uuid::GenerateRandomV4()) {}
AdAuctionServiceImpl::BiddingAndAuctionDataConstructionState::
    BiddingAndAuctionDataConstructionState(
        BiddingAndAuctionDataConstructionState&& other) = default;
AdAuctionServiceImpl::BiddingAndAuctionDataConstructionState::
    ~BiddingAndAuctionDataConstructionState() = default;

// static
void AdAuctionServiceImpl::CreateMojoService(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::AdAuctionService> receiver) {
  CHECK(render_frame_host);

  // The object is bound to the lifetime of `render_frame_host` and the mojo
  // connection. See DocumentService for details.
  new AdAuctionServiceImpl(*render_frame_host, std::move(receiver));
}

void AdAuctionServiceImpl::JoinInterestGroup(
    const blink::InterestGroup& group,
    JoinInterestGroupCallback callback) {
  if (!JoinOrLeaveApiAllowedFromRenderer(group.owner, "joinAdInterestGroup")) {
    return;
  }

  // If the interest group API is not allowed for this origin, report the result
  // of the permissions check, but don't actually join the interest group.
  // The return value of IsInterestGroupAPIAllowed() is potentially affected by
  // a user's browser configuration, which shouldn't be leaked to sites to
  // protect against fingerprinting.
  bool report_result_only = !IsInterestGroupAPIAllowed(
      ContentBrowserClient::InterestGroupApiOperation::kJoin, group.owner);

  // If the group is allowed, we also do a permissions/attestation check on
  // trusted bidding signals URL, in case it's 3rd party.
  if (!report_result_only && group.trusted_bidding_signals_url.has_value() &&
      base::FeatureList::IsEnabled(
          blink::features::kFledgePermitCrossOriginTrustedSignals)) {
    url::Origin trusted_bidding_signals_origin =
        url::Origin::Create(group.trusted_bidding_signals_url.value());
    if (!trusted_bidding_signals_origin.IsSameOriginWith(group.owner) &&
        !IsInterestGroupAPIAllowed(
            ContentBrowserClient::InterestGroupApiOperation::kJoin,
            trusted_bidding_signals_origin)) {
      report_result_only = true;
      render_frame_host().AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kWarning,
          base::StringPrintf(
              "joinAdInterestGroup of interest group with owner '%s' blocked "
              "because it lacks attestation of cross-origin trusted signals "
              "origin '%s' or that origin is disallowed by user preferences",
              group.owner.Serialize().c_str(),
              trusted_bidding_signals_origin.Serialize().c_str()));
    }
  }

  blink::InterestGroup updated_group = group;
  base::Time max_expiry = base::Time::Now() + kMaxExpiry;
  if (updated_group.expiry > max_expiry) {
    updated_group.expiry = max_expiry;
  }

  if (updated_group.aggregation_coordinator_origin &&
      !aggregation_service::IsAggregationCoordinatorOriginAllowed(
          updated_group.aggregation_coordinator_origin.value())) {
    ReportBadMessageAndDeleteThis(
        "Unexpected request: aggregationCoordinatorOrigin is not supported.");
    return;
  }

  // `base::Unretained` is safe here since the `BrowserContext` owns the
  // `StoragePartition` that owns the interest group manager.
  GetInterestGroupManager().CheckPermissionsAndJoinInterestGroup(
      std::move(updated_group), main_frame_url_, origin(),
      GetFrame()->GetNetworkIsolationKey(), report_result_only,
      *GetFrameURLLoaderFactory(),
      base::BindRepeating(
          &AreAllowedReportingOriginsAttested,
          base::Unretained(render_frame_host().GetBrowserContext())),
      std::move(callback));
}

void AdAuctionServiceImpl::LeaveInterestGroup(
    const url::Origin& owner,
    const std::string& name,
    LeaveInterestGroupCallback callback) {
  if (!JoinOrLeaveApiAllowedFromRenderer(owner, "leaveAdInterestGroup")) {
    return;
  }

  // If the interest group API is not allowed for this origin, report the result
  // of the permissions check, but don't actually join the interest group.
  // The return value of IsInterestGroupAPIAllowed() is potentially affected by
  // a user's browser configuration, which shouldn't be leaked to sites to
  // protect against fingerprinting.
  bool report_result_only = !IsInterestGroupAPIAllowed(
      ContentBrowserClient::InterestGroupApiOperation::kLeave, owner);

  GetInterestGroupManager().CheckPermissionsAndLeaveInterestGroup(
      blink::InterestGroupKey(owner, name), main_frame_origin_, origin(),
      GetFrame()->GetNetworkIsolationKey(), report_result_only,
      *GetFrameURLLoaderFactory(), std::move(callback));
}

void AdAuctionServiceImpl::LeaveInterestGroupForDocument() {
  // Based on the spec, permission policy is bypassed for leaving implicit
  // interest groups.

  // If the interest group API is not allowed for this origin do nothing.
  if (!IsInterestGroupAPIAllowed(
          ContentBrowserClient::InterestGroupApiOperation::kLeave, origin())) {
    return;
  }

  if (origin().scheme() != url::kHttpsScheme) {
    ReportBadMessageAndDeleteThis(
        "Unexpected request: LeaveInterestGroupForDocument only supported for "
        "secure origins");
    return;
  }

  // Get interest group owner and name from the ad auction data, which is part
  // of the fenced frame properties. Here the fenced frame properties are
  // obtained from the closest ancestor that has valid fenced frame properties.
  // This is because both top-level ads and ad components may have ad auction
  // data.
  const std::optional<FencedFrameProperties>& fenced_frame_properties =
      GetFrame()->frame_tree_node()->GetFencedFrameProperties(
          FencedFramePropertiesNodeSource::kClosestAncestor);

  // This frame is neither a fenced frame or an urn iframe itself, nor it is
  // nested within a fenced frame or an urn iframe.
  if (!fenced_frame_properties.has_value()) {
    devtools_instrumentation::LogWorkletMessage(
        *GetFrame(), blink::mojom::ConsoleMessageLevel::kError,
        "Owner and name are required to call LeaveAdInterestGroup outside of "
        "a fenced frame or an opaque origin iframe.");
    return;
  }

  if (!fenced_frame_properties->ad_auction_data().has_value()) {
    return;
  }

  const blink::FencedFrame::AdAuctionData& auction_data =
      fenced_frame_properties->ad_auction_data()->GetValueIgnoringVisibility();

  if (auction_data.interest_group_owner != origin()) {
    // The ad page calling LeaveAdInterestGroup is not the owner of the group.
    return;
  }

  GetInterestGroupManager().LeaveInterestGroup(
      blink::InterestGroupKey(auction_data.interest_group_owner,
                              auction_data.interest_group_name),
      main_frame_origin_);
}

void AdAuctionServiceImpl::ClearOriginJoinedInterestGroups(
    const url::Origin& owner,
    const std::vector<std::string>& interest_groups_to_keep,
    ClearOriginJoinedInterestGroupsCallback callback) {
  if (!JoinOrLeaveApiAllowedFromRenderer(owner,
                                         "clearOriginJoinedAdInterestGroups")) {
    return;
  }

  // If the interest group leave API is not allowed for this origin, report the
  // result of the permissions check, but don't actually join the interest
  // group. The return value of IsInterestGroupAPIAllowed() is potentially
  // affected by a user's browser configuration, which shouldn't be leaked to
  // sites to protect against fingerprinting.
  bool report_result_only = !IsInterestGroupAPIAllowed(
      ContentBrowserClient::InterestGroupApiOperation::kLeave, owner);

  GetInterestGroupManager().CheckPermissionsAndClearOriginJoinedInterestGroups(
      owner, interest_groups_to_keep, main_frame_origin_, origin(),
      GetFrame()->GetNetworkIsolationKey(), report_result_only,
      *GetFrameURLLoaderFactory(), std::move(callback));
}

void AdAuctionServiceImpl::UpdateAdInterestGroups() {
  // If the interest group API is not allowed for this context by Permissions
  // Policy, do nothing
  if (!IsPermissionPolicyEnabledAndWarnIfNeeded(
          blink::mojom::PermissionsPolicyFeature::kJoinAdInterestGroup,
          "updateAdInterestGroups")) {
    ReportBadMessageAndDeleteThis("Unexpected request");
    return;
  }
  // If the interest group API is not allowed for this origin do nothing.
  if (!IsInterestGroupAPIAllowed(
          ContentBrowserClient::InterestGroupApiOperation::kUpdate, origin())) {
    return;
  }

  // `base::Unretained` is safe here since the `BrowserContext` owns the
  // `StoragePartition` that owns the interest group manager.
  GetInterestGroupManager().UpdateInterestGroupsOfOwner(
      origin(), GetClientSecurityState(),
      base::BindRepeating(
          &AreAllowedReportingOriginsAttested,
          base::Unretained(render_frame_host().GetBrowserContext())));
}

void AdAuctionServiceImpl::RunAdAuction(
    const blink::AuctionConfig& config,
    mojo::PendingReceiver<blink::mojom::AbortableAdAuction> abort_receiver,
    RunAdAuctionCallback callback) {
  // Ensure the page is not in prerendering as code belows expect it, i.e.
  // GetPageUkmSourceId() doesn't work with prerendering pages.
  CHECK(!render_frame_host().IsInLifecycleState(
      RenderFrameHost::LifecycleState::kPrerendering));

  // If the run ad auction API is not allowed for this context by Permissions
  // Policy, do nothing.
  if (!IsPermissionPolicyEnabledAndWarnIfNeeded(
          blink::mojom::PermissionsPolicyFeature::kRunAdAuction,
          "runAdAuction")) {
    ReportBadMessageAndDeleteThis("Unexpected request");
    return;
  }

  // The `PageImpl` recorded at the construction of the AdAuctionServiceImpl has
  // been invalidated or the current frame's `PageImpl` has changed to a
  // different one, abort the auction.
  // See crbug.com/1422301.
  if (base::FeatureList::IsEnabled(features::kDetectInconsistentPageImpl) &&
      (!GetFrame()->auction_initiator_page() ||
       GetFrame()->auction_initiator_page().get() !=
           &(GetFrame()->GetPage()))) {
    std::move(callback).Run(/*aborted_by_script=*/false,
                            /*config=*/std::nullopt);
    return;
  }

  auto* auction_result_metrics =
      AdAuctionResultMetrics::GetOrCreateForPage(render_frame_host().GetPage());
  if (!auction_result_metrics->ShouldRunAuction()) {
    std::move(callback).Run(/*aborted_by_script=*/false,
                            /*config=*/std::nullopt);
    return;
  }

  FencedFrameURLMapping& fenced_frame_urls_map =
      GetFrame()->GetPage().fenced_frame_urls_map();
  auto urn_uuid = fenced_frame_urls_map.GeneratePendingMappedURN();

  // If pending mapped URN cannot be generated due to number of mappings has
  // reached limit, stop the auction.
  if (!urn_uuid.has_value()) {
    std::move(callback).Run(/*aborted_by_script=*/false,
                            /*config=*/std::nullopt);
    return;
  }

  // Using Unretained here since `this` owns the AuctionRunner.
  AuctionRunner::AdAuctionPageDataCallback ad_auction_page_data_callback =
      base::BindRepeating(&AdAuctionServiceImpl::GetAdAuctionPageData,
                          base::Unretained(this));

  base::trace_event::EmitNamedTrigger("fledge-top-level-auction-start");

  // Try to preconnect to owner and bidding signals origins if this is an
  // on-device auction.
  if (base::FeatureList::IsEnabled(features::kFledgeUsePreconnectCache) &&
      !config.server_response.has_value()) {
    size_t n_owners_cached = PreconnectToBuyerOrigins(config);
    for (const blink::AuctionConfig& component_config :
         config.non_shared_params.component_auctions) {
      if (!component_config.server_response.has_value()) {
        n_owners_cached += PreconnectToBuyerOrigins(component_config);
      }
    }
    base::UmaHistogramCounts100(
        "Ads.InterestGroup.Auction.NumOwnerOriginsCachedForPreconnect",
        n_owners_cached);
  }

  std::unique_ptr<AuctionRunner> auction = AuctionRunner::CreateAndStart(
      auction_metrics_recorder_manager_.CreateAuctionMetricsRecorder(),
      &auction_worklet_manager_, &auction_nonce_manager_,
      &GetInterestGroupManager(), render_frame_host().GetBrowserContext(),
      private_aggregation_manager_, std::move(ad_auction_page_data_callback),
      // Unlike other callbacks, this needs to be safe to call after destruction
      // of the AdAuctionServiceImpl, so that the reporter can outlive it.
      base::BindRepeating(
          &AdAuctionServiceImpl::MaybeLogPrivateAggregationFeatures,
          weak_ptr_factory_.GetWeakPtr()),
      config, main_frame_origin_, origin(), GetClientSecurityState(),
      GetRefCountedTrustedURLLoaderFactory(),
      base::BindRepeating(&AdAuctionServiceImpl::IsInterestGroupAPIAllowed,
                          base::Unretained(this)),
      base::BindRepeating(
          &AreAllowedReportingOriginsAttested,
          base::Unretained(render_frame_host().GetBrowserContext())),
      std::move(abort_receiver),
      base::BindOnce(&AdAuctionServiceImpl::OnAuctionComplete,
                     base::Unretained(this), std::move(callback),
                     std::move(urn_uuid.value())));
  AuctionRunner* raw_auction = auction.get();
  auctions_.emplace(raw_auction, std::move(auction));
}

namespace {

// Helper class to retrieve the URL that a given URN is mapped to.
class FencedFrameURLMappingObserver
    : public FencedFrameURLMapping::MappingResultObserver {
 public:
  // Retrieves the URL that `urn_url` is mapped to, if any. If `send_reports` is
  // true, sends the reports associated with `urn_url`, if there are any.
  static std::optional<GURL> GetURL(RenderFrameHostImpl& render_frame_host,
                                    const GURL& urn_url,
                                    bool send_reports) {
    std::optional<GURL> mapped_url;
    FencedFrameURLMappingObserver obs(&mapped_url, send_reports);
    content::FencedFrameURLMapping& mapping =
        render_frame_host.GetPage().fenced_frame_urls_map();
    // FLEDGE URN URLs should already be mapped, so the observer will be called
    // synchronously.
    mapping.ConvertFencedFrameURNToURL(urn_url, &obs);
    if (!obs.called_) {
      mapping.RemoveObserverForURN(urn_url, &obs);
    }
    return mapped_url;
  }

 private:
  FencedFrameURLMappingObserver(std::optional<GURL>* mapped_url,
                                bool send_reports)
      : mapped_url_(mapped_url), send_reports_(send_reports) {}

  ~FencedFrameURLMappingObserver() override = default;

  void OnFencedFrameURLMappingComplete(
      const std::optional<FencedFrameProperties>& properties) override {
    if (properties) {
      if (properties->mapped_url()) {
        *mapped_url_ = properties->mapped_url()->GetValueIgnoringVisibility();
      }
      if (send_reports_ && properties->on_navigate_callback()) {
        properties->on_navigate_callback().Run();
      }
    }
    called_ = true;
  }

  bool called_ = false;
  raw_ptr<std::optional<GURL>> mapped_url_;
  bool send_reports_;
};

}  // namespace

void AdAuctionServiceImpl::DeprecatedGetURLFromURN(
    const GURL& urn_url,
    bool send_reports,
    DeprecatedGetURLFromURNCallback callback) {
  if (!blink::IsValidUrnUuidURL(urn_url)) {
    ReportBadMessageAndDeleteThis("Unexpected request: invalid URN");
    return;
  }

  std::move(callback).Run(FencedFrameURLMappingObserver::GetURL(
      static_cast<RenderFrameHostImpl&>(render_frame_host()), urn_url,
      send_reports));
}

void AdAuctionServiceImpl::DeprecatedReplaceInURN(
    const GURL& urn_url,
    const std::vector<blink::AuctionConfig::AdKeywordReplacement>& replacements,
    DeprecatedReplaceInURNCallback callback) {
  if (!blink::IsValidUrnUuidURL(urn_url)) {
    ReportBadMessageAndDeleteThis("Unexpected request: invalid URN");
    return;
  }
  std::vector<std::pair<std::string, std::string>> local_replacements;
  for (const auto& replacement : replacements) {
    local_replacements.emplace_back(std::move(replacement.match),
                                    std::move(replacement.replacement));
  }
  content::FencedFrameURLMapping& mapping =
      static_cast<RenderFrameHostImpl&>(render_frame_host())
          .GetPage()
          .fenced_frame_urls_map();
  mapping.SubstituteMappedURL(urn_url, local_replacements);
  std::move(callback).Run();
}

void AdAuctionServiceImpl::GetInterestGroupAdAuctionData(
    const url::Origin& seller,
    const std::optional<url::Origin>& coordinator,
    blink::mojom::AuctionDataConfigPtr config,
    GetInterestGroupAdAuctionDataCallback callback) {
  if (seller.scheme() != url::kHttpsScheme) {
    ReportBadMessageAndDeleteThis("Invalid Seller");
    return;
  }
  if (coordinator && coordinator->scheme() != url::kHttpsScheme) {
    ReportBadMessageAndDeleteThis("Invalid Bidding and Auction Coordinator");
    return;
  }

  if (!config->per_buyer_configs.empty() && !config->request_size) {
    ReportBadMessageAndDeleteThis(
        "Invalid AuctionDataConfig: Missing request_size");
    return;
  }

  for (const auto& per_buyer_config : config->per_buyer_configs) {
    if (per_buyer_config.first.scheme() != url::kHttpsScheme) {
      ReportBadMessageAndDeleteThis("Invalid Buyer");
      return;
    }
  }

  if (!IsPermissionPolicyEnabledAndWarnIfNeeded(
          blink::mojom::PermissionsPolicyFeature::kRunAdAuction,
          "getInterestGroupAdAuctionData")) {
    ReportBadMessageAndDeleteThis("Unexpected request");
    return;
  }

  // If the interest group API is not allowed for this origin do nothing.
  bool api_allowed = IsInterestGroupAPIAllowed(
      ContentBrowserClient::InterestGroupApiOperation::kSell, seller);
  base::UmaHistogramBoolean(
      "Ads.InterestGroup.ServerAuction.Request.APIAllowed", api_allowed);
  if (!api_allowed) {
    std::move(callback).Run({}, {}, "API not allowed for this origin");
    return;
  }

  base::trace_event::EmitNamedTrigger(
      "fledge-get-interest-group-ad-auction-data");

  BiddingAndAuctionDataConstructionState state;
  state.callback = std::move(callback);
  state.seller = seller;
  state.coordinator = coordinator;
  state.timestamp = base::Time::Now();
  state.config = std::move(config);

  ba_data_callbacks_.push(std::move(state));
  // Only start this request if there isn't another request pending.
  if (ba_data_callbacks_.size() == 1) {
    LoadAuctionDataAndKeyForNextQueuedRequest();
  }
}

void AdAuctionServiceImpl::CreateAdRequest(
    blink::mojom::AdRequestConfigPtr config,
    CreateAdRequestCallback callback) {
  if (!IsAdRequestValid(*config)) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // TODO(crbug.com/40197508): Actually request Ads and return a guid.
  // For now just act like it failed.
  std::move(callback).Run(std::nullopt);
}

void AdAuctionServiceImpl::FinalizeAd(const std::string& ads_guid,
                                      const blink::AuctionConfig& config,
                                      FinalizeAdCallback callback) {
  if (ads_guid.empty()) {
    ReportBadMessageAndDeleteThis("GUID empty");
    return;
  }

  // TODO(crbug.com/40197508): Actually finalize Ad and return an URL.
  // For now just act like it failed.
  std::move(callback).Run(std::nullopt);
}

network::mojom::URLLoaderFactory*
AdAuctionServiceImpl::GetFrameURLLoaderFactory() {
  if (!frame_url_loader_factory_ || !frame_url_loader_factory_.is_connected()) {
    frame_url_loader_factory_.reset();
    render_frame_host().CreateNetworkServiceDefaultFactory(
        frame_url_loader_factory_.BindNewPipeAndPassReceiver());
  }
  return frame_url_loader_factory_.get();
}

network::mojom::URLLoaderFactory*
AdAuctionServiceImpl::GetTrustedURLLoaderFactory() {
  // Ensure the page is not in prerendering as code belows expect it, i.e.
  // GetPageUkmSourceId() doesn't work with prerendering pages.
  CHECK(!render_frame_host().IsInLifecycleState(
      RenderFrameHost::LifecycleState::kPrerendering));
  return ref_counted_trusted_url_loader_factory_.get();
}

void AdAuctionServiceImpl::CreateUnderlyingTrustedURLLoaderFactory(
    mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory) {
  // TODO(mmenke): Should this have its own URLLoaderFactoryType? FLEDGE
  // requests are very different from subresource requests.
  //
  // TODO(mmenke): Hook up devtools.
  *out_factory = url_loader_factory::CreatePendingRemote(
      ContentBrowserClient::URLLoaderFactoryType::kDocumentSubResource,
      url_loader_factory::TerminalParams::ForBrowserProcess(
          static_cast<RenderFrameHostImpl&>(render_frame_host())
              .GetStoragePartition()),
      url_loader_factory::ContentClientParams(
          render_frame_host().GetSiteInstance()->GetBrowserContext(),
          &render_frame_host(), render_frame_host().GetProcess()->GetID(),
          url::Origin(), net::IsolationInfo(),
          ukm::SourceIdObj::FromInt64(
              render_frame_host().GetPageUkmSourceId())));
}

void AdAuctionServiceImpl::PreconnectSocket(
    const GURL& url,
    const net::NetworkAnonymizationKey& network_anonymization_key) {
  render_frame_host()
      .GetStoragePartition()
      ->GetNetworkContext()
      ->PreconnectSockets(/*num_streams=*/1, url,
                          network::mojom::CredentialsMode::kOmit,
                          network_anonymization_key);
}

scoped_refptr<network::SharedURLLoaderFactory>
AdAuctionServiceImpl::GetRefCountedTrustedURLLoaderFactory() {
  return ref_counted_trusted_url_loader_factory_;
}

RenderFrameHostImpl* AdAuctionServiceImpl::GetFrame() {
  return static_cast<RenderFrameHostImpl*>(&render_frame_host());
}

scoped_refptr<SiteInstance> AdAuctionServiceImpl::GetFrameSiteInstance() {
  return render_frame_host().GetSiteInstance();
}

network::mojom::ClientSecurityStatePtr
AdAuctionServiceImpl::GetClientSecurityState() {
  return GetFrame()->BuildClientSecurityState();
}

std::optional<std::string> AdAuctionServiceImpl::GetCookieDeprecationLabel() {
  if (!base::FeatureList::IsEnabled(
          features::kFledgeFacilitatedTestingSignalsHeaders)) {
    return std::nullopt;
  }

  CookieDeprecationLabelManager* cdlm =
      render_frame_host()
          .GetStoragePartition()
          ->GetCookieDeprecationLabelManager();
  if (cdlm) {
    return cdlm->GetValue();
  } else {
    return std::nullopt;
  }
}

void AdAuctionServiceImpl::GetBiddingAndAuctionServerKey(
    const std::optional<url::Origin>& coordinator,
    base::OnceCallback<void(
        base::expected<BiddingAndAuctionServerKey, std::string>)> callback) {
  GetInterestGroupManager().GetBiddingAndAuctionServerKey(
      std::move(coordinator), std::move(callback));
}

AdAuctionServiceImpl::AdAuctionServiceImpl(
    RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::AdAuctionService> receiver)
    : DocumentService(render_frame_host, std::move(receiver)),
      main_frame_origin_(
          render_frame_host.GetMainFrame()->GetLastCommittedOrigin()),
      main_frame_url_(render_frame_host.GetMainFrame()->GetLastCommittedURL()),
      auction_metrics_recorder_manager_(render_frame_host.GetPageUkmSourceId()),
      auction_worklet_manager_(
          &GetInterestGroupManager().auction_process_manager(),
          GetTopWindowOrigin(),
          origin(),
          this),
      auction_nonce_manager_(GetFrame()),
      private_aggregation_manager_(PrivateAggregationManager::GetManager(
          *render_frame_host.GetBrowserContext())) {
  // Construct `ref_counted_trusted_url_loader_factory_` here because
  // `weak_ptr_factory_` is not yet initialized during the member initializer
  // list above.
  ref_counted_trusted_url_loader_factory_ =
      base::MakeRefCounted<ReconnectableURLLoaderFactory>(base::BindRepeating(
          &AdAuctionServiceImpl::CreateUnderlyingTrustedURLLoaderFactory,
          weak_ptr_factory_.GetWeakPtr()));

  // Throughout the auction, the `PageImpl` of the frame which initiates the
  // auction should stay the same. When an inconsistency is detected, the
  // auction must be aborted. This is done by storing a weak pointer to the
  // `PageImpl`. It is verified at various stages of the auction.
  //
  // Note: `AdAuctionServiceImpl` is constructed upon the first call of a
  // Protected Audience API. This is why the weak pointer is set here instead of
  // during frame's `RenderFrameHostImpl` construction.
  //
  // See crbug.com/1422301 for a scenario where `PageImpl` can change, and why
  // this is problematic.
  //
  // TODO(crbug.com/40615943): Once RenderDocument is launched, the `PageImpl`
  // will not change. Remove all logics around this weak pointer.
  GetFrame()->set_auction_initiator_page(
      static_cast<PageImpl&>(render_frame_host.GetPage()).GetWeakPtrImpl());
}

AdAuctionServiceImpl::~AdAuctionServiceImpl() {
  while (!auctions_.empty()) {
    // Need to fail all auctions rather than just deleting them, to ensure Mojo
    // callbacks from the renderers are invoked. Uninvoked Mojo callbacks may
    // not be destroyed before the Mojo pipe is, and the parent DocumentService
    // class owns the pipe, so it may still be open at this point.
    auctions_.begin()->first->FailAuction(/*aborted_by_script=*/false);
  }
}

bool AdAuctionServiceImpl::JoinOrLeaveApiAllowedFromRenderer(
    const url::Origin& owner,
    const char* invoked_method) {
  if (origin().scheme() != url::kHttpsScheme) {
    ReportBadMessageAndDeleteThis(
        "Unexpected request: Interest groups may only be joined or left from "
        "https origins");
    return false;
  }

  if (owner.scheme() != url::kHttpsScheme) {
    ReportBadMessageAndDeleteThis(
        "Unexpected request: Interest groups may only be owned by https "
        "origins");
    return false;
  }

  // If the interest group API is not allowed for this context by Permissions
  // Policy, do nothing.
  if (!IsPermissionPolicyEnabledAndWarnIfNeeded(
          blink::mojom::PermissionsPolicyFeature::kJoinAdInterestGroup,
          invoked_method)) {
    ReportBadMessageAndDeleteThis(
        "Unexpected request: Interest groups may only be joined or left when "
        "feature join-ad-interest-group is enabled by Permissions Policy");
    return false;
  }

  return true;
}

bool AdAuctionServiceImpl::IsPermissionPolicyEnabledAndWarnIfNeeded(
    blink::mojom::PermissionsPolicyFeature feature,
    const char* invoked_method) {
  if (!render_frame_host().IsFeatureEnabled(feature)) {
    return false;
  }

  auto warn_it = should_warn_about_feature_.find(feature);
  if (warn_it == should_warn_about_feature_.end()) {
    bool should_warn =
        ShouldWarnAboutPermissionPolicyDefault(*GetFrame(), feature);
    warn_it =
        should_warn_about_feature_.emplace(std::pair(feature, should_warn))
            .first;
  }

  if (warn_it->second) {
    auto feature_it =
        blink::GetPermissionsPolicyFeatureToNameMap().find(feature);
    CHECK(feature_it != blink::GetPermissionsPolicyFeatureToNameMap().end());

    render_frame_host().AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning,
        base::StringPrintf(
            "In the future, Permissions Policy feature %s will not be enabled "
            "by default in cross-origin iframes or same-origin iframes nested "
            "in cross-origin iframes. Calling %s will be rejected with "
            "NotAllowedError if it is not explicitly enabled",
            feature_it->second.data(), invoked_method));
  }

  return true;
}

bool AdAuctionServiceImpl::IsInterestGroupAPIAllowed(
    ContentBrowserClient::InterestGroupApiOperation
        interest_group_api_operation,
    const url::Origin& origin) const {
  return GetContentClient()->browser()->IsInterestGroupAPIAllowed(
      &render_frame_host(), interest_group_api_operation, main_frame_origin_,
      origin);
}

void AdAuctionServiceImpl::OnAuctionComplete(
    RunAdAuctionCallback callback,
    GURL urn_uuid,
    AuctionRunner* auction,
    bool aborted_by_script,
    std::optional<blink::InterestGroupKey> winning_group_key,
    std::optional<blink::AdSize> requested_ad_size,
    std::optional<blink::AdDescriptor> ad_descriptor,
    std::vector<blink::AdDescriptor> ad_component_descriptors,
    std::vector<std::string> errors,
    std::unique_ptr<InterestGroupAuctionReporter> reporter,
    bool contained_server_auction,
    bool contained_on_device_auction,
    AuctionResult result) {
  // Remove `auction` from `auctions_` but temporarily keep it alive - on
  // success, it owns a AuctionWorkletManager::WorkletHandle for the top-level
  // auction, which `reporter` can reuse once started. Fine to delete after
  // starting the reporter.
  auto auction_it = auctions_.find(auction);
  CHECK(auction_it != auctions_.end(), base::NotFatalUntil::M130);
  std::unique_ptr<AuctionRunner> owned_auction = std::move(auction_it->second);
  auctions_.erase(auction_it);

  // The `PageImpl` recorded at the construction of the AdAuctionServiceImpl has
  // been invalidated or the current frame's `PageImpl` has changed to a
  // different one, abort the auction.
  // See crbug.com/1422301.
  if (base::FeatureList::IsEnabled(features::kDetectInconsistentPageImpl) &&
      (!GetFrame()->auction_initiator_page() ||
       GetFrame()->auction_initiator_page().get() !=
           &(GetFrame()->GetPage()))) {
    std::move(callback).Run(aborted_by_script, /*config=*/std::nullopt);
    return;
  }

  // Forward debug information to devtools.
  for (const std::string& error : errors) {
    devtools_instrumentation::LogWorkletMessage(
        *GetFrame(), blink::mojom::ConsoleMessageLevel::kError,
        base::StrCat({"Worklet error: ", error}));
  }

  auto* auction_result_metrics =
      AdAuctionResultMetrics::GetForPage(render_frame_host().GetPage());

  if (!ad_descriptor) {
    DCHECK(!reporter);

    GetContentClient()->browser()->OnAuctionComplete(
        &render_frame_host(), /*winner_data_key=*/std::nullopt,
        contained_server_auction, contained_on_device_auction, result);

    std::move(callback).Run(aborted_by_script, /*config=*/std::nullopt);
    if (auction_result_metrics) {
      // `auction_result_metrics` can be null since PageUserData like
      // AdAuctionResultMetrics isn't guaranteed to be destroyed after document
      // services like `this`, even though this typically is the case for
      // destruction of the RenderFrameHost (except for renderer crashes).
      //
      // So, we need to guard against this.
      auction_result_metrics->ReportAuctionResult(
          AdAuctionResultMetrics::AuctionResult::kFailed);
    }
    return;
  }

  DCHECK(reporter);
  // Should always be present with an ad_descriptor->url.
  DCHECK(winning_group_key);
  DCHECK(blink::IsValidFencedFrameURL(ad_descriptor->url));
  DCHECK(urn_uuid.is_valid());

  GetContentClient()->browser()->OnAuctionComplete(
      &render_frame_host(),
      InterestGroupManager::InterestGroupDataKey{
          reporter->winning_bid_info()
              .storage_interest_group->interest_group.owner,
          reporter->winning_bid_info().storage_interest_group->joining_origin,
      },
      contained_server_auction, contained_on_device_auction, result);

  content::AdAuctionData ad_auction_data{winning_group_key->owner,
                                         winning_group_key->name};
  FencedFrameURLMapping& current_fenced_frame_urls_map =
      GetFrame()->GetPage().fenced_frame_urls_map();

  // Set up reporting for any fenced frame that's navigated to the winning bid's
  // URL. Use a URLLoaderFactory that will automatically reconnect on network
  // process crashes, and can outlive the frame.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      render_frame_host()
          .GetStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess();

  blink::FencedFrame::RedactedFencedFrameConfig config =
      current_fenced_frame_urls_map.AssignFencedFrameURLAndInterestGroupInfo(
          urn_uuid, requested_ad_size, *ad_descriptor,
          std::move(ad_auction_data),
          reporter->OnNavigateToWinningAdCallback(
              GetFrame()->GetFrameTreeNodeId()),
          ad_component_descriptors, reporter->fenced_frame_reporter());
  std::move(callback).Run(/*aborted_by_script=*/false, std::move(config));

  // Start the InterestGroupAuctionReporter. It will run reporting scripts, but
  // nothing will be reported (nor the reporter deleted) until a fenced frame
  // navigates to the winning ad, which will be signalled by invoking the
  // callback returned by the InterestGroupAuctionReporter's
  // OnNavitationToWinningAdCallback() method (invoked just above).
  reporters_.emplace_front(std::move(reporter));
  reporters_.front()->Start(
      base::BindOnce(&AdAuctionServiceImpl::OnReporterComplete,
                     base::Unretained(this), reporters_.begin()));
  if (auction_result_metrics) {
    auction_result_metrics->ReportAuctionResult(
        AdAuctionResultMetrics::AuctionResult::kSucceeded);
  }
}

void AdAuctionServiceImpl::OnReporterComplete(
    ReporterList::iterator reporter_it) {
  // Forward debug information to devtools.
  //
  // TODO(crbug.com/40248758): Ideally this will share code with the
  // handling of the errors from the earlier phases of the auction.
  InterestGroupAuctionReporter* reporter = reporter_it->get();
  for (const std::string& error : reporter->errors()) {
    devtools_instrumentation::LogWorkletMessage(
        *GetFrame(), blink::mojom::ConsoleMessageLevel::kError,
        base::StrCat({"Worklet error: ", error}));
  }

  reporters_.erase(reporter_it);
}

void AdAuctionServiceImpl::MaybeLogPrivateAggregationFeatures(
    const std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>&
        private_aggregation_requests) {
  // TODO(crbug.com/40236382): Improve coverage of these use counters, i.e.
  // for API usage that does not result in a successful request.
  if (private_aggregation_requests.empty()) {
    return;
  }

  if (!has_logged_private_aggregation_filtering_id_web_feature_ &&
      base::ranges::any_of(
          private_aggregation_requests, [](const auto& request) {
            auction_worklet::mojom::AggregatableReportContributionPtr&
                contribution = request->contribution;
            if (contribution->is_histogram_contribution()) {
              return contribution->get_histogram_contribution()
                  ->filtering_id.has_value();
            }
            CHECK(contribution->is_for_event_contribution());
            return contribution->get_for_event_contribution()
                ->filtering_id.has_value();
          })) {
    has_logged_private_aggregation_filtering_id_web_feature_ = true;
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        &render_frame_host(),
        blink::mojom::WebFeature::kPrivateAggregationApiFilteringIds);
  }

  if (!has_logged_private_aggregation_enable_debug_mode_web_feature_ &&
      base::ranges::any_of(private_aggregation_requests,
                           [](const auto& request) {
                             return request->debug_mode_details->is_enabled;
                           })) {
    has_logged_private_aggregation_enable_debug_mode_web_feature_ = true;
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        &render_frame_host(),
        blink::mojom::WebFeature::kPrivateAggregationApiEnableDebugMode);
  }

  if (!has_logged_extended_private_aggregation_web_feature_ &&
      base::ranges::any_of(
          private_aggregation_requests, [](const auto& request) {
            return request->contribution->is_for_event_contribution();
          })) {
    has_logged_extended_private_aggregation_web_feature_ = true;
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        &render_frame_host(),
        blink::mojom::WebFeature::kPrivateAggregationApiFledgeExtensions);
  }

  if (!has_logged_private_aggregation_web_features_) {
    has_logged_private_aggregation_web_features_ = true;
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        &render_frame_host(),
        blink::mojom::WebFeature::kPrivateAggregationApiAll);
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        &render_frame_host(),
        blink::mojom::WebFeature::kPrivateAggregationApiFledge);
  }
}

void AdAuctionServiceImpl::ReturnEmptyGetInterestGroupAdAuctionDataCallback(
    const std::string& msg) {
  if (!ba_data_callbacks_.empty()) {
    BiddingAndAuctionDataConstructionState& state = ba_data_callbacks_.front();

    if (msg.empty()) {
      RecordBaDataConstructionResultMetric(/*data_size=*/0, state.start_time);
    }

    std::move(state.callback).Run({}, {}, msg);
    ba_data_callbacks_.pop();
  }
  if (!ba_data_callbacks_.empty()) {
    LoadAuctionDataAndKeyForNextQueuedRequest();
  }
}

void AdAuctionServiceImpl::LoadAuctionDataAndKeyForNextQueuedRequest() {
  BiddingAndAuctionDataConstructionState& state = ba_data_callbacks_.front();

  GetInterestGroupManager().GetInterestGroupAdAuctionData(
      GetTopWindowOrigin(),
      /* generation_id=*/base::Uuid::GenerateRandomV4(), state.timestamp,
      std::move(state.config),
      base::BindOnce(&AdAuctionServiceImpl::OnGotAuctionData,
                     weak_ptr_factory_.GetWeakPtr(), state.request_id));

  // GetBiddingAndAuctionServerKey can call its callback synchronously, so we
  // need to call it last in case it invalidates `state`.
  GetInterestGroupManager().GetBiddingAndAuctionServerKey(
      state.coordinator,
      base::BindOnce(&AdAuctionServiceImpl::OnGotBiddingAndAuctionServerKey,
                     weak_ptr_factory_.GetWeakPtr(), state.request_id));
}

void AdAuctionServiceImpl::OnGotAuctionData(base::Uuid request_id,
                                            BiddingAndAuctionData data) {
  if (ba_data_callbacks_.empty() ||
      request_id != ba_data_callbacks_.front().request_id) {
    return;
  }
  BiddingAndAuctionDataConstructionState& state = ba_data_callbacks_.front();
  state.data = std::make_unique<BiddingAndAuctionData>(std::move(data));
  if (state.key) {
    OnGotAuctionDataAndKey(request_id);
  }
}

void AdAuctionServiceImpl::OnGotBiddingAndAuctionServerKey(
    base::Uuid request_id,
    base::expected<BiddingAndAuctionServerKey, std::string> maybe_key) {
  if (ba_data_callbacks_.empty() ||
      request_id != ba_data_callbacks_.front().request_id) {
    return;
  }
  if (!maybe_key.has_value()) {
    ReturnEmptyGetInterestGroupAdAuctionDataCallback(maybe_key.error());
    return;
  }
  BiddingAndAuctionDataConstructionState& state = ba_data_callbacks_.front();
  state.key =
      std::make_unique<BiddingAndAuctionServerKey>(std::move(*maybe_key));

  if (state.data) {
    OnGotAuctionDataAndKey(request_id);
  }
}

void AdAuctionServiceImpl::OnGotAuctionDataAndKey(base::Uuid request_id) {
  if (ba_data_callbacks_.empty() ||
      request_id != ba_data_callbacks_.front().request_id) {
    return;
  }

  BiddingAndAuctionDataConstructionState& state = ba_data_callbacks_.front();
  DCHECK(state.data);
  DCHECK(state.key);

  if (state.data->request.empty()) {
    ReturnEmptyGetInterestGroupAdAuctionDataCallback("");
    return;
  }

  auto maybe_key_config = quiche::ObliviousHttpHeaderKeyConfig::Create(
      state.key->id, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256,
      EVP_HPKE_AES_256_GCM);
  CHECK(maybe_key_config.ok()) << maybe_key_config.status();

  const bool use_new_format =
      base::FeatureList::IsEnabled(kBiddingAndAuctionEncryptionMediaType);

  auto maybe_request =
      quiche::ObliviousHttpRequest::CreateClientObliviousRequest(
          std::string(state.data->request.begin(), state.data->request.end()),
          state.key->key, maybe_key_config.value(),
          use_new_format
              ? kBiddingAndAuctionEncryptionRequestMediaType
              : quiche::ObliviousHttpHeaderKeyConfig::kOhttpRequestLabel);
  if (!maybe_request.ok()) {
    ReturnEmptyGetInterestGroupAdAuctionDataCallback(
        "Could not create request");
    return;
  }

  std::string data = maybe_request->EncapsulateAndSerialize();

  // Preconnect to seller since we know JS will send a request there.
  render_frame_host()
      .GetStoragePartition()
      ->GetNetworkContext()
      ->PreconnectSockets(
          /*num_streams=*/1, state.seller.GetURL(),
          network::mojom::CredentialsMode::kInclude,
          render_frame_host()
              .GetIsolationInfoForSubresources()
              .network_anonymization_key());

  AdAuctionPageData* ad_auction_page_data = GetAdAuctionPageData();
  if (!ad_auction_page_data) {
    ReturnEmptyGetInterestGroupAdAuctionDataCallback(
        "Page destruction in progress");
    return;
  }

  AdAuctionRequestContext context(
      state.seller, std::move(state.data->group_names),
      std::move(*maybe_request).ReleaseContext(), state.start_time,
      std::move(state.data->group_pagg_coordinators));
  ad_auction_page_data->RegisterAdAuctionRequestContext(state.request_id,
                                                        std::move(context));
  // Pre-warm data decoder.
  ad_auction_page_data->GetDecoderFor(state.seller)->GetService();

  size_t start_offset = 0;
  if (use_new_format) {
    // For the modified request format we need to prepend a version number byte
    // to the request.
    start_offset = 1;
  }
  mojo_base::BigBuffer buf(data.size() + start_offset);

  // Write the version byte. If we are not using a modified request this will
  // be immediately overwritten.
  buf.data()[0] = 0;

  // Write the request starting at `start_offset`
  CHECK_EQ(data.size() + start_offset, buf.size());
  std::memcpy(&buf.data()[start_offset], data.data(), data.size());

  std::move(state.callback).Run(std::move(buf), state.request_id, "");

  RecordBaDataConstructionResultMetric(data.size(), state.start_time);

  ba_data_callbacks_.pop();
  if (!ba_data_callbacks_.empty()) {
    LoadAuctionDataAndKeyForNextQueuedRequest();
  }
}

InterestGroupManagerImpl& AdAuctionServiceImpl::GetInterestGroupManager()
    const {
  return *static_cast<InterestGroupManagerImpl*>(
      render_frame_host().GetStoragePartition()->GetInterestGroupManager());
}

url::Origin AdAuctionServiceImpl::GetTopWindowOrigin() const {
  if (!render_frame_host().GetParent()) {
    return origin();
  }
  return render_frame_host().GetMainFrame()->GetLastCommittedOrigin();
}

AdAuctionPageData* AdAuctionServiceImpl::GetAdAuctionPageData() {
  // The `PageImpl` recorded at the construction of the AdAuctionServiceImpl has
  // been invalidated or the current frame's `PageImpl` has changed to a
  // different one, signal that state is no longer available.
  // See crbug.com/1422301.
  if (base::FeatureList::IsEnabled(features::kDetectInconsistentPageImpl) &&
      (!GetFrame()->auction_initiator_page() ||
       GetFrame()->auction_initiator_page().get() !=
           &(GetFrame()->GetPage()))) {
    return nullptr;
  }

  return PageUserData<AdAuctionPageData>::GetOrCreateForPage(
      render_frame_host().GetPage());
}

size_t AdAuctionServiceImpl::PreconnectToBuyerOrigins(
    const blink::AuctionConfig& config) {
  if (!config.non_shared_params.interest_group_buyers) {
    return 0;
  }
  size_t n_owners_cached = 0;
  for (const auto& buyer : *config.non_shared_params.interest_group_buyers) {
    std::optional<url::Origin> signals_origin;
    if (GetInterestGroupManager().GetCachedOwnerAndSignalsOrigins(
            buyer, signals_origin)) {
      net::NetworkAnonymizationKey network_anonymization_key =
          net::NetworkAnonymizationKey::CreateSameSite(
              net::SchemefulSite(buyer));
      PreconnectSocket(buyer.GetURL(), network_anonymization_key);
      n_owners_cached += 1;
      if (signals_origin) {
        // We preconnect to the signals origin and not the full signals URL so
        // that we do not need to store the full URL in memory. Preconnecting
        // to the origin will be roughly equivalent to preconnecting to the
        // full URL.
        PreconnectSocket(signals_origin->GetURL(), network_anonymization_key);
      }
    }
  }
  return n_owners_cached;
}

}  // namespace content
