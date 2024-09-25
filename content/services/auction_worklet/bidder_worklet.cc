// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/bidder_worklet.h"

#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/hash/hash.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "base/types/optional_util.h"
#include "content/public/common/content_features.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/auction_v8_logger.h"
#include "content/services/auction_worklet/auction_worklet_util.h"
#include "content/services/auction_worklet/bidder_lazy_filler.h"
#include "content/services/auction_worklet/deprecated_url_lazy_filler.h"
#include "content/services/auction_worklet/direct_from_seller_signals_requester.h"
#include "content/services/auction_worklet/for_debugging_only_bindings.h"
#include "content/services/auction_worklet/private_aggregation_bindings.h"
#include "content/services/auction_worklet/public/cpp/auction_network_events_delegate.h"
#include "content/services/auction_worklet/public/cpp/private_aggregation_reporting.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom-shared.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "content/services/auction_worklet/public/mojom/real_time_reporting.mojom.h"
#include "content/services/auction_worklet/real_time_reporting_bindings.h"
#include "content/services/auction_worklet/register_ad_beacon_bindings.h"
#include "content/services/auction_worklet/register_ad_macro_bindings.h"
#include "content/services/auction_worklet/report_bindings.h"
#include "content/services/auction_worklet/set_bid_bindings.h"
#include "content/services/auction_worklet/set_priority_bindings.h"
#include "content/services/auction_worklet/set_priority_signals_override_bindings.h"
#include "content/services/auction_worklet/shared_storage_bindings.h"
#include "content/services/auction_worklet/trusted_signals.h"
#include "content/services/auction_worklet/trusted_signals_request_manager.h"
#include "content/services/auction_worklet/worklet_loader.h"
#include "content/services/auction_worklet/worklet_util.h"
#include "context_recycler.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/common/interest_group/ad_auction_currencies.h"
#include "third_party/blink/public/common/interest_group/ad_display_size.h"
#include "third_party/blink/public/common/interest_group/ad_display_size_utils.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "v8-statistics.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-template.h"
#include "v8/include/v8-wasm.h"

namespace auction_worklet {

namespace {

class DeepFreezeAllowAll : public v8::Context::DeepFreezeDelegate {
 public:
  bool FreezeEmbedderObjectAndGetChildren(
      v8::Local<v8::Object> obj,
      v8::LocalVector<v8::Object>& children_out) override {
    return true;
  }
};

bool AppendJsonValueOrNull(AuctionV8Helper* const v8_helper,
                           v8::Local<v8::Context> context,
                           const std::string* maybe_json,
                           v8::LocalVector<v8::Value>* args) {
  v8::Isolate* isolate = v8_helper->isolate();
  if (maybe_json) {
    if (!v8_helper->AppendJsonValue(context, *maybe_json, args)) {
      return false;
    }
  } else {
    args->push_back(v8::Null(isolate));
  }
  return true;
}

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

std::optional<base::TimeDelta> NullOptIfZero(base::TimeDelta delta) {
  if (delta.is_zero()) {
    return std::nullopt;
  }
  return delta;
}

// Adjust `bid` to meet `target_num_ad_components`, if any.
void TrimExtraAdComponents(mojom::BidderWorkletBid& bid,
                           std::optional<size_t> target_num_ad_components) {
  if (!target_num_ad_components.has_value()) {
    return;
  }
  // SetBidBindings should have enforced that there are enough adComponents,
  // as should have HandleComponentsKAnon() when bifurcating bids,
  // which also implies they exist.
  DCHECK(bid.ad_component_descriptors.has_value());
  DCHECK_LE(*target_num_ad_components, bid.ad_component_descriptors->size());
  bid.ad_component_descriptors->resize(*target_num_ad_components);
}

// Applies `target_num_ad_components` to `bid` and appends it to `out`.
void TrimAndCollectBid(mojom::BidderWorkletBidPtr bid,
                       std::optional<size_t> target_num_ad_components,
                       std::vector<mojom::BidderWorkletBidPtr>& out) {
  TrimExtraAdComponents(*bid, target_num_ad_components);
  out.push_back(std::move(bid));
}

// Given `bid` that has k-anonymous main ad and some component ads, handles the
// k-anon check on its components, possibly dropping some if allowed. Appends
// the resulting bid (and perhaps a non-k-anon alternative) to `out`.
void HandleComponentsKAnon(
    const mojom::BidderWorkletNonSharedParams* bidder_worklet_non_shared_params,
    mojom::BidderWorkletBidPtr bid,
    std::optional<size_t> target_num_ad_components,
    size_t num_mandatory_ad_components,
    std::vector<mojom::BidderWorkletBidPtr>& out) {
  size_t num_required_components =
      target_num_ad_components.value_or(bid->ad_component_descriptors->size());
  // Go through the ad component list and try to collect
  // `num_required_components` k-anonymous ones into
  // `usable_ad_component_indices`. Gives up if a mandatory ad component isn't
  // k-anonymous.  Sets `saw_non_k_anon` to true if it needed to skip over any
  // non-k-anonymous ones.
  std::vector<size_t> usable_ad_component_indices;
  usable_ad_component_indices.reserve(num_required_components);
  bool saw_non_k_anon = false;
  for (size_t i = 0; i < bid->ad_component_descriptors->size(); ++i) {
    const blink::AdDescriptor& ad_component_descriptor =
        bid->ad_component_descriptors.value()[i];
    if (BidderWorklet::IsComponentAdKAnon(bidder_worklet_non_shared_params,
                                          ad_component_descriptor)) {
      usable_ad_component_indices.push_back(i);
      if (usable_ad_component_indices.size() == num_required_components) {
        break;
      }
    } else {
      saw_non_k_anon = true;
      if (i < num_mandatory_ad_components) {
        // One of the required component ads is not k-anon, so have to give up
        // on this.
        break;
      }
    }
  }

  DCHECK_LE(usable_ad_component_indices.size(), num_required_components);
  if (usable_ad_component_indices.size() == num_required_components) {
    if (!saw_non_k_anon) {
      // The bid was actually completely fine without getting fancy.
      bid->bid_role = mojom::BidRole::kBothKAnonModes;
      TrimAndCollectBid(std::move(bid), target_num_ad_components, out);
    } else {
      // Split the bid into two, with one having just the usable ad components.
      mojom::BidderWorkletBidPtr non_kanon_alternative = bid->Clone();
      DCHECK_EQ(non_kanon_alternative->bid_role,
                mojom::BidRole::kUnenforcedKAnon);

      bid->bid_role = mojom::BidRole::kEnforcedKAnon;
      std::vector<blink::AdDescriptor> usable_ad_components;
      usable_ad_components.reserve(num_required_components);
      for (size_t index : usable_ad_component_indices) {
        usable_ad_components.push_back(
            std::move(bid->ad_component_descriptors.value()[index]));
      }
      bid->ad_component_descriptors = std::move(usable_ad_components);

      TrimAndCollectBid(std::move(bid), target_num_ad_components, out);
      TrimAndCollectBid(std::move(non_kanon_alternative),
                        target_num_ad_components, out);
    }
  } else {
    // Could not salvage the bid; just drop any extra component ads and mark it
    // as non-k-anon.
    DCHECK_EQ(bid->bid_role, mojom::BidRole::kUnenforcedKAnon);
    TrimAndCollectBid(std::move(bid), target_num_ad_components, out);
  }
}

std::vector<mojom::BidderWorkletBidPtr> ClassifyBidsAndApplyComponentAdLimits(
    mojom::KAnonymityBidMode kanon_mode,
    const mojom::BidderWorkletNonSharedParams* bidder_worklet_non_shared_params,
    const GURL& script_source_url,
    std::vector<SetBidBindings::BidAndWorkletOnlyMetadata> bid_info) {
  std::vector<mojom::BidderWorkletBidPtr> bids;
  for (auto& candidate : bid_info) {
    if (kanon_mode == mojom::KAnonymityBidMode::kNone ||
        !BidderWorklet::IsMainAdKAnon(bidder_worklet_non_shared_params,
                                      script_source_url, candidate)) {
      DCHECK_EQ(candidate.bid->bid_role, mojom::BidRole::kUnenforcedKAnon);
      TrimAndCollectBid(std::move(candidate.bid),
                        candidate.target_num_ad_components, bids);
    } else {
      // We care about k-anonymity, and whether the bid is k-anonymous or not
      // depends on whether component ads are k-anonymous; we also may be able
      // to salvage the bid by throwing out some components while trying to get
      // it to the target.
      if (!candidate.bid->ad_component_descriptors.has_value() ||
          candidate.bid->ad_component_descriptors->empty()) {
        // There are no components to worry about, so it's k-anon.
        candidate.bid->bid_role = mojom::BidRole::kBothKAnonModes;
        bids.push_back(std::move(candidate.bid));
      } else {
        HandleComponentsKAnon(bidder_worklet_non_shared_params,
                              std::move(candidate.bid),
                              candidate.target_num_ad_components,
                              candidate.num_mandatory_ad_components, bids);
      }
    }
  }
  return bids;
}

size_t GetNumberOfGroupByOriginContextsToKeep() {
  if (base::FeatureList::IsEnabled(
          blink::features::
              kFledgeNumberBidderWorkletGroupByOriginContextsToKeep)) {
    // Avoid using multiple contexts for the testing population
    // unless otherwise specified by
    // kFledgeNumberBidderWorkletContextsIncludeFacilitedTesting.
    if (blink::features::
            kFledgeNumberBidderWorkletContextsIncludeFacilitedTesting.Get() ||
        !base::FeatureList::IsEnabled(
            features::kCookieDeprecationFacilitatedTesting)) {
      return blink::features::
          kFledgeNumberBidderWorkletGroupByOriginContextsToKeepValue.Get();
    }
  }
  return 1;
}

// Check if trusted bidding signals, if any, are same-origin or cross-origin.
BidderWorklet::SignalsOriginRelation ClassifyTrustedSignals(
    const GURL& script_source_url,
    const std::optional<url::Origin>& trusted_bidding_signals_origin) {
  if (!trusted_bidding_signals_origin) {
    return BidderWorklet::SignalsOriginRelation::kNoTrustedSignals;
  }
  if (trusted_bidding_signals_origin->IsSameOriginWith(script_source_url)) {
    return BidderWorklet::SignalsOriginRelation::kSameOriginSignals;
  }

  return BidderWorklet::SignalsOriginRelation::kCrossOriginSignals;
}

// Sets the appropriate field (if any) of `browser_signals_dict` to data
// version. Returns success/failure.
bool SetDataVersion(
    BidderWorklet::SignalsOriginRelation trusted_signals_relation,
    std::optional<uint32_t> bidding_signals_data_version,
    gin::Dictionary& browser_signals_dict) {
  if (!bidding_signals_data_version.has_value()) {
    return true;
  }

  switch (trusted_signals_relation) {
    case BidderWorklet::SignalsOriginRelation::kNoTrustedSignals:
      return true;

    case BidderWorklet::SignalsOriginRelation::kSameOriginSignals:
      return browser_signals_dict.Set("dataVersion",
                                      bidding_signals_data_version.value());

    case BidderWorklet::SignalsOriginRelation::kCrossOriginSignals:
      return browser_signals_dict.Set("crossOriginDataVersion",
                                      bidding_signals_data_version.value());
  }
}

}  // namespace

BidderWorklet::BidderWorklet(
    std::vector<scoped_refptr<AuctionV8Helper>> v8_helpers,
    std::vector<mojo::PendingRemote<mojom::AuctionSharedStorageHost>>
        shared_storage_hosts,
    bool pause_for_debugger_on_start,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_url_loader_factory,
    mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
        auction_network_events_handler,
    const GURL& script_source_url,
    const std::optional<GURL>& wasm_helper_url,
    const std::optional<GURL>& trusted_bidding_signals_url,
    const std::string& trusted_bidding_signals_slot_size_param,
    const url::Origin& top_window_origin,
    mojom::AuctionWorkletPermissionsPolicyStatePtr permissions_policy_state,
    std::optional<uint16_t> experiment_group_id,
    mojom::TrustedSignalsPublicKeyPtr public_key)
    : join_origin_hash_salt_(base::NumberToString(base::RandUint64())),
      url_loader_factory_(std::move(pending_url_loader_factory)),
      script_source_url_(script_source_url),
      wasm_helper_url_(wasm_helper_url),
      top_window_origin_(top_window_origin),
      auction_network_events_handler_(
          std::move(auction_network_events_handler)) {
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
                    std::move(shared_storage_hosts[i]), script_source_url_,
                    top_window_origin_, permissions_policy_state->Clone(),
                    wasm_helper_url_, trusted_bidding_signals_url,
                    weak_ptr_factory_.GetWeakPtr()),
        base::OnTaskRunnerDeleter(v8_runners_[i])));
  }

  trusted_signals_request_manager_ =
      (trusted_bidding_signals_url
           ? std::make_unique<TrustedSignalsRequestManager>(
                 TrustedSignalsRequestManager::Type::kBiddingSignals,
                 url_loader_factory_.get(),
                 /*auction_network_events_handler=*/
                 CreateNewAuctionNetworkEventsHandlerRemote(
                     auction_network_events_handler_),
                 /*automatically_send_requests=*/false, top_window_origin,
                 *trusted_bidding_signals_url, experiment_group_id,
                 trusted_bidding_signals_slot_size_param, std::move(public_key),
                 v8_helpers_[GetNextThreadIndex()].get())
           : nullptr);

  paused_ = pause_for_debugger_on_start;
  if (!paused_) {
    Start();
  }
}

BidderWorklet::~BidderWorklet() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  for (const auto& debug_id : debug_ids_) {
    debug_id->AbortDebuggerPauses();
  }
}

std::vector<int> BidderWorklet::context_group_ids_for_testing() const {
  std::vector<int> results;
  for (const auto& debug_id : debug_ids_) {
    results.push_back(debug_id->context_group_id());
  }
  return results;
}

size_t BidderWorklet::GetNextThreadIndex() {
  size_t result = next_thread_index_;
  next_thread_index_++;
  next_thread_index_ %= v8_helpers_.size();
  return result;
}

// static
bool BidderWorklet::IsKAnon(
    const mojom::BidderWorkletNonSharedParams* bidder_worklet_non_shared_params,
    const std::string& key) {
  return std::find(bidder_worklet_non_shared_params->kanon_keys.begin(),
                   bidder_worklet_non_shared_params->kanon_keys.end(),
                   mojom::KAnonKey::New(key)) !=
         bidder_worklet_non_shared_params->kanon_keys.end();
}

// static
bool BidderWorklet::IsMainAdKAnon(
    const mojom::BidderWorkletNonSharedParams* bidder_worklet_non_shared_params,
    const GURL& script_source_url,
    const SetBidBindings::BidAndWorkletOnlyMetadata& bid_and_metadata) {
  const mojom::BidderWorkletBidPtr& bid = bid_and_metadata.bid;
  if (!bid) {
    return true;
  }

  url::Origin owner = url::Origin::Create(script_source_url);
  if (!BidderWorklet::IsKAnon(
          bidder_worklet_non_shared_params,
          blink::HashedKAnonKeyForAdBid(owner, script_source_url,
                                        bid->ad_descriptor.url.spec()))) {
    return false;
  }
  if (bid->selected_buyer_and_seller_reporting_id.has_value()) {
    if (!BidderWorklet::IsKAnon(
            bidder_worklet_non_shared_params,
            blink::HashedKAnonKeyForAdNameReportingWithoutInterestGroup(
                /*interest_group_owner=*/owner,
                /*interest_group_name=*/bidder_worklet_non_shared_params->name,
                /*interest_group_bidding_url=*/script_source_url,
                /*ad_render_url=*/bid->ad_descriptor.url.spec(),
                bid_and_metadata.buyer_reporting_id,
                bid_and_metadata.buyer_and_seller_reporting_id,
                bid_and_metadata.bid
                    ->selected_buyer_and_seller_reporting_id))) {
      return false;
    }
  }
  return true;
}

// static
bool BidderWorklet::IsComponentAdKAnon(
    const mojom::BidderWorkletNonSharedParams* bidder_worklet_non_shared_params,
    const blink::AdDescriptor& ad_component_descriptor) {
  return IsKAnon(
      bidder_worklet_non_shared_params,
      blink::HashedKAnonKeyForAdComponentBid(ad_component_descriptor));
}

// static
bool BidderWorklet::SupportMultiBid() {
  // Multi-bid is auto-disabled in mode-A/B trials.
  return base::FeatureList::IsEnabled(blink::features::kFledgeMultiBid) &&
         !base::FeatureList::IsEnabled(
             features::kCookieDeprecationFacilitatedTesting);
}

void BidderWorklet::BeginGenerateBid(
    mojom::BidderWorkletNonSharedParamsPtr bidder_worklet_non_shared_params,
    mojom::KAnonymityBidMode kanon_mode,
    const url::Origin& interest_group_join_origin,
    const std::optional<GURL>& direct_from_seller_per_buyer_signals,
    const std::optional<GURL>& direct_from_seller_auction_signals,
    const url::Origin& browser_signal_seller_origin,
    const std::optional<url::Origin>& browser_signal_top_level_seller_origin,
    const base::TimeDelta browser_signal_recency,
    blink::mojom::BiddingBrowserSignalsPtr bidding_browser_signals,
    base::Time auction_start_time,
    const std::optional<blink::AdSize>& requested_ad_size,
    uint16_t multi_bid_limit,
    uint64_t trace_id,
    mojo::PendingAssociatedRemote<mojom::GenerateBidClient> generate_bid_client,
    mojo::PendingAssociatedReceiver<mojom::GenerateBidFinalizer>
        bid_finalizer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  generate_bid_tasks_.emplace_front();
  auto generate_bid_task = generate_bid_tasks_.begin();
  generate_bid_task->bidder_worklet_non_shared_params =
      std::move(bidder_worklet_non_shared_params);
  generate_bid_task->kanon_mode = kanon_mode;
  generate_bid_task->interest_group_join_origin = interest_group_join_origin;
  generate_bid_task->browser_signal_seller_origin =
      browser_signal_seller_origin;
  generate_bid_task->browser_signal_top_level_seller_origin =
      browser_signal_top_level_seller_origin;
  generate_bid_task->browser_signal_recency = browser_signal_recency;
  generate_bid_task->bidding_browser_signals =
      std::move(bidding_browser_signals);
  generate_bid_task->auction_start_time = auction_start_time;
  generate_bid_task->requested_ad_size = requested_ad_size;
  generate_bid_task->multi_bid_limit = multi_bid_limit;
  generate_bid_task->trace_id = trace_id;
  generate_bid_task->generate_bid_client.Bind(std::move(generate_bid_client));
  // Deleting `generate_bid_task` will destroy `generate_bid_client` and thus
  // abort this callback, so it's safe to use Unretained(this) and
  // `generate_bid_task` here.
  generate_bid_task->generate_bid_client.set_disconnect_handler(
      base::BindOnce(&BidderWorklet::OnGenerateBidClientDestroyed,
                     base::Unretained(this), generate_bid_task));

  // Listen to call to FinalizeGenerateBid completing arguments.
  generate_bid_task->finalize_generate_bid_receiver_id =
      finalize_receiver_set_.Add(this, std::move(bid_finalizer),
                                 generate_bid_task);

  HandleDirectFromSellerForGenerateBid(direct_from_seller_per_buyer_signals,
                                       direct_from_seller_auction_signals,
                                       generate_bid_task);

  const auto& trusted_bidding_signals_keys =
      generate_bid_task->bidder_worklet_non_shared_params
          ->trusted_bidding_signals_keys;
  generate_bid_task->trace_wait_deps_start = base::TimeTicks::Now();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "wait_generate_bid_deps",
                                    trace_id);
  if (trusted_signals_request_manager_) {
    if (trusted_signals_request_manager_->HasPublicKey()) {
      DCHECK(base::FeatureList::IsEnabled(
          blink::features::kFledgeTrustedSignalsKVv2Support));
      generate_bid_task->trusted_bidding_signals_request =
          trusted_signals_request_manager_->RequestKVv2BiddingSignals(
              generate_bid_task->bidder_worklet_non_shared_params->name,
              trusted_bidding_signals_keys,
              generate_bid_task->interest_group_join_origin,
              generate_bid_task->bidder_worklet_non_shared_params
                  ->execution_mode,
              base::BindOnce(&BidderWorklet::OnTrustedBiddingSignalsDownloaded,
                             base::Unretained(this), generate_bid_task));
    } else {
      generate_bid_task->trusted_bidding_signals_request =
          trusted_signals_request_manager_->RequestBiddingSignals(
              generate_bid_task->bidder_worklet_non_shared_params->name,
              trusted_bidding_signals_keys,
              generate_bid_task->bidder_worklet_non_shared_params
                  ->max_trusted_bidding_signals_url_length,
              base::BindOnce(&BidderWorklet::OnTrustedBiddingSignalsDownloaded,
                             base::Unretained(this), generate_bid_task));
    }
    return;
  }

  // Deleting `generate_bid_task` will destroy `generate_bid_client` and thus
  // abort this callback, so it's safe to use Unretained(this) and
  // `generate_bid_task` here.
  generate_bid_task->generate_bid_client->OnBiddingSignalsReceived(
      /*priority_vector=*/{},
      /*trusted_signals_fetch_latency=*/base::TimeDelta(),
      /*update_if_older_than=*/std::nullopt,
      base::BindOnce(&BidderWorklet::SignalsReceivedCallback,
                     base::Unretained(this), generate_bid_task));
}

void BidderWorklet::SendPendingSignalsRequests() {
  if (trusted_signals_request_manager_) {
    trusted_signals_request_manager_->StartBatchedTrustedSignalsRequest();
  }
}

void BidderWorklet::ReportWin(
    bool is_for_additional_bid,
    const std::optional<std::string>& interest_group_name_reporting_id,
    const std::optional<std::string>& buyer_reporting_id,
    const std::optional<std::string>& buyer_and_seller_reporting_id,
    const std::optional<std::string>& selected_buyer_and_seller_reporting_id,
    const std::optional<std::string>& auction_signals_json,
    const std::optional<std::string>& per_buyer_signals_json,
    const std::optional<GURL>& direct_from_seller_per_buyer_signals,
    const std::optional<std::string>&
        direct_from_seller_per_buyer_signals_header_ad_slot,
    const std::optional<GURL>& direct_from_seller_auction_signals,
    const std::optional<std::string>&
        direct_from_seller_auction_signals_header_ad_slot,
    const std::string& seller_signals_json,
    mojom::KAnonymityBidMode kanon_mode,
    bool bid_is_kanon,
    const GURL& browser_signal_render_url,
    double browser_signal_bid,
    const std::optional<blink::AdCurrency>& browser_signal_bid_currency,
    double browser_signal_highest_scoring_other_bid,
    const std::optional<blink::AdCurrency>&
        browser_signal_highest_scoring_other_bid_currency,
    bool browser_signal_made_highest_scoring_other_bid,
    std::optional<double> browser_signal_ad_cost,
    std::optional<uint16_t> browser_signal_modeling_signals,
    uint8_t browser_signal_join_count,
    uint8_t browser_signal_recency,
    const url::Origin& browser_signal_seller_origin,
    const std::optional<url::Origin>& browser_signal_top_level_seller_origin,
    const std::optional<base::TimeDelta> browser_signal_reporting_timeout,
    std::optional<uint32_t> bidding_signals_data_version,
    uint64_t trace_id,
    ReportWinCallback report_win_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  CHECK((!direct_from_seller_per_buyer_signals &&
         !direct_from_seller_auction_signals) ||
        (!direct_from_seller_per_buyer_signals_header_ad_slot &&
         !direct_from_seller_auction_signals_header_ad_slot));

  report_win_tasks_.emplace_front();
  auto report_win_task = report_win_tasks_.begin();
  report_win_task->is_for_additional_bid = is_for_additional_bid;
  report_win_task->interest_group_name_reporting_id =
      interest_group_name_reporting_id;
  report_win_task->buyer_reporting_id = buyer_reporting_id;
  report_win_task->buyer_and_seller_reporting_id =
      buyer_and_seller_reporting_id;
  report_win_task->selected_buyer_and_seller_reporting_id =
      selected_buyer_and_seller_reporting_id;
  report_win_task->auction_signals_json = auction_signals_json;
  report_win_task->per_buyer_signals_json = per_buyer_signals_json;
  report_win_task->seller_signals_json = seller_signals_json;
  report_win_task->kanon_mode = kanon_mode;
  report_win_task->bid_is_kanon = bid_is_kanon;
  report_win_task->browser_signal_render_url = browser_signal_render_url;
  report_win_task->browser_signal_bid = browser_signal_bid;
  report_win_task->browser_signal_bid_currency = browser_signal_bid_currency;
  report_win_task->browser_signal_highest_scoring_other_bid =
      browser_signal_highest_scoring_other_bid;
  report_win_task->browser_signal_highest_scoring_other_bid_currency =
      browser_signal_highest_scoring_other_bid_currency;
  report_win_task->browser_signal_made_highest_scoring_other_bid =
      browser_signal_made_highest_scoring_other_bid;
  report_win_task->browser_signal_ad_cost = browser_signal_ad_cost;
  report_win_task->browser_signal_modeling_signals =
      browser_signal_modeling_signals;
  report_win_task->browser_signal_join_count = browser_signal_join_count;
  report_win_task->browser_signal_recency = browser_signal_recency;
  report_win_task->browser_signal_seller_origin = browser_signal_seller_origin;
  report_win_task->browser_signal_top_level_seller_origin =
      browser_signal_top_level_seller_origin;
  report_win_task->browser_signal_reporting_timeout =
      browser_signal_reporting_timeout;
  report_win_task->bidding_signals_data_version = bidding_signals_data_version;
  report_win_task->trace_id = trace_id;
  report_win_task->callback = std::move(report_win_callback);

  if (direct_from_seller_per_buyer_signals) {
    // Deleting `report_win_task` will destroy
    // `direct_from_seller_request_seller_signals` and thus abort this
    // callback, so it's safe to use Unretained(this) and `report_win_task`
    // here.
    report_win_task->direct_from_seller_request_per_buyer_signals =
        direct_from_seller_requester_per_buyer_signals_.LoadSignals(
            *url_loader_factory_, *direct_from_seller_per_buyer_signals,
            base::BindOnce(
                &BidderWorklet::
                    OnDirectFromSellerPerBuyerSignalsDownloadedReportWin,
                base::Unretained(this), report_win_task));
  } else {
    report_win_task->direct_from_seller_result_per_buyer_signals =
        DirectFromSellerSignalsRequester::Result();
  }

  if (direct_from_seller_auction_signals) {
    // Deleting `report_win_task` will destroy
    // `direct_from_seller_request_auction_signals` and thus abort this
    // callback, so it's safe to use Unretained(this) and `report_win_task`
    // here.
    report_win_task->direct_from_seller_request_auction_signals =
        direct_from_seller_requester_auction_signals_.LoadSignals(
            *url_loader_factory_, *direct_from_seller_auction_signals,
            base::BindOnce(
                &BidderWorklet::
                    OnDirectFromSellerAuctionSignalsDownloadedReportWin,
                base::Unretained(this), report_win_task));
  } else {
    report_win_task->direct_from_seller_result_auction_signals =
        DirectFromSellerSignalsRequester::Result();
  }
  report_win_task->trace_wait_deps_start = base::TimeTicks::Now();
  report_win_task->direct_from_seller_per_buyer_signals_header_ad_slot =
      direct_from_seller_per_buyer_signals_header_ad_slot;
  report_win_task->direct_from_seller_auction_signals_header_ad_slot =
      direct_from_seller_auction_signals_header_ad_slot;

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "wait_report_win_deps", trace_id);
  RunReportWinIfReady(report_win_task);
}

void BidderWorklet::ConnectDevToolsAgent(
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent,
    uint32_t thread_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  v8_runners_[thread_index]->PostTask(
      FROM_HERE, base::BindOnce(&V8State::ConnectDevToolsAgent,
                                base::Unretained(v8_state_[thread_index].get()),
                                std::move(agent)));
}

void BidderWorklet::FinishGenerateBid(
    const std::optional<std::string>& auction_signals_json,
    const std::optional<std::string>& per_buyer_signals_json,
    const std::optional<base::TimeDelta> per_buyer_timeout,
    const std::optional<blink::AdCurrency>& expected_buyer_currency,
    const std::optional<GURL>& direct_from_seller_per_buyer_signals,
    const std::optional<std::string>&
        direct_from_seller_per_buyer_signals_header_ad_slot,
    const std::optional<GURL>& direct_from_seller_auction_signals,
    const std::optional<std::string>&
        direct_from_seller_auction_signals_header_ad_slot) {
  CHECK((!direct_from_seller_per_buyer_signals &&
         !direct_from_seller_auction_signals) ||
        (!direct_from_seller_per_buyer_signals_header_ad_slot &&
         !direct_from_seller_auction_signals_header_ad_slot));

  GenerateBidTaskList::iterator task = finalize_receiver_set_.current_context();
  task->auction_signals_json = auction_signals_json;
  task->per_buyer_signals_json = per_buyer_signals_json;
  task->per_buyer_timeout = per_buyer_timeout;
  task->expected_buyer_currency = expected_buyer_currency;
  task->finalize_generate_bid_called = true;
  task->direct_from_seller_per_buyer_signals_header_ad_slot =
      direct_from_seller_per_buyer_signals_header_ad_slot;
  task->direct_from_seller_auction_signals_header_ad_slot =
      direct_from_seller_auction_signals_header_ad_slot;
  HandleDirectFromSellerForGenerateBid(direct_from_seller_per_buyer_signals,
                                       direct_from_seller_auction_signals,
                                       task);

  finalize_receiver_set_.Remove(*task->finalize_generate_bid_receiver_id);
  task->finalize_generate_bid_receiver_id = std::nullopt;
  task->wait_promises = base::TimeTicks::Now() - task->trace_wait_deps_start;
  GenerateBidIfReady(task);
}

BidderWorklet::GenerateBidTask::GenerateBidTask() = default;
BidderWorklet::GenerateBidTask::~GenerateBidTask() = default;

BidderWorklet::ReportWinTask::ReportWinTask() = default;
BidderWorklet::ReportWinTask::~ReportWinTask() = default;

BidderWorklet::V8State::V8State(
    scoped_refptr<AuctionV8Helper> v8_helper,
    scoped_refptr<AuctionV8Helper::DebugId> debug_id,
    mojo::PendingRemote<mojom::AuctionSharedStorageHost>
        shared_storage_host_remote,
    const GURL& script_source_url,
    const url::Origin& top_window_origin,
    mojom::AuctionWorkletPermissionsPolicyStatePtr permissions_policy_state,
    const std::optional<GURL>& wasm_helper_url,
    const std::optional<GURL>& trusted_bidding_signals_url,
    base::WeakPtr<BidderWorklet> parent)
    : v8_helper_(std::move(v8_helper)),
      debug_id_(std::move(debug_id)),
      parent_(std::move(parent)),
      user_thread_(base::SequencedTaskRunner::GetCurrentDefault()),
      owner_(url::Origin::Create(script_source_url)),
      script_source_url_(script_source_url),
      top_window_origin_(top_window_origin),
      permissions_policy_state_(std::move(permissions_policy_state)),
      wasm_helper_url_(wasm_helper_url),
      trusted_bidding_signals_url_(trusted_bidding_signals_url),
      trusted_bidding_signals_origin_(
          trusted_bidding_signals_url_ ? std::make_optional(url::Origin::Create(
                                             *trusted_bidding_signals_url_))
                                       : std::nullopt),
      context_recyclers_for_origin_group_mode_(
          GetNumberOfGroupByOriginContextsToKeep()) {
  DETACH_FROM_SEQUENCE(v8_sequence_checker_);
  v8_helper_->v8_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V8State::FinishInit, base::Unretained(this),
                                std::move(shared_storage_host_remote)));
}

void BidderWorklet::V8State::SetWorkletScript(
    WorkletLoader::Result worklet_script) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  worklet_script_ = WorkletLoader::TakeScript(std::move(worklet_script));
}

void BidderWorklet::V8State::SetWasmHelper(
    WorkletWasmLoader::Result wasm_helper) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  wasm_helper_ = std::move(wasm_helper);
}

BidderWorklet::V8State::SingleGenerateBidResult::SingleGenerateBidResult() =
    default;
BidderWorklet::V8State::SingleGenerateBidResult::SingleGenerateBidResult(
    std::unique_ptr<ContextRecycler> context_recycler_for_rerun,
    std::vector<SetBidBindings::BidAndWorkletOnlyMetadata> bids,
    std::optional<uint32_t> bidding_signals_data_version,
    std::optional<GURL> debug_loss_report_url,
    std::optional<GURL> debug_win_report_url,
    std::optional<double> set_priority,
    base::flat_map<std::string, mojom::PrioritySignalsDoublePtr>
        update_priority_signals_overrides,
    PrivateAggregationRequests pa_requests,
    RealTimeReportingContributions real_time_contributions,
    mojom::RejectReason reject_reason,
    bool script_timed_out,
    std::vector<std::string> error_msgs)
    : context_recycler_for_rerun(std::move(context_recycler_for_rerun)),
      bids(std::move(bids)),
      bidding_signals_data_version(std::move(bidding_signals_data_version)),
      debug_loss_report_url(std::move(debug_loss_report_url)),
      debug_win_report_url(std::move(debug_win_report_url)),
      set_priority(std::move(set_priority)),
      update_priority_signals_overrides(
          std::move(update_priority_signals_overrides)),
      pa_requests(std::move(pa_requests)),
      real_time_contributions(std::move(real_time_contributions)),
      reject_reason(reject_reason),
      script_timed_out(script_timed_out),
      error_msgs(std::move(error_msgs)) {}

BidderWorklet::V8State::SingleGenerateBidResult::SingleGenerateBidResult(
    SingleGenerateBidResult&&) = default;
BidderWorklet::V8State::SingleGenerateBidResult::~SingleGenerateBidResult() =
    default;
BidderWorklet::V8State::SingleGenerateBidResult&
BidderWorklet::V8State::SingleGenerateBidResult::operator=(
    SingleGenerateBidResult&&) = default;

void BidderWorklet::V8State::ReportWin(
    bool is_for_additional_bid,
    const std::optional<std::string>& interest_group_name_reporting_id,
    const std::optional<std::string>& buyer_reporting_id,
    const std::optional<std::string>& buyer_and_seller_reporting_id,
    const std::optional<std::string>& selected_buyer_and_seller_reporting_id,
    const std::optional<std::string>& auction_signals_json,
    const std::optional<std::string>& per_buyer_signals_json,
    DirectFromSellerSignalsRequester::Result
        direct_from_seller_result_per_buyer_signals,
    const std::optional<std::string>&
        direct_from_seller_per_buyer_signals_header_ad_slot,
    DirectFromSellerSignalsRequester::Result
        direct_from_seller_result_auction_signals,
    const std::optional<std::string>&
        direct_from_seller_auction_signals_header_ad_slot,
    const std::string& seller_signals_json,
    mojom::KAnonymityBidMode kanon_mode,
    bool bid_is_kanon,
    const GURL& browser_signal_render_url,
    double browser_signal_bid,
    const std::optional<blink::AdCurrency>& browser_signal_bid_currency,
    double browser_signal_highest_scoring_other_bid,
    const std::optional<blink::AdCurrency>&
        browser_signal_highest_scoring_other_bid_currency,
    bool browser_signal_made_highest_scoring_other_bid,
    const std::optional<double>& browser_signal_ad_cost,
    const std::optional<uint16_t>& browser_signal_modeling_signals,
    uint8_t browser_signal_join_count,
    uint8_t browser_signal_recency,
    const url::Origin& browser_signal_seller_origin,
    const std::optional<url::Origin>& browser_signal_top_level_seller_origin,
    const std::optional<base::TimeDelta> browser_signal_reporting_timeout,
    const std::optional<uint32_t>& bidding_signals_data_version,
    uint64_t trace_id,
    ReportWinCallbackInternal callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "post_v8_task", trace_id);
  base::ElapsedTimer elapsed_timer;

  // We may not be allowed any time to run.
  if (browser_signal_reporting_timeout.has_value() &&
      !browser_signal_reporting_timeout->is_positive()) {
    PostReportWinCallbackToUserThread(
        std::move(callback),
        /*report_url=*/std::nullopt,
        /*ad_beacon_map=*/{},
        /*ad_macro_map=*/{},
        /*pa_requests=*/{}, base::TimeDelta(),
        /*script_timed_out=*/true,
        /*errors=*/{"reportWin() aborted due to zero timeout."});
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
  if (!AppendJsonValueOrNull(v8_helper_.get(), context,
                             base::OptionalToPtr(auction_signals_json),
                             &args) ||
      !AppendJsonValueOrNull(v8_helper_.get(), context,
                             base::OptionalToPtr(per_buyer_signals_json),
                             &args) ||
      !v8_helper_->AppendJsonValue(context, seller_signals_json, &args)) {
    PostReportWinCallbackToUserThread(std::move(callback),
                                      /*report_url=*/std::nullopt,
                                      /*ad_beacon_map=*/{},
                                      /*ad_macro_map=*/{},
                                      /*pa_requests=*/{}, base::TimeDelta(),
                                      /*script_timed_out=*/false,
                                      /*errors=*/std::vector<std::string>());
    return;
  }

  v8::Local<v8::Object> browser_signals = v8::Object::New(isolate);
  gin::Dictionary browser_signals_dict(isolate, browser_signals);

  std::string kanon_status;
  switch (kanon_mode) {
    case mojom::KAnonymityBidMode::kEnforce:
      // If k-anon was truly enforced and it passed.
      kanon_status = "passedAndEnforced";
      break;
    case mojom::KAnonymityBidMode::kSimulate:
      if (bid_is_kanon) {
        // If K-anon can determine the value and kSimulate is on.
        kanon_status = "passedNotEnforced";
      } else {
        // Number of ad was below the threshold and kSimulate is on.
        kanon_status = "belowThreshold";
      }
      break;
    case mojom::KAnonymityBidMode::kNone:
      // K-anon cannot determine the theoretical outcome.
      kanon_status = "notCalculated";
      break;
  }

  DeprecatedUrlLazyFiller deprecated_render_url(
      v8_helper_.get(), &v8_logger, &browser_signal_render_url,
      "browserSignals.renderUrl is deprecated."
      " Please use browserSignals.renderURL instead.");
  base::TimeDelta reporting_timeout =
      browser_signal_reporting_timeout.has_value()
          ? *browser_signal_reporting_timeout
          : AuctionV8Helper::kScriptTimeout;
  if (!browser_signals_dict.Set("topWindowHostname",
                                top_window_origin_.host()) ||
      !browser_signals_dict.Set(
          "interestGroupOwner",
          url::Origin::Create(script_source_url_).Serialize()) ||
      (interest_group_name_reporting_id.has_value() &&
       !browser_signals_dict.Set("interestGroupName",
                                 *interest_group_name_reporting_id)) ||
      (buyer_reporting_id.has_value() &&
       !browser_signals_dict.Set("buyerReportingId", *buyer_reporting_id)) ||
      (buyer_and_seller_reporting_id.has_value() &&
       !browser_signals_dict.Set("buyerAndSellerReportingId",
                                 *buyer_and_seller_reporting_id)) ||
      (selected_buyer_and_seller_reporting_id.has_value() &&
       !browser_signals_dict.Set("selectedBuyerAndSellerReportingId",
                                 *selected_buyer_and_seller_reporting_id)) ||
      !browser_signals_dict.Set("renderURL",
                                browser_signal_render_url.spec()) ||
      // TODO(crbug.com/40266734): Remove deprecated `renderUrl` alias.
      !deprecated_render_url.AddDeprecatedUrlGetter(browser_signals,
                                                    "renderUrl") ||
      !browser_signals_dict.Set("bid", browser_signal_bid) ||
      !browser_signals_dict.Set(
          "bidCurrency",
          blink::PrintableAdCurrency(browser_signal_bid_currency)) ||
      (browser_signal_ad_cost.has_value() &&
       !browser_signals_dict.Set("adCost", *browser_signal_ad_cost)) ||
      (browser_signal_modeling_signals.has_value() &&
       !browser_signals_dict.Set(
           "modelingSignals",
           static_cast<double>(*browser_signal_modeling_signals))) ||
      !browser_signals_dict.Set(
          "joinCount", static_cast<double>(browser_signal_join_count)) ||
      (!is_for_additional_bid &&
       !browser_signals_dict.Set(
           "recency", static_cast<double>(browser_signal_recency))) ||
      !browser_signals_dict.Set("highestScoringOtherBid",
                                browser_signal_highest_scoring_other_bid) ||
      !browser_signals_dict.Set(
          "highestScoringOtherBidCurrency",
          blink::PrintableAdCurrency(
              browser_signal_highest_scoring_other_bid_currency)) ||
      !browser_signals_dict.Set(
          "madeHighestScoringOtherBid",
          browser_signal_made_highest_scoring_other_bid) ||
      !browser_signals_dict.Set("seller",
                                browser_signal_seller_origin.Serialize()) ||
      (browser_signal_top_level_seller_origin &&
       !browser_signals_dict.Set(
           "topLevelSeller",
           browser_signal_top_level_seller_origin->Serialize())) ||
      (bidding_signals_data_version.has_value() &&
       !browser_signals_dict.Set("dataVersion",
                                 bidding_signals_data_version.value())) ||
      (base::FeatureList::IsEnabled(
           blink::features::kFledgePassKAnonStatusToReportWin) &&
       !kanon_status.empty() &&
       !browser_signals_dict.Set("kAnonStatus", kanon_status)) ||
      (base::FeatureList::IsEnabled(blink::features::kFledgeReportingTimeout) &&
       !browser_signals_dict.Set("reportingTimeout",
                                 reporting_timeout.InMilliseconds()))) {
    PostReportWinCallbackToUserThread(std::move(callback),
                                      /*report_url=*/std::nullopt,
                                      /*ad_beacon_map=*/{},
                                      /*ad_macro_map=*/{},
                                      /*pa_requests=*/{}, base::TimeDelta(),
                                      /*script_timed_out=*/false,
                                      /*errors=*/std::vector<std::string>());
    return;
  }

  args.push_back(browser_signals);

  std::vector<std::string> errors_out;
  v8::Local<v8::Object> direct_from_seller_signals = v8::Object::New(isolate);
  gin::Dictionary direct_from_seller_signals_dict(isolate,
                                                  direct_from_seller_signals);
  v8::Local<v8::Value> per_buyer_signals = GetDirectFromSellerSignals(
      direct_from_seller_result_per_buyer_signals,
      direct_from_seller_per_buyer_signals_header_ad_slot, *v8_helper_, context,
      errors_out);
  v8::Local<v8::Value> auction_signals = GetDirectFromSellerSignals(
      direct_from_seller_result_auction_signals,
      direct_from_seller_auction_signals_header_ad_slot, *v8_helper_, context,
      errors_out);
  if (!direct_from_seller_signals_dict.Set("perBuyerSignals",
                                           per_buyer_signals) ||
      !direct_from_seller_signals_dict.Set("auctionSignals", auction_signals)) {
    PostReportWinCallbackToUserThread(std::move(callback),
                                      /*report_url=*/std::nullopt,
                                      /*ad_beacon_map=*/{},
                                      /*ad_macro_map=*/{}, /*pa_requests=*/{},
                                      base::TimeDelta(),
                                      /*script_timed_out=*/false,
                                      /*errors=*/std::move(errors_out));
    return;
  }
  args.push_back(direct_from_seller_signals);

  // An empty return value indicates an exception was thrown. Any other return
  // value indicates no exception.
  v8_helper_->MaybeTriggerInstrumentationBreakpoint(
      *debug_id_, "beforeBidderWorkletReportingStart");

  v8::Local<v8::UnboundScript> unbound_worklet_script =
      worklet_script_.Get(isolate);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "report_win", trace_id);
  std::unique_ptr<AuctionV8Helper::TimeLimit> total_timeout =
      v8_helper_->CreateTimeLimit(
          /*script_timeout=*/browser_signal_reporting_timeout);
  AuctionV8Helper::Result result =
      v8_helper_->RunScript(context, unbound_worklet_script, debug_id_.get(),
                            total_timeout.get(), errors_out);
  if (result != AuctionV8Helper::Result::kSuccess) {
    TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "report_win", trace_id);
    PostReportWinCallbackToUserThread(
        std::move(callback), /*report_url=*/std::nullopt,
        /*ad_beacon_map=*/{}, /*ad_macro_map=*/{},
        /*pa_requests=*/{}, elapsed_timer.Elapsed(),
        /*script_timed_out=*/result == AuctionV8Helper::Result::kTimeout,
        std::move(errors_out));
    return;
  }

  context_recycler.AddReportBindings();
  context_recycler.AddRegisterAdBeaconBindings();
  context_recycler.AddRegisterAdMacroBindings();

  context_recycler.AddPrivateAggregationBindings(
      permissions_policy_state_->private_aggregation_allowed,
      /*reserved_once_allowed=*/false);

  if (base::FeatureList::IsEnabled(blink::features::kSharedStorageAPI)) {
    context_recycler.AddSharedStorageBindings(
        shared_storage_host_remote_.is_bound()
            ? shared_storage_host_remote_.get()
            : nullptr,
        mojom::AuctionWorkletFunction::kBidderReportWin,
        permissions_policy_state_->shared_storage_allowed);
  }

  v8::MaybeLocal<v8::Value> maybe_report_result_ret;
  result = v8_helper_->CallFunction(
      context, debug_id_.get(),
      v8_helper_->FormatScriptName(unbound_worklet_script),
      is_for_additional_bid ? "reportAdditionalBidWin" : "reportWin", args,
      total_timeout.get(), maybe_report_result_ret, errors_out);
  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "report_win", trace_id);
  base::TimeDelta elapsed = elapsed_timer.Elapsed();
  base::UmaHistogramTimes("Ads.InterestGroup.Auction.ReportWinTime", elapsed);

  if (result != AuctionV8Helper::Result::kSuccess) {
    // Keep Private Aggregation API requests since `reportWin()` might use it to
    // detect script timeout or failures.
    PostReportWinCallbackToUserThread(
        std::move(callback), /*report_url=*/std::nullopt,
        /*ad_beacon_map=*/{}, /*ad_macro_map=*/{},
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        elapsed,
        /*script_timed_out=*/result == AuctionV8Helper::Result::kTimeout,
        std::move(errors_out));
    return;
  }

  // This covers both the case where a report URL was provided, and the case one
  // was not.
  PostReportWinCallbackToUserThread(
      std::move(callback), context_recycler.report_bindings()->report_url(),
      context_recycler.register_ad_beacon_bindings()->TakeAdBeaconMap(),
      context_recycler.register_ad_macro_bindings()->TakeAdMacroMap(),
      context_recycler.private_aggregation_bindings()
          ->TakePrivateAggregationRequests(),
      elapsed, /*script_timed_out=*/false, std::move(errors_out));
}

void BidderWorklet::V8State::GenerateBid(
    mojom::BidderWorkletNonSharedParamsPtr bidder_worklet_non_shared_params,
    mojom::KAnonymityBidMode kanon_mode,
    const url::Origin& interest_group_join_origin,
    const std::optional<std::string>& auction_signals_json,
    const std::optional<std::string>& per_buyer_signals_json,
    DirectFromSellerSignalsRequester::Result
        direct_from_seller_result_per_buyer_signals,
    const std::optional<std::string>&
        direct_from_seller_per_buyer_signals_header_ad_slot,
    DirectFromSellerSignalsRequester::Result
        direct_from_seller_result_auction_signals,
    const std::optional<std::string>&
        direct_from_seller_auction_signals_header_ad_slot,
    const std::optional<base::TimeDelta> per_buyer_timeout,
    const std::optional<blink::AdCurrency>& expected_buyer_currency,
    const url::Origin& browser_signal_seller_origin,
    const std::optional<url::Origin>& browser_signal_top_level_seller_origin,
    const base::TimeDelta browser_signal_recency,
    blink::mojom::BiddingBrowserSignalsPtr bidding_browser_signals,
    base::Time auction_start_time,
    const std::optional<blink::AdSize>& requested_ad_size,
    uint16_t multi_bid_limit,
    scoped_refptr<TrustedSignals::Result> trusted_bidding_signals_result,
    bool trusted_bidding_signals_fetch_failed,
    uint64_t trace_id,
    base::ScopedClosureRunner cleanup_generate_bid_task,
    GenerateBidCallbackInternal callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "post_v8_task", trace_id);

  // Don't need to run `cleanup_generate_bid_task` if this method is invoked;
  // it's bound to the closure to clean things up if this method got cancelled.
  cleanup_generate_bid_task.ReplaceClosure(base::OnceClosure());

  base::TimeTicks bidding_start = base::TimeTicks::Now();
  std::optional<SingleGenerateBidResult> result = RunGenerateBidOnce(
      *bidder_worklet_non_shared_params, interest_group_join_origin,
      base::OptionalToPtr(auction_signals_json),
      base::OptionalToPtr(per_buyer_signals_json),
      direct_from_seller_result_per_buyer_signals,
      direct_from_seller_per_buyer_signals_header_ad_slot,
      direct_from_seller_result_auction_signals,
      direct_from_seller_auction_signals_header_ad_slot, per_buyer_timeout,
      expected_buyer_currency, browser_signal_seller_origin,
      base::OptionalToPtr(browser_signal_top_level_seller_origin),
      browser_signal_recency, bidding_browser_signals, auction_start_time,
      requested_ad_size, multi_bid_limit, trusted_bidding_signals_result,
      trace_id,
      /*context_recycler_for_rerun=*/nullptr,
      /*restrict_to_kanon_ads=*/false);

  if (!result.has_value()) {
    PostErrorBidCallbackToUserThread(
        std::move(callback),
        /*bidding_latency=*/base::TimeTicks::Now() - bidding_start,
        PrivateAggregationRequests(),
        GetRealTimeReportingContributionsOnError(
            trusted_bidding_signals_fetch_failed, /*is_bidding_signal=*/true));
    return;
  }

  std::vector<mojom::BidderWorkletBidPtr> bids =
      ClassifyBidsAndApplyComponentAdLimits(
          kanon_mode, bidder_worklet_non_shared_params.get(),
          script_source_url_, std::move(result->bids));

  if (kanon_mode != mojom::KAnonymityBidMode::kNone) {
    // Go through and see which bids are actually k-anon appropriate.
    bool found_kanon_bid = false;
    for (const auto& bid : bids) {
      if (bid->bid_role != mojom::BidRole::kUnenforcedKAnon) {
        found_kanon_bid = true;
        break;
      }
    }

    // If bids were returned, and none were k-anon, we re-run the script with
    // only k-anon ads available to it.
    //
    // At this point, we don't need to worry about the impact of k-anonymity
    // for reporting on k-anon where a bid returns a value for
    // `selectedBuyerAndSellerReportingId`. Assuming we find some
    // k-anon ads in the loop below that would encourage us to make the
    // k-anon-restricted call to `generateBid()`, we're only going to pass
    // `selectableBuyerAndSellerReportingIds` that would, in combination with
    // the renderUrl and other reporting ids, cause that bid to be k-anonymous
    // for reporting. If no `selectableBuyerAndSellerReportingIds` meet that
    // criteria, that's still fine: The bid could be returned with no
    // `selectedBuyerAndSellerReportingId`, such that the k-anonymous bid
    // would remain k-anonymous, even if it isn't k-anonymous for reporting.
    if (!bids.empty() && !found_kanon_bid) {
      bool has_kanon_ads = false;
      for (const auto& ad : *bidder_worklet_non_shared_params->ads) {
        if (BidderWorklet::IsKAnon(
                bidder_worklet_non_shared_params.get(),
                blink::HashedKAnonKeyForAdBid(owner_, script_source_url_,
                                              ad.render_url()))) {
          has_kanon_ads = true;
          break;
        }
      }

      // Main run got a non-k-anon result, and we care about k-anonymity. Re-run
      // the bidder with non-k-anon ads hidden, limiting it to a single bid.
      std::optional<SingleGenerateBidResult> restricted_result;
      if (has_kanon_ads) {
        restricted_result = RunGenerateBidOnce(
            *bidder_worklet_non_shared_params.get(), interest_group_join_origin,
            base::OptionalToPtr(auction_signals_json),
            base::OptionalToPtr(per_buyer_signals_json),
            direct_from_seller_result_per_buyer_signals,
            direct_from_seller_per_buyer_signals_header_ad_slot,
            direct_from_seller_result_auction_signals,
            direct_from_seller_auction_signals_header_ad_slot,
            per_buyer_timeout, expected_buyer_currency,
            browser_signal_seller_origin,
            base::OptionalToPtr(browser_signal_top_level_seller_origin),
            browser_signal_recency, bidding_browser_signals, auction_start_time,
            requested_ad_size, /* multi_bid_limit=*/1,
            trusted_bidding_signals_result, trace_id,
            std::move(result->context_recycler_for_rerun),
            /*restrict_to_kanon_ads=*/true);
      } else {
        restricted_result = SingleGenerateBidResult(
            std::unique_ptr<ContextRecycler>(),
            std::vector<SetBidBindings::BidAndWorkletOnlyMetadata>(),
            /*bidding_signals_data_version=*/std::nullopt,
            /*debug_loss_report_url=*/std::nullopt,
            /*debug_win_report_url=*/std::nullopt,
            /*set_priority=*/std::nullopt,
            /*update_priority_signals_overrides=*/{},
            /*pa_requests=*/{},
            /*real_time_contributions=*/{},
            /*reject_reason=*/mojom::RejectReason::kNotAvailable,
            /*script_timed_out=*/false,
            /*error_msgs=*/{});
      }
      if (restricted_result.has_value()) {
        // All the bids from the re-run will be k-anon enforced; we need to make
        // sure to apply the component ad reduction, too.
        for (auto& candidate : restricted_result->bids) {
          candidate.bid->bid_role =
              auction_worklet::mojom::BidRole::kEnforcedKAnon;
          TrimAndCollectBid(std::move(candidate.bid),
                            candidate.target_num_ad_components, bids);
        }
      }

      // Figure out which of `restricted_result` or `result` we actually want
      // to use.
      if (kanon_mode == mojom::KAnonymityBidMode::kEnforce) {
        PrivateAggregationRequests non_kanon_pa_requests =
            std::move(result->pa_requests);
        std::erase_if(
            non_kanon_pa_requests,
            [](const auction_worklet::mojom::PrivateAggregationRequestPtr&
                   request) { return !HasKAnonFailureComponent(*request); });

        // We are enforcing the k-anonymity, so the restricted result is the one
        // to use for reporting, etc., and needs to succeed.
        if (!restricted_result.has_value()) {
          PostErrorBidCallbackToUserThread(
              std::move(callback),
              /*bidding_latency=*/base::TimeTicks::Now() - bidding_start,
              std::move(non_kanon_pa_requests),
              GetRealTimeReportingContributionsOnError(
                  trusted_bidding_signals_fetch_failed,
                  /*is_bidding_signal=*/true));
          return;
        }
        result = std::move(restricted_result);
        result->non_kanon_pa_requests = std::move(non_kanon_pa_requests);
      } else {
        DCHECK_EQ(kanon_mode, mojom::KAnonymityBidMode::kSimulate);
        // Here, `result` is already what we want for reporting, etc., so
        // nothing actually to do in this case.
      }
    }
  }

  // Add platform contributions if there needs to be one.
  MaybeAddRealTimeReportingPlatformContributions(
      trusted_bidding_signals_fetch_failed, /*is_bidding_signal=*/true,
      result->real_time_contributions);

  user_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(bids),
                     std::move(result->bidding_signals_data_version),
                     std::move(result->debug_loss_report_url),
                     std::move(result->debug_win_report_url),
                     std::move(result->set_priority),
                     std::move(result->update_priority_signals_overrides),
                     std::move(result->pa_requests),
                     std::move(result->non_kanon_pa_requests),
                     std::move(result->real_time_contributions),
                     /*bidding_latency=*/base::TimeTicks::Now() - bidding_start,
                     result->reject_reason, result->script_timed_out,
                     std::move(result->error_msgs)));
}

std::optional<BidderWorklet::V8State::SingleGenerateBidResult>
BidderWorklet::V8State::RunGenerateBidOnce(
    const mojom::BidderWorkletNonSharedParams& bidder_worklet_non_shared_params,
    const url::Origin& interest_group_join_origin,
    const std::string* auction_signals_json,
    const std::string* per_buyer_signals_json,
    const DirectFromSellerSignalsRequester::Result&
        direct_from_seller_result_per_buyer_signals,
    const std::optional<std::string>&
        direct_from_seller_per_buyer_signals_header_ad_slot,
    const DirectFromSellerSignalsRequester::Result&
        direct_from_seller_result_auction_signals,
    const std::optional<std::string>&
        direct_from_seller_auction_signals_header_ad_slot,
    const std::optional<base::TimeDelta> per_buyer_timeout,
    const std::optional<blink::AdCurrency>& expected_buyer_currency,
    const url::Origin& browser_signal_seller_origin,
    const url::Origin* browser_signal_top_level_seller_origin,
    const base::TimeDelta browser_signal_recency,
    const blink::mojom::BiddingBrowserSignalsPtr& bidding_browser_signals,
    base::Time auction_start_time,
    const std::optional<blink::AdSize>& requested_ad_size,
    uint16_t multi_bid_limit,
    const scoped_refptr<TrustedSignals::Result>& trusted_bidding_signals_result,
    uint64_t trace_id,
    std::unique_ptr<ContextRecycler> context_recycler_for_rerun,
    bool restrict_to_kanon_ads) {
  // Can't make a bid without any ads, or if we aren't permitted to spend any
  // time on it.
  if (!bidder_worklet_non_shared_params.ads) {
    return std::nullopt;
  }

  std::vector<std::string> errors_out;
  if (per_buyer_timeout.has_value() &&
      !per_buyer_timeout.value().is_positive()) {
    errors_out.push_back("generateBid() aborted due to zero timeout.");
    return std::make_optional(SingleGenerateBidResult(
        std::unique_ptr<ContextRecycler>(),
        std::vector<SetBidBindings::BidAndWorkletOnlyMetadata>(),
        /*bidding_signals_data_version=*/std::nullopt,
        /*debug_loss_report_url=*/std::nullopt,
        /*debug_win_report_url=*/std::nullopt,
        /*set_priority=*/std::nullopt,
        /*update_priority_signals_overrides=*/{},
        /*pa_requests=*/{},
        /*real_time_contributions=*/{},
        /*reject_reason=*/mojom::RejectReason::kNotAvailable,
        /*script_timed_out=*/true, std::move(errors_out)));
  }

  if (context_recycler_for_rerun) {
    DCHECK(restrict_to_kanon_ads);
  }

  bool script_timed_out = false;

  base::TimeTicks start = base::TimeTicks::Now();

  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper_.get());
  v8::Isolate* isolate = v8_helper_->isolate();
  ContextRecycler* context_recycler = nullptr;
  std::unique_ptr<ContextRecycler> fresh_context_recycler;

  bool reused_context = false;
  bool should_deep_freeze = false;
  std::string_view execution_mode_string;
  // See if we can reuse an existing context, and determine string to use for
  // `executionMode`.
  switch (bidder_worklet_non_shared_params.execution_mode) {
    case blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode:
      execution_mode_string = "group-by-origin";
      {
        auto it = context_recyclers_for_origin_group_mode_.Get(
            interest_group_join_origin);
        if (it != context_recyclers_for_origin_group_mode_.end()) {
          context_recycler = it->second.get();
          reused_context = true;
        }
      }
      break;
    case blink::mojom::InterestGroup::ExecutionMode::kFrozenContext:
      execution_mode_string = "frozen-context";
      if (!base::FeatureList::IsEnabled(
              blink::features::kFledgeAlwaysReuseBidderContext)) {
        should_deep_freeze = true;
      }
      if (context_recycler_for_frozen_context_) {
        context_recycler = context_recycler_for_frozen_context_.get();
        reused_context = true;
      }
      break;
    case blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode:
      execution_mode_string = "compatibility";
      break;
  }
  DCHECK(!execution_mode_string.empty());

  base::UmaHistogramBoolean("Ads.InterestGroup.Auction.ContextReused",
                            reused_context);

  if (!context_recycler && context_recycler_for_always_reuse_feature_) {
    context_recycler = context_recycler_for_always_reuse_feature_.get();
    reused_context = true;
  }

  // See if we can reuse a context for k-anon re-run. The group-by-origin and
  // frozen context mode would do that, too, so this is only a fallback for
  // when that's not on.
  if (!context_recycler && context_recycler_for_rerun) {
    context_recycler = context_recycler_for_rerun.get();
    reused_context = true;
  }

  v8_helper_->MaybeTriggerInstrumentationBreakpoint(
      *debug_id_, "beforeBidderWorkletBiddingStart");

  std::unique_ptr<AuctionV8Helper::TimeLimit> total_timeout =
      v8_helper_->CreateTimeLimit(per_buyer_timeout);

  // No recycled context, make a fresh one.
  if (!context_recycler) {
    fresh_context_recycler = CreateContextRecyclerAndRunTopLevelForGenerateBid(
        trace_id, *total_timeout, should_deep_freeze, script_timed_out,
        errors_out);

    if (!fresh_context_recycler) {
      return std::make_optional(SingleGenerateBidResult(
          std::unique_ptr<ContextRecycler>(),
          std::vector<SetBidBindings::BidAndWorkletOnlyMetadata>(),
          /*bidding_signals_data_version=*/std::nullopt,
          /*debug_loss_report_url=*/std::nullopt,
          /*debug_win_report_url=*/std::nullopt,
          /*set_priority=*/std::nullopt,
          /*update_priority_signals_overrides=*/{},
          /*pa_requests=*/{},
          /*real_time_contributions=*/{},
          /*reject_reason=*/mojom::RejectReason::kNotAvailable,
          /*script_timed_out=*/script_timed_out, std::move(errors_out)));
    }

    context_recycler = fresh_context_recycler.get();

    // Save the context for next time (if applicable).
    if (base::FeatureList::IsEnabled(
            blink::features::kFledgeAlwaysReuseBidderContext)) {
      context_recycler_for_always_reuse_feature_ =
          std::move(fresh_context_recycler);
    } else {
      switch (bidder_worklet_non_shared_params.execution_mode) {
        case blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode:
          context_recyclers_for_origin_group_mode_.Put(
              interest_group_join_origin, std::move(fresh_context_recycler));
          break;
        case blink::mojom::InterestGroup::ExecutionMode::kFrozenContext:
          context_recycler_for_frozen_context_ =
              std::move(fresh_context_recycler);
          break;
        case blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode:
          break;
      }
    }
  }

  base::RepeatingCallback<bool(const std::string&)>
      should_exclude_ad_due_to_kanon = base::BindRepeating(
          [](bool restrict_to_kanon_ads,
             const mojom::BidderWorkletNonSharedParams* params,
             const url::Origin* owner, const GURL* bidding_url,
             const std::string& ad_url_from_gurl_spec) {
            return restrict_to_kanon_ads &&
                   !BidderWorklet::IsKAnon(
                       params,
                       blink::HashedKAnonKeyForAdBid(*owner, *bidding_url,
                                                     ad_url_from_gurl_spec));
          },
          restrict_to_kanon_ads, &bidder_worklet_non_shared_params, &owner_,
          &script_source_url_);

  base::RepeatingCallback<bool(const std::string&)>
      should_exclude_component_ad_due_to_kanon = base::BindRepeating(
          [](bool restrict_to_kanon_ads,
             const mojom::BidderWorkletNonSharedParams* params,
             const std::string& ad_url_from_gurl_spec) {
            return restrict_to_kanon_ads &&
                   !BidderWorklet::IsKAnon(
                       params, blink::HashedKAnonKeyForAdComponentBid(
                                   ad_url_from_gurl_spec));
          },
          restrict_to_kanon_ads, &bidder_worklet_non_shared_params);

  base::RepeatingCallback<bool(const std::string&,
                               base::optional_ref<const std::string>,
                               base::optional_ref<const std::string>,
                               base::optional_ref<const std::string>)>
      should_exclude_reporting_id_set_due_to_kanon = base::BindRepeating(
          [](bool restrict_to_kanon_ads,
             const mojom::BidderWorkletNonSharedParams* params,
             const url::Origin* owner, const GURL* bidding_url,
             const std::string& ad_url_from_gurl_spec,
             base::optional_ref<const std::string> buyer_reporting_id,
             base::optional_ref<const std::string>
                 buyer_and_seller_reporting_id,
             base::optional_ref<const std::string>
                 selected_buyer_and_seller_reporting_id) {
            return restrict_to_kanon_ads &&
                   !BidderWorklet::IsKAnon(
                       params,
                       blink::
                           HashedKAnonKeyForAdNameReportingWithoutInterestGroup(
                               /*interest_group_owner=*/*owner,
                               /*interest_group_name=*/params->name,
                               /*interest_group_bidding_url=*/*bidding_url,
                               /*ad_render_url=*/ad_url_from_gurl_spec,
                               buyer_reporting_id,
                               buyer_and_seller_reporting_id,
                               selected_buyer_and_seller_reporting_id));
          },
          restrict_to_kanon_ads, &bidder_worklet_non_shared_params, &owner_,
          &script_source_url_);

  ContextRecyclerScope context_recycler_scope(*context_recycler);
  v8::Local<v8::Context> context = context_recycler_scope.GetContext();

  context_recycler->set_bid_bindings()->ReInitialize(
      start, browser_signal_top_level_seller_origin != nullptr,
      &bidder_worklet_non_shared_params, expected_buyer_currency,
      multi_bid_limit, should_exclude_ad_due_to_kanon,
      should_exclude_component_ad_due_to_kanon,
      should_exclude_reporting_id_set_due_to_kanon);

  v8::LocalVector<v8::Value> args(isolate);
  v8::Local<v8::Object> interest_group_object = v8::Object::New(isolate);
  gin::Dictionary interest_group_dict(isolate, interest_group_object);
  if (!interest_group_dict.Set("owner", owner_.Serialize()) ||
      !interest_group_dict.Set("name", bidder_worklet_non_shared_params.name) ||
      !interest_group_dict.Set("enableBiddingSignalsPrioritization",
                               bidder_worklet_non_shared_params
                                   .enable_bidding_signals_prioritization) ||
      !interest_group_dict.Set("executionMode", execution_mode_string) ||
      !interest_group_dict.Set(
          "trustedBiddingSignalsSlotSizeMode",
          blink::InterestGroup::TrustedBiddingSignalsSlotSizeModeToString(
              bidder_worklet_non_shared_params
                  .trusted_bidding_signals_slot_size_mode))) {
    return std::nullopt;
  }

  context_recycler->interest_group_lazy_filler()->ReInitialize(
      &script_source_url_,
      wasm_helper_url_.has_value() ? &wasm_helper_url_.value() : nullptr,
      trusted_bidding_signals_url_.has_value()
          ? &trusted_bidding_signals_url_.value()
          : nullptr,
      &bidder_worklet_non_shared_params);
  if (!context_recycler->interest_group_lazy_filler()->FillInObject(
          interest_group_object, should_exclude_ad_due_to_kanon,
          should_exclude_component_ad_due_to_kanon,
          should_exclude_reporting_id_set_due_to_kanon)) {
    return std::nullopt;
  }

  args.push_back(std::move(interest_group_object));

  if (!AppendJsonValueOrNull(v8_helper_.get(), context, auction_signals_json,
                             &args) ||
      !AppendJsonValueOrNull(v8_helper_.get(), context, per_buyer_signals_json,
                             &args)) {
    return std::nullopt;
  }

  v8::Local<v8::Value> trusted_signals;
  SignalsOriginRelation trusted_signals_relation = ClassifyTrustedSignals(
      script_source_url_, trusted_bidding_signals_origin_);
  UMA_HISTOGRAM_ENUMERATION(
      "Ads.InterestGroup.Auction.TrustedBidderSignalsOriginRelation",
      trusted_signals_relation);
  if (!trusted_bidding_signals_result ||
      !bidder_worklet_non_shared_params.trusted_bidding_signals_keys ||
      bidder_worklet_non_shared_params.trusted_bidding_signals_keys->empty()) {
    trusted_signals = v8::Null(isolate);
  } else {
    trusted_signals = trusted_bidding_signals_result->GetBiddingSignals(
        v8_helper_.get(), context,
        *bidder_worklet_non_shared_params.trusted_bidding_signals_keys);
  }

  if (trusted_signals_relation == SignalsOriginRelation::kSameOriginSignals) {
    args.push_back(trusted_signals);
  } else {
    args.push_back(v8::Null(isolate));
  }

  std::optional<uint32_t> bidding_signals_data_version;
  if (trusted_bidding_signals_result) {
    bidding_signals_data_version =
        trusted_bidding_signals_result->GetDataVersion();
  }

  v8::Local<v8::Object> browser_signals = v8::Object::New(isolate);
  gin::Dictionary browser_signals_dict(isolate, browser_signals);
  // TODO(crbug.com/336164429): Construct the fields of browser signals lazily.
  if (!browser_signals_dict.Set("topWindowHostname",
                                top_window_origin_.host()) ||
      !browser_signals_dict.Set("seller",
                                browser_signal_seller_origin.Serialize()) ||
      (browser_signal_top_level_seller_origin &&
       !browser_signals_dict.Set(
           "topLevelSeller",
           browser_signal_top_level_seller_origin->Serialize())) ||
      !browser_signals_dict.Set("joinCount",
                                bidding_browser_signals->join_count) ||
      !browser_signals_dict.Set("bidCount",
                                bidding_browser_signals->bid_count) ||
      (base::FeatureList::IsEnabled(
           blink::features::kBiddingAndScoringDebugReportingAPI) &&
       base::FeatureList::IsEnabled(
           blink::features::kFledgeSampleDebugReports) &&
       !browser_signals_dict.Set(
           "forDebuggingOnlyInCooldownOrLockout",
           bidding_browser_signals
               ->for_debugging_only_in_cooldown_or_lockout)) ||
      // `adComponentsLimit` is reported only when the corresponding change
      // is rolled out, to avoid affecting behavior if it's not.
      (base::FeatureList::IsEnabled(
           blink::features::kFledgeCustomMaxAuctionAdComponents) &&
       !browser_signals_dict.Set(
           "adComponentsLimit",
           // Cast to help gin on mac.
           static_cast<uint64_t>(blink::MaxAdAuctionAdComponents()))) ||
      // Report the multi-bid limit if the corresponding feature on.
      (SupportMultiBid() &&
       !browser_signals_dict.Set("multiBidLimit",
                                 static_cast<uint32_t>(multi_bid_limit))) ||
      (base::FeatureList::IsEnabled(
           blink::features::kFledgePassRecencyToGenerateBid) &&
       !browser_signals_dict.Set("recency",
                                 browser_signal_recency.InMilliseconds())) ||
      !SetDataVersion(trusted_signals_relation, bidding_signals_data_version,
                      browser_signals_dict) ||
      (requested_ad_size.has_value() &&
       !MaybeSetSizeMember(isolate, browser_signals_dict, "requestedSize",
                           requested_ad_size.value()))) {
    return std::nullopt;
  }

  if (wasm_helper_.success()) {
    v8::Local<v8::WasmModuleObject> module;
    v8::Maybe<bool> result = v8::Nothing<bool>();
    if (WorkletWasmLoader::MakeModule(wasm_helper_).ToLocal(&module)) {
      result = browser_signals->Set(
          context, gin::StringToV8(isolate, "wasmHelper"), module);
    }
    if (result.IsNothing() || !result.FromJust()) {
      return std::nullopt;
    }
  }

  context_recycler->bidding_browser_signals_lazy_filler()->ReInitialize(
      bidding_browser_signals.get(), auction_start_time);
  if (!context_recycler->bidding_browser_signals_lazy_filler()->FillInObject(
          browser_signals)) {
    return std::nullopt;
  }

  args.push_back(browser_signals);

  v8::Local<v8::Object> direct_from_seller_signals = v8::Object::New(isolate);
  gin::Dictionary direct_from_seller_signals_dict(isolate,
                                                  direct_from_seller_signals);
  v8::Local<v8::Value> per_buyer_signals = GetDirectFromSellerSignals(
      direct_from_seller_result_per_buyer_signals,
      direct_from_seller_per_buyer_signals_header_ad_slot, *v8_helper_, context,
      errors_out);
  v8::Local<v8::Value> auction_signals = GetDirectFromSellerSignals(
      direct_from_seller_result_auction_signals,
      direct_from_seller_auction_signals_header_ad_slot, *v8_helper_, context,
      errors_out);
  if (!direct_from_seller_signals_dict.Set("perBuyerSignals",
                                           per_buyer_signals) ||
      !direct_from_seller_signals_dict.Set("auctionSignals", auction_signals)) {
    return std::nullopt;
  }
  args.push_back(direct_from_seller_signals);

  if (base::FeatureList::IsEnabled(
          blink::features::kFledgePermitCrossOriginTrustedSignals)) {
    v8::Local<v8::Value> cross_origin_trusted_bidding_signals_value;
    if (trusted_signals_relation ==
        SignalsOriginRelation::kCrossOriginSignals) {
      cross_origin_trusted_bidding_signals_value =
          TrustedSignals::Result::WrapCrossOriginSignals(
              v8_helper_.get(), context, *trusted_bidding_signals_origin_,
              trusted_signals);
    } else {
      cross_origin_trusted_bidding_signals_value = v8::Null(isolate);
    }
    args.push_back(cross_origin_trusted_bidding_signals_value);
  }

  v8::Local<v8::Value> generate_bid_result;

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "generate_bid", trace_id);
  v8::MaybeLocal<v8::Value> maybe_generate_bid_result;

  AuctionV8Helper::Result script_result = v8_helper_->CallFunction(
      context, debug_id_.get(),
      v8_helper_->FormatScriptName(worklet_script_.Get(isolate)), "generateBid",
      args, total_timeout.get(), maybe_generate_bid_result, errors_out);
  bool got_return_value =
      script_result == AuctionV8Helper::Result::kSuccess &&
      maybe_generate_bid_result.ToLocal(&generate_bid_result);
  script_timed_out = script_result == AuctionV8Helper::Result::kTimeout;
  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "generate_bid", trace_id);

  base::TimeDelta time_duration = base::TimeTicks::Now() - start;
  base::UmaHistogramTimes("Ads.InterestGroup.Auction.GenerateBidTime",
                          time_duration);

  RealTimeReportingContributions real_time_contributions =
      context_recycler->real_time_reporting_bindings()
          ->TakeRealTimeReportingContributions();

  // Remove worklet latency contributions if the worklet execution time is
  // within the threshold.
  std::erase_if(
      real_time_contributions,
      [time_duration](
          const auction_worklet::mojom::RealTimeReportingContributionPtr&
              contribution) {
        return contribution->latency_threshold.has_value() &&
               time_duration.InMilliseconds() <=
                   contribution->latency_threshold.value();
      });

  if (got_return_value) {
    IdlConvert::Status status =
        context_recycler->set_bid_bindings()->SetBidImpl(
            generate_bid_result,
            base::StrCat({script_source_url_.spec(), " generateBid() "}));
    if (!status.is_success()) {
      if (status.is_timeout()) {
        script_timed_out = true;
      }
      errors_out.push_back(status.ConvertToErrorString(isolate));
    }
  }

  if (!context_recycler->set_bid_bindings()->has_bids()) {
    // If no bid was returned (due to an error or just not choosing to bid), or
    // the method timed out and no intermediate result was given through
    // `setBid()`, return an error. Keep debug loss reports, Private
    // Aggregation requests and real time reporting contributions  since
    // `generateBid()` might use them to detect script timeout or failures. Keep
    // any set priority and set priority overrides because an interest group may
    // want to update them even when not bidding. No need to return a
    // ContextRecycler since this will not be re-run.
    return std::make_optional(SingleGenerateBidResult(
        std::unique_ptr<ContextRecycler>(),
        std::vector<SetBidBindings::BidAndWorkletOnlyMetadata>(),
        /*bidding_signals_data_version=*/std::nullopt,
        context_recycler->for_debugging_only_bindings()->TakeLossReportUrl(),
        /*debug_win_report_url=*/std::nullopt,
        context_recycler->set_priority_bindings()->set_priority(),
        context_recycler->set_priority_signals_override_bindings()
            ->TakeSetPrioritySignalsOverrides(),
        context_recycler->private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        std::move(real_time_contributions),
        context_recycler->set_bid_bindings()->reject_reason(), script_timed_out,
        std::move(errors_out)));
  }

  // If the context recycler wasn't saved based on `execution_mode`,
  // `fresh_context_recycler` is non-null here, and it will be provided to the
  // caller for potential re-use for k-anon re-run.
  return std::make_optional(SingleGenerateBidResult(
      std::move(fresh_context_recycler),
      context_recycler->set_bid_bindings()->TakeBids(),
      bidding_signals_data_version,
      context_recycler->for_debugging_only_bindings()->TakeLossReportUrl(),
      context_recycler->for_debugging_only_bindings()->TakeWinReportUrl(),
      context_recycler->set_priority_bindings()->set_priority(),
      context_recycler->set_priority_signals_override_bindings()
          ->TakeSetPrioritySignalsOverrides(),
      context_recycler->private_aggregation_bindings()
          ->TakePrivateAggregationRequests(),
      std::move(real_time_contributions), mojom::RejectReason::kNotAvailable,
      script_timed_out, std::move(errors_out)));
}

std::unique_ptr<ContextRecycler>
BidderWorklet::V8State::CreateContextRecyclerAndRunTopLevelForGenerateBid(
    uint64_t trace_id,
    AuctionV8Helper::TimeLimit& total_timeout,
    bool should_deep_freeze,
    bool& script_timed_out,
    std::vector<std::string>& errors_out) {
  base::TimeTicks start = base::TimeTicks::Now();
  std::unique_ptr<ContextRecycler> context_recycler =
      std::make_unique<ContextRecycler>(v8_helper_.get());

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "get_bidder_context", trace_id);
  ContextRecyclerScope context_recycler_scope(*context_recycler);
  v8::Local<v8::Context> context = context_recycler_scope.GetContext();
  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "get_bidder_context", trace_id);

  v8::Local<v8::UnboundScript> unbound_worklet_script =
      worklet_script_.Get(v8_helper_->isolate());

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "biddingScript", trace_id);
  AuctionV8Helper::Result result =
      v8_helper_->RunScript(context, unbound_worklet_script, debug_id_.get(),
                            &total_timeout, errors_out);
  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "biddingScript", trace_id);
  base::UmaHistogramTimes("Ads.InterestGroup.Auction.BidScriptTime",
                          base::TimeTicks::Now() - start);

  if (result == AuctionV8Helper::Result::kTimeout) {
    script_timed_out = true;
  }

  if (result != AuctionV8Helper::Result::kSuccess) {
    return nullptr;
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
        mojom::AuctionWorkletFunction::kBidderGenerateBid,
        permissions_policy_state_->shared_storage_allowed);
  }

  context_recycler->AddSetBidBindings();
  context_recycler->AddSetPriorityBindings();
  context_recycler->AddSetPrioritySignalsOverrideBindings();
  context_recycler->AddInterestGroupLazyFiller();
  context_recycler->AddBiddingBrowserSignalsLazyFiller();

  if (should_deep_freeze) {
    v8::TryCatch try_catch(v8_helper_->isolate());
    DeepFreezeAllowAll allow_jsapiobject;
    context->DeepFreeze(&allow_jsapiobject);
    if (try_catch.HasCaught()) {
      errors_out.push_back(AuctionV8Helper::FormatExceptionMessage(
          context, try_catch.Message()));
      return nullptr;
    }
  }

  return context_recycler;
}

void BidderWorklet::V8State::ConnectDevToolsAgent(
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  v8_helper_->ConnectDevToolsAgent(std::move(agent), user_thread_, *debug_id_);
}

BidderWorklet::V8State::~V8State() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);

  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper_.get());
  v8::Isolate* isolate = v8_helper_->isolate();
  v8::HeapStatistics heap_statistics;
  isolate->GetHeapStatistics(&heap_statistics);
  base::UmaHistogramCounts100000(
      "Ads.InterestGroup.Auction.BidderWorkletIsolateUsedHeapSizeKilobytes",
      heap_statistics.used_heap_size() / 1024);
  base::UmaHistogramCounts100000(
      "Ads.InterestGroup.Auction.BidderWorkletIsolateTotalHeapSizeKilobytes",
      heap_statistics.total_heap_size() / 1024);
}

void BidderWorklet::V8State::FinishInit(
    mojo::PendingRemote<mojom::AuctionSharedStorageHost>
        shared_storage_host_remote) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);

  if (shared_storage_host_remote) {
    shared_storage_host_remote_.Bind(std::move(shared_storage_host_remote));
  }

  debug_id_->SetResumeCallback(base::BindOnce(
      &BidderWorklet::V8State::PostResumeToUserThread, parent_, user_thread_));
}

// static
void BidderWorklet::V8State::PostResumeToUserThread(
    base::WeakPtr<BidderWorklet> parent,
    scoped_refptr<base::SequencedTaskRunner> user_thread) {
  // This is static since it's called from debugging, not BidderWorklet,
  // so the usual guarantee that BidderWorklet posts things before posting
  // V8State destruction is irrelevant.
  user_thread->PostTask(FROM_HERE,
                        base::BindOnce(&BidderWorklet::ResumeIfPaused, parent));
}

void BidderWorklet::V8State::PostReportWinCallbackToUserThread(
    ReportWinCallbackInternal callback,
    const std::optional<GURL>& report_url,
    base::flat_map<std::string, GURL> ad_beacon_map,
    base::flat_map<std::string, std::string> ad_macro_map,
    PrivateAggregationRequests pa_requests,
    base::TimeDelta reporting_latency,
    bool script_timed_out,
    std::vector<std::string> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  user_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(report_url),
                     std::move(ad_beacon_map), std::move(ad_macro_map),
                     std::move(pa_requests), reporting_latency,
                     script_timed_out, std::move(errors)));
}

void BidderWorklet::V8State::PostErrorBidCallbackToUserThread(
    GenerateBidCallbackInternal callback,
    base::TimeDelta bidding_latency,
    PrivateAggregationRequests non_kanon_pa_requests,
    RealTimeReportingContributions real_time_contributions,
    std::vector<std::string> error_msgs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  user_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback), std::vector<mojom::BidderWorkletBidPtr>(),
          /*bidding_signals_data_version=*/std::nullopt,
          /*debug_loss_report_url=*/std::nullopt,
          /*debug_win_report_url=*/std::nullopt,
          /*set_priority=*/std::nullopt,
          /*update_priority_signals_overrides=*/
          base::flat_map<std::string, mojom::PrioritySignalsDoublePtr>(),
          /*pa_requests=*/
          PrivateAggregationRequests(), std::move(non_kanon_pa_requests),
          std::move(real_time_contributions), bidding_latency,
          /*reject_reason=*/mojom::RejectReason::kNotAvailable,
          /*script_timed_out=*/false, std::move(error_msgs)));
}

void BidderWorklet::ResumeIfPaused() {
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

void BidderWorklet::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  DCHECK(!paused_);
  base::UmaHistogramCounts100000(
      "Ads.InterestGroup.Net.RequestUrlSizeBytes.BiddingScriptJS",
      script_source_url_.spec().size());
  code_download_start_ = base::TimeTicks::Now();
  worklet_loader_ = std::make_unique<WorkletLoader>(
      url_loader_factory_.get(),
      /*auction_network_events_handler=*/
      CreateNewAuctionNetworkEventsHandlerRemote(
          auction_network_events_handler_),
      script_source_url_, v8_helpers_, debug_ids_,
      WorkletLoader::AllowTrustedScoringSignalsCallback(),
      base::BindOnce(&BidderWorklet::OnScriptDownloaded,
                     base::Unretained(this)));

  if (wasm_helper_url_.has_value()) {
    base::UmaHistogramCounts100000(
        "Ads.InterestGroup.Net.RequestUrlSizeBytes.BiddingScriptWasm",
        wasm_helper_url_->spec().size());
    wasm_loader_ = std::make_unique<WorkletWasmLoader>(
        url_loader_factory_.get(),
        /*auction_network_events_handler=*/
        CreateNewAuctionNetworkEventsHandlerRemote(
            auction_network_events_handler_),
        wasm_helper_url_.value(), v8_helpers_, debug_ids_,
        base::BindOnce(&BidderWorklet::OnWasmDownloaded,
                       base::Unretained(this)));
  }
}

void BidderWorklet::OnScriptDownloaded(
    std::vector<WorkletLoader::Result> worklet_scripts,
    std::optional<std::string> error_msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  DCHECK_EQ(worklet_scripts.size(), v8_helpers_.size());
  js_fetch_latency_ = base::TimeTicks::Now() - code_download_start_;

  // Use `worklet_scripts[0]` for metrics and for the failure check. All the
  // results should be the same.
  base::UmaHistogramCounts10M(
      "Ads.InterestGroup.Net.ResponseSizeBytes.BiddingScriptJS",
      worklet_scripts[0].original_size_bytes());
  base::UmaHistogramTimes("Ads.InterestGroup.Net.DownloadTime.BiddingScriptJS",
                          worklet_scripts[0].download_time());
  worklet_loader_.reset();

  // On failure, close pipe and delete `this`, as it can't do anything without a
  // loaded script.
  if (!worklet_scripts[0].success()) {
    std::move(close_pipe_callback_)
        .Run(error_msg ? error_msg.value() : std::string());
    // `this` should be deleted at this point.
    return;
  }

  if (error_msg.has_value()) {
    load_code_error_msgs_.push_back(std::move(error_msg.value()));
  }

  for (size_t i = 0; i < v8_runners_.size(); ++i) {
    v8_runners_[i]->PostTask(
        FROM_HERE, base::BindOnce(&BidderWorklet::V8State::SetWorkletScript,
                                  base::Unretained(v8_state_[i].get()),
                                  std::move(worklet_scripts[i])));
  }

  MaybeRecordCodeWait();
  RunReadyTasks();
}

void BidderWorklet::OnWasmDownloaded(
    std::vector<WorkletWasmLoader::Result> worklet_scripts,
    std::optional<std::string> error_msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  DCHECK_EQ(worklet_scripts.size(), v8_helpers_.size());
  wasm_fetch_latency_ = base::TimeTicks::Now() - code_download_start_;

  // Use `worklet_scripts[0]` for metrics and for the failure check. All the
  // results should be the same.
  base::UmaHistogramCounts10M(
      "Ads.InterestGroup.Net.ResponseSizeBytes.BiddingScriptWasm",
      worklet_scripts[0].original_size_bytes());
  base::UmaHistogramTimes(
      "Ads.InterestGroup.Net.DownloadTime.BiddingScriptWasm",
      worklet_scripts[0].download_time());
  wasm_loader_.reset();

  // If the WASM helper is actually requested, delete `this` and inform the
  // browser process of the failure. ReportWin() calls would theoretically still
  // be allowed, but that adds a lot more complexity around BidderWorklet reuse.
  if (!worklet_scripts[0].success()) {
    std::move(close_pipe_callback_)
        .Run(error_msg ? error_msg.value() : std::string());
    // `this` should be deleted at this point.
    return;
  }

  if (error_msg.has_value()) {
    load_code_error_msgs_.push_back(std::move(error_msg.value()));
  }

  for (size_t i = 0; i < v8_runners_.size(); ++i) {
    v8_runners_[i]->PostTask(
        FROM_HERE, base::BindOnce(&BidderWorklet::V8State::SetWasmHelper,
                                  base::Unretained(v8_state_[i].get()),
                                  std::move(worklet_scripts[i])));
  }

  MaybeRecordCodeWait();
  RunReadyTasks();
}

void BidderWorklet::MaybeRecordCodeWait() {
  if (!IsCodeReady()) {
    return;
  }

  base::TimeTicks now = base::TimeTicks::Now();
  for (auto& task : generate_bid_tasks_) {
    task.wait_code = now - task.trace_wait_deps_start;
  }

  for (auto& task : report_win_tasks_) {
    task.wait_code = now - task.trace_wait_deps_start;
  }
}

void BidderWorklet::RunReadyTasks() {
  // Run all GenerateBid() tasks that are ready. GenerateBidIfReady() does *not*
  // modify `generate_bid_tasks_` when invoked, so this is safe.
  for (auto generate_bid_task = generate_bid_tasks_.begin();
       generate_bid_task != generate_bid_tasks_.end(); ++generate_bid_task) {
    GenerateBidIfReady(generate_bid_task);
  }

  // While reportWin() doesn't use WASM, since we do load it, we wait for it in
  // order to ensure determinism if the load fails.
  if (!IsCodeReady()) {
    return;
  }

  // Run all ReportWin() tasks that are ready. RunReportWinIfReady() does *not*
  // modify `report_win_tasks_` when invoked, so this is safe.
  for (auto report_win_task = report_win_tasks_.begin();
       report_win_task != report_win_tasks_.end(); ++report_win_task) {
    RunReportWinIfReady(report_win_task);
  }
}

void BidderWorklet::OnTrustedBiddingSignalsDownloaded(
    GenerateBidTaskList::iterator task,
    scoped_refptr<TrustedSignals::Result> result,
    std::optional<std::string> error_msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  const TrustedSignals::Result::PerGroupData* per_group_data = nullptr;
  if (result) {
    per_group_data =
        result->GetPerGroupData(task->bidder_worklet_non_shared_params->name);
  }

  task->trusted_bidding_signals_fetch_failed = !result ? true : false;
  task->trusted_bidding_signals_result = std::move(result);
  task->trusted_bidding_signals_error_msg = std::move(error_msg);
  task->trusted_bidding_signals_request.reset();

  // Deleting `generate_bid_task` will destroy `generate_bid_client` and thus
  // abort this callback, so it's safe to use Unretained(this) and
  // `generate_bid_task` here.
  task->generate_bid_client->OnBiddingSignalsReceived(
      per_group_data && per_group_data->priority_vector
          ? *per_group_data->priority_vector
          : TrustedSignals::Result::PriorityVector(),
      /*trusted_signals_fetch_latency=*/base::TimeTicks::Now() -
          task->trace_wait_deps_start,
      /*update_if_older_than=*/
      per_group_data ? per_group_data->update_if_older_than : std::nullopt,
      base::BindOnce(&BidderWorklet::SignalsReceivedCallback,
                     base::Unretained(this), task));
}

void BidderWorklet::OnGenerateBidClientDestroyed(
    GenerateBidTaskList::iterator task) {
  // If the task hasn't received the signals called callback or the code hasn't
  // loaded, it hasn't posted a task to run off-thread, so can be safely
  // deleted, as everything else, including fetching trusted bidding signals,
  // can be safely cancelled.
  if (!IsReadyToGenerateBid(*task)) {
    // GenerateBidIfReady is never called so make sure to close out this trace
    // event.
    TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "wait_generate_bid_deps",
                                    task->trace_id);

    CleanUpBidTaskOnUserThread(task);
  } else {
    // Otherwise, there should be a pending V8 call. Try to cancel that, but if
    // it already started, it will just run and invoke the GenerateBidClient's
    // OnGenerateBidComplete() method, which will safely do nothing since the
    // pipe is now closed.
    DCHECK_NE(task->task_id, base::CancelableTaskTracker::kBadTaskId);
    cancelable_task_tracker_.TryCancel(task->task_id);
  }
}

void BidderWorklet::SignalsReceivedCallback(
    GenerateBidTaskList::iterator task) {
  DCHECK(!task->signals_received_callback_invoked);
  task->signals_received_callback_invoked = true;
  task->wait_trusted_signals =
      base::TimeTicks::Now() - task->trace_wait_deps_start;
  GenerateBidIfReady(task);
}

void BidderWorklet::HandleDirectFromSellerForGenerateBid(
    const std::optional<GURL>& direct_from_seller_per_buyer_signals,
    const std::optional<GURL>& direct_from_seller_auction_signals,
    GenerateBidTaskList::iterator task) {
  if (direct_from_seller_per_buyer_signals) {
    // We expect each parameter to be provided at most once between
    // BeginGenerateBid/FinishGenerateBid.  If we are already fetching this
    // kind of signals this is clearly the second time it was specified.
    DCHECK(!task->direct_from_seller_request_per_buyer_signals);

    // Deleting `task` will destroy
    // `direct_from_seller_request_per_buyer_signals` and thus abort this
    // callback, so it's safe to use Unretained(this) and `task`
    // here.
    task->direct_from_seller_request_per_buyer_signals =
        direct_from_seller_requester_per_buyer_signals_.LoadSignals(
            *url_loader_factory_, *direct_from_seller_per_buyer_signals,
            base::BindOnce(
                &BidderWorklet::
                    OnDirectFromSellerPerBuyerSignalsDownloadedGenerateBid,
                base::Unretained(this), task));
  }

  if (direct_from_seller_auction_signals) {
    // We expect each parameter to be provided at most once between
    // BeginGenerateBid/FinishGenerateBid.  If we are already fetching this
    // kind of signals this is clearly the second time it was specified.
    DCHECK(!task->direct_from_seller_request_auction_signals);
    // Deleting `task` will destroy
    // `direct_from_seller_request_auction_signals` and thus abort this
    // callback, so it's safe to use Unretained(this) and `task`
    // here.
    task->direct_from_seller_request_auction_signals =
        direct_from_seller_requester_auction_signals_.LoadSignals(
            *url_loader_factory_, *direct_from_seller_auction_signals,
            base::BindOnce(
                &BidderWorklet::
                    OnDirectFromSellerAuctionSignalsDownloadedGenerateBid,
                base::Unretained(this), task));
  }
}

void BidderWorklet::OnDirectFromSellerPerBuyerSignalsDownloadedGenerateBid(
    GenerateBidTaskList::iterator task,
    DirectFromSellerSignalsRequester::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  task->direct_from_seller_result_per_buyer_signals = std::move(result);
  task->direct_from_seller_request_per_buyer_signals.reset();

  // The two direct from seller signals metrics for tracing are combined since
  // they should be roughly the same.
  task->wait_direct_from_seller_signals =
      std::max(task->wait_direct_from_seller_signals,
               base::TimeTicks::Now() - task->trace_wait_deps_start);

  GenerateBidIfReady(task);
}

void BidderWorklet::OnDirectFromSellerAuctionSignalsDownloadedGenerateBid(
    GenerateBidTaskList::iterator task,
    DirectFromSellerSignalsRequester::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  task->direct_from_seller_result_auction_signals = std::move(result);
  task->direct_from_seller_request_auction_signals.reset();

  // The two direct from seller signals metrics for tracing are combined since
  // they should be roughly the same.
  task->wait_direct_from_seller_signals =
      std::max(task->wait_direct_from_seller_signals,
               base::TimeTicks::Now() - task->trace_wait_deps_start);

  GenerateBidIfReady(task);
}

bool BidderWorklet::IsReadyToGenerateBid(const GenerateBidTask& task) const {
  return task.signals_received_callback_invoked &&
         task.finalize_generate_bid_called &&
         !task.direct_from_seller_request_per_buyer_signals &&
         !task.direct_from_seller_request_auction_signals && IsCodeReady();
}

void BidderWorklet::GenerateBidIfReady(GenerateBidTaskList::iterator task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  if (!IsReadyToGenerateBid(*task)) {
    return;
  }

  // If there was a trusted signals request, it should have already completed
  // and been cleaned up before `signals_received_callback_invoked` was set to
  // true.
  DCHECK(!task->trusted_bidding_signals_request);

  TRACE_EVENT_NESTABLE_ASYNC_END1(
      "fledge", "wait_generate_bid_deps", task->trace_id, "data",
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
        if (!task->wait_promises.is_zero()) {
          dict.Add("wait_promises_ms", task->wait_promises.InMillisecondsF());
        }
      });
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "post_v8_task", task->trace_id);

  // Normally the PostTask below will eventually get `task` cleaned up once it
  // posts back to DeliverBidCallbackOnUserThread with its results, but that
  // won't happen if it gets cancelled. To deal with that, a ScopedClosureRunner
  // is passed to ask for `task` to get cleaned up in case the
  // V8State::GenerateBid closure gets destroyed without running.
  base::OnceClosure cleanup_generate_bid_task =
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(&BidderWorklet::CleanUpBidTaskOnUserThread,
                         weak_ptr_factory_.GetWeakPtr(), task));

  // In 'group-by-origin' mode, make the thread assignment sticky to
  // join_origin. This favors context reuse to save memory. The per-worklet
  // random salt is added to make sure certain origins won't always be grouped
  // together.
  int thread_index = 0;
  if (task->bidder_worklet_non_shared_params->execution_mode ==
      blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode) {
    size_t join_origin_hash = base::FastHash(
        join_origin_hash_salt_ + task->interest_group_join_origin.Serialize());
    thread_index = join_origin_hash % v8_helpers_.size();
  } else {
    thread_index = GetNextThreadIndex();
  }

  // Other than the `generate_bid_client` and `task_id` fields, no fields of
  // `task` are needed after this point, so can consume them instead of copying
  // them.
  //
  // Since IsReadyToGenerateBid() is true, the GenerateBidTask won't be deleted
  // on the main thread during this call, even if the GenerateBidClient pipe is
  // deleted by the caller (unless the BidderWorklet  itself is deleted).
  // Therefore, it's safe to post a callback with the `task`  iterator the v8
  // thread.
  task->generate_bid_start_time = base::TimeTicks::Now();
  task->task_id = cancelable_task_tracker_.PostTask(
      v8_runners_[thread_index].get(), FROM_HERE,
      base::BindOnce(
          &BidderWorklet::V8State::GenerateBid,
          base::Unretained(v8_state_[thread_index].get()),
          std::move(task->bidder_worklet_non_shared_params), task->kanon_mode,
          std::move(task->interest_group_join_origin),
          std::move(task->auction_signals_json),
          std::move(task->per_buyer_signals_json),
          std::move(task->direct_from_seller_result_per_buyer_signals),
          std::move(task->direct_from_seller_per_buyer_signals_header_ad_slot),
          std::move(task->direct_from_seller_result_auction_signals),
          std::move(task->direct_from_seller_auction_signals_header_ad_slot),
          std::move(task->per_buyer_timeout),
          std::move(task->expected_buyer_currency),
          std::move(task->browser_signal_seller_origin),
          std::move(task->browser_signal_top_level_seller_origin),
          std::move(task->browser_signal_recency),
          std::move(task->bidding_browser_signals), task->auction_start_time,
          std::move(task->requested_ad_size), task->multi_bid_limit,
          std::move(task->trusted_bidding_signals_result),
          task->trusted_bidding_signals_fetch_failed, task->trace_id,
          base::ScopedClosureRunner(std::move(cleanup_generate_bid_task)),
          base::BindOnce(&BidderWorklet::DeliverBidCallbackOnUserThread,
                         weak_ptr_factory_.GetWeakPtr(), task)));
}

void BidderWorklet::OnDirectFromSellerPerBuyerSignalsDownloadedReportWin(
    ReportWinTaskList::iterator task,
    DirectFromSellerSignalsRequester::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  task->direct_from_seller_result_per_buyer_signals = std::move(result);
  task->direct_from_seller_request_per_buyer_signals.reset();

  // The two direct from seller signals metrics for tracing are combined since
  // they should be roughly the same.
  task->wait_direct_from_seller_signals =
      std::max(task->wait_direct_from_seller_signals,
               base::TimeTicks::Now() - task->trace_wait_deps_start);

  RunReportWinIfReady(task);
}

void BidderWorklet::OnDirectFromSellerAuctionSignalsDownloadedReportWin(
    ReportWinTaskList::iterator task,
    DirectFromSellerSignalsRequester::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  task->direct_from_seller_result_auction_signals = std::move(result);
  task->direct_from_seller_request_auction_signals.reset();

  // The two direct from seller signals metrics for tracing are combined since
  // they should be roughly the same.
  task->wait_direct_from_seller_signals =
      std::max(task->wait_direct_from_seller_signals,
               base::TimeTicks::Now() - task->trace_wait_deps_start);

  RunReportWinIfReady(task);
}

bool BidderWorklet::IsReadyToReportWin(const ReportWinTask& task) const {
  return IsCodeReady() && !task.direct_from_seller_request_per_buyer_signals &&
         !task.direct_from_seller_request_auction_signals;
}

void BidderWorklet::RunReportWinIfReady(ReportWinTaskList::iterator task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  if (!IsReadyToReportWin(*task)) {
    return;
  }

  TRACE_EVENT_NESTABLE_ASYNC_END1(
      "fledge", "wait_report_win_deps", task->trace_id, "data",
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

  // Other than the callback field, no fields of `task` are needed after this
  // point, so can consume them instead of copying them.
  size_t thread_index = GetNextThreadIndex();
  cancelable_task_tracker_.PostTask(
      v8_runners_[thread_index].get(), FROM_HERE,
      base::BindOnce(
          &BidderWorklet::V8State::ReportWin,
          base::Unretained(v8_state_[thread_index].get()),
          std::move(task->is_for_additional_bid),
          std::move(task->interest_group_name_reporting_id),
          std::move(task->buyer_reporting_id),
          std::move(task->buyer_and_seller_reporting_id),
          std::move(task->selected_buyer_and_seller_reporting_id),
          std::move(task->auction_signals_json),
          std::move(task->per_buyer_signals_json),
          std::move(task->direct_from_seller_result_per_buyer_signals),
          std::move(task->direct_from_seller_per_buyer_signals_header_ad_slot),
          std::move(task->direct_from_seller_result_auction_signals),
          std::move(task->direct_from_seller_auction_signals_header_ad_slot),
          std::move(task->seller_signals_json), std::move(task->kanon_mode),
          std::move(task->bid_is_kanon),
          std::move(task->browser_signal_render_url),
          std::move(task->browser_signal_bid),
          std::move(task->browser_signal_bid_currency),
          std::move(task->browser_signal_highest_scoring_other_bid),
          std::move(task->browser_signal_highest_scoring_other_bid_currency),
          std::move(task->browser_signal_made_highest_scoring_other_bid),
          std::move(task->browser_signal_ad_cost),
          std::move(task->browser_signal_modeling_signals),
          std::move(task->browser_signal_join_count),
          std::move(task->browser_signal_recency),
          std::move(task->browser_signal_seller_origin),
          std::move(task->browser_signal_top_level_seller_origin),
          std::move(task->browser_signal_reporting_timeout),
          std::move(task->bidding_signals_data_version), task->trace_id,
          base::BindOnce(&BidderWorklet::DeliverReportWinOnUserThread,
                         weak_ptr_factory_.GetWeakPtr(), task)));
}

void BidderWorklet::DeliverBidCallbackOnUserThread(
    GenerateBidTaskList::iterator task,
    std::vector<mojom::BidderWorkletBidPtr> bids,
    std::optional<uint32_t> bidding_signals_data_version,
    std::optional<GURL> debug_loss_report_url,
    std::optional<GURL> debug_win_report_url,
    std::optional<double> set_priority,
    base::flat_map<std::string, mojom::PrioritySignalsDoublePtr>
        update_priority_signals_overrides,
    PrivateAggregationRequests pa_requests,
    PrivateAggregationRequests non_kanon_pa_requests,
    RealTimeReportingContributions real_time_contributions,
    base::TimeDelta bidding_latency,
    mojom::RejectReason reject_reason,
    bool script_timed_out,
    std::vector<std::string> error_msgs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  // TODO(https://crbug.com): Remove once bug is identified and fixed.
  CHECK(!debug_loss_report_url || debug_loss_report_url->is_valid());
  CHECK(!debug_win_report_url || debug_win_report_url->is_valid());

  error_msgs.insert(error_msgs.end(), load_code_error_msgs_.begin(),
                    load_code_error_msgs_.end());
  if (task->trusted_bidding_signals_error_msg) {
    error_msgs.emplace_back(
        std::move(task->trusted_bidding_signals_error_msg).value());
  }
  task->generate_bid_client->OnGenerateBidComplete(
      std::move(bids), bidding_signals_data_version, debug_loss_report_url,
      debug_win_report_url, set_priority,
      std::move(update_priority_signals_overrides), std::move(pa_requests),
      std::move(non_kanon_pa_requests), std::move(real_time_contributions),
      mojom::BidderTimingMetrics::New(
          /*js_fetch_latency=*/js_fetch_latency_,
          /*wasm_fetch_latency=*/wasm_fetch_latency_,
          /*script_latency=*/bidding_latency,
          /*script_timed_out=*/script_timed_out),
      mojom::GenerateBidDependencyLatencies::New(
          /*code_ready_latency=*/NullOptIfZero(task->wait_code),
          /*config_promises_latency=*/NullOptIfZero(task->wait_promises),
          /*direct_from_seller_signals_latency=*/
          NullOptIfZero(task->wait_direct_from_seller_signals),
          /*trusted_bidding_signals_latency=*/
          NullOptIfZero(task->wait_trusted_signals),
          /*deps_wait_start_time=*/task->trace_wait_deps_start,
          /*generate_bid_start_time=*/task->generate_bid_start_time,
          /*generate_bid_finish_time=*/base::TimeTicks::Now()),
      reject_reason, error_msgs);
  CleanUpBidTaskOnUserThread(task);
}

void BidderWorklet::CleanUpBidTaskOnUserThread(
    GenerateBidTaskList::iterator task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  // Disconnect the FinalizeGenerateBid pipe, if any, since that refers
  // to `task` (it generally will be closed already, but may not be if
  // GenerateBidClient disconnected before FinalizeGenerateBid was called).
  if (task->finalize_generate_bid_receiver_id.has_value()) {
    finalize_receiver_set_.Remove(*task->finalize_generate_bid_receiver_id);
  }
  generate_bid_tasks_.erase(task);
}

void BidderWorklet::DeliverReportWinOnUserThread(
    ReportWinTaskList::iterator task,
    std::optional<GURL> report_url,
    base::flat_map<std::string, GURL> ad_beacon_map,
    base::flat_map<std::string, std::string> ad_macro_map,
    PrivateAggregationRequests pa_requests,
    base::TimeDelta reporting_latency,
    bool script_timed_out,
    std::vector<std::string> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  errors.insert(errors.end(), load_code_error_msgs_.begin(),
                load_code_error_msgs_.end());
  std::move(task->callback)
      .Run(std::move(report_url), std::move(ad_beacon_map),
           std::move(ad_macro_map), std::move(pa_requests),
           mojom::BidderTimingMetrics::New(
               /*js_fetch_latency=*/js_fetch_latency_,
               /*wasm_fetch_latency=*/wasm_fetch_latency_,
               /*script_latency=*/reporting_latency,
               /*script_timed_out=*/script_timed_out),
           std::move(errors));
  report_win_tasks_.erase(task);
}

bool BidderWorklet::IsCodeReady() const {
  // If `paused_`, loading hasn't started yet. Otherwise, null loaders indicate
  // the worklet script has loaded successfully, and there's no WASM helper, or
  // it has also loaded successfully.
  return !paused_ && !worklet_loader_ && !wasm_loader_;
}

}  // namespace auction_worklet
