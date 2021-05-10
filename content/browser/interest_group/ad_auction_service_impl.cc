// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/ad_auction_service_impl.h"

#include <string>
#include <vector>

#include "auction_url_loader_factory_proxy.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/service_sandbox_type.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace content {

namespace {

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
                 const url::Origin& frame_origin) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->redirect_mode = network::mojom::RedirectMode::kError;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->request_initiator = frame_origin;
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->isolation_info =
      net::IsolationInfo::CreateTransient();
  auto simple_url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), kTrafficAnnotation);
  network::SimpleURLLoader* simple_url_loader_ptr = simple_url_loader.get();
  // Pass simple_url_loader to keep it alive until the request fails or succeeds
  // to prevent cancelling the request.
  // TODO(qingxin): time out these requests if they take too long.
  simple_url_loader_ptr->DownloadHeadersOnly(
      url_loader_factory,
      base::BindOnce(
          base::DoNothing::Once<std::unique_ptr<network::SimpleURLLoader>,
                                scoped_refptr<net::HttpResponseHeaders>>(),
          std::move(simple_url_loader)));
}

void OnWorkletCrashed(
    std::unique_ptr<AdAuctionServiceImpl::RunAdAuctionCallback> callback) {
  if (*callback)
    std::move(*callback).Run(base::nullopt);
}

struct ValidatedResult {
  bool is_valid_auction_result = false;
  std::string ad_json;
};

ValidatedResult ValidateAuctionResult(
    std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> bidders_copy,
    const GURL& render_url,
    const url::Origin& owner,
    const std::string& name) {
  ValidatedResult result;
  if (!render_url.is_valid() || !render_url.SchemeIs(url::kHttpsScheme))
    return result;

  for (const auto& bidder : bidders_copy) {
    // Auction winner must be one of the bidders and bidder must have ads.
    if (bidder->group->owner != owner || bidder->group->name != name ||
        !bidder->group->ads) {
      continue;
    }
    // `render_url` must be one of the winning bidder's ads.
    for (const auto& ad : bidder->group->ads.value()) {
      if (ad->render_url == render_url) {
        result.is_valid_auction_result = true;
        if (ad->metadata) {
          //`metadata` is already in JSON so no quotes are needed.
          result.ad_json = base::StringPrintf(
              R"({"render_url":"%s","metadata":%s})", render_url.spec().c_str(),
              ad->metadata.value().c_str());
        } else {
          result.ad_json = base::StringPrintf(R"({"render_url":"%s"})",
                                              render_url.spec().c_str());
        }
        return result;
      }
    }
  }
  return result;
}

}  // namespace

AdAuctionServiceImpl::AdAuctionServiceImpl(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::AdAuctionService> receiver)
    : FrameServiceBase(render_frame_host, std::move(receiver)) {}

AdAuctionServiceImpl::~AdAuctionServiceImpl() = default;

// static
void AdAuctionServiceImpl::CreateMojoService(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::AdAuctionService> receiver) {
  DCHECK(render_frame_host);

  // The object is bound to the lifetime of `render_frame_host` and the mojo
  // connection. See FrameServiceBase for details.
  new AdAuctionServiceImpl(render_frame_host, std::move(receiver));
}

void AdAuctionServiceImpl::RunAdAuction(blink::mojom::AuctionAdConfigPtr config,
                                        RunAdAuctionCallback callback) {
  const url::Origin frame_origin = origin();
  // If the interest group API is not allowed for this seller do nothing.
  if (!GetContentClient()->browser()->IsInterestGroupAPIAllowed(
          render_frame_host()->GetBrowserContext(), frame_origin,
          config->seller.GetURL())) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  // The seller origin has to match the `decision_logic_url` origin.
  if (config->seller.scheme() != url::kHttpsScheme ||
      !config->decision_logic_url.SchemeIs(url::kHttpsScheme) ||
      config->seller != url::Origin::Create(config->decision_logic_url)) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  if (!config->interest_group_buyers ||
      config->interest_group_buyers->is_all_buyers()) {
    std::move(callback).Run(base::nullopt);
    return;
  }
  DCHECK(config->interest_group_buyers->is_buyers());
  auto buyers = config->interest_group_buyers->get_buyers();

  std::vector<url::Origin> trimmed_buyers;
  std::copy_if(
      buyers.begin(), buyers.end(), std::back_inserter(trimmed_buyers),
      [this, &frame_origin](const url::Origin& buyer) {
        return GetContentClient()->browser()->IsInterestGroupAPIAllowed(
            render_frame_host()->GetBrowserContext(), frame_origin,
            buyer.GetURL());
      });
  if (trimmed_buyers.size() == 0) {
    std::move(callback).Run(base::nullopt);
    return;
  }
  GetInterestGroupsFromStorage(std::move(config), trimmed_buyers,
                               std::move(callback));
}

InterestGroupManager* AdAuctionServiceImpl::GetInterestGroupManager() {
  return static_cast<StoragePartitionImpl*>(
             render_frame_host()->GetStoragePartition())
      ->GetInterestGroupStorage();
}

void AdAuctionServiceImpl::LaunchWorkletServiceIfNeeded() {
  if (auction_worklet_service_ && auction_worklet_service_.is_connected())
    return;
  auction_worklet_service_.reset();
  content::ServiceProcessHost::Launch(
      auction_worklet_service_.BindNewPipeAndPassReceiver(),
      ServiceProcessHost::Options()
          .WithDisplayName("Auction Worklet Service")
          .Pass());
}

void AdAuctionServiceImpl::GetInterestGroupsFromStorage(
    blink::mojom::AuctionAdConfigPtr config,
    const std::vector<url::Origin>& buyers,
    RunAdAuctionCallback callback) {
  if (buyers.empty()) {
    std::move(callback).Run(base::nullopt);
    return;
  }
  // Buyers in `per_buyer_signals` should be a subset of `buyers`.
  if (config->per_buyer_signals) {
    for (const auto& it : config->per_buyer_signals.value()) {
      if (!base::Contains(buyers, it.first)) {
        std::move(callback).Run(base::nullopt);
        return;
      }
    }
  }

  std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> bidders;
  std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> interest_groups;
  GetInterestGroup(buyers, std::move(bidders), std::move(config),
                   std::move(callback), std::move(interest_groups));
}

void AdAuctionServiceImpl::GetInterestGroup(
    std::vector<url::Origin> buyers,
    std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> bidders,
    blink::mojom::AuctionAdConfigPtr config,
    RunAdAuctionCallback callback,
    std::vector<auction_worklet::mojom::BiddingInterestGroupPtr>
        interest_groups) {
  for (auto& interest_group : interest_groups)
    bidders.emplace_back(std::move(interest_group));

  // Get interest groups of each buyer from storage and push them to bidders.
  if (!buyers.empty()) {
    url::Origin buyer = buyers.back();
    buyers.pop_back();
    if (buyer.scheme() != url::kHttpsScheme) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    GetInterestGroupManager()->GetInterestGroupsForOwner(
        buyer, base::BindOnce(&AdAuctionServiceImpl::GetInterestGroup,
                              weak_ptr_factory_.GetWeakPtr(), std::move(buyers),
                              std::move(bidders), std::move(config),
                              std::move(callback)));
    return;
  }

  StartAuction(std::move(config), std::move(bidders), std::move(callback));
}

void AdAuctionServiceImpl::StartAuction(
    blink::mojom::AuctionAdConfigPtr config,
    std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> bidders,
    RunAdAuctionCallback callback) {
  if (bidders.empty()) {
    std::move(callback).Run(base::nullopt);
    return;
  }
  // TODO(qingxin): Determine if one service per auction / frame is good.
  LaunchWorkletServiceIfNeeded();
  auto browser_signals =
      auction_worklet::mojom::BrowserSignals::New(origin(), config->seller);

  mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory;
  auto url_loader_factory_proxy =
      std::make_unique<AuctionURLLoaderFactoryProxy>(
          url_loader_factory.InitWithNewPipeAndPassReceiver(),
          base::BindRepeating(&AdAuctionServiceImpl::GetFrameURLLoaderFactory,
                              base::Unretained(this)),
          base::BindRepeating(&AdAuctionServiceImpl::GetTrustedURLLoaderFactory,
                              base::Unretained(this)),
          browser_signals->top_frame_origin, *config, bidders);

  // If the AuctionWorklet service crashes, it will silently delete the bound
  // WorkletComplete callback. Create a ScopedClosureRunner to invoke the
  // RunAdAuctionCallback if the WorkletComplete callback is destroyed without
  // being invoked.
  // TODO(crbug.com/1201642): Redesign this to handle worklet crashes
  // differently.
  std::unique_ptr<RunAdAuctionCallback> owned_callback =
      std::make_unique<RunAdAuctionCallback>(std::move(callback));
  RunAdAuctionCallback* unowned_callback = owned_callback.get();
  base::ScopedClosureRunner on_crash(
      base::BindOnce(&OnWorkletCrashed, std::move(owned_callback)));
  std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> bidders_copy;
  bidders_copy.reserve(bidders.size());
  for (auto& bidder : bidders)
    bidders_copy.emplace_back(bidder.Clone());
  auction_worklet_service_->RunAuction(
      std::move(url_loader_factory), std::move(config), std::move(bidders),
      std::move(browser_signals),
      base::BindOnce(&AdAuctionServiceImpl::WorkletComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(bidders_copy),
                     unowned_callback, std::move(url_loader_factory_proxy),
                     std::move(on_crash)));
}

void AdAuctionServiceImpl::WorkletComplete(
    std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> bidders_copy,
    RunAdAuctionCallback* callback,
    std::unique_ptr<AuctionURLLoaderFactoryProxy> url_loader_factory_proxy,
    base::ScopedClosureRunner on_crash,
    const GURL& render_url,
    const url::Origin& owner,
    const std::string& name,
    auction_worklet::mojom::WinningBidderReportPtr bidder_report,
    auction_worklet::mojom::SellerReportPtr seller_report) {
  std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> bidders;
  bidders.reserve(bidders_copy.size());
  for (auto& bidder : bidders_copy)
    bidders.emplace_back(bidder.Clone());
  // Check if returned winner's information is valid.
  ValidatedResult result =
      ValidateAuctionResult(std::move(bidders_copy), render_url, owner, name);
  if (!result.is_valid_auction_result) {
    std::move(*callback).Run(base::nullopt);
    return;
  }
  std::move(*callback).Run(render_url);

  GetInterestGroupManager()->RecordInterestGroupWin(owner, name,
                                                    result.ad_json);
  // TODO(qingxin): Decide if we should record a bid if the auction fails, or
  // the interest group doesn't make a bid.
  for (auto& bidder : bidders) {
    GetInterestGroupManager()->RecordInterestGroupBid(bidder->group->owner,
                                                      bidder->group->name);
  }

  network::mojom::URLLoaderFactory* factory = GetTrustedURLLoaderFactory();
  if (bidder_report->report_requested && bidder_report->report_url.is_valid() &&
      bidder_report->report_url.SchemeIs(url::kHttpsScheme)) {
    FetchReport(factory, bidder_report->report_url, origin());
  }
  if (seller_report->report_requested && seller_report->report_url.is_valid() &&
      seller_report->report_url.SchemeIs(url::kHttpsScheme)) {
    FetchReport(factory, seller_report->report_url, origin());
  }
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
        url::Origin(), base::nullopt /* navigation_id */,
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

}  // namespace content
