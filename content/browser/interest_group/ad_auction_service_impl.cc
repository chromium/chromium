// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/ad_auction_service_impl.h"

#include <set>
#include <string>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
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
#include "content/browser/renderer_host/page_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace content {

namespace {

constexpr base::TimeDelta kMaxExpiry = base::Days(30);

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
            "default now.  When enabled, they can be disabled by the Privacy"
            " Sandbox setting."
          policy_exception_justification:
            "These requests are triggered by a website."
        })");

// Makes a self-owned uncredentialed request. It's used to report the result of
// an in-browser interest group based ad auction to an auction participant.
void FetchReport(network::mojom::URLLoaderFactory* url_loader_factory,
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
  network::SimpleURLLoader* simple_url_loader_ptr = simple_url_loader.get();
  // Pass simple_url_loader to keep it alive until the request fails or succeeds
  // to prevent cancelling the request.
  simple_url_loader_ptr->SetTimeoutDuration(base::Seconds(30));
  simple_url_loader_ptr->DownloadHeadersOnly(
      url_loader_factory,
      base::BindOnce([](std::unique_ptr<network::SimpleURLLoader>,
                        scoped_refptr<net::HttpResponseHeaders>) {},
                     std::move(simple_url_loader)));
}

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

bool IsAuctionValid(const blink::mojom::AuctionAdConfig& config,
                    bool is_top_level_auction) {
  // The seller origin has to be HTTPS.
  if (config.seller.scheme() != url::kHttpsScheme)
    return false;

  // Opaque Origins have empty schemes.
  DCHECK(!config.seller.opaque());

  // `decision_logic_url` and, if present, `trusted_scoring_signals_url` must
  // share the seller's origin.
  if (url::SchemeHostPort(config.decision_logic_url) !=
          config.seller.GetTupleOrPrecursorTupleIfOpaque() ||
      (config.trusted_scoring_signals_url &&
       url::SchemeHostPort(*config.trusted_scoring_signals_url) !=
           config.seller.GetTupleOrPrecursorTupleIfOpaque())) {
    return false;
  }

  const auto& non_shared_params = config.auction_ad_config_non_shared_params;
  // This isn't marked as optional in the Mojo struct, so Mojo should make sure
  // it is non-null.
  DCHECK(non_shared_params);

  // All interest group owners must be HTTPS.
  if (non_shared_params->interest_group_buyers) {
    for (const url::Origin& buyer : *non_shared_params->interest_group_buyers) {
      if (buyer.scheme() != url::kHttpsScheme)
        return false;
    }
  }

  for (const auto& component_auction :
       config.auction_ad_config_non_shared_params->component_auctions) {
    // Component auctions may not have their own nested component auctions.
    if (!is_top_level_auction)
      return false;
    if (!IsAuctionValid(*component_auction, /*is_top_level_auction=*/false))
      return false;
  }

  return true;
}

}  // namespace

AdAuctionServiceImpl::AdAuctionServiceImpl(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::AdAuctionService> receiver)
    : DocumentService(render_frame_host, std::move(receiver)),
      main_frame_origin_(
          render_frame_host->GetMainFrame()->GetLastCommittedOrigin()),
      main_frame_url_(render_frame_host->GetMainFrame()->GetLastCommittedURL()),
      auction_worklet_manager_(
          &GetInterestGroupManager().auction_process_manager(),
          GetTopWindowOrigin(),
          origin(),
          this) {}

AdAuctionServiceImpl::~AdAuctionServiceImpl() {
  while (!auctions_.empty()) {
    // Need to fail all auctions rather than just deleting them, to ensure Mojo
    // callbacks from the renderers are invoked. Uninvoked Mojo callbacks may
    // not be destroyed before the Mojo pipe is, and the parent DocumentService
    // class owns the pipe, so it may still be open at this point.
    (*auctions_.begin())->FailAuction();
  }
}

// static
void AdAuctionServiceImpl::CreateMojoService(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::AdAuctionService> receiver) {
  DCHECK(render_frame_host);

  // The object is bound to the lifetime of `render_frame_host` and the mojo
  // connection. See DocumentService for details.
  new AdAuctionServiceImpl(render_frame_host, std::move(receiver));
}

void AdAuctionServiceImpl::JoinInterestGroup(
    const blink::InterestGroup& group) {
  // If the interest group API is not allowed for this context by Permissions
  // Policy, do nothing
  if (!render_frame_host()->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kJoinAdInterestGroup)) {
    mojo::ReportBadMessage("Unexpected request");
    return;
  }
  // If the interest group API is not allowed for this origin do nothing.
  if (!IsInterestGroupAPIAllowed(
          ContentBrowserClient::InterestGroupApiOperation::kJoin,
          group.owner)) {
    return;
  }

  // Disallow setting interest groups for another origin. Eventually, this will
  // need to perform a fetch to check for cross-origin permissions to add an
  // interest group.
  if (origin() != group.owner)
    return;

  blink::InterestGroup updated_group = group;
  base::Time max_expiry = base::Time::Now() + kMaxExpiry;
  if (updated_group.expiry > max_expiry)
    updated_group.expiry = max_expiry;
  GetInterestGroupManager().JoinInterestGroup(std::move(updated_group),
                                              main_frame_url_);
}

void AdAuctionServiceImpl::LeaveInterestGroup(const url::Origin& owner,
                                              const std::string& name) {
  // If the interest group API is not allowed for this context by Permissions
  // Policy, do nothing
  if (!render_frame_host()->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kJoinAdInterestGroup)) {
    mojo::ReportBadMessage(
        "Unexpected request: JoinAdInterestGroup feature disabled");
    return;
  }
  // If the interest group API is not allowed for this origin do nothing.
  if (!IsInterestGroupAPIAllowed(
          ContentBrowserClient::InterestGroupApiOperation::kLeave, origin())) {
    return;
  }

  if (origin().scheme() != url::kHttpsScheme) {
    mojo::ReportBadMessage(
        "Unexpected request: JoinAdInterestGroup only supported for secure "
        "origins");
    return;
  }

  if (owner != origin())
    return;

  GetInterestGroupManager().LeaveInterestGroup(owner, name);
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
    mojo::ReportBadMessage(
        "Unexpected request: JoinAdInterestGroupForDocument only supported for "
        "secure origins");
    return;
  }

  if (!render_frame_host()->IsNestedWithinFencedFrame()) {
    mojo::ReportBadMessage(
        "Unexpected request: JoinAdInterestGroupForDocument only supported "
        "within fenced frames");
    return;
  }

  // Get interest group owner and name. AdAuctionDocumentData is created as
  // part of navigation to a mapped URN URL. We need to find the top-level
  // fenced frame, since only the top-level frame has the document data.
  RenderFrameHost* rfh = render_frame_host();
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
      auction_data->interest_group_owner(),
      auction_data->interest_group_name());
}

void AdAuctionServiceImpl::UpdateAdInterestGroups() {
  // If the interest group API is not allowed for this context by Permissions
  // Policy, do nothing
  if (!render_frame_host()->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kJoinAdInterestGroup)) {
    mojo::ReportBadMessage("Unexpected request");
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

void AdAuctionServiceImpl::RunAdAuction(blink::mojom::AuctionAdConfigPtr config,
                                        RunAdAuctionCallback callback) {
  // If the run ad auction API is not allowed for this context by Permissions
  // Policy, do nothing
  if (!render_frame_host()->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kRunAdAuction)) {
    mojo::ReportBadMessage("Unexpected request");
    return;
  }
  if (!IsAuctionValid(*config, /*is_top_level_auction=*/true)) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  auto* auction_result_metrics = AdAuctionResultMetrics::GetOrCreateForPage(
      render_frame_host()->GetPage());
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
      absl::optional<GURL> mapped_url,
      absl::optional<AdAuctionData> ad_auction_data,
      absl::optional<FencedFrameURLMapping::PendingAdComponentsMap>
          pending_ad_components_map,
      ReportingMetadata& reporting_metadata) override {
    mapped_url_ = mapped_url;
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
    std::move(callback).Run(absl::nullopt);
    return;
  }
  FencedFrameURLMappingObserver obs;
  content::FencedFrameURLMapping& mapping =
      static_cast<RenderFrameHostImpl*>(render_frame_host())
          ->GetPage()
          .fenced_frame_urls_map();
  // FLEDGE URN URLs should already be mapped, so the observer will be called
  // synchronously.
  mapping.ConvertFencedFrameURNToURL(urn_url, &obs);
  if (!obs.called_)
    mapping.RemoveObserverForURN(urn_url, &obs);
  std::move(callback).Run(std::move(obs.mapped_url_));
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
                                      ::blink::mojom::AuctionAdConfigPtr config,
                                      FinalizeAdCallback callback) {
  if (!config->decision_logic_url.SchemeIs(url::kHttpsScheme)) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  if (ads_guid.empty()) {
    std::move(callback).Run(absl::nullopt);
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
    render_frame_host()->CreateNetworkServiceDefaultFactory(
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
        render_frame_host()->GetSiteInstance()->GetBrowserContext(),
        render_frame_host(), render_frame_host()->GetProcess()->GetID(),
        ContentBrowserClient::URLLoaderFactoryType::kDocumentSubResource,
        url::Origin(), absl::nullopt /* navigation_id */,
        ukm::SourceIdObj::FromInt64(render_frame_host()->GetPageUkmSourceId()),
        &factory_receiver, nullptr /* header_client */,
        nullptr /* bypass_redirect_checks */, nullptr /* disable_secure_dns */,
        nullptr /* factory_override */);

    render_frame_host()
        ->GetStoragePartition()
        ->GetURLLoaderFactoryForBrowserProcess()
        ->Clone(std::move(factory_receiver));
  }
  return trusted_url_loader_factory_.get();
}

RenderFrameHostImpl* AdAuctionServiceImpl::GetFrame() {
  return static_cast<RenderFrameHostImpl*>(render_frame_host());
}

network::mojom::ClientSecurityStatePtr
AdAuctionServiceImpl::GetClientSecurityState() {
  return GetFrame()->BuildClientSecurityState();
}

bool AdAuctionServiceImpl::IsInterestGroupAPIAllowed(
    ContentBrowserClient::InterestGroupApiOperation
        interest_group_api_operation,
    const url::Origin& origin) const {
  return GetContentClient()->browser()->IsInterestGroupAPIAllowed(
      render_frame_host(), interest_group_api_operation, main_frame_origin_,
      origin);
}

void AdAuctionServiceImpl::HandleReports(
    network::mojom::URLLoaderFactory* factory,
    const std::vector<GURL>& report_urls,
    const std::string& name) {
  for (const GURL& report_url : report_urls) {
    base::UmaHistogramCounts100000(
        base::StrCat({"Ads.InterestGroup.Net.RequestUrlSizeBytes.", name}),
        report_url.spec().size());
    base::UmaHistogramCounts100(
        base::StrCat({"Ads.InterestGroup.Net.ResponseSizeBytes.", name}), 0);
    FetchReport(factory, report_url, origin(), GetClientSecurityState());
  }
}

void AdAuctionServiceImpl::OnAuctionComplete(
    RunAdAuctionCallback callback,
    AuctionRunner* auction,
    absl::optional<AuctionRunner::InterestGroupKey> winning_group_id,
    absl::optional<GURL> render_url,
    std::vector<GURL> ad_component_urls,
    std::vector<GURL> report_urls,
    std::vector<GURL> debug_loss_report_urls,
    std::vector<GURL> debug_win_report_urls,
    ReportingMetadata ad_beacon_map,
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

  auto* auction_result_metrics =
      AdAuctionResultMetrics::GetForPage(render_frame_host()->GetPage());

  if (!render_url) {
    DCHECK(report_urls.empty());
    std::move(callback).Run(absl::nullopt);
    auction_result_metrics->ReportAuctionResult(
        AdAuctionResultMetrics::AuctionResult::kFailed);
    HandleReports(GetTrustedURLLoaderFactory(),
                  std::move(debug_loss_report_urls), "DebugLossReport");
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
  auction_result_metrics->ReportAuctionResult(
      AdAuctionResultMetrics::AuctionResult::kSucceeded);

  network::mojom::URLLoaderFactory* factory = GetTrustedURLLoaderFactory();
  HandleReports(factory, std::move(report_urls), "SendReportToReport");
  HandleReports(factory, std::move(debug_loss_report_urls), "DebugLossReport");
  HandleReports(factory, std::move(debug_win_report_urls), "DebugWinReport");
}

InterestGroupManagerImpl& AdAuctionServiceImpl::GetInterestGroupManager()
    const {
  return *static_cast<InterestGroupManagerImpl*>(
      render_frame_host()->GetStoragePartition()->GetInterestGroupManager());
}

url::Origin AdAuctionServiceImpl::GetTopWindowOrigin() const {
  if (!render_frame_host()->GetParent())
    return origin();
  return render_frame_host()->GetMainFrame()->GetLastCommittedOrigin();
}

}  // namespace content
