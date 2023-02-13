// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/bidder_worklet.h"

#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/types/optional_util.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/bidder_lazy_filler.h"
#include "content/services/auction_worklet/direct_from_seller_signals_requester.h"
#include "content/services/auction_worklet/for_debugging_only_bindings.h"
#include "content/services/auction_worklet/private_aggregation_bindings.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "content/services/auction_worklet/register_ad_beacon_bindings.h"
#include "content/services/auction_worklet/report_bindings.h"
#include "content/services/auction_worklet/set_bid_bindings.h"
#include "content/services/auction_worklet/set_priority_bindings.h"
#include "content/services/auction_worklet/set_priority_signals_override_bindings.h"
#include "content/services/auction_worklet/shared_storage_bindings.h"
#include "content/services/auction_worklet/trusted_signals.h"
#include "content/services/auction_worklet/trusted_signals_request_manager.h"
#include "content/services/auction_worklet/worklet_loader.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-template.h"
#include "v8/include/v8-wasm.h"

namespace auction_worklet {

namespace {

bool AppendJsonValueOrNull(AuctionV8Helper* const v8_helper,
                           v8::Local<v8::Context> context,
                           const std::string* maybe_json,
                           std::vector<v8::Local<v8::Value>>* args) {
  v8::Isolate* isolate = v8_helper->isolate();
  if (maybe_json) {
    if (!v8_helper->AppendJsonValue(context, *maybe_json, args))
      return false;
  } else {
    args->push_back(v8::Null(isolate));
  }
  return true;
}

// Converts a vector of blink::InterestGroup::Ads into a v8 object.
bool CreateAdVector(AuctionV8Helper* v8_helper,
                    v8::Local<v8::Context> context,
                    base::RepeatingCallback<bool(const GURL&)> is_ad_excluded,
                    const std::vector<blink::InterestGroup::Ad>& ads,
                    v8::Local<v8::Value>& out_value) {
  v8::Isolate* isolate = v8_helper->isolate();

  std::vector<v8::Local<v8::Value>> ads_vector;
  for (const auto& ad : ads) {
    if (is_ad_excluded.Run(ad.render_url)) {
      continue;
    }
    v8::Local<v8::Object> ad_object = v8::Object::New(isolate);
    gin::Dictionary ad_dict(isolate, ad_object);
    if (!ad_dict.Set("renderUrl", ad.render_url.spec()) ||
        (ad.metadata && !v8_helper->InsertJsonValue(context, "metadata",
                                                    *ad.metadata, ad_object))) {
      return false;
    }
    ads_vector.emplace_back(std::move(ad_object));
  }
  out_value = v8::Array::New(isolate, ads_vector.data(), ads_vector.size());
  return true;
}

}  // namespace

BidderWorklet::BidderWorklet(
    scoped_refptr<AuctionV8Helper> v8_helper,
    mojo::PendingRemote<mojom::AuctionSharedStorageHost>
        shared_storage_host_remote,
    bool pause_for_debugger_on_start,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_url_loader_factory,
    const GURL& script_source_url,
    const absl::optional<GURL>& wasm_helper_url,
    const absl::optional<GURL>& trusted_bidding_signals_url,
    const url::Origin& top_window_origin,
    mojom::AuctionWorkletPermissionsPolicyStatePtr permissions_policy_state,
    absl::optional<uint16_t> experiment_group_id)
    : v8_runner_(v8_helper->v8_runner()),
      v8_helper_(v8_helper),
      debug_id_(
          base::MakeRefCounted<AuctionV8Helper::DebugId>(v8_helper.get())),
      url_loader_factory_(std::move(pending_url_loader_factory)),
      script_source_url_(script_source_url),
      wasm_helper_url_(wasm_helper_url),
      trusted_signals_request_manager_(
          trusted_bidding_signals_url
              ? std::make_unique<TrustedSignalsRequestManager>(
                    TrustedSignalsRequestManager::Type::kBiddingSignals,
                    url_loader_factory_.get(),
                    /*automatically_send_requests=*/false,
                    top_window_origin,
                    *trusted_bidding_signals_url,
                    experiment_group_id,
                    v8_helper_.get())
              : nullptr),
      top_window_origin_(top_window_origin),
      v8_state_(nullptr, base::OnTaskRunnerDeleter(v8_runner_)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  v8_state_ = std::unique_ptr<V8State, base::OnTaskRunnerDeleter>(
      new V8State(v8_helper, debug_id_, std::move(shared_storage_host_remote),
                  script_source_url_, top_window_origin_,
                  std::move(permissions_policy_state), wasm_helper_url_,
                  trusted_bidding_signals_url, weak_ptr_factory_.GetWeakPtr()),
      base::OnTaskRunnerDeleter(v8_runner_));

  paused_ = pause_for_debugger_on_start;
  if (!paused_)
    Start();
}

BidderWorklet::~BidderWorklet() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  debug_id_->AbortDebuggerPauses();
}

int BidderWorklet::context_group_id_for_testing() const {
  return debug_id_->context_group_id();
}

// static
bool BidderWorklet::IsKAnon(
    const mojom::BidderWorkletNonSharedParams* bidder_worklet_non_shared_params,
    const std::string& key) {
  auto it = bidder_worklet_non_shared_params->kanon_keys.find(
      mojom::KAnonKey::New(key));
  return it != bidder_worklet_non_shared_params->kanon_keys.end() && it->second;
}

// static
bool BidderWorklet::IsKAnon(
    const mojom::BidderWorkletNonSharedParams* bidder_worklet_non_shared_params,
    const GURL& script_source_url,
    const mojom::BidderWorkletBidPtr& bid) {
  if (!bid)
    return true;
  if (!BidderWorklet::IsKAnon(
          bidder_worklet_non_shared_params,
          blink::KAnonKeyForAdBid(url::Origin::Create(script_source_url),
                                  script_source_url, bid->render_url))) {
    return false;
  }
  if (bid->ad_components.has_value()) {
    for (const auto& component : bid->ad_components.value()) {
      if (!BidderWorklet::IsKAnon(
              bidder_worklet_non_shared_params,
              blink::KAnonKeyForAdComponentBid(component))) {
        return false;
      }
    }
  }
  return true;
}

void BidderWorklet::BeginGenerateBid(
    mojom::BidderWorkletNonSharedParamsPtr bidder_worklet_non_shared_params,
    mojom::KAnonymityBidMode kanon_mode,
    const url::Origin& interest_group_join_origin,
    const absl::optional<GURL>& direct_from_seller_per_buyer_signals,
    const absl::optional<GURL>& direct_from_seller_auction_signals,
    const url::Origin& browser_signal_seller_origin,
    const absl::optional<url::Origin>& browser_signal_top_level_seller_origin,
    mojom::BiddingBrowserSignalsPtr bidding_browser_signals,
    base::Time auction_start_time,
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
  generate_bid_task->bidding_browser_signals =
      std::move(bidding_browser_signals);
  generate_bid_task->auction_start_time = auction_start_time;
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
    generate_bid_task->trusted_bidding_signals_request =
        trusted_signals_request_manager_->RequestBiddingSignals(
            generate_bid_task->bidder_worklet_non_shared_params->name,
            trusted_bidding_signals_keys,
            base::BindOnce(&BidderWorklet::OnTrustedBiddingSignalsDownloaded,
                           base::Unretained(this), generate_bid_task));
    return;
  }

  // Deleting `generate_bid_task` will destroy `generate_bid_client` and thus
  // abort this callback, so it's safe to use Unretained(this) and
  // `generate_bid_task` here.
  generate_bid_task->generate_bid_client->OnBiddingSignalsReceived(
      /*priority_vector=*/{},
      /*trusted_signals_fetch_duration=*/base::TimeDelta(),
      base::BindOnce(&BidderWorklet::SignalsReceivedCallback,
                     base::Unretained(this), generate_bid_task));
}

void BidderWorklet::SendPendingSignalsRequests() {
  if (trusted_signals_request_manager_)
    trusted_signals_request_manager_->StartBatchedTrustedSignalsRequest();
}

void BidderWorklet::ReportWin(
    const std::string& interest_group_name,
    const absl::optional<std::string>& auction_signals_json,
    const absl::optional<std::string>& per_buyer_signals_json,
    const absl::optional<GURL>& direct_from_seller_per_buyer_signals,
    const absl::optional<GURL>& direct_from_seller_auction_signals,
    const std::string& seller_signals_json,
    const GURL& browser_signal_render_url,
    double browser_signal_bid,
    double browser_signal_highest_scoring_other_bid,
    bool browser_signal_made_highest_scoring_other_bid,
    const url::Origin& browser_signal_seller_origin,
    const absl::optional<url::Origin>& browser_signal_top_level_seller_origin,
    uint32_t bidding_signals_data_version,
    bool has_bidding_signals_data_version,
    uint64_t trace_id,
    ReportWinCallback report_win_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  report_win_tasks_.emplace_front();
  auto report_win_task = report_win_tasks_.begin();
  report_win_task->interest_group_name = interest_group_name;
  report_win_task->auction_signals_json = auction_signals_json;
  report_win_task->per_buyer_signals_json = per_buyer_signals_json;
  report_win_task->seller_signals_json = seller_signals_json;
  report_win_task->browser_signal_render_url = browser_signal_render_url;
  report_win_task->browser_signal_bid = browser_signal_bid;
  report_win_task->browser_signal_highest_scoring_other_bid =
      browser_signal_highest_scoring_other_bid;
  report_win_task->browser_signal_made_highest_scoring_other_bid =
      browser_signal_made_highest_scoring_other_bid;
  report_win_task->browser_signal_seller_origin = browser_signal_seller_origin;
  report_win_task->browser_signal_top_level_seller_origin =
      browser_signal_top_level_seller_origin;
  if (has_bidding_signals_data_version)
    report_win_task->bidding_signals_data_version =
        bidding_signals_data_version;
  report_win_task->callback = std::move(report_win_callback);
  report_win_task->trace_id = trace_id;

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

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "wait_report_win_deps", trace_id);
  RunReportWinIfReady(report_win_task);
}

void BidderWorklet::ConnectDevToolsAgent(
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  v8_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&V8State::ConnectDevToolsAgent,
                     base::Unretained(v8_state_.get()), std::move(agent)));
}

void BidderWorklet::FinishGenerateBid(
    const absl::optional<std::string>& auction_signals_json,
    const absl::optional<std::string>& per_buyer_signals_json,
    const absl::optional<base::TimeDelta> per_buyer_timeout,
    const absl::optional<GURL>& direct_from_seller_per_buyer_signals,
    const absl::optional<GURL>& direct_from_seller_auction_signals) {
  GenerateBidTaskList::iterator task = finalize_receiver_set_.current_context();
  task->auction_signals_json = auction_signals_json;
  task->per_buyer_signals_json = per_buyer_signals_json;
  task->per_buyer_timeout = per_buyer_timeout;
  task->finalize_generate_bid_called = true;
  HandleDirectFromSellerForGenerateBid(direct_from_seller_per_buyer_signals,
                                       direct_from_seller_auction_signals,
                                       task);

  finalize_receiver_set_.Remove(*task->finalize_generate_bid_receiver_id);
  task->finalize_generate_bid_receiver_id = absl::nullopt;
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
    const absl::optional<GURL>& wasm_helper_url,
    const absl::optional<GURL>& trusted_bidding_signals_url,
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
      trusted_bidding_signals_url_(trusted_bidding_signals_url) {
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
    mojom::BidderWorkletBidPtr bid,
    absl::optional<uint32_t> bidding_signals_data_version,
    absl::optional<GURL> debug_loss_report_url,
    absl::optional<GURL> debug_win_report_url,
    absl::optional<double> set_priority,
    base::flat_map<std::string, mojom::PrioritySignalsDoublePtr>
        update_priority_signals_overrides,
    PrivateAggregationRequests pa_requests,
    std::vector<std::string> error_msgs)
    : context_recycler_for_rerun(std::move(context_recycler_for_rerun)),
      bid(std::move(bid)),
      bidding_signals_data_version(std::move(bidding_signals_data_version)),
      debug_loss_report_url(std::move(debug_loss_report_url)),
      debug_win_report_url(std::move(debug_win_report_url)),
      set_priority(std::move(set_priority)),
      update_priority_signals_overrides(
          std::move(update_priority_signals_overrides)),
      pa_requests(std::move(pa_requests)),
      error_msgs(std::move(error_msgs)) {}

BidderWorklet::V8State::SingleGenerateBidResult::SingleGenerateBidResult(
    SingleGenerateBidResult&&) = default;
BidderWorklet::V8State::SingleGenerateBidResult::~SingleGenerateBidResult() =
    default;
BidderWorklet::V8State::SingleGenerateBidResult&
BidderWorklet::V8State::SingleGenerateBidResult::operator=(
    SingleGenerateBidResult&&) = default;

void BidderWorklet::V8State::ReportWin(
    const std::string& interest_group_name,
    const absl::optional<std::string>& auction_signals_json,
    const absl::optional<std::string>& per_buyer_signals_json,
    DirectFromSellerSignalsRequester::Result
        direct_from_seller_result_per_buyer_signals,
    DirectFromSellerSignalsRequester::Result
        direct_from_seller_result_auction_signals,
    const std::string& seller_signals_json,
    const GURL& browser_signal_render_url,
    double browser_signal_bid,
    double browser_signal_highest_scoring_other_bid,
    bool browser_signal_made_highest_scoring_other_bid,
    const url::Origin& browser_signal_seller_origin,
    const absl::optional<url::Origin>& browser_signal_top_level_seller_origin,
    const absl::optional<uint32_t>& bidding_signals_data_version,
    uint64_t trace_id,
    ReportWinCallbackInternal callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "post_v8_task", trace_id);

  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper_.get());
  v8::Isolate* isolate = v8_helper_->isolate();

  // Short lived context, to avoid leaking data at global scope between either
  // repeated calls to this worklet, or to calls to any other worklet.
  ContextRecycler context_recycler(v8_helper_.get());
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

  ContextRecyclerScope context_recycler_scope(context_recycler);
  v8::Local<v8::Context> context = context_recycler_scope.GetContext();

  std::vector<v8::Local<v8::Value>> args;
  if (!AppendJsonValueOrNull(v8_helper_.get(), context,
                             base::OptionalToPtr(auction_signals_json),
                             &args) ||
      !AppendJsonValueOrNull(v8_helper_.get(), context,
                             base::OptionalToPtr(per_buyer_signals_json),
                             &args) ||
      !v8_helper_->AppendJsonValue(context, seller_signals_json, &args)) {
    PostReportWinCallbackToUserThread(std::move(callback),
                                      /*report_url=*/absl::nullopt,
                                      /*ad_beacon_map=*/{}, /*pa_requests=*/{},
                                      /*errors=*/std::vector<std::string>());
    return;
  }

  v8::Local<v8::Object> browser_signals = v8::Object::New(isolate);
  gin::Dictionary browser_signals_dict(isolate, browser_signals);
  if (!browser_signals_dict.Set("topWindowHostname",
                                top_window_origin_.host()) ||
      !browser_signals_dict.Set(
          "interestGroupOwner",
          url::Origin::Create(script_source_url_).Serialize()) ||
      !browser_signals_dict.Set("interestGroupName", interest_group_name) ||
      !browser_signals_dict.Set("renderUrl",
                                browser_signal_render_url.spec()) ||
      !browser_signals_dict.Set("bid", browser_signal_bid) ||
      !browser_signals_dict.Set("highestScoringOtherBid",
                                browser_signal_highest_scoring_other_bid) ||
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
                                 bidding_signals_data_version.value()))) {
    PostReportWinCallbackToUserThread(std::move(callback),
                                      /*report_url=*/absl::nullopt,
                                      /*ad_beacon_map=*/{}, /*pa_requests=*/{},
                                      /*errors=*/std::vector<std::string>());
    return;
  }
  args.push_back(browser_signals);

  std::vector<std::string> errors_out;
  v8::Local<v8::Object> direct_from_seller_signals = v8::Object::New(isolate);
  gin::Dictionary direct_from_seller_signals_dict(isolate,
                                                  direct_from_seller_signals);
  v8::Local<v8::Value> per_buyer_signals =
      direct_from_seller_result_per_buyer_signals.GetSignals(
          *v8_helper_, context, errors_out);
  v8::Local<v8::Value> auction_signals =
      direct_from_seller_result_auction_signals.GetSignals(*v8_helper_, context,
                                                           errors_out);
  if (!direct_from_seller_signals_dict.Set("perBuyerSignals",
                                           per_buyer_signals) ||
      !direct_from_seller_signals_dict.Set("auctionSignals", auction_signals)) {
    PostReportWinCallbackToUserThread(std::move(callback),
                                      /*report_url=*/absl::nullopt,
                                      /*ad_beacon_map=*/{}, /*pa_requests=*/{},
                                      /*errors=*/std::move(errors_out));
    return;
  }
  args.push_back(direct_from_seller_signals);

  // An empty return value indicates an exception was thrown. Any other return
  // value indicates no exception.
  v8_helper_->MaybeTriggerInstrumentationBreakpoint(
      *debug_id_, "beforeBidderWorkletReportingStart");

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "report_win", trace_id);
  bool script_failed =
      v8_helper_
          ->RunScript(context, worklet_script_.Get(isolate), debug_id_.get(),
                      AuctionV8Helper::ExecMode::kTopLevelAndFunction,
                      "reportWin", args, /*script_timeout=*/absl::nullopt,
                      errors_out)
          .IsEmpty();
  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "report_win", trace_id);

  if (script_failed) {
    // Keep Private Aggregation API requests since `reportWin()` might use it to
    // detect script timeout or failures.
    PostReportWinCallbackToUserThread(
        std::move(callback), /*report_url=*/absl::nullopt,
        /*ad_beacon_map=*/{},
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        std::move(errors_out));
    return;
  }

  // This covers both the case where a report URL was provided, and the case one
  // was not.
  PostReportWinCallbackToUserThread(
      std::move(callback), context_recycler.report_bindings()->report_url(),
      context_recycler.register_ad_beacon_bindings()->TakeAdBeaconMap(),
      context_recycler.private_aggregation_bindings()
          ->TakePrivateAggregationRequests(),
      std::move(errors_out));
}

void BidderWorklet::V8State::GenerateBid(
    mojom::BidderWorkletNonSharedParamsPtr bidder_worklet_non_shared_params,
    mojom::KAnonymityBidMode kanon_mode,
    const url::Origin& interest_group_join_origin,
    const absl::optional<std::string>& auction_signals_json,
    const absl::optional<std::string>& per_buyer_signals_json,
    DirectFromSellerSignalsRequester::Result
        direct_from_seller_result_per_buyer_signals,
    DirectFromSellerSignalsRequester::Result
        direct_from_seller_result_auction_signals,
    const absl::optional<base::TimeDelta> per_buyer_timeout,
    const url::Origin& browser_signal_seller_origin,
    const absl::optional<url::Origin>& browser_signal_top_level_seller_origin,
    mojom::BiddingBrowserSignalsPtr bidding_browser_signals,
    base::Time auction_start_time,
    scoped_refptr<TrustedSignals::Result> trusted_bidding_signals_result,
    uint64_t trace_id,
    base::ScopedClosureRunner cleanup_generate_bid_task,
    GenerateBidCallbackInternal callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "post_v8_task", trace_id);

  // Don't need to run `cleanup_generate_bid_task` if this method is invoked;
  // it's bound to the closure to clean things up if this method got cancelled.
  cleanup_generate_bid_task.ReplaceClosure(base::OnceClosure());

  base::TimeTicks bidding_start = base::TimeTicks::Now();
  absl::optional<SingleGenerateBidResult> result = GenerateSingleBid(
      *bidder_worklet_non_shared_params, interest_group_join_origin,
      base::OptionalToPtr(auction_signals_json),
      base::OptionalToPtr(per_buyer_signals_json),
      direct_from_seller_result_per_buyer_signals,
      direct_from_seller_result_auction_signals, per_buyer_timeout,
      browser_signal_seller_origin,
      base::OptionalToPtr(browser_signal_top_level_seller_origin),
      bidding_browser_signals, auction_start_time,
      trusted_bidding_signals_result, trace_id,
      /*context_recycler_for_rerun=*/nullptr,
      /*restrict_to_kanon_ads=*/false);
  if (!result.has_value()) {
    PostErrorBidCallbackToUserThread(
        std::move(callback),
        /*bidding_duration=*/base::TimeTicks::Now() - bidding_start);
    return;
  }

  mojom::BidderWorkletBidPtr bid = std::move(result->bid);
  mojom::BidderWorkletKAnonEnforcedBidPtr kanon_bid;

  // No need for `kanon_bid` if not doing anything with k-anon, or if bidding
  // fails w/o the restriction.  This assumes it follows it won't succeed with
  // k-anon restriction, but if we don't we will have to re-run every rejected
  // bid, which is unreasonable.
  if (kanon_mode != mojom::KAnonymityBidMode::kNone && bid) {
    if (IsKAnon(bidder_worklet_non_shared_params.get(), script_source_url_,
                bid)) {
      // Result is already k-anon so it's the same for both runs.
      kanon_bid =
          mojom::BidderWorkletKAnonEnforcedBid::NewSameAsNonEnforced(nullptr);
    } else {
      // Main run got a non-k-anon result, and we care about k-anonymity. Re-run
      // the bidder with non-k-anon ads hidden.
      absl::optional<SingleGenerateBidResult> restricted_result =
          GenerateSingleBid(
              *bidder_worklet_non_shared_params.get(),
              interest_group_join_origin,
              base::OptionalToPtr(auction_signals_json),
              base::OptionalToPtr(per_buyer_signals_json),
              direct_from_seller_result_per_buyer_signals,
              direct_from_seller_result_auction_signals, per_buyer_timeout,
              browser_signal_seller_origin,
              base::OptionalToPtr(browser_signal_top_level_seller_origin),
              bidding_browser_signals, auction_start_time,
              trusted_bidding_signals_result, trace_id,
              std::move(result->context_recycler_for_rerun),
              /*restrict_to_kanon_ads=*/true);
      if (restricted_result.has_value() && restricted_result->bid) {
        kanon_bid = mojom::BidderWorkletKAnonEnforcedBid::NewBid(
            std::move(restricted_result->bid));
      }

      if (kanon_mode == mojom::KAnonymityBidMode::kEnforce) {
        // We are enforcing the k-anonymity, so the restricted result is the one
        // to use for reporting, etc., and needs to succeed.
        if (!restricted_result.has_value()) {
          PostErrorBidCallbackToUserThread(
              std::move(callback),
              /*bidding_duration=*/base::TimeTicks::Now() - bidding_start);
          return;
        }
        result = std::move(restricted_result);
      } else {
        DCHECK_EQ(kanon_mode, mojom::KAnonymityBidMode::kSimulate);
        // Here, `result` is already what we want for reporting, etc., so
        // nothing actually to do in this case.
      }
    }
  }

  user_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback), std::move(bid), std::move(kanon_bid),
          std::move(result->bidding_signals_data_version),
          std::move(result->debug_loss_report_url),
          std::move(result->debug_win_report_url),
          std::move(result->set_priority),
          std::move(result->update_priority_signals_overrides),
          std::move(result->pa_requests),
          /*bidding_duration=*/base::TimeTicks::Now() - bidding_start,
          std::move(result->error_msgs)));
}

absl::optional<BidderWorklet::V8State::SingleGenerateBidResult>
BidderWorklet::V8State::GenerateSingleBid(
    const mojom::BidderWorkletNonSharedParams& bidder_worklet_non_shared_params,
    const url::Origin& interest_group_join_origin,
    const std::string* auction_signals_json,
    const std::string* per_buyer_signals_json,
    const DirectFromSellerSignalsRequester::Result&
        direct_from_seller_result_per_buyer_signals,
    const DirectFromSellerSignalsRequester::Result&
        direct_from_seller_result_auction_signals,
    const absl::optional<base::TimeDelta> per_buyer_timeout,
    const url::Origin& browser_signal_seller_origin,
    const url::Origin* browser_signal_top_level_seller_origin,
    const mojom::BiddingBrowserSignalsPtr& bidding_browser_signals,
    base::Time auction_start_time,
    const scoped_refptr<TrustedSignals::Result>& trusted_bidding_signals_result,
    uint64_t trace_id,
    std::unique_ptr<ContextRecycler> context_recycler_for_rerun,
    bool restrict_to_kanon_ads) {
  // Can't make a bid without any ads, or if we aren't permitted to spend any
  // time on it.
  if (!bidder_worklet_non_shared_params.ads ||
      (per_buyer_timeout.has_value() && per_buyer_timeout.value().is_zero())) {
    return absl::nullopt;
  }

  if (context_recycler_for_rerun) {
    DCHECK(restrict_to_kanon_ads);
  }

  base::TimeTicks start = base::TimeTicks::Now();

  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper_.get());
  v8::Isolate* isolate = v8_helper_->isolate();
  ContextRecycler* context_recycler = nullptr;
  std::unique_ptr<ContextRecycler> fresh_context_recycler;

  bool reused_context = false;
  // See if we can reuse an existing context in group-by-origin mode.
  bool group_by_origin_mode =
      (bidder_worklet_non_shared_params.execution_mode ==
       blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode);
  if (group_by_origin_mode && context_recycler_for_origin_group_mode_ &&
      join_origin_for_origin_group_mode_ == interest_group_join_origin) {
    context_recycler = context_recycler_for_origin_group_mode_.get();
    reused_context = true;
  }
  base::UmaHistogramBoolean("Ads.InterestGroup.Auction.ContextReused",
                            reused_context);

  // See if we can reuse a context for k-anon re-run. The group-by-origin mode
  // would do that, too, so this is only a fallback for when that's not on.
  if (!context_recycler && context_recycler_for_rerun) {
    context_recycler = context_recycler_for_rerun.get();
    reused_context = true;
  }

  // No recycled context, make a fresh one.
  if (!context_recycler) {
    fresh_context_recycler =
        std::make_unique<ContextRecycler>(v8_helper_.get());
    fresh_context_recycler->AddForDebuggingOnlyBindings();
    fresh_context_recycler->AddPrivateAggregationBindings(
        permissions_policy_state_->private_aggregation_allowed);

    if (base::FeatureList::IsEnabled(blink::features::kSharedStorageAPI)) {
      fresh_context_recycler->AddSharedStorageBindings(
          shared_storage_host_remote_.is_bound()
              ? shared_storage_host_remote_.get()
              : nullptr,
          permissions_policy_state_->shared_storage_allowed);
    }

    fresh_context_recycler->AddSetBidBindings();
    fresh_context_recycler->AddSetPriorityBindings();
    fresh_context_recycler->AddSetPrioritySignalsOverrideBindings();
    fresh_context_recycler->AddInterestGroupLazyFiller();
    fresh_context_recycler->AddBiddingBrowserSignalsLazyFiller();
    context_recycler = fresh_context_recycler.get();
  }

  // Save a reusable context.
  if (group_by_origin_mode && fresh_context_recycler) {
    context_recycler_for_origin_group_mode_ = std::move(fresh_context_recycler);
    join_origin_for_origin_group_mode_ = interest_group_join_origin;
  }

  base::RepeatingCallback<bool(const GURL&)> should_exclude_ad_due_to_kanon =
      base::BindRepeating(
          [](bool restrict_to_kanon_ads,
             const mojom::BidderWorkletNonSharedParams* params,
             const url::Origin* owner, const GURL* bidding_url,
             const GURL& ad_url) {
            return restrict_to_kanon_ads &&
                   !BidderWorklet::IsKAnon(
                       params,
                       blink::KAnonKeyForAdBid(*owner, *bidding_url, ad_url));
          },
          restrict_to_kanon_ads, &bidder_worklet_non_shared_params, &owner_,
          &script_source_url_);

  base::RepeatingCallback<bool(const GURL&)>
      should_exclude_component_ad_due_to_kanon = base::BindRepeating(
          [](bool restrict_to_kanon_ads,
             const mojom::BidderWorkletNonSharedParams* params,
             const GURL& ad_url) {
            return restrict_to_kanon_ads &&
                   !BidderWorklet::IsKAnon(
                       params, blink::KAnonKeyForAdComponentBid(ad_url));
          },
          restrict_to_kanon_ads, &bidder_worklet_non_shared_params);

  ContextRecyclerScope context_recycler_scope(*context_recycler);
  v8::Local<v8::Context> context = context_recycler_scope.GetContext();
  context_recycler->set_bid_bindings()->ReInitialize(
      start, browser_signal_top_level_seller_origin != nullptr,
      &bidder_worklet_non_shared_params, should_exclude_ad_due_to_kanon,
      should_exclude_component_ad_due_to_kanon);

  std::vector<v8::Local<v8::Value>> args;
  v8::Local<v8::Object> interest_group_object = v8::Object::New(isolate);
  gin::Dictionary interest_group_dict(isolate, interest_group_object);
  if (!interest_group_dict.Set("owner", owner_.Serialize()) ||
      !interest_group_dict.Set("name", bidder_worklet_non_shared_params.name) ||
      !interest_group_dict.Set("useBiddingSignalsPrioritization",
                               bidder_worklet_non_shared_params
                                   .enable_bidding_signals_prioritization) ||
      !interest_group_dict.Set("biddingLogicUrl", script_source_url_.spec()) ||
      (wasm_helper_url_ &&
       !interest_group_dict.Set("biddingWasmHelperUrl",
                                wasm_helper_url_->spec())) ||
      (bidder_worklet_non_shared_params.daily_update_url &&
       !interest_group_dict.Set(
           "dailyUpdateUrl",
           bidder_worklet_non_shared_params.daily_update_url->spec())) ||
      (trusted_bidding_signals_url_ &&
       !interest_group_dict.Set("trustedBiddingSignalsUrl",
                                trusted_bidding_signals_url_->spec()))) {
    return absl::nullopt;
  }

  context_recycler->interest_group_lazy_filler()->ReInitialize(
      &bidder_worklet_non_shared_params);
  if (!context_recycler->interest_group_lazy_filler()->FillInObject(
          interest_group_object)) {
    return absl::nullopt;
  }

  v8::Local<v8::Value> ads;
  if (!CreateAdVector(v8_helper_.get(), context, should_exclude_ad_due_to_kanon,
                      *bidder_worklet_non_shared_params.ads, ads) ||
      !v8_helper_->InsertValue("ads", std::move(ads), interest_group_object)) {
    return absl::nullopt;
  }

  if (bidder_worklet_non_shared_params.ad_components) {
    v8::Local<v8::Value> ad_components;
    if (!CreateAdVector(
            v8_helper_.get(), context, should_exclude_component_ad_due_to_kanon,
            *bidder_worklet_non_shared_params.ad_components, ad_components) ||
        !v8_helper_->InsertValue("adComponents", std::move(ad_components),
                                 interest_group_object)) {
      return absl::nullopt;
    }
  }

  args.push_back(std::move(interest_group_object));

  if (!AppendJsonValueOrNull(v8_helper_.get(), context, auction_signals_json,
                             &args) ||
      !AppendJsonValueOrNull(v8_helper_.get(), context, per_buyer_signals_json,
                             &args)) {
    return absl::nullopt;
  }

  v8::Local<v8::Value> trusted_signals;
  absl::optional<uint32_t> bidding_signals_data_version;
  if (!trusted_bidding_signals_result ||
      !bidder_worklet_non_shared_params.trusted_bidding_signals_keys ||
      bidder_worklet_non_shared_params.trusted_bidding_signals_keys->empty()) {
    trusted_signals = v8::Null(isolate);
  } else {
    trusted_signals = trusted_bidding_signals_result->GetBiddingSignals(
        v8_helper_.get(), context,
        *bidder_worklet_non_shared_params.trusted_bidding_signals_keys);
    bidding_signals_data_version =
        trusted_bidding_signals_result->GetDataVersion();
  }
  args.push_back(trusted_signals);

  v8::Local<v8::Object> browser_signals = v8::Object::New(isolate);
  gin::Dictionary browser_signals_dict(isolate, browser_signals);
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
      (bidding_signals_data_version.has_value() &&
       !browser_signals_dict.Set("dataVersion",
                                 bidding_signals_data_version.value()))) {
    return absl::nullopt;
  }

  if (wasm_helper_.success()) {
    v8::Local<v8::WasmModuleObject> module;
    v8::Maybe<bool> result = v8::Nothing<bool>();
    if (WorkletWasmLoader::MakeModule(wasm_helper_).ToLocal(&module)) {
      result = browser_signals->Set(
          context, gin::StringToV8(isolate, "wasmHelper"), module);
    }
    if (result.IsNothing() || !result.FromJust()) {
      return absl::nullopt;
    }
  }

  context_recycler->bidding_browser_signals_lazy_filler()->ReInitialize(
      bidding_browser_signals.get(), auction_start_time);
  if (!context_recycler->bidding_browser_signals_lazy_filler()->FillInObject(
          browser_signals)) {
    return absl::nullopt;
  }

  args.push_back(browser_signals);

  std::vector<std::string> errors_out;
  v8::Local<v8::Object> direct_from_seller_signals = v8::Object::New(isolate);
  gin::Dictionary direct_from_seller_signals_dict(isolate,
                                                  direct_from_seller_signals);
  v8::Local<v8::Value> per_buyer_signals =
      direct_from_seller_result_per_buyer_signals.GetSignals(
          *v8_helper_, context, errors_out);
  v8::Local<v8::Value> auction_signals =
      direct_from_seller_result_auction_signals.GetSignals(*v8_helper_, context,
                                                           errors_out);
  if (!direct_from_seller_signals_dict.Set("perBuyerSignals",
                                           per_buyer_signals) ||
      !direct_from_seller_signals_dict.Set("auctionSignals", auction_signals)) {
    return absl::nullopt;
  }
  args.push_back(direct_from_seller_signals);

  v8::Local<v8::Value> generate_bid_result;
  v8_helper_->MaybeTriggerInstrumentationBreakpoint(
      *debug_id_, "beforeBidderWorkletBiddingStart");

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "generate_bid", trace_id);
  bool got_return_value =
      v8_helper_
          ->RunScript(
              context, worklet_script_.Get(isolate), debug_id_.get(),
              reused_context ? AuctionV8Helper::ExecMode::kFunctionOnly
                             : AuctionV8Helper::ExecMode::kTopLevelAndFunction,
              "generateBid", args, std::move(per_buyer_timeout), errors_out)
          .ToLocal(&generate_bid_result);
  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "generate_bid", trace_id);
  base::UmaHistogramTimes("Ads.InterestGroup.Auction.GenerateBidTime",
                          base::TimeTicks::Now() - start);

  if (got_return_value) {
    context_recycler->set_bid_bindings()->SetBid(
        generate_bid_result,
        base::StrCat({script_source_url_.spec(), " generateBid() "}),
        errors_out);
  }

  if (!context_recycler->set_bid_bindings()->has_bid()) {
    // If no bid was returned (due to an error or just not choosing to bid), or
    // the method timed out and no intermediate result was given through
    // `setBid()`, return an error. Keep debug loss reports and Private
    // Aggregation API requests since `generateBid()` might use them to detect
    // script timeout or failures. Keep any set priority and set priority
    // overrides because an interest group may want to update them even when not
    // bidding. No need to return a ContextRecycler since this will not be
    // re-run.
    return absl::make_optional(SingleGenerateBidResult(
        std::unique_ptr<ContextRecycler>(), mojom::BidderWorkletBidPtr(),
        /*bidding_signals_data_version=*/absl::nullopt,
        context_recycler->for_debugging_only_bindings()->TakeLossReportUrl(),
        /*debug_win_report_url=*/absl::nullopt,
        context_recycler->set_priority_bindings()->set_priority(),
        context_recycler->set_priority_signals_override_bindings()
            ->TakeSetPrioritySignalsOverrides(),
        context_recycler->private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        std::move(errors_out)));
  }

  // If the context recycler wasn't saved based on `execution_mode`,
  // `fresh_context_recycler` is non-null here, and it will be provided to the
  // caller for potential re-use for k-anon re-run.
  return absl::make_optional(SingleGenerateBidResult(
      std::move(fresh_context_recycler),
      context_recycler->set_bid_bindings()->TakeBid(),
      bidding_signals_data_version,
      context_recycler->for_debugging_only_bindings()->TakeLossReportUrl(),
      context_recycler->for_debugging_only_bindings()->TakeWinReportUrl(),
      context_recycler->set_priority_bindings()->set_priority(),
      context_recycler->set_priority_signals_override_bindings()
          ->TakeSetPrioritySignalsOverrides(),
      context_recycler->private_aggregation_bindings()
          ->TakePrivateAggregationRequests(),
      std::move(errors_out)));
}

void BidderWorklet::V8State::ConnectDevToolsAgent(
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  v8_helper_->ConnectDevToolsAgent(std::move(agent), user_thread_, *debug_id_);
}

BidderWorklet::V8State::~V8State() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
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
    const absl::optional<GURL>& report_url,
    base::flat_map<std::string, GURL> ad_beacon_map,
    PrivateAggregationRequests pa_requests,
    std::vector<std::string> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  user_thread_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(report_url),
                                std::move(ad_beacon_map),
                                std::move(pa_requests), std::move(errors)));
}

void BidderWorklet::V8State::PostErrorBidCallbackToUserThread(
    GenerateBidCallbackInternal callback,
    base::TimeDelta bidding_duration,
    std::vector<std::string> error_msgs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  user_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback), mojom::BidderWorkletBidPtr(),
          mojom::BidderWorkletKAnonEnforcedBidPtr(),
          /*bidding_signals_data_version=*/absl::nullopt,
          /*debug_loss_report_url=*/absl::nullopt,
          /*debug_win_report_url=*/absl::nullopt,
          /*set_priority=*/absl::nullopt,
          /*update_priority_signals_overrides=*/
          base::flat_map<std::string, mojom::PrioritySignalsDoublePtr>(),
          /*pa_requests=*/
          PrivateAggregationRequests(), bidding_duration,
          std::move(error_msgs)));
}

void BidderWorklet::ResumeIfPaused() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  if (!paused_)
    return;

  paused_ = false;
  Start();
}

void BidderWorklet::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  DCHECK(!paused_);

  base::UmaHistogramCounts100000(
      "Ads.InterestGroup.Net.RequestUrlSizeBytes.BiddingScriptJS",
      script_source_url_.spec().size());
  worklet_loader_ = std::make_unique<WorkletLoader>(
      url_loader_factory_.get(), script_source_url_, v8_helper_, debug_id_,
      base::BindOnce(&BidderWorklet::OnScriptDownloaded,
                     base::Unretained(this)));

  if (wasm_helper_url_.has_value()) {
    base::UmaHistogramCounts100000(
        "Ads.InterestGroup.Net.RequestUrlSizeBytes.BiddingScriptWasm",
        wasm_helper_url_->spec().size());
    wasm_loader_ = std::make_unique<WorkletWasmLoader>(
        url_loader_factory_.get(), wasm_helper_url_.value(), v8_helper_,
        debug_id_,
        base::BindOnce(&BidderWorklet::OnWasmDownloaded,
                       base::Unretained(this)));
  }
}

void BidderWorklet::OnScriptDownloaded(WorkletLoader::Result worklet_script,
                                       absl::optional<std::string> error_msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  base::UmaHistogramCounts10M(
      "Ads.InterestGroup.Net.ResponseSizeBytes.BiddingScriptJS",
      worklet_script.original_size_bytes());
  base::UmaHistogramTimes("Ads.InterestGroup.Net.DownloadTime.BiddingScriptJS",
                          worklet_script.download_time());
  worklet_loader_.reset();

  // On failure, close pipe and delete `this`, as it can't do anything without a
  // loaded script.
  if (!worklet_script.success()) {
    std::move(close_pipe_callback_)
        .Run(error_msg ? error_msg.value() : std::string());
    // `this` should be deleted at this point.
    return;
  }

  if (error_msg.has_value())
    load_code_error_msgs_.push_back(std::move(error_msg.value()));

  v8_runner_->PostTask(FROM_HERE,
                       base::BindOnce(&BidderWorklet::V8State::SetWorkletScript,
                                      base::Unretained(v8_state_.get()),
                                      std::move(worklet_script)));
  MaybeRecordCodeWait();
  RunReadyTasks();
}

void BidderWorklet::OnWasmDownloaded(WorkletWasmLoader::Result wasm_helper,
                                     absl::optional<std::string> error_msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  base::UmaHistogramCounts10M(
      "Ads.InterestGroup.Net.ResponseSizeBytes.BiddingScriptWasm",
      wasm_helper.original_size_bytes());
  base::UmaHistogramTimes(
      "Ads.InterestGroup.Net.DownloadTime.BiddingScriptWasm",
      wasm_helper.download_time());
  wasm_loader_.reset();

  // If the WASM helper is actually requested, delete `this` and inform the
  // browser process of the failure. ReportWin() calls would theoretically still
  // be allowed, but that adds a lot more complexity around BidderWorklet reuse.
  if (!wasm_helper.success()) {
    std::move(close_pipe_callback_)
        .Run(error_msg ? error_msg.value() : std::string());
    // `this` should be deleted at this point.
    return;
  }

  if (error_msg.has_value())
    load_code_error_msgs_.push_back(std::move(error_msg.value()));

  v8_runner_->PostTask(FROM_HERE,
                       base::BindOnce(&BidderWorklet::V8State::SetWasmHelper,
                                      base::Unretained(v8_state_.get()),
                                      std::move(wasm_helper)));
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
  if (!IsCodeReady())
    return;

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
    absl::optional<std::string> error_msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  const TrustedSignals::Result::PriorityVector* priority_vector = nullptr;
  if (result) {
    priority_vector =
        result->GetPriorityVector(task->bidder_worklet_non_shared_params->name);
  }

  task->trusted_bidding_signals_error_msg = std::move(error_msg);
  // Only hold onto `result` if it has information that needs to be passed to
  // generateBid().
  if (task->bidder_worklet_non_shared_params->trusted_bidding_signals_keys &&
      !task->bidder_worklet_non_shared_params->trusted_bidding_signals_keys
           ->empty()) {
    task->trusted_bidding_signals_result = std::move(result);
  }
  task->trusted_bidding_signals_request.reset();

  // Deleting `generate_bid_task` will destroy `generate_bid_client` and thus
  // abort this callback, so it's safe to use Unretained(this) and
  // `generate_bid_task` here.
  task->generate_bid_client->OnBiddingSignalsReceived(
      priority_vector ? *priority_vector
                      : TrustedSignals::Result::PriorityVector(),
      /*trusted_signals_fetch_duration=*/base::TimeTicks::Now() -
          task->trace_wait_deps_start,
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
    const absl::optional<GURL>& direct_from_seller_per_buyer_signals,
    const absl::optional<GURL>& direct_from_seller_auction_signals,
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
  if (!IsReadyToGenerateBid(*task))
    return;

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

  // Other than the `generate_bid_client` and `task_id` fields, no fields of
  // `task` are needed after this point, so can consume them instead of copying
  // them.
  //
  // Since IsReadyToGenerateBid() is true, the GenerateBidTask won't be deleted
  // on the main thread during this call, even if the GenerateBidClient pipe is
  // deleted by the caller (unless the BidderWorklet  itself is deleted).
  // Therefore, it's safe to post a callback with the `task`  iterator the v8
  // thread.
  task->task_id = cancelable_task_tracker_.PostTask(
      v8_runner_.get(), FROM_HERE,
      base::BindOnce(
          &BidderWorklet::V8State::GenerateBid,
          base::Unretained(v8_state_.get()),
          std::move(task->bidder_worklet_non_shared_params), task->kanon_mode,
          std::move(task->interest_group_join_origin),
          std::move(task->auction_signals_json),
          std::move(task->per_buyer_signals_json),
          std::move(task->direct_from_seller_result_per_buyer_signals),
          std::move(task->direct_from_seller_result_auction_signals),
          std::move(task->per_buyer_timeout),
          std::move(task->browser_signal_seller_origin),
          std::move(task->browser_signal_top_level_seller_origin),
          std::move(task->bidding_browser_signals), task->auction_start_time,
          std::move(task->trusted_bidding_signals_result), task->trace_id,
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
  if (!IsReadyToReportWin(*task))
    return;

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
  cancelable_task_tracker_.PostTask(
      v8_runner_.get(), FROM_HERE,
      base::BindOnce(
          &BidderWorklet::V8State::ReportWin, base::Unretained(v8_state_.get()),
          std::move(task->interest_group_name),
          std::move(task->auction_signals_json),
          std::move(task->per_buyer_signals_json),
          std::move(task->direct_from_seller_result_per_buyer_signals),
          std::move(task->direct_from_seller_result_auction_signals),
          std::move(task->seller_signals_json),
          std::move(task->browser_signal_render_url),
          std::move(task->browser_signal_bid),
          std::move(task->browser_signal_highest_scoring_other_bid),
          std::move(task->browser_signal_made_highest_scoring_other_bid),
          std::move(task->browser_signal_seller_origin),
          std::move(task->browser_signal_top_level_seller_origin),
          std::move(task->bidding_signals_data_version), task->trace_id,
          base::BindOnce(&BidderWorklet::DeliverReportWinOnUserThread,
                         weak_ptr_factory_.GetWeakPtr(), task)));
}

void BidderWorklet::DeliverBidCallbackOnUserThread(
    GenerateBidTaskList::iterator task,
    mojom::BidderWorkletBidPtr bid,
    mojom::BidderWorkletKAnonEnforcedBidPtr kanon_bid,
    absl::optional<uint32_t> bidding_signals_data_version,
    absl::optional<GURL> debug_loss_report_url,
    absl::optional<GURL> debug_win_report_url,
    absl::optional<double> set_priority,
    base::flat_map<std::string, mojom::PrioritySignalsDoublePtr>
        update_priority_signals_overrides,
    PrivateAggregationRequests pa_requests,
    base::TimeDelta bidding_duration,
    std::vector<std::string> error_msgs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  error_msgs.insert(error_msgs.end(), load_code_error_msgs_.begin(),
                    load_code_error_msgs_.end());
  if (task->trusted_bidding_signals_error_msg) {
    error_msgs.emplace_back(
        std::move(task->trusted_bidding_signals_error_msg).value());
  }
  task->generate_bid_client->OnGenerateBidComplete(
      std::move(bid), std::move(kanon_bid),
      bidding_signals_data_version.value_or(0),
      bidding_signals_data_version.has_value(), debug_loss_report_url,
      debug_win_report_url, set_priority.value_or(0), set_priority.has_value(),
      std::move(update_priority_signals_overrides), std::move(pa_requests),
      bidding_duration, error_msgs);
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
    absl::optional<GURL> report_url,
    base::flat_map<std::string, GURL> ad_beacon_map,
    PrivateAggregationRequests pa_requests,
    std::vector<std::string> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  errors.insert(errors.end(), load_code_error_msgs_.begin(),
                load_code_error_msgs_.end());
  std::move(task->callback)
      .Run(std::move(report_url), std::move(ad_beacon_map),
           std::move(pa_requests), std::move(errors));
  report_win_tasks_.erase(task);
}

bool BidderWorklet::IsCodeReady() const {
  // If `paused_`, loading hasn't started yet. Otherwise, null loaders indicate
  // the worklet script has loaded successfully, and there's no WASM helper, or
  // it has also loaded successfully.
  return !paused_ && !worklet_loader_ && !wasm_loader_;
}

}  // namespace auction_worklet
