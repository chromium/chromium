// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/bidder_lazy_filler.h"

#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-json.h"

namespace auction_worklet {

namespace {

// Creates a V8 array containing information about the passed in previous wins.
// Array is sorted by time, earliest wins first. Modifies order of `prev_wins`
// input vector. This should should be harmless, since each list of previous
// wins is only used for a single bid in a single auction, and its order is
// unspecified, anyways.
v8::MaybeLocal<v8::Value> CreatePrevWinsArray(
    PrevWinsType prev_wins_type,
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
  v8::Local<v8::String> render_url_key =
      v8_helper->CreateStringFromLiteral("renderURL");
  v8::Local<v8::String> metadata_key =
      v8_helper->CreateStringFromLiteral("metadata");
  for (const auto& prev_win : prev_wins) {
    base::TimeDelta time_delta = auction_start_time - prev_win->time;
    int time_delta_int;
    switch (prev_wins_type) {
      case PrevWinsType::kSeconds:
        time_delta_int = time_delta.InSeconds();
        break;
      case PrevWinsType::kMilliseconds:
        // Truncate to the nearest second, rather than providing the result in
        // millisecond granularity.
        time_delta_int = time_delta.InSeconds() * 1000;
        break;
    }
    // Don't give negative times if clock has changed since last auction win.
    if (time_delta_int < 0) {
      time_delta_int = 0;
    }
    v8::Local<v8::Value> win_values[2];
    win_values[0] = v8::Number::New(isolate, time_delta_int);
    if (!v8_helper->CreateValueFromJson(context, prev_win->ad_json)
             .ToLocal(&win_values[1])) {
      return v8::MaybeLocal<v8::Value>();
    }
    DCHECK(win_values[1]->IsObject());
    v8::Local<v8::Object> prev_ad = win_values[1].As<v8::Object>();
    // TODO(crbug.com/1448936): Remove this condition logic when we can assume
    // it is true (30 days after switching to a Chrome version at least this
    // recent).
    // If prev_ad has "renderURL" instead of "render_url" it must be the newer
    // version. In that version the metadata is still kept as serialized JSON
    // and needs to be parsed again. If it has metadata, parse it.
    // We also need to provide the render URL in a "render_url" field for
    // backward compatibility.
    // TODO(crbug.com/1441988): Remove render_url alias when it is no longer
    // needed for compatibility.
    if (prev_ad->Has(context, render_url_key).FromMaybe(false)) {
      v8::Local<v8::Value> serialized_metadata;
      v8::Local<v8::Value> metadata;
      if (prev_ad->Get(context, metadata_key).ToLocal(&serialized_metadata) &&
          serialized_metadata->IsString()) {
        if (!v8::JSON::Parse(context, serialized_metadata.As<v8::String>())
                 .ToLocal(&metadata) ||
            !prev_ad->Set(context, metadata_key, metadata).FromMaybe(false)) {
          return v8::MaybeLocal<v8::Value>();
        }
      }
      // For compatibility we need to provide the render URL as the "render_url"
      // attribute in addition to the "renderURL" attribute.
      v8::Local<v8::Value> render_url;
      if (!prev_ad->Get(context, render_url_key).ToLocal(&render_url) ||
          !prev_ad
               ->Set(context, v8_helper->CreateStringFromLiteral("render_url"),
                     render_url)
               .FromMaybe(false)) {
        return v8::MaybeLocal<v8::Value>();
      }
    }
    prev_wins_v8.push_back(
        v8::Array::New(isolate, win_values, std::size(win_values)));
  }
  return v8::Array::New(isolate, prev_wins_v8.data(), prev_wins_v8.size());
}

}  // namespace

InterestGroupLazyFiller::InterestGroupLazyFiller(AuctionV8Helper* v8_helper)
    : LazyFiller(v8_helper) {}

void InterestGroupLazyFiller::ReInitialize(
    const mojom::BidderWorkletNonSharedParams*
        bidder_worklet_non_shared_params) {
  bidder_worklet_non_shared_params_ = bidder_worklet_non_shared_params;
}

bool InterestGroupLazyFiller::FillInObject(v8::Local<v8::Object> object) {
  if (bidder_worklet_non_shared_params_->user_bidding_signals &&
      !DefineLazyAttribute(object, "userBiddingSignals",
                           &HandleUserBiddingSignals)) {
    return false;
  }
  if (bidder_worklet_non_shared_params_->trusted_bidding_signals_keys &&
      !DefineLazyAttribute(object, "trustedBiddingSignalsKeys",
                           &HandleTrustedBiddingSignalsKeys)) {
    return false;
  }
  if (bidder_worklet_non_shared_params_->priority_vector &&
      !DefineLazyAttribute(object, "priorityVector", &HandlePriorityVector)) {
    return false;
  }
  return true;
}

void InterestGroupLazyFiller::Reset() {
  bidder_worklet_non_shared_params_ = nullptr;
}

// static
void InterestGroupLazyFiller::HandleUserBiddingSignals(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  InterestGroupLazyFiller* self = GetSelf<InterestGroupLazyFiller>(info);
  AuctionV8Helper* v8_helper = self->v8_helper();
  v8::Isolate* isolate = v8_helper->isolate();
  v8::Local<v8::Value> value;
  v8::TryCatch try_catch(isolate);
  if (self->bidder_worklet_non_shared_params_ &&
      self->bidder_worklet_non_shared_params_->user_bidding_signals &&
      v8_helper
          ->CreateValueFromJson(
              isolate->GetCurrentContext(),
              *self->bidder_worklet_non_shared_params_->user_bidding_signals)
          .ToLocal(&value)) {
    SetResult(info, value);
  } else {
    try_catch.Reset();
    SetResult(info, v8::Null(isolate));
  }
}

// static
void InterestGroupLazyFiller::HandleTrustedBiddingSignalsKeys(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  InterestGroupLazyFiller* self = GetSelf<InterestGroupLazyFiller>(info);
  AuctionV8Helper* v8_helper = self->v8_helper();
  v8::Isolate* isolate = v8_helper->isolate();
  if (self->bidder_worklet_non_shared_params_ &&
      self->bidder_worklet_non_shared_params_->trusted_bidding_signals_keys) {
    std::vector<v8::Local<v8::Value>> trusted_bidding_signals_keys;
    for (const auto& key : *self->bidder_worklet_non_shared_params_
                                ->trusted_bidding_signals_keys) {
      v8::Local<v8::Value> key_value;
      if (!v8_helper->CreateUtf8String(key).ToLocal(&key_value)) {
        SetResult(info, v8::Null(isolate));
        return;
      }
      trusted_bidding_signals_keys.emplace_back(std::move(key_value));
    }

    SetResult(info, v8::Array::New(isolate, trusted_bidding_signals_keys.data(),
                                   trusted_bidding_signals_keys.size()));
  } else {
    SetResult(info, v8::Null(isolate));
  }
}

// static
void InterestGroupLazyFiller::HandlePriorityVector(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  InterestGroupLazyFiller* self = GetSelf<InterestGroupLazyFiller>(info);
  AuctionV8Helper* v8_helper = self->v8_helper();
  v8::Isolate* isolate = v8_helper->isolate();
  if (self->bidder_worklet_non_shared_params_ &&
      self->bidder_worklet_non_shared_params_->priority_vector) {
    v8::Local<v8::Object> priority_vector = v8::Object::New(isolate);
    gin::Dictionary priority_vector_dict(isolate, priority_vector);
    for (const auto& pair :
         *self->bidder_worklet_non_shared_params_->priority_vector) {
      if (!priority_vector_dict.Set(pair.first, pair.second)) {
        SetResult(info, v8::Null(isolate));
        return;
      }
    }
    SetResult(info, priority_vector);
  } else {
    SetResult(info, v8::Null(isolate));
  }
}

BiddingBrowserSignalsLazyFiller::BiddingBrowserSignalsLazyFiller(
    AuctionV8Helper* v8_helper)
    : LazyFiller(v8_helper) {}

void BiddingBrowserSignalsLazyFiller::ReInitialize(
    mojom::BiddingBrowserSignals* bidder_browser_signals,
    base::Time auction_start_time) {
  bidder_browser_signals_ = bidder_browser_signals;
  auction_start_time_ = auction_start_time;
}

bool BiddingBrowserSignalsLazyFiller::FillInObject(
    v8::Local<v8::Object> object) {
  if (!DefineLazyAttribute(object, "prevWins", &HandlePrevWins)) {
    return false;
  }
  if (!DefineLazyAttribute(object, "prevWinsMs", &HandlePrevWinsMs)) {
    return false;
  }
  return true;
}

void BiddingBrowserSignalsLazyFiller::Reset() {
  bidder_browser_signals_ = nullptr;
}

// static
void BiddingBrowserSignalsLazyFiller::HandlePrevWins(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  HandlePrevWinsInternal(name, info, PrevWinsType::kSeconds);
}

// static
void BiddingBrowserSignalsLazyFiller::HandlePrevWinsMs(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  HandlePrevWinsInternal(name, info, PrevWinsType::kMilliseconds);
}

// TODO(crbug.com/1451034): Clean up support for deprecated seconds-based
// version after API users migrate, and remove this indirection function.
// static
void BiddingBrowserSignalsLazyFiller::HandlePrevWinsInternal(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info,
    PrevWinsType prev_wins_type) {
  BiddingBrowserSignalsLazyFiller* self =
      GetSelf<BiddingBrowserSignalsLazyFiller>(info);
  AuctionV8Helper* v8_helper = self->v8_helper();
  v8::Isolate* isolate = v8_helper->isolate();
  v8::Local<v8::Value> value;
  if (self->bidder_browser_signals_ &&
      CreatePrevWinsArray(
          prev_wins_type, v8_helper, isolate->GetCurrentContext(),
          self->auction_start_time_, self->bidder_browser_signals_->prev_wins)
          .ToLocal(&value)) {
    SetResult(info, value);
  } else {
    SetResult(info, v8::Null(isolate));
  }
}

}  // namespace auction_worklet
