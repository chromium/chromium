// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_runner.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

std::unique_ptr<AuctionRunner> AuctionRunner::CreateAndStart(
    AuctionWorkletManager* auction_worklet_manager,
    InterestGroupManagerImpl* interest_group_manager,
    const blink::AuctionConfig& auction_config,
    network::mojom::ClientSecurityStatePtr client_security_state,
    IsInterestGroupApiAllowedCallback is_interest_group_api_allowed_callback,
    mojo::PendingReceiver<AbortableAdAuction> abort_receiver,
    RunAuctionCallback callback) {
  std::unique_ptr<AuctionRunner> instance(new AuctionRunner(
      auction_worklet_manager, interest_group_manager,
      std::move(auction_config), std::move(client_security_state),
      std::move(is_interest_group_api_allowed_callback),
      std::move(abort_receiver), std::move(callback)));
  instance->StartAuction();
  return instance;
}

AuctionRunner::~AuctionRunner() = default;

void AuctionRunner::Abort() {
  // Don't abort if the auction already finished (either as success or failure).
  if (state_ != State::kFailed && state_ != State::kSucceeded) {
    FailAuction(/*manually_aborted=*/true);
  }
}

void AuctionRunner::FailAuction(bool manually_aborted) {
  DCHECK(callback_);
  state_ = State::kFailed;

  // Can have loss report URLs if the auction failed because the seller rejected
  // all bids.
  std::vector<GURL> debug_win_report_urls;
  std::vector<GURL> debug_loss_report_urls;
  auction_.TakeDebugReportUrls(debug_win_report_urls, debug_loss_report_urls);
  // Shouldn't have any win report URLs if nothing won the auction.
  DCHECK(debug_win_report_urls.empty());

  UpdateInterestGroupsPostAuction();

  std::move(callback_).Run(
      this, manually_aborted, /*winning_group_key=*/absl::nullopt,
      /*render_url=*/absl::nullopt,
      /*ad_component_urls=*/{},
      /*report_urls=*/{}, std::move(debug_loss_report_urls),
      std::move(debug_win_report_urls),
      /*ad_beacon_map=*/{}, auction_.TakePrivateAggregationRequests(),
      auction_.TakeErrors());
}

AuctionRunner::AuctionRunner(
    AuctionWorkletManager* auction_worklet_manager,
    InterestGroupManagerImpl* interest_group_manager,
    const blink::AuctionConfig& auction_config,
    network::mojom::ClientSecurityStatePtr client_security_state,
    IsInterestGroupApiAllowedCallback is_interest_group_api_allowed_callback,
    mojo::PendingReceiver<AbortableAdAuction> abort_receiver,
    RunAuctionCallback callback)
    : interest_group_manager_(interest_group_manager),
      client_security_state_(std::move(client_security_state)),
      is_interest_group_api_allowed_callback_(
          is_interest_group_api_allowed_callback),
      abort_receiver_(this, std::move(abort_receiver)),
      owned_auction_config_(auction_config),
      callback_(std::move(callback)),
      auction_(&owned_auction_config_,
               /*parent=*/nullptr,
               auction_worklet_manager,
               interest_group_manager,
               /*auction_start_time=*/base::Time::Now()) {}

void AuctionRunner::StartAuction() {
  auction_.StartLoadInterestGroupsPhase(
      is_interest_group_api_allowed_callback_,
      base::BindOnce(&AuctionRunner::OnLoadInterestGroupsComplete,
                     base::Unretained(this)));
}

void AuctionRunner::OnLoadInterestGroupsComplete(bool success) {
  if (!success) {
    FailAuction(/*manually_aborted=*/false);
    return;
  }

  state_ = State::kBiddingAndScoringPhase;
  auction_.StartBiddingAndScoringPhase(
      /*on_seller_receiver_callback=*/base::OnceClosure(),
      base::BindOnce(&AuctionRunner::OnBidsGeneratedAndScored,
                     base::Unretained(this)));
}

void AuctionRunner::OnBidsGeneratedAndScored(bool success) {
  blink::InterestGroupSet interest_groups_that_bid;
  auction_.GetInterestGroupsThatBid(interest_groups_that_bid);
  if (!success) {
    interest_group_manager_->RecordInterestGroupBids(interest_groups_that_bid);
    FailAuction(/*manually_aborted=*/false);
    return;
  }
  // If the auction is ongoing, RecordInterestGroupBids is deferred until
  // completion, so that bids don't get recorded if it gets aborted.

  state_ = State::kReportingPhase;
  auction_.StartReportingPhase(
      /*top_seller_signals=*/absl::nullopt,
      base::BindOnce(&AuctionRunner::OnReportingPhaseComplete,
                     base::Unretained(this),
                     std::move(interest_groups_that_bid)));
}

void AuctionRunner::OnReportingPhaseComplete(
    const blink::InterestGroupSet& interest_groups_that_bid,
    bool success) {
  interest_group_manager_->RecordInterestGroupBids(interest_groups_that_bid);
  if (!success) {
    FailAuction(/*manually_aborted=*/false);
    return;
  }

  DCHECK(callback_);

  std::string ad_metadata;
  if (auction_.top_bid()->bid->bid_ad->metadata) {
    //`metadata` is already in JSON so no quotes are needed.
    ad_metadata = base::StringPrintf(
        R"({"render_url":"%s","metadata":%s})",
        auction_.top_bid()->bid->render_url.spec().c_str(),
        auction_.top_bid()->bid->bid_ad->metadata.value().c_str());
  } else {
    ad_metadata =
        base::StringPrintf(R"({"render_url":"%s"})",
                           auction_.top_bid()->bid->render_url.spec().c_str());
  }

  DCHECK(auction_.top_bid()->bid->interest_group);
  const blink::InterestGroup& winning_group =
      *auction_.top_bid()->bid->interest_group;
  blink::InterestGroupKey winning_group_key(
      {winning_group.owner, winning_group.name});

  interest_group_manager_->RecordInterestGroupWin(winning_group_key,
                                                  ad_metadata);
  interest_group_manager_->RegisterAdAsWon(auction_.top_bid()->bid->render_url);

  std::vector<GURL> debug_win_report_urls;
  std::vector<GURL> debug_loss_report_urls;
  auction_.TakeDebugReportUrls(debug_win_report_urls, debug_loss_report_urls);

  UpdateInterestGroupsPostAuction();

  state_ = State::kSucceeded;
  std::move(callback_).Run(
      this, /*manually_aborted=*/false, std::move(winning_group_key),
      auction_.top_bid()->bid->render_url,
      auction_.top_bid()->bid->ad_components, auction_.TakeReportUrls(),
      std::move(debug_loss_report_urls), std::move(debug_win_report_urls),
      auction_.TakeAdBeaconMap(), auction_.TakePrivateAggregationRequests(),
      auction_.TakeErrors());
}

void AuctionRunner::UpdateInterestGroupsPostAuction() {
  std::vector<url::Origin> update_owners;
  auction_.TakePostAuctionUpdateOwners(update_owners);

  // De-duplicate.
  std::sort(update_owners.begin(), update_owners.end());
  update_owners.erase(std::unique(update_owners.begin(), update_owners.end()),
                      update_owners.end());

  // Filter owners not allowed to update.
  base::EraseIf(update_owners, [this](const url::Origin& owner) {
    return !is_interest_group_api_allowed_callback_.Run(
        ContentBrowserClient::InterestGroupApiOperation::kUpdate, owner);
  });

  interest_group_manager_->UpdateInterestGroupsOfOwners(
      update_owners, client_security_state_.Clone());
}

}  // namespace content
