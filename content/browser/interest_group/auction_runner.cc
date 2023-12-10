// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_runner.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "content/browser/interest_group/auction_metrics_recorder.h"
#include "content/browser/interest_group/auction_nonce_manager.h"
#include "content/browser/interest_group/interest_group_auction_reporter.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
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
  // K-anonymity enforcement is always disabled for the testing population.
  if (base::FeatureList::IsEnabled(
          features::kCookieDeprecationFacilitatedTesting)) {
    return auction_worklet::mojom::KAnonymityBidMode::kNone;
  }
  if (base::FeatureList::IsEnabled(
          blink::features::kFledgeConsiderKAnonymity)) {
    if (base::FeatureList::IsEnabled(
            blink::features::kFledgeEnforceKAnonymity)) {
      return auction_worklet::mojom::KAnonymityBidMode::kEnforce;
    } else {
      return auction_worklet::mojom::KAnonymityBidMode::kSimulate;
    }
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
    AuctionNonceManager* auction_nonce_manager,
    InterestGroupManagerImpl* interest_group_manager,
    BrowserContext* browser_context,
    PrivateAggregationManager* private_aggregation_manager,
    AdAuctionPageData* ad_auction_page_data,
    InterestGroupAuctionReporter::LogPrivateAggregationRequestsCallback
        log_private_aggregation_requests_callback,
    const blink::AuctionConfig& auction_config,
    const url::Origin& main_frame_origin,
    const url::Origin& frame_origin,
    ukm::SourceId ukm_source_id,
    network::mojom::ClientSecurityStatePtr client_security_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    IsInterestGroupApiAllowedCallback is_interest_group_api_allowed_callback,
    AreReportingOriginsAttestedCallback attestation_callback,
    mojo::PendingReceiver<AbortableAdAuction> abort_receiver,
    RunAuctionCallback callback) {
  std::unique_ptr<AuctionRunner> instance(new AuctionRunner(
      auction_worklet_manager, auction_nonce_manager, interest_group_manager,
      browser_context, private_aggregation_manager, ad_auction_page_data,
      std::move(log_private_aggregation_requests_callback),
      DetermineKAnonMode(), std::move(auction_config), main_frame_origin,
      frame_origin, ukm_source_id, std::move(client_security_state),
      std::move(url_loader_factory),
      std::move(is_interest_group_api_allowed_callback),
      std::move(attestation_callback), std::move(abort_receiver),
      std::move(callback)));
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

void AuctionRunner::ResolvedDirectFromSellerSignalsHeaderAdSlotPromise(
    blink::mojom::AuctionAdConfigAuctionIdPtr auction_id,
    const absl::optional<std::string>&
        direct_from_seller_signals_header_ad_slot) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kFledgeDirectFromSellerSignalsHeaderAdSlot)) {
    mojo::ReportBadMessage(
        "ResolvedDirectFromSellerSignalsHeaderAdSlot with "
        "FledgeDirectFromSellerSignalsHeaderAdSlot off");
    return;
  }

  if (state_ == State::kFailed) {
    return;
  }

  blink::AuctionConfig* config =
      LookupAuction(*owned_auction_config_, auction_id);
  if (!config) {
    mojo::ReportBadMessage(
        "Invalid auction ID in ResolvedDirectFromSellerSignalsHeaderAdSlot");
    return;
  }

  if (!config->expects_direct_from_seller_signals_header_ad_slot) {
    mojo::ReportBadMessage(
        "ResolvedDirectFromSellerSignalsHeaderAdSlot updating non-promise");
    return;
  }

  AdAuctionPageData* page_data = ad_auction_page_data_.get();
  // The `page_data` shouldn't be null (since we create it before starting the
  // auction), but if it is, the auction will just pass default-constructed
  // signals to worklets.

  if (auction_id->is_main_auction()) {
    auction_.NotifyDirectFromSellerSignalsHeaderAdSlotConfig(
        page_data, std::move(direct_from_seller_signals_header_ad_slot));
  } else {
    auction_.NotifyComponentDirectFromSellerSignalsHeaderAdSlotConfig(
        auction_id->get_component_auction(), page_data,
        std::move(direct_from_seller_signals_header_ad_slot));
  }

  config->expects_direct_from_seller_signals_header_ad_slot = false;
  NotifyPromiseResolved(auction_id.get(), config);
}

void AuctionRunner::ResolvedAuctionAdResponsePromise(
    blink::mojom::AuctionAdConfigAuctionIdPtr auction_id,
    mojo_base::BigBuffer response) {
  if (state_ == State::kFailed) {
    return;
  }
  blink::AuctionConfig* config =
      LookupAuction(*owned_auction_config_, auction_id);
  if (!config) {
    mojo::ReportBadMessage(
        "Invalid auction ID in ResolvedAuctionAdResponsePromise");
    return;
  }
  // If we aren't in B&A mode or we already have the response it's an error.
  if (!config->server_response ||
      (config->server_response && config->server_response->got_response)) {
    mojo::ReportBadMessage(
        "ResolvedAuctionAdResponsePromise updating non-promise");
    return;
  }
  config->server_response->got_response = true;
  AdAuctionPageData* page_data = ad_auction_page_data_.get();
  if (!page_data) {
    // There's no page data attached so we can't decode the response. There's
    // no way the auction can proceed.
    FailAuction(false);
    return;
  }

  if (auction_id->is_main_auction()) {
    auction_.HandleServerResponse(std::move(response), page_data);
  } else {
    auction_.HandleComponentServerResponse(auction_id->get_component_auction(),
                                           std::move(response), page_data);
  }
}

void AuctionRunner::ResolvedAdditionalBids(
    blink::mojom::AuctionAdConfigAuctionIdPtr auction_id) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kFledgeNegativeTargeting)) {
    mojo::ReportBadMessage(
        "ResolvedAdditionalBids with FledgeNegativeTargeting off");
    return;
  }

  if (state_ == State::kFailed) {
    return;
  }

  blink::AuctionConfig* config =
      LookupAuction(*owned_auction_config_, auction_id);
  if (!config) {
    mojo::ReportBadMessage("Invalid auction ID in ResolvedAdditionalBids");
    return;
  }

  if (!config->expects_additional_bids) {
    mojo::ReportBadMessage("ResolvedAdditionalBids updating non-promise");
    return;
  }

  config->expects_additional_bids = false;

  AdAuctionPageData* page_data = ad_auction_page_data_.get();
  if (auction_id->is_main_auction()) {
    auction_.NotifyAdditionalBidsConfig(page_data);
  } else {
    auction_.NotifyComponentAdditionalBidsConfig(
        auction_id->get_component_auction(), page_data);
  }

  NotifyPromiseResolved(auction_id.get(), config);
}

void AuctionRunner::Abort() {
  // Don't abort if the auction already finished (either as success or failure;
  // this includes the case of multiple promise arguments rejecting).
  if (state_ != State::kFailed && state_ != State::kSucceeded) {
    FailAuction(/*aborted_by_script=*/true);
  }
}

void AuctionRunner::FailAuction(
    bool aborted_by_script,
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

  if (!aborted_by_script) {
    interest_group_manager_->RegisterAdKeysAsJoined(
        auction_.GetKAnonKeysToJoin());
    interest_group_manager_->EnqueueReports(
        InterestGroupManagerImpl::ReportType::kDebugLoss,
        std::move(debug_loss_report_urls),
        FrameTreeNode::kFrameTreeNodeInvalidId, frame_origin_,
        *client_security_state_, url_loader_factory_);

    InterestGroupAuctionReporter::OnFledgePrivateAggregationRequests(
        private_aggregation_manager_, main_frame_origin_,
        auction_.TakeReservedPrivateAggregationRequests());
  }

  interest_group_manager_->RecordInterestGroupBids(interest_groups_that_bid);

  UpdateInterestGroupsPostAuction();

  // When the auction fails, private aggregation requests of non-reserved event
  // types cannot be triggered anyway, so no need to pass it along.
  std::move(callback_).Run(this, aborted_by_script,
                           /*winning_group_key=*/absl::nullopt,
                           /*requested_ad_size=*/absl::nullopt,
                           /*ad_descriptor=*/absl::nullopt,
                           /*ad_component_descriptors=*/{},
                           auction_.TakeErrors(),
                           /*reporter=*/nullptr);
}

AuctionRunner::AuctionRunner(
    AuctionWorkletManager* auction_worklet_manager,
    AuctionNonceManager* auction_nonce_manager,
    InterestGroupManagerImpl* interest_group_manager,
    BrowserContext* browser_context,
    PrivateAggregationManager* private_aggregation_manager,
    AdAuctionPageData* ad_auction_page_data,
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
    AreReportingOriginsAttestedCallback attestation_callback,
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
          std::move(is_interest_group_api_allowed_callback)),
      ad_auction_page_data_(ad_auction_page_data),
      attestation_callback_(attestation_callback),
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
               auction_nonce_manager,
               interest_group_manager,
               &auction_metrics_recorder_,
               ad_auction_page_data,
               /*auction_start_time=*/base::Time::Now(),
               is_interest_group_api_allowed_callback_,
               std::move(log_private_aggregation_requests_callback)) {}

void AuctionRunner::StartAuction() {
  if (owned_auction_config_->server_response) {
    // Entire auction is running server-side, so skip interest group loading.
    state_ = State::kBiddingAndScoringPhase;
    auction_.StartBiddingAndScoringPhase(
        /*on_seller_receiver_callback=*/base::OnceClosure(),
        base::BindOnce(&AuctionRunner::OnBidsGeneratedAndScored,
                       base::Unretained(this), base::TimeTicks::Now()));
    return;
  }
  auction_.StartLoadInterestGroupsPhase(base::BindOnce(
      &AuctionRunner::OnLoadInterestGroupsComplete, base::Unretained(this)));
}

void AuctionRunner::OnLoadInterestGroupsComplete(bool success) {
  if (!success) {
    FailAuction(/*aborted_by_script=*/false);
    return;
  }

  state_ = State::kBiddingAndScoringPhase;
  auction_.StartBiddingAndScoringPhase(
      /*on_seller_receiver_callback=*/base::OnceClosure(),
      base::BindOnce(&AuctionRunner::OnBidsGeneratedAndScored,
                     base::Unretained(this), base::TimeTicks::Now()));
}

void AuctionRunner::OnBidsGeneratedAndScored(base::TimeTicks start_time,
                                             bool success) {
  DCHECK(callback_);
  bool is_server_auction = owned_auction_config_->server_response.has_value();

  blink::InterestGroupSet interest_groups_that_bid;
  auction_.GetInterestGroupsThatBidAndReportBidCounts(interest_groups_that_bid);
  if (!success) {
    FailAuction(/*aborted_by_script=*/false,
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
  // Need this before `CreateReporter()` since the reporter takes over
  // AuctonConfig.
  auto requested_ad_size = auction_.RequestedAdSize();

  std::unique_ptr<InterestGroupAuctionReporter> reporter =
      auction_.CreateReporter(
          browser_context_, private_aggregation_manager_, url_loader_factory_,
          std::move(owned_auction_config_), main_frame_origin_, frame_origin_,
          client_security_state_.Clone(), std::move(interest_groups_that_bid));
  DCHECK(reporter);

  if (is_server_auction) {
    base::UmaHistogramTimes(
        "Ads.InterestGroup.Auction.ParseBaServerResponseDuration",
        base::TimeTicks::Now() - start_time);
  }

  state_ = State::kSucceeded;
  std::move(callback_).Run(
      this, /*aborted_by_script=*/false, std::move(winning_group_key),
      std::move(requested_ad_size), auction_.top_bid()->bid->ad_descriptor,
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
      update_owners, client_security_state_.Clone(), attestation_callback_);
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
