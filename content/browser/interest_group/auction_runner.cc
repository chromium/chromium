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
#include "base/types/optional_ref.h"
#include "content/browser/interest_group/interest_group_auction_reporter.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

auction_worklet::mojom::KAnonymityBidMode DetermineKAnonMode() {
  if (base::FeatureList::IsEnabled(
          blink::features::kFledgeConsiderKAnonymity)) {
    if (base::FeatureList::IsEnabled(blink::features::kFledgeEnforceKAnonymity))
      return auction_worklet::mojom::KAnonymityBidMode::kEnforce;
    else
      return auction_worklet::mojom::KAnonymityBidMode::kSimulate;
  } else {
    return auction_worklet::mojom::KAnonymityBidMode::kNone;
  }
}

// Returns null if failed to verify.
blink::AuctionConfig* LookupAuction(
    blink::AuctionConfig& config,
    const blink::mojom::AuctionAdConfigAuctionIdPtr& auction) {
  if (auction->is_main_auction()) {
    return &config;
  }
  uint32_t pos = auction->get_component_auction();
  if (pos < config.non_shared_params.component_auctions.size()) {
    return &config.non_shared_params.component_auctions[pos];
  }
  return nullptr;
}

blink::AuctionConfig::MaybePromiseJson FromOptionalString(
    base::optional_ref<const std::string> maybe_json) {
  if (maybe_json.has_value()) {
    return blink::AuctionConfig::MaybePromiseJson::FromJson(*maybe_json);
  } else {
    return blink::AuctionConfig::MaybePromiseJson::FromNothing();
  }
}

}  // namespace

std::unique_ptr<AuctionRunner> AuctionRunner::CreateAndStart(
    AuctionWorkletManager* auction_worklet_manager,
    InterestGroupManagerImpl* interest_group_manager,
    const blink::AuctionConfig& auction_config,
    network::mojom::ClientSecurityStatePtr client_security_state,
    IsInterestGroupApiAllowedCallback is_interest_group_api_allowed_callback,
    mojo::PendingReceiver<AbortableAdAuction> abort_receiver,
    RunAuctionCallback callback) {
  std::unique_ptr<AuctionRunner> instance(new AuctionRunner(
      auction_worklet_manager, interest_group_manager, DetermineKAnonMode(),
      std::move(auction_config), std::move(client_security_state),
      std::move(is_interest_group_api_allowed_callback),
      std::move(abort_receiver), std::move(callback)));
  instance->StartAuctionIfReady();
  return instance;
}

AuctionRunner::~AuctionRunner() = default;

void AuctionRunner::ResolvedPromiseParam(
    blink::mojom::AuctionAdConfigAuctionIdPtr auction,
    blink::mojom::AuctionAdConfigField field,
    const absl::optional<std::string>& json_value) {
  if (state_ == State::kFailed) {
    return;
  }

  blink::AuctionConfig* config = LookupAuction(*owned_auction_config_, auction);
  if (!config) {
    mojo::ReportBadMessage("Invalid auction ID in ResolvedPromiseParam");
    return;
  }

  blink::AuctionConfig::MaybePromiseJson new_val =
      FromOptionalString(json_value);

  switch (field) {
    case blink::mojom::AuctionAdConfigField::kAuctionSignals:
      if (!config->non_shared_params.auction_signals.is_promise()) {
        mojo::ReportBadMessage("ResolvedPromiseParam updating non-promise");
        return;
      }
      config->non_shared_params.auction_signals = std::move(new_val);
      --promise_fields_in_auction_config_;
      break;

    case blink::mojom::AuctionAdConfigField::kSellerSignals:
      if (!config->non_shared_params.seller_signals.is_promise()) {
        mojo::ReportBadMessage("ResolvedPromiseParam updating non-promise");
        return;
      }
      config->non_shared_params.seller_signals = std::move(new_val);
      --promise_fields_in_auction_config_;
      break;
  }
  StartAuctionIfReady();
}

void AuctionRunner::Abort() {
  // Don't abort if the auction already finished (either as success or failure;
  // this includes the case of multiple promise arguments rejecting).
  if (state_ != State::kFailed && state_ != State::kSucceeded) {
    FailAuction(/*manually_aborted=*/true);
  }
}

void AuctionRunner::FailAuction(
    bool manually_aborted,
    blink::InterestGroupSet interest_groups_that_bid) {
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
      this, manually_aborted,
      /*winning_group_key=*/absl::nullopt,
      /*render_url=*/absl::nullopt,
      /*ad_component_urls=*/{},
      /*winning_group_ad_metadata=*/std::string(),
      std::move(debug_loss_report_urls), std::move(debug_win_report_urls),
      auction_.TakePrivateAggregationRequests(),
      std::move(interest_groups_that_bid), auction_.GetKAnonKeysToJoin(),
      auction_.TakeErrors(), /*reporter=*/nullptr);
}

AuctionRunner::AuctionRunner(
    AuctionWorkletManager* auction_worklet_manager,
    InterestGroupManagerImpl* interest_group_manager,
    auction_worklet::mojom::KAnonymityBidMode kanon_mode,
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
      kanon_mode_(kanon_mode),
      owned_auction_config_(
          std::make_unique<blink::AuctionConfig>(auction_config)),
      callback_(std::move(callback)),
      promise_fields_in_auction_config_(
          owned_auction_config_->non_shared_params.NumPromises()),
      auction_(kanon_mode_,
               owned_auction_config_.get(),
               /*parent=*/nullptr,
               auction_worklet_manager,
               interest_group_manager,
               /*auction_start_time=*/base::Time::Now()) {}

void AuctionRunner::StartAuctionIfReady() {
  if (promise_fields_in_auction_config_ > 0) {
    return;
  }
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
  DCHECK(callback_);

  blink::InterestGroupSet interest_groups_that_bid;
  auction_.GetInterestGroupsThatBid(interest_groups_that_bid);
  if (!success) {
    FailAuction(/*manually_aborted=*/false,
                std::move(interest_groups_that_bid));
    return;
  }

  std::string winning_group_ad_metadata;
  if (auction_.top_bid()->bid->bid_ad->metadata) {
    //`metadata` is already in JSON so no quotes are needed.
    winning_group_ad_metadata = base::StringPrintf(
        R"({"render_url":"%s","metadata":%s})",
        auction_.top_bid()->bid->render_url.spec().c_str(),
        auction_.top_bid()->bid->bid_ad->metadata.value().c_str());
  } else {
    winning_group_ad_metadata =
        base::StringPrintf(R"({"render_url":"%s"})",
                           auction_.top_bid()->bid->render_url.spec().c_str());
  }

  DCHECK(auction_.top_bid()->bid->interest_group);
  const blink::InterestGroup& winning_group =
      *auction_.top_bid()->bid->interest_group;
  blink::InterestGroupKey winning_group_key(
      {winning_group.owner, winning_group.name});

  std::vector<GURL> debug_win_report_urls;
  std::vector<GURL> debug_loss_report_urls;
  auction_.TakeDebugReportUrls(debug_win_report_urls, debug_loss_report_urls);

  UpdateInterestGroupsPostAuction();

  auto errors = auction_.TakeErrors();

  std::unique_ptr<InterestGroupAuctionReporter> reporter =
      auction_.CreateReporter(std::move(owned_auction_config_));
  DCHECK(reporter);

  state_ = State::kSucceeded;
  std::move(callback_).Run(
      this, /*manually_aborted=*/false, std::move(winning_group_key),
      auction_.top_bid()->bid->render_url,
      auction_.top_bid()->bid->ad_components,
      std::move(winning_group_ad_metadata), std::move(debug_loss_report_urls),
      std::move(debug_win_report_urls),
      // In this case, the reporter has all the private aggregation requests.
      std::map<url::Origin, PrivateAggregationRequests>(),
      std::move(interest_groups_that_bid), auction_.GetKAnonKeysToJoin(),
      std::move(errors), std::move(reporter));
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
