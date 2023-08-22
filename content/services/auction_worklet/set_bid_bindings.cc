// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/set_bid_bindings.h"

#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/bidder_worklet.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "content/services/auction_worklet/webidl_compat.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/common/interest_group/ad_auction_currencies.h"
#include "third_party/blink/public/common/interest_group/ad_display_size_utils.h"
#include "url/gurl.h"
#include "url/url_constants.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-function.h"

namespace auction_worklet {

namespace {

// Checks that `url` is a valid URL and is in `ads`. Appends an error to
// `out_errors` if not. `error_prefix` is used in output error messages
// only.
bool IsAllowedAdUrl(
    const GURL& url,
    std::string& error_prefix,
    const char* argument_name,
    const base::RepeatingCallback<bool(const GURL&)>& is_excluded,
    const std::vector<blink::InterestGroup::Ad>& ads,
    std::string& out_error) {
  if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme)) {
    out_error = base::StrCat({error_prefix, "bid ", argument_name, " URL '",
                              url.possibly_invalid_spec(),
                              "' isn't a valid https:// URL."});
    return false;
  }

  for (const auto& ad : ads) {
    if (is_excluded.Run(ad.render_url)) {
      continue;
    }
    if (url == ad.render_url)
      return true;
  }
  out_error = base::StrCat({error_prefix, "bid ", argument_name, " URL '",
                            url.possibly_invalid_spec(),
                            "' isn't one of the registered creative URLs."});
  return false;
}

struct AdRender {
  std::string url;
  absl::optional<std::string> width;
  absl::optional<std::string> height;
};

// Handles conversion of (DOMString or AdRender) IDL type.
bool ConvertDomStringOrAdRender(
    AuctionV8Helper* v8_helper,
    AuctionV8Helper::TimeLimitScope& time_limit_scope,
    const std::string& error_prefix,
    v8::Local<v8::Value> value,
    AdRender& out,
    DictConverter& propagate_errors_to) {
  if (value->IsString()) {
    bool ok = gin::ConvertFromV8(v8_helper->isolate(), value, &out.url);
    DCHECK(ok);  // Shouldn't fail since it's known to be String.
    return true;
  }

  DictConverter convert_ad_render(v8_helper, time_limit_scope, error_prefix,
                                  value);
  // This is alphabetical, since that's how dictionaries work.
  if (!convert_ad_render.GetOptional("height", out.height) ||
      !convert_ad_render.GetRequired("url", out.url) ||
      !convert_ad_render.GetOptional("width", out.width)) {
    propagate_errors_to.SetStatus(convert_ad_render.TakeStatus());
    return false;
  }
  return true;
}

// Parses an AdRender, either a top-level value of render: field in bid or
// as part of its components array. This is meant to run on the output
// of ConvertDomStringOrAdRender, which has already converted the string form
// into a struct matching the dictionary form, and represents the semantics
// step of the checking, which happens after all the IDL conversions take place.
//
// Return whether the parse is successful.
//
// The dictionary can be in one of two forms:
// 1. Contains only the url field:
//      {url: "https://example.test/"}
// 2. Contains the url and both width and height fields:
//      {url: "https://example.test/", width: "100sw", height: "50px"}
// Any other fields will be ignored.
//
// The size units are allowed to be specified as:
// 1. "px": pixels.
// 2. "sw": screenwidth.
//
// Note the parse is still considered successful even if the size unit ends up
// being invalid, for example:
// {url: "https://example.test/", width: "100ft", height: "50in"}
//
// This will be immediately handled by `IsValidAdSize`, so we know the reason
// for the failure in order to emit more accurate error messages.
bool TryToParseUrlWithSize(AuctionV8Helper* v8_helper,
                           AuctionV8Helper::TimeLimitScope& time_limit_scope,
                           const std::string& error_prefix,
                           AdRender& value,
                           std::string& ad_url_out,
                           absl::optional<blink::AdSize>& size_out,
                           std::string& error_out) {
  // Either no dimensions must be specified, or both.
  if (value.width.has_value() != value.height.has_value()) {
    error_out = base::StrCat(
        {error_prefix, "ads that specify dimensions must specify both."});
    return false;
  }

  ad_url_out = std::move(value.url);
  if (value.width.has_value()) {
    auto [width_val, width_units] = blink::ParseAdSizeString(*value.width);
    auto [height_val, height_units] = blink::ParseAdSizeString(*value.height);

    size_out = blink::AdSize(width_val, width_units, height_val, height_units);
  } else {
    size_out = absl::nullopt;
  }

  return true;
}

}  // namespace

mojom::BidderWorkletBidPtr SetBidBindings::TakeBid() {
  DCHECK(has_bid());
  // Set `bid_duration` here instead of in SetBid(), so it can include the
  // entire script execution time.
  bid_->bid_duration = base::TimeTicks::Now() - start_;
  return std::move(bid_);
}

SetBidBindings::SetBidBindings(AuctionV8Helper* v8_helper)
    : v8_helper_(v8_helper) {}

SetBidBindings::~SetBidBindings() = default;

void SetBidBindings::ReInitialize(
    base::TimeTicks start,
    bool has_top_level_seller_origin,
    const mojom::BidderWorkletNonSharedParams* bidder_worklet_non_shared_params,
    const absl::optional<blink::AdCurrency>& per_buyer_currency,
    base::RepeatingCallback<bool(const GURL&)> is_ad_excluded,
    base::RepeatingCallback<bool(const GURL&)> is_component_ad_excluded) {
  DCHECK(bidder_worklet_non_shared_params->ads.has_value());
  start_ = start;
  has_top_level_seller_origin_ = has_top_level_seller_origin;
  bidder_worklet_non_shared_params_ = bidder_worklet_non_shared_params;
  per_buyer_currency_ = per_buyer_currency;
  is_ad_excluded_ = std::move(is_ad_excluded);
  is_component_ad_excluded_ = std::move(is_component_ad_excluded);
}

void SetBidBindings::AttachToContext(v8::Local<v8::Context> context) {
  v8::Local<v8::External> v8_this =
      v8::External::New(v8_helper_->isolate(), this);
  v8::Local<v8::Function> v8_function =
      v8::Function::New(context, &SetBidBindings::SetBid, v8_this)
          .ToLocalChecked();
  context->Global()
      ->Set(context, v8_helper_->CreateStringFromLiteral("setBid"), v8_function)
      .Check();
}

void SetBidBindings::Reset() {
  bid_.reset();
  // Make sure we don't keep any dangling references to auction input.
  bidder_worklet_non_shared_params_ = nullptr;
  reject_reason_ = mojom::RejectReason::kNotAvailable;
  per_buyer_currency_ = absl::nullopt;
  is_ad_excluded_.Reset();
  is_component_ad_excluded_.Reset();
}

IdlConvert::Status SetBidBindings::SetBidImpl(
    v8::Local<v8::Value> generate_bid_result,
    std::string error_prefix) {
  v8::Isolate* isolate = v8_helper_->isolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  bid_.reset();

  DCHECK(bidder_worklet_non_shared_params_)
      << "ReInitialize() must be called before each use";

  struct GenerateBidOutput {
    absl::optional<double> bid;
    absl::optional<std::string> bid_currency;
    absl::optional<AdRender> render;
    absl::optional<v8::Local<v8::Value>> ad;
    absl::optional<std::vector<AdRender>> ad_components;
    absl::optional<double> ad_cost;
    absl::optional<UnrestrictedDouble> modeling_signals;
    absl::optional<bool> allow_component_auction;
  } idl;

  auto components_exist = base::BindOnce(
      [](GenerateBidOutput& idl) { idl.ad_components.emplace(); },
      std::ref(idl));

  AuctionV8Helper::TimeLimitScope time_limit_scope(v8_helper_->GetTimeLimit());
  DictConverter convert_set_bid(v8_helper_.get(), time_limit_scope,
                                error_prefix, generate_bid_result);

  scoped_refptr<AuctionV8Helper> ref_v8_helper(v8_helper_.get());
  std::string render_prefix = base::StrCat({error_prefix, "'render': "});
  std::string components_prefix =
      base::StrCat({error_prefix, "adComponents entry: "});
  auto collect_components = base::BindRepeating(
      [](scoped_refptr<AuctionV8Helper> v8_helper,
         AuctionV8Helper::TimeLimitScope& time_limit_scope,
         DictConverter& convert_set_bid, const std::string& error_prefix,
         GenerateBidOutput& idl, v8::Local<v8::Value> component) -> bool {
        AdRender converted_component;
        if (ConvertDomStringOrAdRender(v8_helper.get(), time_limit_scope,
                                       error_prefix, component,
                                       converted_component, convert_set_bid)) {
          idl.ad_components->push_back(std::move(converted_component));
          return true;
        } else {
          // ConvertDomStringOrAdRender already forwarded the error for us.
          return false;
        }
      },
      ref_v8_helper, std::ref(time_limit_scope), std::ref(convert_set_bid),
      std::cref(components_prefix), std::ref(idl));

  convert_set_bid.GetOptional("ad", idl.ad);
  convert_set_bid.GetOptionalSequence(
      "adComponents", std::move(components_exist), collect_components);
  convert_set_bid.GetOptional("adCost", idl.ad_cost);
  convert_set_bid.GetOptional("allowComponentAuction",
                              idl.allow_component_auction);
  convert_set_bid.GetOptional("bid", idl.bid);
  convert_set_bid.GetOptional("bidCurrency", idl.bid_currency);
  convert_set_bid.GetOptional("modelingSignals", idl.modeling_signals);

  absl::optional<v8::Local<v8::Value>> render_value;
  if (convert_set_bid.GetOptional("render", render_value) &&
      render_value.has_value()) {
    idl.render.emplace();
    ConvertDomStringOrAdRender(v8_helper_.get(), time_limit_scope,
                               render_prefix, *render_value, *idl.render,
                               convert_set_bid);
  }

  if (convert_set_bid.is_failed()) {
    return convert_set_bid.TakeStatus();
  }

  if (!idl.allow_component_auction.has_value()) {
    idl.allow_component_auction.emplace(false);
  }

  if (!idl.bid.has_value() || *idl.bid <= 0.0) {
    // Not an error, just no bid.
    return IdlConvert::Status::MakeSuccess();
  }

  if (!idl.render.has_value()) {
    return IdlConvert::Status::MakeErrorMessage(base::StrCat(
        {error_prefix, "'render' is required when making a bid."}));
  }

  absl::optional<blink::AdCurrency> bid_currency;
  if (idl.bid_currency.has_value()) {
    if (!blink::IsValidAdCurrencyCode(*idl.bid_currency)) {
      reject_reason_ = mojom::RejectReason::kWrongGenerateBidCurrency;
      return IdlConvert::Status::MakeErrorMessage(
          base::StringPrintf("%sbidCurrency of '%s' is not a currency code.",
                             error_prefix.c_str(), idl.bid_currency->c_str()));
    }
    bid_currency = blink::AdCurrency::From(*idl.bid_currency);
  }

  if (!blink::VerifyAdCurrencyCode(per_buyer_currency_, bid_currency)) {
    reject_reason_ = mojom::RejectReason::kWrongGenerateBidCurrency;
    return IdlConvert::Status::MakeErrorMessage(base::StringPrintf(
        "%sbidCurrency mismatch; returned '%s', expected '%s'.",
        error_prefix.c_str(), blink::PrintableAdCurrency(bid_currency).c_str(),
        blink::PrintableAdCurrency(per_buyer_currency_).c_str()));
  }

  // "ad" field is optional, but if present, must be possible to convert to
  // JSON.
  std::string ad_json;
  if (!idl.ad.has_value()) {
    ad_json = "null";
  } else {
    if (!v8_helper_->ExtractJson(context, *idl.ad, &ad_json)) {
      return IdlConvert::Status::MakeErrorMessage(
          base::StrCat({error_prefix, "bid has invalid ad value."}));
    }
  }

  if (has_top_level_seller_origin_) {
    if (!*idl.allow_component_auction) {
      return IdlConvert::Status::MakeErrorMessage(
          base::StrCat({error_prefix,
                        "bid does not have allowComponentAuction "
                        "set to true. Bid dropped from component auction."}));
    }
  }

  absl::optional<double> modeling_signals;
  if (idl.modeling_signals.has_value() && idl.modeling_signals->number >= 0 &&
      idl.modeling_signals->number < (1 << 12)) {
    modeling_signals = idl.modeling_signals->number;
  }

  std::string render_url_string;
  absl::optional<blink::AdSize> render_size = absl::nullopt;
  std::string error_msg;
  if (!TryToParseUrlWithSize(v8_helper_.get(), time_limit_scope, render_prefix,
                             *idl.render, render_url_string, render_size,
                             error_msg)) {
    return IdlConvert::Status::MakeErrorMessage(std::move(error_msg));
  }

  if (render_size.has_value() && !IsValidAdSize(render_size.value())) {
    return IdlConvert::Status::MakeErrorMessage(
        base::StrCat({error_prefix, "bid has invalid size for render ad."}));
  }

  GURL render_url(render_url_string);
  if (!IsAllowedAdUrl(render_url, error_prefix, "render", is_ad_excluded_,
                      bidder_worklet_non_shared_params_->ads.value(),
                      error_msg)) {
    return IdlConvert::Status::MakeErrorMessage(std::move(error_msg));
  }

  absl::optional<std::vector<blink::AdDescriptor>> ad_component_descriptors;
  if (idl.ad_components.has_value()) {
    if (!bidder_worklet_non_shared_params_->ad_components.has_value()) {
      return IdlConvert::Status::MakeErrorMessage(
          base::StrCat({error_prefix,
                        "bid contains adComponents but InterestGroup has no "
                        "adComponents."}));
    }

    // We want < rather than <= here so the semantic check is testable and not
    // hidden by implementation details of DictConverter.
    static_assert(blink::kMaxAdAuctionAdComponents <
                  DictConverter::kSequenceLengthLimit);

    if (idl.ad_components->size() > blink::kMaxAdAuctionAdComponents) {
      return IdlConvert::Status::MakeErrorMessage(base::StringPrintf(
          "%sbid adComponents with over %zu items.", error_prefix.c_str(),
          blink::kMaxAdAuctionAdComponents));
    }

    ad_component_descriptors.emplace();
    for (AdRender& component : *idl.ad_components) {
      std::string ad_component_url_string;
      absl::optional<blink::AdSize> ad_component_size = absl::nullopt;
      if (!TryToParseUrlWithSize(
              v8_helper_.get(), time_limit_scope, components_prefix, component,
              ad_component_url_string, ad_component_size, error_msg)) {
        return IdlConvert::Status::MakeErrorMessage(std::move(error_msg));
      }

      if (ad_component_size.has_value() &&
          !IsValidAdSize(ad_component_size.value())) {
        return IdlConvert::Status::MakeErrorMessage(base::StrCat(
            {error_prefix,
             "bid adComponents have invalid size for ad component."}));
      }

      GURL ad_component_url(ad_component_url_string);
      if (!IsAllowedAdUrl(
              ad_component_url, error_prefix, "adComponents",
              is_component_ad_excluded_,
              bidder_worklet_non_shared_params_->ad_components.value(),
              error_msg)) {
        return IdlConvert::Status::MakeErrorMessage(std::move(error_msg));
      }
      ad_component_descriptors->emplace_back(std::move(ad_component_url),
                                             std::move(ad_component_size));
    }
  }

  // `bid_duration` needs to include the entire time the bid script took to run,
  // including the time from the last setBid() call to when the bidder worklet
  // timed out, if the worklet did time out. So `bid_duration` is calculated
  // when ownership of the bid is taken by the caller, instead of here.
  bid_ = mojom::BidderWorkletBid::New(
      std::move(ad_json), *idl.bid, std::move(bid_currency),
      std::move(idl.ad_cost), blink::AdDescriptor(render_url, render_size),
      std::move(ad_component_descriptors),
      static_cast<absl::optional<uint16_t>>(modeling_signals),
      /*bid_duration=*/base::TimeDelta());
  return IdlConvert::Status::MakeSuccess();
}

// static
void SetBidBindings::SetBid(const v8::FunctionCallbackInfo<v8::Value>& args) {
  SetBidBindings* bindings =
      static_cast<SetBidBindings*>(v8::External::Cast(*args.Data())->Value());
  AuctionV8Helper* v8_helper = bindings->v8_helper_;

  v8::Local<v8::Value> argument_value;
  // Treat no arguments as an undefined argument, which should clear the bid.
  if (args.Length() < 1) {
    argument_value = v8::Undefined(v8_helper->isolate());
  } else {
    argument_value = args[0];
  }

  IdlConvert::Status status =
      bindings->SetBidImpl(argument_value, /*error_prefix=*/"");
  status.PropagateErrorsToV8(v8_helper);
}

}  // namespace auction_worklet
