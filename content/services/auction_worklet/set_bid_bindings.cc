// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/set_bid_bindings.h"

#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/bidder_worklet.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "content/services/auction_worklet/webidl_compat.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "third_party/blink/public/common/features_generated.h"
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
// only. Returns the associated `blink::InterestGroup::Ad` if one is found,
// or std::nullopt if not.
base::optional_ref<const blink::InterestGroup::Ad> FindAdForAssociatedAdUrl(
    const GURL& url,
    const std::string& error_prefix,
    const char* argument_name,
    const base::RepeatingCallback<bool(const std::string&)>& is_excluded,
    const std::vector<blink::InterestGroup::Ad>& ads,
    std::string& out_error) {
  if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme)) {
    out_error = base::StrCat({error_prefix, "bid ", argument_name, " URL '",
                              url.possibly_invalid_spec(),
                              "' isn't a valid https:// URL."});
    return std::nullopt;
  }

  for (const auto& ad : ads) {
    if (is_excluded.Run(ad.render_url())) {
      continue;
    }
    if (url.spec() == ad.render_url()) {
      return ad;
    }
  }
  out_error = base::StrCat({error_prefix, "bid ", argument_name, " URL '",
                            url.possibly_invalid_spec(),
                            "' isn't one of the registered creative URLs."});
  return std::nullopt;
}

struct AdRender {
  std::string url;
  std::optional<std::string> width;
  std::optional<std::string> height;
};

// Handles conversion of (DOMString or AdRender) IDL type.
IdlConvert::Status ConvertDomStringOrAdRender(
    AuctionV8Helper* v8_helper,
    AuctionV8Helper::TimeLimitScope& time_limit_scope,
    const std::string& error_prefix,
    v8::Local<v8::Value> value,
    AdRender& out) {
  if (value->IsString()) {
    bool ok = gin::ConvertFromV8(v8_helper->isolate(), value, &out.url);
    DCHECK(ok);  // Shouldn't fail since it's known to be String.
    return IdlConvert::Status::MakeSuccess();
  }

  DictConverter convert_ad_render(v8_helper, time_limit_scope, error_prefix,
                                  value);
  // This is alphabetical, since that's how dictionaries work.
  if (!convert_ad_render.GetOptional("height", out.height) ||
      !convert_ad_render.GetRequired("url", out.url) ||
      !convert_ad_render.GetOptional("width", out.width)) {
    return convert_ad_render.TakeStatus();
  }
  return IdlConvert::Status::MakeSuccess();
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
                           const AdRender& value,
                           std::string& ad_url_out,
                           std::optional<blink::AdSize>& size_out,
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
    size_out = std::nullopt;
  }

  return true;
}

std::string RenderPrefix(const std::string& error_prefix) {
  return base::StrCat({error_prefix, "'render': "});
}

std::string ComponentsPrefix(const std::string& error_prefix) {
  return base::StrCat({error_prefix, "adComponents entry: "});
}

}  // namespace

SetBidBindings::BidAndWorkletOnlyMetadata::BidAndWorkletOnlyMetadata() =
    default;
SetBidBindings::BidAndWorkletOnlyMetadata::BidAndWorkletOnlyMetadata(
    BidAndWorkletOnlyMetadata&&) = default;
SetBidBindings::BidAndWorkletOnlyMetadata::~BidAndWorkletOnlyMetadata() =
    default;
SetBidBindings::BidAndWorkletOnlyMetadata&
SetBidBindings::BidAndWorkletOnlyMetadata::operator=(
    BidAndWorkletOnlyMetadata&&) = default;

std::vector<SetBidBindings::BidAndWorkletOnlyMetadata>
SetBidBindings::TakeBids() {
  // Set `bid_duration` here instead of in SetBid(), so it can include the
  // entire script execution time.
  base::TimeDelta time_duration = base::TimeTicks::Now() - start_;
  for (auto& bid_info : bids_) {
    bid_info.bid->bid_duration = time_duration;
  }
  return std::move(bids_);
}

// This basically corresponds to the GenerateBidOutput type in the spec,
// except the (DOMString or AdRender) unions are normalized to AdRender.
struct SetBidBindings::GenerateBidOutput {
  GenerateBidOutput() = default;
  GenerateBidOutput(GenerateBidOutput&&) = default;
  GenerateBidOutput(const GenerateBidOutput&) = delete;
  ~GenerateBidOutput() = default;

  GenerateBidOutput& operator=(GenerateBidOutput&&) = default;
  GenerateBidOutput& operator=(const GenerateBidOutput&) = delete;

  std::optional<double> bid;
  std::optional<std::string> bid_currency;
  std::optional<AdRender> render;
  std::optional<v8::Local<v8::Value>> ad;
  std::optional<std::vector<AdRender>> ad_components;
  std::optional<double> ad_cost;
  std::optional<UnrestrictedDouble> modeling_signals;
  std::optional<std::string> selected_buyer_and_seller_reporting_id;
  bool allow_component_auction = false;
  uint32_t num_mandatory_ad_components = 0;
  std::optional<uint32_t> target_num_ad_components;
};

SetBidBindings::SetBidBindings(AuctionV8Helper* v8_helper)
    : v8_helper_(v8_helper),
      support_multi_bid_(BidderWorklet::SupportMultiBid()) {}

SetBidBindings::~SetBidBindings() = default;

void SetBidBindings::ReInitialize(
    base::TimeTicks start,
    bool has_top_level_seller_origin,
    const mojom::BidderWorkletNonSharedParams* bidder_worklet_non_shared_params,
    const std::optional<blink::AdCurrency>& per_buyer_currency,
    uint16_t multi_bid_limit,
    base::RepeatingCallback<bool(const std::string&)> is_ad_excluded,
    base::RepeatingCallback<bool(const std::string&)> is_component_ad_excluded,
    base::RepeatingCallback<bool(const std::string&,
                                 base::optional_ref<const std::string>,
                                 base::optional_ref<const std::string>,
                                 base::optional_ref<const std::string>)>
        is_reporting_id_set_excluded) {
  DCHECK(bidder_worklet_non_shared_params->ads.has_value());
  start_ = start;
  has_top_level_seller_origin_ = has_top_level_seller_origin;
  bidder_worklet_non_shared_params_ = bidder_worklet_non_shared_params;
  per_buyer_currency_ = per_buyer_currency;
  multi_bid_limit_ = multi_bid_limit;
  is_ad_excluded_ = std::move(is_ad_excluded);
  is_component_ad_excluded_ = std::move(is_component_ad_excluded);
  is_reporting_id_set_excluded_ = std::move(is_reporting_id_set_excluded);
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
  bids_.clear();
  // Make sure we don't keep any dangling references to auction input.
  bidder_worklet_non_shared_params_ = nullptr;
  reject_reason_ = mojom::RejectReason::kNotAvailable;
  per_buyer_currency_ = std::nullopt;
  multi_bid_limit_ = 1;
  is_ad_excluded_.Reset();
  is_component_ad_excluded_.Reset();
  is_reporting_id_set_excluded_.Reset();
}

IdlConvert::Status SetBidBindings::SetBidImpl(v8::Local<v8::Value> value,
                                              std::string error_prefix) {
  bids_.clear();

  AuctionV8Helper::TimeLimitScope time_limit_scope(v8_helper_->GetTimeLimit());

  // If the bid value is an object, check if it's convertible to a sequence,
  // and if so parse as multiple bids. This will be denoted by non-null
  // `iterator_factory`.
  v8::Local<v8::Object> iterable, iterator_factory;
  if (value->IsObject() && support_multi_bid_) {
    iterable = value.As<v8::Object>();
    IdlConvert::Status seq_check_status = IdlConvert::CheckForSequence(
        v8_helper_->isolate(), error_prefix, {}, iterable, iterator_factory);
    if (!seq_check_status.is_success()) {
      // Side effects of overload check returned failure.
      return seq_check_status;
    }
  }

  std::string render_prefix;
  std::string components_prefix;
  std::string too_many_bids_error;

  // Do the IDL conversion of the input, whatever type it is.
  std::vector<GenerateBidOutput> idl_bids;
  auto item_handler = base::BindRepeating(
      [](SetBidBindings* self,
         AuctionV8Helper::TimeLimitScope& time_limit_scope,
         const std::string& error_prefix, const std::string& render_prefix,
         const std::string& components_prefix,
         std::vector<GenerateBidOutput>& idl_bids,
         v8::Local<v8::Value> item) -> IdlConvert::Status {
        auto bid_idl_or_error =
            self->ConvertBidToIDL(time_limit_scope, item, error_prefix,
                                  render_prefix, components_prefix);
        if (bid_idl_or_error.has_value()) {
          idl_bids.push_back(std::move(bid_idl_or_error).value());
          return IdlConvert::Status::MakeSuccess();
        } else {
          // IDL conversion failed.
          return std::move(bid_idl_or_error).error();
        }
      },
      this, std::ref(time_limit_scope), std::cref(error_prefix),
      std::cref(render_prefix), std::cref(components_prefix),
      std::ref(idl_bids));

  if (iterator_factory.IsEmpty()) {
    // Single bid.
    render_prefix = RenderPrefix(error_prefix);
    components_prefix = ComponentsPrefix(error_prefix);
    auto status = item_handler.Run(value);
    if (!status.is_success()) {
      return status;
    }
  } else {
    // Bid sequence.
    too_many_bids_error = base::StrCat(
        {error_prefix,
         "more bids provided than permitted by auction configuration."});
    error_prefix = base::StrCat({error_prefix, "bids sequence entry: "});
    render_prefix = RenderPrefix(error_prefix);
    components_prefix = ComponentsPrefix(error_prefix);

    auto seq_status =
        IdlConvert::ConvertSequence(v8_helper_.get(), error_prefix, {},
                                    iterable, iterator_factory, item_handler);
    if (!seq_status.is_success()) {
      return seq_status;
    }
  }

  // Now do semantic checks. Length is first.
  if (idl_bids.size() > multi_bid_limit_) {
    reject_reason_ = mojom::RejectReason::kMultiBidLimitExceeded;
    return IdlConvert::Status::MakeErrorMessage(too_many_bids_error);
  }

  // Check all bids and convert them to Mojo structs in `bids_`.
  for (const auto& bid_idl : idl_bids) {
    auto maybe_new_bid =
        SemanticCheckBid(time_limit_scope, bid_idl, error_prefix, render_prefix,
                         components_prefix);

    if (!maybe_new_bid.has_value()) {
      // In case of error, clear the list of bids.
      bids_.clear();
      return std::move(maybe_new_bid).error();
    }

    // Unlike with IDL conversion, success does not imply existence of bid;
    // "no bid" is a valid outcome.
    if (maybe_new_bid.value().bid) {
      bids_.push_back(std::move(maybe_new_bid).value());
    }
  }
  return IdlConvert::Status::MakeSuccess();
}

base::expected<SetBidBindings::GenerateBidOutput, IdlConvert::Status>
SetBidBindings::ConvertBidToIDL(
    AuctionV8Helper::TimeLimitScope& time_limit_scope,
    v8::Local<v8::Value> input,
    const std::string& error_prefix,
    const std::string& render_prefix,
    const std::string& components_prefix) {
  GenerateBidOutput idl;

  auto components_exist = base::BindOnce(
      [](GenerateBidOutput& idl) { idl.ad_components.emplace(); },
      std::ref(idl));

  DictConverter convert_set_bid(v8_helper_.get(), time_limit_scope,
                                error_prefix, input);

  scoped_refptr<AuctionV8Helper> ref_v8_helper(v8_helper_.get());
  auto collect_components = base::BindRepeating(
      [](scoped_refptr<AuctionV8Helper> v8_helper,
         AuctionV8Helper::TimeLimitScope& time_limit_scope,
         const std::string& error_prefix, GenerateBidOutput& idl,
         v8::Local<v8::Value> component) -> IdlConvert::Status {
        AdRender converted_component;
        IdlConvert::Status status = ConvertDomStringOrAdRender(
            v8_helper.get(), time_limit_scope, error_prefix, component,
            converted_component);
        if (status.is_success()) {
          idl.ad_components->push_back(std::move(converted_component));
        }
        return status;
      },
      ref_v8_helper, std::ref(time_limit_scope), std::cref(components_prefix),
      std::ref(idl));

  // Note that this happens in lexicographic order of field names, to match
  // WebIDL behavior.
  convert_set_bid.GetOptional("ad", idl.ad);
  convert_set_bid.GetOptionalSequence(
      "adComponents", std::move(components_exist), collect_components);
  convert_set_bid.GetOptional("adCost", idl.ad_cost);
  std::optional<bool> maybe_allow_component_auction;
  convert_set_bid.GetOptional("allowComponentAuction",
                              maybe_allow_component_auction);
  idl.allow_component_auction = maybe_allow_component_auction.value_or(false);
  convert_set_bid.GetOptional("bid", idl.bid);
  convert_set_bid.GetOptional("bidCurrency", idl.bid_currency);
  convert_set_bid.GetOptional("modelingSignals", idl.modeling_signals);

  std::optional<uint32_t> maybe_num_mandatory_ad_components;
  if (support_multi_bid_) {
    convert_set_bid.GetOptional("numMandatoryAdComponents",
                                maybe_num_mandatory_ad_components);
  }
  idl.num_mandatory_ad_components =
      maybe_num_mandatory_ad_components.value_or(0);

  std::optional<v8::Local<v8::Value>> render_value;
  if (convert_set_bid.GetOptional("render", render_value) &&
      render_value.has_value()) {
    idl.render.emplace();
    convert_set_bid.SetStatus(
        ConvertDomStringOrAdRender(v8_helper_.get(), time_limit_scope,
                                   render_prefix, *render_value, *idl.render));
  }

  if (base::FeatureList::IsEnabled(
          blink::features::kFledgeAuctionDealSupport)) {
    convert_set_bid.GetOptional("selectedBuyerAndSellerReportingId",
                                idl.selected_buyer_and_seller_reporting_id);
  }

  if (support_multi_bid_) {
    convert_set_bid.GetOptional("targetNumAdComponents",
                                idl.target_num_ad_components);
  }

  if (convert_set_bid.is_failed()) {
    return base::unexpected(convert_set_bid.TakeStatus());
  }

  return idl;
}

base::expected<SetBidBindings::BidAndWorkletOnlyMetadata, IdlConvert::Status>
SetBidBindings::SemanticCheckBid(
    AuctionV8Helper::TimeLimitScope& time_limit_scope,
    const GenerateBidOutput& idl,
    const std::string& error_prefix,
    const std::string& render_prefix,
    const std::string& components_prefix) {
  DCHECK(bidder_worklet_non_shared_params_)
      << "ReInitialize() must be called before each use";
  BidAndWorkletOnlyMetadata bid_and_worklet_only_metadata;

  v8::Isolate* isolate = v8_helper_->isolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (!idl.bid.has_value() || *idl.bid <= 0.0) {
    // Not an error, just no bid.
    return bid_and_worklet_only_metadata;
  }

  if (!idl.render.has_value()) {
    return base::unexpected(IdlConvert::Status::MakeErrorMessage(base::StrCat(
        {error_prefix, "'render' is required when making a bid."})));
  }

  std::optional<blink::AdCurrency> bid_currency;
  if (idl.bid_currency.has_value()) {
    if (!blink::IsValidAdCurrencyCode(*idl.bid_currency)) {
      reject_reason_ = mojom::RejectReason::kWrongGenerateBidCurrency;
      return base::unexpected(IdlConvert::Status::MakeErrorMessage(
          base::StringPrintf("%sbidCurrency of '%s' is not a currency code.",
                             error_prefix.c_str(), idl.bid_currency->c_str())));
    }
    bid_currency = blink::AdCurrency::From(*idl.bid_currency);
  }

  if (!blink::VerifyAdCurrencyCode(per_buyer_currency_, bid_currency)) {
    reject_reason_ = mojom::RejectReason::kWrongGenerateBidCurrency;
    return base::unexpected(
        IdlConvert::Status::MakeErrorMessage(base::StringPrintf(
            "%sbidCurrency mismatch; returned '%s', expected '%s'.",
            error_prefix.c_str(),
            blink::PrintableAdCurrency(bid_currency).c_str(),
            blink::PrintableAdCurrency(per_buyer_currency_).c_str())));
  }

  // "ad" field is optional, but if present, must be possible to convert to
  // JSON.
  std::string ad_json;
  if (!idl.ad.has_value()) {
    ad_json = "null";
  } else {
    // Don't need a `script_timeout` here since one is already active.
    AuctionV8Helper::Result json_result = v8_helper_->ExtractJson(
        context, *idl.ad, /*script_timeout=*/nullptr, &ad_json);
    if (json_result == AuctionV8Helper::Result::kFailure) {
      return base::unexpected(IdlConvert::Status::MakeErrorMessage(
          base::StrCat({error_prefix, "bid has invalid ad value."})));
    } else if (json_result == AuctionV8Helper::Result::kTimeout) {
      return base::unexpected(IdlConvert::Status::MakeTimeout(base::StrCat(
          {error_prefix, "serializing bid 'ad' value to JSON timed out."})));
    }
  }

  if (has_top_level_seller_origin_) {
    if (!idl.allow_component_auction) {
      return base::unexpected(IdlConvert::Status::MakeErrorMessage(
          base::StrCat({error_prefix,
                        "bid does not have allowComponentAuction "
                        "set to true. Bid dropped from component auction."})));
    }
  }

  std::optional<double> modeling_signals;
  if (idl.modeling_signals.has_value() && idl.modeling_signals->number >= 0 &&
      idl.modeling_signals->number < (1 << 12)) {
    modeling_signals = idl.modeling_signals->number;
  }

  std::string render_url_string;
  std::optional<blink::AdSize> render_size = std::nullopt;
  std::string error_msg;
  if (!TryToParseUrlWithSize(v8_helper_.get(), time_limit_scope, render_prefix,
                             *idl.render, render_url_string, render_size,
                             error_msg)) {
    return base::unexpected(
        IdlConvert::Status::MakeErrorMessage(std::move(error_msg)));
  }

  if (render_size.has_value() && !IsValidAdSize(render_size.value())) {
    return base::unexpected(IdlConvert::Status::MakeErrorMessage(
        base::StrCat({error_prefix, "bid has invalid size for render ad."})));
  }

  GURL render_url(render_url_string);
  base::optional_ref<const blink::InterestGroup::Ad> maybe_ad =
      FindAdForAssociatedAdUrl(
          render_url, error_prefix, "render", is_ad_excluded_,
          bidder_worklet_non_shared_params_->ads.value(), error_msg);
  if (!maybe_ad.has_value()) {
    return base::unexpected(
        IdlConvert::Status::MakeErrorMessage(std::move(error_msg)));
  }

  if (idl.selected_buyer_and_seller_reporting_id.has_value()) {
    if (!IsSelectedReportingIdValid(
            *maybe_ad, *idl.selected_buyer_and_seller_reporting_id)) {
      return base::unexpected(IdlConvert::Status::MakeErrorMessage(base::StrCat(
          {error_prefix, "Invalid selected buyer and seller reporting id"})));
    }
  }

  bid_and_worklet_only_metadata.buyer_reporting_id =
      maybe_ad->buyer_reporting_id;
  bid_and_worklet_only_metadata.buyer_and_seller_reporting_id =
      maybe_ad->buyer_and_seller_reporting_id;

  std::optional<std::vector<blink::AdDescriptor>> ad_component_descriptors;
  const size_t kMaxAdAuctionAdComponents = blink::MaxAdAuctionAdComponents();
  if (idl.target_num_ad_components.has_value()) {
    if (*idl.target_num_ad_components > kMaxAdAuctionAdComponents) {
      return base::unexpected(IdlConvert::Status::MakeErrorMessage(
          base::StringPrintf("%sbid targetNumAdComponents larger than "
                             "component ad limit of %zu.",
                             error_prefix.c_str(), kMaxAdAuctionAdComponents)));
    }
    if (*idl.target_num_ad_components == 0) {
      return base::unexpected(IdlConvert::Status::MakeErrorMessage(base::StrCat(
          {error_prefix, "targetNumAdComponents must be positive."})));
    }

    if (idl.num_mandatory_ad_components > *idl.target_num_ad_components) {
      return base::unexpected(IdlConvert::Status::MakeErrorMessage(base::StrCat(
          {error_prefix,
           "numMandatoryAdComponents cannot exceed targetNumAdComponents."})));
    }
  }

  if (idl.ad_components.has_value()) {
    if (!bidder_worklet_non_shared_params_->ad_components.has_value()) {
      return base::unexpected(IdlConvert::Status::MakeErrorMessage(
          base::StrCat({error_prefix,
                        "bid contains adComponents but InterestGroup has no "
                        "adComponents."})));
    }

    // We want < rather than <= here so the semantic check is testable and not
    // hidden by implementation details of IdlConvert.
    static_assert(blink::kMaxAdAuctionAdComponentsConfigLimit <
                  IdlConvert::kSequenceLengthLimit);

    if (!idl.target_num_ad_components.has_value() &&
        idl.ad_components->size() > kMaxAdAuctionAdComponents) {
      return base::unexpected(IdlConvert::Status::MakeErrorMessage(
          base::StringPrintf("%sbid adComponents with over %zu items.",
                             error_prefix.c_str(), kMaxAdAuctionAdComponents)));
    }

    ad_component_descriptors.emplace();
    for (const AdRender& component : *idl.ad_components) {
      std::string ad_component_url_string;
      std::optional<blink::AdSize> ad_component_size = std::nullopt;
      if (!TryToParseUrlWithSize(
              v8_helper_.get(), time_limit_scope, components_prefix, component,
              ad_component_url_string, ad_component_size, error_msg)) {
        return base::unexpected(
            IdlConvert::Status::MakeErrorMessage(std::move(error_msg)));
      }

      if (ad_component_size.has_value() &&
          !IsValidAdSize(ad_component_size.value())) {
        return base::unexpected(
            IdlConvert::Status::MakeErrorMessage(base::StrCat(
                {error_prefix,
                 "bid adComponents have invalid size for ad component."})));
      }

      GURL ad_component_url(ad_component_url_string);
      base::optional_ref<const blink::InterestGroup::Ad> maybe_component_ad =
          FindAdForAssociatedAdUrl(
              ad_component_url, error_prefix, "adComponents",
              is_component_ad_excluded_,
              bidder_worklet_non_shared_params_->ad_components.value(),
              error_msg);
      if (!maybe_component_ad.has_value()) {
        return base::unexpected(
            IdlConvert::Status::MakeErrorMessage(std::move(error_msg)));
      }
      ad_component_descriptors->emplace_back(std::move(ad_component_url),
                                             std::move(ad_component_size));
    }
  }

  if (idl.target_num_ad_components.has_value()) {
    size_t num_ad_component_descriptors = ad_component_descriptors.has_value()
                                              ? ad_component_descriptors->size()
                                              : 0u;

    if (num_ad_component_descriptors < *idl.target_num_ad_components) {
      return base::unexpected(IdlConvert::Status::MakeErrorMessage(base::StrCat(
          {error_prefix,
           "adComponents list smaller than targetNumAdComponents."})));
    }

    // Must have some component ads since their number is >=
    // `target_num_ad_components`, and that's positive.
    DCHECK(idl.ad_components.has_value());
    bid_and_worklet_only_metadata.target_num_ad_components =
        idl.target_num_ad_components;
    bid_and_worklet_only_metadata.num_mandatory_ad_components =
        idl.num_mandatory_ad_components;
  }

  // `bid_duration` needs to include the entire time the bid script took to run,
  // including the time from the last setBid() call to when the bidder worklet
  // timed out, if the worklet did time out. So `bid_duration` is calculated
  // when ownership of the bid is taken by the caller, instead of here.
  //
  // Similarly it's easier for BidderWorklet to compute the proper role.
  bid_and_worklet_only_metadata.bid = mojom::BidderWorkletBid::New(
      auction_worklet::mojom::BidRole::kUnenforcedKAnon, std::move(ad_json),
      *idl.bid, std::move(bid_currency), std::move(idl.ad_cost),
      blink::AdDescriptor(render_url, render_size),
      std::move(idl.selected_buyer_and_seller_reporting_id),
      std::move(ad_component_descriptors),
      static_cast<std::optional<uint16_t>>(modeling_signals),
      /*bid_duration=*/base::TimeDelta());
  return bid_and_worklet_only_metadata;
}

bool SetBidBindings::IsSelectedReportingIdValid(
    const blink::InterestGroup::Ad& ad,
    const std::string& selected_buyer_and_seller_reporting_id) {
  if (!ad.selectable_buyer_and_seller_reporting_ids.has_value()) {
    return false;
  }
  if (!base::Contains(*ad.selectable_buyer_and_seller_reporting_ids,
                      selected_buyer_and_seller_reporting_id)) {
    return false;
  }
  if (is_reporting_id_set_excluded_.Run(
          ad.render_url(), ad.buyer_reporting_id,
          ad.buyer_and_seller_reporting_id,
          selected_buyer_and_seller_reporting_id)) {
    return false;
  }
  return true;
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
