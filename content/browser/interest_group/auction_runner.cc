// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_runner.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "content/browser/interest_group/auction_metrics_recorder.h"
#include "content/browser/interest_group/interest_group_auction_reporter.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
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

}  // namespace

std::unique_ptr<AuctionRunner> AuctionRunner::CreateAndStart(
    AuctionWorkletManager* auction_worklet_manager,
    InterestGroupManagerImpl* interest_group_manager,
    BrowserContext* browser_context,
    PrivateAggregationManager* private_aggregation_manager,
    InterestGroupAuctionReporter::LogPrivateAggregationRequestsCallback
        log_private_aggregation_requests_callback,
    const blink::AuctionConfig& auction_config,
    const url::Origin& main_frame_origin,
    const url::Origin& frame_origin,
    ukm::SourceId ukm_source_id,
    network::mojom::ClientSecurityStatePtr client_security_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    IsInterestGroupApiAllowedCallback is_interest_group_api_allowed_callback,
    mojo::PendingReceiver<AbortableAdAuction> abort_receiver,
    RunAuctionCallback callback) {
  std::unique_ptr<AuctionRunner> instance(new AuctionRunner(
      auction_worklet_manager, interest_group_manager, browser_context,
      private_aggregation_manager,
      std::move(log_private_aggregation_requests_callback),
      DetermineKAnonMode(), std::move(auction_config), main_frame_origin,
      frame_origin, ukm_source_id, std::move(client_security_state),
      std::move(url_loader_factory),
      std::move(is_interest_group_api_allowed_callback),
      std::move(abort_receiver), std::move(callback)));
  instance->StartAuction();
  return instance;
}

AuctionRunner::~AuctionRunner() = default;

void AuctionRunner::ResolvedPromiseParam(
    blink::mojom::AuctionAdConfigAuctionIdPtr auction_id,
    blink::mojom::AuctionAdConfigField field,
    const absl::optional<std::string>& json_value) {
  if (state_ == State::kFailed) {
    return;
  }

  blink::AuctionConfig* config =
      LookupAuction(*owned_auction_config_, auction_id);
  if (!config) {
    // TODO(morlovich): Abort on these.
    mojo::ReportBadMessage("Invalid auction ID in ResolvedPromiseParam");
    return;
  }

  blink::AuctionConfig::MaybePromiseJson new_val =
      blink::AuctionConfig::MaybePromiseJson::FromValue(json_value);

  switch (field) {
    case blink::mojom::AuctionAdConfigField::kAuctionSignals:
      if (!config->non_shared_params.auction_signals.is_promise()) {
        mojo::ReportBadMessage("ResolvedPromiseParam updating non-promise");
        return;
      }
      config->non_shared_params.auction_signals = std::move(new_val);
      break;

    case blink::mojom::AuctionAdConfigField::kSellerSignals:
      if (!config->non_shared_params.seller_signals.is_promise()) {
        mojo::ReportBadMessage("ResolvedPromiseParam updating non-promise");
        return;
      }
      config->non_shared_params.seller_signals = std::move(new_val);
      break;
  }

  NotifyPromiseResolved(auction_id.get(), config);
}

void AuctionRunner::ResolvedPerBuyerSignalsPromise(
    blink::mojom::AuctionAdConfigAuctionIdPtr auction_id,
    const absl::optional<base::flat_map<url::Origin, std::string>>&
        per_buyer_signals) {
  if (state_ == State::kFailed) {
    return;
  }

  blink::AuctionConfig* config =
      LookupAuction(*owned_auction_config_, auction_id);
  if (!config) {
    mojo::ReportBadMessage(
        "Invalid auction ID in ResolvedPerBuyerSignalsPromise");
    return;
  }

  if (!config->non_shared_params.per_buyer_signals.is_promise()) {
    mojo::ReportBadMessage(
        "ResolvedPerBuyerSignalsPromise updating non-promise");
    return;
  }

  config->non_shared_params.per_buyer_signals =
      blink::AuctionConfig::MaybePromisePerBuyerSignals::FromValue(
          per_buyer_signals);
  NotifyPromiseResolved(auction_id.get(), config);
}

void AuctionRunner::ResolvedBuyerTimeoutsPromise(
    blink::mojom::AuctionAdConfigAuctionIdPtr auction_id,
    blink::mojom::AuctionAdConfigBuyerTimeoutField field,
    const blink::AuctionConfig::BuyerTimeouts& buyer_timeouts) {
  if (state_ == State::kFailed) {
    return;
  }

  blink::AuctionConfig* config =
      LookupAuction(*owned_auction_config_, auction_id);
  if (!config) {
    mojo::ReportBadMessage(
        "Invalid auction ID in ResolvedBuyerTimeoutsPromise");
    return;
  }

  blink::AuctionConfig::MaybePromiseBuyerTimeouts* field_ptr = nullptr;
  switch (field) {
    case blink::mojom::AuctionAdConfigBuyerTimeoutField::kPerBuyerTimeouts:
      field_ptr = &config->non_shared_params.buyer_timeouts;
      break;
    case blink::mojom::AuctionAdConfigBuyerTimeoutField::
        kPerBuyerCumulativeTimeouts:
      field_ptr = &config->non_shared_params.buyer_cumulative_timeouts;
      break;
  }

  if (!field_ptr->is_promise()) {
    mojo::ReportBadMessage("ResolvedBuyerTimeoutsPromise updating non-promise");
    return;
  }

  *field_ptr = blink::AuctionConfig::MaybePromiseBuyerTimeouts::FromValue(
      buyer_timeouts);
  NotifyPromiseResolved(auction_id.get(), config);
}

void AuctionRunner::ResolvedBuyerCurrenciesPromise(
    blink::mojom::AuctionAdConfigAuctionIdPtr auction_id,
    const blink::AuctionConfig::BuyerCurrencies& buyer_currencies) {
  if (state_ == State::kFailed) {
    return;
  }

  blink::AuctionConfig* config =
      LookupAuction(*owned_auction_config_, auction_id);
  if (!config) {
    mojo::ReportBadMessage(
        "Invalid auction ID in ResolvedBuyerCurrenciesPromise");
    return;
  }

  if (!config->non_shared_params.buyer_currencies.is_promise()) {
    mojo::ReportBadMessage(
        "ResolvedBuyerCurrenciesPromise updating non-promise");
    return;
  }

  config->non_shared_params.buyer_currencies =
      blink::AuctionConfig::MaybePromiseBuyerCurrencies::FromValue(
          buyer_currencies);
  NotifyPromiseResolved(auction_id.get(), config);
}

void AuctionRunner::ResolvedDirectFromSellerSignalsPromise(
    blink::mojom::AuctionAdConfigAuctionIdPtr auction_id,
    const absl::optional<blink::DirectFromSellerSignals>&
        direct_from_seller_signals) {
  if (state_ == State::kFailed) {
    return;
  }

  blink::AuctionConfig* config =
      LookupAuction(*owned_auction_config_, auction_id);
  if (!config) {
    mojo::ReportBadMessage(
        "Invalid auction ID in ResolvedDirectFromSellerSignalsPromise");
    return;
  }

  if (!config->direct_from_seller_signals.is_promise()) {
    mojo::ReportBadMessage(
        "ResolvedDirectFromSellerSignalsPromise updating non-promise");
    return;
  }

  if (!config->IsDirectFromSellerSignalsValid(direct_from_seller_signals)) {
    mojo::ReportBadMessage(
        "ResolvedDirectFromSellerSignalsPromise with invalid signals");
    return;
  }

  config->direct_from_seller_signals =
      blink::AuctionConfig::MaybePromiseDirectFromSellerSignals::FromValue(
          direct_from_seller_signals);
  NotifyPromiseResolved(auction_id.get(), config);
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

  // Can have loss report URLs if the auction failed because the seller
  // rejected all bids.
  std::vector<GURL> debug_win_report_urls;
  std::vector<GURL> debug_loss_report_urls;
  auction_.TakeDebugReportUrlsAndFillInPrivateAggregationRequests(
      debug_win_report_urls, debug_loss_report_urls);
  // Shouldn't have any win report URLs if nothing won the auction.
  DCHECK(debug_win_report_urls.empty());

  if (!manually_aborted) {
    interest_group_manager_->RegisterAdKeysAsJoined(
        auction_.GetKAnonKeysToJoin());
    interest_group_manager_->EnqueueReports(
        InterestGroupManagerImpl::ReportType::kDebugLoss,
        std::move(debug_loss_report_urls), frame_origin_,
        *client_security_state_, url_loader_factory_);

    InterestGroupAuctionReporter::OnFledgePrivateAggregationRequests(
        private_aggregation_manager_, main_frame_origin_,
        auction_.TakeReservedPrivateAggregationRequests());
  }

  interest_group_manager_->RecordInterestGroupBids(interest_groups_that_bid);

  UpdateInterestGroupsPostAuction();

  // When the auction fails, private aggregation requests of non-reserved event
  // types cannot be triggered anyway, so no need to pass it along.
  std::move(callback_).Run(this, manually_aborted,
                           /*winning_group_key=*/absl::nullopt,
                           /*requested_ad_size=*/absl::nullopt,
                           /*ad_descriptor=*/absl::nullopt,
                           /*ad_component_descriptors=*/{},
                           auction_.TakeErrors(),
                           /*reporter=*/nullptr);
}

AuctionRunner::AuctionRunner(
    AuctionWorkletManager* auction_worklet_manager,
    InterestGroupManagerImpl* interest_group_manager,
    BrowserContext* browser_context,
    PrivateAggregationManager* private_aggregation_manager,
    InterestGroupAuctionReporter::LogPrivateAggregationRequestsCallback
        log_private_aggregation_requests_callback,
    auction_worklet::mojom::KAnonymityBidMode kanon_mode,
    const blink::AuctionConfig& auction_config,
    const url::Origin& main_frame_origin,
    const url::Origin& frame_origin,
    ukm::SourceId ukm_source_id,
    network::mojom::ClientSecurityStatePtr client_security_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    IsInterestGroupApiAllowedCallback is_interest_group_api_allowed_callback,
    mojo::PendingReceiver<AbortableAdAuction> abort_receiver,
    RunAuctionCallback callback)
    : interest_group_manager_(interest_group_manager),
      browser_context_(browser_context),
      private_aggregation_manager_(private_aggregation_manager),
      main_frame_origin_(main_frame_origin),
      frame_origin_(frame_origin),
      client_security_state_(std::move(client_security_state)),
      url_loader_factory_(std::move(url_loader_factory)),
      is_interest_group_api_allowed_callback_(
          is_interest_group_api_allowed_callback),
      abort_receiver_(this, std::move(abort_receiver)),
      kanon_mode_(kanon_mode),
      owned_auction_config_(
          std::make_unique<blink::AuctionConfig>(auction_config)),
      callback_(std::move(callback)),
      promise_fields_in_auction_config_(owned_auction_config_->NumPromises()),
      auction_metrics_recorder_(ukm_source_id),
      auction_(kanon_mode_,
               owned_auction_config_.get(),
               /*parent=*/nullptr,
               auction_worklet_manager,
               interest_group_manager,
               &auction_metrics_recorder_,
               /*auction_start_time=*/base::Time::Now(),
               std::move(log_private_aggregation_requests_callback)) {}

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
  DCHECK(callback_);

  blink::InterestGroupSet interest_groups_that_bid;
  auction_.GetInterestGroupsThatBidAndReportBidCounts(interest_groups_that_bid);
  if (!success) {
    FailAuction(/*manually_aborted=*/false,
                std::move(interest_groups_that_bid));
    return;
  }

  DCHECK(auction_.top_bid()->bid->interest_group);
  const blink::InterestGroup& winning_group =
      *auction_.top_bid()->bid->interest_group;
  blink::InterestGroupKey winning_group_key(
      {winning_group.owner, winning_group.name});

  UpdateInterestGroupsPostAuction();

  auto errors = auction_.TakeErrors();

  std::unique_ptr<InterestGroupAuctionReporter> reporter =
      auction_.CreateReporter(
          browser_context_, private_aggregation_manager_, url_loader_factory_,
          std::move(owned_auction_config_), main_frame_origin_, frame_origin_,
          client_security_state_.Clone(), std::move(interest_groups_that_bid));
  DCHECK(reporter);

  state_ = State::kSucceeded;
  std::move(callback_).Run(
      this, /*manually_aborted=*/false, std::move(winning_group_key),
      auction_.RequestedAdSize(), auction_.top_bid()->bid->ad_descriptor,
      auction_.top_bid()->bid->ad_component_descriptors, std::move(errors),
      std::move(reporter));
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

void AuctionRunner::NotifyPromiseResolved(
    const blink::mojom::AuctionAdConfigAuctionId* auction_id,
    blink::AuctionConfig* config) {
  --promise_fields_in_auction_config_;
  DCHECK_EQ(promise_fields_in_auction_config_,
            owned_auction_config_->NumPromises());

  if (!auction_id->is_main_auction() && config->NumPromises() == 0) {
    auction_.NotifyComponentConfigPromisesResolved(
        auction_id->get_component_auction());
  }

  // This may happen when updating a component auction as well.
  if (promise_fields_in_auction_config_ == 0) {
    auction_.NotifyConfigPromisesResolved();
  }
}

}  // namespace content
