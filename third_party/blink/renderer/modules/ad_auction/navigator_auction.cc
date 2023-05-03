// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ad_auction/navigator_auction.h"

#include <stdint.h>

#include <utility>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/map_traits_wtf_hash_map.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/frame/fenced_frame_sandbox_flags.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/common/interest_group/ad_auction_currencies.h"
#include "third_party/blink/public/common/interest_group/ad_display_size_utils.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-blink.h"
#include "third_party/blink/public/mojom/parakeet/ad_request.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_fencedframeconfig_usvstring.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_usvstring_usvstringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ad_properties.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ad_request_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ad_targeting.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ads.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_auction_ad.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_auction_ad_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_auction_ad_interest_group.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_auction_ad_interest_group_key.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_auction_ad_interest_group_size.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_auction_report_buyers_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_adproperties_adpropertiessequence.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/scoped_abort_state.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/csp/csp_directive_list.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_config.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/ad_auction/ads.h"
#include "third_party/blink/renderer/modules/ad_auction/join_leave_queue.h"
#include "third_party/blink/renderer/modules/ad_auction/validate_blink_interest_group.h"
#include "third_party/blink/renderer/modules/geolocation/geolocation_coordinates.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/string_operators.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-value.h"

namespace blink {

// Helper to manage runtime of abort + promise resolution pipe.
// Can interface to AbortController itself, and has helper classes that can be
// connected to promises via Then and ScriptFunction.
class NavigatorAuction::AuctionHandle final : public AbortSignal::Algorithm {
 public:
  class JsonResolved : public ScriptFunction::Callable {
   public:
    // `field_name` is expected to point to a literal.
    JsonResolved(AuctionHandle* auction_handle,
                 mojom::blink::AuctionAdConfigAuctionIdPtr auction_id,
                 mojom::blink::AuctionAdConfigField field,
                 const String& seller_name,
                 const char* field_name);

    ScriptValue Call(ScriptState* script_state, ScriptValue value) override;
    void Trace(Visitor* visitor) const override;

   private:
    Member<AuctionHandle> auction_handle_;
    const mojom::blink::AuctionAdConfigAuctionIdPtr auction_id_;
    const mojom::blink::AuctionAdConfigField field_;
    const String seller_name_;
    const char* const field_name_;
  };

  class PerBuyerSignalsResolved : public ScriptFunction::Callable {
   public:
    PerBuyerSignalsResolved(
        AuctionHandle* auction_handle,
        mojom::blink::AuctionAdConfigAuctionIdPtr auction_id,
        const String& seller_name);

    ScriptValue Call(ScriptState* script_state, ScriptValue value) override;
    void Trace(Visitor* visitor) const override;

   private:
    Member<AuctionHandle> auction_handle_;
    const mojom::blink::AuctionAdConfigAuctionIdPtr auction_id_;
    const String seller_name_;
  };

  // This is used for perBuyerTimeouts and perBuyerCumulativeTimeouts, with
  // `field` indicating which of the two fields an object is being used for.
  class BuyerTimeoutsResolved : public ScriptFunction::Callable {
   public:
    BuyerTimeoutsResolved(AuctionHandle* auction_handle,
                          mojom::blink::AuctionAdConfigAuctionIdPtr auction_id,
                          mojom::blink::AuctionAdConfigBuyerTimeoutField field,
                          const String& seller_name);

    ScriptValue Call(ScriptState* script_state, ScriptValue value) override;
    void Trace(Visitor* visitor) const override;

   private:
    Member<AuctionHandle> auction_handle_;
    const mojom::blink::AuctionAdConfigAuctionIdPtr auction_id_;
    const mojom::blink::AuctionAdConfigBuyerTimeoutField field_;
    const String seller_name_;
  };

  class BuyerCurrenciesResolved : public ScriptFunction::Callable {
   public:
    BuyerCurrenciesResolved(
        AuctionHandle* auction_handle,
        mojom::blink::AuctionAdConfigAuctionIdPtr auction_id,
        const String& seller_name);

    ScriptValue Call(ScriptState* script_state, ScriptValue value) override;
    void Trace(Visitor* visitor) const override;

   private:
    Member<AuctionHandle> auction_handle_;
    const mojom::blink::AuctionAdConfigAuctionIdPtr auction_id_;
    const String seller_name_;
  };

  class DirectFromSellerSignalsResolved : public ScriptFunction::Callable {
   public:
    DirectFromSellerSignalsResolved(
        AuctionHandle* auction_handle,
        mojom::blink::AuctionAdConfigAuctionIdPtr auction_id,
        const String& seller_name,
        const scoped_refptr<const SecurityOrigin>& seller_origin,
        const absl::optional<Vector<scoped_refptr<const SecurityOrigin>>>&
            interest_group_buyers);

    ScriptValue Call(ScriptState* script_state, ScriptValue value) override;
    void Trace(Visitor* visitor) const override;

   private:
    Member<AuctionHandle> auction_handle_;
    const mojom::blink::AuctionAdConfigAuctionIdPtr auction_id_;
    const String seller_name_;
    const scoped_refptr<const SecurityOrigin> seller_origin_;
    absl::optional<Vector<scoped_refptr<const SecurityOrigin>>>
        interest_group_buyers_;
  };

  class ResolveToConfigResolved : public ScriptFunction::Callable {
   public:
    ResolveToConfigResolved(AuctionHandle* auction_handle);

    ScriptValue Call(ScriptState* script_state, ScriptValue value) override;
    void Trace(Visitor* visitor) const override;

   private:
    Member<AuctionHandle> auction_handle_;
  };

  class Rejected : public ScriptFunction::Callable {
   public:
    explicit Rejected(AuctionHandle* auction_handle);

    ScriptValue Call(ScriptState*, ScriptValue) override;
    void Trace(Visitor* visitor) const override;

   private:
    Member<AuctionHandle> auction_handle_;
  };

  AuctionHandle(ExecutionContext* context,
                mojo::PendingRemote<mojom::blink::AbortableAdAuction> remote)
      : abortable_ad_auction_(context) {
    abortable_ad_auction_.Bind(
        std::move(remote), context->GetTaskRunner(TaskType::kMiscPlatformAPI));
  }

  ~AuctionHandle() override = default;

  void AttachPromiseHandler(ScriptState& script_state,
                            ScriptPromise& promise,
                            ScriptFunction::Callable* success_helper) {
    promise.Then(
        MakeGarbageCollected<ScriptFunction>(&script_state, success_helper),
        MakeGarbageCollected<ScriptFunction>(
            &script_state,
            MakeGarbageCollected<NavigatorAuction::AuctionHandle::Rejected>(
                this)));
  }

  void Abort() { abortable_ad_auction_->Abort(); }

  void ResolvedPromiseParam(mojom::blink::AuctionAdConfigAuctionIdPtr auction,
                            mojom::blink::AuctionAdConfigField field,
                            const String& json_value) {
    abortable_ad_auction_->ResolvedPromiseParam(std::move(auction), field,
                                                json_value);
  }

  void ResolvedPerBuyerSignalsPromise(
      mojom::blink::AuctionAdConfigAuctionIdPtr auction,
      const absl::optional<WTF::HashMap<scoped_refptr<const SecurityOrigin>,
                                        String>>& per_buyer_signals) {
    abortable_ad_auction_->ResolvedPerBuyerSignalsPromise(std::move(auction),
                                                          per_buyer_signals);
  }

  void ResolvedBuyerTimeoutsPromise(
      mojom::blink::AuctionAdConfigAuctionIdPtr auction,
      mojom::blink::AuctionAdConfigBuyerTimeoutField field,
      mojom::blink::AuctionAdConfigBuyerTimeoutsPtr buyer_timeouts) {
    abortable_ad_auction_->ResolvedBuyerTimeoutsPromise(
        std::move(auction), field, std::move(buyer_timeouts));
  }

  void ResolvedBuyerCurrencies(
      mojom::blink::AuctionAdConfigAuctionIdPtr auction,
      mojom::blink::AuctionAdConfigBuyerCurrenciesPtr buyer_currencies) {
    abortable_ad_auction_->ResolvedBuyerCurrenciesPromise(
        std::move(auction), std::move(buyer_currencies));
  }

  void ResolvedDirectFromSellerSignalsPromise(
      mojom::blink::AuctionAdConfigAuctionIdPtr auction,
      mojom::blink::DirectFromSellerSignalsPtr direct_from_seller_signals) {
    abortable_ad_auction_->ResolvedDirectFromSellerSignalsPromise(
        std::move(auction), std::move(direct_from_seller_signals));
  }

  // AbortSignal::Algorithm implementation:
  void Run() override { Abort(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(abortable_ad_auction_);
    visitor->Trace(auction_resolver_);
    AbortSignal::Algorithm::Trace(visitor);
  }

  void AuctionComplete(
      ScriptPromiseResolver*,
      std::unique_ptr<ScopedAbortState>,
      bool manually_aborted,
      const absl::optional<FencedFrame::RedactedFencedFrameConfig>&);

  void MaybeResolveAuction();

  void SetResolveToConfig(bool value) { resolve_to_config_ = value; }

 private:
  HeapMojoRemote<mojom::blink::AbortableAdAuction> abortable_ad_auction_;

  absl::optional<bool> resolve_to_config_;
  Member<ScriptPromiseResolver> auction_resolver_;
  absl::optional<FencedFrame::RedactedFencedFrameConfig> auction_config_;
};

namespace {

// The maximum number of active cross-site joins and leaves. Once these are hit,
// cross-site joins/leaves are queued until they drop below this number. Queued
// pending operations are dropped on destruction / navigation away.
const int kMaxActiveCrossSiteJoins = 20;
const int kMaxActiveCrossSiteLeaves = 20;

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

String ErrorInvalidAuctionConfigSeller(const String& seller_name,
                                       const String& field_name,
                                       const String& field_value,
                                       const String& error) {
  return String::Format("%s '%s' for AuctionAdConfig with seller '%s' %s",
                        field_name.Utf8().c_str(), field_value.Utf8().c_str(),
                        seller_name.Utf8().c_str(), error.Utf8().c_str());
}

String ErrorInvalidAuctionConfig(const AuctionAdConfig& config,
                                 const String& field_name,
                                 const String& field_value,
                                 const String& error) {
  return ErrorInvalidAuctionConfigSeller(config.seller(), field_name,
                                         field_value, error);
}

String ErrorInvalidAuctionConfigSellerJson(const String& seller_name,
                                           const String& field_name) {
  return String::Format(
      "%s for AuctionAdConfig with seller '%s' must be a JSON-serializable "
      "object.",
      field_name.Utf8().c_str(), seller_name.Utf8().c_str());
}

String ErrorInvalidAuctionConfigJson(const AuctionAdConfig& config,
                                     const String& field_name) {
  return ErrorInvalidAuctionConfigSellerJson(config.seller(), field_name);
}

String ErrorInvalidAdRequestConfig(const AdRequestConfig& config,
                                   const String& field_name,
                                   const String& field_value,
                                   const String& error) {
  return String::Format("%s '%s' for AdRequestConfig with URL '%s' %s",
                        field_name.Utf8().c_str(), field_value.Utf8().c_str(),
                        config.adRequestUrl().Utf8().c_str(),
                        error.Utf8().c_str());
}

String ErrorInvalidAuctionConfigUint128(const AuctionAdConfig& config,
                                        const String& field_name,
                                        const String& error) {
  return String::Format("%s for AuctionAdConfig with seller '%s': %s",
                        field_name.Utf8().c_str(),
                        config.seller().Utf8().c_str(), error.Utf8().c_str());
}

String WarningPermissionsPolicy(const String& feature, const String& api) {
  return String::Format(
      "In the future, Permissions Policy feature %s will not be enabled by "
      "default in cross-origin iframes or same-origin iframes nested in "
      "cross-origin iframes. Calling %s will be rejected with NotAllowedError "
      "if it is not explicitly enabled",
      feature.Utf8().c_str(), api.Utf8().c_str());
}

// Console warnings.

void AddWarningMessageToConsole(ScriptState* script_state,
                                const String& feature,
                                const String& api) {
  auto* window = To<LocalDOMWindow>(ExecutionContext::From(script_state));
  WebLocalFrameImpl::FromFrame(window->GetFrame())
      ->AddMessageToConsole(
          WebConsoleMessage(mojom::blink::ConsoleMessageLevel::kWarning,
                            WarningPermissionsPolicy(feature, api)),
          /*discard_duplicates=*/true);
}

void ConsoleWarnDeprecatedEnum(const ExecutionContext& execution_context,
                               String enum_name,
                               String deprecated_value) {
  auto* window = To<LocalDOMWindow>(&execution_context);
  WebLocalFrameImpl::FromFrame(window->GetFrame())
      ->AddMessageToConsole(
          WebConsoleMessage(
              mojom::blink::ConsoleMessageLevel::kWarning,
              String::Format(
                  "Enum %s used deprecated value %s -- \"dashed-naming\" "
                  "should be used instead of \"camelCase\".",
                  enum_name.Utf8().c_str(), deprecated_value.Utf8().c_str())),
          /*discard_duplicates=*/true);
}

// JSON and Origin conversion helpers.

bool Jsonify(const ScriptState& script_state,
             const v8::Local<v8::Value>& value,
             String& output) {
  v8::Local<v8::String> v8_string;
  // v8::JSON throws on certain inputs that can't be converted to JSON (like
  // recursive structures). Use TryCatch to consume them. Otherwise, they'd take
  // precedence over the returned ExtensionState for methods that return
  // ScriptPromises, since ExceptionState is used to generate a rejected
  // promise, which V8 exceptions take precedence over.
  v8::TryCatch try_catch(script_state.GetIsolate());
  if (!v8::JSON::Stringify(script_state.GetContext(), value)
           .ToLocal(&v8_string) ||
      try_catch.HasCaught()) {
    return false;
  }

  output = ToCoreString(v8_string);
  // JSON.stringify can fail to produce a string value in one of two ways: it
  // can throw an exception (as with unserializable objects), or it can return
  // `undefined` (as with e.g. passing a function). If JSON.stringify returns
  // `undefined`, the v8 API then coerces it to the string value "undefined".
  // Check for this, and consider it a failure (since we didn't properly
  // serialize a value, and v8::JSON::Parse() rejects "undefined").
  return output != "undefined";
}

base::expected<absl::uint128, String> CopyBigIntToUint128(
    const BigInt& bigint) {
  if (!bigint.FitsIn128Bits()) {
    return base::unexpected("Too large BigInt; Must fit in 128 bits");
  }
  if (bigint.IsNegative()) {
    return base::unexpected("Negative BigInt cannot be converted to uint128");
  }
  return *bigint.ToUInt128();
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

  output.owner = std::move(owner);
  return true;
}

// Converts a sparse vector used in `priority_vector` and
// `priority_signals_overrides` to a WTF::HashMap, as is used in mojom structs.
// Has no failure cases.
WTF::HashMap<WTF::String, double> ConvertSparseVectorIdlToMojo(
    const Vector<std::pair<WTF::String, double>>& priority_signals_in) {
  WTF::HashMap<WTF::String, double> priority_signals_out;
  for (const auto& key_value_pair : priority_signals_in) {
    priority_signals_out.insert(key_value_pair.first, key_value_pair.second);
  }
  return priority_signals_out;
}

mojom::blink::SellerCapabilitiesPtr ConvertSellerCapabilitiesTypeFromIdlToMojo(
    const ExecutionContext& execution_context,
    const Vector<String>& capabilities_vector) {
  auto seller_capabilities = mojom::blink::SellerCapabilities::New();
  for (const String& capability_str : capabilities_vector) {
    const bool used_deprecated_names =
        capability_str == "interestGroupCounts" ||
        capability_str == "latencyStats";
    base::UmaHistogramBoolean(
        "Ads.InterestGroup.EnumNaming.Renderer.SellerCapabilities",
        used_deprecated_names);
    if (used_deprecated_names) {
      ConsoleWarnDeprecatedEnum(execution_context, "SellerCapabilities",
                                capability_str);
    }
    if (capability_str == "interest-group-counts" ||
        capability_str == "interestGroupCounts") {
      seller_capabilities->allows_interest_group_counts = true;
    } else if (capability_str == "latency-stats" ||
               capability_str == "latencyStats") {
      seller_capabilities->allows_latency_stats = true;
    } else {
      // For forward compatibility with new values, don't throw.
      continue;
    }
  }
  return seller_capabilities;
}

bool CopySellerCapabilitiesFromIdlToMojo(
    const ExecutionContext& execution_context,
    ExceptionState& exception_state,
    const AuctionAdInterestGroup& input,
    mojom::blink::InterestGroup& output) {
  output.all_sellers_capabilities = mojom::blink::SellerCapabilities::New();
  if (!input.hasSellerCapabilities())
    return true;

  for (const auto& [origin_string, capabilities_vector] :
       input.sellerCapabilities()) {
    mojom::blink::SellerCapabilitiesPtr seller_capabilities =
        ConvertSellerCapabilitiesTypeFromIdlToMojo(execution_context,
                                                   capabilities_vector);
    if (origin_string == "*") {
      output.all_sellers_capabilities = std::move(seller_capabilities);
    } else {
      if (!output.seller_capabilities)
        output.seller_capabilities.emplace();
      output.seller_capabilities->insert(
          SecurityOrigin::CreateFromString(origin_string),
          std::move(seller_capabilities));
    }
  }

  return true;
}

bool CopyExecutionModeFromIdlToMojo(const ExecutionContext& execution_context,
                                    ExceptionState& exception_state,
                                    const AuctionAdInterestGroup& input,
                                    mojom::blink::InterestGroup& output) {
  if (!input.hasExecutionMode())
    return true;
  const bool used_deprecated_names = input.executionMode() == "groupByOrigin";
  base::UmaHistogramBoolean(
      "Ads.InterestGroup.EnumNaming.Renderer.WorkletExecutionMode",
      used_deprecated_names);
  if (used_deprecated_names) {
    ConsoleWarnDeprecatedEnum(execution_context, "executionMode",
                              input.executionMode());
  }

  // TODO(crbug.com/1330341): Support "frozen-context".
  if (input.executionMode() == "compatibility") {
    output.execution_mode =
        mojom::blink::InterestGroup::ExecutionMode::kCompatibilityMode;
  } else if (input.executionMode() == "group-by-origin" ||
             input.executionMode() == "groupByOrigin") {
    output.execution_mode =
        mojom::blink::InterestGroup::ExecutionMode::kGroupedByOriginMode;
  } else if (input.executionMode() == "frozen-context") {
    output.execution_mode =
        mojom::blink::InterestGroup::ExecutionMode::kFrozenContext;
  }
  // For forward compatibility with new values, don't throw if unrecognized enum
  // values encountered.
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

bool CopyWasmHelperUrlFromIdlToMojo(const ExecutionContext& context,
                                    ExceptionState& exception_state,
                                    const AuctionAdInterestGroup& input,
                                    mojom::blink::InterestGroup& output) {
  if (!input.hasBiddingWasmHelperUrl())
    return true;
  KURL wasm_url = context.CompleteURL(input.biddingWasmHelperUrl());
  if (!wasm_url.IsValid()) {
    exception_state.ThrowTypeError(ErrorInvalidInterestGroup(
        input, "biddingWasmHelperUrl", input.biddingWasmHelperUrl(),
        "cannot be resolved to a valid URL."));
    return false;
  }
  // ValidateBlinkInterestGroup will checks whether this follows all the rules.
  output.bidding_wasm_helper_url = wasm_url;
  return true;
}

bool CopyUpdateUrlFromIdlToMojo(const ExecutionContext& context,
                                ExceptionState& exception_state,
                                const AuctionAdInterestGroup& input,
                                mojom::blink::InterestGroup& output) {
  if (input.hasUpdateUrl()) {
    if (input.hasDailyUpdateUrl() &&
        input.updateUrl() != input.dailyUpdateUrl()) {
      exception_state.ThrowTypeError(ErrorInvalidInterestGroup(
          input, "updateUrl", input.updateUrl(),
          "must match dailyUpdateUrl, when both are present."));
      return false;
    }
    KURL update_url = context.CompleteURL(input.updateUrl());
    if (!update_url.IsValid()) {
      exception_state.ThrowTypeError(
          ErrorInvalidInterestGroup(input, "updateUrl", input.updateUrl(),
                                    "cannot be resolved to a valid URL."));
      return false;
    }
    output.update_url = update_url;
    return true;
  }
  if (input.hasDailyUpdateUrl()) {
    KURL daily_update_url = context.CompleteURL(input.dailyUpdateUrl());
    if (!daily_update_url.IsValid()) {
      exception_state.ThrowTypeError(ErrorInvalidInterestGroup(
          input, "dailyUpdateUrl", input.dailyUpdateUrl(),
          "cannot be resolved to a valid URL."));
      return false;
    }
    output.update_url = daily_update_url;
  }
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
  if (!Jsonify(script_state, input.userBiddingSignals().V8Value(),
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
    if (ad->hasSizeGroup()) {
      mojo_ad->size_group = ad->sizeGroup();
    }
    if (ad->hasMetadata()) {
      if (!Jsonify(script_state, ad->metadata().V8Value(), mojo_ad->metadata)) {
        exception_state.ThrowTypeError(
            ErrorInvalidInterestGroupJson(input, "ad metadata"));
        return false;
      }
    }
    output.ads->push_back(std::move(mojo_ad));
  }
  return true;
}

bool CopyAdComponentsFromIdlToMojo(const ExecutionContext& context,
                                   const ScriptState& script_state,
                                   ExceptionState& exception_state,
                                   const AuctionAdInterestGroup& input,
                                   mojom::blink::InterestGroup& output) {
  if (!input.hasAdComponents())
    return true;
  output.ad_components.emplace();
  for (const auto& ad : input.adComponents()) {
    auto mojo_ad = mojom::blink::InterestGroupAd::New();
    KURL render_url = context.CompleteURL(ad->renderUrl());
    if (!render_url.IsValid()) {
      exception_state.ThrowTypeError(
          ErrorInvalidInterestGroup(input, "ad renderUrl", ad->renderUrl(),
                                    "cannot be resolved to a valid URL."));
      return false;
    }
    mojo_ad->render_url = render_url;
    if (ad->hasSizeGroup()) {
      mojo_ad->size_group = ad->sizeGroup();
    }
    if (ad->hasMetadata()) {
      if (!Jsonify(script_state, ad->metadata().V8Value(), mojo_ad->metadata)) {
        exception_state.ThrowTypeError(
            ErrorInvalidInterestGroupJson(input, "ad metadata"));
        return false;
      }
    }
    output.ad_components->push_back(std::move(mojo_ad));
  }
  return true;
}

bool CopyAdSizesFromIdlToMojo(const ExecutionContext& context,
                              const ScriptState& script_state,
                              ExceptionState& exception_state,
                              const AuctionAdInterestGroup& input,
                              mojom::blink::InterestGroup& output) {
  if (!input.hasAdSizes()) {
    return true;
  }
  output.ad_sizes.emplace();
  for (const auto& [name, size] : input.adSizes()) {
    auto [width_val, width_units] =
        blink::ParseAdSizeString(size->width().Ascii());
    auto [height_val, height_units] =
        blink::ParseAdSizeString(size->height().Ascii());

    output.ad_sizes->insert(
        name, mojom::blink::AdSize::New(width_val, width_units, height_val,
                                        height_units));
  }
  return true;
}

bool CopySizeGroupsFromIdlToMojo(const ExecutionContext& context,
                                 const ScriptState& script_state,
                                 ExceptionState& exception_state,
                                 const AuctionAdInterestGroup& input,
                                 mojom::blink::InterestGroup& output) {
  if (!input.hasSizeGroups()) {
    return true;
  }
  output.size_groups.emplace();
  for (const auto& group : input.sizeGroups()) {
    output.size_groups->insert(group.first, group.second);
  }
  return true;
}

// createAdRequest copy functions.
bool CopyAdRequestUrlFromIdlToMojo(const ExecutionContext& context,
                                   ExceptionState& exception_state,
                                   const AdRequestConfig& input,
                                   mojom::blink::AdRequestConfig& output) {
  KURL ad_request_url = context.CompleteURL(input.adRequestUrl());
  if (!ad_request_url.IsValid() ||
      (ad_request_url.Protocol() != url::kHttpsScheme)) {
    exception_state.ThrowTypeError(
        String::Format("adRequestUrl '%s' for AdRequestConfig must "
                       "be a valid https origin.",
                       input.adRequestUrl().Utf8().c_str()));
    return false;
  }
  output.ad_request_url = ad_request_url;
  return true;
}

bool CopyAdPropertiesFromIdlToMojo(const ExecutionContext& context,
                                   ExceptionState& exception_state,
                                   const AdRequestConfig& input,
                                   mojom::blink::AdRequestConfig& output) {
  if (!input.hasAdProperties()) {
    exception_state.ThrowTypeError(
        ErrorInvalidAdRequestConfig(input, "adProperties", input.adRequestUrl(),
                                    "must be provided to createAdRequest."));
    return false;
  }

  // output.ad_properties = mojom::blink::AdProperties::New();
  switch (input.adProperties()->GetContentType()) {
    case V8UnionAdPropertiesOrAdPropertiesSequence::ContentType::
        kAdProperties: {
      const auto* ad_properties = input.adProperties()->GetAsAdProperties();
      auto mojo_ad_properties = mojom::blink::AdProperties::New();
      mojo_ad_properties->width =
          ad_properties->hasWidth() ? ad_properties->width() : "";
      mojo_ad_properties->height =
          ad_properties->hasHeight() ? ad_properties->height() : "";
      mojo_ad_properties->slot =
          ad_properties->hasSlot() ? ad_properties->slot() : "";
      mojo_ad_properties->lang =
          ad_properties->hasLang() ? ad_properties->lang() : "";
      mojo_ad_properties->ad_type =
          ad_properties->hasAdtype() ? ad_properties->adtype() : "";
      mojo_ad_properties->bid_floor =
          ad_properties->hasBidFloor() ? ad_properties->bidFloor() : 0.0;

      output.ad_properties.push_back(std::move(mojo_ad_properties));
      break;
    }
    case V8UnionAdPropertiesOrAdPropertiesSequence::ContentType::
        kAdPropertiesSequence: {
      if (input.adProperties()->GetAsAdPropertiesSequence().size() <= 0) {
        exception_state.ThrowTypeError(ErrorInvalidAdRequestConfig(
            input, "adProperties", input.adRequestUrl(),
            "must be non-empty to createAdRequest."));
        return false;
      }

      for (const auto& ad_properties :
           input.adProperties()->GetAsAdPropertiesSequence()) {
        auto mojo_ad_properties = mojom::blink::AdProperties::New();
        mojo_ad_properties->width =
            ad_properties->hasWidth() ? ad_properties->width() : "";
        mojo_ad_properties->height =
            ad_properties->hasHeight() ? ad_properties->height() : "";
        mojo_ad_properties->slot =
            ad_properties->hasSlot() ? ad_properties->slot() : "";
        mojo_ad_properties->lang =
            ad_properties->hasLang() ? ad_properties->lang() : "";
        mojo_ad_properties->ad_type =
            ad_properties->hasAdtype() ? ad_properties->adtype() : "";
        mojo_ad_properties->bid_floor =
            ad_properties->hasBidFloor() ? ad_properties->bidFloor() : 0.0;

        output.ad_properties.push_back(std::move(mojo_ad_properties));
      }
      break;
    }
  }
  return true;
}

bool CopyTargetingFromIdlToMojo(const ExecutionContext& context,
                                ExceptionState& exception_state,
                                const AdRequestConfig& input,
                                mojom::blink::AdRequestConfig& output) {
  if (!input.hasTargeting()) {
    // Targeting information is not required.
    return true;
  }

  output.targeting = mojom::blink::AdTargeting::New();

  if (input.targeting()->hasInterests()) {
    output.targeting->interests.emplace();
    for (const auto& interest : input.targeting()->interests()) {
      output.targeting->interests->push_back(interest);
    }
  }

  if (input.targeting()->hasGeolocation()) {
    output.targeting->geolocation = mojom::blink::AdGeolocation::New();
    output.targeting->geolocation->latitude =
        input.targeting()->geolocation()->latitude();
    output.targeting->geolocation->longitude =
        input.targeting()->geolocation()->longitude();
  }

  return true;
}

bool CopyAdSignalsFromIdlToMojo(const ExecutionContext& context,
                                ExceptionState& exception_state,
                                const AdRequestConfig& input,
                                mojom::blink::AdRequestConfig& output) {
  if (!input.hasAnonymizedProxiedSignals()) {
    // AdSignals information is not required.
    return true;
  }

  output.anonymized_proxied_signals.emplace();

  for (const auto& signal : input.anonymizedProxiedSignals()) {
    if (signal == "coarse-geolocation") {
      output.anonymized_proxied_signals->push_back(
          blink::mojom::AdSignals::kCourseGeolocation);
    } else if (signal == "coarse-ua") {
      output.anonymized_proxied_signals->push_back(
          blink::mojom::AdSignals::kCourseUserAgent);
    } else if (signal == "targeting") {
      output.anonymized_proxied_signals->push_back(
          blink::mojom::AdSignals::kTargeting);
    } else if (signal == "user-ad-interests") {
      output.anonymized_proxied_signals->push_back(
          blink::mojom::AdSignals::kUserAdInterests);
    }
  }
  return true;
}

bool CopyFallbackSourceFromIdlToMojo(const ExecutionContext& context,
                                     ExceptionState& exception_state,
                                     const AdRequestConfig& input,
                                     mojom::blink::AdRequestConfig& output) {
  if (!input.hasFallbackSource()) {
    // FallbackSource information is not required.
    return true;
  }

  KURL fallback_source = context.CompleteURL(input.fallbackSource());
  if (!fallback_source.IsValid() ||
      (fallback_source.Protocol() != url::kHttpsScheme)) {
    exception_state.ThrowTypeError(
        String::Format("fallbackSource '%s' for AdRequestConfig must "
                       "be a valid https origin.",
                       input.fallbackSource().Utf8().c_str()));
    return false;
  }
  output.fallback_source = fallback_source;
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

  // Need to check scheme of the URL in addition to comparing origins because
  // FLEDGE currently only supports HTTPS URLs, and some non-HTTPS URLs can have
  // HTTPS origins.
  if (decision_logic_url.Protocol() != url::kHttpsScheme ||
      !output.seller->IsSameOriginWith(
          SecurityOrigin::Create(decision_logic_url).get())) {
    exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
        input, "decisionLogicUrl", input.decisionLogicUrl(),
        "must match seller origin."));
    return false;
  }

  output.decision_logic_url = decision_logic_url;
  return true;
}

bool CopyTrustedScoringSignalsFromIdlToMojo(
    const ExecutionContext& context,
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasTrustedScoringSignalsUrl())
    return true;
  KURL trusted_scoring_signals_url =
      context.CompleteURL(input.trustedScoringSignalsUrl());
  if (!trusted_scoring_signals_url.IsValid()) {
    exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
        input, "trustedScoringSignalsUrl", input.trustedScoringSignalsUrl(),
        "cannot be resolved to a valid URL."));
    return false;
  }

  // Need to check scheme of the URL in addition to comparing origins because
  // FLEDGE currently only supports HTTPS URLs, and some non-HTTPS URLs can have
  // HTTPS origins.
  if (trusted_scoring_signals_url.Protocol() != url::kHttpsScheme ||
      !output.seller->IsSameOriginWith(
          SecurityOrigin::Create(trusted_scoring_signals_url).get())) {
    exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
        input, "trustedScoringSignalsUrl", input.trustedScoringSignalsUrl(),
        "must match seller origin."));
    return false;
  }

  output.trusted_scoring_signals_url = trusted_scoring_signals_url;
  return true;
}

bool CopyInterestGroupBuyersFromIdlToMojo(
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  DCHECK(!output.auction_ad_config_non_shared_params->interest_group_buyers);

  if (!input.hasInterestGroupBuyers())
    return true;

  Vector<scoped_refptr<const SecurityOrigin>> buyers;
  for (const auto& buyer_str : input.interestGroupBuyers()) {
    scoped_refptr<const SecurityOrigin> buyer = ParseOrigin(buyer_str);
    if (!buyer) {
      exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
          input, "interestGroupBuyers buyer", buyer_str,
          "must be a valid https origin."));
      return false;
    }
    buyers.push_back(buyer);
  }
  output.auction_ad_config_non_shared_params->interest_group_buyers =
      std::move(buyers);
  return true;
}

mojom::blink::AuctionAdConfigMaybePromiseJsonPtr
ConvertJsonPromiseFromIdlToMojo(
    NavigatorAuction::AuctionHandle* auction_handle,
    mojom::blink::AuctionAdConfigAuctionId* auction_id,
    ScriptState& script_state,
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    const ScriptValue& input_value,
    mojom::blink::AuctionAdConfigField field,
    const char* field_name) {
  v8::Local<v8::Value> value = input_value.V8Value();

  if (auction_handle && value->IsPromise()) {
    ScriptPromise promise(&script_state, value);
    auction_handle->AttachPromiseHandler(
        script_state, promise,
        MakeGarbageCollected<NavigatorAuction::AuctionHandle::JsonResolved>(
            auction_handle, auction_id->Clone(), field, input.seller(),
            field_name));
    return mojom::blink::AuctionAdConfigMaybePromiseJson::NewPromise(0);
  } else {
    String json_payload;
    if (!Jsonify(script_state, value, json_payload)) {
      exception_state.ThrowTypeError(
          ErrorInvalidAuctionConfigJson(input, field_name));
      return nullptr;
    }

    return mojom::blink::AuctionAdConfigMaybePromiseJson::NewValue(
        json_payload);
  }
}

// null `auction_handle` disables promise handling.
// `auction_id` should be null iff `auction_handle` is.
bool CopyAuctionSignalsFromIdlToMojo(
    NavigatorAuction::AuctionHandle* auction_handle,
    mojom::blink::AuctionAdConfigAuctionId* auction_id,
    ScriptState& script_state,
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  DCHECK_EQ(auction_id == nullptr, auction_handle == nullptr);

  if (!input.hasAuctionSignals()) {
    output.auction_ad_config_non_shared_params->auction_signals =
        mojom::blink::AuctionAdConfigMaybePromiseJson::NewValue(String());
    return true;
  }

  output.auction_ad_config_non_shared_params->auction_signals =
      ConvertJsonPromiseFromIdlToMojo(
          auction_handle, auction_id, script_state, exception_state, input,
          input.auctionSignals(),
          mojom::blink::AuctionAdConfigField::kAuctionSignals,
          "auctionSignals");
  return !output.auction_ad_config_non_shared_params->auction_signals.is_null();
}

bool CopySellerSignalsFromIdlToMojo(
    NavigatorAuction::AuctionHandle* auction_handle,
    mojom::blink::AuctionAdConfigAuctionId* auction_id,
    ScriptState& script_state,
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasSellerSignals()) {
    output.auction_ad_config_non_shared_params->seller_signals =
        mojom::blink::AuctionAdConfigMaybePromiseJson::NewValue(String());
    return true;
  }

  output.auction_ad_config_non_shared_params->seller_signals =
      ConvertJsonPromiseFromIdlToMojo(
          auction_handle, auction_id, script_state, exception_state, input,
          input.sellerSignals(),
          mojom::blink::AuctionAdConfigField::kSellerSignals, "sellerSignals");
  return !output.auction_ad_config_non_shared_params->seller_signals.is_null();
}

// Attempts to build a DirectFromSellerSignalsSubresource. If there is no
// registered subresource URL `subresource_url` returns nullptr -- processing
// may continue with the next `subresource_url`.
mojom::blink::DirectFromSellerSignalsSubresourcePtr
TryToBuildDirectFromSellerSignalsSubresource(
    const KURL& subresource_url,
    const SecurityOrigin& seller,
    ExceptionState& exception_state,
    const ResourceFetcher& resource_fetcher) {
  DCHECK(subresource_url.IsValid());
  DCHECK(
      subresource_url.ProtocolIs(url::kHttpsScheme) &&
      seller.IsSameOriginWith(SecurityOrigin::Create(subresource_url).get()));
  // NOTE: If subresource bundles are disabled, GetSubresourceBundleToken() will
  // always return absl::nullopt.
  absl::optional<base::UnguessableToken> token =
      resource_fetcher.GetSubresourceBundleToken(subresource_url);
  if (!token)
    return nullptr;
  absl::optional<KURL> bundle_url =
      resource_fetcher.GetSubresourceBundleSourceUrl(subresource_url);
  DCHECK(bundle_url->ProtocolIs(url::kHttpsScheme));
  DCHECK(seller.IsSameOriginWith(SecurityOrigin::Create(*bundle_url).get()));
  auto mojo_bundle = mojom::blink::DirectFromSellerSignalsSubresource::New();
  mojo_bundle->token = *token;
  mojo_bundle->bundle_url = *bundle_url;
  return mojo_bundle;
}

mojom::blink::DirectFromSellerSignalsPtr
ConvertDirectFromSellerSignalsFromV8ToMojo(
    const ScriptState& script_state,
    const ExecutionContext& context,
    ExceptionState& exception_state,
    const ResourceFetcher& resource_fetcher,
    const String& seller_name,
    const SecurityOrigin& seller_origin,
    const absl::optional<Vector<scoped_refptr<const SecurityOrigin>>>&
        interest_group_buyers,
    v8::Local<v8::Value> value) {
  String prefix_string = NativeValueTraits<IDLUSVString>::NativeValue(
      script_state.GetIsolate(), value, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  const KURL direct_from_seller_signals_prefix =
      context.CompleteURL(prefix_string);
  if (!direct_from_seller_signals_prefix.IsValid()) {
    exception_state.ThrowTypeError(ErrorInvalidAuctionConfigSeller(
        seller_name, "directFromSellerSignals", prefix_string,
        "cannot be resolved to a valid URL."));
    return nullptr;
  }
  if (!direct_from_seller_signals_prefix.ProtocolIs(url::kHttpsScheme) ||
      !seller_origin.IsSameOriginWith(
          SecurityOrigin::Create(direct_from_seller_signals_prefix).get())) {
    exception_state.ThrowTypeError(ErrorInvalidAuctionConfigSeller(
        seller_name, "directFromSellerSignals", prefix_string,
        "must match seller origin; only https scheme is supported."));
    return nullptr;
  }
  if (!direct_from_seller_signals_prefix.Query().empty()) {
    exception_state.ThrowTypeError(ErrorInvalidAuctionConfigSeller(
        seller_name, "directFromSellerSignals", prefix_string,
        "URL prefix must not have a query string."));
    return nullptr;
  }
  auto mojo_direct_from_seller_signals =
      mojom::blink::DirectFromSellerSignals::New();
  mojo_direct_from_seller_signals->prefix = direct_from_seller_signals_prefix;

  if (interest_group_buyers) {
    for (scoped_refptr<const SecurityOrigin> buyer : *interest_group_buyers) {
      // Replace "/" with "%2F" to match the behavior of
      // base::EscapeQueryParamValue(). Also, the subresource won't be found if
      // the URL doesn't match.
      const KURL subresource_url(
          direct_from_seller_signals_prefix.GetString() + "?perBuyerSignals=" +
          EncodeWithURLEscapeSequences(buyer->ToString()).Replace("/", "%2F"));
      mojom::blink::DirectFromSellerSignalsSubresourcePtr maybe_mojo_bundle =
          TryToBuildDirectFromSellerSignalsSubresource(
              subresource_url, seller_origin, exception_state,
              resource_fetcher);
      if (!maybe_mojo_bundle)
        continue;  // The bundle wasn't found, try the next one.
      mojo_direct_from_seller_signals->per_buyer_signals.insert(
          buyer, std::move(maybe_mojo_bundle));
    }
  }

  {
    const KURL subresource_url(direct_from_seller_signals_prefix.GetString() +
                               "?sellerSignals");
    mojom::blink::DirectFromSellerSignalsSubresourcePtr maybe_mojo_bundle =
        TryToBuildDirectFromSellerSignalsSubresource(
            subresource_url, seller_origin, exception_state, resource_fetcher);
    // May be null if the signals weren't found.
    mojo_direct_from_seller_signals->seller_signals =
        std::move(maybe_mojo_bundle);
  }

  {
    const KURL subresource_url(direct_from_seller_signals_prefix.GetString() +
                               "?auctionSignals");
    mojom::blink::DirectFromSellerSignalsSubresourcePtr maybe_mojo_bundle =
        TryToBuildDirectFromSellerSignalsSubresource(
            subresource_url, seller_origin, exception_state, resource_fetcher);
    // May be null if the signals weren't found.
    mojo_direct_from_seller_signals->auction_signals =
        std::move(maybe_mojo_bundle);
  }

  return mojo_direct_from_seller_signals;
}

bool CopyDirectFromSellerSignalsFromIdlToMojo(
    NavigatorAuction::AuctionHandle* auction_handle,
    const mojom::blink::AuctionAdConfigAuctionId* auction_id,
    ScriptState& script_state,
    const ExecutionContext& context,
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    const ResourceFetcher& resource_fetcher,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasDirectFromSellerSignals()) {
    output.direct_from_seller_signals = mojom::blink::
        AuctionAdConfigMaybePromiseDirectFromSellerSignals::NewValue(nullptr);
    return true;
  }

  v8::Local<v8::Value> value = input.directFromSellerSignals().V8Value();
  if (auction_handle && value->IsPromise()) {
    ScriptPromise promise(&script_state, value);
    auction_handle->AttachPromiseHandler(
        script_state, promise,
        MakeGarbageCollected<
            NavigatorAuction::AuctionHandle::DirectFromSellerSignalsResolved>(
            auction_handle, auction_id->Clone(), input.seller(), output.seller,
            output.auction_ad_config_non_shared_params->interest_group_buyers));
    output.direct_from_seller_signals = mojom::blink::
        AuctionAdConfigMaybePromiseDirectFromSellerSignals::NewPromise(0);
    return true;
  }

  auto direct_from_seller_signals = ConvertDirectFromSellerSignalsFromV8ToMojo(
      script_state, context, exception_state, resource_fetcher, input.seller(),
      *output.seller,
      output.auction_ad_config_non_shared_params->interest_group_buyers, value);
  if (direct_from_seller_signals) {
    output.direct_from_seller_signals =
        mojom::blink::AuctionAdConfigMaybePromiseDirectFromSellerSignals::
            NewValue(std::move(direct_from_seller_signals));
    return true;
  }

  return false;
}

// Returns nullopt + sets exception on failure, or returns a concrete value.
absl::optional<HashMap<scoped_refptr<const SecurityOrigin>, String>>
ConvertNonPromisePerBuyerSignalsFromV8ToMojo(const ScriptState& script_state,
                                             ExceptionState& exception_state,
                                             const String& seller_name,
                                             v8::Local<v8::Value> value) {
  HeapVector<std::pair<WTF::String, blink::ScriptValue>> decoded =
      NativeValueTraits<IDLRecord<IDLUSVString, IDLAny>>::NativeValue(
          script_state.GetIsolate(), value, exception_state);
  if (exception_state.HadException()) {
    return absl::nullopt;
  }

  absl::optional<HashMap<scoped_refptr<const SecurityOrigin>, String>>
      per_buyer_signals;

  per_buyer_signals.emplace();
  for (const auto& per_buyer_signal : decoded) {
    scoped_refptr<const SecurityOrigin> buyer =
        ParseOrigin(per_buyer_signal.first);
    if (!buyer) {
      exception_state.ThrowTypeError(ErrorInvalidAuctionConfigSeller(
          seller_name, "perBuyerSignals buyer", per_buyer_signal.first,
          "must be a valid https origin."));
      return absl::nullopt;
    }
    String buyer_signals_str;
    if (!Jsonify(script_state, per_buyer_signal.second.V8Value(),
                 buyer_signals_str)) {
      exception_state.ThrowTypeError(
          ErrorInvalidAuctionConfigSellerJson(seller_name, "perBuyerSignals"));
      return absl::nullopt;
    }
    per_buyer_signals->insert(buyer, std::move(buyer_signals_str));
  }

  return per_buyer_signals;
}

bool CopyPerBuyerSignalsFromIdlToMojo(
    NavigatorAuction::AuctionHandle* auction_handle,
    const mojom::blink::AuctionAdConfigAuctionId* auction_id,
    ScriptState& script_state,
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasPerBuyerSignals()) {
    output.auction_ad_config_non_shared_params->per_buyer_signals =
        mojom::blink::AuctionAdConfigMaybePromisePerBuyerSignals::NewValue(
            absl::nullopt);
    return true;
  }

  v8::Local<v8::Value> value = input.perBuyerSignals().V8Value();
  if (auction_handle && value->IsPromise()) {
    ScriptPromise promise(&script_state, value);
    auction_handle->AttachPromiseHandler(
        script_state, promise,
        MakeGarbageCollected<
            NavigatorAuction::AuctionHandle::PerBuyerSignalsResolved>(
            auction_handle, auction_id->Clone(), input.seller()));
    output.auction_ad_config_non_shared_params->per_buyer_signals =
        mojom::blink::AuctionAdConfigMaybePromisePerBuyerSignals::NewPromise(0);
    return true;
  }

  auto per_buyer_signals = ConvertNonPromisePerBuyerSignalsFromV8ToMojo(
      script_state, exception_state, input.seller(), value);
  if (per_buyer_signals.has_value()) {
    output.auction_ad_config_non_shared_params->per_buyer_signals =
        mojom::blink::AuctionAdConfigMaybePromisePerBuyerSignals::NewValue(
            std::move(per_buyer_signals));
    return true;
  }

  return false;
}

// Returns nullptr + sets exception on failure, or returns a concrete value.
//
// This is shared logic for `perBuyerTimeouts` and `perBuyerCumulativeTimeouts`,
// with `field` indicating which name to use in error messages. The logic is
// identical in both cases.
mojom::blink::AuctionAdConfigBuyerTimeoutsPtr
ConvertNonPromisePerBuyerTimeoutsFromV8ToMojo(
    const ScriptState& script_state,
    ExceptionState& exception_state,
    const String& seller_name,
    v8::Local<v8::Value> value,
    mojom::blink::AuctionAdConfigBuyerTimeoutField field) {
  Vector<std::pair<String, uint64_t>> decoded =
      NativeValueTraits<IDLRecord<IDLUSVString, IDLUnsignedLongLong>>::
          NativeValue(script_state.GetIsolate(), value, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  mojom::blink::AuctionAdConfigBuyerTimeoutsPtr buyer_timeouts =
      mojom::blink::AuctionAdConfigBuyerTimeouts::New();
  buyer_timeouts->per_buyer_timeouts.emplace();
  for (const auto& per_buyer_timeout : decoded) {
    if (per_buyer_timeout.first == "*") {
      buyer_timeouts->all_buyers_timeout =
          base::Milliseconds(per_buyer_timeout.second);
      continue;
    }
    scoped_refptr<const SecurityOrigin> buyer =
        ParseOrigin(per_buyer_timeout.first);
    if (!buyer) {
      String field_name;
      switch (field) {
        case mojom::blink::AuctionAdConfigBuyerTimeoutField::kPerBuyerTimeouts:
          field_name = "perBuyerTimeouts buyer";
          break;
        case mojom::blink::AuctionAdConfigBuyerTimeoutField::
            kPerBuyerCumulativeTimeouts:
          field_name = "perBuyerCumulativeTimeouts buyer";
          break;
      }
      exception_state.ThrowTypeError(ErrorInvalidAuctionConfigSeller(
          seller_name, field_name, per_buyer_timeout.first,
          "must be \"*\" (wildcard) or a valid https origin."));
      return nullptr;
    }
    buyer_timeouts->per_buyer_timeouts->insert(
        buyer, base::Milliseconds(per_buyer_timeout.second));
  }

  return buyer_timeouts;
}

bool CopyPerBuyerTimeoutsFromIdlToMojo(
    NavigatorAuction::AuctionHandle* auction_handle,
    const mojom::blink::AuctionAdConfigAuctionId* auction_id,
    ScriptState& script_state,
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasPerBuyerTimeouts()) {
    output.auction_ad_config_non_shared_params->buyer_timeouts =
        mojom::blink::AuctionAdConfigMaybePromiseBuyerTimeouts::NewValue(
            mojom::blink::AuctionAdConfigBuyerTimeouts::New());
    return true;
  }

  v8::Local<v8::Value> value = input.perBuyerTimeouts().V8Value();
  if (auction_handle && value->IsPromise()) {
    ScriptPromise promise(&script_state, value);
    auction_handle->AttachPromiseHandler(
        script_state, promise,
        MakeGarbageCollected<
            NavigatorAuction::AuctionHandle::BuyerTimeoutsResolved>(
            auction_handle, auction_id->Clone(),
            mojom::blink::AuctionAdConfigBuyerTimeoutField::kPerBuyerTimeouts,
            input.seller()));
    output.auction_ad_config_non_shared_params->buyer_timeouts =
        mojom::blink::AuctionAdConfigMaybePromiseBuyerTimeouts::NewPromise(0);
    return true;
  }

  mojom::blink::AuctionAdConfigBuyerTimeoutsPtr buyer_timeouts =
      ConvertNonPromisePerBuyerTimeoutsFromV8ToMojo(
          script_state, exception_state, input.seller(), value,
          mojom::blink::AuctionAdConfigBuyerTimeoutField::kPerBuyerTimeouts);
  if (buyer_timeouts) {
    output.auction_ad_config_non_shared_params->buyer_timeouts =
        mojom::blink::AuctionAdConfigMaybePromiseBuyerTimeouts::NewValue(
            std::move(buyer_timeouts));
    return true;
  }
  return false;
}

bool CopyPerBuyerCumulativeTimeoutsFromIdlToMojo(
    NavigatorAuction::AuctionHandle* auction_handle,
    const mojom::blink::AuctionAdConfigAuctionId* auction_id,
    ScriptState& script_state,
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasPerBuyerCumulativeTimeouts()) {
    output.auction_ad_config_non_shared_params->buyer_cumulative_timeouts =
        mojom::blink::AuctionAdConfigMaybePromiseBuyerTimeouts::NewValue(
            mojom::blink::AuctionAdConfigBuyerTimeouts::New());
    return true;
  }

  v8::Local<v8::Value> value = input.perBuyerCumulativeTimeouts().V8Value();
  if (auction_handle && value->IsPromise()) {
    ScriptPromise promise(&script_state, value);
    auction_handle->AttachPromiseHandler(
        script_state, promise,
        MakeGarbageCollected<
            NavigatorAuction::AuctionHandle::BuyerTimeoutsResolved>(
            auction_handle, auction_id->Clone(),
            mojom::blink::AuctionAdConfigBuyerTimeoutField::
                kPerBuyerCumulativeTimeouts,
            input.seller()));
    output.auction_ad_config_non_shared_params->buyer_cumulative_timeouts =
        mojom::blink::AuctionAdConfigMaybePromiseBuyerTimeouts::NewPromise(0);
    return true;
  }

  mojom::blink::AuctionAdConfigBuyerTimeoutsPtr buyer_cumulative_timeouts =
      ConvertNonPromisePerBuyerTimeoutsFromV8ToMojo(
          script_state, exception_state, input.seller(), value,
          mojom::blink::AuctionAdConfigBuyerTimeoutField::
              kPerBuyerCumulativeTimeouts);
  if (buyer_cumulative_timeouts) {
    output.auction_ad_config_non_shared_params->buyer_cumulative_timeouts =
        mojom::blink::AuctionAdConfigMaybePromiseBuyerTimeouts::NewValue(
            std::move(buyer_cumulative_timeouts));
    return true;
  }
  return false;
}

// Returns nullptr + sets exception on failure, or returns a concrete value.
mojom::blink::AuctionAdConfigBuyerCurrenciesPtr
ConvertNonPromisePerBuyerCurrenciesFromV8ToMojo(const ScriptState& script_state,
                                                ExceptionState& exception_state,
                                                const String& seller_name,
                                                v8::Local<v8::Value> value) {
  Vector<std::pair<String, String>> decoded =
      NativeValueTraits<IDLRecord<IDLUSVString, IDLUSVString>>::NativeValue(
          script_state.GetIsolate(), value, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  mojom::blink::AuctionAdConfigBuyerCurrenciesPtr buyer_currencies =
      mojom::blink::AuctionAdConfigBuyerCurrencies::New();
  buyer_currencies->per_buyer_currencies.emplace();
  for (const auto& per_buyer_currency : decoded) {
    std::string per_buyer_currency_str = per_buyer_currency.second.Ascii();
    if (!IsValidAdCurrencyCode(per_buyer_currency_str)) {
      exception_state.ThrowTypeError(ErrorInvalidAuctionConfigSeller(
          seller_name, "perBuyerCurrencies currency", per_buyer_currency.second,
          "must be a 3-letter uppercase currency code."));
      return nullptr;
    }

    if (per_buyer_currency.first == "*") {
      buyer_currencies->all_buyers_currency =
          blink::AdCurrency::From(per_buyer_currency_str);
      continue;
    }
    scoped_refptr<const SecurityOrigin> buyer =
        ParseOrigin(per_buyer_currency.first);
    if (!buyer) {
      exception_state.ThrowTypeError(ErrorInvalidAuctionConfigSeller(
          seller_name, "perBuyerCurrencies buyer", per_buyer_currency.first,
          "must be \"*\" (wildcard) or a valid https origin."));
      return nullptr;
    }
    buyer_currencies->per_buyer_currencies->insert(
        buyer, blink::AdCurrency::From(per_buyer_currency_str));
  }

  return buyer_currencies;
}

bool CopyPerBuyerCurrenciesFromIdlToMojo(
    NavigatorAuction::AuctionHandle* auction_handle,
    const mojom::blink::AuctionAdConfigAuctionId* auction_id,
    ScriptState& script_state,
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasPerBuyerCurrencies()) {
    output.auction_ad_config_non_shared_params->buyer_currencies =
        mojom::blink::AuctionAdConfigMaybePromiseBuyerCurrencies::NewValue(
            mojom::blink::AuctionAdConfigBuyerCurrencies::New());
    return true;
  }

  v8::Local<v8::Value> value = input.perBuyerCurrencies().V8Value();
  if (auction_handle && value->IsPromise()) {
    ScriptPromise promise(&script_state, value);
    auction_handle->AttachPromiseHandler(
        script_state, promise,
        MakeGarbageCollected<
            NavigatorAuction::AuctionHandle::BuyerCurrenciesResolved>(
            auction_handle, auction_id->Clone(), input.seller()));
    output.auction_ad_config_non_shared_params->buyer_currencies =
        mojom::blink::AuctionAdConfigMaybePromiseBuyerCurrencies::NewPromise(0);
    return true;
  }

  mojom::blink::AuctionAdConfigBuyerCurrenciesPtr buyer_currencies =
      ConvertNonPromisePerBuyerCurrenciesFromV8ToMojo(
          script_state, exception_state, input.seller(), value);
  if (buyer_currencies) {
    output.auction_ad_config_non_shared_params->buyer_currencies =
        mojom::blink::AuctionAdConfigMaybePromiseBuyerCurrencies::NewValue(
            std::move(buyer_currencies));
    return true;
  }
  return false;
}

bool CopyPerBuyerExperimentIdsFromIdlToMojo(
    const ScriptState& script_state,
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasPerBuyerExperimentGroupIds())
    return true;
  for (const auto& per_buyer_experiment_id :
       input.perBuyerExperimentGroupIds()) {
    if (per_buyer_experiment_id.first == "*") {
      output.has_all_buyer_experiment_group_id = true;
      output.all_buyer_experiment_group_id = per_buyer_experiment_id.second;
      continue;
    }
    scoped_refptr<const SecurityOrigin> buyer =
        ParseOrigin(per_buyer_experiment_id.first);
    if (!buyer) {
      exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
          input, "perBuyerExperimentGroupIds buyer",
          per_buyer_experiment_id.first,
          "must be \"*\" (wildcard) or a valid https origin."));
      return false;
    }
    output.per_buyer_experiment_group_ids.insert(
        buyer, per_buyer_experiment_id.second);
  }

  return true;
}

bool CopyPerBuyerGroupLimitsFromIdlToMojo(
    const ScriptState& script_state,
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasPerBuyerGroupLimits())
    return true;
  for (const auto& per_buyer_group_limit : input.perBuyerGroupLimits()) {
    if (per_buyer_group_limit.second <= 0) {
      exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
          input, "perBuyerGroupLimits value",
          String::Number(per_buyer_group_limit.second),
          "must be greater than 0."));
      return false;
    }
    if (per_buyer_group_limit.first == "*") {
      output.auction_ad_config_non_shared_params->all_buyers_group_limit =
          per_buyer_group_limit.second;
      continue;
    }
    scoped_refptr<const SecurityOrigin> buyer =
        ParseOrigin(per_buyer_group_limit.first);
    if (!buyer) {
      exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
          input, "perBuyerGroupLimits buyer", per_buyer_group_limit.first,
          "must be \"*\" (wildcard) or a valid https origin."));
      return false;
    }
    output.auction_ad_config_non_shared_params->per_buyer_group_limits.insert(
        buyer, per_buyer_group_limit.second);
  }

  return true;
}

bool ConvertAuctionConfigPrioritySignalsFromIdlToMojo(
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    const Vector<std::pair<WTF::String, double>>& priority_signals_in,
    WTF::HashMap<WTF::String, double>& priority_signals_out) {
  for (const auto& key_value_pair : priority_signals_in) {
    if (key_value_pair.first.StartsWith("browserSignals.")) {
      exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
          input, "perBuyerPrioritySignals key", key_value_pair.first,
          "must not start with reserved \"browserSignals.\" prefix."));
      return false;
    }
    priority_signals_out.insert(key_value_pair.first, key_value_pair.second);
  }
  return true;
}

bool CopyPerBuyerPrioritySignalsFromIdlToMojo(
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasPerBuyerPrioritySignals())
    return true;

  output.auction_ad_config_non_shared_params->per_buyer_priority_signals
      .emplace();
  for (const auto& per_buyer_priority_signals :
       input.perBuyerPrioritySignals()) {
    WTF::HashMap<WTF::String, double> signals;
    if (!ConvertAuctionConfigPrioritySignalsFromIdlToMojo(
            exception_state, input, per_buyer_priority_signals.second,
            signals)) {
      return false;
    }
    if (per_buyer_priority_signals.first == "*") {
      output.auction_ad_config_non_shared_params->all_buyers_priority_signals =
          std::move(signals);
      continue;
    }
    scoped_refptr<const SecurityOrigin> buyer =
        ParseOrigin(per_buyer_priority_signals.first);
    if (!buyer) {
      exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
          input, "perBuyerPrioritySignals buyer",
          per_buyer_priority_signals.first,
          "must be \"*\" (wildcard) or a valid https origin."));
      return false;
    }
    output.auction_ad_config_non_shared_params->per_buyer_priority_signals
        ->insert(buyer, std::move(signals));
  }

  return true;
}

// TODO(caraitto): Consider validating keys -- no bucket base + offset
// conflicts, no overflow, etc.
bool CopyAuctionReportBuyerKeysFromIdlToMojo(
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasAuctionReportBuyerKeys()) {
    return true;
  }

  output.auction_ad_config_non_shared_params->auction_report_buyer_keys
      .emplace();
  for (const BigInt& value : input.auctionReportBuyerKeys()) {
    base::expected<absl::uint128, String> maybe_bucket =
        CopyBigIntToUint128(value);
    if (!maybe_bucket.has_value()) {
      exception_state.ThrowTypeError(ErrorInvalidAuctionConfigUint128(
          input, "auctionReportBuyerKeys", maybe_bucket.error()));
      return false;
    }
    output.auction_ad_config_non_shared_params->auction_report_buyer_keys
        ->push_back(*maybe_bucket);
  }

  return true;
}

bool CopyAuctionReportBuyersFromIdlToMojo(
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasAuctionReportBuyers()) {
    return true;
  }

  output.auction_ad_config_non_shared_params->auction_report_buyers.emplace();
  for (const auto& [report_type_string, report_config] :
       input.auctionReportBuyers()) {
    mojom::blink::AuctionAdConfigNonSharedParams::BuyerReportType report_type;
    if (report_type_string == "interestGroupCount") {
      report_type = mojom::blink::AuctionAdConfigNonSharedParams::
          BuyerReportType::kInterestGroupCount;
    } else if (report_type_string == "bidCount") {
      report_type = mojom::blink::AuctionAdConfigNonSharedParams::
          BuyerReportType::kBidCount;
    } else if (report_type_string == "totalGenerateBidLatency") {
      report_type = mojom::blink::AuctionAdConfigNonSharedParams::
          BuyerReportType::kTotalGenerateBidLatency;
    } else if (report_type_string == "totalSignalsFetchLatency") {
      report_type = mojom::blink::AuctionAdConfigNonSharedParams::
          BuyerReportType::kTotalSignalsFetchLatency;
    } else {
      // Don't throw an error if an unknown type is provided to provide forward
      // compatibility with new fields added later.
      continue;
    }
    base::expected<absl::uint128, String> maybe_bucket =
        CopyBigIntToUint128(report_config->bucket());
    if (!maybe_bucket.has_value()) {
      exception_state.ThrowTypeError(ErrorInvalidAuctionConfigUint128(
          input, "auctionReportBuyers", maybe_bucket.error()));
      return false;
    }
    output.auction_ad_config_non_shared_params->auction_report_buyers->insert(
        report_type, mojom::blink::AuctionReportBuyersConfig::New(
                         *maybe_bucket, report_config->scale()));
  }

  return true;
}

bool CopyRequiredSellerSignalsFromIdlToMojo(
    const ExecutionContext& execution_context,
    ExceptionState& exception_state,
    const AuctionAdConfig& input,
    mojom::blink::AuctionAdConfig& output) {
  output.auction_ad_config_non_shared_params->required_seller_capabilities =
      mojom::blink::SellerCapabilities::New();
  if (!input.hasRequiredSellerCapabilities()) {
    return true;
  }

  output.auction_ad_config_non_shared_params->required_seller_capabilities =
      ConvertSellerCapabilitiesTypeFromIdlToMojo(
          execution_context, input.requiredSellerCapabilities());
  return true;
}

bool CopyRequestedSizeFromIdlToMojo(const ExecutionContext& execution_context,
                                    ExceptionState& exception_state,
                                    const AuctionAdConfig& input,
                                    mojom::blink::AuctionAdConfig& output) {
  if (!input.hasRequestedSize()) {
    return true;
  }
  auto [width_val, width_units] =
      blink::ParseAdSizeString(input.requestedSize()->width().Ascii());
  auto [height_val, height_units] =
      blink::ParseAdSizeString(input.requestedSize()->height().Ascii());
  if (width_units == blink::AdSize::LengthUnit::kInvalid) {
    exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
        input, "requestedSize width", input.requestedSize()->width(),
        "must use units '', 'px', 'sw', or 'sh'."));
    return false;
  }
  if (height_units == blink::AdSize::LengthUnit::kInvalid) {
    exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
        input, "requestedSize height", input.requestedSize()->height(),
        "must use units '', 'px', 'sw', or 'sh'."));
    return false;
  }
  if (width_val <= 0 || !std::isfinite(width_val)) {
    exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
        input, "requestedSize width", input.requestedSize()->width(),
        "must be finite and positive."));
    return false;
  }
  if (height_val <= 0 || !std::isfinite(height_val)) {
    exception_state.ThrowTypeError(ErrorInvalidAuctionConfig(
        input, "requestedSize height", input.requestedSize()->height(),
        "must be finite and positive."));
    return false;
  }
  output.auction_ad_config_non_shared_params->requested_size =
      mojom::blink::AdSize::New(width_val, width_units, height_val,
                                height_units);
  return true;
}

// Attempts to convert the AuctionAdConfig `config`, passed in via Javascript,
// to a `mojom::blink::AuctionAdConfig`. Throws a Javascript exception and
// return null on failure. `auction_handle` is used for promise handling;
// if it's null, promise will not be accepted.
mojom::blink::AuctionAdConfigPtr IdlAuctionConfigToMojo(
    NavigatorAuction::AuctionHandle* auction_handle,
    bool is_top_level,
    uint32_t nested_pos,
    ScriptState& script_state,
    const ExecutionContext& context,
    ExceptionState& exception_state,
    const ResourceFetcher& resource_fetcher,
    const AuctionAdConfig& config) {
  auto mojo_config = mojom::blink::AuctionAdConfig::New();
  mojo_config->auction_ad_config_non_shared_params =
      mojom::blink::AuctionAdConfigNonSharedParams::New();
  mojom::blink::AuctionAdConfigAuctionIdPtr auction_id;
  if (is_top_level) {
    auction_id = mojom::blink::AuctionAdConfigAuctionId::NewMainAuction(0);
  } else {
    auction_id =
        mojom::blink::AuctionAdConfigAuctionId::NewComponentAuction(nested_pos);
  }

  if (!CopySellerFromIdlToMojo(exception_state, config, *mojo_config) ||
      !CopyDecisionLogicUrlFromIdlToMojo(context, exception_state, config,
                                         *mojo_config) ||
      !CopyTrustedScoringSignalsFromIdlToMojo(context, exception_state, config,
                                              *mojo_config) ||
      !CopyInterestGroupBuyersFromIdlToMojo(exception_state, config,
                                            *mojo_config) ||
      !CopyAuctionSignalsFromIdlToMojo(auction_handle, auction_id.get(),
                                       script_state, exception_state, config,
                                       *mojo_config) ||
      !CopySellerSignalsFromIdlToMojo(auction_handle, auction_id.get(),
                                      script_state, exception_state, config,
                                      *mojo_config) ||
      !CopyDirectFromSellerSignalsFromIdlToMojo(
          auction_handle, auction_id.get(), script_state, context,
          exception_state, config, resource_fetcher, *mojo_config) ||
      !CopyPerBuyerSignalsFromIdlToMojo(auction_handle, auction_id.get(),
                                        script_state, exception_state, config,
                                        *mojo_config) ||
      !CopyPerBuyerTimeoutsFromIdlToMojo(auction_handle, auction_id.get(),
                                         script_state, exception_state, config,
                                         *mojo_config) ||
      !CopyPerBuyerCumulativeTimeoutsFromIdlToMojo(
          auction_handle, auction_id.get(), script_state, exception_state,
          config, *mojo_config) ||
      !CopyPerBuyerCurrenciesFromIdlToMojo(auction_handle, auction_id.get(),
                                           script_state, exception_state,
                                           config, *mojo_config) ||
      !CopyPerBuyerExperimentIdsFromIdlToMojo(script_state, exception_state,
                                              config, *mojo_config) ||
      !CopyPerBuyerGroupLimitsFromIdlToMojo(script_state, exception_state,
                                            config, *mojo_config) ||
      !CopyPerBuyerPrioritySignalsFromIdlToMojo(exception_state, config,
                                                *mojo_config) ||
      !CopyAuctionReportBuyerKeysFromIdlToMojo(exception_state, config,
                                               *mojo_config) ||
      !CopyAuctionReportBuyersFromIdlToMojo(exception_state, config,
                                            *mojo_config) ||
      !CopyRequiredSellerSignalsFromIdlToMojo(context, exception_state, config,
                                              *mojo_config) ||
      !CopyRequestedSizeFromIdlToMojo(context, exception_state, config,
                                      *mojo_config)) {
    return mojom::blink::AuctionAdConfigPtr();
  }

  if (config.hasSellerTimeout()) {
    mojo_config->auction_ad_config_non_shared_params->seller_timeout =
        base::Milliseconds(config.sellerTimeout());
  }

  if (config.hasSellerCurrency()) {
    std::string seller_currency_str = config.sellerCurrency().Ascii();
    if (!IsValidAdCurrencyCode(seller_currency_str)) {
      exception_state.ThrowTypeError(ErrorInvalidAuctionConfigSeller(
          config.seller(), "sellerCurrency", config.sellerCurrency(),
          "must be a 3-letter uppercase currency code."));
      return mojom::blink::AuctionAdConfigPtr();
    }
    mojo_config->auction_ad_config_non_shared_params->seller_currency =
        blink::AdCurrency::From(seller_currency_str);
  }

  // Recursively handle component auctions, if there are any.
  if (config.hasComponentAuctions()) {
    if (config.componentAuctions().size() > 0 &&
        mojo_config->auction_ad_config_non_shared_params
            ->interest_group_buyers &&
        mojo_config->auction_ad_config_non_shared_params->interest_group_buyers
                ->size() > 0) {
      exception_state.ThrowTypeError(
          "Auctions may only have one of 'interestGroupBuyers' or "
          "'componentAuctions'.");
      return mojom::blink::AuctionAdConfigPtr();
    }

    for (uint32_t pos = 0; pos < config.componentAuctions().size(); ++pos) {
      const auto& idl_component_auction = config.componentAuctions()[pos];
      // Component auctions may not have their own nested component auctions.
      if (!is_top_level) {
        exception_state.ThrowTypeError(
            "Auctions listed in componentAuctions may not have their own "
            "nested componentAuctions.");
        return mojom::blink::AuctionAdConfigPtr();
      }

      auto mojo_component_auction = IdlAuctionConfigToMojo(
          auction_handle, /*is_top_level=*/false, pos, script_state, context,
          exception_state, resource_fetcher, *idl_component_auction);
      if (!mojo_component_auction)
        return mojom::blink::AuctionAdConfigPtr();
      mojo_config->auction_ad_config_non_shared_params->component_auctions
          .emplace_back(std::move(mojo_component_auction));
    }
  }

  if (config.hasSellerExperimentGroupId()) {
    mojo_config->has_seller_experiment_group_id = true;
    mojo_config->seller_experiment_group_id = config.sellerExperimentGroupId();
  }

  return mojo_config;
}

// finalizeAd() validation methods
bool ValidateAdsObject(ExceptionState& exception_state, const Ads* ads) {
  if (!ads || !ads->IsValid()) {
    exception_state.ThrowTypeError(
        "Ads used for finalizeAds() must be a valid Ads object from "
        "navigator.createAdRequest.");
    return false;
  }
  return true;
}

// Modified from
// LocalFrame::CountUseIfFeatureWouldBeBlockedByPermissionsPolicy.
//
// Checks whether or not a policy-controlled feature would be blocked by our
// restricted permissions policy EnableForSelf.
// Under EnableForSelf policy, the features will not be available in
// cross-origin document unless explicitly enabled.
// Returns true if the frame is cross-origin relative to the top-level document,
// or if it is same-origin with the top level, but is embedded in any way
// through a cross-origin frame (A->B->A embedding).
bool FeatureWouldBeBlockedByRestrictedPermissionsPolicy(Navigator& navigator) {
  const Frame* frame = navigator.DomWindow()->GetFrame();

  // Fenced Frames block all permissions, so we shouldn't end up here because
  // the policy is checked before this method is called.
  DCHECK(!frame->IsInFencedFrameTree());

  // Get the origin of the top-level document.
  const SecurityOrigin* top_origin =
      frame->Tree().Top().GetSecurityContext()->GetSecurityOrigin();

  // Walk up the frame tree looking for any cross-origin embeds. Even if this
  // frame is same-origin with the top-level, if it is embedded by a cross-
  // origin frame (like A->B->A) it would be blocked without a permissions
  // policy.
  while (!frame->IsMainFrame()) {
    if (!frame->GetSecurityContext()->GetSecurityOrigin()->CanAccess(
            top_origin)) {
      return true;
    }
    frame = frame->Tree().Parent();
  }
  return false;
}

void RecordCommonFledgeUseCounters(Document* document) {
  if (!document)
    return;
  UseCounter::Count(document, mojom::blink::WebFeature::kFledge);
  // Only record the ads APIs counter if enabled in that manner.
  if (RuntimeEnabledFeatures::PrivacySandboxAdsAPIsEnabled(
          document->GetExecutionContext())) {
    UseCounter::Count(document,
                      mojom::blink::WebFeature::kPrivacySandboxAdsAPIs);
  }
}

}  // namespace

NavigatorAuction::AuctionHandle::JsonResolved::JsonResolved(
    AuctionHandle* auction_handle,
    mojom::blink::AuctionAdConfigAuctionIdPtr auction_id,
    mojom::blink::AuctionAdConfigField field,
    const String& seller_name,
    const char* field_name)
    : auction_handle_(auction_handle),
      auction_id_(std::move(auction_id)),
      field_(field),
      seller_name_(seller_name),
      field_name_(field_name) {}

ScriptValue NavigatorAuction::AuctionHandle::JsonResolved::Call(
    ScriptState* script_state,
    ScriptValue value) {
  ExceptionState exception_state(script_state->GetIsolate(),
                                 ExceptionState::kExecutionContext,
                                 "NavigatorAuction", "runAdAuction");
  String maybe_json;
  bool maybe_json_ok = false;
  if (!value.IsEmpty()) {
    v8::Local<v8::Value> v8_value = value.V8Value();
    if (v8_value->IsUndefined() || v8_value->IsNull()) {
      // `maybe_json` left as the null string here; that's the blink equivalent
      // of absl::nullopt for a string? in mojo.
      maybe_json_ok = true;
    } else {
      maybe_json_ok = Jsonify(*script_state, value.V8Value(), maybe_json);
      if (!maybe_json_ok) {
        exception_state.ThrowTypeError(
            ErrorInvalidAuctionConfigSellerJson(seller_name_, field_name_));
      }
    }
  }

  if (maybe_json_ok) {
    auction_handle_->ResolvedPromiseParam(auction_id_->Clone(), field_,
                                          std::move(maybe_json));
  } else {
    auction_handle_->Abort();
  }

  return ScriptValue();
}

void NavigatorAuction::AuctionHandle::JsonResolved::Trace(
    Visitor* visitor) const {
  visitor->Trace(auction_handle_);
  Callable::Trace(visitor);
}

NavigatorAuction::AuctionHandle::PerBuyerSignalsResolved::
    PerBuyerSignalsResolved(
        AuctionHandle* auction_handle,
        mojom::blink::AuctionAdConfigAuctionIdPtr auction_id,
        const String& seller_name)
    : auction_handle_(auction_handle),
      auction_id_(std::move(auction_id)),
      seller_name_(seller_name) {}

ScriptValue NavigatorAuction::AuctionHandle::PerBuyerSignalsResolved::Call(
    ScriptState* script_state,
    ScriptValue value) {
  ExceptionState exception_state(script_state->GetIsolate(),
                                 ExceptionState::kExecutionContext,
                                 "NavigatorAuction", "runAdAuction");
  absl::optional<WTF::HashMap<scoped_refptr<const SecurityOrigin>, String>>
      per_buyer_signals;
  if (!value.IsEmpty()) {
    v8::Local<v8::Value> v8_value = value.V8Value();
    if (!v8_value->IsUndefined() && !v8_value->IsNull()) {
      per_buyer_signals = ConvertNonPromisePerBuyerSignalsFromV8ToMojo(
          *script_state, exception_state, seller_name_, v8_value);
    }
  }

  if (!exception_state.HadException()) {
    auction_handle_->ResolvedPerBuyerSignalsPromise(
        auction_id_->Clone(), std::move(per_buyer_signals));
  } else {
    auction_handle_->Abort();
  }

  return ScriptValue();
}

void NavigatorAuction::AuctionHandle::PerBuyerSignalsResolved::Trace(
    Visitor* visitor) const {
  visitor->Trace(auction_handle_);
  Callable::Trace(visitor);
}

NavigatorAuction::AuctionHandle::BuyerTimeoutsResolved::BuyerTimeoutsResolved(
    AuctionHandle* auction_handle,
    mojom::blink::AuctionAdConfigAuctionIdPtr auction_id,
    mojom::blink::AuctionAdConfigBuyerTimeoutField field,
    const String& seller_name)
    : auction_handle_(auction_handle),
      auction_id_(std::move(auction_id)),
      field_(field),
      seller_name_(seller_name) {}

ScriptValue NavigatorAuction::AuctionHandle::BuyerTimeoutsResolved::Call(
    ScriptState* script_state,
    ScriptValue value) {
  ExceptionState exception_state(script_state->GetIsolate(),
                                 ExceptionState::kExecutionContext,
                                 "NavigatorAuction", "runAdAuction");
  mojom::blink::AuctionAdConfigBuyerTimeoutsPtr buyer_timeouts;
  if (!value.IsEmpty()) {
    v8::Local<v8::Value> v8_value = value.V8Value();
    if (!v8_value->IsUndefined() && !v8_value->IsNull()) {
      buyer_timeouts = ConvertNonPromisePerBuyerTimeoutsFromV8ToMojo(
          *script_state, exception_state, seller_name_, v8_value, field_);
    }
  }

  if (!buyer_timeouts) {
    buyer_timeouts = mojom::blink::AuctionAdConfigBuyerTimeouts::New();
  }

  if (!exception_state.HadException()) {
    auction_handle_->ResolvedBuyerTimeoutsPromise(auction_id_->Clone(), field_,
                                                  std::move(buyer_timeouts));
  } else {
    auction_handle_->Abort();
  }

  return ScriptValue();
}

void NavigatorAuction::AuctionHandle::BuyerTimeoutsResolved::Trace(
    Visitor* visitor) const {
  visitor->Trace(auction_handle_);
  Callable::Trace(visitor);
}

NavigatorAuction::AuctionHandle::BuyerCurrenciesResolved::
    BuyerCurrenciesResolved(
        AuctionHandle* auction_handle,
        mojom::blink::AuctionAdConfigAuctionIdPtr auction_id,
        const String& seller_name)
    : auction_handle_(auction_handle),
      auction_id_(std::move(auction_id)),
      seller_name_(seller_name) {}

ScriptValue NavigatorAuction::AuctionHandle::BuyerCurrenciesResolved::Call(
    ScriptState* script_state,
    ScriptValue value) {
  ExceptionState exception_state(script_state->GetIsolate(),
                                 ExceptionState::kExecutionContext,
                                 "NavigatorAuction", "runAdAuction");
  mojom::blink::AuctionAdConfigBuyerCurrenciesPtr buyer_currencies;
  if (!value.IsEmpty()) {
    v8::Local<v8::Value> v8_value = value.V8Value();
    if (!v8_value->IsUndefined() && !v8_value->IsNull()) {
      buyer_currencies = ConvertNonPromisePerBuyerCurrenciesFromV8ToMojo(
          *script_state, exception_state, seller_name_, v8_value);
    }
  }

  if (!buyer_currencies) {
    buyer_currencies = mojom::blink::AuctionAdConfigBuyerCurrencies::New();
  }

  if (!exception_state.HadException()) {
    auction_handle_->ResolvedBuyerCurrencies(auction_id_->Clone(),
                                             std::move(buyer_currencies));
  } else {
    auction_handle_->Abort();
  }

  return ScriptValue();
}

void NavigatorAuction::AuctionHandle::BuyerCurrenciesResolved::Trace(
    Visitor* visitor) const {
  visitor->Trace(auction_handle_);
  Callable::Trace(visitor);
}

NavigatorAuction::AuctionHandle::DirectFromSellerSignalsResolved::
    DirectFromSellerSignalsResolved(
        AuctionHandle* auction_handle,
        mojom::blink::AuctionAdConfigAuctionIdPtr auction_id,
        const String& seller_name,
        const scoped_refptr<const SecurityOrigin>& seller_origin,
        const absl::optional<Vector<scoped_refptr<const SecurityOrigin>>>&
            interest_group_buyers)
    : auction_handle_(auction_handle),
      auction_id_(std::move(auction_id)),
      seller_name_(seller_name),
      seller_origin_(seller_origin),
      interest_group_buyers_(interest_group_buyers) {}

ScriptValue
NavigatorAuction::AuctionHandle::DirectFromSellerSignalsResolved::Call(
    ScriptState* script_state,
    ScriptValue value) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context) {
    return ScriptValue();
  }

  ExceptionState exception_state(script_state->GetIsolate(),
                                 ExceptionState::kExecutionContext,
                                 "NavigatorAuction", "runAdAuction");
  mojom::blink::DirectFromSellerSignalsPtr direct_from_seller_signals;
  if (!value.IsEmpty()) {
    v8::Local<v8::Value> v8_value = value.V8Value();
    if (!v8_value->IsUndefined() && !v8_value->IsNull()) {
      direct_from_seller_signals = ConvertDirectFromSellerSignalsFromV8ToMojo(
          *script_state, *context, exception_state, *context->Fetcher(),
          seller_name_, *seller_origin_, interest_group_buyers_, v8_value);
    }
  }

  if (!exception_state.HadException()) {
    auction_handle_->ResolvedDirectFromSellerSignalsPromise(
        auction_id_->Clone(), std::move(direct_from_seller_signals));
  } else {
    auction_handle_->Abort();
  }

  return ScriptValue();
}

void NavigatorAuction::AuctionHandle::DirectFromSellerSignalsResolved::Trace(
    Visitor* visitor) const {
  visitor->Trace(auction_handle_);
  Callable::Trace(visitor);
}

NavigatorAuction::AuctionHandle::ResolveToConfigResolved::
    ResolveToConfigResolved(AuctionHandle* auction_handle)
    : auction_handle_(auction_handle) {}
ScriptValue NavigatorAuction::AuctionHandle::ResolveToConfigResolved::Call(
    ScriptState* script_state,
    ScriptValue value) {
  v8::Local<v8::Value> v8_value = value.V8Value();

  ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context) {
    return ScriptValue();
  }

  if (!v8_value->IsBoolean()) {
    auction_handle_->SetResolveToConfig(false);
  } else {
    auction_handle_->SetResolveToConfig(
        v8_value->BooleanValue(script_state->GetIsolate()));
  }

  auction_handle_->MaybeResolveAuction();
  return ScriptValue();
}

void NavigatorAuction::AuctionHandle::ResolveToConfigResolved::Trace(
    Visitor* visitor) const {
  visitor->Trace(auction_handle_);
  Callable::Trace(visitor);
}

NavigatorAuction::AuctionHandle::Rejected::Rejected(
    AuctionHandle* auction_handle)
    : auction_handle_(auction_handle) {}

ScriptValue NavigatorAuction::AuctionHandle::Rejected::Call(ScriptState*,
                                                            ScriptValue) {
  // Abort the auction if any input promise rejects
  auction_handle_->Abort();
  return ScriptValue();
}

void NavigatorAuction::AuctionHandle::Rejected::Trace(Visitor* visitor) const {
  visitor->Trace(auction_handle_);
  Callable::Trace(visitor);
}

NavigatorAuction::NavigatorAuction(Navigator& navigator)
    : Supplement(navigator),
      queued_cross_site_joins_(kMaxActiveCrossSiteJoins,
                               WTF::BindRepeating(&NavigatorAuction::StartJoin,
                                                  WrapWeakPersistent(this))),
      queued_cross_site_leaves_(
          kMaxActiveCrossSiteLeaves,
          WTF::BindRepeating(&NavigatorAuction::StartLeave,
                             WrapWeakPersistent(this))),
      ad_auction_service_(navigator.GetExecutionContext()) {
  navigator.GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      ad_auction_service_.BindNewPipeAndPassReceiver(
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

ScriptPromise NavigatorAuction::joinAdInterestGroup(
    ScriptState* script_state,
    const AuctionAdInterestGroup* group,
    double duration_seconds,
    ExceptionState& exception_state) {
  const ExecutionContext* context = ExecutionContext::From(script_state);

  auto mojo_group = mojom::blink::InterestGroup::New();
  mojo_group->expiry = base::Time::Now() + base::Seconds(duration_seconds);
  if (!CopyOwnerFromIdlToMojo(*context, exception_state, *group, *mojo_group))
    return ScriptPromise();
  mojo_group->name = group->name();
  mojo_group->priority = (group->hasPriority()) ? group->priority() : 0.0;

  mojo_group->enable_bidding_signals_prioritization =
      group->hasEnableBiddingSignalsPrioritization()
          ? group->enableBiddingSignalsPrioritization()
          : false;
  if (group->hasPriorityVector()) {
    mojo_group->priority_vector =
        ConvertSparseVectorIdlToMojo(group->priorityVector());
  }
  if (group->hasPrioritySignalsOverrides()) {
    mojo_group->priority_signals_overrides =
        ConvertSparseVectorIdlToMojo(group->prioritySignalsOverrides());
  }

  if (!CopySellerCapabilitiesFromIdlToMojo(*context, exception_state, *group,
                                           *mojo_group)) {
    return ScriptPromise();
  }
  if (!CopyExecutionModeFromIdlToMojo(*context, exception_state, *group,
                                      *mojo_group)) {
    return ScriptPromise();
  }
  if (!CopyBiddingLogicUrlFromIdlToMojo(*context, exception_state, *group,
                                        *mojo_group)) {
    return ScriptPromise();
  }
  if (!CopyWasmHelperUrlFromIdlToMojo(*context, exception_state, *group,
                                      *mojo_group)) {
    return ScriptPromise();
  }
  if (!CopyUpdateUrlFromIdlToMojo(*context, exception_state, *group,
                                  *mojo_group)) {
    return ScriptPromise();
  }
  if (!CopyTrustedBiddingSignalsUrlFromIdlToMojo(*context, exception_state,
                                                 *group, *mojo_group)) {
    return ScriptPromise();
  }
  if (!CopyTrustedBiddingSignalsKeysFromIdlToMojo(*group, *mojo_group))
    return ScriptPromise();
  if (!CopyUserBiddingSignalsFromIdlToMojo(*script_state, exception_state,
                                           *group, *mojo_group)) {
    return ScriptPromise();
  }
  if (!CopyAdsFromIdlToMojo(*context, *script_state, exception_state, *group,
                            *mojo_group)) {
    return ScriptPromise();
  }
  if (!CopyAdComponentsFromIdlToMojo(*context, *script_state, exception_state,
                                     *group, *mojo_group)) {
    return ScriptPromise();
  }
  if (!CopyAdSizesFromIdlToMojo(*context, *script_state, exception_state,
                                *group, *mojo_group)) {
    return ScriptPromise();
  }
  if (!CopySizeGroupsFromIdlToMojo(*context, *script_state, exception_state,
                                   *group, *mojo_group)) {
    return ScriptPromise();
  }

  String error_field_name;
  String error_field_value;
  String error;
  if (!ValidateBlinkInterestGroup(*mojo_group, error_field_name,
                                  error_field_value, error)) {
    exception_state.ThrowTypeError(ErrorInvalidInterestGroup(
        *group, error_field_name, error_field_value, error));
    return ScriptPromise();
  }

  bool is_cross_origin =
      !context->GetSecurityOrigin()->IsSameOriginWith(mojo_group->owner.get());

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();
  mojom::blink::AdAuctionService::JoinInterestGroupCallback callback =
      resolver->WrapCallbackInScriptScope(
          WTF::BindOnce(&NavigatorAuction::JoinComplete,
                        WrapWeakPersistent(this), is_cross_origin));

  PendingJoin pending_join{std::move(mojo_group), std::move(callback)};
  if (is_cross_origin) {
    queued_cross_site_joins_.Enqueue(std::move(pending_join));
  } else {
    StartJoin(std::move(pending_join));
  }

  return promise;
}

/* static */
ScriptPromise NavigatorAuction::joinAdInterestGroup(
    ScriptState* script_state,
    Navigator& navigator,
    const AuctionAdInterestGroup* group,
    double duration_seconds,
    ExceptionState& exception_state) {
  if (!navigator.DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document has no window associated.");
    return ScriptPromise();
  }
  RecordCommonFledgeUseCounters(navigator.DomWindow()->document());
  const ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kJoinAdInterestGroup)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Feature join-ad-interest-group is not enabled by Permissions Policy");
    return ScriptPromise();
  }
  if (!base::FeatureList::IsEnabled(
          blink::features::kAdInterestGroupAPIRestrictedPolicyByDefault) &&
      FeatureWouldBeBlockedByRestrictedPermissionsPolicy(navigator)) {
    AddWarningMessageToConsole(script_state, "join-ad-interest-group",
                               "joinAdInterestGroup");
  }

  return From(ExecutionContext::From(script_state), navigator)
      .joinAdInterestGroup(script_state, group, duration_seconds,
                           exception_state);
}

ScriptPromise NavigatorAuction::leaveAdInterestGroup(
    ScriptState* script_state,
    const AuctionAdInterestGroupKey* group_key,
    ExceptionState& exception_state) {
  scoped_refptr<const SecurityOrigin> owner = ParseOrigin(group_key->owner());
  if (!owner) {
    exception_state.ThrowTypeError("owner '" + group_key->owner() +
                                   "' for AuctionAdInterestGroup with name '" +
                                   group_key->name() +
                                   "' must be a valid https origin.");
    return ScriptPromise();
  }

  bool is_cross_origin = !ExecutionContext::From(script_state)
                              ->GetSecurityOrigin()
                              ->IsSameOriginWith(owner.get());

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();
  mojom::blink::AdAuctionService::LeaveInterestGroupCallback callback =
      resolver->WrapCallbackInScriptScope(
          WTF::BindOnce(&NavigatorAuction::LeaveComplete,
                        WrapWeakPersistent(this), is_cross_origin));

  PendingLeave pending_leave{std::move(owner), std::move(group_key->name()),
                             std::move(callback)};
  if (is_cross_origin) {
    queued_cross_site_leaves_.Enqueue(std::move(pending_leave));
  } else {
    StartLeave(std::move(pending_leave));
  }

  return promise;
}

ScriptPromise NavigatorAuction::leaveAdInterestGroupForDocument(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  LocalDOMWindow* window = GetSupplementable()->DomWindow();

  if (!window) {
    exception_state.ThrowSecurityError(
        "May not leaveAdInterestGroup from a Document that is not fully "
        "active");
    return ScriptPromise();
  }
  if (!window->GetFrame()->IsInFencedFrameTree()) {
    exception_state.ThrowTypeError(
        "owner and name are required outside of a fenced frame.");
    return ScriptPromise();
  }
  // The renderer does not have enough information to verify that this document
  // is the result of a FLEDGE auction. The browser will silently ignore
  // this request if this document is not the result of a FLEDGE auction.
  ad_auction_service_->LeaveInterestGroupForDocument();

  // Return resolved promise. The browser-side code doesn't do anything
  // meaningful in this case (no .well-known fetches), and if it ever does do
  // them, likely don't want to expose timing information to the fenced frame,
  // anyways.
  return ScriptPromise::CastUndefined(script_state);
}

/* static */
ScriptPromise NavigatorAuction::leaveAdInterestGroup(
    ScriptState* script_state,
    Navigator& navigator,
    const AuctionAdInterestGroupKey* group_key,
    ExceptionState& exception_state) {
  if (!navigator.DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document has no window associated.");
    return ScriptPromise();
  }
  RecordCommonFledgeUseCounters(navigator.DomWindow()->document());
  ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kJoinAdInterestGroup)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Feature join-ad-interest-group is not enabled by Permissions Policy");
    return ScriptPromise();
  }
  if (!base::FeatureList::IsEnabled(
          blink::features::kAdInterestGroupAPIRestrictedPolicyByDefault) &&
      FeatureWouldBeBlockedByRestrictedPermissionsPolicy(navigator)) {
    AddWarningMessageToConsole(script_state, "join-ad-interest-group",
                               "leaveAdInterestGroup");
  }

  return From(context, navigator)
      .leaveAdInterestGroup(script_state, group_key, exception_state);
}

/* static */
ScriptPromise NavigatorAuction::leaveAdInterestGroup(
    ScriptState* script_state,
    Navigator& navigator,
    ExceptionState& exception_state) {
  if (!navigator.DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document has no window associated.");
    return ScriptPromise();
  }
  ExecutionContext* context = ExecutionContext::From(script_state);
  // According to the spec, implicit leave bypasses permission policy.
  return From(context, navigator)
      .leaveAdInterestGroupForDocument(script_state, exception_state);
}

void NavigatorAuction::updateAdInterestGroups() {
  ad_auction_service_->UpdateAdInterestGroups();
}

/* static */
void NavigatorAuction::updateAdInterestGroups(ScriptState* script_state,
                                              Navigator& navigator,
                                              ExceptionState& exception_state) {
  if (!navigator.DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document has no window associated.");
    return;
  }
  RecordCommonFledgeUseCounters(navigator.DomWindow()->document());
  ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kJoinAdInterestGroup)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Feature join-ad-interest-group is not enabled by Permissions Policy");
    return;
  }
  if (!base::FeatureList::IsEnabled(
          blink::features::kAdInterestGroupAPIRestrictedPolicyByDefault) &&
      FeatureWouldBeBlockedByRestrictedPermissionsPolicy(navigator)) {
    AddWarningMessageToConsole(script_state, "join-ad-interest-group",
                               "updateAdInterestGroups");
  }

  return From(context, navigator).updateAdInterestGroups();
}

ScriptPromise NavigatorAuction::runAdAuction(ScriptState* script_state,
                                             const AuctionAdConfig* config,
                                             ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);

  mojo::PendingReceiver<mojom::blink::AbortableAdAuction> abort_receiver;
  auto* auction_handle = MakeGarbageCollected<AuctionHandle>(
      context, abort_receiver.InitWithNewPipeAndPassRemote());
  auto mojo_config = IdlAuctionConfigToMojo(
      auction_handle,
      /*is_top_level=*/true, /*nested_pos=*/0, *script_state, *context,
      exception_state,
      /*resource_fetcher=*/
      *GetSupplementable()->DomWindow()->document()->Fetcher(), *config);
  if (!mojo_config)
    return ScriptPromise();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();
  std::unique_ptr<ScopedAbortState> scoped_abort_state = nullptr;
  if (auto* signal = config->getSignalOr(nullptr)) {
    if (signal->aborted()) {
      resolver->Reject(signal->reason(script_state));
      return promise;
    }
    auto* abort_handle = signal->AddAlgorithm(auction_handle);
    scoped_abort_state =
        std::make_unique<ScopedAbortState>(signal, abort_handle);
  }

  if (config->hasResolveToConfig() &&
      config->resolveToConfig().V8Value()->IsPromise()) {
    ScriptPromise resolve_to_config_promise(
        script_state, config->resolveToConfig().V8Value());
    auction_handle->AttachPromiseHandler(
        *script_state, resolve_to_config_promise,
        MakeGarbageCollected<
            NavigatorAuction::AuctionHandle::ResolveToConfigResolved>(
            auction_handle));
  } else {
    bool resolve_val = false;

    if (config->hasResolveToConfig() &&
        config->resolveToConfig().V8Value()->IsBoolean()) {
      resolve_val = config->resolveToConfig().V8Value()->BooleanValue(
          script_state->GetIsolate());
    }

    auction_handle->SetResolveToConfig(resolve_val);
  }

  ad_auction_service_->RunAdAuction(
      std::move(mojo_config), std::move(abort_receiver),
      WTF::BindOnce(&NavigatorAuction::AuctionHandle::AuctionComplete,
                    WrapPersistent(auction_handle), WrapPersistent(resolver),
                    std::move(scoped_abort_state)));
  return promise;
}

/* static */
ScriptPromise NavigatorAuction::runAdAuction(ScriptState* script_state,
                                             Navigator& navigator,
                                             const AuctionAdConfig* config,
                                             ExceptionState& exception_state) {
  if (!navigator.DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document has no window associated.");
    return ScriptPromise();
  }
  RecordCommonFledgeUseCounters(navigator.DomWindow()->document());
  const ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kRunAdAuction)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Feature run-ad-auction is not enabled by Permissions Policy");
    return ScriptPromise();
  }
  if (!base::FeatureList::IsEnabled(
          blink::features::kAdInterestGroupAPIRestrictedPolicyByDefault) &&
      FeatureWouldBeBlockedByRestrictedPermissionsPolicy(navigator)) {
    AddWarningMessageToConsole(script_state, "run-ad-auction", "runAdAuction");
  }

  return From(ExecutionContext::From(script_state), navigator)
      .runAdAuction(script_state, config, exception_state);
}

/* static */
Vector<String> NavigatorAuction::adAuctionComponents(
    ScriptState* script_state,
    Navigator& navigator,
    uint16_t num_ad_components,
    ExceptionState& exception_state) {
  Vector<String> out;
  if (!navigator.DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document has no window associated.");
    return out;
  }
  RecordCommonFledgeUseCounters(navigator.DomWindow()->document());
  const auto& ad_auction_components =
      navigator.DomWindow()->document()->Loader()->AdAuctionComponents();
  if (!ad_auction_components) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "This frame was not loaded with the "
                                      "result of an interest group auction.");
    return out;
  }

  // Clamp the number of ad components at blink::kMaxAdAuctionAdComponents.
  if (num_ad_components >
      static_cast<int16_t>(blink::kMaxAdAuctionAdComponents)) {
    num_ad_components = blink::kMaxAdAuctionAdComponents;
  }

  DCHECK_EQ(kMaxAdAuctionAdComponents, ad_auction_components->size());

  for (int i = 0; i < num_ad_components; ++i) {
    out.push_back((*ad_auction_components)[i].GetString());
  }
  return out;
}

ScriptPromise NavigatorAuction::deprecatedURNToURL(
    ScriptState* script_state,
    const String& uuid_url_string,
    bool send_reports,
    ExceptionState& exception_state) {
  KURL uuid_url(uuid_url_string);
  if (!blink::IsValidUrnUuidURL(GURL(uuid_url))) {
    exception_state.ThrowTypeError("Passed URL must be a valid URN URL.");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();
  ad_auction_service_->DeprecatedGetURLFromURN(
      std::move(uuid_url), send_reports,
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          &NavigatorAuction::GetURLFromURNComplete, WrapPersistent(this))));
  return promise;
}

ScriptPromise NavigatorAuction::deprecatedURNToURL(
    ScriptState* script_state,
    Navigator& navigator,
    const V8UnionFencedFrameConfigOrUSVString* urn_or_config,
    bool send_reports,
    ExceptionState& exception_state) {
  if (!navigator.DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document has no window associated.");
    return ScriptPromise();
  }
  String uuid_url_string;
  switch (urn_or_config->GetContentType()) {
    case V8UnionFencedFrameConfigOrUSVString::ContentType::kUSVString:
      uuid_url_string = urn_or_config->GetAsUSVString();
      break;
    case V8UnionFencedFrameConfigOrUSVString::ContentType::kFencedFrameConfig:
      absl::optional<KURL> uuid_url_opt =
          urn_or_config->GetAsFencedFrameConfig()->urn_uuid(
              base::PassKey<NavigatorAuction>());
      if (!uuid_url_opt.has_value()) {
        exception_state.ThrowTypeError("Passed config must have a mapped URL.");
        return ScriptPromise();
      }
      uuid_url_string = uuid_url_opt->GetString();
      break;
  }
  return From(ExecutionContext::From(script_state), navigator)
      .deprecatedURNToURL(script_state, uuid_url_string, send_reports,
                          exception_state);
}

ScriptPromise NavigatorAuction::deprecatedReplaceInURN(
    ScriptState* script_state,
    const String& uuid_url_string,
    const Vector<std::pair<String, String>>& replacements,
    ExceptionState& exception_state) {
  KURL uuid_url(uuid_url_string);
  if (!blink::IsValidUrnUuidURL(GURL(uuid_url))) {
    exception_state.ThrowTypeError("Passed URL must be a valid URN URL.");
    return ScriptPromise();
  }
  Vector<mojom::blink::AdKeywordReplacementPtr> replacements_list;
  for (const auto& replacement : replacements) {
    if (!(replacement.first.StartsWith("${") &&
          replacement.first.EndsWith("}")) &&
        !(replacement.first.StartsWith("%%") &&
          replacement.first.EndsWith("%%"))) {
      exception_state.ThrowTypeError(
          "Replacements must be of the form '${...}' or '%%...%%'");
      return ScriptPromise();
    }
    replacements_list.push_back(mojom::blink::AdKeywordReplacement::New(
        replacement.first, replacement.second));
  }
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();
  ad_auction_service_->DeprecatedReplaceInURN(
      std::move(uuid_url), std::move(replacements_list),
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          &NavigatorAuction::ReplaceInURNComplete, WrapPersistent(this))));
  return promise;
}

ScriptPromise NavigatorAuction::deprecatedReplaceInURN(
    ScriptState* script_state,
    Navigator& navigator,
    const V8UnionFencedFrameConfigOrUSVString* urn_or_config,
    const Vector<std::pair<String, String>>& replacements,
    ExceptionState& exception_state) {
  if (!navigator.DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document has no window associated.");
    return ScriptPromise();
  }
  String uuid_url_string;
  switch (urn_or_config->GetContentType()) {
    case V8UnionFencedFrameConfigOrUSVString::ContentType::kUSVString:
      uuid_url_string = urn_or_config->GetAsUSVString();
      break;
    case V8UnionFencedFrameConfigOrUSVString::ContentType::kFencedFrameConfig:
      absl::optional<KURL> uuid_url_opt =
          urn_or_config->GetAsFencedFrameConfig()->urn_uuid(
              base::PassKey<NavigatorAuction>());
      if (!uuid_url_opt.has_value()) {
        exception_state.ThrowTypeError("Passed config must have a mapped URL.");
        return ScriptPromise();
      }
      uuid_url_string = uuid_url_opt->GetString();
      break;
  }
  return From(ExecutionContext::From(script_state), navigator)
      .deprecatedReplaceInURN(script_state, uuid_url_string,
                              std::move(replacements), exception_state);
}

ScriptPromise NavigatorAuction::createAdRequest(
    ScriptState* script_state,
    const AdRequestConfig* config,
    ExceptionState& exception_state) {
  const ExecutionContext* context = ExecutionContext::From(script_state);
  auto mojo_config = mojom::blink::AdRequestConfig::New();

  if (!CopyAdRequestUrlFromIdlToMojo(*context, exception_state, *config,
                                     *mojo_config))
    return ScriptPromise();

  if (!CopyAdPropertiesFromIdlToMojo(*context, exception_state, *config,
                                     *mojo_config))
    return ScriptPromise();

  if (config->hasPublisherCode()) {
    mojo_config->publisher_code = config->publisherCode();
  }

  if (!CopyTargetingFromIdlToMojo(*context, exception_state, *config,
                                  *mojo_config))
    return ScriptPromise();

  if (!CopyAdSignalsFromIdlToMojo(*context, exception_state, *config,
                                  *mojo_config))
    return ScriptPromise();

  if (!CopyFallbackSourceFromIdlToMojo(*context, exception_state, *config,
                                       *mojo_config))
    return ScriptPromise();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();
  ad_auction_service_->CreateAdRequest(
      std::move(mojo_config),
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          &NavigatorAuction::AdsRequested, WrapPersistent(this))));
  return promise;
}

/* static */
ScriptPromise NavigatorAuction::createAdRequest(
    ScriptState* script_state,
    Navigator& navigator,
    const AdRequestConfig* config,
    ExceptionState& exception_state) {
  if (!navigator.DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document has no window associated.");
    return ScriptPromise();
  }
  return From(ExecutionContext::From(script_state), navigator)
      .createAdRequest(script_state, config, exception_state);
}

void NavigatorAuction::AdsRequested(ScriptPromiseResolver* resolver,
                                    const WTF::String&) {
  // TODO(https://crbug.com/1249186): Add full impl of methods.
  resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
      resolver->GetScriptState()->GetIsolate(),
      DOMExceptionCode::kNotSupportedError,
      "createAdRequest API not yet implemented"));
}

ScriptPromise NavigatorAuction::finalizeAd(ScriptState* script_state,
                                           const Ads* ads,
                                           const AuctionAdConfig* config,
                                           ExceptionState& exception_state) {
  const ExecutionContext* context = ExecutionContext::From(script_state);
  auto mojo_config = mojom::blink::AuctionAdConfig::New();

  // For finalizing an Ad PARAKEET only really cares about the decisionLogicUrl,
  // auctionSignals, sellerSignals, and perBuyerSignals. Also need seller, since
  // it's used to validate the decision logic URL. We can ignore
  // copying/validating other fields on AuctionAdConfig.
  if (!CopySellerFromIdlToMojo(exception_state, *config, *mojo_config) ||
      !CopyDecisionLogicUrlFromIdlToMojo(*context, exception_state, *config,
                                         *mojo_config) ||
      !CopyAuctionSignalsFromIdlToMojo(/*auction_handle=*/nullptr,
                                       /*auction_id=*/nullptr, *script_state,
                                       exception_state, *config,
                                       *mojo_config) ||
      !CopySellerSignalsFromIdlToMojo(/*auction_handle=*/nullptr,
                                      /*auction_id=*/nullptr, *script_state,
                                      exception_state, *config, *mojo_config) ||
      !CopyPerBuyerSignalsFromIdlToMojo(/*auction_handle=*/nullptr,
                                        /*auction_id=*/nullptr, *script_state,
                                        exception_state, *config,
                                        *mojo_config)) {
    return ScriptPromise();
  }

  if (!ValidateAdsObject(exception_state, ads))
    return ScriptPromise();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();
  ad_auction_service_->FinalizeAd(
      ads->GetGuid(), std::move(mojo_config),
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          &NavigatorAuction::FinalizeAdComplete, WrapPersistent(this))));
  return promise;
}

/* static */
ScriptPromise NavigatorAuction::finalizeAd(ScriptState* script_state,
                                           Navigator& navigator,
                                           const Ads* ads,
                                           const AuctionAdConfig* config,
                                           ExceptionState& exception_state) {
  if (!navigator.DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document has no window associated.");
    return ScriptPromise();
  }
  return From(ExecutionContext::From(script_state), navigator)
      .finalizeAd(script_state, ads, config, exception_state);
}

void NavigatorAuction::FinalizeAdComplete(
    ScriptPromiseResolver* resolver,
    const absl::optional<KURL>& creative_url) {
  if (creative_url) {
    resolver->Resolve(creative_url);
  } else {
    // TODO(https://crbug.com/1249186): Add full impl of methods.
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        resolver->GetScriptState()->GetIsolate(),
        DOMExceptionCode::kNotSupportedError,
        "finalizeAd API not yet implemented"));
  }
}

void NavigatorAuction::StartJoin(PendingJoin&& pending_join) {
  ad_auction_service_->JoinInterestGroup(std::move(pending_join.interest_group),
                                         std::move(pending_join.callback));
}

void NavigatorAuction::JoinComplete(bool is_cross_origin,
                                    ScriptPromiseResolver* resolver,
                                    bool failed_well_known_check) {
  if (is_cross_origin)
    queued_cross_site_joins_.OnComplete();

  if (failed_well_known_check) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        resolver->GetScriptState()->GetIsolate(),
        DOMExceptionCode::kNotAllowedError,
        "Permission to join interest group denied."));
    return;
  }
  resolver->Resolve();
}

void NavigatorAuction::StartLeave(PendingLeave&& pending_leave) {
  ad_auction_service_->LeaveInterestGroup(pending_leave.owner,
                                          pending_leave.name,
                                          std::move(pending_leave.callback));
}

void NavigatorAuction::LeaveComplete(bool is_cross_origin,
                                     ScriptPromiseResolver* resolver,
                                     bool failed_well_known_check) {
  if (is_cross_origin)
    queued_cross_site_leaves_.OnComplete();

  if (failed_well_known_check) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        resolver->GetScriptState()->GetIsolate(),
        DOMExceptionCode::kNotAllowedError,
        "Permission to leave interest group denied."));
    return;
  }
  resolver->Resolve();
}

void NavigatorAuction::AuctionHandle::AuctionComplete(
    ScriptPromiseResolver* resolver,
    std::unique_ptr<ScopedAbortState> scoped_abort_state,
    bool manually_aborted,
    const absl::optional<FencedFrame::RedactedFencedFrameConfig>&
        result_config) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;
  AbortSignal* abort_signal =
      scoped_abort_state ? scoped_abort_state->Signal() : nullptr;
  ScriptState* script_state = resolver->GetScriptState();
  ScriptState::Scope script_state_scope(script_state);
  if (manually_aborted) {
    if (abort_signal && abort_signal->aborted()) {
      resolver->Reject(abort_signal->reason(script_state));
    } else {
      // TODO(morlovich): It would probably be better to wire something more
      // precise.
      resolver->Reject(
          "Promise argument rejected or resolved to invalid value.");
    }
  } else if (result_config) {
    DCHECK(result_config->mapped_url().has_value());
    DCHECK(!result_config->mapped_url()->potentially_opaque_value.has_value());

    auction_resolver_ = resolver;
    auction_config_ = result_config;

    MaybeResolveAuction();
  } else {
    resolver->Resolve(v8::Null(script_state->GetIsolate()));
  }
}

void NavigatorAuction::AuctionHandle::MaybeResolveAuction() {
  if (!resolve_to_config_.has_value() || !auction_resolver_ ||
      !auction_config_.has_value()) {
    // Once both the resolveToConfig promise is resolved and the auction is
    // completed, this function will be called again to actually
    // complete the auction.
    return;
  }

  if (resolve_to_config_.value() == true) {
    auction_resolver_->Resolve(
        FencedFrameConfig::From(auction_config_.value()));
  } else {
    auction_resolver_->Resolve(KURL(auction_config_->urn_uuid().value()));
  }
}

void NavigatorAuction::GetURLFromURNComplete(
    ScriptPromiseResolver* resolver,
    const absl::optional<KURL>& decoded_url) {
  if (decoded_url) {
    resolver->Resolve(*decoded_url);
  } else {
    resolver->Resolve(v8::Null(resolver->GetScriptState()->GetIsolate()));
  }
}

void NavigatorAuction::ReplaceInURNComplete(ScriptPromiseResolver* resolver) {
  resolver->Resolve();
}

bool NavigatorAuction::canLoadAdAuctionFencedFrame(ScriptState* script_state) {
  if (!script_state->ContextIsValid()) {
    return false;
  }

  LocalFrame* frame_to_check = LocalDOMWindow::From(script_state)->GetFrame();
  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(frame_to_check && context);

  ContentSecurityPolicy* csp = context->GetContentSecurityPolicy();
  DCHECK(csp);

  // "A fenced frame tree of one mode cannot contain a child fenced frame of
  // another mode."
  // See: https://github.com/WICG/fenced-frame/blob/master/explainer/modes.md
  if (frame_to_check->GetPage()->IsMainFrameFencedFrameRoot() &&
      frame_to_check->GetPage()->DeprecatedFencedFrameMode() !=
          blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds) {
    return false;
  }

  if (!context->IsSecureContext()) {
    return false;
  }

  // Check that the flags specified in kFencedFrameMandatoryUnsandboxedFlags
  // are not set in this context. Fenced frames loaded in a sandboxed document
  // require these flags to remain unsandboxed.
  if (context->IsSandboxed(kFencedFrameMandatoryUnsandboxedFlags)) {
    return false;
  }

  // Check the results of the browser checks for the current frame.
  // If the embedding frame is an iframe with CSPEE set, or any ancestor
  // iframes has CSPEE set, the fenced frame will not be allowed to load.
  // The renderer has no knowledge of CSPEE up the ancestor chain, so we defer
  // to the browser to determine the existence of CSPEE outside of the scope
  // we can see here.
  if (frame_to_check->AncestorOrSelfHasCSPEE()) {
    return false;
  }

  // Ensure that if any CSP headers are set that will affect a fenced frame,
  // they allow all https urls to load. Opaque-ads fenced frames do not support
  // allowing/disallowing specific hosts, as that could reveal information to
  // a fenced frame about its embedding page. See design doc for more info:
  // https://github.com/WICG/fenced-frame/blob/master/explainer/interaction_with_content_security_policy.md
  // This is being checked in the renderer because processing of <meta> tags
  // (including CSP) happen in the renderer after navigation commit, so we can't
  // piggy-back off of the ancestor_or_self_has_cspee bit being sent from the
  // browser (which is sent at commit time) since it doesn't know about all the
  // CSP headers yet.
  for (const auto& policy : csp->GetParsedPolicies()) {
    CSPOperativeDirective directive = CSPDirectiveListOperativeDirective(
        *policy, network::mojom::CSPDirectiveName::FencedFrameSrc);
    if (directive.type != network::mojom::CSPDirectiveName::Unknown) {
      // "*" urls will cause the allow_star flag to set
      if (directive.source_list->allow_star) {
        continue;
      }

      // Check for "https:" or "https://*:*"
      bool found_matching_source = false;
      for (const auto& source : directive.source_list->sources) {
        if (source->scheme == url::kHttpsScheme && source->host == "") {
          found_matching_source = true;
          break;
        }
      }
      if (!found_matching_source) {
        return false;
      }
    }
  }

  return true;
}

/* static */
bool NavigatorAuction::canLoadAdAuctionFencedFrame(ScriptState* script_state,
                                                   Navigator& navigator) {
  if (!navigator.DomWindow()) {
    return false;
  }
  return From(ExecutionContext::From(script_state), navigator)
      .canLoadAdAuctionFencedFrame(script_state);
}

}  // namespace blink
