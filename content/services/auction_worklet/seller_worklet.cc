// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/seller_worklet.h"

#include <stdint.h>

#include <cmath>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "base/types/optional_ref.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/auction_v8_logger.h"
#include "content/services/auction_worklet/auction_worklet_util.h"
#include "content/services/auction_worklet/context_recycler.h"
#include "content/services/auction_worklet/direct_from_seller_signals_requester.h"
#include "content/services/auction_worklet/for_debugging_only_bindings.h"
#include "content/services/auction_worklet/private_aggregation_bindings.h"
#include "content/services/auction_worklet/public/cpp/auction_network_events_delegate.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "content/services/auction_worklet/real_time_reporting_bindings.h"
#include "content/services/auction_worklet/register_ad_beacon_bindings.h"
#include "content/services/auction_worklet/report_bindings.h"
#include "content/services/auction_worklet/seller_lazy_filler.h"
#include "content/services/auction_worklet/shared_storage_bindings.h"
#include "content/services/auction_worklet/trusted_signals.h"
#include "content/services/auction_worklet/webidl_compat.h"
#include "content/services/auction_worklet/worklet_loader.h"
#include "content/services/auction_worklet/worklet_util.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/ad_auction_currencies.h"
#include "third_party/blink/public/common/interest_group/ad_display_size.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-template.h"

namespace auction_worklet {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ScoreAdInput {
  kTrustedSignals = 0,
  kDirectFromSellerSignals = 1,
  kScoringScript = 2,

  kMaxValue = kScoringScript
};

// Checks both types of DirectFromSellerSignals results (subresource bundle
// based and header based) -- at most one of these should be non-null.
//
// Returns the V8 conversion of the in-use version of DirectFromSellerSignals,
// or v8::Null() if both types of DirectFromSellerSignals are null.
v8::Local<v8::Value> GetDirectFromSellerSignals(
    const DirectFromSellerSignalsRequester::Result& subresource_bundle_result,
    const std::optional<std::string>& header_result,
    AuctionV8Helper& v8_helper,
    v8::Local<v8::Context> context,
    std::vector<std::string>& errors) {
  CHECK(subresource_bundle_result.IsNull() || !header_result);

  if (header_result) {
    // `header_result` JSON was validated, parsed and reconstructed into a
    // string by the browser process, so CHECK it is valid JSON.
    return v8_helper.CreateValueFromJson(context, *header_result)
        .ToLocalChecked();
  }

  return subresource_bundle_result.GetSignals(v8_helper, context, errors);
}

// ### some duplication with same in interest_group_auction.cc
bool IsValidBid(double bid) {
  return std::isfinite(bid) && (bid > 0.0);
}

// Converts `auction_config` back to JSON format, and appends to args.
// Returns true if conversion succeeded.
//
// `auction_config_lazy_fillers` is incoming, and is organized as follows:
//   [0] corresponds to the top-level auction.
//   [1] corresponds to the 0th component auction.
//   [2] corresponds to the 1th component auction.
//   ... and so on.
// where `auction_config_lazy_filler_pos` describes the position the current
// invocation is expected to use; e.g. it's 0 for top-level, and i + 1 for
// i'th component auction.
//
// The resulting object will look something like this (based on example from
// explainer):
//
// {
//  'seller': 'https://www.example-ssp.com/',
//  'decisionLogicURL': 'https://www.example-ssp.com/seller.js',
//  'trustedScoringSignalsURL': ...,
//  'interestGroupBuyers': ['https://www.example-dsp.com', 'https://buyer2.com',
//  ...], 'auctionSignals': {...}, 'sellerSignals': {...}, 'sellerTimeout': 100,
//  `reportingTimeout`: 600,
//  'perBuyerSignals': {'https://www.example-dsp.com': {...},
//                      'https://www.another-buyer.com': {...},
//                       ...},
//  'perBuyerTimeouts': {'https://www.example-dsp.com': 50,
//                       'https://www.another-buyer.com': 200,
//                       '*': 150,
//                       ...},
//  'perBuyerPrioritySignals': {'https://www.example-dsp.com': {...},
//                              'https://www.another-buyer.com': {...},
//                              '*': {...},
//                              ...},
// }
//
// (With many fields filled in on-demand by an AuctionConfigLazyFiller).
bool AppendAuctionConfig(
    AuctionV8Helper* v8_helper,
    AuctionV8Logger* v8_logger,
    v8::Local<v8::Context> context,
    const url::Origin& seller,
    base::optional_ref<const GURL> decision_logic_url,
    base::optional_ref<const GURL> trusted_scoring_signals_url,
    const std::optional<uint16_t> experiment_group_id,
    const blink::AuctionConfig::NonSharedParams&
        auction_ad_config_non_shared_params,
    const std::vector<std::unique_ptr<AuctionConfigLazyFiller>>&
        auction_config_lazy_fillers,
    size_t auction_config_lazy_filler_pos,
    v8::LocalVector<v8::Value>* args) {
  v8::Isolate* isolate = v8_helper->isolate();
  v8::Local<v8::Object> auction_config_value = v8::Object::New(isolate);

  gin::Dictionary auction_config_dict(isolate, auction_config_value);
  if (!auction_config_dict.Set("seller", seller.Serialize())) {
    return false;
  }

  auction_config_lazy_fillers[auction_config_lazy_filler_pos]->FillInObject(
      auction_ad_config_non_shared_params, decision_logic_url,
      trusted_scoring_signals_url, auction_config_value);

  // Deprecated decisionLogicUrl is lazily filled by AuctionConfigLazyFiller.
  if (decision_logic_url.has_value()) {
    if (!auction_config_dict.Set("decisionLogicURL",
                                 decision_logic_url->spec())) {
      return false;
    }
  }
  // Deprecated trustedScoringSignalsUrl is lazily filled by
  // AuctionConfigLazyFiller.
  if (trusted_scoring_signals_url.has_value() &&
      !auction_config_dict.Set("trustedScoringSignalsURL",
                               trusted_scoring_signals_url->spec())) {
    return false;
  }

  DCHECK(!auction_ad_config_non_shared_params.auction_signals.is_promise());
  if (auction_ad_config_non_shared_params.auction_signals.value() &&
      !v8_helper->InsertJsonValue(
          context, "auctionSignals",
          *auction_ad_config_non_shared_params.auction_signals.value(),
          auction_config_value)) {
    return false;
  }

  DCHECK(!auction_ad_config_non_shared_params.seller_signals.is_promise());
  if (auction_ad_config_non_shared_params.seller_signals.value() &&
      !v8_helper->InsertJsonValue(
          context, "sellerSignals",
          *auction_ad_config_non_shared_params.seller_signals.value(),
          auction_config_value)) {
    return false;
  }

  if (auction_ad_config_non_shared_params.seller_timeout.has_value() &&
      !auction_config_dict.Set(
          "sellerTimeout",
          auction_ad_config_non_shared_params.seller_timeout.value()
              .InMilliseconds())) {
    return false;
  }

  base::TimeDelta reporting_timeout =
      auction_ad_config_non_shared_params.reporting_timeout.has_value()
          ? *auction_ad_config_non_shared_params.reporting_timeout
          : AuctionV8Helper::kScriptTimeout;

  if (base::FeatureList::IsEnabled(blink::features::kFledgeReportingTimeout) &&
      !auction_config_dict.Set("reportingTimeout",
                               reporting_timeout.InMilliseconds())) {
    return false;
  }

  if (auction_ad_config_non_shared_params.seller_currency.has_value()) {
    auction_config_dict.Set(
        "sellerCurrency",
        auction_ad_config_non_shared_params.seller_currency->currency_code());
  }

  const auto& component_auctions =
      auction_ad_config_non_shared_params.component_auctions;
  if (!component_auctions.empty()) {
    v8::LocalVector<v8::Value> component_auction_vector(isolate);

    for (size_t pos = 0; pos < component_auctions.size(); ++pos) {
      const auto& component_auction = component_auctions[pos];
      if (!AppendAuctionConfig(
              v8_helper, v8_logger, context, component_auction.seller,
              component_auction.decision_logic_url,
              component_auction.trusted_scoring_signals_url,
              experiment_group_id, component_auction.non_shared_params,
              auction_config_lazy_fillers, pos + 1,
              &component_auction_vector)) {
        return false;
      }
    }
    v8::Maybe<bool> result = auction_config_value->Set(
        context, v8_helper->CreateStringFromLiteral("componentAuctions"),
        v8::Array::New(isolate, component_auction_vector.data(),
                       component_auction_vector.size()));
    if (result.IsNothing() || !result.FromJust()) {
      return false;
    }
  }

  if (experiment_group_id.has_value()) {
    auction_config_dict.Set("experimentGroupId",
                            static_cast<unsigned>(experiment_group_id.value()));
  }

  args->push_back(std::move(auction_config_value));
  return true;
}

// Adds the top-level/component seller origin from
// `browser_signals_other_seller` to `browser_signals_dict`. Does nothing if
// `browser_signals_other_seller` is null. Returns false on error.
bool AddOtherSeller(
    mojom::ComponentAuctionOtherSeller* browser_signals_other_seller,
    gin::Dictionary& browser_signals_dict) {
  if (!browser_signals_other_seller) {
    return true;
  }
  if (browser_signals_other_seller->is_top_level_seller()) {
    return browser_signals_dict.Set(
        "topLevelSeller",
        browser_signals_other_seller->get_top_level_seller().Serialize());
  }
  DCHECK(browser_signals_other_seller->is_component_seller());
  return browser_signals_dict.Set(
      "componentSeller",
      browser_signals_other_seller->get_component_seller().Serialize());
}

// Converts reject reason string to corresponding mojom enum.
std::optional<mojom::RejectReason> RejectReasonStringToEnum(
    const std::string& reason) {
  if (reason == "not-available") {
    return mojom::RejectReason::kNotAvailable;
  } else if (reason == "invalid-bid") {
    return mojom::RejectReason::kInvalidBid;
  } else if (reason == "bid-below-auction-floor") {
    return mojom::RejectReason::kBidBelowAuctionFloor;
  } else if (reason == "pending-approval-by-exchange") {
    return mojom::RejectReason::kPendingApprovalByExchange;
  } else if (reason == "disapproved-by-exchange") {
    return mojom::RejectReason::kDisapprovedByExchange;
  } else if (reason == "blocked-by-publisher") {
    return mojom::RejectReason::kBlockedByPublisher;
  } else if (reason == "language-exclusions") {
    return mojom::RejectReason::kLanguageExclusions;
  } else if (reason == "category-exclusions") {
    return mojom::RejectReason::kCategoryExclusions;
  }
  // Invalid (out of range) reject reason.
  return std::nullopt;
}

// Checks `provided_currency` against both `expected_seller_currency` and
// `component_expect_bid_currency`, formatting an error if needed, with
// `bid_label` identifying the bid being checked.
// Returns true on success.
bool VerifySellerCurrency(
    std::optional<blink::AdCurrency> provided_currency,
    std::optional<blink::AdCurrency> expected_seller_currency,
    std::optional<blink::AdCurrency> component_expect_bid_currency,
    const GURL& script_url,
    std::string_view bid_label,
    std::vector<std::string>& errors_out) {
  if (!blink::VerifyAdCurrencyCode(expected_seller_currency,
                                   provided_currency)) {
    errors_out.push_back(base::StrCat(
        {script_url.spec(), " scoreAd() ", bid_label,
         " mismatch vs own sellerCurrency, expected '",
         blink::PrintableAdCurrency(expected_seller_currency), "' got '",
         blink::PrintableAdCurrency(provided_currency), "'."}));
    return false;
  }
  if (!blink::VerifyAdCurrencyCode(component_expect_bid_currency,
                                   provided_currency)) {
    errors_out.push_back(base::StrCat(
        {script_url.spec(), " scoreAd() ", bid_label,
         " mismatch in component auction "
         "vs parent auction bidderCurrency, expected '",
         blink::PrintableAdCurrency(component_expect_bid_currency), "' got '",
         blink::PrintableAdCurrency(provided_currency), "'."}));
    return false;
  }
  return true;
}

std::optional<base::TimeDelta> NullOptIfZero(base::TimeDelta delta) {
  if (delta.is_zero()) {
    return std::nullopt;
  }
  return delta;
}

// Check if trusted scoring signals are absent, same-origin, or cross-origin.
SellerWorklet::SignalsOriginRelation ClassifyTrustedSignals(
    const GURL& decision_logic_url,
    const std::optional<url::Origin>& trusted_scoring_signals_origin) {
  if (!trusted_scoring_signals_origin.has_value()) {
    return SellerWorklet::SignalsOriginRelation::kNoTrustedSignals;
  }

  if (trusted_scoring_signals_origin->IsSameOriginWith(decision_logic_url)) {
    return SellerWorklet::SignalsOriginRelation::kSameOriginSignals;
  }

  return SellerWorklet::SignalsOriginRelation::
      kUnknownPermissionCrossOriginSignals;
}

// Sets the appropriate field (if any) of `browser_signals` to data version,
// considering the cross-origin validity.
// Returns success/failure.
bool SetDataVersion(
    SellerWorklet::SignalsOriginRelation trusted_signals_relation,
    std::optional<uint32_t> scoring_signals_data_version,
    gin::Dictionary& browser_signals_dict) {
  if (!scoring_signals_data_version.has_value()) {
    return true;
  }

  switch (trusted_signals_relation) {
    case SellerWorklet::SignalsOriginRelation::kNoTrustedSignals:
      return true;

    case SellerWorklet::SignalsOriginRelation::kSameOriginSignals:
      return browser_signals_dict.Set("dataVersion",
                                      scoring_signals_data_version.value());

    case SellerWorklet::SignalsOriginRelation::
        kUnknownPermissionCrossOriginSignals:
      // This should be turned into permitted or forbidden by now.
      CHECK(false);
      return false;

    case SellerWorklet::SignalsOriginRelation::kPermittedCrossOriginSignals:
      return browser_signals_dict.Set("crossOriginDataVersion",
                                      scoring_signals_data_version.value());

    case SellerWorklet::SignalsOriginRelation::kForbiddenCrossOriginSignals:
      // We shouldn't have a fetch to get a version from if it's forbidden.
      CHECK(false);
      return false;
  }
}

// Remove worklet latency contributions if the worklet execution time is
// within the threshold.
std::vector<auction_worklet::mojom::RealTimeReportingContributionPtr>
FilterRealtimeContributions(
    std::vector<auction_worklet::mojom::RealTimeReportingContributionPtr>
        real_time_contributions,
    base::TimeDelta elapsed) {
  std::erase_if(
      real_time_contributions,
      [elapsed](const auction_worklet::mojom::RealTimeReportingContributionPtr&
                    contribution) {
        return contribution->latency_threshold.has_value() &&
               elapsed.InMilliseconds() <=
                   contribution->latency_threshold.value();
      });
  return real_time_contributions;
}

}  // namespace

SellerWorklet::SellerWorklet(
    std::vector<scoped_refptr<AuctionV8Helper>> v8_helpers,
    std::vector<mojo::PendingRemote<mojom::AuctionSharedStorageHost>>
        shared_storage_hosts,
    bool pause_for_debugger_on_start,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_url_loader_factory,
    mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
        auction_network_events_handler,
    const GURL& decision_logic_url,
    const std::optional<GURL>& trusted_scoring_signals_url,
    const url::Origin& top_window_origin,
    mojom::AuctionWorkletPermissionsPolicyStatePtr permissions_policy_state,
    std::optional<uint16_t> experiment_group_id,
    mojom::TrustedSignalsPublicKeyPtr public_key,
    GetNextThreadIndexCallback get_next_thread_index_callback)
    : url_loader_factory_(std::move(pending_url_loader_factory)),
      script_source_url_(decision_logic_url),
      trusted_scoring_signals_origin_(
          trusted_scoring_signals_url ? std::make_optional(url::Origin::Create(
                                            *trusted_scoring_signals_url))
                                      : std::nullopt),
      auction_network_events_handler_(
          std::move(auction_network_events_handler)),
      get_next_thread_index_callback_(
          std::move(get_next_thread_index_callback)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  DCHECK(!v8_helpers.empty());
  DCHECK_EQ(v8_helpers.size(), shared_storage_hosts.size());

  for (size_t i = 0; i < v8_helpers.size(); ++i) {
    v8_runners_.push_back(v8_helpers[i]->v8_runner());
    v8_helpers_.push_back(std::move(v8_helpers[i]));
    debug_ids_.push_back(
        base::MakeRefCounted<AuctionV8Helper::DebugId>(v8_helpers_[i].get()));
    v8_state_.push_back(std::unique_ptr<V8State, base::OnTaskRunnerDeleter>(
        new V8State(v8_helpers_[i], debug_ids_[i],
                    std::move(shared_storage_hosts[i]), decision_logic_url,
                    trusted_scoring_signals_url,
                    trusted_scoring_signals_origin_, top_window_origin,
                    permissions_policy_state->Clone(), experiment_group_id,
                    weak_ptr_factory_.GetWeakPtr()),
        base::OnTaskRunnerDeleter(v8_runners_[i])));
  }

  trusted_signals_request_manager_ =
      (trusted_scoring_signals_url
           ? std::make_unique<TrustedSignalsRequestManager>(
                 TrustedSignalsRequestManager::Type::kScoringSignals,
                 url_loader_factory_.get(),
                 /*auction_network_events_handler=*/
                 CreateNewAuctionNetworkEventsHandlerRemote(
                     auction_network_events_handler_),
                 /*automatically_send_requests=*/true, top_window_origin,
                 *trusted_scoring_signals_url,
                 /*experiment_group_id=*/experiment_group_id,
                 /*trusted_bidding_signals_slot_size_param=*/std::string(),
                 std::move(public_key),
                 v8_helpers_[get_next_thread_index_callback_.Run()].get())
           : nullptr);
  trusted_signals_relation_ = ClassifyTrustedSignals(
      script_source_url_, trusted_scoring_signals_origin_);

  paused_ = pause_for_debugger_on_start;
  if (!paused_) {
    Start();
  }
}

SellerWorklet::~SellerWorklet() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  for (const auto& debug_id : debug_ids_) {
    debug_id->AbortDebuggerPauses();
  }
}

std::vector<int> SellerWorklet::context_group_ids_for_testing() const {
  std::vector<int> results;
  for (const auto& debug_id : debug_ids_) {
    results.push_back(debug_id->context_group_id());
  }
  return results;
}

void SellerWorklet::ScoreAd(
    const std::string& ad_metadata_json,
    double bid,
    const std::optional<blink::AdCurrency>& bid_currency,
    const blink::AuctionConfig::NonSharedParams&
        auction_ad_config_non_shared_params,
    const std::optional<GURL>& direct_from_seller_seller_signals,
    const std::optional<std::string>&
        direct_from_seller_seller_signals_header_ad_slot,
    const std::optional<GURL>& direct_from_seller_auction_signals,
    const std::optional<std::string>&
        direct_from_seller_auction_signals_header_ad_slot,
    mojom::ComponentAuctionOtherSellerPtr browser_signals_other_seller,
    const std::optional<blink::AdCurrency>& component_expect_bid_currency,
    const url::Origin& browser_signal_interest_group_owner,
    const GURL& browser_signal_render_url,
    const std::optional<std::string>&
        browser_signal_selected_buyer_and_seller_reporting_id,
    const std::optional<std::string>&
        browser_signal_buyer_and_seller_reporting_id,
    const std::vector<GURL>& browser_signal_ad_components,
    uint32_t browser_signal_bidding_duration_msecs,
    const std::optional<blink::AdSize>& browser_signal_render_size,
    bool browser_signal_for_debugging_only_in_cooldown_or_lockout,
    const std::optional<base::TimeDelta> seller_timeout,
    uint64_t trace_id,
    const url::Origin& bidder_joining_origin,
    mojo::PendingRemote<auction_worklet::mojom::ScoreAdClient>
        score_ad_client) {
  CHECK((!direct_from_seller_seller_signals &&
         !direct_from_seller_auction_signals) ||
        (!direct_from_seller_seller_signals_header_ad_slot &&
         !direct_from_seller_auction_signals_header_ad_slot));
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  base::UmaHistogramCounts1000(
      "Ads.InterestGroup.Auction.NumberOfPendingScoreAdTasks",
      score_ad_tasks_.size());
  score_ad_tasks_.emplace_front();

  auto score_ad_task = score_ad_tasks_.begin();
  score_ad_task->ad_metadata_json = ad_metadata_json;
  score_ad_task->bid = bid;
  score_ad_task->bid_currency = bid_currency;
  score_ad_task->auction_ad_config_non_shared_params =
      auction_ad_config_non_shared_params;
  score_ad_task->browser_signals_other_seller =
      std::move(browser_signals_other_seller);
  score_ad_task->component_expect_bid_currency = component_expect_bid_currency;
  score_ad_task->browser_signal_interest_group_owner =
      browser_signal_interest_group_owner;
  score_ad_task->bidder_joining_origin = bidder_joining_origin;
  score_ad_task->browser_signal_render_url = browser_signal_render_url;
  score_ad_task->browser_signal_selected_buyer_and_seller_reporting_id =
      browser_signal_selected_buyer_and_seller_reporting_id;
  score_ad_task->browser_signal_buyer_and_seller_reporting_id =
      browser_signal_buyer_and_seller_reporting_id;
  for (const GURL& url : browser_signal_ad_components) {
    score_ad_task->browser_signal_ad_components.emplace_back(url.spec());
  }
  score_ad_task->browser_signal_bidding_duration_msecs =
      browser_signal_bidding_duration_msecs;
  score_ad_task->browser_signal_render_size = browser_signal_render_size;
  score_ad_task->browser_signal_for_debugging_only_in_cooldown_or_lockout =
      browser_signal_for_debugging_only_in_cooldown_or_lockout;
  score_ad_task->seller_timeout = seller_timeout;
  score_ad_task->trace_id = trace_id;
  score_ad_task->score_ad_client.Bind(std::move(score_ad_client));

  // Deleting `score_ad_task` will destroy `score_ad_client` and thus
  // abort this callback, so it's safe to use Unretained(this) and
  // `score_ad_task` here.
  score_ad_task->score_ad_client.set_disconnect_handler(
      base::BindOnce(&SellerWorklet::OnScoreAdClientDestroyed,
                     base::Unretained(this), score_ad_task));

  if (direct_from_seller_seller_signals) {
    // Deleting `score_ad_task` will destroy
    // `direct_from_seller_request_seller_signals` and thus abort this callback,
    // so it's safe to use Unretained(this) and `score_ad_task` here.
    score_ad_task->direct_from_seller_request_seller_signals =
        direct_from_seller_requester_seller_signals_.LoadSignals(
            *url_loader_factory_, *direct_from_seller_seller_signals,
            base::BindOnce(&SellerWorklet::
                               OnDirectFromSellerSellerSignalsDownloadedScoreAd,
                           base::Unretained(this), score_ad_task));
  } else {
    score_ad_task->direct_from_seller_result_seller_signals =
        DirectFromSellerSignalsRequester::Result();
  }

  if (direct_from_seller_auction_signals) {
    // Deleting `score_ad_task` will destroy
    // `direct_from_seller_request_auction_signals` and thus abort this
    // callback, so it's safe to use Unretained(this) and `score_ad_task` here.
    score_ad_task->direct_from_seller_request_auction_signals =
        direct_from_seller_requester_auction_signals_.LoadSignals(
            *url_loader_factory_, *direct_from_seller_auction_signals,
            base::BindOnce(
                &SellerWorklet::
                    OnDirectFromSellerAuctionSignalsDownloadedScoreAd,
                base::Unretained(this), score_ad_task));
  } else {
    score_ad_task->direct_from_seller_result_auction_signals =
        DirectFromSellerSignalsRequester::Result();
  }
  score_ad_task->direct_from_seller_seller_signals_header_ad_slot =
      direct_from_seller_seller_signals_header_ad_slot;
  score_ad_task->direct_from_seller_auction_signals_header_ad_slot =
      direct_from_seller_auction_signals_header_ad_slot;

  score_ad_task->trace_wait_deps_start = base::TimeTicks::Now();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "wait_score_ad_deps", trace_id);

  // If `trusted_signals_request_manager_` exists, there's a trusted scoring
  // signals URL which needs to be fetched before the auction can be run.
  if (trusted_signals_request_manager_) {
    // Can only start fetching trusted seller signals if they're same-origin or
    // we have confirmation they are authorized by guaranteed-same-origin
    // script, as otherwise we may end up sending sensitive IG information to an
    // unrelated third party.
    if (trusted_signals_relation_ !=
        SignalsOriginRelation::kUnknownPermissionCrossOriginSignals) {
      StartFetchingSignalsForTask(score_ad_task);
    } else {
      if (!first_deferred_trusted_signals_time_.has_value()) {
        first_deferred_trusted_signals_time_ = base::TimeTicks::Now();
      }
    }
    return;
  }

  ScoreAdIfReady(score_ad_task);
}

void SellerWorklet::SendPendingSignalsRequests() {
  if (trusted_signals_request_manager_) {
    trusted_signals_request_manager_->StartBatchedTrustedSignalsRequest();
  }
}

void SellerWorklet::ReportResult(
    const blink::AuctionConfig::NonSharedParams&
        auction_ad_config_non_shared_params,
    const std::optional<GURL>& direct_from_seller_seller_signals,
    const std::optional<std::string>&
        direct_from_seller_seller_signals_header_ad_slot,
    const std::optional<GURL>& direct_from_seller_auction_signals,
    const std::optional<std::string>&
        direct_from_seller_auction_signals_header_ad_slot,
    mojom::ComponentAuctionOtherSellerPtr browser_signals_other_seller,
    const url::Origin& browser_signal_interest_group_owner,
    const std::optional<std::string>&
        browser_signal_buyer_and_seller_reporting_id,
    const std::optional<std::string>&
        browser_signal_selected_buyer_and_seller_reporting_id,
    const GURL& browser_signal_render_url,
    double browser_signal_bid,
    const std::optional<blink::AdCurrency>& browser_signal_bid_currency,
    double browser_signal_desirability,
    double browser_signal_highest_scoring_other_bid,
    const std::optional<blink::AdCurrency>&
        browser_signal_highest_scoring_other_bid_currency,
    auction_worklet::mojom::ComponentAuctionReportResultParamsPtr
        browser_signals_component_auction_report_result_params,
    std::optional<uint32_t> scoring_signals_data_version,
    uint64_t trace_id,
    ReportResultCallback callback) {
  CHECK((!direct_from_seller_seller_signals &&
         !direct_from_seller_auction_signals) ||
        (!direct_from_seller_seller_signals_header_ad_slot &&
         !direct_from_seller_auction_signals_header_ad_slot));
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  // `browser_signals_component_auction_report_result_params` should only be
  // populated for sellers in component auctions, which are the only case where
  // `browser_signals_other_seller` is a top-level seller.
  DCHECK_EQ(browser_signals_other_seller &&
                browser_signals_other_seller->is_top_level_seller(),
            !browser_signals_component_auction_report_result_params.is_null());

  report_result_tasks_.emplace_front();

  auto report_result_task = report_result_tasks_.begin();

  report_result_task->auction_ad_config_non_shared_params =
      auction_ad_config_non_shared_params;
  report_result_task->browser_signals_other_seller =
      std::move(browser_signals_other_seller);
  report_result_task->browser_signal_interest_group_owner =
      browser_signal_interest_group_owner;
  report_result_task->browser_signal_buyer_and_seller_reporting_id =
      browser_signal_buyer_and_seller_reporting_id;
  report_result_task->browser_signal_selected_buyer_and_seller_reporting_id =
      browser_signal_selected_buyer_and_seller_reporting_id;
  report_result_task->browser_signal_render_url = browser_signal_render_url;
  report_result_task->browser_signal_bid = browser_signal_bid;
  report_result_task->browser_signal_bid_currency =
      std::move(browser_signal_bid_currency);
  report_result_task->browser_signal_desirability = browser_signal_desirability;
  report_result_task->browser_signal_highest_scoring_other_bid =
      browser_signal_highest_scoring_other_bid;
  report_result_task->browser_signal_highest_scoring_other_bid_currency =
      browser_signal_highest_scoring_other_bid_currency;
  report_result_task->browser_signals_component_auction_report_result_params =
      std::move(browser_signals_component_auction_report_result_params);
  report_result_task->trace_id = trace_id;
  report_result_task->scoring_signals_data_version =
      scoring_signals_data_version;
  report_result_task->callback = std::move(callback);

  if (direct_from_seller_seller_signals) {
    // Deleting `report_result_task` will destroy
    // `direct_from_seller_request_seller_signals` and thus abort this callback,
    // so it's safe to use Unretained(this) and `report_result_task` here.
    report_result_task->direct_from_seller_request_seller_signals =
        direct_from_seller_requester_seller_signals_.LoadSignals(
            *url_loader_factory_, *direct_from_seller_seller_signals,
            base::BindOnce(
                &SellerWorklet::
                    OnDirectFromSellerSellerSignalsDownloadedReportResult,
                base::Unretained(this), report_result_task));
  } else {
    report_result_task->direct_from_seller_result_seller_signals =
        DirectFromSellerSignalsRequester::Result();
  }

  if (direct_from_seller_auction_signals) {
    // Deleting `report_result_task` will destroy
    // `direct_from_seller_request_auction_signals` and thus abort this
    // callback, so it's safe to use Unretained(this) and `report_result_task`
    // here.
    report_result_task->direct_from_seller_request_auction_signals =
        direct_from_seller_requester_auction_signals_.LoadSignals(
            *url_loader_factory_, *direct_from_seller_auction_signals,
            base::BindOnce(
                &SellerWorklet::
                    OnDirectFromSellerAuctionSignalsDownloadedReportResult,
                base::Unretained(this), report_result_task));
  } else {
    report_result_task->direct_from_seller_result_auction_signals =
        DirectFromSellerSignalsRequester::Result();
  }
  report_result_task->direct_from_seller_seller_signals_header_ad_slot =
      direct_from_seller_seller_signals_header_ad_slot;
  report_result_task->direct_from_seller_auction_signals_header_ad_slot =
      direct_from_seller_auction_signals_header_ad_slot;

  report_result_task->trace_wait_deps_start = base::TimeTicks::Now();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "wait_report_result_deps",
                                    trace_id);
  RunReportResultIfReady(report_result_task);
}

void SellerWorklet::ConnectDevToolsAgent(
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent,
    uint32_t thread_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  v8_runners_[thread_index]->PostTask(
      FROM_HERE, base::BindOnce(&V8State::ConnectDevToolsAgent,
                                base::Unretained(v8_state_[thread_index].get()),
                                std::move(agent)));
}

SellerWorklet::ScoreAdTask::ScoreAdTask() = default;
SellerWorklet::ScoreAdTask::~ScoreAdTask() = default;

SellerWorklet::ReportResultTask::ReportResultTask() = default;
SellerWorklet::ReportResultTask::~ReportResultTask() = default;

SellerWorklet::V8State::V8State(
    scoped_refptr<AuctionV8Helper> v8_helper,
    scoped_refptr<AuctionV8Helper::DebugId> debug_id,
    mojo::PendingRemote<mojom::AuctionSharedStorageHost>
        shared_storage_host_remote,
    const GURL& decision_logic_url,
    const std::optional<GURL>& trusted_scoring_signals_url,
    const std::optional<url::Origin>& trusted_scoring_signals_origin,
    const url::Origin& top_window_origin,
    mojom::AuctionWorkletPermissionsPolicyStatePtr permissions_policy_state,
    std::optional<uint16_t> experiment_group_id,
    base::WeakPtr<SellerWorklet> parent)
    : v8_helper_(std::move(v8_helper)),
      debug_id_(debug_id),
      parent_(std::move(parent)),
      user_thread_(base::SequencedTaskRunner::GetCurrentDefault()),
      decision_logic_url_(decision_logic_url),
      trusted_scoring_signals_url_(trusted_scoring_signals_url),
      trusted_scoring_signals_origin_(trusted_scoring_signals_origin),
      top_window_origin_(top_window_origin),
      permissions_policy_state_(std::move(permissions_policy_state)),
      experiment_group_id_(experiment_group_id) {
  DETACH_FROM_SEQUENCE(v8_sequence_checker_);
  v8_helper_->v8_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V8State::FinishInit, base::Unretained(this),
                                std::move(shared_storage_host_remote)));
}

void SellerWorklet::V8State::SetWorkletScript(
    WorkletLoader::Result worklet_script,
    SignalsOriginRelation trusted_signals_relation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  worklet_script_ = WorkletLoader::TakeScript(std::move(worklet_script));
  trusted_signals_relation_ = trusted_signals_relation;
}

void SellerWorklet::V8State::ScoreAd(
    const std::string& ad_metadata_json,
    double bid,
    const std::optional<blink::AdCurrency>& bid_currency,
    const blink::AuctionConfig::NonSharedParams&
        auction_ad_config_non_shared_params,
    DirectFromSellerSignalsRequester::Result
        direct_from_seller_result_seller_signals,
    const std::optional<std::string>&
        direct_from_seller_seller_signals_header_ad_slot,
    DirectFromSellerSignalsRequester::Result
        direct_from_seller_result_auction_signals,
    const std::optional<std::string>&
        direct_from_seller_auction_signals_header_ad_slot,
    scoped_refptr<TrustedSignals::Result> trusted_scoring_signals,
    bool trusted_scoring_signals_fetch_failed,
    mojom::ComponentAuctionOtherSellerPtr browser_signals_other_seller,
    const std::optional<blink::AdCurrency>& component_expect_bid_currency,
    const url::Origin& browser_signal_interest_group_owner,
    const GURL& browser_signal_render_url,
    const std::optional<std::string>&
        browser_signal_selected_buyer_and_seller_reporting_id,
    const std::optional<std::string>&
        browser_signal_buyer_and_seller_reporting_id,
    const std::vector<std::string>& browser_signal_ad_components,
    uint32_t browser_signal_bidding_duration_msecs,
    const std::optional<blink::AdSize>& browser_signal_render_size,
    bool browser_signal_for_debugging_only_in_cooldown_or_lockout,
    const std::optional<base::TimeDelta> seller_timeout,
    uint64_t trace_id,
    base::ScopedClosureRunner cleanup_score_ad_task,
    base::TimeTicks task_enqueued_time,
    ScoreAdCallbackInternal callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  CHECK_NE(trusted_signals_relation_,
           SignalsOriginRelation::kUnknownPermissionCrossOriginSignals);
  if (trusted_signals_relation_ ==
      SignalsOriginRelation::kForbiddenCrossOriginSignals) {
    // We must have cancelled the fetch, so nothing should be set).
    CHECK(!trusted_scoring_signals);
  }
  UMA_HISTOGRAM_ENUMERATION(
      "Ads.InterestGroup.Auction.TrustedSellerSignalsOriginRelation",
      trusted_signals_relation_);
  base::UmaHistogramTimes("Ads.InterestGroup.Auction.ScoreAdQueueTime",
                          base::TimeTicks::Now() - task_enqueued_time);
  base::ElapsedTimer elapsed_timer;

  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "post_v8_task", trace_id);

  // Don't need to run `cleanup_score_ad_task` if this method is invoked;
  // it's bound to the closure to clean things up if this method got cancelled.
  cleanup_score_ad_task.ReplaceClosure(base::OnceClosure());

  // We may not be allowed any time to run.
  if (seller_timeout.has_value() && !seller_timeout->is_positive()) {
    PostScoreAdCallbackToUserThreadOnError(
        std::move(callback),
        /*scoring_latency=*/base::TimeDelta(),
        /*script_timed_out=*/true,
        /*errors=*/{"scoreAd() aborted due to zero timeout."},
        /*pa_requests=*/{},
        GetRealTimeReportingContributionsOnError(
            trusted_scoring_signals_fetch_failed, /*is_bidding_signal=*/false));
    return;
  }

  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper_.get());
  v8::Isolate* isolate = v8_helper_->isolate();

  ContextRecycler* context_recycler = nullptr;
  std::unique_ptr<ContextRecycler> fresh_context_recycler;
  if (context_recycler_for_context_reuse_) {
    context_recycler = context_recycler_for_context_reuse_.get();
  } else {
    fresh_context_recycler =
        std::make_unique<ContextRecycler>(v8_helper_.get());
    context_recycler = fresh_context_recycler.get();
  }

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "get_seller_context", trace_id);
  ContextRecyclerScope context_recycler_scope(*context_recycler);
  v8::Local<v8::Context> context = context_recycler_scope.GetContext();
  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "get_seller_context", trace_id);

  AuctionV8Logger v8_logger(v8_helper_.get(), context);

  v8::LocalVector<v8::Value> args(isolate);
  if (!v8_helper_->AppendJsonValue(context, ad_metadata_json, &args)) {
    PostScoreAdCallbackToUserThreadOnError(
        std::move(callback),
        /*scoring_latency=*/elapsed_timer.Elapsed(),
        /*script_timed_out=*/false,
        /*errors=*/std::vector<std::string>(),
        /*pa_requests=*/{},
        GetRealTimeReportingContributionsOnError(
            trusted_scoring_signals_fetch_failed, /*is_bidding_signal=*/false));
    return;
  }

  args.push_back(gin::ConvertToV8(isolate, bid));

  context_recycler->EnsureAuctionConfigLazyFillers(
      1 + auction_ad_config_non_shared_params.component_auctions.size());
  if (!AppendAuctionConfig(v8_helper_.get(), &v8_logger, context,
                           url::Origin::Create(decision_logic_url_),
                           decision_logic_url_, trusted_scoring_signals_url_,
                           experiment_group_id_,
                           auction_ad_config_non_shared_params,
                           context_recycler->auction_config_lazy_fillers(),
                           /*auction_config_lazy_filler_pos=*/0, &args)) {
    PostScoreAdCallbackToUserThreadOnError(
        std::move(callback),
        /*scoring_latency=*/elapsed_timer.Elapsed(),
        /*script_timed_out=*/false,
        /*errors=*/std::vector<std::string>(),
        /*pa_requests=*/{},
        GetRealTimeReportingContributionsOnError(
            trusted_scoring_signals_fetch_failed, /*is_bidding_signal=*/false));
    return;
  }

  std::vector<std::string> errors_out;
  v8::Local<v8::Value> trusted_scoring_signals_value;
  std::optional<uint32_t> scoring_signals_data_version;
  if (trusted_scoring_signals) {
    trusted_scoring_signals_value = trusted_scoring_signals->GetScoringSignals(
        v8_helper_.get(), context, browser_signal_render_url,
        browser_signal_ad_components);
    scoring_signals_data_version = trusted_scoring_signals->GetDataVersion();
  } else {
    trusted_scoring_signals_value = v8::Null(isolate);
  }

  if (trusted_signals_relation_ ==
      SignalsOriginRelation::kForbiddenCrossOriginSignals) {
    // Add a warning to help people debug.
    errors_out.push_back(base::StrCat(
        {decision_logic_url_.spec(),
         " disregarding trusted scoring signals since origin '",
         trusted_scoring_signals_origin_->Serialize(),
         "' is different from script's origin but not authorized by script's "
         "Ad-Auction-Allow-Trusted-Scoring-Signals-From."}));
  }

  if (trusted_signals_relation_ == SignalsOriginRelation::kSameOriginSignals) {
    args.push_back(trusted_scoring_signals_value);
  } else {
    args.push_back(v8::Null(isolate));
  }

  v8::Local<v8::Object> browser_signals = v8::Object::New(isolate);
  gin::Dictionary browser_signals_dict(isolate, browser_signals);

  if (!context_recycler->seller_browser_signals_lazy_filler()) {
    context_recycler->AddSellerBrowserSignalsLazyFiller();
  }
  context_recycler->seller_browser_signals_lazy_filler()->FillInObject(
      browser_signal_render_url, browser_signals);
  // TODO(crbug.com/336164429): Construct the fields of browser signals lazily.
  if (!browser_signals_dict.Set("topWindowHostname",
                                top_window_origin_.host()) ||
      !AddOtherSeller(browser_signals_other_seller.get(),
                      browser_signals_dict) ||
      !browser_signals_dict.Set(
          "interestGroupOwner",
          browser_signal_interest_group_owner.Serialize()) ||
      !browser_signals_dict.Set("renderURL",
                                browser_signal_render_url.spec()) ||
      (browser_signal_selected_buyer_and_seller_reporting_id.has_value() &&
       !browser_signals_dict.Set(
           "selectedBuyerAndSellerReportingId",
           *browser_signal_selected_buyer_and_seller_reporting_id)) ||
      (browser_signal_buyer_and_seller_reporting_id.has_value() &&
       !browser_signals_dict.Set(
           "buyerAndSellerReportingId",
           *browser_signal_buyer_and_seller_reporting_id)) ||
      (base::FeatureList::IsEnabled(
           blink::features::kRenderSizeInScoreAdBrowserSignals) &&
       browser_signal_render_size.has_value() &&
       !MaybeSetSizeMember(isolate, browser_signals_dict, "renderSize",
                           browser_signal_render_size.value())) ||
      !browser_signals_dict.Set("biddingDurationMsec",
                                browser_signal_bidding_duration_msecs) ||
      !browser_signals_dict.Set("bidCurrency",
                                blink::PrintableAdCurrency(bid_currency)) ||
      !SetDataVersion(trusted_signals_relation_, scoring_signals_data_version,
                      browser_signals_dict) ||
      (base::FeatureList::IsEnabled(
           blink::features::kBiddingAndScoringDebugReportingAPI) &&
       base::FeatureList::IsEnabled(
           blink::features::kFledgeSampleDebugReports) &&
       !browser_signals_dict.Set(
           "forDebuggingOnlyInCooldownOrLockout",
           browser_signal_for_debugging_only_in_cooldown_or_lockout))) {
    PostScoreAdCallbackToUserThreadOnError(
        std::move(callback),
        /*scoring_latency=*/elapsed_timer.Elapsed(),
        /*script_timed_out=*/false,
        /*errors=*/std::vector<std::string>(),
        /*pa_requests=*/{},
        GetRealTimeReportingContributionsOnError(
            trusted_scoring_signals_fetch_failed, /*is_bidding_signal=*/false));
    return;
  }
  if (!browser_signal_ad_components.empty()) {
    if (!browser_signals_dict.Set("adComponents",
                                  browser_signal_ad_components)) {
      PostScoreAdCallbackToUserThreadOnError(
          std::move(callback),
          /*scoring_latency=*/elapsed_timer.Elapsed(),
          /*script_timed_out=*/false,
          /*errors=*/std::vector<std::string>(),
          /*pa_requests=*/{},
          GetRealTimeReportingContributionsOnError(
              trusted_scoring_signals_fetch_failed,
              /*is_bidding_signal=*/false));
      return;
    }
  }
  args.push_back(browser_signals);

  v8::Local<v8::Object> direct_from_seller_signals = v8::Object::New(isolate);
  gin::Dictionary direct_from_seller_signals_dict(isolate,
                                                  direct_from_seller_signals);
  v8::Local<v8::Value> seller_signals = GetDirectFromSellerSignals(
      direct_from_seller_result_seller_signals,
      direct_from_seller_seller_signals_header_ad_slot, *v8_helper_, context,
      errors_out);
  v8::Local<v8::Value> auction_signals = GetDirectFromSellerSignals(
      direct_from_seller_result_auction_signals,
      direct_from_seller_auction_signals_header_ad_slot, *v8_helper_, context,
      errors_out);
  if (!direct_from_seller_signals_dict.Set("sellerSignals", seller_signals) ||
      !direct_from_seller_signals_dict.Set("auctionSignals", auction_signals)) {
    PostScoreAdCallbackToUserThreadOnError(
        std::move(callback),
        /*scoring_latency=*/elapsed_timer.Elapsed(),
        /*script_timed_out=*/false,
        /*errors=*/std::move(errors_out),
        /*pa_requests=*/{},
        GetRealTimeReportingContributionsOnError(
            trusted_scoring_signals_fetch_failed, /*is_bidding_signal=*/false));
    return;
  }
  args.push_back(direct_from_seller_signals);

  if (base::FeatureList::IsEnabled(
          blink::features::kFledgePermitCrossOriginTrustedSignals)) {
    v8::Local<v8::Value> cross_origin_trusted_scoring_signals_value;
    if (trusted_signals_relation_ ==
        SignalsOriginRelation::kPermittedCrossOriginSignals) {
      cross_origin_trusted_scoring_signals_value =
          TrustedSignals::Result::WrapCrossOriginSignals(
              v8_helper_.get(), context, *trusted_scoring_signals_origin_,
              trusted_scoring_signals_value);
    } else {
      cross_origin_trusted_scoring_signals_value = v8::Null(isolate);
    }
    args.push_back(cross_origin_trusted_scoring_signals_value);
  }

  v8::Local<v8::Value> score_ad_result;
  v8_helper_->MaybeTriggerInstrumentationBreakpoint(
      *debug_id_, "beforeSellerWorkletScoringStart");

  v8::Local<v8::UnboundScript> unbound_worklet_script =
      worklet_script_.Get(isolate);
  std::unique_ptr<AuctionV8Helper::TimeLimit> total_timeout =
      v8_helper_->CreateTimeLimit(seller_timeout);
  // For a context we're reusing, the top level script was already run and the
  // bindings were already added.
  if (!context_recycler_for_context_reuse_) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "sellerScript", trace_id);
    AuctionV8Helper::Result result =
        v8_helper_->RunScript(context, unbound_worklet_script, debug_id_.get(),
                              total_timeout.get(), errors_out);
    TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "sellerScript", trace_id);
    if (result != AuctionV8Helper::Result::kSuccess) {
      PostScoreAdCallbackToUserThreadOnError(
          std::move(callback),
          /*scoring_latency=*/elapsed_timer.Elapsed(),
          /*script_timed_out=*/result == AuctionV8Helper::Result::kTimeout,
          /*errors=*/std::move(errors_out),
          /*pa_requests=*/{},
          GetRealTimeReportingContributionsOnError(
              trusted_scoring_signals_fetch_failed,
              /*is_bidding_signal=*/false));
      return;
    }
    context_recycler->AddForDebuggingOnlyBindings();
    context_recycler->AddPrivateAggregationBindings(
        permissions_policy_state_->private_aggregation_allowed,
        /*reserved_once_allowed=*/true);
    context_recycler->AddRealTimeReportingBindings();
    if (base::FeatureList::IsEnabled(blink::features::kSharedStorageAPI)) {
      context_recycler->AddSharedStorageBindings(
          shared_storage_host_remote_.is_bound()
              ? shared_storage_host_remote_.get()
              : nullptr,
          mojom::AuctionWorkletFunction::kSellerScoreAd,
          permissions_policy_state_->shared_storage_allowed);
    }
  }

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "score_ad", trace_id);
  v8::MaybeLocal<v8::Value> maybe_score_ad_result;
  AuctionV8Helper::Result result = v8_helper_->CallFunction(
      context, debug_id_.get(),
      v8_helper_->FormatScriptName(unbound_worklet_script), "scoreAd", args,
      total_timeout.get(), maybe_score_ad_result, errors_out);
  if (result == AuctionV8Helper::Result::kSuccess) {
    score_ad_result = maybe_score_ad_result.ToLocalChecked();
  }

  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "score_ad", trace_id);
  base::UmaHistogramTimes("Ads.InterestGroup.Auction.ScoreAdTime",
                          elapsed_timer.Elapsed());

  std::vector<auction_worklet::mojom::RealTimeReportingContributionPtr>
      real_time_contributions = context_recycler->real_time_reporting_bindings()
                                    ->TakeRealTimeReportingContributions();

  // Add platform contributions if there are any.
  MaybeAddRealTimeReportingPlatformContributions(
      trusted_scoring_signals_fetch_failed, /*is_bidding_signal=*/false,
      real_time_contributions);

  if (result != AuctionV8Helper::Result::kSuccess) {
    // Keep debug loss reports, Private Aggregation API requests, and real time
    // reporting contributions since `scoreAd()` might use them to detect script
    // timeout or failures.
    base::TimeDelta elapsed = elapsed_timer.Elapsed();
    PostScoreAdCallbackToUserThread(
        std::move(callback), /*score=*/0,
        /*reject_reason=*/mojom::RejectReason::kNotAvailable,
        /*component_auction_modified_bid_params=*/nullptr,
        /*bid_in_seller_currency=*/std::nullopt,
        /*scoring_signals_data_version=*/std::nullopt,
        /*debug_loss_report_url=*/
        context_recycler->for_debugging_only_bindings()->TakeLossReportUrl(),
        /*debug_win_report_url=*/std::nullopt,
        context_recycler->private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        FilterRealtimeContributions(std::move(real_time_contributions),
                                    elapsed),
        /*scoring_latency=*/elapsed,
        /*script_timed_out=*/result == AuctionV8Helper::Result::kTimeout,
        std::move(errors_out));
    return;
  }

  if (!context_recycler_for_context_reuse_ &&
      base::FeatureList::IsEnabled(
          blink::features::kFledgeAlwaysReuseSellerContext)) {
    context_recycler_for_context_reuse_ = std::move(fresh_context_recycler);
  }

  double score;
  mojom::RejectReason reject_reason = mojom::RejectReason::kNotAvailable;
  bool allow_component_auction = false;
  mojom::ComponentAuctionModifiedBidParamsPtr
      component_auction_modified_bid_params;
  std::optional<double> bid_in_seller_currency;
  // If the bid is already in seller currency, forward it as
  // incomingBidInSellerCurrency.
  if (bid_currency.has_value() &&
      auction_ad_config_non_shared_params.seller_currency.has_value() &&
      bid_currency->currency_code() == auction_ad_config_non_shared_params
                                           .seller_currency->currency_code()) {
    bid_in_seller_currency = bid;
  }

  // Try to parse the result as a number. On success, it's the desirability
  // score. Otherwise, it must be an object with the desireability score, and
  // potentially other fields as well.
  if (!gin::ConvertFromV8(isolate, score_ad_result, &score)) {
    struct ScoreAdOutput {
      double desirability;
      std::optional<double> bid;
      std::optional<std::string> bid_currency;
      std::optional<v8::Local<v8::Value>> ad;
      std::optional<double> incoming_bid_in_seller_currency;
      std::optional<std::string> reject_reason;
      std::optional<bool> allow_component_auction;
    } result_idl;

    AuctionV8Helper::TimeLimitScope time_limit_scope(total_timeout.get());
    DictConverter convert_score_ad(
        v8_helper_.get(), time_limit_scope,
        base::StrCat({v8_helper_->FormatScriptName(unbound_worklet_script),
                      " scoreAd() return: "}),
        score_ad_result);
    if (!convert_score_ad.GetOptional("ad", result_idl.ad) ||
        !convert_score_ad.GetOptional("allowComponentAuction",
                                      result_idl.allow_component_auction) ||
        !convert_score_ad.GetOptional("bid", result_idl.bid) ||
        !convert_score_ad.GetOptional("bidCurrency", result_idl.bid_currency) ||
        !convert_score_ad.GetRequired("desirability",
                                      result_idl.desirability) ||
        !convert_score_ad.GetOptional(
            "incomingBidInSellerCurrency",
            result_idl.incoming_bid_in_seller_currency) ||
        !convert_score_ad.GetOptional("rejectReason",
                                      result_idl.reject_reason)) {
      errors_out.push_back(convert_score_ad.ErrorMessage());
      base::TimeDelta elapsed = elapsed_timer.Elapsed();
      PostScoreAdCallbackToUserThreadOnError(
          std::move(callback),
          /*scoring_latency=*/elapsed,
          /*script_timed_out=*/convert_score_ad.FailureIsTimeout(),
          std::move(errors_out),
          context_recycler->private_aggregation_bindings()
              ->TakePrivateAggregationRequests(),
          FilterRealtimeContributions(std::move(real_time_contributions),
                                      elapsed));
      return;
    }

    // allowComponentAuction defaults to false.
    allow_component_auction =
        result_idl.allow_component_auction.value_or(false);
    score = result_idl.desirability;

    if (result_idl.incoming_bid_in_seller_currency.has_value()) {
      bool ok = true;
      if (!auction_ad_config_non_shared_params.seller_currency.has_value()) {
        errors_out.push_back(base::StrCat(
            {decision_logic_url_.spec(),
             " scoreAd() attempting to set incomingBidInSellerCurrency without "
             "a configured sellerCurrency."}));
        ok = false;
      }
      if (ok &&
          !IsValidBid(result_idl.incoming_bid_in_seller_currency.value())) {
        errors_out.push_back(base::StrCat(
            {decision_logic_url_.spec(),
             " scoreAd() incomingBidInSellerCurrency not a valid bid."}));
        ok = false;
      }
      if (bid_in_seller_currency.has_value() &&
          *result_idl.incoming_bid_in_seller_currency !=
              *bid_in_seller_currency) {
        errors_out.push_back(base::StrCat(
            {decision_logic_url_.spec(),
             " scoreAd() attempting to set incomingBidInSellerCurrency "
             "inconsistent with incoming bid already in seller currency."}));
        ok = false;
      }
      if (!ok) {
        base::TimeDelta elapsed = elapsed_timer.Elapsed();
        PostScoreAdCallbackToUserThreadOnError(
            std::move(callback),
            /*scoring_latency=*/elapsed,
            /*script_timed_out=*/false, std::move(errors_out),
            context_recycler->private_aggregation_bindings()
                ->TakePrivateAggregationRequests(),
            FilterRealtimeContributions(std::move(real_time_contributions),
                                        elapsed));
        return;
      }
      bid_in_seller_currency = result_idl.incoming_bid_in_seller_currency;
    }

    if (result_idl.reject_reason.has_value()) {
      auto reject_reason_opt =
          RejectReasonStringToEnum(*result_idl.reject_reason);

      if (!reject_reason_opt.has_value()) {
        errors_out.push_back(
            base::StrCat({decision_logic_url_.spec(),
                          " scoreAd() returned an invalid reject reason."}));
      } else {
        reject_reason = reject_reason_opt.value();
      }
    }

    // If this is the seller in a component auction (and thus it was passed a
    // top-level seller), need to return a
    // mojom::ComponentAuctionModifiedBidParams.
    if (allow_component_auction && browser_signals_other_seller &&
        browser_signals_other_seller->is_top_level_seller()) {
      component_auction_modified_bid_params =
          mojom::ComponentAuctionModifiedBidParams::New();

      component_auction_modified_bid_params->ad = "null";
      if (result_idl.ad.has_value()) {
        std::string candidate_ad;

        // Can pass null for timeout here since we already have a TimeLimitScope
        // active.
        AuctionV8Helper::Result json_result = v8_helper_->ExtractJson(
            context, *result_idl.ad, /*script_timeout=*/nullptr, &candidate_ad);
        if (json_result == AuctionV8Helper::Result::kSuccess) {
          component_auction_modified_bid_params->ad = std::move(candidate_ad);
        } else if (json_result == AuctionV8Helper::Result::kTimeout) {
          errors_out.push_back(base::StrCat(
              {decision_logic_url_.spec(),
               " timeout serializing `ad` field of scoreAd() return value."}));
          base::TimeDelta elapsed = elapsed_timer.Elapsed();
          PostScoreAdCallbackToUserThread(
              std::move(callback), /*score=*/0,
              /*reject_reason=*/mojom::RejectReason::kNotAvailable,
              /*component_auction_modified_bid_params=*/nullptr,
              /*bid_in_seller_currency=*/std::nullopt,
              /*scoring_signals_data_version=*/std::nullopt,
              /*debug_loss_report_url=*/
              context_recycler->for_debugging_only_bindings()
                  ->TakeLossReportUrl(),
              /*debug_win_report_url=*/std::nullopt,
              context_recycler->private_aggregation_bindings()
                  ->TakePrivateAggregationRequests(),
              FilterRealtimeContributions(std::move(real_time_contributions),
                                          elapsed),
              /*scoring_latency=*/elapsed, /*script_timed_out=*/true,
              std::move(errors_out));
          return;
        }
        // else, it's a regular failure; leave at "null".
      }

      component_auction_modified_bid_params->bid = result_idl.bid;
      if (component_auction_modified_bid_params->bid.has_value()) {
        bool drop_for_invalid_currency = false;
        if (result_idl.bid_currency.has_value()) {
          if (!blink::IsValidAdCurrencyCode(*result_idl.bid_currency)) {
            errors_out.push_back(
                base::StrCat({decision_logic_url_.spec(),
                              " scoreAd() returned an invalid bidCurrency."}));
            drop_for_invalid_currency = true;
          } else {
            component_auction_modified_bid_params->bid_currency =
                blink::AdCurrency::From(*result_idl.bid_currency);
          }
        }

        if (!drop_for_invalid_currency &&
            !VerifySellerCurrency(
                /*provided_currency=*/component_auction_modified_bid_params
                    ->bid_currency,
                /*expected_seller_currency=*/
                auction_ad_config_non_shared_params.seller_currency,
                /*component_expect_bid_currency=*/component_expect_bid_currency,
                decision_logic_url_, "bidCurrency", errors_out)) {
          drop_for_invalid_currency = true;
        }
        if (drop_for_invalid_currency) {
          score = 0;
          // If scoreAd() didn't already specify a reject reason, note the
          // currency mismatch.
          if (reject_reason == mojom::RejectReason::kNotAvailable) {
            reject_reason = mojom::RejectReason::kWrongScoreAdCurrency;
          }
        }
      }
    }
  }

  // Fail if the score is invalid.
  if (std::isnan(score) || !std::isfinite(score)) {
    errors_out.push_back(base::StrCat(
        {decision_logic_url_.spec(), " scoreAd() returned an invalid score."}));
    base::TimeDelta elapsed = elapsed_timer.Elapsed();
    PostScoreAdCallbackToUserThreadOnError(
        std::move(callback),
        /*scoring_latency=*/elapsed,
        /*script_timed_out=*/false, std::move(errors_out),
        context_recycler->private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        FilterRealtimeContributions(std::move(real_time_contributions),
                                    elapsed));
    return;
  }

  if (score <= 0) {
    // Keep debug report URLs because we want to send debug loss reports if
    // seller rejected all bids.
    base::TimeDelta elapsed = elapsed_timer.Elapsed();
    PostScoreAdCallbackToUserThread(
        std::move(callback), /*score=*/0, reject_reason,
        /*component_auction_modified_bid_params=*/nullptr,
        /*bid_in_seller_currency=*/std::nullopt, scoring_signals_data_version,
        context_recycler->for_debugging_only_bindings()->TakeLossReportUrl(),
        context_recycler->for_debugging_only_bindings()->TakeWinReportUrl(),
        context_recycler->private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        FilterRealtimeContributions(std::move(real_time_contributions),
                                    elapsed),
        /*scoring_latency=*/elapsed, /*script_timed_out=*/false,
        std::move(errors_out));
    return;
  }

  // Fail if `allow_component_auction` is false and this is a component seller
  // or a top-level seller scoring a bid from a component auction -
  // `browser_signals_other_seller` is non-null in only those two cases.
  // This is after the score check so that returning a negative score with
  // nothing else is not treated as an error in a component auction.
  if (browser_signals_other_seller && !allow_component_auction) {
    errors_out.push_back(base::StrCat(
        {decision_logic_url_.spec(),
         " scoreAd() return value does not have allowComponentAuction set to "
         "true. Ad dropped from component auction."}));
    base::TimeDelta elapsed = elapsed_timer.Elapsed();
    PostScoreAdCallbackToUserThreadOnError(
        std::move(callback),
        /*scoring_latency=*/elapsed, /*script_timed_out=*/false,
        std::move(errors_out),
        context_recycler->private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        FilterRealtimeContributions(std::move(real_time_contributions),
                                    elapsed));
    return;
  }

  // This bid got accepted by scoreAd(), so clear any reject reason it may have
  // set.
  reject_reason = mojom::RejectReason::kNotAvailable;

  // If this is a component auction that modified the bid, validate the bid. Do
  // this after checking the score to avoid validating modified bid values from
  // reporting errors when desirability is <= 0.
  if (component_auction_modified_bid_params &&
      component_auction_modified_bid_params->bid.has_value()) {
    // Fail if the new bid is not valid or is 0 or less.
    if (!IsValidBid(component_auction_modified_bid_params->bid.value())) {
      errors_out.push_back(base::StrCat(
          {decision_logic_url_.spec(), " scoreAd() returned an invalid bid."}));
      base::TimeDelta elapsed = elapsed_timer.Elapsed();
      PostScoreAdCallbackToUserThreadOnError(
          std::move(callback),
          /*scoring_latency=*/elapsed,
          /*script_timed_out=*/false, std::move(errors_out),
          context_recycler->private_aggregation_bindings()
              ->TakePrivateAggregationRequests(),
          FilterRealtimeContributions(std::move(real_time_contributions),
                                      elapsed));
      return;
    }
  } else if (browser_signals_other_seller &&
             browser_signals_other_seller->is_top_level_seller()) {
    // This is a component auction that did not modify the bid; e.g. it's using
    // the bidder's bid as its own. Therefore, check it against our own
    // currency requirements.
    if (!VerifySellerCurrency(
            /*provided_currency=*/bid_currency,
            /*expected_seller_currency=*/
            auction_ad_config_non_shared_params.seller_currency,
            /*component_expect_bid_currency=*/component_expect_bid_currency,
            decision_logic_url_, "bid passthrough", errors_out)) {
      score = 0;
      reject_reason = mojom::RejectReason::kWrongScoreAdCurrency;
    }
  }

  base::TimeDelta elapsed = elapsed_timer.Elapsed();
  PostScoreAdCallbackToUserThread(
      std::move(callback), score, reject_reason,
      std::move(component_auction_modified_bid_params), bid_in_seller_currency,
      scoring_signals_data_version,
      context_recycler->for_debugging_only_bindings()->TakeLossReportUrl(),
      context_recycler->for_debugging_only_bindings()->TakeWinReportUrl(),
      context_recycler->private_aggregation_bindings()
          ->TakePrivateAggregationRequests(),
      FilterRealtimeContributions(std::move(real_time_contributions), elapsed),
      /*scoring_latency=*/elapsed, /*script_timed_out=*/false,
      std::move(errors_out));
}

void SellerWorklet::V8State::ReportResult(
    const blink::AuctionConfig::NonSharedParams&
        auction_ad_config_non_shared_params,
    DirectFromSellerSignalsRequester::Result
        direct_from_seller_result_seller_signals,
    const std::optional<std::string>&
        direct_from_seller_seller_signals_header_ad_slot,
    DirectFromSellerSignalsRequester::Result
        direct_from_seller_result_auction_signals,
    const std::optional<std::string>&
        direct_from_seller_auction_signals_header_ad_slot,
    mojom::ComponentAuctionOtherSellerPtr browser_signals_other_seller,
    const url::Origin& browser_signal_interest_group_owner,
    const std::optional<std::string>&
        browser_signal_buyer_and_seller_reporting_id,
    const std::optional<std::string>&
        browser_signal_selected_buyer_and_seller_reporting_id,
    const GURL& browser_signal_render_url,
    double browser_signal_bid,
    const std::optional<blink::AdCurrency>& browser_signal_bid_currency,
    double browser_signal_desirability,
    double browser_signal_highest_scoring_other_bid,
    const std::optional<blink::AdCurrency>&
        browser_signal_highest_scoring_other_bid_currency,
    auction_worklet::mojom::ComponentAuctionReportResultParamsPtr
        browser_signals_component_auction_report_result_params,
    std::optional<uint32_t> scoring_signals_data_version,
    uint64_t trace_id,
    ReportResultCallbackInternal callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "post_v8_task", trace_id);
  base::ElapsedTimer elapsed_timer;

  // We may not be allowed any time to run.
  if (auction_ad_config_non_shared_params.reporting_timeout.has_value() &&
      !auction_ad_config_non_shared_params.reporting_timeout->is_positive()) {
    PostReportResultCallbackToUserThread(
        std::move(callback),
        /*signals_for_winner=*/std::nullopt,
        /*report_url=*/std::nullopt,
        /*ad_beacon_map=*/{},
        /*pa_requests=*/{}, base::TimeDelta(),
        /*script_timed_out=*/true,
        /*errors=*/{"reportResult() aborted due to zero timeout."});
    return;
  }

  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper_.get());
  v8::Isolate* isolate = v8_helper_->isolate();

  // Short lived context, to avoid leaking data at global scope between either
  // repeated calls to this worklet, or to calls to any other worklet.
  ContextRecycler context_recycler(v8_helper_.get());

  ContextRecyclerScope context_recycler_scope(context_recycler);
  v8::Local<v8::Context> context = context_recycler_scope.GetContext();
  AuctionV8Logger v8_logger(v8_helper_.get(), context);

  v8::LocalVector<v8::Value> args(isolate);

  context_recycler.EnsureAuctionConfigLazyFillers(
      1 + auction_ad_config_non_shared_params.component_auctions.size());
  if (!AppendAuctionConfig(v8_helper_.get(), &v8_logger, context,
                           url::Origin::Create(decision_logic_url_),
                           decision_logic_url_, trusted_scoring_signals_url_,
                           experiment_group_id_,
                           auction_ad_config_non_shared_params,
                           context_recycler.auction_config_lazy_fillers(),
                           /*auction_config_lazy_filler_pos=*/0, &args)) {
    PostReportResultCallbackToUserThread(std::move(callback),
                                         /*signals_for_winner=*/std::nullopt,
                                         /*report_url=*/std::nullopt,
                                         /*ad_beacon_map=*/{},
                                         /*pa_requests=*/{}, base::TimeDelta(),
                                         /*script_timed_out=*/false,
                                         /*errors=*/std::vector<std::string>());
    return;
  }

  v8::Local<v8::Object> browser_signals = v8::Object::New(isolate);
  gin::Dictionary browser_signals_dict(isolate, browser_signals);

  context_recycler.AddSellerBrowserSignalsLazyFiller();
  context_recycler.seller_browser_signals_lazy_filler()->FillInObject(
      browser_signal_render_url, browser_signals);

  if (!browser_signals_dict.Set("topWindowHostname",
                                top_window_origin_.host()) ||
      !AddOtherSeller(browser_signals_other_seller.get(),
                      browser_signals_dict) ||
      !browser_signals_dict.Set(
          "interestGroupOwner",
          browser_signal_interest_group_owner.Serialize()) ||
      (browser_signal_buyer_and_seller_reporting_id.has_value() &&
       !browser_signals_dict.Set(
           "buyerAndSellerReportingId",
           *browser_signal_buyer_and_seller_reporting_id)) ||
      (browser_signal_selected_buyer_and_seller_reporting_id.has_value() &&
       !browser_signals_dict.Set(
           "selectedBuyerAndSellerReportingId",
           *browser_signal_selected_buyer_and_seller_reporting_id)) ||
      !browser_signals_dict.Set("renderURL",
                                browser_signal_render_url.spec()) ||
      !browser_signals_dict.Set("bid", browser_signal_bid) ||
      !browser_signals_dict.Set(
          "bidCurrency",
          blink::PrintableAdCurrency(browser_signal_bid_currency)) ||
      !browser_signals_dict.Set("desirability", browser_signal_desirability) ||
      !browser_signals_dict.Set("highestScoringOtherBid",
                                browser_signal_highest_scoring_other_bid) ||
      !browser_signals_dict.Set(
          "highestScoringOtherBidCurrency",
          blink::PrintableAdCurrency(
              browser_signal_highest_scoring_other_bid_currency)) ||
      (scoring_signals_data_version.has_value() &&
       !browser_signals_dict.Set("dataVersion",
                                 scoring_signals_data_version.value()))) {
    PostReportResultCallbackToUserThread(std::move(callback),
                                         /*signals_for_winner=*/std::nullopt,
                                         /*report_url=*/std::nullopt,
                                         /*ad_beacon_map=*/{},
                                         /*pa_requests=*/{}, base::TimeDelta(),
                                         /*script_timed_out=*/false,
                                         /*errors=*/std::vector<std::string>());
    return;
  }
  if (browser_signals_component_auction_report_result_params) {
    if (!v8_helper_->InsertJsonValue(
            context, "topLevelSellerSignals",
            browser_signals_component_auction_report_result_params
                ->top_level_seller_signals,
            browser_signals) ||
        (browser_signals_component_auction_report_result_params->modified_bid
             .has_value() &&
         !browser_signals_dict.Set(
             "modifiedBid",
             browser_signals_component_auction_report_result_params
                 ->modified_bid.value()))) {
      PostReportResultCallbackToUserThread(
          std::move(callback),
          /*signals_for_winner=*/std::nullopt,
          /*report_url=*/std::nullopt,
          /*ad_beacon_map=*/{},
          /*pa_requests=*/{}, base::TimeDelta(),
          /*script_timed_out=*/false,
          /*errors=*/std::vector<std::string>());
      return;
    }
  }
  args.push_back(browser_signals);

  std::vector<std::string> errors_out;
  v8::Local<v8::Object> direct_from_seller_signals = v8::Object::New(isolate);
  gin::Dictionary direct_from_seller_signals_dict(isolate,
                                                  direct_from_seller_signals);
  v8::Local<v8::Value> seller_signals = GetDirectFromSellerSignals(
      direct_from_seller_result_seller_signals,
      direct_from_seller_seller_signals_header_ad_slot, *v8_helper_, context,
      errors_out);
  v8::Local<v8::Value> auction_signals = GetDirectFromSellerSignals(
      direct_from_seller_result_auction_signals,
      direct_from_seller_auction_signals_header_ad_slot, *v8_helper_, context,
      errors_out);
  if (!direct_from_seller_signals_dict.Set("sellerSignals", seller_signals) ||
      !direct_from_seller_signals_dict.Set("auctionSignals", auction_signals)) {
    PostReportResultCallbackToUserThread(std::move(callback),
                                         /*signals_for_winner=*/std::nullopt,
                                         /*report_url=*/std::nullopt,
                                         /*ad_beacon_map=*/{},
                                         /*pa_requests=*/{}, base::TimeDelta(),
                                         /*script_timed_out=*/false,
                                         /*errors=*/errors_out);
    return;
  }
  args.push_back(direct_from_seller_signals);

  v8::Local<v8::Value> signals_for_winner_value;
  v8_helper_->MaybeTriggerInstrumentationBreakpoint(
      *debug_id_, "beforeSellerWorkletReportingStart");

  v8::Local<v8::UnboundScript> unbound_worklet_script =
      worklet_script_.Get(isolate);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "report_result", trace_id);

  std::unique_ptr<AuctionV8Helper::TimeLimit> total_timeout =
      v8_helper_->CreateTimeLimit(
          /*script_timeout=*/auction_ad_config_non_shared_params
              .reporting_timeout);
  AuctionV8Helper::Result result =
      v8_helper_->RunScript(context, unbound_worklet_script, debug_id_.get(),
                            total_timeout.get(), errors_out);

  if (result != AuctionV8Helper::Result::kSuccess) {
    TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "report_result", trace_id);
    PostReportResultCallbackToUserThread(
        std::move(callback), /*signals_for_winner=*/std::nullopt,
        /*report_url=*/std::nullopt, /*ad_beacon_map=*/{},
        /*pa_requests=*/{}, elapsed_timer.Elapsed(),
        /*script_timed_out=*/result == AuctionV8Helper::Result::kTimeout,
        std::move(errors_out));
    return;
  }

  context_recycler.AddReportBindings();
  context_recycler.AddRegisterAdBeaconBindings();
  context_recycler.AddPrivateAggregationBindings(
      permissions_policy_state_->private_aggregation_allowed,
      /*reserved_once_allowed=*/false);

  if (base::FeatureList::IsEnabled(blink::features::kSharedStorageAPI)) {
    context_recycler.AddSharedStorageBindings(
        shared_storage_host_remote_.is_bound()
            ? shared_storage_host_remote_.get()
            : nullptr,
        mojom::AuctionWorkletFunction::kSellerReportResult,
        permissions_policy_state_->shared_storage_allowed);
  }

  v8::MaybeLocal<v8::Value> maybe_signals_for_winner_value;
  result = v8_helper_->CallFunction(
      context, debug_id_.get(),
      v8_helper_->FormatScriptName(unbound_worklet_script), "reportResult",
      args, total_timeout.get(), maybe_signals_for_winner_value, errors_out);

  std::string signals_for_winner;
  if (result == AuctionV8Helper::Result::kSuccess) {
    signals_for_winner_value = maybe_signals_for_winner_value.ToLocalChecked();
    result = v8_helper_->ExtractJson(context, signals_for_winner_value,
                                     total_timeout.get(), &signals_for_winner);
    if (result == AuctionV8Helper::Result::kFailure) {
      // Consider lack of script error but a return value that can't be
      // converted to JSON a valid result.
      result = AuctionV8Helper::Result::kSuccess;
      signals_for_winner = "null";
    } else if (result == AuctionV8Helper::Result::kTimeout) {
      errors_out.push_back(
          base::StrCat({decision_logic_url_.spec(),
                        " timeout serializing reportResult() return value."}));
    }
  }

  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "report_result", trace_id);
  base::TimeDelta elapsed = elapsed_timer.Elapsed();
  base::UmaHistogramTimes("Ads.InterestGroup.Auction.ReportResultTime",
                          elapsed);

  if (result != AuctionV8Helper::Result::kSuccess) {
    // Keep Private Aggregation API requests since `reportReport()` might use
    // it to detect script timeout or failures.
    PostReportResultCallbackToUserThread(
        std::move(callback), /*signals_for_winner=*/std::nullopt,
        /*report_url=*/std::nullopt, /*ad_beacon_map=*/{},
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        elapsed,
        /*script_timed_out=*/result == AuctionV8Helper::Result::kTimeout,
        std::move(errors_out));
    return;
  }

  PostReportResultCallbackToUserThread(
      std::move(callback), std::move(signals_for_winner),
      context_recycler.report_bindings()->report_url(),
      context_recycler.register_ad_beacon_bindings()->TakeAdBeaconMap(),
      context_recycler.private_aggregation_bindings()
          ->TakePrivateAggregationRequests(),
      elapsed, /*script_timed_out=*/false, std::move(errors_out));
}

void SellerWorklet::V8State::ConnectDevToolsAgent(
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  v8_helper_->ConnectDevToolsAgent(std::move(agent), user_thread_, *debug_id_);
}

SellerWorklet::V8State::~V8State() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
}

void SellerWorklet::V8State::FinishInit(
    mojo::PendingRemote<mojom::AuctionSharedStorageHost>
        shared_storage_host_remote) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);

  if (shared_storage_host_remote) {
    shared_storage_host_remote_.Bind(std::move(shared_storage_host_remote));
  }

  debug_id_->SetResumeCallback(base::BindOnce(
      &SellerWorklet::V8State::PostResumeToUserThread, parent_, user_thread_));
}

// static
void SellerWorklet::V8State::PostResumeToUserThread(
    base::WeakPtr<SellerWorklet> parent,
    scoped_refptr<base::SequencedTaskRunner> user_thread) {
  // This is static since it's called from debugging, not SellerWorklet,
  // so the usual guarantee that SellerWorklet posts things before posting
  // V8State destruction is irrelevant.
  user_thread->PostTask(FROM_HERE,
                        base::BindOnce(&SellerWorklet::ResumeIfPaused, parent));
}

void SellerWorklet::V8State::PostScoreAdCallbackToUserThreadOnError(
    ScoreAdCallbackInternal callback,
    base::TimeDelta scoring_latency,
    bool script_timed_out,
    std::vector<std::string> errors,
    PrivateAggregationRequests pa_requests,
    RealTimeReportingContributions real_time_contributions) {
  PostScoreAdCallbackToUserThread(
      std::move(callback), /*score=*/0,
      /*reject_reason=*/mojom::RejectReason::kNotAvailable,
      /*component_auction_modified_bid_params=*/nullptr,
      /*bid_in_seller_currency=*/std::nullopt,
      /*scoring_signals_data_version=*/std::nullopt,
      /*debug_loss_report_url=*/std::nullopt,
      /*debug_win_report_url=*/std::nullopt, std::move(pa_requests),
      std::move(real_time_contributions),
      /*scoring_latency=*/scoring_latency,
      /*script_timed_out=*/script_timed_out, std::move(errors));
}

void SellerWorklet::V8State::PostScoreAdCallbackToUserThread(
    ScoreAdCallbackInternal callback,
    double score,
    mojom::RejectReason reject_reason,
    mojom::ComponentAuctionModifiedBidParamsPtr
        component_auction_modified_bid_params,
    std::optional<double> bid_in_seller_currency,
    std::optional<uint32_t> scoring_signals_data_version,
    std::optional<GURL> debug_loss_report_url,
    std::optional<GURL> debug_win_report_url,
    PrivateAggregationRequests pa_requests,
    RealTimeReportingContributions real_time_contributions,
    base::TimeDelta scoring_latency,
    bool script_timed_out,
    std::vector<std::string> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  user_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), score, reject_reason,
                     std::move(component_auction_modified_bid_params),
                     bid_in_seller_currency, scoring_signals_data_version,
                     std::move(debug_loss_report_url),
                     std::move(debug_win_report_url), std::move(pa_requests),
                     std::move(real_time_contributions), scoring_latency,
                     script_timed_out, std::move(errors)));
}

void SellerWorklet::V8State::PostReportResultCallbackToUserThread(
    ReportResultCallbackInternal callback,
    std::optional<std::string> signals_for_winner,
    std::optional<GURL> report_url,
    base::flat_map<std::string, GURL> ad_beacon_map,
    PrivateAggregationRequests pa_requests,
    base::TimeDelta reporting_latency,
    bool script_timed_out,
    std::vector<std::string> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  user_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(signals_for_winner),
                     std::move(report_url), std::move(ad_beacon_map),
                     std::move(pa_requests), reporting_latency,
                     script_timed_out, std::move(errors)));
}

void SellerWorklet::ResumeIfPaused() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  if (!paused_) {
    return;
  }

  resumed_count_++;
  if (resumed_count_ == v8_helpers_.size()) {
    paused_ = false;
    Start();
  }
}

void SellerWorklet::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  DCHECK(!paused_);

  WorkletLoader::AllowTrustedScoringSignalsCallback
      on_got_cross_origin_signals_permissions;
  if (trusted_signals_relation_ ==
      SignalsOriginRelation::kUnknownPermissionCrossOriginSignals) {
    on_got_cross_origin_signals_permissions = base::BindOnce(
        &SellerWorklet::OnGotCrossOriginTrustedSignalsPermissions,
        base::Unretained(this));
  }

  base::UmaHistogramCounts100000(
      "Ads.InterestGroup.Net.RequestUrlSizeBytes.ScoringScriptJS",
      script_source_url_.spec().size());

  code_download_start_ = base::TimeTicks::Now();
  worklet_loader_ = std::make_unique<WorkletLoader>(
      url_loader_factory_.get(), /*auction_network_events_handler=*/
      CreateNewAuctionNetworkEventsHandlerRemote(
          auction_network_events_handler_),
      script_source_url_, v8_helpers_, debug_ids_,
      std::move(on_got_cross_origin_signals_permissions),
      base::BindOnce(&SellerWorklet::OnDownloadComplete,
                     base::Unretained(this)));
}

void SellerWorklet::OnDownloadComplete(
    std::vector<WorkletLoader::Result> worklet_scripts,
    std::optional<std::string> error_msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  DCHECK_EQ(worklet_scripts.size(), v8_helpers_.size());
  js_fetch_latency_ = base::TimeTicks::Now() - code_download_start_;

  // Use `worklet_scripts[0]` for metrics and for the failure check. All the
  // results should be the same.
  base::UmaHistogramCounts10M(
      "Ads.InterestGroup.Net.ResponseSizeBytes.ScoringScriptJS",
      worklet_scripts[0].original_size_bytes());
  base::UmaHistogramTimes("Ads.InterestGroup.Net.DownloadTime.ScoringScriptJS",
                          worklet_scripts[0].download_time());
  worklet_loader_.reset();

  // On failure, delete `this`, as it can't do anything without a loaded script.
  bool success = worklet_scripts[0].success();
  if (!success) {
    std::move(close_pipe_callback_)
        .Run(error_msg ? error_msg.value() : std::string());
    // `this` should be deleted at this point.
    return;
  }

  // The error message, if any, will be appended to all invoked ScoreAd() and
  // ReportResult() callbacks.
  load_script_error_msg_ = std::move(error_msg);

  DCHECK_NE(trusted_signals_relation_,
            SignalsOriginRelation::kUnknownPermissionCrossOriginSignals);

  for (size_t i = 0; i < v8_runners_.size(); ++i) {
    v8_runners_[i]->PostTask(
        FROM_HERE, base::BindOnce(&SellerWorklet::V8State::SetWorkletScript,
                                  base::Unretained(v8_state_[i].get()),
                                  std::move(worklet_scripts[i]),
                                  trusted_signals_relation_));
  }

  MaybeRecordCodeWait();

  for (auto score_ad_task = score_ad_tasks_.begin();
       score_ad_task != score_ad_tasks_.end(); ++score_ad_task) {
    ScoreAdIfReady(score_ad_task);
  }

  for (auto report_result_task = report_result_tasks_.begin();
       report_result_task != report_result_tasks_.end(); ++report_result_task) {
    RunReportResultIfReady(report_result_task);
  }
}

void SellerWorklet::MaybeRecordCodeWait() {
  if (!IsCodeReady()) {
    return;
  }

  base::TimeTicks now = base::TimeTicks::Now();
  for (auto& task : score_ad_tasks_) {
    task.wait_code = now - task.trace_wait_deps_start;
  }

  for (auto& task : report_result_tasks_) {
    task.wait_code = now - task.trace_wait_deps_start;
  }
}

void SellerWorklet::OnGotCrossOriginTrustedSignalsPermissions(
    std::vector<url::Origin> permit_origins) {
  DCHECK_EQ(trusted_signals_relation_,
            SignalsOriginRelation::kUnknownPermissionCrossOriginSignals);

  if (std::any_of(permit_origins.begin(), permit_origins.end(),
                  [&](const auto& permitted_origin) {
                    return trusted_scoring_signals_origin_->IsSameOriginWith(
                        permitted_origin);
                  })) {
    // Cross-origin trusted signals fetch authorized. Update
    // `trusted_signals_relation_` accordingly and start fetches.
    trusted_signals_relation_ =
        SignalsOriginRelation::kPermittedCrossOriginSignals;

    // Measure about how long this delayed things by.
    base::TimeDelta approx_classify_delay;  // 0 initially.
    if (first_deferred_trusted_signals_time_.has_value()) {
      approx_classify_delay =
          base::TimeTicks::Now() - *first_deferred_trusted_signals_time_;
    }

    base::UmaHistogramTimes(
        "Ads.InterestGroup.Auction."
        "TrustedSellerSignalsCrossOriginPermissionWait",
        approx_classify_delay);

    for (auto it = score_ad_tasks_.begin(); it != score_ad_tasks_.end(); ++it) {
      StartFetchingSignalsForTask(it);
    }
    // Rather than keep track of whether there was a
    // SendPendingSignalsRequests() call while waiting to check the cross-origin
    // trusted signals permissions, unconditionally flush pending signals
    // requests, to keep things simple.
    SendPendingSignalsRequests();
    return;
  }

  // Trusted scoring signals fetch disallowed.
  trusted_signals_relation_ =
      SellerWorklet::SignalsOriginRelation::kForbiddenCrossOriginSignals;

  // Remove the `trusted_signals_request_manager_` so we don't try to fetch any
  // more.
  trusted_signals_request_manager_.reset();

  // If we're here, we don't actually have to worry about kicking off scoreAd()
  // execution since we only got the headers for the script; the body hasn't
  // been handed to us yet.
  DCHECK(!IsCodeReady());
}

void SellerWorklet::StartFetchingSignalsForTask(
    ScoreAdTaskList::iterator score_ad_task) {
  CHECK(trusted_signals_relation_ ==
            SignalsOriginRelation::kSameOriginSignals ||
        trusted_signals_relation_ ==
            SignalsOriginRelation::kPermittedCrossOriginSignals);

  if (trusted_signals_request_manager_->HasPublicKey()) {
    DCHECK(base::FeatureList::IsEnabled(
        blink::features::kFledgeTrustedSignalsKVv2Support));

    score_ad_task->trusted_scoring_signals_request =
        trusted_signals_request_manager_->RequestKVv2ScoringSignals(
            score_ad_task->browser_signal_render_url,
            score_ad_task->browser_signal_ad_components,
            score_ad_task->browser_signal_interest_group_owner,
            score_ad_task->bidder_joining_origin,
            base::BindOnce(&SellerWorklet::OnTrustedScoringSignalsDownloaded,
                           base::Unretained(this), score_ad_task));
  } else {
    score_ad_task->trusted_scoring_signals_request =
        trusted_signals_request_manager_->RequestScoringSignals(
            score_ad_task->browser_signal_render_url,
            score_ad_task->browser_signal_ad_components,
            score_ad_task->auction_ad_config_non_shared_params
                .max_trusted_scoring_signals_url_length,
            base::BindOnce(&SellerWorklet::OnTrustedScoringSignalsDownloaded,
                           base::Unretained(this), score_ad_task));
  }
}

void SellerWorklet::OnTrustedScoringSignalsDownloaded(
    ScoreAdTaskList::iterator task,
    scoped_refptr<TrustedSignals::Result> result,
    std::optional<std::string> error_msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  task->trusted_bidding_signals_fetch_failed = !result ? true : false;
  task->trusted_scoring_signals_result = std::move(result);
  task->trusted_scoring_signals_error_msg = std::move(error_msg);
  // Clean up single-use object, now that it has done its job.
  task->trusted_scoring_signals_request.reset();

  task->wait_trusted_signals =
      base::TimeTicks::Now() - task->trace_wait_deps_start;
  ScoreAdIfReady(task);
}

void SellerWorklet::OnScoreAdClientDestroyed(ScoreAdTaskList::iterator task) {
  // If IsReadyToScoreAd() is false, it also hasn't posted the iterator
  // off-thread, so we can just remove the object and have it cancel everything
  // else.
  if (!IsReadyToScoreAd(*task)) {
    score_ad_tasks_.erase(task);
  } else {
    // Otherwise, there should be a pending V8 call. Try to cancel that, but if
    // it already started, it will just run and throw out the results thanks to
    // the closed client pipe.
    DCHECK_NE(task->task_id, base::CancelableTaskTracker::kBadTaskId);
    cancelable_task_tracker_.TryCancel(task->task_id);
  }
}

void SellerWorklet::OnDirectFromSellerSellerSignalsDownloadedScoreAd(
    ScoreAdTaskList::iterator task,
    DirectFromSellerSignalsRequester::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  task->direct_from_seller_result_seller_signals = std::move(result);
  task->direct_from_seller_request_seller_signals.reset();
  // The two direct from seller signals metrics for tracing are combined since
  // they should be roughly the same.
  task->wait_direct_from_seller_signals =
      std::max(task->wait_direct_from_seller_signals,
               base::TimeTicks::Now() - task->trace_wait_deps_start);

  ScoreAdIfReady(task);
}

void SellerWorklet::OnDirectFromSellerAuctionSignalsDownloadedScoreAd(
    ScoreAdTaskList::iterator task,
    DirectFromSellerSignalsRequester::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  task->direct_from_seller_result_auction_signals = std::move(result);
  task->direct_from_seller_request_auction_signals.reset();
  // The two direct from seller signals metrics for tracing are combined since
  // they should be roughly the same.
  task->wait_direct_from_seller_signals =
      std::max(task->wait_direct_from_seller_signals,
               base::TimeTicks::Now() - task->trace_wait_deps_start);

  ScoreAdIfReady(task);
}

bool SellerWorklet::IsReadyToScoreAd(const ScoreAdTask& task) const {
  // The first check should be implied by IsCodeReady(), but best to be safe.
  return trusted_signals_relation_ !=
             SignalsOriginRelation::kUnknownPermissionCrossOriginSignals &&
         !task.trusted_scoring_signals_request &&
         !task.direct_from_seller_request_seller_signals &&
         !task.direct_from_seller_request_auction_signals && IsCodeReady();
}

void SellerWorklet::ScoreAdIfReady(ScoreAdTaskList::iterator task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  if (!IsReadyToScoreAd(*task)) {
    return;
  }

  TRACE_EVENT_NESTABLE_ASYNC_END1(
      "fledge", "wait_score_ad_deps", task->trace_id, "data",
      [&](perfetto::TracedValue trace_context) {
        auto dict = std::move(trace_context).WriteDictionary();
        if (!task->wait_code.is_zero()) {
          dict.Add("wait_code_ms", task->wait_code.InMillisecondsF());
        }
        if (!task->wait_trusted_signals.is_zero()) {
          dict.Add("wait_trusted_signals_ms",
                   task->wait_trusted_signals.InMillisecondsF());
        }
        if (!task->wait_direct_from_seller_signals.is_zero()) {
          dict.Add("wait_direct_from_seller_signals_ms",
                   task->wait_direct_from_seller_signals.InMillisecondsF());
        }
      });

  ScoreAdInput slowest_input = ScoreAdInput::kScoringScript;
  base::TimeDelta slowest_input_time = task->wait_code;
  if (task->wait_trusted_signals > task->wait_code) {
    slowest_input = ScoreAdInput::kTrustedSignals;
    slowest_input_time = task->wait_trusted_signals;
  }
  if (task->wait_direct_from_seller_signals > task->wait_trusted_signals) {
    slowest_input = ScoreAdInput::kDirectFromSellerSignals;
    slowest_input_time = task->wait_direct_from_seller_signals;
  }
  base::UmaHistogramEnumeration("Ads.InterestGroup.Auction.ScoreAdSlowestInput",
                                slowest_input);
  base::UmaHistogramTimes("Ads.InterestGroup.Auction.ScoreAdInputWaitTime",
                          slowest_input_time);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "post_v8_task", task->trace_id);

  // Normally the PostTask below will eventually get `task` cleaned up once it
  // posts back to DeliverScoreAdCallbackOnUserThread with its results, but that
  // won't happen if it gets cancelled. To deal with that, a ScopedClosureRunner
  // is passed to ask for `task` to get cleaned up in case the V8State::ScoreAd
  // closure gets destroyed without running.
  base::OnceClosure cleanup_score_ad_task = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&SellerWorklet::CleanUpScoreAdTaskOnUserThread,
                     weak_ptr_factory_.GetWeakPtr(), task));

  int thread_index = get_next_thread_index_callback_.Run();
  task->score_ad_start_time = base::TimeTicks::Now();
  task->task_id = cancelable_task_tracker_.PostTask(
      v8_runners_[thread_index].get(), FROM_HERE,
      base::BindOnce(
          &SellerWorklet::V8State::ScoreAd,
          base::Unretained(v8_state_[thread_index].get()),
          task->ad_metadata_json, task->bid, std::move(task->bid_currency),
          std::move(task->auction_ad_config_non_shared_params),
          std::move(task->direct_from_seller_result_seller_signals),
          std::move(task->direct_from_seller_seller_signals_header_ad_slot),
          std::move(task->direct_from_seller_result_auction_signals),
          std::move(task->direct_from_seller_auction_signals_header_ad_slot),
          std::move(task->trusted_scoring_signals_result),
          task->trusted_bidding_signals_fetch_failed,
          std::move(task->browser_signals_other_seller),
          std::move(task->component_expect_bid_currency),
          std::move(task->browser_signal_interest_group_owner),
          std::move(task->browser_signal_render_url),
          std::move(
              task->browser_signal_selected_buyer_and_seller_reporting_id),
          std::move(task->browser_signal_buyer_and_seller_reporting_id),
          std::move(task->browser_signal_ad_components),
          task->browser_signal_bidding_duration_msecs,
          std::move(task->browser_signal_render_size),
          task->browser_signal_for_debugging_only_in_cooldown_or_lockout,
          std::move(task->seller_timeout), task->trace_id,
          base::ScopedClosureRunner(std::move(cleanup_score_ad_task)),
          /*task_enqueued_time=*/base::TimeTicks::Now(),
          base::BindOnce(&SellerWorklet::DeliverScoreAdCallbackOnUserThread,
                         weak_ptr_factory_.GetWeakPtr(), task)));
}

void SellerWorklet::DeliverScoreAdCallbackOnUserThread(
    ScoreAdTaskList::iterator task,
    double score,
    mojom::RejectReason reject_reason,
    mojom::ComponentAuctionModifiedBidParamsPtr
        component_auction_modified_bid_params,
    std::optional<double> bid_in_seller_currency,
    std::optional<uint32_t> scoring_signals_data_version,
    std::optional<GURL> debug_loss_report_url,
    std::optional<GURL> debug_win_report_url,
    PrivateAggregationRequests pa_requests,
    RealTimeReportingContributions real_time_contributions,
    base::TimeDelta scoring_latency,
    bool script_timed_out,
    std::vector<std::string> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  if (load_script_error_msg_) {
    errors.insert(errors.begin(), load_script_error_msg_.value());
  }
  if (task->trusted_scoring_signals_error_msg) {
    errors.insert(errors.begin(), *task->trusted_scoring_signals_error_msg);
  }

  // This is safe to do, even if the pipe was closed - the message will just be
  // dropped.
  //
  // TOOD(mmenke): Consider watching for the pipe closing and aborting work if
  // it does. Only useful if the SellerWorklet object is still in use, so
  // unclear how useful it would be.
  task->score_ad_client->OnScoreAdComplete(
      score, reject_reason, std::move(component_auction_modified_bid_params),
      std::move(bid_in_seller_currency), scoring_signals_data_version,
      debug_loss_report_url, debug_win_report_url, std::move(pa_requests),
      std::move(real_time_contributions),
      mojom::SellerTimingMetrics::New(
          /*js_fetch_latency=*/js_fetch_latency_,
          /*script_latency=*/scoring_latency,
          /*script_timed_out=*/script_timed_out),
      mojom::ScoreAdDependencyLatencies::New(
          /*code_ready_latency=*/NullOptIfZero(task->wait_code),
          /*direct_from_seller_signals_latency=*/
          NullOptIfZero(task->wait_direct_from_seller_signals),
          /*trusted_scoring_signals_latency=*/
          NullOptIfZero(task->wait_trusted_signals),
          /*deps_wait_start_time=*/task->trace_wait_deps_start,
          /*score_ad_start_time=*/task->score_ad_start_time,
          /*score_ad_finish_time=*/base::TimeTicks::Now()),
      std::move(errors));
  score_ad_tasks_.erase(task);
}

void SellerWorklet::CleanUpScoreAdTaskOnUserThread(
    ScoreAdTaskList::iterator task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  score_ad_tasks_.erase(task);
}

void SellerWorklet::OnDirectFromSellerSellerSignalsDownloadedReportResult(
    ReportResultTaskList::iterator task,
    DirectFromSellerSignalsRequester::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  task->direct_from_seller_result_seller_signals = std::move(result);
  task->direct_from_seller_request_seller_signals.reset();
  // The two direct from seller signals metrics for tracing are combined since
  // they should be roughly the same.
  task->wait_direct_from_seller_signals =
      std::max(task->wait_direct_from_seller_signals,
               base::TimeTicks::Now() - task->trace_wait_deps_start);

  RunReportResultIfReady(task);
}

void SellerWorklet::OnDirectFromSellerAuctionSignalsDownloadedReportResult(
    ReportResultTaskList::iterator task,
    DirectFromSellerSignalsRequester::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  task->direct_from_seller_result_auction_signals = std::move(result);
  task->direct_from_seller_request_auction_signals.reset();
  // The two direct from seller signals metrics for tracing are combined since
  // they should be roughly the same.
  task->wait_direct_from_seller_signals =
      std::max(task->wait_direct_from_seller_signals,
               base::TimeTicks::Now() - task->trace_wait_deps_start);

  RunReportResultIfReady(task);
}

bool SellerWorklet::IsReadyToReportResult(const ReportResultTask& task) const {
  return IsCodeReady() && !task.direct_from_seller_request_seller_signals &&
         !task.direct_from_seller_request_auction_signals;
}

void SellerWorklet::RunReportResultIfReady(
    ReportResultTaskList::iterator task) {
  if (!IsReadyToReportResult(*task)) {
    return;
  }

  TRACE_EVENT_NESTABLE_ASYNC_END1(
      "fledge", "wait_report_result_deps", task->trace_id, "data",
      [&](perfetto::TracedValue trace_context) {
        auto dict = std::move(trace_context).WriteDictionary();
        if (!task->wait_code.is_zero()) {
          dict.Add("wait_code_ms", task->wait_code.InMillisecondsF());
        }
        if (!task->wait_direct_from_seller_signals.is_zero()) {
          dict.Add("wait_direct_from_seller_signals_ms",
                   task->wait_direct_from_seller_signals.InMillisecondsF());
        }
      });
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "post_v8_task", task->trace_id);

  size_t thread_index = get_next_thread_index_callback_.Run();
  cancelable_task_tracker_.PostTask(
      v8_runners_[thread_index].get(), FROM_HERE,
      base::BindOnce(
          &SellerWorklet::V8State::ReportResult,
          base::Unretained(v8_state_[thread_index].get()),
          std::move(task->auction_ad_config_non_shared_params),
          std::move(task->direct_from_seller_result_seller_signals),
          std::move(task->direct_from_seller_seller_signals_header_ad_slot),
          std::move(task->direct_from_seller_result_auction_signals),
          std::move(task->direct_from_seller_auction_signals_header_ad_slot),
          std::move(task->browser_signals_other_seller),
          std::move(task->browser_signal_interest_group_owner),
          std::move(task->browser_signal_buyer_and_seller_reporting_id),
          std::move(
              task->browser_signal_selected_buyer_and_seller_reporting_id),
          std::move(task->browser_signal_render_url), task->browser_signal_bid,
          std::move(task->browser_signal_bid_currency),
          task->browser_signal_desirability,
          task->browser_signal_highest_scoring_other_bid,
          std::move(task->browser_signal_highest_scoring_other_bid_currency),
          std::move(
              task->browser_signals_component_auction_report_result_params),
          task->scoring_signals_data_version, task->trace_id,
          base::BindOnce(
              &SellerWorklet::DeliverReportResultCallbackOnUserThread,
              weak_ptr_factory_.GetWeakPtr(), task)));
}

void SellerWorklet::DeliverReportResultCallbackOnUserThread(
    ReportResultTaskList::iterator task,
    const std::optional<std::string> signals_for_winner,
    const std::optional<GURL> report_url,
    base::flat_map<std::string, GURL> ad_beacon_map,
    PrivateAggregationRequests pa_requests,
    base::TimeDelta reporting_latency,
    bool script_timed_out,
    std::vector<std::string> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  if (load_script_error_msg_) {
    errors.insert(errors.begin(), load_script_error_msg_.value());
  }

  std::move(task->callback)
      .Run(signals_for_winner, report_url, ad_beacon_map,
           std::move(pa_requests),
           mojom::SellerTimingMetrics::New(
               /*js_fetch_latency=*/js_fetch_latency_,
               /*script_latency=*/reporting_latency,
               /*script_timed_out=*/script_timed_out),
           std::move(errors));
  report_result_tasks_.erase(task);
}

bool SellerWorklet::IsCodeReady() const {
  return (!paused_ && !worklet_loader_);
}

}  // namespace auction_worklet
