// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/bidder_worklet.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/cxx17_backports.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/report_bindings.h"
#include "content/services/auction_worklet/trusted_signals.h"
#include "content/services/auction_worklet/worklet_loader.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

namespace auction_worklet {

namespace {

bool AppendJsonValueOrNull(AuctionV8Helper* const v8_helper,
                           v8::Local<v8::Context> context,
                           const absl::optional<std::string>& maybe_json,
                           std::vector<v8::Local<v8::Value>>* args) {
  v8::Isolate* isolate = v8_helper->isolate();
  if (maybe_json.has_value()) {
    if (!v8_helper->AppendJsonValue(context, maybe_json.value(), args))
      return false;
  } else {
    args->push_back(v8::Null(isolate));
  }
  return true;
}

// Creates a V8 array containing information about the passed in previous wins.
// Array is sorted by time, earliest wins first. Modifies order of `prev_wins`
// input vector. This should should be harmless, since each list of previous
// wins is only used for a single bid in a single auction, and its order is
// unspecified, anyways.
v8::MaybeLocal<v8::Value> CreatePrevWinsArray(
    AuctionV8Helper* v8_helper,
    v8::Local<v8::Context> context,
    base::Time auction_start_time,
    std::vector<mojom::PreviousWinPtr>& prev_wins) {
  std::sort(prev_wins.begin(), prev_wins.end(),
            [](const mojom::PreviousWinPtr& prev_win1,
               const mojom::PreviousWinPtr& prev_win2) {
              return prev_win1->time < prev_win2->time;
            });
  std::vector<v8::Local<v8::Value>> prev_wins_v8;
  v8::Isolate* isolate = v8_helper->isolate();
  for (const auto& prev_win : prev_wins) {
    int64_t time_delta = (auction_start_time - prev_win->time).InSeconds();
    // Don't give negative times if clock has changed since last auction win.
    if (time_delta < 0)
      time_delta = 0;
    v8::Local<v8::Value> win_values[2];
    win_values[0] = v8::Number::New(isolate, time_delta);
    if (!v8_helper->CreateValueFromJson(context, prev_win->ad_json)
             .ToLocal(&win_values[1])) {
      return v8::MaybeLocal<v8::Value>();
    }
    prev_wins_v8.push_back(
        v8::Array::New(isolate, win_values, base::size(win_values)));
  }
  return v8::Array::New(isolate, prev_wins_v8.data(), prev_wins_v8.size());
}

// Converts a vector of blink::InterestGroup::Ads into a v8 object.
bool CreateAdVector(AuctionV8Helper* v8_helper,
                    v8::Local<v8::Context> context,
                    const std::vector<blink::InterestGroup::Ad>& ads,
                    v8::Local<v8::Value>& out_value) {
  v8::Isolate* isolate = v8_helper->isolate();

  std::vector<v8::Local<v8::Value>> ads_vector;
  for (const auto& ad : ads) {
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

// Checks that `url` is a valid URL and is in `ads`. Appends an error to
// `out_errors` if not. `script_source_url` is used in output error messages
// only.
bool IsAllowedAdUrl(const GURL& url,
                    const GURL& script_source_url,
                    const char* argument_name,
                    const std::vector<blink::InterestGroup::Ad>& ads,
                    std::vector<std::string>& out_errors) {
  if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme)) {
    out_errors.push_back(
        base::StrCat({script_source_url.spec(), " generateBid() returned ",
                      argument_name, " URL that isn't a valid https:// URL."}));
    return false;
  }

  for (const auto& ad : ads) {
    if (url == ad.render_url)
      return true;
  }
  out_errors.push_back(base::StrCat({script_source_url.spec(),
                                     " generateBid() returned ", argument_name,
                                     " URL that isn't one "
                                     "of the registered creative URLs."}));
  return false;
}

}  // namespace

BidderWorklet::BidderWorklet(
    scoped_refptr<AuctionV8Helper> v8_helper,
    bool pause_for_debugger_on_start,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_url_loader_factory,
    mojom::BiddingInterestGroupPtr bidding_interest_group)
    : v8_runner_(v8_helper->v8_runner()),
      v8_helper_(v8_helper),
      debug_id_(
          base::MakeRefCounted<AuctionV8Helper::DebugId>(v8_helper.get())),
      // TODO(mmenke): Remove up the value_or() for script_source_url_; auction
      // worklets shouldn't be created when there's no bidding URL.
      script_source_url_(
          bidding_interest_group->group.bidding_url.value_or(GURL())),
      trusted_bidding_signals_url_(
          bidding_interest_group->group.trusted_bidding_signals_url),
      trusted_bidding_signals_keys_(
          bidding_interest_group->group.trusted_bidding_signals_keys),
      v8_state_(nullptr, base::OnTaskRunnerDeleter(v8_runner_)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  url_loader_factory_.Bind(std::move(pending_url_loader_factory));

  v8_state_ = std::unique_ptr<V8State, base::OnTaskRunnerDeleter>(
      new V8State(v8_helper, debug_id_, script_source_url_,
                  weak_ptr_factory_.GetWeakPtr(),
                  std::move(bidding_interest_group)),
      base::OnTaskRunnerDeleter(v8_runner_));

  paused_ = pause_for_debugger_on_start;
  if (!paused_)
    Start();
}

BidderWorklet::~BidderWorklet() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  // Invoke any pending callbacks, since destroying uninvoked Mojo callbacks can
  // sometimes cause issues if the pipe they're over is still open. This is not
  // strictly necessary here, since the BidderWorklet's received pipe is
  // destroyed before the BidderWorklet itself is, but makes the class safer
  // against refactors of the Mojo API.
  FailAllPendingTasks();

  debug_id_->AbortDebuggerPauses();
}

int BidderWorklet::context_group_id_for_testing() const {
  return debug_id_->context_group_id();
}

void BidderWorklet::GenerateBid(
    const absl::optional<std::string>& auction_signals_json,
    const absl::optional<std::string>& per_buyer_signals_json,
    const url::Origin& top_window_origin,
    const url::Origin& seller_origin,
    base::Time auction_start_time,
    GenerateBidCallback generate_bid_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  generate_bid_tasks_.emplace_front();
  auto generate_bid_task = generate_bid_tasks_.begin();
  generate_bid_task->auction_signals_json = auction_signals_json;
  generate_bid_task->per_buyer_signals_json = per_buyer_signals_json;
  generate_bid_task->top_window_origin = top_window_origin;
  generate_bid_task->seller_origin = seller_origin;
  generate_bid_task->auction_start_time = auction_start_time;
  generate_bid_task->callback = std::move(generate_bid_callback);

  // If worklet script failed to load, fail and exit early.
  if (!is_loading_ && !have_worklet_script_) {
    DeliverBidCallbackOnUserThread(generate_bid_task,
                                   mojom::BidderWorkletBidPtr(),
                                   /*error_msgs=*/std::vector<std::string>());
    return;
  }

  if (trusted_bidding_signals_url_.has_value() &&
      trusted_bidding_signals_keys_.has_value() &&
      !trusted_bidding_signals_keys_->empty()) {
    generate_bid_task->trusted_bidding_signals =
        TrustedSignals::LoadBiddingSignals(
            url_loader_factory_.get(), *trusted_bidding_signals_keys_,
            top_window_origin.host(), *trusted_bidding_signals_url_, v8_helper_,
            base::BindOnce(&BidderWorklet::OnTrustedBiddingSignalsDownloaded,
                           base::Unretained(this), generate_bid_task));
    return;
  }

  GenerateBidIfReady(generate_bid_task);
}

void BidderWorklet::ReportWin(
    const absl::optional<std::string>& auction_signals_json,
    const absl::optional<std::string>& per_buyer_signals_json,
    const url::Origin& top_window_origin,
    const std::string& seller_signals_json,
    const GURL& browser_signal_render_url,
    double browser_signal_bid,
    ReportWinCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  report_win_tasks_.emplace_front();
  auto report_win_task = report_win_tasks_.begin();
  report_win_task->auction_signals_json = auction_signals_json;
  report_win_task->per_buyer_signals_json = per_buyer_signals_json;
  report_win_task->top_window_origin = top_window_origin;
  report_win_task->seller_signals_json = seller_signals_json;
  report_win_task->browser_signal_render_url = browser_signal_render_url;
  report_win_task->browser_signal_bid = browser_signal_bid;
  report_win_task->callback = std::move(callback);

  // If worklet script isn't loaded, can't run script immediately.
  if (!have_worklet_script_) {
    // If worklet script failed to load, fail and exit early.
    if (!is_loading_) {
      DeliverReportWinOnUserThread(report_win_task,
                                   /*report_url=*/absl::optional<GURL>(),
                                   /*errors=*/std::vector<std::string>());
    }
    return;
  }

  RunReportWin(report_win_task);
}

void BidderWorklet::ConnectDevToolsAgent(
    mojo::PendingReceiver<blink::mojom::DevToolsAgent> agent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  v8_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&V8State::ConnectDevToolsAgent,
                     base::Unretained(v8_state_.get()), std::move(agent)));
}

BidderWorklet::GenerateBidTask::GenerateBidTask() = default;
BidderWorklet::GenerateBidTask::~GenerateBidTask() = default;

BidderWorklet::ReportWinTask::ReportWinTask() = default;
BidderWorklet::ReportWinTask::~ReportWinTask() = default;

BidderWorklet::V8State::V8State(
    scoped_refptr<AuctionV8Helper> v8_helper,
    scoped_refptr<AuctionV8Helper::DebugId> debug_id,
    const GURL& script_source_url,
    base::WeakPtr<BidderWorklet> parent,
    mojom::BiddingInterestGroupPtr bidding_interest_group)
    : v8_helper_(std::move(v8_helper)),
      debug_id_(std::move(debug_id)),
      parent_(std::move(parent)),
      user_thread_(base::SequencedTaskRunnerHandle::Get()),
      bidding_interest_group_(std::move(bidding_interest_group)),
      script_source_url_(std::move(script_source_url)) {
  DETACH_FROM_SEQUENCE(v8_sequence_checker_);
  v8_helper_->v8_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V8State::FinishInit, base::Unretained(this)));
}

void BidderWorklet::V8State::SetWorkletScript(
    WorkletLoader::Result worklet_script) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  worklet_script_ = worklet_script.TakeScript();
}

void BidderWorklet::V8State::ReportWin(
    const absl::optional<std::string>& auction_signals_json,
    const absl::optional<std::string>& per_buyer_signals_json,
    const url::Origin& browser_signal_top_window_origin,
    const std::string& seller_signals_json,
    const GURL& browser_signal_render_url,
    double browser_signal_bid,
    ReportWinCallbackInternal callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);

  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper_.get());
  v8::Isolate* isolate = v8_helper_->isolate();

  v8::Local<v8::ObjectTemplate> global_template =
      v8::ObjectTemplate::New(isolate);
  ReportBindings report_bindings(v8_helper_.get(), global_template);

  // Short lived context, to avoid leaking data at global scope between either
  // repeated calls to this worklet, or to calls to any other worklet.
  v8::Local<v8::Context> context = v8_helper_->CreateContext(global_template);
  v8::Context::Scope context_scope(context);

  std::vector<v8::Local<v8::Value>> args;
  if (!AppendJsonValueOrNull(v8_helper_.get(), context, auction_signals_json,
                             &args) ||
      !AppendJsonValueOrNull(v8_helper_.get(), context, per_buyer_signals_json,
                             &args) ||
      !v8_helper_->AppendJsonValue(context, seller_signals_json, &args)) {
    PostReportWinCallbackToUserThread(std::move(callback),
                                      absl::nullopt /* report_url */,
                                      std::vector<std::string>() /* errors */);
    return;
  }

  v8::Local<v8::Object> browser_signals = v8::Object::New(isolate);
  gin::Dictionary browser_signals_dict(isolate, browser_signals);
  if (!browser_signals_dict.Set("topWindowHostname",
                                browser_signal_top_window_origin.host()) ||
      !browser_signals_dict.Set(
          "interestGroupOwner",
          bidding_interest_group_->group.owner.Serialize()) ||
      !browser_signals_dict.Set("interestGroupName",
                                bidding_interest_group_->group.name) ||
      !browser_signals_dict.Set("renderUrl",
                                browser_signal_render_url.spec()) ||
      !browser_signals_dict.Set("bid", browser_signal_bid)) {
    PostReportWinCallbackToUserThread(std::move(callback),
                                      absl::nullopt /* report_url */,
                                      std::vector<std::string>() /* errors */);
    return;
  }
  args.push_back(browser_signals);

  // An empty return value indicates an exception was thrown. Any other return
  // value indicates no exception.
  std::vector<std::string> errors_out;
  v8_helper_->MaybeTriggerInstrumentationBreakpoint(
      *debug_id_, "beforeBidderWorkletReportingStart");
  if (v8_helper_
          ->RunScript(context, worklet_script_.Get(isolate), debug_id_.get(),
                      "reportWin", args, errors_out)
          .IsEmpty()) {
    PostReportWinCallbackToUserThread(std::move(callback),
                                      absl::nullopt /* report_url */,
                                      std::move(errors_out));
    return;
  }

  // This covers both the case where a report URL was provided, and the case one
  // was not.
  PostReportWinCallbackToUserThread(
      std::move(callback), report_bindings.report_url(), std::move(errors_out));
}

void BidderWorklet::V8State::GenerateBid(
    const absl::optional<std::string>& auction_signals_json,
    const absl::optional<std::string>& per_buyer_signals_json,
    const url::Origin& browser_signal_top_window_origin,
    const url::Origin& browser_signal_seller_origin,
    base::Time auction_start_time,
    std::unique_ptr<TrustedSignals::Result> trusted_bidding_signals_result,
    GenerateBidCallbackInternal callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);

  const blink::InterestGroup& interest_group = bidding_interest_group_->group;
  // Can't make a bid without any ads.
  if (!interest_group.ads) {
    PostErrorBidCallbackToUserThread(std::move(callback));
    return;
  }

  base::TimeTicks start = base::TimeTicks::Now();

  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper_.get());
  v8::Isolate* isolate = v8_helper_->isolate();
  // Short lived context, to avoid leaking data at global scope between either
  // repeated calls to this worklet, or to calls to any other worklet.
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope context_scope(context);

  std::vector<v8::Local<v8::Value>> args;
  v8::Local<v8::Object> interest_group_object = v8::Object::New(isolate);
  gin::Dictionary interest_group_dict(isolate, interest_group_object);
  if (!interest_group_dict.Set("owner", interest_group.owner.Serialize()) ||
      !interest_group_dict.Set("name", interest_group.name) ||
      (interest_group.user_bidding_signals &&
       !v8_helper_->InsertJsonValue(context, "userBiddingSignals",
                                    *interest_group.user_bidding_signals,
                                    interest_group_object))) {
    PostErrorBidCallbackToUserThread(std::move(callback));
    return;
  }

  v8::Local<v8::Value> ads;
  if (!CreateAdVector(v8_helper_.get(), context, *interest_group.ads, ads) ||
      !v8_helper_->InsertValue("ads", std::move(ads), interest_group_object)) {
    PostErrorBidCallbackToUserThread(std::move(callback));
    return;
  }

  if (interest_group.ad_components) {
    v8::Local<v8::Value> ad_components;
    if (!CreateAdVector(v8_helper_.get(), context,
                        *interest_group.ad_components, ad_components) ||
        !v8_helper_->InsertValue("adComponents", std::move(ad_components),
                                 interest_group_object)) {
      PostErrorBidCallbackToUserThread(std::move(callback));
      return;
    }
  }

  args.push_back(std::move(interest_group_object));

  if (!AppendJsonValueOrNull(v8_helper_.get(), context, auction_signals_json,
                             &args) ||
      !AppendJsonValueOrNull(v8_helper_.get(), context, per_buyer_signals_json,
                             &args)) {
    PostErrorBidCallbackToUserThread(std::move(callback));
    return;
  }

  v8::Local<v8::Value> trusted_signals;
  if (!trusted_bidding_signals_result) {
    trusted_signals = v8::Null(isolate);
  } else {
    trusted_signals = trusted_bidding_signals_result->GetBiddingSignals(
        v8_helper_.get(), context,
        *interest_group.trusted_bidding_signals_keys);
  }
  args.push_back(trusted_signals);

  v8::Local<v8::Object> browser_signals = v8::Object::New(isolate);
  gin::Dictionary browser_signals_dict(isolate, browser_signals);
  if (!browser_signals_dict.Set("topWindowHostname",
                                browser_signal_top_window_origin.host()) ||
      !browser_signals_dict.Set("seller",
                                browser_signal_seller_origin.Serialize()) ||
      !browser_signals_dict.Set("joinCount",
                                bidding_interest_group_->signals->join_count) ||
      !browser_signals_dict.Set("bidCount",
                                bidding_interest_group_->signals->bid_count)) {
    PostErrorBidCallbackToUserThread(std::move(callback));
    return;
  }

  v8::Local<v8::Value> prev_wins;
  if (!CreatePrevWinsArray(v8_helper_.get(), context, auction_start_time,
                           bidding_interest_group_->signals->prev_wins)
           .ToLocal(&prev_wins)) {
    PostErrorBidCallbackToUserThread(std::move(callback));
    return;
  }

  v8::Maybe<bool> result = browser_signals->Set(
      context, gin::StringToV8(isolate, "prevWins"), prev_wins);
  if (result.IsNothing() || !result.FromJust()) {
    PostErrorBidCallbackToUserThread(std::move(callback));
    return;
  }

  args.push_back(browser_signals);

  v8::Local<v8::Value> generate_bid_result;
  std::vector<std::string> errors_out;
  v8_helper_->MaybeTriggerInstrumentationBreakpoint(
      *debug_id_, "beforeBidderWorkletBiddingStart");
  if (!v8_helper_
           ->RunScript(context, worklet_script_.Get(isolate), debug_id_.get(),
                       "generateBid", args, errors_out)
           .ToLocal(&generate_bid_result)) {
    PostErrorBidCallbackToUserThread(std::move(callback),
                                     std::move(errors_out));
    return;
  }

  if (!generate_bid_result->IsObject()) {
    errors_out.push_back(
        base::StrCat({script_source_url_.spec(),
                      " generateBid() return value not an object."}));
    PostErrorBidCallbackToUserThread(std::move(callback),
                                     std::move(errors_out));
    return;
  }

  gin::Dictionary result_dict(isolate, generate_bid_result.As<v8::Object>());

  v8::Local<v8::Value> ad_object;
  std::string ad_json;
  double bid;
  std::string render_url_string;
  // Parse and validate values.
  if (!result_dict.Get("ad", &ad_object) ||
      !v8_helper_->ExtractJson(context, ad_object, &ad_json) ||
      !result_dict.Get("bid", &bid) ||
      !result_dict.Get("render", &render_url_string)) {
    errors_out.push_back(
        base::StrCat({script_source_url_.spec(),
                      " generateBid() return value has incorrect structure."}));
    PostErrorBidCallbackToUserThread(std::move(callback),
                                     std::move(errors_out));
    return;
  }

  if (bid <= 0 || std::isnan(bid) || !std::isfinite(bid)) {
    PostErrorBidCallbackToUserThread(std::move(callback),
                                     std::move(errors_out));
    return;
  }

  GURL render_url(render_url_string);
  if (!IsAllowedAdUrl(render_url, script_source_url_, "render",
                      *interest_group.ads, errors_out)) {
    PostErrorBidCallbackToUserThread(std::move(callback),
                                     std::move(errors_out));
    return;
  }

  absl::optional<std::vector<GURL>> ad_component_urls;
  v8::Local<v8::Value> ad_components;
  if (result_dict.Get("adComponents", &ad_components) &&
      !ad_components->IsNullOrUndefined()) {
    if (!interest_group.ad_components) {
      errors_out.push_back(
          base::StrCat({script_source_url_.spec(),
                        " generateBid() return value contains adComponents but "
                        "InterestGroup has no adComponents."}));
      PostErrorBidCallbackToUserThread(std::move(callback),
                                       std::move(errors_out));
      return;
    }

    if (!ad_components->IsArray()) {
      errors_out.push_back(base::StrCat(
          {script_source_url_.spec(),
           " generateBid() returned adComponents value must be an array."}));
      PostErrorBidCallbackToUserThread(std::move(callback),
                                       std::move(errors_out));
      return;
    }

    v8::Local<v8::Array> ad_components_array = ad_components.As<v8::Array>();
    if (ad_components_array->Length() > blink::kMaxAdAuctionAdComponents) {
      errors_out.push_back(base::StringPrintf(
          "%s generateBid() returned adComponents with over %zu items.",
          script_source_url_.spec().c_str(), blink::kMaxAdAuctionAdComponents));
      PostErrorBidCallbackToUserThread(std::move(callback),
                                       std::move(errors_out));
      return;
    }

    ad_component_urls.emplace();
    for (size_t i = 0; i < ad_components_array->Length(); ++i) {
      std::string url_string;
      if (!gin::ConvertFromV8(
              isolate, ad_components_array->Get(context, i).ToLocalChecked(),
              &url_string)) {
        errors_out.push_back(
            base::StrCat({script_source_url_.spec(),
                          " generateBid() returned adComponents value must be "
                          "an array of strings."}));
        PostErrorBidCallbackToUserThread(std::move(callback),
                                         std::move(errors_out));
        return;
      }

      GURL ad_component_url(url_string);
      if (!IsAllowedAdUrl(ad_component_url, script_source_url_, "adComponents",
                          *interest_group.ad_components, errors_out)) {
        PostErrorBidCallbackToUserThread(std::move(callback),
                                         std::move(errors_out));
        return;
      }
      ad_component_urls->emplace_back(std::move(ad_component_url));
    }
  }

  user_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     mojom::BidderWorkletBid::New(
                         std::move(ad_json), bid, std::move(render_url),
                         std::move(ad_component_urls),
                         base::TimeTicks::Now() - start /* bid_duration */),
                     std::move(errors_out)));
}

void BidderWorklet::V8State::ConnectDevToolsAgent(
    mojo::PendingReceiver<blink::mojom::DevToolsAgent> agent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  v8_helper_->ConnectDevToolsAgent(std::move(agent), user_thread_, *debug_id_);
}

BidderWorklet::V8State::~V8State() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
}

void BidderWorklet::V8State::FinishInit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
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
    std::vector<std::string> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  user_thread_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(report_url),
                                std::move(errors)));
}

void BidderWorklet::V8State::PostErrorBidCallbackToUserThread(
    GenerateBidCallbackInternal callback,
    std::vector<std::string> error_msgs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  user_thread_->PostTask(FROM_HERE, base::BindOnce(std::move(callback),
                                                   mojom::BidderWorkletBidPtr(),
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

  worklet_loader_ = std::make_unique<WorkletLoader>(
      url_loader_factory_.get(), script_source_url_, v8_helper_, debug_id_,
      base::BindOnce(&BidderWorklet::OnScriptDownloaded,
                     base::Unretained(this)));
}

void BidderWorklet::OnScriptDownloaded(WorkletLoader::Result worklet_script,
                                       absl::optional<std::string> error_msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  worklet_loader_.reset();

  is_loading_ = false;

  // Fail all pending tasks if the script failed to load.
  if (!worklet_script.success()) {
    load_script_error_msg_ = std::move(error_msg);
    FailAllPendingTasks();
    return;
  }

  have_worklet_script_ = true;
  v8_runner_->PostTask(FROM_HERE,
                       base::BindOnce(&BidderWorklet::V8State::SetWorkletScript,
                                      base::Unretained(v8_state_.get()),
                                      std::move(worklet_script)));

  // Run any pending task that's ready to run.

  // Run all GenerateBid() tasks that are ready. GenerateBidIfReady() does *not*
  // modify `generate_bid_tasks_` when invoked, so this is safe.
  for (auto generate_bid_task = generate_bid_tasks_.begin();
       generate_bid_task != generate_bid_tasks_.end(); ++generate_bid_task) {
    GenerateBidIfReady(generate_bid_task);
  }

  // Run all ReportWin() tasks. RunReportWin() does *not* modify
  // `generate_bid_tasks_` when invoked, so this is safe.
  for (auto report_win_task = report_win_tasks_.begin();
       report_win_task != report_win_tasks_.end(); ++report_win_task) {
    RunReportWin(report_win_task);
  }
}

void BidderWorklet::OnTrustedBiddingSignalsDownloaded(
    GenerateBidTaskList::iterator task,
    std::unique_ptr<TrustedSignals::Result> result,
    absl::optional<std::string> error_msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  task->trusted_bidding_signals_error_msg = std::move(error_msg);
  task->trusted_bidding_signals_result = std::move(result);
  task->trusted_bidding_signals.reset();

  GenerateBidIfReady(task);
}

void BidderWorklet::GenerateBidIfReady(GenerateBidTaskList::iterator task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  if (task->trusted_bidding_signals || !have_worklet_script_)
    return;

  v8_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BidderWorklet::V8State::GenerateBid,
          base::Unretained(v8_state_.get()), task->auction_signals_json,
          task->per_buyer_signals_json, task->top_window_origin,
          task->seller_origin, task->auction_start_time,
          std::move(task->trusted_bidding_signals_result),
          base::BindOnce(&BidderWorklet::DeliverBidCallbackOnUserThread,
                         weak_ptr_factory_.GetWeakPtr(), task)));
}

void BidderWorklet::RunReportWin(ReportWinTaskList::iterator task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  v8_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BidderWorklet::V8State::ReportWin, base::Unretained(v8_state_.get()),
          task->auction_signals_json, task->per_buyer_signals_json,
          task->top_window_origin, task->seller_signals_json,
          task->browser_signal_render_url, task->browser_signal_bid,
          base::BindOnce(&BidderWorklet::DeliverReportWinOnUserThread,
                         weak_ptr_factory_.GetWeakPtr(), task)));
}

void BidderWorklet::FailAllPendingTasks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  while (!generate_bid_tasks_.empty()) {
    DeliverBidCallbackOnUserThread(generate_bid_tasks_.begin(),
                                   mojom::BidderWorkletBidPtr(),
                                   /*error_msgs=*/std::vector<std::string>());
  }
  while (!report_win_tasks_.empty()) {
    DeliverReportWinOnUserThread(report_win_tasks_.begin(),
                                 /*report_url=*/absl::nullopt,
                                 /*errors=*/std::vector<std::string>());
  }
}

void BidderWorklet::DeliverBidCallbackOnUserThread(
    GenerateBidTaskList::iterator task,
    mojom::BidderWorkletBidPtr bid,
    std::vector<std::string> error_msgs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  if (load_script_error_msg_)
    error_msgs.emplace_back(load_script_error_msg_.value());
  if (task->trusted_bidding_signals_error_msg) {
    error_msgs.emplace_back(
        std::move(task->trusted_bidding_signals_error_msg).value());
  }
  std::move(task->callback).Run(std::move(bid), error_msgs);
  generate_bid_tasks_.erase(task);
}

void BidderWorklet::DeliverReportWinOnUserThread(
    ReportWinTaskList::iterator task,
    absl::optional<GURL> report_url,
    std::vector<std::string> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  if (load_script_error_msg_)
    errors.emplace_back(load_script_error_msg_.value());
  std::move(task->callback).Run(std::move(report_url), errors);
  report_win_tasks_.erase(task);
}

}  // namespace auction_worklet
