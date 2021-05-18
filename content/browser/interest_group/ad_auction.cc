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
#include "content/browser/interest_group/auction_runner.h"
#include "content/browser/interest_group/auction_url_loader_factory_proxy.h"
#include "content/browser/interest_group/interest_group_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
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

  // TODO(mmenke): This should be top frame origin, not frame origin.
  auto browser_signals = auction_worklet::mojom::BrowserSignals::New(
      ad_auction_service_->origin(), config_->seller);

  std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> bidders_copy;
  bidders_copy.reserve(bidders_.size());
  for (auto& bidder : bidders_)
    bidders_copy.emplace_back(bidder.Clone());

  // `config_` is no longer needed after this point, so pass ownership of it
  // over to the AuctionRunner, instead of copying it.
  auction_runner_ = AuctionRunner::CreateAndStart(
      ad_auction_service_, std::move(config_), std::move(bidders_copy),
      std::move(browser_signals), ad_auction_service_->origin(),
      base::BindOnce(&AdAuction::WorkletComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AdAuction::WorkletComplete(const GURL& render_url,
                                const std::string& ad_metadata,
                                const url::Origin& owner,
                                const std::string& name,
                                const GURL& bidder_report_url,
                                const GURL& seller_report_url,
                                const std::vector<std::string>& errors) {
  DCHECK(callback_);

  // Forward debug information to devtools.
  for (const std::string& error : errors) {
    devtools_instrumentation::LogWorkletError(
        static_cast<RenderFrameHostImpl*>(
            ad_auction_service_->render_frame_host()),
        error);
  }

  if (!render_url.is_valid()) {
    OnAuctionFailed();
    return;
  }

  absl::optional<GURL> opt_bidder_report_url;
  if (bidder_report_url.is_valid())
    opt_bidder_report_url = bidder_report_url;

  absl::optional<GURL> opt_seller_report_url;
  if (seller_report_url.is_valid())
    opt_seller_report_url = seller_report_url;

  ad_auction_service_->GetInterestGroupManager()->RecordInterestGroupWin(
      owner, name, ad_metadata);
  // TODO(qingxin): Decide if we should record a bid if the auction fails, or
  // the interest group doesn't make a bid.
  for (const auto& bidder : bidders_) {
    ad_auction_service_->GetInterestGroupManager()->RecordInterestGroupBid(
        bidder->group->owner, bidder->group->name);
  }

  std::move(callback_).Run(this, render_url, opt_bidder_report_url,
                           opt_seller_report_url);
}

void AdAuction::OnAuctionFailed() {
  DCHECK(callback_);

  std::move(callback_).Run(this, absl::nullopt, absl::nullopt, absl::nullopt);
}

}  // namespace content
