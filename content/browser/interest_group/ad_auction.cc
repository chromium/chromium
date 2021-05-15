// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/ad_auction.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/interest_group/ad_auction_service_impl.h"
#include "content/browser/interest_group/auction_url_loader_factory_proxy.h"
#include "content/browser/interest_group/interest_group_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace content {

namespace {

bool IsAuctionValid(const blink::mojom::AuctionAdConfig& config) {
  // The seller origin has to be HTTPS and match the `decision_logic_url`
  // origin.
  if (config.seller.scheme() != url::kHttpsScheme ||
      !config.decision_logic_url.SchemeIs(url::kHttpsScheme) ||
      config.seller != url::Origin::Create(config.decision_logic_url)) {
    return false;
  }

  if (!config.interest_group_buyers ||
      config.interest_group_buyers->is_all_buyers()) {
    return false;
  }
  DCHECK(config.interest_group_buyers->is_buyers());

  // All interest group owners must be HTTPS.
  for (const url::Origin& buyer : config.interest_group_buyers->get_buyers()) {
    if (buyer.scheme() != url::kHttpsScheme)
      return false;
  }

  // All buyer signals must be for listed buyers.
  if (config.per_buyer_signals) {
    for (const auto& it : config.per_buyer_signals.value()) {
      if (!base::Contains(config.interest_group_buyers->get_buyers(),
                          it.first)) {
        return false;
      }
    }
  }

  return true;
}

struct ValidatedResult {
  bool is_valid_auction_result = false;
  std::string ad_json;
};

ValidatedResult ValidateAuctionResult(
    const std::vector<auction_worklet::mojom::BiddingInterestGroupPtr>& bidders,
    const GURL& render_url,
    const url::Origin& owner,
    const std::string& name) {
  ValidatedResult result;
  if (!render_url.is_valid() || !render_url.SchemeIs(url::kHttpsScheme))
    return result;

  for (const auto& bidder : bidders) {
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

AdAuction::AdAuction(AdAuctionServiceImpl* ad_auction_service,
                     blink::mojom::AuctionAdConfigPtr config,
                     AuctionCompleteCallback callback)
    : ad_auction_service_(ad_auction_service),
      config_(std::move(config)),
      callback_(std::move(callback)) {
  DCHECK(ad_auction_service_);
  DCHECK(config_);
  DCHECK(callback_);
}

AdAuction::~AdAuction() = default;

void AdAuction::StartAuction() {
  if (!IsAuctionValid(*config_)) {
    OnAuctionFailed();
    return;
  }

  const url::Origin& frame_origin = ad_auction_service_->origin();
  BrowserContext* browser_context =
      ad_auction_service_->render_frame_host()->GetBrowserContext();
  // If the interest group API is not allowed for this seller do nothing.
  if (!GetContentClient()->browser()->IsInterestGroupAPIAllowed(
          browser_context, frame_origin, config_->seller.GetURL())) {
    OnAuctionFailed();
    return;
  }

  // Filter out buyers for whom the interest group API is not allowed.
  const auto& buyers = config_->interest_group_buyers->get_buyers();
  std::copy_if(
      buyers.begin(), buyers.end(), std::back_inserter(pending_buyers_),
      [browser_context, &frame_origin](const url::Origin& buyer) {
        return GetContentClient()->browser()->IsInterestGroupAPIAllowed(
            browser_context, frame_origin, buyer.GetURL());
      });

  // If there are no buyers (either due to filtering, or in the original auction
  // request), fail the auction.
  if (pending_buyers_.empty()) {
    OnAuctionFailed();
    return;
  }

  ReadNextInterestGroup();
}

void AdAuction::OnServiceCrash() {
  // This cancels any pending async callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();

  OnAuctionFailed();
}

void AdAuction::ReadNextInterestGroup() {
  DCHECK(!pending_buyers_.empty());

  url::Origin buyer = std::move(pending_buyers_.back());
  pending_buyers_.pop_back();

  ad_auction_service_->GetInterestGroupManager()->GetInterestGroupsForOwner(
      buyer, base::BindOnce(&AdAuction::OnInterestGroupRead,
                            weak_ptr_factory_.GetWeakPtr()));
}

void AdAuction::OnInterestGroupRead(
    std::vector<auction_worklet::mojom::BiddingInterestGroupPtr>
        interest_groups) {
  bidders_.insert(bidders_.end(),
                  std::make_move_iterator(interest_groups.begin()),
                  std::make_move_iterator(interest_groups.end()));

  // If more buyers in the queue, load the next one.
  if (!pending_buyers_.empty()) {
    ReadNextInterestGroup();
    return;
  }

  // If there are no found interest groups, end the auction without a winner.
  if (bidders_.empty()) {
    OnAuctionFailed();
    return;
  }

  StartWorklets();
}

void AdAuction::StartWorklets() {
  DCHECK(pending_buyers_.empty());
  DCHECK(!bidders_.empty());

  auto browser_signals = auction_worklet::mojom::BrowserSignals::New(
      ad_auction_service_->origin(), config_->seller);

  mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory;
  url_loader_factory_proxy_ = std::make_unique<AuctionURLLoaderFactoryProxy>(
      url_loader_factory.InitWithNewPipeAndPassReceiver(),
      base::BindRepeating(&AdAuctionServiceImpl::GetFrameURLLoaderFactory,
                          base::Unretained(ad_auction_service_)),
      base::BindRepeating(&AdAuctionServiceImpl::GetTrustedURLLoaderFactory,
                          base::Unretained(ad_auction_service_)),
      browser_signals->top_frame_origin, *config_, bidders_);

  std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> bidders_copy;
  bidders_copy.reserve(bidders_.size());
  for (auto& bidder : bidders_)
    bidders_copy.emplace_back(bidder.Clone());

  // `config_` is no longer needed after this point, so pass ownership of it
  // over to the worklet, instead of copying it.
  ad_auction_service_->GetWorkletService()->RunAuction(
      std::move(url_loader_factory), std::move(config_),
      std::move(bidders_copy), std::move(browser_signals),
      base::BindOnce(&AdAuction::WorkletComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AdAuction::WorkletComplete(
    const GURL& render_url,
    const url::Origin& owner,
    const std::string& name,
    auction_worklet::mojom::WinningBidderReportPtr bidder_report,
    auction_worklet::mojom::SellerReportPtr seller_report,
    const std::vector<std::string>& errors) {
  DCHECK(callback_);

  // Forward debug information to devtools.
  for (const std::string& error : errors) {
    devtools_instrumentation::LogWorkletError(
        static_cast<RenderFrameHostImpl*>(
            ad_auction_service_->render_frame_host()),
        error);
  }

  // Check if returned winner's information is valid.
  ValidatedResult result =
      ValidateAuctionResult(bidders_, render_url, owner, name);
  if (!result.is_valid_auction_result) {
    OnAuctionFailed();
    return;
  }

  absl::optional<GURL> bidder_report_url;
  if (bidder_report->report_requested && bidder_report->report_url.is_valid() &&
      bidder_report->report_url.SchemeIs(url::kHttpsScheme)) {
    bidder_report_url = bidder_report->report_url;
  }

  absl::optional<GURL> seller_report_url;
  if (seller_report->success && seller_report->report_url.is_valid() &&
      seller_report->report_url.SchemeIs(url::kHttpsScheme)) {
    seller_report_url = seller_report->report_url;
  }

  ad_auction_service_->GetInterestGroupManager()->RecordInterestGroupWin(
      owner, name, result.ad_json);
  // TODO(qingxin): Decide if we should record a bid if the auction fails, or
  // the interest group doesn't make a bid.
  for (const auto& bidder : bidders_) {
    ad_auction_service_->GetInterestGroupManager()->RecordInterestGroupBid(
        bidder->group->owner, bidder->group->name);
  }

  std::move(callback_).Run(this, render_url, bidder_report_url,
                           seller_report_url);
}

void AdAuction::OnAuctionFailed() {
  DCHECK(callback_);

  std::move(callback_).Run(this, absl::nullopt, absl::nullopt, absl::nullopt);
}

}  // namespace content
