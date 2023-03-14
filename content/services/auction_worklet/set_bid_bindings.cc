// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/set_bid_bindings.h"

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
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/common/interest_group/ad_display_size_utils.h"
#include "url/gurl.h"
#include "url/url_constants.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-template.h"

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
    std::vector<std::string>& out_errors) {
  if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme)) {
    out_errors.push_back(base::StrCat({error_prefix, "bid ", argument_name,
                                       " URL '", url.possibly_invalid_spec(),
                                       "' isn't a valid https:// URL."}));
    return false;
  }

  for (const auto& ad : ads) {
    if (is_excluded.Run(ad.render_url)) {
      continue;
    }
    if (url == ad.render_url)
      return true;
  }
  out_errors.push_back(
      base::StrCat({error_prefix, "bid ", argument_name, " URL '",
                    url.possibly_invalid_spec(),
                    "' isn't one of the registered creative URLs."}));
  return false;
}

// Parse the field corresponds to 'render' or the entry in 'adComponents' array.
// Return whether the parse is successful.
// The JavaScript object can be in one of two forms:
// 1. Contains only the url field:
//      {url: "https://example.test/"}
// 2. Contains the url and both width and height fields:
//      {url: "https://example.test/", width: "100sw", height: "50px"}
//
// The size units are allowed to be specified as:
// 1. "px": pixels.
// 2. "sw": screenwidth.
//
// Note the parse is still considered successful even if the size unit ends up
// being invalid, for example:
// {url: "https://example.test/", width: "100ft", height: "50in"}
//
// This will be immediately handled by `HoldsInvalidSize`, so we know the reason
// for the failure in order to emit more accurate error messages.
bool TryToParseUrlWithSize(v8::Isolate* isolate,
                           const v8::Local<v8::Value>& value,
                           std::string& ad_url,
                           absl::optional<blink::AdSize>& size) {
  if (!value->IsObject()) {
    return false;
  }

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  gin::Dictionary dict(isolate, value.As<v8::Object>());
  if (!dict.Get("url", &ad_url)) {
    return false;
  }

  // The object being parse must either:
  // 1. contain the 'url' field only.
  // 2. contain the 'url', 'width' and 'height' fields.
  uint32_t properties_count = value.As<v8::Object>()
                                  ->GetPropertyNames(context)
                                  .ToLocalChecked()
                                  ->Length();
  if (properties_count == 1u) {
    return true;
  }
  if (properties_count != 3u) {
    return false;
  }

  std::string render_width;
  std::string render_height;
  if (!dict.Get("width", &render_width) ||
      !dict.Get("height", &render_height)) {
    return false;
  }

  auto [width_val, width_units] = blink::ParseAdSizeString(render_width);
  auto [height_val, height_units] = blink::ParseAdSizeString(render_height);

  size = blink::AdSize(width_val, width_units, height_val, height_units);

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
    base::RepeatingCallback<bool(const GURL&)> is_ad_excluded,
    base::RepeatingCallback<bool(const GURL&)> is_component_ad_excluded) {
  DCHECK(bidder_worklet_non_shared_params->ads.has_value());
  start_ = start;
  has_top_level_seller_origin_ = has_top_level_seller_origin;
  bidder_worklet_non_shared_params_ = bidder_worklet_non_shared_params;
  is_ad_excluded_ = std::move(is_ad_excluded);
  is_component_ad_excluded_ = std::move(is_component_ad_excluded);
}

void SetBidBindings::FillInGlobalTemplate(
    v8::Local<v8::ObjectTemplate> global_template) {
  v8::Local<v8::External> v8_this =
      v8::External::New(v8_helper_->isolate(), this);
  v8::Local<v8::FunctionTemplate> v8_template = v8::FunctionTemplate::New(
      v8_helper_->isolate(), &SetBidBindings::SetBid, v8_this);
  v8_template->RemovePrototype();
  global_template->Set(v8_helper_->CreateStringFromLiteral("setBid"),
                       v8_template);
}

void SetBidBindings::Reset() {
  bid_.reset();
  // Make sure we don't keep any dangling references to auction input.
  bidder_worklet_non_shared_params_ = nullptr;
  is_ad_excluded_.Reset();
  is_component_ad_excluded_.Reset();
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

  std::vector<std::string> errors;
  if (!bindings->SetBid(argument_value, /*error_prefix=*/"", errors)) {
    DCHECK_EQ(1u, errors.size());
    // Remove the trailing period from the error message.
    std::string error_msg = errors[0].substr(0, errors[0].length() - 1);
    args.GetIsolate()->ThrowException(v8::Exception::TypeError(
        v8_helper->CreateUtf8String(error_msg).ToLocalChecked()));
    return;
  }
}

bool SetBidBindings::SetBid(v8::Local<v8::Value> generate_bid_result,
                            std::string error_prefix,
                            std::vector<std::string>& errors_out) {
  v8::Isolate* isolate = v8_helper_->isolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  bid_.reset();

  DCHECK(bidder_worklet_non_shared_params_)
      << "ReInitialize() must be called before each use";

  // Undefined and null are interpreted as choosing not to bid.
  if (generate_bid_result->IsNullOrUndefined())
    return true;

  if (!generate_bid_result->IsObject()) {
    errors_out.push_back(base::StrCat({error_prefix, "bid not an object."}));
    return false;
  }

  gin::Dictionary result_dict(isolate, generate_bid_result.As<v8::Object>());

  double bid;
  if (!result_dict.Get("bid", &bid)) {
    errors_out.push_back(base::StrCat(
        {error_prefix, "returned object must have numeric bid field."}));
    return false;
  }

  if (!std::isfinite(bid)) {
    // Bids should not be infinite or NaN.
    errors_out.push_back(base::StringPrintf("%sbid of %lf is not a valid bid.",
                                            error_prefix.c_str(), bid));
    return false;
  }
  if (bid <= 0.0) {
    // Not an error, just no bid.
    return true;
  }

  absl::optional<double> ad_cost;
  double tmp_ad_cost;
  if (result_dict.Get("adCost", &tmp_ad_cost)) {
    ad_cost = tmp_ad_cost;
  }

  v8::Local<v8::Value> ad_object;
  v8::Local<v8::Value> ad_render;
  // Parse and validate values.
  if (!result_dict.Get("ad", &ad_object) ||
      !result_dict.Get("render", &ad_render)) {
    errors_out.push_back(
        base::StrCat({error_prefix, "bid has incorrect structure."}));
    return false;
  }

  // "ad" field is optional, but if present, must be possible to convert to
  // JSON. Note that if "ad" field isn't present, Get("ad", ...) succeeds, but
  // `ad_object` is undefined.
  std::string ad_json;
  if (ad_object->IsUndefined()) {
    ad_json = "null";
  } else {
    if (!v8_helper_->ExtractJson(context, ad_object, &ad_json)) {
      errors_out.push_back(
          base::StrCat({error_prefix, "bid has invalid ad value."}));
      return false;
    }
  }

  if (has_top_level_seller_origin_) {
    bool allow_component_auction;
    if (!result_dict.Get("allowComponentAuction", &allow_component_auction) ||
        !allow_component_auction) {
      errors_out.push_back(
          base::StrCat({error_prefix,
                        "bid does not have allowComponentAuction "
                        "set to true. Bid dropped from component auction."}));
      return false;
    }
  }

  std::string render_url_string;
  absl::optional<blink::AdSize> render_size = absl::nullopt;
  if (ad_render->IsString()) {
    // Old behavior before FLEDGE API incorporating ad size.
    // The 'render' field corresponds to an url string, for example:
    // render: "https://response.test/"
    if (!gin::ConvertFromV8(isolate, ad_render, &render_url_string)) {
      errors_out.push_back(
          base::StrCat({error_prefix, "bid has incorrect structure."}));
      return false;
    }
  } else if (!TryToParseUrlWithSize(isolate, ad_render, render_url_string,
                                    render_size)) {
    // New behavior after FLEDGE API incorporating ad size.
    // The 'render' field corresponds to an object that contains the url string,
    // and optional width and height, for example:
    // 1. render: {url: "https://example.test/"}
    // 2. render: {url: "https://example.test/", width: "100sw", height: "50px"}
    errors_out.push_back(
        base::StrCat({error_prefix, "bid has incorrect structure."}));
    return false;
  }

  if (render_size.has_value() && !IsValidAdSize(render_size.value())) {
    errors_out.push_back(
        base::StrCat({error_prefix, "bid has invalid size for render ad."}));
    return false;
  }

  GURL render_url(render_url_string);
  if (!IsAllowedAdUrl(render_url, error_prefix, "render", is_ad_excluded_,
                      bidder_worklet_non_shared_params_->ads.value(),
                      errors_out)) {
    return false;
  }

  absl::optional<std::vector<blink::AdDescriptor>> ad_component_descriptors;
  v8::Local<v8::Value> ad_components;
  if (result_dict.Get("adComponents", &ad_components) &&
      !ad_components->IsNullOrUndefined()) {
    if (!bidder_worklet_non_shared_params_->ad_components.has_value()) {
      errors_out.push_back(
          base::StrCat({error_prefix,
                        "bid contains adComponents but InterestGroup has no "
                        "adComponents."}));
      return false;
    }

    if (!ad_components->IsArray()) {
      errors_out.push_back(base::StrCat(
          {error_prefix, "bid adComponents value must be an array."}));
      return false;
    }

    v8::Local<v8::Array> ad_components_array = ad_components.As<v8::Array>();
    if (ad_components_array->Length() > blink::kMaxAdAuctionAdComponents) {
      errors_out.push_back(base::StringPrintf(
          "%sbid adComponents with over %zu items.", error_prefix.c_str(),
          blink::kMaxAdAuctionAdComponents));
      return false;
    }

    ad_component_descriptors.emplace();
    for (size_t i = 0; i < ad_components_array->Length(); ++i) {
      std::string ad_component_url_string;
      absl::optional<blink::AdSize> ad_component_size = absl::nullopt;
      if (ad_components_array->Get(context, i).ToLocalChecked()->IsString()) {
        // Old behavior before FLEDGE API incorporating ad size.
        // The 'adComponents' field corresponds to an array of url strings, for
        // example:
        // adComponents: ["https://test/1",
        //                "https://test/2",
        //                "https://test/3"]
        if (!gin::ConvertFromV8(
                isolate, ad_components_array->Get(context, i).ToLocalChecked(),
                &ad_component_url_string)) {
          errors_out.push_back(base::StrCat(
              {error_prefix,
               "bid adComponents value must be an array of strings or objects "
               "that contain the url string field and optional width and "
               "height fields."}));
          return false;
        }
      } else if (!TryToParseUrlWithSize(
                     isolate,
                     ad_components_array->Get(context, i).ToLocalChecked(),
                     ad_component_url_string, ad_component_size)) {
        // New behavior after FLEDGE API incorporating ad size.
        // The 'adComponents' field corresponds to
        // 1. an array of url strings or
        // 2. objects that contain the url string, and optional width and height
        //    fiedls
        // For example:
        // adComponents: [{url: "https://test/1"},
        //                {url: "https://test/2", width: "10sw", height: "5px"},
        //                "https://test/3"]
        errors_out.push_back(
            base::StrCat({error_prefix,
                          "bid adComponents value must be an array of strings "
                          "or objects that contain the url string field and "
                          "optional width and height fields."}));
        return false;
      }

      if (ad_component_size.has_value() &&
          !IsValidAdSize(ad_component_size.value())) {
        errors_out.push_back(base::StrCat(
            {error_prefix,
             "bid adComponents have invalid size for ad component."}));
        return false;
      }

      GURL ad_component_url(ad_component_url_string);
      if (!IsAllowedAdUrl(
              ad_component_url, error_prefix, "adComponents",
              is_component_ad_excluded_,
              bidder_worklet_non_shared_params_->ad_components.value(),
              errors_out)) {
        return false;
      }
      ad_component_descriptors->emplace_back(std::move(ad_component_url),
                                             std::move(ad_component_size));
    }
  }

  // `bid_duration` needs to include the entire time the bid script took to run,
  // including the time from the last setBid() call to when the bidder worklet
  // timed out, if the worklet did time out. So `bid_duration` is calculated
  // when ownership of the bid is taken by the caller, instead of here.
  bid_ =
      mojom::BidderWorkletBid::New(std::move(ad_json), bid, std::move(ad_cost),
                                   blink::AdDescriptor(render_url, render_size),
                                   std::move(ad_component_descriptors),
                                   /*bid_duration=*/base::TimeDelta());
  return true;
}

}  // namespace auction_worklet
