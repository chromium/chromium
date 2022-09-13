// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/ad_auction_service_impl.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "content/browser/interest_group/ad_auction_document_data.h"
#include "content/browser/interest_group/ad_auction_result_metrics.h"
#include "content/browser/interest_group/auction_runner.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/private_aggregation/private_aggregation_manager.h"
#include "content/browser/renderer_host/page_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/aggregatable_report.mojom.h"
#include "content/common/private_aggregation_host.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
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

}  // namespace

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
  if (!JoinOrLeaveApiAllowedFromRenderer(group.owner))
    return;

  // If the interest group API is not allowed for this origin, report the result
  // of the permissions check, but don't actually join the interest group.
  // The return value of IsInterestGroupAPIAllowed() is potentially affected by
  // a user's browser configuration, which shouldn't be leaked to sites to
  // protect against fingerprinting.
  bool report_result_only = !IsInterestGroupAPIAllowed(
      ContentBrowserClient::InterestGroupApiOperation::kJoin, group.owner);

  blink::InterestGroup updated_group = group;
  base::Time max_expiry = base::Time::Now() + kMaxExpiry;
  if (updated_group.expiry > max_expiry)
    updated_group.expiry = max_expiry;

  GetInterestGroupManager().CheckPermissionsAndJoinInterestGroup(
      std::move(updated_group), main_frame_url_, origin(),
      GetFrame()->GetNetworkIsolationKey(), report_result_only,
      *GetFrameURLLoaderFactory(), std::move(callback));
}

void AdAuctionServiceImpl::LeaveInterestGroup(
    const url::Origin& owner,
    const std::string& name,
    LeaveInterestGroupCallback callback) {
  if (!JoinOrLeaveApiAllowedFromRenderer(owner))
    return;

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

  if (!render_frame_host().IsNestedWithinFencedFrame()) {
    ReportBadMessageAndDeleteThis(
        "Unexpected request: LeaveInterestGroupForDocument only supported "
        "within fenced frames");
    return;
  }

  // Get interest group owner and name. AdAuctionDocumentData is created as
  // part of navigation to a mapped URN URL. We need to find the top-level
  // fenced frame, since only the top-level frame has the document data.
  RenderFrameHost* rfh = &render_frame_host();
  while (!rfh->IsFencedFrameRoot()) {
    rfh = rfh->GetParentOrOuterDocument();
    if (!rfh) {
      return;
    }
  }
  AdAuctionDocumentData* auction_data =
      AdAuctionDocumentData::GetForCurrentDocument(rfh);
  if (!auction_data) {
    return;
  }

  if (auction_data->interest_group_owner() != origin()) {
    // The ad page calling LeaveAdInterestGroup is not the owner of the group.
    return;
  }

  GetInterestGroupManager().LeaveInterestGroup(
      blink::InterestGroupKey(auction_data->interest_group_owner(),
                              auction_data->interest_group_name()),
      main_frame_origin_);
}

void AdAuctionServiceImpl::UpdateAdInterestGroups() {
  // If the interest group API is not allowed for this context by Permissions
  // Policy, do nothing
  if (!render_frame_host().IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kJoinAdInterestGroup)) {
    ReportBadMessageAndDeleteThis("Unexpected request");
    return;
  }
  // If the interest group API is not allowed for this origin do nothing.
  if (!IsInterestGroupAPIAllowed(
          ContentBrowserClient::InterestGroupApiOperation::kUpdate, origin())) {
    return;
  }
  GetInterestGroupManager().UpdateInterestGroupsOfOwner(
      origin(), GetClientSecurityState());
}

void AdAuctionServiceImpl::RunAdAuction(const blink::AuctionConfig& config,
                                        RunAdAuctionCallback callback) {
  // If the run ad auction API is not allowed for this context by Permissions
  // Policy, do nothing
  if (!render_frame_host().IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kRunAdAuction)) {
    ReportBadMessageAndDeleteThis("Unexpected request");
    return;
  }

  auto* auction_result_metrics =
      AdAuctionResultMetrics::GetOrCreateForPage(render_frame_host().GetPage());
  if (!auction_result_metrics->ShouldRunAuction()) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  std::unique_ptr<AuctionRunner> auction = AuctionRunner::CreateAndStart(
      &auction_worklet_manager_, &GetInterestGroupManager(), std::move(config),
      GetClientSecurityState(),
      base::BindRepeating(&AdAuctionServiceImpl::IsInterestGroupAPIAllowed,
                          base::Unretained(this)),
      base::BindOnce(&AdAuctionServiceImpl::OnAuctionComplete,
                     base::Unretained(this), std::move(callback)));
  auctions_.insert(std::move(auction));
}

namespace {
class FencedFrameURLMappingObserver
    : public FencedFrameURLMapping::MappingResultObserver {
 public:
  FencedFrameURLMappingObserver() = default;
  ~FencedFrameURLMappingObserver() override = default;

  void OnFencedFrameURLMappingComplete(
      const absl::optional<FencedFrameURLMapping::FencedFrameProperties>&
          properties) override {
    if (properties) {
      mapped_url_ = properties->mapped_url;
    }
    called_ = true;
  }

  bool called_;
  absl::optional<GURL> mapped_url_;
};

}  // namespace

void AdAuctionServiceImpl::DeprecatedGetURLFromURN(
    const GURL& urn_url,
    DeprecatedGetURLFromURNCallback callback) {
  if (!blink::IsValidUrnUuidURL(urn_url)) {
    ReportBadMessageAndDeleteThis("Unexpected request: invalid URN");
    return;
  }
  FencedFrameURLMappingObserver obs;
  content::FencedFrameURLMapping& mapping =
      static_cast<RenderFrameHostImpl&>(render_frame_host())
          .GetPage()
          .fenced_frame_urls_map();
  // FLEDGE URN URLs should already be mapped, so the observer will be called
  // synchronously.
  mapping.ConvertFencedFrameURNToURL(urn_url, &obs);
  if (!obs.called_)
    mapping.RemoveObserverForURN(urn_url, &obs);
  std::move(callback).Run(std::move(obs.mapped_url_));
}

void AdAuctionServiceImpl::DeprecatedReplaceInURN(
    const GURL& urn_url,
    std::vector<blink::mojom::ReplacementPtr> replacements,
    DeprecatedReplaceInURNCallback callback) {
  if (!blink::IsValidUrnUuidURL(urn_url)) {
    ReportBadMessageAndDeleteThis("Unexpected request: invalid URN");
    return;
  }
  std::vector<std::pair<std::string, std::string>> local_replacements;
  for (const auto& replacement : replacements) {
    if (!(base::StartsWith(replacement->match, "${") &&
          base::EndsWith(replacement->match, "}")) &&
        !(base::StartsWith(replacement->match, "%%") &&
          base::EndsWith(replacement->match, "%%"))) {
      ReportBadMessageAndDeleteThis("Unexpected request: bad replacement");
      return;
    }
    local_replacements.emplace_back(std::move(replacement->match),
                                    std::move(replacement->replacement));
  }
  content::FencedFrameURLMapping& mapping =
      static_cast<RenderFrameHostImpl&>(render_frame_host())
          .GetPage()
          .fenced_frame_urls_map();
  mapping.SubstituteMappedURL(urn_url, local_replacements);
  std::move(callback).Run();
}

void AdAuctionServiceImpl::CreateAdRequest(
    blink::mojom::AdRequestConfigPtr config,
    CreateAdRequestCallback callback) {
  if (!IsAdRequestValid(*config)) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  // TODO(https://crbug.com/1249186): Actually request Ads and return a guid.
  // For now just act like it failed.
  std::move(callback).Run(absl::nullopt);
}

void AdAuctionServiceImpl::FinalizeAd(const std::string& ads_guid,
                                      const blink::AuctionConfig& config,
                                      FinalizeAdCallback callback) {
  if (ads_guid.empty()) {
    ReportBadMessageAndDeleteThis("GUID empty");
    return;
  }

  // TODO(https://crbug.com/1249186): Actually finalize Ad and return an URL.
  // For now just act like it failed.
  std::move(callback).Run(absl::nullopt);
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
  if (!trusted_url_loader_factory_ ||
      !trusted_url_loader_factory_.is_connected()) {
    trusted_url_loader_factory_.reset();
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver =
        trusted_url_loader_factory_.BindNewPipeAndPassReceiver();

    // TODO(mmenke): Should this have its own URLLoaderFactoryType? FLEDGE
    // requests are very different from subresource requests.
    //
    // TODO(mmenke): Hook up devtools.
    GetContentClient()->browser()->WillCreateURLLoaderFactory(
        render_frame_host().GetSiteInstance()->GetBrowserContext(),
        &render_frame_host(), render_frame_host().GetProcess()->GetID(),
        ContentBrowserClient::URLLoaderFactoryType::kDocumentSubResource,
        url::Origin(), /*navigation_id=*/absl::nullopt,
        ukm::SourceIdObj::FromInt64(render_frame_host().GetPageUkmSourceId()),
        &factory_receiver, /*header_client=*/nullptr,
        /*bypass_redirect_checks=*/nullptr, /*disable_secure_dns=*/nullptr,
        /*factory_override=*/nullptr);

    render_frame_host()
        .GetStoragePartition()
        ->GetURLLoaderFactoryForBrowserProcess()
        ->Clone(std::move(factory_receiver));

    mojo::Remote<network::mojom::URLLoaderFactory> shared_remote;
    trusted_url_loader_factory_->Clone(
        shared_remote.BindNewPipeAndPassReceiver());
    ref_counted_trusted_url_loader_factory_ =
        base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
            std::move(shared_remote));
  }
  return trusted_url_loader_factory_.get();
}

scoped_refptr<network::WrapperSharedURLLoaderFactory>
AdAuctionServiceImpl::GetRefCountedTrustedURLLoaderFactory() {
  GetTrustedURLLoaderFactory();
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

AdAuctionServiceImpl::AdAuctionServiceImpl(
    RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::AdAuctionService> receiver)
    : DocumentService(render_frame_host, std::move(receiver)),
      main_frame_origin_(
          render_frame_host.GetMainFrame()->GetLastCommittedOrigin()),
      main_frame_url_(render_frame_host.GetMainFrame()->GetLastCommittedURL()),
      auction_worklet_manager_(
          &GetInterestGroupManager().auction_process_manager(),
          GetTopWindowOrigin(),
          origin(),
          this),
      private_aggregation_manager_(PrivateAggregationManager::GetManager(
          *render_frame_host.GetBrowserContext())) {}

AdAuctionServiceImpl::~AdAuctionServiceImpl() {
  while (!auctions_.empty()) {
    // Need to fail all auctions rather than just deleting them, to ensure Mojo
    // callbacks from the renderers are invoked. Uninvoked Mojo callbacks may
    // not be destroyed before the Mojo pipe is, and the parent DocumentService
    // class owns the pipe, so it may still be open at this point.
    (*auctions_.begin())->FailAuction();
  }
}

bool AdAuctionServiceImpl::JoinOrLeaveApiAllowedFromRenderer(
    const url::Origin& owner) {
  // If the interest group API is not allowed for this context by Permissions
  // Policy, do nothing
  if (!render_frame_host().IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kJoinAdInterestGroup)) {
    ReportBadMessageAndDeleteThis("Unexpected request");
    return false;
  }

  if (owner.scheme() != url::kHttpsScheme) {
    ReportBadMessageAndDeleteThis(
        "Unexpected request: Interest groups may only be owned by secure "
        "origins");
    return false;
  }

  if (origin().scheme() != url::kHttpsScheme) {
    ReportBadMessageAndDeleteThis(
        "Unexpected request: Interest groups may only be joined or left from "
        "secure origins");
    return false;
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

void AdAuctionServiceImpl::SendPrivateAggregationRequests(
    std::map<url::Origin,
             std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>>
        private_aggregation_requests) const {
  // Empty vectors should've been filtered out.
  DCHECK(base::ranges::none_of(private_aggregation_requests,
                               [](auto& it) { return it.second.empty(); }));

  // TODO(crbug.com/1356654): Improve coverage of these use counters, i.e. for
  // API usage that does not result in a successful request.
  if (!private_aggregation_requests.empty()) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        &render_frame_host(),
        blink::mojom::WebFeature::kPrivateAggregationApiAll);
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        &render_frame_host(),
        blink::mojom::WebFeature::kPrivateAggregationApiFledge);
  }

  if (!private_aggregation_manager_) {
    return;
  }

  for (auto& [origin, requests] : private_aggregation_requests) {
    mojo::Remote<mojom::PrivateAggregationHost> remote;
    if (!private_aggregation_manager_->BindNewReceiver(
            origin, main_frame_origin_,
            PrivateAggregationBudgetKey::Api::kFledge,
            remote.BindNewPipeAndPassReceiver())) {
      continue;
    }

    for (auction_worklet::mojom::PrivateAggregationRequestPtr& request :
         requests) {
      DCHECK(request);
      std::vector<mojom::AggregatableReportHistogramContributionPtr>
          contributions;
      contributions.push_back(std::move(request->contribution));
      remote->SendHistogramReport(std::move(contributions),
                                  request->aggregation_mode,
                                  std::move(request->debug_mode_details));
    }
  }
}

void AdAuctionServiceImpl::OnAuctionComplete(
    RunAdAuctionCallback callback,
    AuctionRunner* auction,
    absl::optional<blink::InterestGroupKey> winning_group_id,
    absl::optional<GURL> render_url,
    std::vector<GURL> ad_component_urls,
    std::vector<GURL> report_urls,
    std::vector<GURL> debug_loss_report_urls,
    std::vector<GURL> debug_win_report_urls,
    ReportingMetadata ad_beacon_map,
    std::map<url::Origin,
             std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>>
        private_aggregation_requests,
    std::vector<std::string> errors) {
  // Delete the AuctionRunner. Since all arguments are passed by value, they're
  // all safe to used after this has been done.
  auto auction_it = auctions_.find(auction);
  DCHECK(auction_it != auctions_.end());
  auctions_.erase(auction_it);

  // Forward debug information to devtools.
  for (const std::string& error : errors) {
    devtools_instrumentation::LogWorkletMessage(
        *GetFrame(), blink::mojom::ConsoleMessageLevel::kError,
        base::StrCat({"Worklet error: ", error}));
  }

  SendPrivateAggregationRequests(std::move(private_aggregation_requests));

  auto* auction_result_metrics =
      AdAuctionResultMetrics::GetForPage(render_frame_host().GetPage());

  if (!render_url) {
    DCHECK(report_urls.empty());
    std::move(callback).Run(absl::nullopt);
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
    GetInterestGroupManager().EnqueueReports(
        std::vector<GURL>(), std::vector<GURL>(), debug_loss_report_urls,
        origin(), GetClientSecurityState(),
        GetRefCountedTrustedURLLoaderFactory());
    return;
  }
  DCHECK(winning_group_id);  // Should always be present with a render_url
  DCHECK(blink::IsValidFencedFrameURL(*render_url));
  FencedFrameURLMapping& fenced_frame_urls_map =
      GetFrame()->GetPage().fenced_frame_urls_map();
  render_url = fenced_frame_urls_map.AddFencedFrameURLWithInterestGroupInfo(
      *render_url, {winning_group_id->owner, winning_group_id->name},
      ad_component_urls, ad_beacon_map);
  DCHECK(render_url->is_valid());

  std::move(callback).Run(render_url);
  if (auction_result_metrics) {
    auction_result_metrics->ReportAuctionResult(
        AdAuctionResultMetrics::AuctionResult::kSucceeded);
  }
  GetInterestGroupManager().EnqueueReports(
      report_urls, debug_win_report_urls, debug_loss_report_urls, origin(),
      GetClientSecurityState(), GetRefCountedTrustedURLLoaderFactory());
}

InterestGroupManagerImpl& AdAuctionServiceImpl::GetInterestGroupManager()
    const {
  return *static_cast<InterestGroupManagerImpl*>(
      render_frame_host().GetStoragePartition()->GetInterestGroupManager());
}

url::Origin AdAuctionServiceImpl::GetTopWindowOrigin() const {
  if (!render_frame_host().GetParent())
    return origin();
  return render_frame_host().GetMainFrame()->GetLastCommittedOrigin();
}

}  // namespace content
