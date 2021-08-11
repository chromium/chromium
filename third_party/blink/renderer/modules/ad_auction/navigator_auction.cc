// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ad_auction/navigator_auction.h"

#include <utility>

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_usvstring_usvstringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_auction_ad.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_auction_ad_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_auction_ad_interest_group.h"
#include "third_party/blink/renderer/modules/ad_auction/validate_blink_interest_group.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin_hash.h"

namespace blink {

namespace {

// Error string builders.

String ErrorInvalidInterestGroup(const AuctionAdInterestGroup& group,
                                 const String& field_name,
                                 const String& field_value,
                                 const String& error) {
  return String::Format(
      "%s '%s' for AuctionAdInterestGroup with owner '%s' and name '%s' %s",
      field_name.Utf8().c_str(), field_value.Utf8().c_str(),
      group.owner().Utf8().c_str(), group.name().Utf8().c_str(),
      error.Utf8().c_str());
}

String ErrorInvalidInterestGroupJson(const AuctionAdInterestGroup& group,
                                     const String& field_name) {
  return String::Format(
      "%s for AuctionAdInterestGroup with owner '%s' and name '%s' must be a "
      "JSON-serializable object.",
      field_name.Utf8().c_str(), group.owner().Utf8().c_str(),
      group.name().Utf8().c_str());
}

String ErrorInvalidAuctionConfig(const AuctionAdConfig& config,
                                 const String& field_name,
                                 const String& field_value,
                                 const String& error) {
  return String::Format("%s '%s' for AuctionAdConfig with seller '%s' %s",
                        field_name.Utf8().c_str(), field_value.Utf8().c_str(),
                        config.seller().Utf8().c_str(), error.Utf8().c_str());
}

String ErrorInvalidAuctionConfigJson(const AuctionAdConfig& config,
                                     const String& field_name) {
  return String::Format(
      "%s for AuctionAdConfig with seller '%s' must be a JSON-serializable "
      "object.",
      field_name.Utf8().c_str(), config.seller().Utf8().c_str());
}

// JSON and Origin conversion helpers.

bool Jsonify(const ScriptState& script_state,
             const v8::Local<v8::Object> object,
             String& output) {
  v8::Local<v8::String> v8_string;
  if (!v8::JSON::Stringify(script_state.GetContext(), object)
           .ToLocal(&v8_string))
    return false;
  output = ToCoreString(v8_string);
  // JSON.stringify can fail to produce a string value in one of two ways: it
  // can throw an exception (as with unserializable objects), or it can return
  // `undefined` (as with e.g. passing a function). If JSON.stringify returns
  // `undefined`, the v8 API then coerces it to the string value "undefined".
  // Check for this, and consider it a failure (since we didn't properly
  // serialize a value, and v8::JSON::Parse() rejects "undefined").
  return output != "undefined";
}

// Returns nullptr if |origin_string| couldn't be parsed into an acceptable
// origin.
scoped_refptr<const SecurityOrigin> ParseOrigin(const String& origin_string) {
  scoped_refptr<const SecurityOrigin> origin =
      SecurityOrigin::CreateFromString(origin_string);
  if (origin->Protocol() != url::kHttpsScheme)
    return nullptr;
  return origin;
}

// WebIDL -> Mojom copy functions -- each return true if successful (including
// the not present, nothing to copy case), returns false and throws JS exception
// for invalid input.

// joinAdInterestGroup() copy functions.

bool CopyOwnerFromIdlToMojo(const ExecutionContext& execution_context,
                            ExceptionState& exception_state,
                            const AuctionAdInterestGroup& input,
                            mojom::blink::InterestGroup& output) {
  scoped_refptr<const SecurityOrigin> owner = ParseOrigin(input.owner());
  if (!owner) {
    exception_state.ThrowTypeError(String::Format(
        "owner '%s' for AuctionAdInterestGroup with name '%s' must be a valid "
        "https origin.",
        input.owner().Utf8().c_str(), input.name().Utf8().c_str()));
    return false;
  }

  if (!execution_context.GetSecurityOrigin()->IsSameOriginWith(owner.get())) {
    exception_state.ThrowTypeError(String::Format(
        "owner '%s' for AuctionAdInterestGroup with name '%s' match frame "
        "origin '%s'.",
        input.owner().Utf8().c_str(), input.name().Utf8().c_str(),
        owner->ToString().Utf8().c_str()));
    return false;
  }

  output.owner = std::move(owner);
  return true;
}

bool CopyBiddingLogicUrlFromIdlToMojo(const ExecutionContext& context,
                                      ExceptionState& exception_state,
                                      const AuctionAdInterestGroup& input,
                                      mojom::blink::InterestGroup& output) {
  if (!input.hasBiddingLogicUrl())
    return true;
  KURL bidding_url = context.CompleteURL(input.biddingLogicUrl());
  if (!bidding_url.IsValid()) {
    exception_state.ThrowTypeError(ErrorInvalidInterestGroup(
        input, "biddingLogicUrl", input.biddingLogicUrl(),
        "cannot be resolved to a valid URL."));
    return false;
  }
  output.bidding_url = bidding_url;
  return true;
}

bool CopyDailyUpdateUrlFromIdlToMojo(const ExecutionContext& context,
                                     ExceptionState& exception_state,
                                     const AuctionAdInterestGroup& input,
                                     mojom::blink::InterestGroup& output) {
  if (!input.hasDailyUpdateUrl())
    return true;
  KURL daily_update_url = context.CompleteURL(input.dailyUpdateUrl());
  if (!daily_update_url.IsValid()) {
    exception_state.ThrowTypeError(ErrorInvalidInterestGroup(
        input, "dailyUpdateUrl", input.dailyUpdateUrl(),
        "cannot be resolved to a valid URL."));
    return false;
  }
  output.update_url = daily_update_url;
  return true;
}

bool CopyTrustedBiddingSignalsUrlFromIdlToMojo(
    const ExecutionContext& context,
    ExceptionState& exception_state,
    const AuctionAdInterestGroup& input,
    mojom::blink::InterestGroup& output) {
  if (!input.hasTrustedBiddingSignalsUrl())
    return true;
  KURL trusted_bidding_signals_url =
      context.CompleteURL(input.trustedBiddingSignalsUrl());
  if (!trusted_bidding_signals_url.IsValid()) {
    exception_state.ThrowTypeError(ErrorInvalidInterestGroup(
        input, "trustedBiddingSignalsUrl", input.trustedBiddingSignalsUrl(),
        "cannot be resolved to a valid URL."));
    return false;
  }
  output.trusted_bidding_signals_url = trusted_bidding_signals_url;
  return true;
}

bool CopyTrustedBiddingSignalsKeysFromIdlToMojo(
    const AuctionAdInterestGroup& input,
    mojom::blink::InterestGroup& output) {
  if (!input.hasTrustedBiddingSignalsKeys())
    return true;
  output.trusted_bidding_signals_keys.emplace();
  for (const auto& key : input.trustedBiddingSignalsKeys()) {
    output.trusted_bidding_signals_keys->push_back(key);
  }
  return true;
}

bool CopyUserBiddingSignalsFromIdlToMojo(const ScriptState& script_state,
                                         ExceptionState& exception_state,
                                         const AuctionAdInterestGroup& input,
                                         mojom::blink::InterestGroup& output) {
  if (!input.hasUserBiddingSignals())
    return true;
  if (!Jsonify(script_state,
               input.userBiddingSignals().V8Value().As<v8::Object>(),
               output.user_bidding_signals)) {
    exception_state.ThrowTypeError(
        ErrorInvalidInterestGroupJson(input, "userBiddingSignals"));
    return false;
  }

  return true;
}

bool CopyAdsFromIdlToMojo(const ExecutionContext& context,
                          const ScriptState& script_state,
                          ExceptionState& exception_state,
                          const AuctionAdInterestGroup& input,
                          mojom::blink::InterestGroup& output) {
  if (!input.hasAds())
    return true;
  output.ads.emplace();
  for (const auto& ad : input.ads()) {
    auto mojo_ad = mojom::blink::InterestGroupAd::New();
    KURL render_url = context.CompleteURL(ad->renderUrl());
    if (!render_url.IsValid()) {
      exception_state.ThrowTypeError(
          ErrorInvalidInterestGroup(input, "ad renderUrl", ad->renderUrl(),
                                    "cannot be resolved to a valid URL."));
      return false;
    }
    mojo_ad->render_url = render_url;
    if (ad->hasMetadata()) {
      if (!Jsonify(script_state, ad->metadata().V8Value().As<v8::Object>(),
                   mojo_ad->metadata)) {
        exception_state.ThrowTypeError(
            ErrorInvalidInterestGroupJson(input, "ad metadata"));
        return false;
      }
    }
    output.ads->push_back(std::move(mojo_ad));
  }
  return true;
}

// runAdAuction() copy functions.

bool CopySellerFromIdlToMojo(ExceptionState& exception_state,
                             const AuctionAdConfig& input,
                             mojom::blink::AuctionAdConfig& output) {
  scoped_refptr<const SecurityOrigin> seller = ParseOrigin(input.seller());
  if (!seller) {
    exception_state.ThrowTypeError(String::Format(
        "seller '%s' for AuctionAdConfig must be a valid https origin.",
        input.seller().Utf8().c_str()));
    return false;
  }
  output.seller = seller;
  return true;
}

bool CopyDecisionLogicUrlFromIdlToMojo(const ExecutionContext& context,
                                       ExceptionState& exception_state,
                                       const AuctionAdConfig& input,
                                       mojom::blink::AuctionAdConfig& output) {
  KURL decision_logic_url = context.CompleteURL(input.decisionLogicUrl());
  if (!decision_logic_url.IsValid()) {
    exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
        input, "decisionLogicUrl", input.decisionLogicUrl(),
        "cannot be resolved to a valid URL."));
    return false;
  }
  output.decision_logic_url = decision_logic_url;
  return true;
}

bool CopyInterestGroupBuyersFromIdlToMojo(
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasInterestGroupBuyers())
    return true;
  output.interest_group_buyers = mojom::blink::InterestGroupBuyers::New();
  switch (input.interestGroupBuyers()->GetContentType()) {
    case V8UnionUSVStringOrUSVStringSequence::ContentType::kUSVString: {
      const String& maybe_wildcard =
          input.interestGroupBuyers()->GetAsUSVString();
      if (maybe_wildcard != "*") {
        exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
            input, "interestGroupBuyers", maybe_wildcard,
            "must be \"*\" (wildcard) or a list of buyer https origin "
            "strings."));
        return false;
      }
      output.interest_group_buyers->set_all_buyers(
          mojom::blink::AllBuyers::New());
      break;
    }
    case V8UnionUSVStringOrUSVStringSequence::ContentType::kUSVStringSequence: {
      Vector<scoped_refptr<const SecurityOrigin>> buyers;
      for (const auto& buyer_str :
           input.interestGroupBuyers()->GetAsUSVStringSequence()) {
        scoped_refptr<const SecurityOrigin> buyer = ParseOrigin(buyer_str);
        if (!buyer) {
          exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
              input, "interestGroupBuyers buyer", buyer_str,
              "must be a valid https origin."));
          return false;
        }
        buyers.push_back(buyer);
      }
      output.interest_group_buyers->set_buyers(std::move(buyers));
      break;
    }
  }

  return true;
}

bool CopyAuctionSignalsFromIdlToMojo(const ScriptState& script_state,
                                     ExceptionState& exception_state,
                                     const AuctionAdConfig& input,
                                     mojom::blink::AuctionAdConfig& output) {
  if (!input.hasAuctionSignals())
    return true;
  if (!Jsonify(script_state, input.auctionSignals().V8Value().As<v8::Object>(),
               output.auction_signals)) {
    exception_state.ThrowTypeError(
        ErrorInvalidAuctionConfigJson(input, "auctionSignals"));
    return false;
  }
  return true;
}

bool CopySellerSignalsFromIdlToMojo(const ScriptState& script_state,
                                    ExceptionState& exception_state,
                                    const AuctionAdConfig& input,
                                    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasSellerSignals())
    return true;
  if (!Jsonify(script_state, input.sellerSignals().V8Value().As<v8::Object>(),
               output.seller_signals)) {
    exception_state.ThrowTypeError(
        ErrorInvalidAuctionConfigJson(input, "sellerSignals"));
    return false;
  }

  return true;
}

bool CopyPerBuyerSignalsFromIdlToMojo(const ScriptState& script_state,
                                      ExceptionState& exception_state,
                                      const AuctionAdConfig& input,
                                      mojom::blink::AuctionAdConfig& output) {
  if (!input.hasPerBuyerSignals())
    return true;
  output.per_buyer_signals.emplace();
  for (const auto& per_buyer_signal : input.perBuyerSignals()) {
    scoped_refptr<const SecurityOrigin> buyer =
        ParseOrigin(per_buyer_signal.first);
    if (!buyer) {
      exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
          input, "perBuyerSignals buyer", per_buyer_signal.first,
          "must be a valid https origin."));
      return false;
    }
    String buyer_signals_str;
    if (!Jsonify(script_state,
                 per_buyer_signal.second.V8Value().As<v8::Object>(),
                 buyer_signals_str)) {
      exception_state.ThrowTypeError(
          ErrorInvalidAuctionConfigJson(input, "perBuyerSignals"));
      return false;
    }
    output.per_buyer_signals->insert(buyer, std::move(buyer_signals_str));
  }

  return true;
}

}  // namespace

NavigatorAuction::NavigatorAuction(Navigator& navigator)
    : Supplement(navigator),
      ad_auction_service_(navigator.GetExecutionContext()),
      interest_group_store_(navigator.GetExecutionContext()) {
  navigator.GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      ad_auction_service_.BindNewPipeAndPassReceiver(
          navigator.GetExecutionContext()->GetTaskRunner(
              TaskType::kMiscPlatformAPI)));
  navigator.GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      interest_group_store_.BindNewPipeAndPassReceiver(
          navigator.GetExecutionContext()->GetTaskRunner(
              TaskType::kMiscPlatformAPI)));
}

NavigatorAuction& NavigatorAuction::From(ExecutionContext* context,
                                         Navigator& navigator) {
  NavigatorAuction* supplement =
      Supplement<Navigator>::From<NavigatorAuction>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorAuction>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

const char NavigatorAuction::kSupplementName[] = "NavigatorAuction";

void NavigatorAuction::joinAdInterestGroup(ScriptState* script_state,
                                           const AuctionAdInterestGroup* group,
                                           double duration_seconds,
                                           ExceptionState& exception_state) {
  const ExecutionContext* context = ExecutionContext::From(script_state);
  auto mojo_group = mojom::blink::InterestGroup::New();
  mojo_group->expiry =
      base::Time::Now() + base::TimeDelta::FromSecondsD(duration_seconds);
  if (!CopyOwnerFromIdlToMojo(*context, exception_state, *group, *mojo_group))
    return;
  mojo_group->name = group->name();
  if (!CopyBiddingLogicUrlFromIdlToMojo(*context, exception_state, *group,
                                        *mojo_group)) {
    return;
  }
  if (!CopyDailyUpdateUrlFromIdlToMojo(*context, exception_state, *group,
                                       *mojo_group)) {
    return;
  }
  if (!CopyTrustedBiddingSignalsUrlFromIdlToMojo(*context, exception_state,
                                                 *group, *mojo_group)) {
    return;
  }
  if (!CopyTrustedBiddingSignalsKeysFromIdlToMojo(*group, *mojo_group))
    return;
  if (!CopyUserBiddingSignalsFromIdlToMojo(*script_state, exception_state,
                                           *group, *mojo_group)) {
    return;
  }
  if (!CopyAdsFromIdlToMojo(*context, *script_state, exception_state, *group,
                            *mojo_group)) {
    return;
  }

  String error_field_name;
  String error_field_value;
  String error;
  if (!ValidateBlinkInterestGroup(
          *mojo_group, error_field_name, error_field_value, error)) {
    exception_state.ThrowTypeError(ErrorInvalidInterestGroup(
        *group, error_field_name, error_field_value, error));
    return;
  }

  interest_group_store_->JoinInterestGroup(std::move(mojo_group));
}

/* static */
void NavigatorAuction::joinAdInterestGroup(ScriptState* script_state,
                                           Navigator& navigator,
                                           const AuctionAdInterestGroup* group,
                                           double duration_seconds,
                                           ExceptionState& exception_state) {
  return From(ExecutionContext::From(script_state), navigator)
      .joinAdInterestGroup(script_state, group, duration_seconds,
                           exception_state);
}

void NavigatorAuction::leaveAdInterestGroup(ScriptState* script_state,
                                            const AuctionAdInterestGroup* group,
                                            ExceptionState& exception_state) {
  scoped_refptr<const SecurityOrigin> owner = ParseOrigin(group->owner());
  if (!owner) {
    exception_state.ThrowTypeError("owner '" + group->owner() +
                                   "' for AuctionAdInterestGroup with name '" +
                                   group->name() +
                                   "' must be a valid https origin.");
    return;
  }
  interest_group_store_->LeaveInterestGroup(owner, group->name());
}

/* static */
void NavigatorAuction::leaveAdInterestGroup(ScriptState* script_state,
                                            Navigator& navigator,
                                            const AuctionAdInterestGroup* group,
                                            ExceptionState& exception_state) {
  return From(ExecutionContext::From(script_state), navigator)
      .leaveAdInterestGroup(script_state, group, exception_state);
}

void NavigatorAuction::updateAdInterestGroups() {
  interest_group_store_->UpdateAdInterestGroups();
}

/* static */
void NavigatorAuction::updateAdInterestGroups(ScriptState* script_state,
                                              Navigator& navigator) {
  return From(ExecutionContext::From(script_state), navigator)
      .updateAdInterestGroups();
}

ScriptPromise NavigatorAuction::runAdAuction(ScriptState* script_state,
                                             const AuctionAdConfig* config,
                                             ExceptionState& exception_state) {
  const ExecutionContext* context = ExecutionContext::From(script_state);
  auto mojo_config = mojom::blink::AuctionAdConfig::New();
  if (!CopySellerFromIdlToMojo(exception_state, *config, *mojo_config))
    return ScriptPromise();
  if (!CopyDecisionLogicUrlFromIdlToMojo(*context, exception_state, *config,
                                         *mojo_config))
    return ScriptPromise();
  if (!CopyInterestGroupBuyersFromIdlToMojo(exception_state, *config,
                                            *mojo_config))
    return ScriptPromise();
  if (!CopyAuctionSignalsFromIdlToMojo(*script_state, exception_state, *config,
                                       *mojo_config))
    return ScriptPromise();
  if (!CopySellerSignalsFromIdlToMojo(*script_state, exception_state, *config,
                                      *mojo_config))
    return ScriptPromise();
  if (!CopyPerBuyerSignalsFromIdlToMojo(*script_state, exception_state, *config,
                                        *mojo_config))
    return ScriptPromise();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  ad_auction_service_->RunAdAuction(
      std::move(mojo_config),
      WTF::Bind(&NavigatorAuction::AuctionComplete, WrapPersistent(this),
                WrapPersistent(resolver)));
  return promise;
}

/* static */
ScriptPromise NavigatorAuction::runAdAuction(ScriptState* script_state,
                                             Navigator& navigator,
                                             const AuctionAdConfig* config,
                                             ExceptionState& exception_state) {
  return From(ExecutionContext::From(script_state), navigator)
      .runAdAuction(script_state, config, exception_state);
}

void NavigatorAuction::AuctionComplete(ScriptPromiseResolver* resolver,
                                       const absl::optional<KURL>& result_url) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;
  if (result_url) {
    resolver->Resolve(result_url);
  } else {
    resolver->Resolve(v8::Null(resolver->GetScriptState()->GetIsolate()));
  }
}

}  // namespace blink
