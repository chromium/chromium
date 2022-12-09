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

  // Most kinds of failures here mean that the auction could not complete at
  // all (such as seller worklet process crashing, page getting unloaded, etc.),
  // but there plain not being a winner gets reported as a failure, too. In
  // that case, however, it's possible that there would be a winner with or
  // without k-anon enforcement (depending on the kanon_mode) that needs to be
  // reported to the client.
  absl::optional<InterestGroupAuction::AuctionResult> result =
      auction_.final_auction_result();
  bool may_have_valid_kanon_info =
      result &&
      (*result == InterestGroupAuction::AuctionResult::kAllBidsRejected ||
       *result == InterestGroupAuction::AuctionResult::kNoBids);

  bool report_kanon_enforce =
      !manually_aborted && may_have_valid_kanon_info &&
      (kanon_mode_ == auction_worklet::mojom::KAnonymityBidMode::kEnforce);
  bool report_kanon_sim =
      !manually_aborted && may_have_valid_kanon_info &&
      (kanon_mode_ == auction_worklet::mojom::KAnonymityBidMode::kSimulate);
  std::move(callback_).Run(
      this, manually_aborted,
      /*winning_group_key=*/absl::nullopt,
      /*render_url=*/absl::nullopt,
      /*ad_component_urls=*/{},
      /*winning_group_ad_metadata=*/std::string(),
      /*report_urls=*/{}, std::move(debug_loss_report_urls),
      std::move(debug_win_report_urls),
      /*ad_beacon_map=*/{}, auction_.TakePrivateAggregationRequests(),
      std::move(interest_groups_that_bid),
      /*render_url_without_kanon_enforced=*/
      report_kanon_enforce ? auction_.TakeRenderUrlWithoutKAnonEnforced()
                           : absl::nullopt,
      /*ad_component_urls_without_kanon_enforced=*/
      report_kanon_enforce ? auction_.TakeComponentUrlsWithoutKAnonEnforced()
                           : std::vector<GURL>(),
      /*render_url_with_kanon_simulated=*/
      report_kanon_sim ? auction_.TakeSimulatedKAnonRenderUrl() : absl::nullopt,
      /*ad_component_urls_with_kanon_simulated=*/
      report_kanon_sim ? auction_.TakeSimulatedKAnonComponentUrls()
                       : std::vector<GURL>(),
      auction_.TakeErrors());
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
      owned_auction_config_(auction_config),
      callback_(std::move(callback)),
      auction_(kanon_mode_,
               &owned_auction_config_,
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
    FailAuction(/*manually_aborted=*/false,
                std::move(interest_groups_that_bid));
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
    blink::InterestGroupSet interest_groups_that_bid,
    bool success) {
  if (!success) {
    FailAuction(/*manually_aborted=*/false,
                std::move(interest_groups_that_bid));
    return;
  }

  DCHECK(callback_);

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

  // TODO(https://crbug.com/1396068): Only do this if/when the winning ad is
  // loaded in a frame.
  interest_group_manager_->RegisterAdAsWon(
      *auction_.top_bid()->bid->interest_group,
      *auction_.top_bid()->bid->bid_ad);

  std::vector<GURL> debug_win_report_urls;
  std::vector<GURL> debug_loss_report_urls;
  auction_.TakeDebugReportUrls(debug_win_report_urls, debug_loss_report_urls);

  UpdateInterestGroupsPostAuction();

  std::unique_ptr<InterestGroupAuctionReporter> reporter =
      auction_.TakeReporter();
  DCHECK(reporter);

  auto errors = auction_.TakeErrors();
  errors.insert(errors.end(), reporter->errors().begin(),
                reporter->errors().end());

  auto ad_beacon_map = reporter->TakeAdBeaconMap();
  auto report_urls = reporter->TakeReportUrls();
  auto private_aggregation_requests =
      reporter->TakePrivateAggregationRequests();
  reporter.reset();

  state_ = State::kSucceeded;
  bool in_kanon_enforce =
      (kanon_mode_ == auction_worklet::mojom::KAnonymityBidMode::kEnforce);
  bool in_kanon_sim =
      (kanon_mode_ == auction_worklet::mojom::KAnonymityBidMode::kSimulate);
  std::move(callback_).Run(
      this, /*manually_aborted=*/false, std::move(winning_group_key),
      auction_.top_bid()->bid->render_url,
      auction_.top_bid()->bid->ad_components,
      std::move(winning_group_ad_metadata), std::move(report_urls),
      std::move(debug_loss_report_urls), std::move(debug_win_report_urls),
      std::move(ad_beacon_map), std::move(private_aggregation_requests),
      std::move(interest_groups_that_bid),
      /*render_url_without_kanon_enforced=*/
      in_kanon_enforce ? auction_.TakeRenderUrlWithoutKAnonEnforced()
                       : absl::nullopt,
      /*ad_component_urls_without_kanon_enforced=*/
      in_kanon_enforce ? auction_.TakeComponentUrlsWithoutKAnonEnforced()
                       : std::vector<GURL>(),
      /*render_url_with_kanon_simulated=*/
      in_kanon_sim ? auction_.TakeSimulatedKAnonRenderUrl() : absl::nullopt,
      /*ad_component_urls_with_kanon_simulated=*/
      in_kanon_sim ? auction_.TakeSimulatedKAnonComponentUrls()
                   : std::vector<GURL>(),
      std::move(errors));
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
