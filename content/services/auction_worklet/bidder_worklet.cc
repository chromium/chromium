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
#include "base/time/time.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/report_bindings.h"
#include "content/services/auction_worklet/trusted_bidding_signals.h"
#include "content/services/auction_worklet/worklet_loader.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "v8/include/v8.h"

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

}  // namespace

BidderWorklet::BidderWorklet(
    scoped_refptr<AuctionV8Helper> v8_helper,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_url_loader_factory,
    mojom::BiddingInterestGroupPtr bidding_interest_group,
    const absl::optional<std::string>& auction_signals_json,
    const absl::optional<std::string>& per_buyer_signals_json,
    const url::Origin& browser_signal_top_window_origin,
    const url::Origin& browser_signal_seller_origin,
    base::Time auction_start_time,
    mojom::AuctionWorkletService::LoadBidderWorkletAndGenerateBidCallback
        load_bidder_worklet_and_generate_bid_callback)
    : v8_runner_(v8_helper->v8_runner()),
      load_bidder_worklet_and_generate_bid_callback_(
          std::move(load_bidder_worklet_and_generate_bid_callback)),
      v8_state_(nullptr, base::OnTaskRunnerDeleter(v8_runner_)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  DCHECK(load_bidder_worklet_and_generate_bid_callback_);

  // TODO(mmenke): Remove up the value_or() for script_source_url- auction
  // worklets shouldn't be created when there's no bidding URL.
  GURL script_source_url(
      bidding_interest_group->group.bidding_url.value_or(GURL()));

  // Bind URLLoaderFactory. Remote is not needed after this method completes,
  // since requests will continue after the URLLoaderFactory pipe has been
  // closed, so no need to keep it around after requests have been issued.
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory(
      std::move(pending_url_loader_factory));

  if (bidding_interest_group->group.trusted_bidding_signals_url.has_value() &&
      bidding_interest_group->group.trusted_bidding_signals_keys.has_value() &&
      !bidding_interest_group->group.trusted_bidding_signals_keys->empty()) {
    trusted_bidding_signals_ = std::make_unique<TrustedBiddingSignals>(
        url_loader_factory.get(),
        *bidding_interest_group->group.trusted_bidding_signals_keys,
        browser_signal_top_window_origin.host(),
        *bidding_interest_group->group.trusted_bidding_signals_url, v8_helper,
        base::BindOnce(&BidderWorklet::OnTrustedBiddingSignalsDownloaded,
                       base::Unretained(this)));
  }

  v8_state_ = std::unique_ptr<V8State, base::OnTaskRunnerDeleter>(
      new V8State(v8_helper, script_source_url, weak_ptr_factory_.GetWeakPtr(),
                  std::move(bidding_interest_group), auction_signals_json,
                  per_buyer_signals_json, browser_signal_top_window_origin,
                  browser_signal_seller_origin, auction_start_time),
      base::OnTaskRunnerDeleter(v8_runner_));

  worklet_loader_ = std::make_unique<WorkletLoader>(
      url_loader_factory.get(), script_source_url, v8_helper,
      base::BindOnce(&BidderWorklet::OnScriptDownloaded,
                     base::Unretained(this)));
}

BidderWorklet::~BidderWorklet() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  if (load_bidder_worklet_and_generate_bid_callback_)
    InvokeBidCallbackOnError();
}

void BidderWorklet::ReportWin(
    const std::string& seller_signals_json,
    const GURL& browser_signal_render_url,
    const std::string& browser_signal_ad_render_fingerprint,
    double browser_signal_bid,
    ReportWinCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  v8_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BidderWorklet::V8State::ReportWin,
                                base::Unretained(v8_state_.get()),
                                seller_signals_json, browser_signal_render_url,
                                browser_signal_ad_render_fingerprint,
                                browser_signal_bid, std::move(callback)));
}

BidderWorklet::V8State::V8State(
    scoped_refptr<AuctionV8Helper> v8_helper,
    GURL script_source_url,
    base::WeakPtr<BidderWorklet> parent,
    mojom::BiddingInterestGroupPtr bidding_interest_group,
    const absl::optional<std::string>& auction_signals_json,
    const absl::optional<std::string>& per_buyer_signals_json,
    const url::Origin& browser_signal_top_window_origin,
    const url::Origin& browser_signal_seller_origin,
    base::Time auction_start_time)
    : v8_helper_(std::move(v8_helper)),
      parent_(std::move(parent)),
      user_thread_(base::SequencedTaskRunnerHandle::Get()),
      bidding_interest_group_(std::move(bidding_interest_group)),
      auction_signals_json_(auction_signals_json),
      per_buyer_signals_json_(per_buyer_signals_json),
      browser_signal_top_window_origin_(browser_signal_top_window_origin),
      browser_signal_seller_origin_(browser_signal_seller_origin),
      auction_start_time_(auction_start_time),
      script_source_url_(std::move(script_source_url)) {
  DETACH_FROM_SEQUENCE(v8_sequence_checker_);
}

void BidderWorklet::V8State::SetWorkletScript(
    WorkletLoader::Result worklet_script) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  worklet_script_ = worklet_script.TakeScript();
}

void BidderWorklet::V8State::SetTrustedSignalsResult(
    std::unique_ptr<TrustedBiddingSignals::Result>
        trusted_bidding_signals_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  trusted_bidding_signals_result_ = std::move(trusted_bidding_signals_result);
}

void BidderWorklet::V8State::ReportWin(
    const std::string& seller_signals_json,
    const GURL& browser_signal_render_url,
    const std::string& browser_signal_ad_render_fingerprint,
    double browser_signal_bid,
    ReportWinCallback callback) {
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
  if (!AppendJsonValueOrNull(v8_helper_.get(), context, auction_signals_json_,
                             &args) ||
      !AppendJsonValueOrNull(v8_helper_.get(), context, per_buyer_signals_json_,
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
                                browser_signal_top_window_origin_.host()) ||
      !browser_signals_dict.Set(
          "interestGroupOwner",
          bidding_interest_group_->group.owner.Serialize()) ||
      !browser_signals_dict.Set("interestGroupName",
                                bidding_interest_group_->group.name) ||
      !browser_signals_dict.Set("renderUrl",
                                browser_signal_render_url.spec()) ||
      !browser_signals_dict.Set("adRenderFingerprint",
                                browser_signal_ad_render_fingerprint) ||
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
  if (v8_helper_
          ->RunScript(context, worklet_script_.Get(isolate), "reportWin", args,
                      errors_out)
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

void BidderWorklet::V8State::GenerateBid() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);

  const blink::InterestGroup& interest_group = bidding_interest_group_->group;
  // Can't make a bid without any ads.
  if (!interest_group.ads) {
    PostErrorBidCallbackToUserThread();
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
    PostErrorBidCallbackToUserThread();
    return;
  }

  std::vector<v8::Local<v8::Value>> ads_vector;
  for (const auto& ad : *interest_group.ads) {
    v8::Local<v8::Object> ad_object = v8::Object::New(isolate);
    gin::Dictionary ad_dict(isolate, ad_object);
    if (!ad_dict.Set("renderUrl", ad.render_url.spec()) ||
        (ad.metadata && !v8_helper_->InsertJsonValue(
                            context, "metadata", *ad.metadata, ad_object))) {
      PostErrorBidCallbackToUserThread();
      return;
    }
    ads_vector.emplace_back(std::move(ad_object));
  }
  if (!v8_helper_->InsertValue(
          "ads", v8::Array::New(isolate, ads_vector.data(), ads_vector.size()),
          interest_group_object)) {
    PostErrorBidCallbackToUserThread();
    return;
  }

  args.push_back(std::move(interest_group_object));

  if (!AppendJsonValueOrNull(v8_helper_.get(), context, auction_signals_json_,
                             &args) ||
      !AppendJsonValueOrNull(v8_helper_.get(), context, per_buyer_signals_json_,
                             &args)) {
    PostErrorBidCallbackToUserThread();
    return;
  }

  v8::Local<v8::Value> trusted_signals;
  if (!trusted_bidding_signals_result_) {
    trusted_signals = v8::Null(isolate);
  } else {
    trusted_signals = trusted_bidding_signals_result_->GetSignals(
        v8_helper_.get(), context,
        *interest_group.trusted_bidding_signals_keys);
  }
  args.push_back(trusted_signals);

  v8::Local<v8::Object> browser_signals = v8::Object::New(isolate);
  gin::Dictionary browser_signals_dict(isolate, browser_signals);
  if (!browser_signals_dict.Set("topWindowHostname",
                                browser_signal_top_window_origin_.host()) ||
      !browser_signals_dict.Set("seller",
                                browser_signal_seller_origin_.Serialize()) ||
      !browser_signals_dict.Set("joinCount",
                                bidding_interest_group_->signals->join_count) ||
      !browser_signals_dict.Set("bidCount",
                                bidding_interest_group_->signals->bid_count)) {
    PostErrorBidCallbackToUserThread();
    return;
  }

  v8::Local<v8::Value> prev_wins;
  if (!CreatePrevWinsArray(v8_helper_.get(), context, auction_start_time_,
                           bidding_interest_group_->signals->prev_wins)
           .ToLocal(&prev_wins)) {
    PostErrorBidCallbackToUserThread();
    return;
  }

  v8::Maybe<bool> result = browser_signals->Set(
      context, gin::StringToV8(isolate, "prevWins"), prev_wins);
  if (result.IsNothing() || !result.FromJust()) {
    PostErrorBidCallbackToUserThread();
    return;
  }

  args.push_back(browser_signals);

  v8::Local<v8::Value> generate_bid_result;
  std::vector<std::string> errors_out;
  if (!v8_helper_
           ->RunScript(context, worklet_script_.Get(isolate), "generateBid",
                       args, errors_out)
           .ToLocal(&generate_bid_result)) {
    PostErrorBidCallbackToUserThread(std::move(errors_out));
    return;
  }

  if (!generate_bid_result->IsObject()) {
    errors_out.push_back(
        base::StrCat({script_source_url_.spec(),
                      " generateBid() return value not an object."}));
    PostErrorBidCallbackToUserThread(std::move(errors_out));
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
    PostErrorBidCallbackToUserThread(std::move(errors_out));
    return;
  }

  if (bid <= 0 || std::isnan(bid) || !std::isfinite(bid)) {
    PostErrorBidCallbackToUserThread(std::move(errors_out));
    return;
  }

  GURL render_url(render_url_string);
  if (!render_url.is_valid() || !render_url.SchemeIs(url::kHttpsScheme)) {
    errors_out.push_back(base::StrCat(
        {script_source_url_.spec(),
         " generateBid() returned render_url isn't a valid https:// URL."}));
    PostErrorBidCallbackToUserThread(std::move(errors_out));
    return;
  }

  // `render_url` must be in `ad_render_urls`.
  for (const auto& ad : *interest_group.ads) {
    if (render_url == ad.render_url) {
      user_thread_->PostTask(
          FROM_HERE,
          base::BindOnce(&BidderWorklet::DeliverBidCallbackOnUserThread,
                         parent_,
                         mojom::BidderWorkletBid::New(
                             std::move(ad_json), bid, std::move(render_url),
                             base::TimeTicks::Now() - start /* bid_duration */),
                         std::move(errors_out)));
      return;
    }
  }
  errors_out.push_back(
      base::StrCat({script_source_url_.spec(),
                    " generateBid() returned render_url isn't one "
                    "of the registered creative URLs."}));
  PostErrorBidCallbackToUserThread(std::move(errors_out));
}

BidderWorklet::V8State::~V8State() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
}

void BidderWorklet::V8State::PostReportWinCallbackToUserThread(
    ReportWinCallback callback,
    absl::optional<GURL> report_url,
    std::vector<std::string> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  user_thread_->PostTask(
      FROM_HERE, base::BindOnce(&BidderWorklet::DeliverReportWinOnUserThread,
                                parent_, std::move(callback),
                                std::move(report_url), std::move(errors)));
}

void BidderWorklet::V8State::PostErrorBidCallbackToUserThread(
    std::vector<std::string> error_msgs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  user_thread_->PostTask(
      FROM_HERE, base::BindOnce(&BidderWorklet::InvokeBidCallbackOnError,
                                parent_, std::move(error_msgs)));
}

void BidderWorklet::OnScriptDownloaded(WorkletLoader::Result worklet_script,
                                       absl::optional<std::string> error_msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  DCHECK(load_bidder_worklet_and_generate_bid_callback_);

  if (!worklet_script.success()) {
    // Abort loading trusted bidding signals, if it hasn't completed already.
    trusted_bidding_signals_.reset();
    std::vector<std::string> errors;
    if (error_msg.has_value())
      errors.emplace_back(std::move(error_msg).value());
    InvokeBidCallbackOnError(std::move(errors));
    return;
  }

  worklet_loader_.reset();
  have_worklet_script_ = true;
  v8_runner_->PostTask(FROM_HERE,
                       base::BindOnce(&BidderWorklet::V8State::SetWorkletScript,
                                      base::Unretained(v8_state_.get()),
                                      std::move(worklet_script)));

  GenerateBidIfReady();
}

void BidderWorklet::OnTrustedBiddingSignalsDownloaded(
    std::unique_ptr<TrustedBiddingSignals::Result> result,
    absl::optional<std::string> error_msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  // Worklet results should still be pending.
  DCHECK(load_bidder_worklet_and_generate_bid_callback_);

  trusted_bidding_signals_error_msg_ = std::move(error_msg);
  trusted_bidding_signals_.reset();
  v8_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BidderWorklet::V8State::SetTrustedSignalsResult,
                     base::Unretained(v8_state_.get()), std::move(result)));

  GenerateBidIfReady();
}

void BidderWorklet::GenerateBidIfReady() {
  DCHECK(load_bidder_worklet_and_generate_bid_callback_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  if (trusted_bidding_signals_ || !have_worklet_script_)
    return;

  v8_runner_->PostTask(FROM_HERE,
                       base::BindOnce(&BidderWorklet::V8State::GenerateBid,
                                      base::Unretained(v8_state_.get())));
}

void BidderWorklet::InvokeBidCallbackOnError(
    std::vector<std::string> error_msgs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  DeliverBidCallbackOnUserThread(mojom::BidderWorkletBidPtr(),
                                 std::move(error_msgs));
}

void BidderWorklet::DeliverBidCallbackOnUserThread(
    mojom::BidderWorkletBidPtr bid,
    std::vector<std::string> error_msgs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  if (trusted_bidding_signals_error_msg_) {
    error_msgs.emplace_back(
        std::move(trusted_bidding_signals_error_msg_).value());
  }
  std::move(load_bidder_worklet_and_generate_bid_callback_)
      .Run(std::move(bid), error_msgs);
}

void BidderWorklet::DeliverReportWinOnUserThread(
    ReportWinCallback callback,
    absl::optional<GURL> report_url,
    std::vector<std::string> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  std::move(callback).Run(std::move(report_url), std::move(errors));
}

}  // namespace auction_worklet
