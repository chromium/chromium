// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/bidder_worklet.h"

#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/report_bindings.h"
#include "content/services/auction_worklet/trusted_bidding_signals.h"
#include "content/services/auction_worklet/worklet_loader.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "v8/include/v8.h"

namespace auction_worklet {

namespace {

bool AppendJsonValueOrNull(AuctionV8Helper* const v8_helper,
                           v8::Local<v8::Context> context,
                           const base::Optional<std::string>& maybe_json,
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

}  // namespace

BidderWorklet::BidResult::BidResult() = default;

BidderWorklet::BidResult::BidResult(std::string ad, double bid, GURL render_url)
    : success(true),
      ad(std::move(ad)),
      bid(bid),
      render_url(std::move(render_url)) {
  DCHECK_GT(this->bid, 0);
  DCHECK(this->render_url.is_valid());
}

BidderWorklet::ReportWinResult::ReportWinResult() = default;

BidderWorklet::ReportWinResult::ReportWinResult(GURL report_url)
    : success(true), report_url(std::move(report_url)) {
  DCHECK(this->report_url.is_valid());
}

BidderWorklet::BidderWorklet(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const GURL& script_source_url,
    AuctionV8Helper* v8_helper,
    LoadWorkletCallback load_worklet_callback)
    : v8_helper_(v8_helper) {
  DCHECK(load_worklet_callback);
  worklet_loader_ = std::make_unique<WorkletLoader>(
      url_loader_factory, script_source_url, v8_helper,
      base::BindOnce(&BidderWorklet::OnDownloadComplete, base::Unretained(this),
                     std::move(load_worklet_callback)));
}

BidderWorklet::~BidderWorklet() = default;

BidderWorklet::BidResult BidderWorklet::GenerateBid(
    const blink::mojom::InterestGroup& interest_group,
    const base::Optional<std::string>& auction_signals_json,
    const base::Optional<std::string>& per_buyer_signals_json,
    const std::vector<std::string>& trusted_bidding_signals_keys,
    TrustedBiddingSignals* trusted_bidding_signals,
    const std::string& browser_signal_top_window_hostname,
    const std::string& browser_signal_seller,
    int browser_signal_join_count,
    int browser_signal_bid_count,
    const std::vector<mojo::StructPtr<mojom::PreviousWin>>&
        browser_signal_prev_wins,
    base::Time auction_start_time) {
  // Can't make a bid without any ads.
  if (!interest_group.ads)
    return BidResult();

  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper_);
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
    return BidResult();
  }

  if (interest_group.ads) {
    std::vector<v8::Local<v8::Value>> ads_vector;
    for (const auto& ad : *interest_group.ads) {
      v8::Local<v8::Object> ad_object = v8::Object::New(isolate);
      gin::Dictionary ad_dict(isolate, ad_object);
      if (!ad_dict.Set("renderUrl", ad->render_url.spec()) ||
          (ad->metadata &&
           !v8_helper_->InsertJsonValue(context, "metadata", *ad->metadata,
                                        ad_object))) {
        return BidResult();
      }
      ads_vector.emplace_back(std::move(ad_object));
    }
    if (!v8_helper_->InsertValue(
            "ads",
            v8::Array::New(isolate, ads_vector.data(), ads_vector.size()),
            interest_group_object)) {
      return BidResult();
    }
  }

  args.push_back(std::move(interest_group_object));

  if (!AppendJsonValueOrNull(v8_helper_, context, auction_signals_json,
                             &args) ||
      !AppendJsonValueOrNull(v8_helper_, context, per_buyer_signals_json,
                             &args)) {
    return BidResult();
  }

  v8::Local<v8::Value> trusted_signals;
  if (!trusted_bidding_signals || trusted_bidding_signals_keys.empty()) {
    trusted_signals = v8::Null(isolate);
  } else {
    trusted_signals = trusted_bidding_signals->GetSignals(
        context, trusted_bidding_signals_keys);
  }
  args.push_back(trusted_signals);

  v8::Local<v8::Object> browser_signals = v8::Object::New(isolate);
  gin::Dictionary browser_signals_dict(isolate, browser_signals);
  if (!browser_signals_dict.Set("topWindowHostname",
                                browser_signal_top_window_hostname) ||
      !browser_signals_dict.Set("seller", browser_signal_seller) ||
      !browser_signals_dict.Set("joinCount", browser_signal_join_count) ||
      !browser_signals_dict.Set("bidCount", browser_signal_bid_count)) {
    return BidResult();
  }

  std::vector<v8::Local<v8::Value>> prev_wins_v8;
  for (const auto& prev_win : browser_signal_prev_wins) {
    int64_t time_delta = (auction_start_time - prev_win->time).InSeconds();
    // Don't give negative times if clock has changed since last auction win.
    // Clock changes do mean times can be out of numerical order, despite being
    // in chronological order.
    if (time_delta < 0)
      time_delta = 0;
    v8::Local<v8::Value> win_values[2];
    win_values[0] = v8::Number::New(isolate, time_delta);
    if (!v8_helper_->CreateValueFromJson(context, prev_win->ad_json)
             .ToLocal(&win_values[1])) {
      return BidResult();
    }
    prev_wins_v8.push_back(
        v8::Array::New(isolate, win_values, base::size(win_values)));
  }
  v8::Maybe<bool> result = browser_signals->Set(
      context, gin::StringToV8(isolate, "prevWins"),
      v8::Array::New(isolate, prev_wins_v8.data(), prev_wins_v8.size()));
  if (result.IsNothing() || !result.FromJust())
    return BidResult();

  args.push_back(browser_signals);

  v8::Local<v8::Value> generate_bid_result;
  if (!v8_helper_
           ->RunScript(context, worklet_script_->Get(isolate), "generateBid",
                       args)
           .ToLocal(&generate_bid_result) ||
      !generate_bid_result->IsObject()) {
    return BidResult();
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
    return BidResult();
  }

  if (bid <= 0 || std::isnan(bid) || !std::isfinite(bid))
    return BidResult();

  GURL render_url(render_url_string);
  if (!render_url.is_valid() || !render_url.SchemeIs(url::kHttpsScheme))
    return BidResult();

  // `render_url` must be in `ad_render_urls`.
  for (const auto& ad : *interest_group.ads) {
    if (render_url == ad->render_url)
      return BidResult(std::move(ad_json), bid, std::move(render_url));
  }
  return BidResult();
}

BidderWorklet::ReportWinResult BidderWorklet::ReportWin(
    const base::Optional<std::string>& auction_signals_json,
    const base::Optional<std::string>& per_buyer_signals_json,
    const std::string& seller_signals_json,
    const std::string& browser_signal_top_window_hostname,
    const url::Origin& browser_signal_interest_group_owner,
    const std::string& browser_signal_interest_group_name,
    const GURL& browser_signal_render_url,
    const std::string& browser_signal_ad_render_fingerprint,
    double browser_signal_bid) {
  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper_);
  v8::Isolate* isolate = v8_helper_->isolate();

  v8::Local<v8::ObjectTemplate> global_template =
      v8::ObjectTemplate::New(isolate);
  ReportBindings report_bindings(v8_helper_, global_template);

  // Short lived context, to avoid leaking data at global scope between either
  // repeated calls to this worklet, or to calls to any other worklet.
  v8::Local<v8::Context> context = v8_helper_->CreateContext(global_template);
  v8::Context::Scope context_scope(context);

  std::vector<v8::Local<v8::Value>> args;
  if (!AppendJsonValueOrNull(v8_helper_, context, auction_signals_json,
                             &args) ||
      !AppendJsonValueOrNull(v8_helper_, context, per_buyer_signals_json,
                             &args) ||
      !v8_helper_->AppendJsonValue(context, seller_signals_json, &args)) {
    return ReportWinResult();
  }

  v8::Local<v8::Object> browser_signals = v8::Object::New(isolate);
  gin::Dictionary browser_signals_dict(isolate, browser_signals);
  if (!browser_signals_dict.Set("topWindowHostname",
                                browser_signal_top_window_hostname) ||
      !browser_signals_dict.Set(
          "interestGroupOwner",
          browser_signal_interest_group_owner.Serialize()) ||
      !browser_signals_dict.Set("interestGroupName",
                                browser_signal_interest_group_name) ||
      !browser_signals_dict.Set("renderUrl",
                                browser_signal_render_url.spec()) ||
      !browser_signals_dict.Set("adRenderFingerprint",
                                browser_signal_ad_render_fingerprint) ||
      !browser_signals_dict.Set("bid", browser_signal_bid)) {
    return ReportWinResult();
  }
  args.push_back(browser_signals);

  // An empty return value indicates an exception was thrown. Any other return
  // value indicates no exception.
  if (v8_helper_
          ->RunScript(context, worklet_script_->Get(isolate), "reportWin", args)
          .IsEmpty()) {
    return ReportWinResult();
  }

  if (!report_bindings.report_url().is_valid())
    return ReportWinResult();

  return ReportWinResult(report_bindings.report_url());
}

void BidderWorklet::OnDownloadComplete(
    LoadWorkletCallback load_worklet_callback,
    std::unique_ptr<v8::Global<v8::UnboundScript>> worklet_script) {
  worklet_loader_.reset();
  worklet_script_ = std::move(worklet_script);
  std::move(load_worklet_callback).Run(worklet_script_ != nullptr);
}

}  // namespace auction_worklet
