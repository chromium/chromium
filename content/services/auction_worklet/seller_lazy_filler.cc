// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/seller_lazy_filler.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/strings/strcat.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/auction_worklet_util.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "third_party/blink/public/common/interest_group/ad_display_size_utils.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-json.h"
#include "v8/include/v8-template.h"

namespace auction_worklet {

namespace {

bool HasTimeouts(const blink::AuctionConfig::MaybePromiseBuyerTimeouts&
                     maybe_promise_buyer_timeouts) {
  DCHECK(!maybe_promise_buyer_timeouts.is_promise());
  return maybe_promise_buyer_timeouts.value().per_buyer_timeouts.has_value() ||
         maybe_promise_buyer_timeouts.value().all_buyers_timeout.has_value();
}

// Attempts to create an v8 Object from `maybe_promise_buyer_timeouts`. On fatal
// error, returns false. Otherwise, writes the result to
// `out_per_buyer_timeouts`.
bool CreatePerBuyerTimeoutsObject(
    v8::Isolate* isolate,
    const blink::AuctionConfig::MaybePromiseBuyerTimeouts&
        maybe_promise_buyer_timeouts,
    v8::Local<v8::Object>& out_per_buyer_timeouts) {
  // If there are no timeouts set at all, this should not be called.
  DCHECK(HasTimeouts(maybe_promise_buyer_timeouts));

  const blink::AuctionConfig::BuyerTimeouts& buyer_timeouts =
      maybe_promise_buyer_timeouts.value();

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

bool HasCurrencies(const blink::AuctionConfig::MaybePromiseBuyerCurrencies&
                       maybe_promise_buyer_currencies) {
  DCHECK(!maybe_promise_buyer_currencies.is_promise());
  return maybe_promise_buyer_currencies.value()
             .per_buyer_currencies.has_value() ||
         maybe_promise_buyer_currencies.value().all_buyers_currency.has_value();
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
  // If there are no currencies set at all, this should not be called.
  DCHECK(HasCurrencies(maybe_promise_buyer_currencies));

  const blink::AuctionConfig::BuyerCurrencies& buyer_currencies =
      maybe_promise_buyer_currencies.value();

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

bool InsertPrioritySignals(
    AuctionV8Helper* v8_helper,
    std::string_view key,
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

bool SetDictMember(v8::Isolate* isolate,
                   v8::Local<v8::Object> object,
                   const std::string& key,
                   v8::Local<v8::Value> v8_value) {
  v8::Maybe<bool> result = object->Set(isolate->GetCurrentContext(),
                                       gin::StringToV8(isolate, key), v8_value);
  return !result.IsNothing() && result.FromJust();
}

// Creates an AdSize object with a "width" and a "height" from a blink::AdSize.
// Returns false on failure.
bool CreateAdSizeObject(v8::Isolate* isolate,
                        const blink::AdSize& ad_size,
                        v8::Local<v8::Object>& ad_size_out) {
  DCHECK(blink::IsValidAdSize(ad_size));

  v8::Local<v8::Value> v8_width;
  if (!gin::TryConvertToV8(
          isolate,
          base::StrCat({base::NumberToString(ad_size.width),
                        blink::ConvertAdSizeUnitToString(ad_size.width_units)}),
          &v8_width)) {
    return false;
  }

  v8::Local<v8::Value> v8_height;
  if (!gin::TryConvertToV8(isolate,
                           base::StrCat({base::NumberToString(ad_size.height),
                                         blink::ConvertAdSizeUnitToString(
                                             ad_size.height_units)}),
                           &v8_height)) {
    return false;
  }

  ad_size_out = v8::Object::New(isolate);

  return SetDictMember(isolate, ad_size_out, "width", v8_width) &&
         SetDictMember(isolate, ad_size_out, "height", v8_height);
}

}  // namespace

SellerBrowserSignalsLazyFiller::SellerBrowserSignalsLazyFiller(
    AuctionV8Helper* v8_helper,
    AuctionV8Logger* v8_logger)
    : PersistedLazyFiller(v8_helper), v8_logger_(v8_logger) {}

void SellerBrowserSignalsLazyFiller::Reset() {
  browser_signal_render_url_ = nullptr;
}

bool SellerBrowserSignalsLazyFiller::FillInObject(
    const GURL& browser_signal_render_url,
    v8::Local<v8::Object> object) {
  browser_signal_render_url_ = &browser_signal_render_url;
  // TODO(crbug.com/40266734): Remove deprecated `renderUrl` alias.
  if (!DefineLazyAttribute(object, "renderUrl", &HandleDeprecatedRenderUrl)) {
    return false;
  }
  return true;
}

void SellerBrowserSignalsLazyFiller::HandleDeprecatedRenderUrl(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  SellerBrowserSignalsLazyFiller* self =
      GetSelf<SellerBrowserSignalsLazyFiller>(info);
  self->v8_logger_->LogConsoleWarning(
      "browserSignals.renderUrl is deprecated."
      " Please use browserSignals.renderURL instead.");

  AuctionV8Helper* v8_helper = self->v8_helper();
  v8::Isolate* isolate = v8_helper->isolate();
  v8::Local<v8::Value> value;
  if (self->browser_signal_render_url_ &&
      gin::TryConvertToV8(isolate, self->browser_signal_render_url_->spec(),
                          &value)) {
    SetResult(info, value);
  }
}

AuctionConfigLazyFiller::AuctionConfigLazyFiller(AuctionV8Helper* v8_helper,
                                                 AuctionV8Logger* v8_logger)
    : PersistedLazyFiller(v8_helper), v8_logger_(v8_logger) {}

void AuctionConfigLazyFiller::Reset() {
  auction_ad_config_non_shared_params_ = nullptr;
  decision_logic_url_ = nullptr;
  trusted_scoring_signals_url_ = nullptr;
}

bool AuctionConfigLazyFiller::FillInObject(
    const blink::AuctionConfig::NonSharedParams&
        auction_ad_config_non_shared_params,
    base::optional_ref<const GURL> decision_logic_url,
    base::optional_ref<const GURL> trusted_scoring_signals_url,
    v8::Local<v8::Object> object) {
  DCHECK(!auction_ad_config_non_shared_params_);
  auction_ad_config_non_shared_params_ = &auction_ad_config_non_shared_params;
  if (auction_ad_config_non_shared_params_->interest_group_buyers &&
      !DefineLazyAttribute(object, "interestGroupBuyers",
                           &HandleInterestGroupBuyers)) {
    return false;
  }

  DCHECK(!auction_ad_config_non_shared_params_
              ->deprecated_render_url_replacements.is_promise());
  if (!auction_ad_config_non_shared_params_->deprecated_render_url_replacements
           .value()
           .empty() &&
      !DefineLazyAttribute(object, "deprecatedRenderURLReplacements",
                           HandleDeprecatedRenderURLReplacements)) {
    return false;
  }

  DCHECK(!auction_ad_config_non_shared_params_->per_buyer_signals.is_promise());
  if (auction_ad_config_non_shared_params_->per_buyer_signals.value()
          .has_value() &&
      !DefineLazyAttribute(object, "perBuyerSignals", &HandlePerBuyerSignals)) {
    return false;
  }

  if (HasTimeouts(auction_ad_config_non_shared_params_->buyer_timeouts) &&
      !DefineLazyAttribute(object, "perBuyerTimeouts",
                           &HandlePerBuyerTimeouts)) {
    return false;
  }

  if (HasTimeouts(
          auction_ad_config_non_shared_params_->buyer_cumulative_timeouts) &&
      !DefineLazyAttribute(object, "perBuyerCumulativeTimeouts",
                           &HandlePerBuyerCumulativeTimeouts)) {
    return false;
  }

  if (HasCurrencies(auction_ad_config_non_shared_params_->buyer_currencies) &&
      !DefineLazyAttribute(object, "perBuyerCurrencies",
                           &HandlePerBuyerCurrencies)) {
    return false;
  }

  if ((auction_ad_config_non_shared_params_->per_buyer_priority_signals ||
       auction_ad_config_non_shared_params_->all_buyers_priority_signals) &&
      !DefineLazyAttribute(object, "perBuyerPrioritySignals",
                           &HandlePerBuyerPrioritySignals)) {
    return false;
  }

  if (auction_ad_config_non_shared_params_->requested_size &&
      !DefineLazyAttribute(object, "requestedSize", &HandleRequestedSize)) {
    return false;
  }

  if (auction_ad_config_non_shared_params_->all_slots_requested_sizes &&
      !DefineLazyAttribute(object, "allSlotsRequestedSizes",
                           &HandleAllSlotsRequestedSizes)) {
    return false;
  }

  if (decision_logic_url.has_value()) {
    decision_logic_url_ = &decision_logic_url.value();
    if (!DefineLazyAttribute(object, "decisionLogicUrl",
                             &HandleDeprecatedDecisionLogicUrl)) {
      return false;
    }
  }
  if (trusted_scoring_signals_url.has_value()) {
    trusted_scoring_signals_url_ = &trusted_scoring_signals_url.value();
    if (!DefineLazyAttribute(object, "trustedScoringSignalsUrl",
                             &HandleDeprecatedTrustedScoringSignalsUrl)) {
      return false;
    }
  }

  return true;
}

// static
void AuctionConfigLazyFiller::HandleInterestGroupBuyers(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  AuctionConfigLazyFiller* self = GetSelf<AuctionConfigLazyFiller>(info);
  AuctionV8Helper* v8_helper = self->v8_helper();
  v8::Isolate* isolate = v8_helper->isolate();

  // The first case is possible if a context is reused with fewer component
  // auctions; which may happen when multiple auctions share a worklet process.
  // The second is possible if an old object is kept around and has a field,
  // while the new config doesn't.
  if (!self->auction_ad_config_non_shared_params_ ||
      !self->auction_ad_config_non_shared_params_->interest_group_buyers) {
    return;
  }

  v8::LocalVector<v8::Value> interest_group_buyers(isolate);
  for (const url::Origin& buyer :
       *self->auction_ad_config_non_shared_params_->interest_group_buyers) {
    v8::Local<v8::String> v8_buyer;
    if (!v8_helper->CreateUtf8String(buyer.Serialize()).ToLocal(&v8_buyer)) {
      return;
    }
    interest_group_buyers.push_back(v8_buyer);
  }
  SetResult(info, v8::Array::New(isolate, interest_group_buyers.data(),
                                 interest_group_buyers.size()));
}

// static
void AuctionConfigLazyFiller::HandleDeprecatedRenderURLReplacements(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  AuctionConfigLazyFiller* self = GetSelf<AuctionConfigLazyFiller>(info);
  AuctionV8Helper* v8_helper = self->v8_helper();
  v8::Isolate* isolate = v8_helper->isolate();

  // The first case is possible if a context is reused with fewer component
  // auctions; which may happen when multiple auctions share a worklet process.
  // The second is possible if an old object is kept around and has a field,
  // while the new config doesn't.
  if (!self->auction_ad_config_non_shared_params_ ||
      self->auction_ad_config_non_shared_params_
          ->deprecated_render_url_replacements.value()
          .empty()) {
    return;
  }

  v8::Local<v8::Object> deprecated_render_url_replacements =
      v8::Object::New(isolate);
  for (const auto& kv : self->auction_ad_config_non_shared_params_
                            ->deprecated_render_url_replacements.value()) {
    v8::Local<v8::String> v8_replacement;
    if (!v8_helper->CreateUtf8String(kv.replacement).ToLocal(&v8_replacement)) {
      return;
    }
    if (!v8_helper->InsertValue(kv.match, v8_replacement,
                                deprecated_render_url_replacements)) {
      return;
    }
  }

  SetResult(info, deprecated_render_url_replacements);
}

// static
void AuctionConfigLazyFiller::HandlePerBuyerSignals(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  AuctionConfigLazyFiller* self = GetSelf<AuctionConfigLazyFiller>(info);
  AuctionV8Helper* v8_helper = self->v8_helper();
  v8::Isolate* isolate = v8_helper->isolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  // The first case is possible if a context is reused with fewer component
  // auctions; which may happen when multiple auctions share a worklet process.
  // The second is possible if an old object is kept around and has a field,
  // while the new config doesn't.
  if (!self->auction_ad_config_non_shared_params_ ||
      !self->auction_ad_config_non_shared_params_->per_buyer_signals.value()) {
    return;
  }

  v8::Local<v8::Object> per_buyer_value = v8::Object::New(isolate);
  for (const auto& kv :
       *self->auction_ad_config_non_shared_params_->per_buyer_signals.value()) {
    if (!v8_helper->InsertJsonValue(context, kv.first.Serialize(), kv.second,
                                    per_buyer_value)) {
      return;
    }
  }
  SetResult(info, per_buyer_value);
}

// static
void AuctionConfigLazyFiller::HandlePerBuyerTimeouts(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  AuctionConfigLazyFiller* self = GetSelf<AuctionConfigLazyFiller>(info);

  // The first case is possible if a context is reused with fewer component
  // auctions; which may happen when multiple auctions share a worklet process.
  // The second is possible if an old object is kept around and has a field,
  // while the new config doesn't.
  if (!self->auction_ad_config_non_shared_params_ ||
      !HasTimeouts(
          self->auction_ad_config_non_shared_params_->buyer_timeouts)) {
    return;
  }
  self->HandleTimeoutsImpl(
      info, self->auction_ad_config_non_shared_params_->buyer_timeouts);
}

// static
void AuctionConfigLazyFiller::HandlePerBuyerCumulativeTimeouts(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  AuctionConfigLazyFiller* self = GetSelf<AuctionConfigLazyFiller>(info);

  // The first case is possible if a context is reused with fewer component
  // auctions; which may happen when multiple auctions share a worklet process.
  // The second is possible if an old object is kept around and has a field,
  // while the new config doesn't.
  if (!self->auction_ad_config_non_shared_params_ ||
      !HasTimeouts(self->auction_ad_config_non_shared_params_
                       ->buyer_cumulative_timeouts)) {
    return;
  }
  self->HandleTimeoutsImpl(
      info,
      self->auction_ad_config_non_shared_params_->buyer_cumulative_timeouts);
}

void AuctionConfigLazyFiller::HandleTimeoutsImpl(
    const v8::PropertyCallbackInfo<v8::Value>& info,
    const blink::AuctionConfig::MaybePromiseBuyerTimeouts&
        maybe_promise_buyer_timeouts) {
  v8::Local<v8::Object> per_buyer_timeouts;
  if (CreatePerBuyerTimeoutsObject(v8_helper()->isolate(),
                                   maybe_promise_buyer_timeouts,
                                   per_buyer_timeouts)) {
    SetResult(info, per_buyer_timeouts);
  }
}

// static
void AuctionConfigLazyFiller::HandlePerBuyerCurrencies(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  AuctionConfigLazyFiller* self = GetSelf<AuctionConfigLazyFiller>(info);
  AuctionV8Helper* v8_helper = self->v8_helper();

  // The first case is possible if a context is reused with fewer component
  // auctions; which may happen when multiple auctions share a worklet process.
  // The second is possible if an old object is kept around and has a field,
  // while the new config doesn't.
  if (!self->auction_ad_config_non_shared_params_ ||
      !HasCurrencies(
          self->auction_ad_config_non_shared_params_->buyer_currencies)) {
    return;
  }

  v8::Local<v8::Object> per_buyer_currencies;
  if (CreatePerBuyerCurrenciesObject(
          v8_helper->isolate(),
          self->auction_ad_config_non_shared_params_->buyer_currencies,
          per_buyer_currencies)) {
    SetResult(info, per_buyer_currencies);
  }
}

// static
void AuctionConfigLazyFiller::HandlePerBuyerPrioritySignals(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  AuctionConfigLazyFiller* self = GetSelf<AuctionConfigLazyFiller>(info);
  AuctionV8Helper* v8_helper = self->v8_helper();
  v8::Isolate* isolate = v8_helper->isolate();

  // This is possible if a context is reused with fewer component
  // auctions; which may happen when multiple auctions share a worklet process.
  if (!self->auction_ad_config_non_shared_params_) {
    return;
  }

  if (!self->auction_ad_config_non_shared_params_->per_buyer_priority_signals &&
      !self->auction_ad_config_non_shared_params_
           ->all_buyers_priority_signals) {
    // Be consistent with other fields and return undefined if we are not
    // expecting the field to be set at all.
    return;
  }

  v8::Local<v8::Object> per_buyer_priority_signals = v8::Object::New(isolate);
  if (self->auction_ad_config_non_shared_params_->per_buyer_priority_signals) {
    for (const auto& kv : *self->auction_ad_config_non_shared_params_
                               ->per_buyer_priority_signals) {
      if (!InsertPrioritySignals(v8_helper, kv.first.Serialize(), kv.second,
                                 per_buyer_priority_signals)) {
        return;
      }
    }
  }
  if (self->auction_ad_config_non_shared_params_->all_buyers_priority_signals) {
    if (!InsertPrioritySignals(v8_helper, "*",
                               *self->auction_ad_config_non_shared_params_
                                    ->all_buyers_priority_signals,
                               per_buyer_priority_signals)) {
      return;
    }
  }
  SetResult(info, per_buyer_priority_signals);
}

// static
void AuctionConfigLazyFiller::HandleRequestedSize(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  AuctionConfigLazyFiller* self = GetSelf<AuctionConfigLazyFiller>(info);
  AuctionV8Helper* v8_helper = self->v8_helper();
  v8::Isolate* isolate = v8_helper->isolate();

  // The first case is possible if a context is reused with fewer component
  // auctions; which may happen when multiple auctions share a worklet process.
  // The second is possible if an old object is kept around and has a field,
  // while the new config doesn't.
  if (!self->auction_ad_config_non_shared_params_ ||
      !CanSetAdSize(
          self->auction_ad_config_non_shared_params_->requested_size)) {
    return;
  }

  v8::Local<v8::Object> size_object;
  if (CreateAdSizeObject(
          isolate, *self->auction_ad_config_non_shared_params_->requested_size,
          size_object)) {
    SetResult(info, size_object);
  }
}

// static
void AuctionConfigLazyFiller::HandleAllSlotsRequestedSizes(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  AuctionConfigLazyFiller* self = GetSelf<AuctionConfigLazyFiller>(info);
  AuctionV8Helper* v8_helper = self->v8_helper();
  v8::Isolate* isolate = v8_helper->isolate();

  // The first case is possible if a context is reused with fewer component
  // auctions; which may happen when multiple auctions share a worklet
  // process. The second is possible if an old object is kept around and has
  // a field, while the new config doesn't.
  if (!self->auction_ad_config_non_shared_params_ ||
      !self->auction_ad_config_non_shared_params_->all_slots_requested_sizes) {
    return;
  }

  v8::LocalVector<v8::Value> size_vector(isolate);
  for (const auto& slot_size :
       *self->auction_ad_config_non_shared_params_->all_slots_requested_sizes) {
    v8::Local<v8::Object> size_object;
    if (!CreateAdSizeObject(isolate, slot_size, size_object)) {
      return;
    }
    size_vector.push_back(std::move(size_object));
  }

  SetResult(info,
            v8::Array::New(isolate, size_vector.data(), size_vector.size()));
}

void AuctionConfigLazyFiller::HandleDeprecatedDecisionLogicUrl(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  AuctionConfigLazyFiller* self = GetSelf<AuctionConfigLazyFiller>(info);
  self->v8_logger_->LogConsoleWarning(
      "auctionConfig.decisionLogicUrl is deprecated."
      " Please use auctionConfig.decisionLogicURL instead.");

  AuctionV8Helper* v8_helper = self->v8_helper();
  v8::Isolate* isolate = v8_helper->isolate();
  v8::Local<v8::Value> value;
  if (self->decision_logic_url_ &&
      gin::TryConvertToV8(isolate, self->decision_logic_url_->spec(), &value)) {
    SetResult(info, value);
  }
}

void AuctionConfigLazyFiller::HandleDeprecatedTrustedScoringSignalsUrl(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  AuctionConfigLazyFiller* self = GetSelf<AuctionConfigLazyFiller>(info);
  self->v8_logger_->LogConsoleWarning(
      "auctionConfig.trustedScoringSignalsUrl is deprecated."
      " Please use auctionConfig.trustedScoringSignalsURL instead.");

  AuctionV8Helper* v8_helper = self->v8_helper();
  v8::Isolate* isolate = v8_helper->isolate();
  v8::Local<v8::Value> value;
  if (self->trusted_scoring_signals_url_ &&
      gin::TryConvertToV8(isolate, self->trusted_scoring_signals_url_->spec(),
                          &value)) {
    SetResult(info, value);
  }
}

}  // namespace auction_worklet
