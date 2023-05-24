// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/seller_worklet.h"

#include <stdint.h>

#include <cmath>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/context_recycler.h"
#include "content/services/auction_worklet/direct_from_seller_signals_requester.h"
#include "content/services/auction_worklet/for_debugging_only_bindings.h"
#include "content/services/auction_worklet/private_aggregation_bindings.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "content/services/auction_worklet/register_ad_beacon_bindings.h"
#include "content/services/auction_worklet/report_bindings.h"
#include "content/services/auction_worklet/shared_storage_bindings.h"
#include "content/services/auction_worklet/trusted_signals.h"
#include "content/services/auction_worklet/worklet_loader.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/ad_auction_currencies.h"
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

// TODO(crbug.com/1441988): Remove this code after rename. These functions allow
// having multiple dictionary keys (e.g. renderUrl and renderURL) share the same
// V8 value.
bool SetDictMember(v8::Isolate* isolate,
                   v8::Local<v8::Object> object,
                   const std::string& key,
                   v8::Local<v8::Value> v8_value) {
  v8::Maybe<bool> result = object->Set(isolate->GetCurrentContext(),
                                       gin::StringToV8(isolate, key), v8_value);
  return !result.IsNothing() && result.FromJust();
}

bool SetRenderUrl(v8::Isolate* isolate,
                  v8::Local<v8::Object> object,
                  const std::string& val) {
  v8::Local<v8::Value> v8_value;
  if (!gin::TryConvertToV8(isolate, val, &v8_value)) {
    return false;
  }
  return SetDictMember(isolate, object, "renderURL", v8_value) &&
         SetDictMember(isolate, object, "renderUrl", v8_value);
}

bool SetDecisionLogicUrl(v8::Isolate* isolate,
                         v8::Local<v8::Object> object,
                         const std::string& val) {
  v8::Local<v8::Value> v8_value;
  if (!gin::TryConvertToV8(isolate, val, &v8_value)) {
    return false;
  }
  return SetDictMember(isolate, object, "decisionLogicURL", v8_value) &&
         SetDictMember(isolate, object, "decisionLogicUrl", v8_value);
}

bool SetTrustedScoringSignalsUrl(v8::Isolate* isolate,
                                 v8::Local<v8::Object> object,
                                 const std::string& val) {
  v8::Local<v8::Value> v8_value;
  if (!gin::TryConvertToV8(isolate, val, &v8_value)) {
    return false;
  }
  return SetDictMember(isolate, object, "trustedScoringSignalsURL", v8_value) &&
         SetDictMember(isolate, object, "trustedScoringSignalsUrl", v8_value);
}

bool InsertPrioritySignals(
    AuctionV8Helper* v8_helper,
    base::StringPiece key,
    const base::flat_map<std::string, double>& priority_signals,
    v8::Local<v8::Object> object) {
  v8::Isolate* isolate = v8_helper->isolate();
  v8::Local<v8::Object> v8_priority_signals = v8::Object::New(isolate);
  for (const auto& signal : priority_signals) {
    if (!v8_helper->InsertValue(signal.first,
                                v8::Number::New(isolate, signal.second),
                                v8_priority_signals)) {
      return false;
    }
  }
  return v8_helper->InsertValue(key, v8_priority_signals, object);
}

// Attempts to create an v8 Object from `maybe_promise_buyer_timeouts`. On fatal
// error, returns false. Otherwise, writes the result to
// `out_per_buyer_timeouts`, which will be left unchanged if there are no times
// to write to it.
bool CreatePerBuyerTimeoutsObject(
    v8::Isolate* isolate,
    const blink::AuctionConfig::MaybePromiseBuyerTimeouts&
        maybe_promise_buyer_timeouts,
    v8::Local<v8::Object>& out_per_buyer_timeouts) {
  DCHECK(!maybe_promise_buyer_timeouts.is_promise());

  const blink::AuctionConfig::BuyerTimeouts& buyer_timeouts =
      maybe_promise_buyer_timeouts.value();
  // If there are no times, leave `out_per_buyer_timeouts` empty, and indicate
  // success.
  if (!buyer_timeouts.per_buyer_timeouts.has_value() &&
      !buyer_timeouts.all_buyers_timeout.has_value()) {
    return true;
  }

  out_per_buyer_timeouts = v8::Object::New(isolate);
  gin::Dictionary per_buyer_timeouts_dict(isolate, out_per_buyer_timeouts);

  if (buyer_timeouts.per_buyer_timeouts.has_value()) {
    for (const auto& kv : buyer_timeouts.per_buyer_timeouts.value()) {
      if (!per_buyer_timeouts_dict.Set(kv.first.Serialize(),
                                       kv.second.InMilliseconds())) {
        return false;
      }
    }
  }
  if (buyer_timeouts.all_buyers_timeout.has_value()) {
    if (!per_buyer_timeouts_dict.Set(
            "*", buyer_timeouts.all_buyers_timeout.value().InMilliseconds())) {
      return false;
    }
  }
  return true;
}

// Attempts to create an v8 Object from `maybe_promise_buyer_currencies`. On
//  fatal error, returns false. Otherwise, writes the result to
// `out_per_buyer_currencies`, which will be left unchanged if there are no
// currencies to write to it.
bool CreatePerBuyerCurrenciesObject(
    v8::Isolate* isolate,
    const blink::AuctionConfig::MaybePromiseBuyerCurrencies&
        maybe_promise_buyer_currencies,
    v8::Local<v8::Object>& out_per_buyer_currencies) {
  DCHECK(!maybe_promise_buyer_currencies.is_promise());

  const blink::AuctionConfig::BuyerCurrencies& buyer_currencies =
      maybe_promise_buyer_currencies.value();
  // If there is nothing specified, leave `out_per_buyer_currencies` empty, and
  // indicate success.
  if (!buyer_currencies.per_buyer_currencies.has_value() &&
      !buyer_currencies.all_buyers_currency.has_value()) {
    return true;
  }

  out_per_buyer_currencies = v8::Object::New(isolate);
  gin::Dictionary per_buyer_currencies_dict(isolate, out_per_buyer_currencies);

  if (buyer_currencies.per_buyer_currencies.has_value()) {
    for (const auto& kv : buyer_currencies.per_buyer_currencies.value()) {
      if (!per_buyer_currencies_dict.Set(kv.first.Serialize(),
                                         kv.second.currency_code())) {
        return false;
      }
    }
  }
  if (buyer_currencies.all_buyers_currency.has_value()) {
    if (!per_buyer_currencies_dict.Set(
            "*", buyer_currencies.all_buyers_currency->currency_code())) {
      return false;
    }
  }
  return true;
}

// ### some duplication with same in interest_group_auction.cc
bool IsValidBid(double bid) {
  return std::isfinite(bid) && (bid > 0.0);
}

// Converts `auction_config` back to JSON format, and appends to args.
// Returns true if conversion succeeded.
//
// The resulting object will look something like this (based on example from
// explainer):
//
// {
//  'seller': 'https://www.example-ssp.com/',
//  'decisionLogicUrl': 'https://www.example-ssp.com/seller.js',
//  'trustedScoringSignalsUrl': ...,
//  'interestGroupBuyers': ['https://www.example-dsp.com', 'https://buyer2.com',
//  ...], 'auctionSignals': {...}, 'sellerSignals': {...}, 'sellerTimeout': 100,
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
bool AppendAuctionConfig(AuctionV8Helper* v8_helper,
                         v8::Local<v8::Context> context,
                         const GURL& decision_logic_url,
                         const absl::optional<GURL>& trusted_coding_signals_url,
                         const absl::optional<uint16_t> experiment_group_id,
                         const blink::AuctionConfig::NonSharedParams&
                             auction_ad_config_non_shared_params,
                         std::vector<v8::Local<v8::Value>>* args) {
  v8::Isolate* isolate = v8_helper->isolate();
  v8::Local<v8::Object> auction_config_value = v8::Object::New(isolate);
  gin::Dictionary auction_config_dict(isolate, auction_config_value);
  if (!auction_config_dict.Set(
          "seller", url::Origin::Create(decision_logic_url).Serialize()) ||
      !SetDecisionLogicUrl(isolate, auction_config_value,
                           decision_logic_url.spec()) ||
      (trusted_coding_signals_url &&
       !SetTrustedScoringSignalsUrl(isolate, auction_config_value,
                                    trusted_coding_signals_url->spec()))) {
    return false;
  }

  if (auction_ad_config_non_shared_params.interest_group_buyers) {
    std::vector<v8::Local<v8::Value>> interest_group_buyers;
    for (const url::Origin& buyer :
         *auction_ad_config_non_shared_params.interest_group_buyers) {
      v8::Local<v8::String> v8_buyer;
      if (!v8_helper->CreateUtf8String(buyer.Serialize()).ToLocal(&v8_buyer))
        return false;
      interest_group_buyers.push_back(v8_buyer);
    }
    auction_config_dict.Set("interestGroupBuyers", interest_group_buyers);
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
      !v8_helper->InsertJsonValue(
          context, "sellerTimeout",
          base::NumberToString(
              auction_ad_config_non_shared_params.seller_timeout.value()
                  .InMilliseconds()),
          auction_config_value)) {
    return false;
  }

  DCHECK(!auction_ad_config_non_shared_params.per_buyer_signals.is_promise());
  if (auction_ad_config_non_shared_params.per_buyer_signals.value()
          .has_value()) {
    v8::Local<v8::Object> per_buyer_value = v8::Object::New(isolate);
    for (const auto& kv :
         auction_ad_config_non_shared_params.per_buyer_signals.value()
             .value()) {
      if (!v8_helper->InsertJsonValue(context, kv.first.Serialize(), kv.second,
                                      per_buyer_value)) {
        return false;
      }
    }
    auction_config_dict.Set("perBuyerSignals", per_buyer_value);
  }

  v8::Local<v8::Object> per_buyer_timeouts;
  if (!CreatePerBuyerTimeoutsObject(
          isolate, auction_ad_config_non_shared_params.buyer_timeouts,
          per_buyer_timeouts)) {
    return false;
  }
  if (!per_buyer_timeouts.IsEmpty()) {
    auction_config_dict.Set("perBuyerTimeouts", per_buyer_timeouts);
  }

  v8::Local<v8::Object> per_buyer_cumulative_timeouts;
  if (!CreatePerBuyerTimeoutsObject(
          isolate,
          auction_ad_config_non_shared_params.buyer_cumulative_timeouts,
          per_buyer_cumulative_timeouts)) {
    return false;
  }
  if (!per_buyer_cumulative_timeouts.IsEmpty()) {
    auction_config_dict.Set("perBuyerCumulativeTimeouts",
                            per_buyer_cumulative_timeouts);
  }

  v8::Local<v8::Object> per_buyer_currencies;
  if (!CreatePerBuyerCurrenciesObject(
          isolate, auction_ad_config_non_shared_params.buyer_currencies,
          per_buyer_currencies)) {
    return false;
  }

  if (!per_buyer_currencies.IsEmpty()) {
    auction_config_dict.Set("perBuyerCurrencies", per_buyer_currencies);
  }

  if (auction_ad_config_non_shared_params.seller_currency.has_value()) {
    auction_config_dict.Set(
        "sellerCurrency",
        auction_ad_config_non_shared_params.seller_currency->currency_code());
  }

  if (auction_ad_config_non_shared_params.per_buyer_priority_signals ||
      auction_ad_config_non_shared_params.all_buyers_priority_signals) {
    v8::Local<v8::Object> per_buyer_priority_signals = v8::Object::New(isolate);
    if (auction_ad_config_non_shared_params.per_buyer_priority_signals) {
      for (const auto& kv :
           *auction_ad_config_non_shared_params.per_buyer_priority_signals) {
        if (!InsertPrioritySignals(v8_helper, kv.first.Serialize(), kv.second,
                                   per_buyer_priority_signals)) {
          return false;
        }
      }
    }
    if (auction_ad_config_non_shared_params.all_buyers_priority_signals) {
      if (!InsertPrioritySignals(
              v8_helper, "*",
              *auction_ad_config_non_shared_params.all_buyers_priority_signals,
              per_buyer_priority_signals)) {
        return false;
      }
    }
    auction_config_dict.Set("perBuyerPrioritySignals",
                            per_buyer_priority_signals);
  }

  const auto& component_auctions =
      auction_ad_config_non_shared_params.component_auctions;
  if (!component_auctions.empty()) {
    std::vector<v8::Local<v8::Value>> component_auction_vector;
    for (const auto& component_auction : component_auctions) {
      if (!AppendAuctionConfig(
              v8_helper, context, component_auction.decision_logic_url,
              component_auction.trusted_scoring_signals_url,
              experiment_group_id, component_auction.non_shared_params,
              &component_auction_vector)) {
        return false;
      }
    }
    v8::Maybe<bool> result = auction_config_value->Set(
        context, v8_helper->CreateStringFromLiteral("componentAuctions"),
        v8::Array::New(isolate, component_auction_vector.data(),
                       component_auction_vector.size()));
    if (result.IsNothing() || !result.FromJust())
      return false;
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
  if (!browser_signals_other_seller)
    return true;
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
absl::optional<mojom::RejectReason> RejectReasonStringToEnum(
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
  return absl::nullopt;
}

// Checks `provided_currency` against both `expected_seller_currency` and
// `component_expect_bid_currency`, formatting an error if needed, with
// `bid_label` identifying the bid being checked.
// Returns true on success.
bool VerifySellerCurrency(
    absl::optional<blink::AdCurrency> provided_currency,
    absl::optional<blink::AdCurrency> expected_seller_currency,
    absl::optional<blink::AdCurrency> component_expect_bid_currency,
    const GURL& script_url,
    base::StringPiece bid_label,
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

}  // namespace

SellerWorklet::SellerWorklet(
    scoped_refptr<AuctionV8Helper> v8_helper,
    mojo::PendingRemote<mojom::AuctionSharedStorageHost>
        shared_storage_host_remote,
    bool pause_for_debugger_on_start,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_url_loader_factory,
    const GURL& decision_logic_url,
    const absl::optional<GURL>& trusted_scoring_signals_url,
    const url::Origin& top_window_origin,
    mojom::AuctionWorkletPermissionsPolicyStatePtr permissions_policy_state,
    absl::optional<uint16_t> experiment_group_id)
    : v8_runner_(v8_helper->v8_runner()),
      v8_helper_(std::move(v8_helper)),
      debug_id_(
          base::MakeRefCounted<AuctionV8Helper::DebugId>(v8_helper_.get())),
      url_loader_factory_(std::move(pending_url_loader_factory)),
      script_source_url_(decision_logic_url),
      trusted_signals_request_manager_(
          trusted_scoring_signals_url
              ? std::make_unique<TrustedSignalsRequestManager>(
                    TrustedSignalsRequestManager::Type::kScoringSignals,
                    url_loader_factory_.get(),
                    /*automatically_send_requests=*/true,
                    top_window_origin,
                    *trusted_scoring_signals_url,
                    /*experiment_group_id=*/experiment_group_id,
                    v8_helper_.get())
              : nullptr),
      v8_state_(nullptr, base::OnTaskRunnerDeleter(v8_runner_)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  v8_state_ = std::unique_ptr<V8State, base::OnTaskRunnerDeleter>(
      new V8State(v8_helper_, debug_id_, std::move(shared_storage_host_remote),
                  decision_logic_url, trusted_scoring_signals_url,
                  top_window_origin, std::move(permissions_policy_state),
                  experiment_group_id, weak_ptr_factory_.GetWeakPtr()),
      base::OnTaskRunnerDeleter(v8_runner_));

  paused_ = pause_for_debugger_on_start;
  if (!paused_)
    Start();
}

SellerWorklet::~SellerWorklet() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  debug_id_->AbortDebuggerPauses();
}

int SellerWorklet::context_group_id_for_testing() const {
  return debug_id_->context_group_id();
}

void SellerWorklet::ScoreAd(
    const std::string& ad_metadata_json,
    double bid,
    const absl::optional<blink::AdCurrency>& bid_currency,
    const blink::AuctionConfig::NonSharedParams&
        auction_ad_config_non_shared_params,
    const absl::optional<GURL>& direct_from_seller_seller_signals,
    const absl::optional<GURL>& direct_from_seller_auction_signals,
    mojom::ComponentAuctionOtherSellerPtr browser_signals_other_seller,
    const absl::optional<blink::AdCurrency>& component_expect_bid_currency,
    const url::Origin& browser_signal_interest_group_owner,
    const GURL& browser_signal_render_url,
    const std::vector<GURL>& browser_signal_ad_components,
    uint32_t browser_signal_bidding_duration_msecs,
    const absl::optional<base::TimeDelta> seller_timeout,
    uint64_t trace_id,
    mojo::PendingRemote<auction_worklet::mojom::ScoreAdClient>
        score_ad_client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
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
  score_ad_task->browser_signal_render_url = browser_signal_render_url;
  for (const GURL& url : browser_signal_ad_components) {
    score_ad_task->browser_signal_ad_components.emplace_back(url.spec());
  }
  score_ad_task->browser_signal_bidding_duration_msecs =
      browser_signal_bidding_duration_msecs;
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

  score_ad_task->trace_wait_deps_start = base::TimeTicks::Now();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "wait_score_ad_deps", trace_id);

  // If `trusted_signals_request_manager_` exists, there's a trusted scoring
  // signals URL which needs to be fetched before the auction can be run.
  if (trusted_signals_request_manager_) {
    score_ad_task->trusted_scoring_signals_request =
        trusted_signals_request_manager_->RequestScoringSignals(
            browser_signal_render_url,
            score_ad_task->browser_signal_ad_components,
            base::BindOnce(&SellerWorklet::OnTrustedScoringSignalsDownloaded,
                           base::Unretained(this), score_ad_task));
    return;
  }

  ScoreAdIfReady(score_ad_task);
}

void SellerWorklet::SendPendingSignalsRequests() {
  if (trusted_signals_request_manager_)
    trusted_signals_request_manager_->StartBatchedTrustedSignalsRequest();
}

void SellerWorklet::ReportResult(
    const blink::AuctionConfig::NonSharedParams&
        auction_ad_config_non_shared_params,
    const absl::optional<GURL>& direct_from_seller_seller_signals,
    const absl::optional<GURL>& direct_from_seller_auction_signals,
    mojom::ComponentAuctionOtherSellerPtr browser_signals_other_seller,
    const url::Origin& browser_signal_interest_group_owner,
    const absl::optional<std::string>&
        browser_signal_buyer_and_seller_reporting_id,
    const GURL& browser_signal_render_url,
    double browser_signal_bid,
    const absl::optional<blink::AdCurrency>& browser_signal_bid_currency,
    double browser_signal_desirability,
    double browser_signal_highest_scoring_other_bid,
    const absl::optional<blink::AdCurrency>&
        browser_signal_highest_scoring_other_bid_currency,
    auction_worklet::mojom::ComponentAuctionReportResultParamsPtr
        browser_signals_component_auction_report_result_params,
    uint32_t scoring_signals_data_version,
    bool has_scoring_signals_data_version,
    uint64_t trace_id,
    ReportResultCallback callback) {
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

  if (has_scoring_signals_data_version) {
    report_result_task->scoring_signals_data_version =
        scoring_signals_data_version;
  }
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

  report_result_task->trace_wait_deps_start = base::TimeTicks::Now();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "wait_report_result_deps",
                                    trace_id);
  RunReportResultIfReady(report_result_task);
}

void SellerWorklet::ConnectDevToolsAgent(
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  v8_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&V8State::ConnectDevToolsAgent,
                     base::Unretained(v8_state_.get()), std::move(agent)));
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
    const absl::optional<GURL>& trusted_scoring_signals_url,
    const url::Origin& top_window_origin,
    mojom::AuctionWorkletPermissionsPolicyStatePtr permissions_policy_state,
    absl::optional<uint16_t> experiment_group_id,
    base::WeakPtr<SellerWorklet> parent)
    : v8_helper_(std::move(v8_helper)),
      debug_id_(debug_id),
      parent_(std::move(parent)),
      user_thread_(base::SequencedTaskRunner::GetCurrentDefault()),
      decision_logic_url_(decision_logic_url),
      trusted_scoring_signals_url_(trusted_scoring_signals_url),
      top_window_origin_(top_window_origin),
      permissions_policy_state_(std::move(permissions_policy_state)),
      experiment_group_id_(experiment_group_id) {
  DETACH_FROM_SEQUENCE(v8_sequence_checker_);
  v8_helper_->v8_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V8State::FinishInit, base::Unretained(this),
                                std::move(shared_storage_host_remote)));
}

void SellerWorklet::V8State::SetWorkletScript(
    WorkletLoader::Result worklet_script) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  worklet_script_ = WorkletLoader::TakeScript(std::move(worklet_script));
}

void SellerWorklet::V8State::ScoreAd(
    const std::string& ad_metadata_json,
    double bid,
    const absl::optional<blink::AdCurrency>& bid_currency,
    const blink::AuctionConfig::NonSharedParams&
        auction_ad_config_non_shared_params,
    DirectFromSellerSignalsRequester::Result
        direct_from_seller_result_seller_signals,
    DirectFromSellerSignalsRequester::Result
        direct_from_seller_result_auction_signals,
    scoped_refptr<TrustedSignals::Result> trusted_scoring_signals,
    mojom::ComponentAuctionOtherSellerPtr browser_signals_other_seller,
    const absl::optional<blink::AdCurrency>& component_expect_bid_currency,
    const url::Origin& browser_signal_interest_group_owner,
    const GURL& browser_signal_render_url,
    const std::vector<std::string>& browser_signal_ad_components,
    uint32_t browser_signal_bidding_duration_msecs,
    const absl::optional<base::TimeDelta> seller_timeout,
    uint64_t trace_id,
    base::TimeDelta trusted_signals_fetch_latency,
    base::ScopedClosureRunner cleanup_score_ad_task,
    ScoreAdCallbackInternal callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  base::ElapsedTimer elapsed_timer;

  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "post_v8_task", trace_id);

  // Don't need to run `cleanup_score_ad_task` if this method is invoked;
  // it's bound to the closure to clean things up if this method got cancelled.
  cleanup_score_ad_task.ReplaceClosure(base::OnceClosure());

  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper_.get());
  v8::Isolate* isolate = v8_helper_->isolate();

  // Short lived context, to avoid leaking data at global scope between either
  // repeated calls to this worklet, or to calls to any other worklet.
  ContextRecycler context_recycler(v8_helper_.get());
  ContextRecyclerScope context_recycler_scope(context_recycler);
  v8::Local<v8::Context> context = context_recycler_scope.GetContext();

  std::vector<v8::Local<v8::Value>> args;
  if (!v8_helper_->AppendJsonValue(context, ad_metadata_json, &args)) {
    PostScoreAdCallbackToUserThreadOnError(
        std::move(callback),
        /*errors=*/std::vector<std::string>());
    return;
  }

  args.push_back(gin::ConvertToV8(isolate, bid));

  if (!AppendAuctionConfig(v8_helper_.get(), context, decision_logic_url_,
                           trusted_scoring_signals_url_, experiment_group_id_,
                           auction_ad_config_non_shared_params, &args)) {
    PostScoreAdCallbackToUserThreadOnError(
        std::move(callback),
        /*errors=*/std::vector<std::string>());
    return;
  }

  v8::Local<v8::Value> trusted_scoring_signals_value;
  absl::optional<uint32_t> scoring_signals_data_version;
  if (trusted_scoring_signals) {
    trusted_scoring_signals_value = trusted_scoring_signals->GetScoringSignals(
        v8_helper_.get(), context, browser_signal_render_url,
        browser_signal_ad_components);
    scoring_signals_data_version = trusted_scoring_signals->GetDataVersion();
  } else {
    trusted_scoring_signals_value = v8::Null(isolate);
  }
  args.push_back(trusted_scoring_signals_value);

  v8::Local<v8::Object> browser_signals = v8::Object::New(isolate);
  gin::Dictionary browser_signals_dict(isolate, browser_signals);
  if (!browser_signals_dict.Set("topWindowHostname",
                                top_window_origin_.host()) ||
      !AddOtherSeller(browser_signals_other_seller.get(),
                      browser_signals_dict) ||
      !browser_signals_dict.Set(
          "interestGroupOwner",
          browser_signal_interest_group_owner.Serialize()) ||
      // TODO(crbug.com/1441988): Remove deprecated `renderUrl` alias.
      !SetRenderUrl(isolate, browser_signals,
                    browser_signal_render_url.spec()) ||
      !browser_signals_dict.Set("biddingDurationMsec",
                                browser_signal_bidding_duration_msecs) ||
      !browser_signals_dict.Set("bidCurrency",
                                blink::PrintableAdCurrency(bid_currency)) ||
      (scoring_signals_data_version.has_value() &&
       !browser_signals_dict.Set("dataVersion",
                                 scoring_signals_data_version.value()))) {
    PostScoreAdCallbackToUserThreadOnError(
        std::move(callback),
        /*errors=*/std::vector<std::string>());
    return;
  }
  if (!browser_signal_ad_components.empty()) {
    if (!browser_signals_dict.Set("adComponents",
                                  browser_signal_ad_components)) {
      PostScoreAdCallbackToUserThreadOnError(
          std::move(callback),
          /*errors=*/std::vector<std::string>());
      return;
    }
  }
  args.push_back(browser_signals);

  v8::Local<v8::Object> direct_from_seller_signals = v8::Object::New(isolate);
  gin::Dictionary direct_from_seller_signals_dict(isolate,
                                                  direct_from_seller_signals);
  std::vector<std::string> errors_out;
  v8::Local<v8::Value> seller_signals =
      direct_from_seller_result_seller_signals.GetSignals(*v8_helper_, context,
                                                          errors_out);
  v8::Local<v8::Value> auction_signals =
      direct_from_seller_result_auction_signals.GetSignals(*v8_helper_, context,
                                                           errors_out);
  if (!direct_from_seller_signals_dict.Set("sellerSignals", seller_signals) ||
      !direct_from_seller_signals_dict.Set("auctionSignals", auction_signals)) {
    PostScoreAdCallbackToUserThreadOnError(std::move(callback),
                                           /*errors=*/std::move(errors_out));
    return;
  }
  args.push_back(direct_from_seller_signals);

  v8::Local<v8::Value> score_ad_result;
  v8_helper_->MaybeTriggerInstrumentationBreakpoint(
      *debug_id_, "beforeSellerWorkletScoringStart");

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "score_ad", trace_id);
  v8::Local<v8::UnboundScript> unbound_worklet_script =
      worklet_script_.Get(isolate);
  bool success =
      v8_helper_->RunScript(context, unbound_worklet_script, debug_id_.get(),
                            seller_timeout, errors_out);
  if (!success) {
    TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "score_ad", trace_id);
    // Keep debug loss reports and Private Aggregation API requests since
    // `scoreAd()` might use them to detect script timeout or failures.
    PostScoreAdCallbackToUserThread(
        std::move(callback), /*score=*/0,
        /*reject_reason=*/mojom::RejectReason::kNotAvailable,
        /*component_auction_modified_bid_params=*/nullptr,
        /*bid_in_seller_currency=*/absl::nullopt,
        /*scoring_signals_data_version=*/absl::nullopt,
        /*debug_loss_report_url=*/absl::nullopt,
        /*debug_win_report_url=*/absl::nullopt,
        /*pa_requests=*/{},
        /*scoring_latency=*/elapsed_timer.Elapsed(),
        /*trusted_signals_fetch_latency=*/trusted_signals_fetch_latency,
        std::move(errors_out));
    return;
  }
  context_recycler.AddForDebuggingOnlyBindings();
  context_recycler.AddPrivateAggregationBindings(
      permissions_policy_state_->private_aggregation_allowed);

  if (base::FeatureList::IsEnabled(blink::features::kSharedStorageAPI)) {
    context_recycler.AddSharedStorageBindings(
        shared_storage_host_remote_.is_bound()
            ? shared_storage_host_remote_.get()
            : nullptr,
        permissions_policy_state_->shared_storage_allowed);
  }

  success =
      v8_helper_
          ->CallFunction(context, debug_id_.get(),
                         v8_helper_->FormatScriptName(unbound_worklet_script),
                         "scoreAd", args, std::move(seller_timeout), errors_out)
          .ToLocal(&score_ad_result);

  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "score_ad", trace_id);
  base::TimeDelta elapsed = elapsed_timer.Elapsed();
  base::UmaHistogramTimes("Ads.InterestGroup.Auction.ScoreAdTime", elapsed);

  if (!success) {
    // Keep debug loss reports and Private Aggregation API requests since
    // `scoreAd()` might use them to detect script timeout or failures.
    PostScoreAdCallbackToUserThread(
        std::move(callback), /*score=*/0,
        /*reject_reason=*/mojom::RejectReason::kNotAvailable,
        /*component_auction_modified_bid_params=*/nullptr,
        /*bid_in_seller_currency=*/absl::nullopt,
        /*scoring_signals_data_version=*/absl::nullopt,
        /*debug_loss_report_url=*/
        context_recycler.for_debugging_only_bindings()->TakeLossReportUrl(),
        /*debug_win_report_url=*/absl::nullopt,
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        /*scoring_latency=*/elapsed,
        /*trusted_signals_fetch_latency=*/trusted_signals_fetch_latency,
        std::move(errors_out));
    return;
  }

  double score;
  mojom::RejectReason reject_reason = mojom::RejectReason::kNotAvailable;
  bool allow_component_auction = false;
  mojom::ComponentAuctionModifiedBidParamsPtr
      component_auction_modified_bid_params;
  absl::optional<double> bid_in_seller_currency;
  // Try to parse the result as a number. On success, it's the desirability
  // score.
  if (!gin::ConvertFromV8(isolate, score_ad_result, &score)) {
    // Otherwise, it must be an object with the desireability score, and
    // potentially other fields as well.
    if (!score_ad_result->IsObject()) {
      errors_out.push_back(
          base::StrCat({decision_logic_url_.spec(),
                        " scoreAd() did not return an object or a number."}));
      PostScoreAdCallbackToUserThreadOnError(
          std::move(callback), std::move(errors_out),
          context_recycler.private_aggregation_bindings()
              ->TakePrivateAggregationRequests());
      return;
    }

    v8::Local<v8::Object> score_ad_object = score_ad_result.As<v8::Object>();
    gin::Dictionary result_dict(isolate, score_ad_object);
    if (!result_dict.Get("desirability", &score)) {
      errors_out.push_back(
          base::StrCat({decision_logic_url_.spec(),
                        " scoreAd() return value has incorrect structure."}));
      PostScoreAdCallbackToUserThreadOnError(
          std::move(callback), std::move(errors_out),
          context_recycler.private_aggregation_bindings()
              ->TakePrivateAggregationRequests());
      return;
    }

    if (!result_dict.Get("allowComponentAuction", &allow_component_auction))
      allow_component_auction = false;

    // If the bid is already in seller currency, forward it as
    // incomingBidInSellerCurrency.
    if (bid_currency.has_value() &&
        auction_ad_config_non_shared_params.seller_currency.has_value() &&
        bid_currency->currency_code() ==
            auction_ad_config_non_shared_params.seller_currency
                ->currency_code()) {
      bid_in_seller_currency = bid;
    }

    v8::Local<v8::Value> incoming_bid_in_seller_currency_value;
    if (score_ad_object
            ->Get(context, v8_helper_->CreateStringFromLiteral(
                               "incomingBidInSellerCurrency"))
            .ToLocal(&incoming_bid_in_seller_currency_value) &&
        !incoming_bid_in_seller_currency_value->IsUndefined()) {
      bool ok = true;
      double incoming_bid_in_seller_currency = 0.0;

      if (ok &&
          !auction_ad_config_non_shared_params.seller_currency.has_value()) {
        errors_out.push_back(base::StrCat(
            {decision_logic_url_.spec(),
             " scoreAd() attempting to set incomingBidInSellerCurrency without "
             "a configured sellerCurrency."}));
        ok = false;
      }
      if (ok &&
          !gin::ConvertFromV8(isolate, incoming_bid_in_seller_currency_value,
                              &incoming_bid_in_seller_currency)) {
        errors_out.push_back(base::StrCat(
            {decision_logic_url_.spec(),
             " scoreAd() incomingBidInSellerCurrency not a number."}));
        ok = false;
      }
      if (ok && !IsValidBid(incoming_bid_in_seller_currency)) {
        errors_out.push_back(base::StrCat(
            {decision_logic_url_.spec(),
             " scoreAd() incomingBidInSellerCurrency not a valid bid."}));
        ok = false;
      }
      if (bid_in_seller_currency.has_value() &&
          incoming_bid_in_seller_currency != *bid_in_seller_currency) {
        errors_out.push_back(base::StrCat(
            {decision_logic_url_.spec(),
             " scoreAd() attempting to set incomingBidInSellerCurrency "
             "inconsistent with incoming bid already in seller currency."}));
        ok = false;
      }
      if (!ok) {
        PostScoreAdCallbackToUserThreadOnError(
            std::move(callback), std::move(errors_out),
            context_recycler.private_aggregation_bindings()
                ->TakePrivateAggregationRequests());
        return;
      }
      bid_in_seller_currency = incoming_bid_in_seller_currency;
    }

    v8::Local<v8::Value> reject_reason_value;
    if (score_ad_object
            ->Get(context, v8_helper_->CreateStringFromLiteral("rejectReason"))
            .ToLocal(&reject_reason_value) &&
        !reject_reason_value->IsUndefined()) {
      if (!reject_reason_value->IsString()) {
        errors_out.push_back(base::StrCat(
            {decision_logic_url_.spec(),
             " rejectReason returned by scoreAd() must be a string."}));
      } else {
        std::string reject_reason_str;
        result_dict.Get("rejectReason", &reject_reason_str);
        auto reject_reason_opt = RejectReasonStringToEnum(reject_reason_str);

        if (!reject_reason_opt.has_value()) {
          errors_out.push_back(
              base::StrCat({decision_logic_url_.spec(),
                            " scoreAd() returned an invalid reject reason."}));
        } else {
          reject_reason = reject_reason_opt.value();
        }
      }
    }

    // If this is the seller in a component auction (and thus it was passed a
    // top-level seller), need to return a
    // mojom::ComponentAuctionModifiedBidParams.
    if (allow_component_auction && browser_signals_other_seller &&
        browser_signals_other_seller->is_top_level_seller()) {
      component_auction_modified_bid_params =
          mojom::ComponentAuctionModifiedBidParams::New();

      v8::Local<v8::Value> ad_value;
      if (!score_ad_object
               ->Get(context, v8_helper_->CreateStringFromLiteral("ad"))
               .ToLocal(&ad_value) ||
          !v8_helper_->ExtractJson(
              context, ad_value, &component_auction_modified_bid_params->ad)) {
        component_auction_modified_bid_params->ad = "null";
      }

      component_auction_modified_bid_params->bid = 0;
      component_auction_modified_bid_params->has_bid =
          result_dict.Get("bid", &component_auction_modified_bid_params->bid);
      if (component_auction_modified_bid_params->has_bid) {
        bool drop_for_invalid_currency = false;
        v8::Local<v8::Value> bid_currency_value;
        component_auction_modified_bid_params->bid_currency = absl::nullopt;
        std::string bid_currency_str;
        if (score_ad_object
                ->Get(context,
                      v8_helper_->CreateStringFromLiteral("bidCurrency"))
                .ToLocal(&bid_currency_value) &&
            !bid_currency_value->IsUndefined()) {
          if (!gin::ConvertFromV8(isolate, bid_currency_value,
                                  &bid_currency_str) ||
              !blink::IsValidAdCurrencyCode(bid_currency_str)) {
            errors_out.push_back(
                base::StrCat({decision_logic_url_.spec(),
                              " scoreAd() returned an invalid bidCurrency."}));
            drop_for_invalid_currency = true;
          }
          if (!drop_for_invalid_currency) {
            component_auction_modified_bid_params->bid_currency =
                blink::AdCurrency::From(bid_currency_str);
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

  // Fail if `allow_component_auction` is false and this is a component seller
  // or a top-level seller scoring a bid from a component auction -
  // `browser_signals_other_seller` is non-null in only those two cases.
  if (browser_signals_other_seller && !allow_component_auction) {
    errors_out.push_back(base::StrCat(
        {decision_logic_url_.spec(),
         " scoreAd() return value does not have allowComponentAuction set to "
         "true. Ad dropped from component auction."}));
    PostScoreAdCallbackToUserThreadOnError(
        std::move(callback), std::move(errors_out),
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests());
    return;
  }

  // Fail if the score is invalid.
  if (std::isnan(score) || !std::isfinite(score)) {
    errors_out.push_back(base::StrCat(
        {decision_logic_url_.spec(), " scoreAd() returned an invalid score."}));
    PostScoreAdCallbackToUserThreadOnError(
        std::move(callback), std::move(errors_out),
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests());
    return;
  }

  if (score <= 0) {
    // Keep debug report URLs because we want to send debug loss reports if
    // seller rejected all bids.
    PostScoreAdCallbackToUserThread(
        std::move(callback), /*score=*/0, reject_reason,
        /*component_auction_modified_bid_params=*/nullptr,
        /*bid_in_seller_currency=*/absl::nullopt, scoring_signals_data_version,
        context_recycler.for_debugging_only_bindings()->TakeLossReportUrl(),
        context_recycler.for_debugging_only_bindings()->TakeWinReportUrl(),
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        /*scoring_latency=*/elapsed,
        /*trusted_signals_fetch_latency=*/trusted_signals_fetch_latency,
        std::move(errors_out));
    return;
  }

  // This bid got accepted by scoreAd(), so clear any reject reason it may have
  // set.
  reject_reason = mojom::RejectReason::kNotAvailable;

  // If this is a component auction that modified the bid, validate the bid. Do
  // this after checking the score to avoid validating modified bid values from
  // reporting errors when desirability is <= 0.
  if (component_auction_modified_bid_params &&
      component_auction_modified_bid_params->has_bid) {
    // Fail if the new bid is not valid or is 0 or less.
    if (!IsValidBid(component_auction_modified_bid_params->bid)) {
      errors_out.push_back(base::StrCat(
          {decision_logic_url_.spec(), " scoreAd() returned an invalid bid."}));
      PostScoreAdCallbackToUserThreadOnError(
          std::move(callback), std::move(errors_out),
          context_recycler.private_aggregation_bindings()
              ->TakePrivateAggregationRequests());
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

  PostScoreAdCallbackToUserThread(
      std::move(callback), score, reject_reason,
      std::move(component_auction_modified_bid_params), bid_in_seller_currency,
      scoring_signals_data_version,
      context_recycler.for_debugging_only_bindings()->TakeLossReportUrl(),
      context_recycler.for_debugging_only_bindings()->TakeWinReportUrl(),
      context_recycler.private_aggregation_bindings()
          ->TakePrivateAggregationRequests(),
      /*scoring_latency=*/elapsed,
      /*trusted_signals_fetch_latency=*/trusted_signals_fetch_latency,
      std::move(errors_out));
}

void SellerWorklet::V8State::ReportResult(
    const blink::AuctionConfig::NonSharedParams&
        auction_ad_config_non_shared_params,
    DirectFromSellerSignalsRequester::Result
        direct_from_seller_result_seller_signals,
    DirectFromSellerSignalsRequester::Result
        direct_from_seller_result_auction_signals,
    mojom::ComponentAuctionOtherSellerPtr browser_signals_other_seller,
    const url::Origin& browser_signal_interest_group_owner,
    const absl::optional<std::string>&
        browser_signal_buyer_and_seller_reporting_id,
    const GURL& browser_signal_render_url,
    double browser_signal_bid,
    const absl::optional<blink::AdCurrency>& browser_signal_bid_currency,
    double browser_signal_desirability,
    double browser_signal_highest_scoring_other_bid,
    const absl::optional<blink::AdCurrency>&
        browser_signal_highest_scoring_other_bid_currency,
    auction_worklet::mojom::ComponentAuctionReportResultParamsPtr
        browser_signals_component_auction_report_result_params,
    absl::optional<uint32_t> scoring_signals_data_version,
    uint64_t trace_id,
    ReportResultCallbackInternal callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "post_v8_task", trace_id);
  base::ElapsedTimer elapsed_timer;

  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper_.get());
  v8::Isolate* isolate = v8_helper_->isolate();

  // Short lived context, to avoid leaking data at global scope between either
  // repeated calls to this worklet, or to calls to any other worklet.
  ContextRecycler context_recycler(v8_helper_.get());

  ContextRecyclerScope context_recycler_scope(context_recycler);
  v8::Local<v8::Context> context = context_recycler_scope.GetContext();

  std::vector<v8::Local<v8::Value>> args;
  if (!AppendAuctionConfig(v8_helper_.get(), context, decision_logic_url_,
                           trusted_scoring_signals_url_, experiment_group_id_,
                           auction_ad_config_non_shared_params, &args)) {
    PostReportResultCallbackToUserThread(std::move(callback),
                                         /*signals_for_winner=*/absl::nullopt,
                                         /*report_url=*/absl::nullopt,
                                         /*ad_beacon_map=*/{},
                                         /*pa_requests=*/{}, base::TimeDelta(),
                                         /*errors=*/std::vector<std::string>());
    return;
  }

  v8::Local<v8::Object> browser_signals = v8::Object::New(isolate);
  gin::Dictionary browser_signals_dict(isolate, browser_signals);
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
      // TODO(crbug.com/1441988): Remove deprecated `renderUrl` alias.
      !SetRenderUrl(isolate, browser_signals,
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
                                         /*signals_for_winner=*/absl::nullopt,
                                         /*report_url=*/absl::nullopt,
                                         /*ad_beacon_map=*/{},
                                         /*pa_requests=*/{}, base::TimeDelta(),
                                         /*errors=*/std::vector<std::string>());
    return;
  }
  if (browser_signals_component_auction_report_result_params) {
    if (!v8_helper_->InsertJsonValue(
            context, "topLevelSellerSignals",
            browser_signals_component_auction_report_result_params
                ->top_level_seller_signals,
            browser_signals) ||
        (browser_signals_component_auction_report_result_params
             ->has_modified_bid &&
         !browser_signals_dict.Set(
             "modifiedBid",
             browser_signals_component_auction_report_result_params
                 ->modified_bid))) {
      PostReportResultCallbackToUserThread(
          std::move(callback),
          /*signals_for_winner=*/absl::nullopt,
          /*report_url=*/absl::nullopt,
          /*ad_beacon_map=*/{},
          /*pa_requests=*/{}, base::TimeDelta(),
          /*errors=*/std::vector<std::string>());
      return;
    }
  }
  args.push_back(browser_signals);

  std::vector<std::string> errors_out;
  v8::Local<v8::Object> direct_from_seller_signals = v8::Object::New(isolate);
  gin::Dictionary direct_from_seller_signals_dict(isolate,
                                                  direct_from_seller_signals);
  v8::Local<v8::Value> seller_signals =
      direct_from_seller_result_seller_signals.GetSignals(*v8_helper_, context,
                                                          errors_out);
  v8::Local<v8::Value> auction_signals =
      direct_from_seller_result_auction_signals.GetSignals(*v8_helper_, context,
                                                           errors_out);
  if (!direct_from_seller_signals_dict.Set("sellerSignals", seller_signals) ||
      !direct_from_seller_signals_dict.Set("auctionSignals", auction_signals)) {
    PostReportResultCallbackToUserThread(std::move(callback),
                                         /*signals_for_winner=*/absl::nullopt,
                                         /*report_url=*/absl::nullopt,
                                         /*ad_beacon_map=*/{},
                                         /*pa_requests=*/{}, base::TimeDelta(),
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
  bool success =
      v8_helper_->RunScript(context, unbound_worklet_script, debug_id_.get(),
                            /*script_timeout=*/absl::nullopt, errors_out);

  if (!success) {
    TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "report_result", trace_id);
    PostReportResultCallbackToUserThread(
        std::move(callback), /*signals_for_winner=*/absl::nullopt,
        /*report_url=*/absl::nullopt, /*ad_beacon_map=*/{},
        /*pa_requests=*/{}, elapsed_timer.Elapsed(), std::move(errors_out));
    return;
  }

  context_recycler.AddReportBindings();
  context_recycler.AddRegisterAdBeaconBindings();
  context_recycler.AddPrivateAggregationBindings(
      permissions_policy_state_->private_aggregation_allowed);

  if (base::FeatureList::IsEnabled(blink::features::kSharedStorageAPI)) {
    context_recycler.AddSharedStorageBindings(
        shared_storage_host_remote_.is_bound()
            ? shared_storage_host_remote_.get()
            : nullptr,
        permissions_policy_state_->shared_storage_allowed);
  }

  success =
      v8_helper_
          ->CallFunction(context, debug_id_.get(),
                         v8_helper_->FormatScriptName(unbound_worklet_script),
                         "reportResult", args,
                         /*script_timeout=*/absl::nullopt, errors_out)
          .ToLocal(&signals_for_winner_value);

  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "report_result", trace_id);

  if (!success) {
    // Keep Private Aggregation API requests since `reportReport()` might use
    // it to detect script timeout or failures.
    PostReportResultCallbackToUserThread(
        std::move(callback), /*signals_for_winner=*/absl::nullopt,
        /*report_url=*/absl::nullopt, /*ad_beacon_map=*/{},
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        elapsed_timer.Elapsed(), std::move(errors_out));
    return;
  }

  // Consider lack of error but no return value type, or a return value that
  // can't be converted to JSON a valid result.
  std::string signals_for_winner;
  if (!v8_helper_->ExtractJson(context, signals_for_winner_value,
                               &signals_for_winner)) {
    signals_for_winner = "null";
  }

  PostReportResultCallbackToUserThread(
      std::move(callback), std::move(signals_for_winner),
      context_recycler.report_bindings()->report_url(),
      context_recycler.register_ad_beacon_bindings()->TakeAdBeaconMap(),
      context_recycler.private_aggregation_bindings()
          ->TakePrivateAggregationRequests(),
      elapsed_timer.Elapsed(), std::move(errors_out));
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
    std::vector<std::string> errors,
    PrivateAggregationRequests pa_requests) {
  PostScoreAdCallbackToUserThread(
      std::move(callback), /*score=*/0,
      /*reject_reason=*/mojom::RejectReason::kNotAvailable,
      /*component_auction_modified_bid_params=*/nullptr,
      /*bid_in_seller_currency=*/absl::nullopt,
      /*scoring_signals_data_version=*/absl::nullopt,
      /*debug_loss_report_url=*/absl::nullopt,
      /*debug_win_report_url=*/absl::nullopt, std::move(pa_requests),
      /*scoring_latency=*/base::TimeDelta(),
      /*trusted_signals_fetch_latency=*/base::TimeDelta(), std::move(errors));
}

void SellerWorklet::V8State::PostScoreAdCallbackToUserThread(
    ScoreAdCallbackInternal callback,
    double score,
    mojom::RejectReason reject_reason,
    mojom::ComponentAuctionModifiedBidParamsPtr
        component_auction_modified_bid_params,
    absl::optional<double> bid_in_seller_currency,
    absl::optional<uint32_t> scoring_signals_data_version,
    absl::optional<GURL> debug_loss_report_url,
    absl::optional<GURL> debug_win_report_url,
    PrivateAggregationRequests pa_requests,
    base::TimeDelta scoring_latency,
    base::TimeDelta trusted_signals_fetch_latency,
    std::vector<std::string> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  user_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), score, reject_reason,
                     std::move(component_auction_modified_bid_params),
                     bid_in_seller_currency, scoring_signals_data_version,
                     std::move(debug_loss_report_url),
                     std::move(debug_win_report_url), std::move(pa_requests),
                     scoring_latency, trusted_signals_fetch_latency,
                     std::move(errors)));
}

void SellerWorklet::V8State::PostReportResultCallbackToUserThread(
    ReportResultCallbackInternal callback,
    absl::optional<std::string> signals_for_winner,
    absl::optional<GURL> report_url,
    base::flat_map<std::string, GURL> ad_beacon_map,
    PrivateAggregationRequests pa_requests,
    base::TimeDelta reporting_latency,
    std::vector<std::string> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  user_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(signals_for_winner),
                     std::move(report_url), std::move(ad_beacon_map),
                     std::move(pa_requests), reporting_latency,
                     std::move(errors)));
}

void SellerWorklet::ResumeIfPaused() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  if (!paused_)
    return;

  paused_ = false;
  Start();
}

void SellerWorklet::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  DCHECK(!paused_);

  base::UmaHistogramCounts100000(
      "Ads.InterestGroup.Net.RequestUrlSizeBytes.ScoringScriptJS",
      script_source_url_.spec().size());
  worklet_loader_ = std::make_unique<WorkletLoader>(
      url_loader_factory_.get(), script_source_url_, v8_helper_, debug_id_,
      base::BindOnce(&SellerWorklet::OnDownloadComplete,
                     base::Unretained(this)));
}

void SellerWorklet::OnDownloadComplete(WorkletLoader::Result worklet_script,
                                       absl::optional<std::string> error_msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  base::UmaHistogramCounts10M(
      "Ads.InterestGroup.Net.ResponseSizeBytes.ScoringScriptJS",
      worklet_script.original_size_bytes());
  base::UmaHistogramTimes("Ads.InterestGroup.Net.DownloadTime.ScoringScriptJS",
                          worklet_script.download_time());
  worklet_loader_.reset();

  // On failure, delete `this`, as it can't do anything without a loaded script.
  bool success = worklet_script.success();
  if (!success) {
    std::move(close_pipe_callback_)
        .Run(error_msg ? error_msg.value() : std::string());
    // `this` should be deleted at this point.
    return;
  }

  // The error message, if any, will be appended to all invoked ScoreAd() and
  // ReportResult() callbacks.
  load_script_error_msg_ = std::move(error_msg);

  v8_runner_->PostTask(FROM_HERE,
                       base::BindOnce(&SellerWorklet::V8State::SetWorkletScript,
                                      base::Unretained(v8_state_.get()),
                                      std::move(worklet_script)));
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

void SellerWorklet::OnTrustedScoringSignalsDownloaded(
    ScoreAdTaskList::iterator task,
    scoped_refptr<TrustedSignals::Result> result,
    absl::optional<std::string> error_msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  task->trusted_scoring_signals_error_msg = std::move(error_msg);
  task->trusted_scoring_signals_result = std::move(result);
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
  return !task.trusted_scoring_signals_request &&
         !task.direct_from_seller_request_seller_signals &&
         !task.direct_from_seller_request_auction_signals && IsCodeReady();
}

void SellerWorklet::ScoreAdIfReady(ScoreAdTaskList::iterator task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  if (!IsReadyToScoreAd(*task))
    return;

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
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "post_v8_task", task->trace_id);

  // Normally the PostTask below will eventually get `task` cleaned up once it
  // posts back to DeliverScoreAdCallbackOnUserThread with its results, but that
  // won't happen if it gets cancelled. To deal with that, a ScopedClosureRunner
  // is passed to ask for `task` to get cleaned up in case the V8State::ScoreAd
  // closure gets destroyed without running.
  base::OnceClosure cleanup_score_ad_task = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&SellerWorklet::CleanUpScoreAdTaskOnUserThread,
                     weak_ptr_factory_.GetWeakPtr(), task));

  task->task_id = cancelable_task_tracker_.PostTask(
      v8_runner_.get(), FROM_HERE,
      base::BindOnce(
          &SellerWorklet::V8State::ScoreAd, base::Unretained(v8_state_.get()),
          task->ad_metadata_json, task->bid, std::move(task->bid_currency),
          std::move(task->auction_ad_config_non_shared_params),
          std::move(task->direct_from_seller_result_seller_signals),
          std::move(task->direct_from_seller_result_auction_signals),
          std::move(task->trusted_scoring_signals_result),
          std::move(task->browser_signals_other_seller),
          std::move(task->component_expect_bid_currency),
          std::move(task->browser_signal_interest_group_owner),
          std::move(task->browser_signal_render_url),
          std::move(task->browser_signal_ad_components),
          task->browser_signal_bidding_duration_msecs,
          std::move(task->seller_timeout), task->trace_id,
          task->wait_trusted_signals,
          base::ScopedClosureRunner(std::move(cleanup_score_ad_task)),
          base::BindOnce(&SellerWorklet::DeliverScoreAdCallbackOnUserThread,
                         weak_ptr_factory_.GetWeakPtr(), task)));
}

void SellerWorklet::DeliverScoreAdCallbackOnUserThread(
    ScoreAdTaskList::iterator task,
    double score,
    mojom::RejectReason reject_reason,
    mojom::ComponentAuctionModifiedBidParamsPtr
        component_auction_modified_bid_params,
    absl::optional<double> bid_in_seller_currency,
    absl::optional<uint32_t> scoring_signals_data_version,
    absl::optional<GURL> debug_loss_report_url,
    absl::optional<GURL> debug_win_report_url,
    PrivateAggregationRequests pa_requests,
    base::TimeDelta scoring_latency,
    base::TimeDelta trusted_signals_fetch_latency,
    std::vector<std::string> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  if (load_script_error_msg_)
    errors.insert(errors.begin(), load_script_error_msg_.value());
  if (task->trusted_scoring_signals_error_msg)
    errors.insert(errors.begin(), *task->trusted_scoring_signals_error_msg);

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
      scoring_latency, trusted_signals_fetch_latency, std::move(errors));
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
  if (!IsReadyToReportResult(*task))
    return;

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

  cancelable_task_tracker_.PostTask(
      v8_runner_.get(), FROM_HERE,
      base::BindOnce(
          &SellerWorklet::V8State::ReportResult,
          base::Unretained(v8_state_.get()),
          std::move(task->auction_ad_config_non_shared_params),
          std::move(task->direct_from_seller_result_seller_signals),
          std::move(task->direct_from_seller_result_auction_signals),
          std::move(task->browser_signals_other_seller),
          std::move(task->browser_signal_interest_group_owner),
          std::move(task->browser_signal_buyer_and_seller_reporting_id),
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
    const absl::optional<std::string> signals_for_winner,
    const absl::optional<GURL> report_url,
    base::flat_map<std::string, GURL> ad_beacon_map,
    PrivateAggregationRequests pa_requests,
    base::TimeDelta reporting_latency,
    std::vector<std::string> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  if (load_script_error_msg_)
    errors.insert(errors.begin(), load_script_error_msg_.value());

  std::move(task->callback)
      .Run(signals_for_winner, report_url, ad_beacon_map,
           std::move(pa_requests), reporting_latency, std::move(errors));
  report_result_tasks_.erase(task);
}

bool SellerWorklet::IsCodeReady() const {
  return (!paused_ && !worklet_loader_);
}

}  // namespace auction_worklet
