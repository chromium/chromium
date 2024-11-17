// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/private_aggregation_bindings.h"

#include <stdint.h>

#include <cmath>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/auction_v8_logger.h"
#include "content/services/auction_worklet/public/cpp/private_aggregation_reporting.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "content/services/auction_worklet/webidl_compat.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"

namespace auction_worklet {

namespace {
// https://patcg-individual-drafts.github.io/private-aggregation-api/#dictdef-pasignalvalue
//
// dictionary PASignalValue {
//   required DOMString baseValue;
//   double scale;
//   (bigint or long) offset;
// };
struct PASignalValue {
  std::string base_value;
  std::optional<double> scale;
  std::optional<absl::variant<int32_t, v8::Local<v8::BigInt>>> offset;
};

bool ConvertToPASignalValue(AuctionV8Helper* v8_helper,
                            AuctionV8Helper::TimeLimitScope& time_limit_scope,
                            std::string error_prefix,
                            v8::Local<v8::Value> value,
                            DictConverter& report_errors_to,
                            PASignalValue& out) {
  DictConverter converter(v8_helper, time_limit_scope, std::move(error_prefix),
                          value);
  // Note: alphabetical to match WebIDL.
  converter.GetRequired("baseValue", out.base_value);
  converter.GetOptional("offset", out.offset);
  converter.GetOptional("scale", out.scale);

  report_errors_to.SetStatus(converter.TakeStatus());
  return report_errors_to.is_success();
}

// If T is `int32_t`, tries to converts `value` to
//    WebIDL type (PASignalValue or long)
// If T is v8::Local<v8::BigInt>>, tries to converts `value` to
//    WebIDL type (PASignalValue or bigint)
template <typename T>
bool ConvertToPASignalValueOr(AuctionV8Helper* v8_helper,
                              AuctionV8Helper::TimeLimitScope& time_limit_scope,
                              std::string error_prefix,
                              std::string_view field_name,
                              v8::Local<v8::Value> value,
                              DictConverter& report_errors_to,
                              absl::variant<PASignalValue, T>& out) {
  if (value->IsObject() || value->IsNullOrUndefined()) {
    out.template emplace<PASignalValue>();
    return ConvertToPASignalValue(
        v8_helper, time_limit_scope, std::move(error_prefix), value,
        report_errors_to, absl::get<PASignalValue>(out));
  } else {
    out.template emplace<T>();
    IdlConvert::Status status = IdlConvert::Convert(
        v8_helper->isolate(), error_prefix, {"field '", field_name, "'"}, value,
        absl::get<T>(out));
    report_errors_to.SetStatus(std::move(status));
    return report_errors_to.is_success();
  }
}

constexpr auto kBaseValueNames =
    base::MakeFixedFlatMap<std::string_view, mojom::BaseValue>({
        {"winning-bid", mojom::BaseValue::kWinningBid},
        {"highest-scoring-other-bid",
         mojom::BaseValue::kHighestScoringOtherBid},
        {"script-run-time", mojom::BaseValue::kScriptRunTime},
        {"signals-fetch-time", mojom::BaseValue::kSignalsFetchTime},
        {"bid-reject-reason", mojom::BaseValue::kBidRejectReason},
        {"participating-ig-count",
         mojom::BaseValue::kParticipatingInterestGroupCount},
        {"average-code-fetch-time", mojom::BaseValue::kAverageCodeFetchTime},
        {"percent-scripts-timeout", mojom::BaseValue::kPercentScriptsTimeout},
        {"percent-igs-cumulative-timeout",
         mojom::BaseValue::kPercentInterestGroupsCumulativeTimeout},
        {"cumulative-buyer-time", mojom::BaseValue::kCumulativeBuyerTime},
        {"regular-igs-count", mojom::BaseValue::kRegularInterestGroupsUsed},
        {"percent-regular-ig-count-quota-used",
         mojom::BaseValue::kPercentRegularInterestGroupQuotaUsed},
        {"negative-igs-count", mojom::BaseValue::kNegativeInterestGroupsUsed},
        {"percent-negative-ig-count-quota-used",
         mojom::BaseValue::kPercentNegativeInterestGroupQuotaUsed},
        {"ig-storage-used", mojom::BaseValue::kInterestGroupStorageUsed},
        {"percent-ig-storage-quota-used",
         mojom::BaseValue::kPercentInterestGroupStorageQuotaUsed},
    });

// Converts base value string to corresponding mojom enum.
std::optional<auction_worklet::mojom::BaseValue> BaseValueStringToEnum(
    const std::string& base_value,
    bool additional_extensions_allowed) {
  auto it = kBaseValueNames.find(base_value);
  if (it == kBaseValueNames.end()) {
    return std::nullopt;
  }
  auction_worklet::mojom::BaseValue value_enum = it->second;
  if (!additional_extensions_allowed &&
      RequiresAdditionalExtensions(value_enum)) {
    return std::nullopt;
  }

  return value_enum;
}

// If returns `std::nullopt`, will output an error to `error`.
std::optional<absl::uint128> ConvertBigIntToUint128(
    v8::Local<v8::BigInt> bigint,
    std::string* error) {
  if (bigint->WordCount() > 2) {
    *error = "BigInt is too large";
    return std::nullopt;
  }
  // Signals the size of the `words` array to `ToWordsArray()`. The number of
  // elements actually used is then written here by the function.
  int word_count = 2;
  int sign_bit = 0;
  uint64_t words[2] = {0, 0};  // Least significant to most significant.
  bigint->ToWordsArray(&sign_bit, &word_count, words);
  if (sign_bit) {
    *error = "BigInt must be non-negative";
    return std::nullopt;
  }

  return absl::MakeUint128(words[1], words[0]);
}

// If returns `std::nullopt`, will output an error to `error`.
// Modified from `ConvertBigIntToUint128()`.
std::optional<auction_worklet::mojom::BucketOffsetPtr>
ConvertBigIntToBucketOffset(v8::Local<v8::BigInt> bigint, std::string* error) {
  if (bigint->WordCount() > 2) {
    *error = "Bucket BigInt is too large";
    return std::nullopt;
  }
  // Signals the size of the `words` array to `ToWordsArray()`. The number of
  // elements actually used is then written here by the function.
  int word_count = 2;
  int sign_bit = 0;

  uint64_t words[2] = {0, 0};  // Least significant to most significant.
  bigint->ToWordsArray(&sign_bit, &word_count, words);

  return auction_worklet::mojom::BucketOffset::New(
      absl::MakeUint128(words[1], words[0]),
      /*is_negative=*/sign_bit);
}

std::optional<auction_worklet::mojom::SignalBucketPtr> GetSignalBucket(
    v8::Isolate* isolate,
    const PASignalValue& input,
    bool additional_extensions_allowed,
    std::string* error) {
  std::optional<auction_worklet::mojom::BaseValue> base_value_opt =
      BaseValueStringToEnum(input.base_value, additional_extensions_allowed);
  if (!base_value_opt.has_value()) {
    *error = "Bucket's 'baseValue' is invalid";
    return std::nullopt;
  }

  double scale = input.scale.value_or(1.0);

  if (!input.offset.has_value()) {
    return auction_worklet::mojom::SignalBucket::New(*base_value_opt, scale,
                                                     /*offset=*/nullptr);
  }

  // Offset must be BigInt for bucket.
  const v8::Local<v8::BigInt>* maybe_bigint =
      absl::get_if<v8::Local<v8::BigInt>>(&input.offset.value());
  if (!maybe_bigint) {
    *error = "Bucket's 'offset' must be BigInt";
    return std::nullopt;
  }

  std::optional<auction_worklet::mojom::BucketOffsetPtr> offset_opt =
      ConvertBigIntToBucketOffset(*maybe_bigint, error);
  if (!offset_opt.has_value()) {
    return nullptr;
  }
  return auction_worklet::mojom::SignalBucket::New(*base_value_opt, scale,
                                                   std::move(*offset_opt));
}

std::optional<auction_worklet::mojom::SignalValuePtr> GetSignalValue(
    v8::Isolate* isolate,
    const PASignalValue& input,
    bool additional_extensions_allowed,
    std::string* error) {
  std::optional<auction_worklet::mojom::BaseValue> base_value_opt =
      BaseValueStringToEnum(input.base_value, additional_extensions_allowed);
  if (!base_value_opt.has_value()) {
    *error = "Value's 'baseValue' is invalid";
    return std::nullopt;
  }

  double scale = input.scale.value_or(1.0);

  if (!input.offset.has_value()) {
    return auction_worklet::mojom::SignalValue::New(*base_value_opt, scale,
                                                    /*offset=*/0);
  }

  // Offset must be int32 for value.
  const int32_t* maybe_long = absl::get_if<int32_t>(&input.offset.value());
  if (!maybe_long) {
    *error = "Value's 'offset' must be a 32-bit signed integer";
    return std::nullopt;
  }
  return auction_worklet::mojom::SignalValue::New(*base_value_opt, scale,
                                                  *maybe_long);
}

// Returns contribution's bucket from `idl_bucket`. Returns nullptr if there is
// an error.
auction_worklet::mojom::ForEventSignalBucketPtr GetBucket(
    v8::Isolate* isolate,
    const absl::variant<PASignalValue, v8::Local<v8::BigInt>>& idl_bucket,
    bool additional_extensions_allowed,
    std::string* error) {
  const v8::Local<v8::BigInt>* big_int =
      absl::get_if<v8::Local<v8::BigInt>>(&idl_bucket);
  if (big_int) {
    std::optional<absl::uint128> maybe_bucket =
        ConvertBigIntToUint128(*big_int, error);
    if (!maybe_bucket.has_value()) {
      CHECK(base::IsStringUTF8(*error));
      return nullptr;
    }
    return auction_worklet::mojom::ForEventSignalBucket::NewIdBucket(
        *maybe_bucket);
  } else {
    std::optional<auction_worklet::mojom::SignalBucketPtr>
        maybe_signal_bucket_ptr =
            GetSignalBucket(isolate, absl::get<PASignalValue>(idl_bucket),
                            additional_extensions_allowed, error);
    if (!maybe_signal_bucket_ptr.has_value()) {
      CHECK(base::IsStringUTF8(*error));
      return nullptr;
    }
    return auction_worklet::mojom::ForEventSignalBucket::NewSignalBucket(
        std::move(*maybe_signal_bucket_ptr));
  }
}

// Returns contribution's value from `idl_value`. Returns nullptr if there is an
// error.
auction_worklet::mojom::ForEventSignalValuePtr GetValue(
    v8::Isolate* isolate,
    const absl::variant<PASignalValue, int32_t>& idl_value,
    bool additional_extensions_allowed,
    std::string* error) {
  const int32_t* int_value = absl::get_if<int32_t>(&idl_value);
  if (int_value) {
    if (*int_value < 0) {
      *error = "Value must be non-negative";
      return nullptr;
    }
    return auction_worklet::mojom::ForEventSignalValue::NewIntValue(*int_value);
  } else {
    std::optional<auction_worklet::mojom::SignalValuePtr>
        maybe_signal_value_ptr =
            GetSignalValue(isolate, absl::get<PASignalValue>(idl_value),
                           additional_extensions_allowed, error);
    if (!maybe_signal_value_ptr.has_value()) {
      CHECK(base::IsStringUTF8(*error));
      return nullptr;
    }
    return auction_worklet::mojom::ForEventSignalValue::NewSignalValue(
        std::move(*maybe_signal_value_ptr));
  }
}

// Returns false in case of an error.
bool GetFilteringId(v8::Isolate* isolate,
                    std::optional<v8::Local<v8::BigInt>> idl_filtering_id,
                    std::optional<uint64_t>* out_filtering_id,
                    std::string* error) {
  if (!idl_filtering_id.has_value()) {
    *out_filtering_id = std::nullopt;
    return true;
  }

  std::optional<absl::uint128> maybe_filtering_id =
      ConvertBigIntToUint128(idl_filtering_id.value(), error);
  if (!maybe_filtering_id.has_value()) {
    return false;
  }
  if (maybe_filtering_id.value() > 255) {
    *error = "Filtering ID is too large";
    return false;
  }

  *out_filtering_id = absl::Uint128Low64(maybe_filtering_id.value());
  return true;
}

auction_worklet::mojom::AggregatableReportForEventContributionPtr
ParseForEventContribution(
    v8::Isolate* isolate,
    auction_worklet::mojom::EventTypePtr event_type,
    absl::variant<PASignalValue, v8::Local<v8::BigInt>> idl_bucket,
    absl::variant<PASignalValue, int32_t> idl_value,
    std::optional<v8::Local<v8::BigInt>> idl_filtering_id,
    bool additional_extensions_allowed,
    std::string* error) {
  auction_worklet::mojom::ForEventSignalBucketPtr bucket = GetBucket(
      isolate, std::move(idl_bucket), additional_extensions_allowed, error);
  if (!bucket) {
    return nullptr;
  }
  auction_worklet::mojom::ForEventSignalValuePtr value = GetValue(
      isolate, std::move(idl_value), additional_extensions_allowed, error);
  if (!value) {
    return nullptr;
  }
  std::optional<uint64_t> filtering_id;
  if (!GetFilteringId(isolate, std::move(idl_filtering_id), &filtering_id,
                      error)) {
    return nullptr;
  }

  return auction_worklet::mojom::AggregatableReportForEventContribution::New(
      std::move(bucket), std::move(value), filtering_id, std::move(event_type));
}

// In case of failure, will return `std::nullopt` and output an error to
// `error`.
std::optional<uint64_t> ParseDebugKey(v8::Local<v8::BigInt> js_debug_key,
                                      std::string* error) {
  std::optional<absl::uint128> maybe_debug_key =
      ConvertBigIntToUint128(js_debug_key.As<v8::BigInt>(), error);
  if (!maybe_debug_key.has_value()) {
    return std::nullopt;
  }
  if (absl::Uint128High64(maybe_debug_key.value()) != 0) {
    *error = "BigInt is too large";
    return std::nullopt;
  }
  return absl::Uint128Low64(maybe_debug_key.value());
}

}  // namespace

PrivateAggregationBindings::PrivateAggregationBindings(
    AuctionV8Helper* v8_helper,
    AuctionV8Logger* v8_logger,
    bool private_aggregation_permissions_policy_allowed,
    bool reserved_once_allowed)
    : v8_helper_(v8_helper),
      v8_logger_(v8_logger),
      private_aggregation_permissions_policy_allowed_(
          private_aggregation_permissions_policy_allowed),
      enforce_permission_policy_for_on_event_(base::FeatureList::IsEnabled(
          blink::features::kFledgeEnforcePermissionPolicyContributeOnEvent)),
      additional_extensions_allowed_(base::FeatureList::IsEnabled(
          blink::features::
              kPrivateAggregationApiProtectedAudienceAdditionalExtensions)),
      reserved_once_allowed_(reserved_once_allowed) {}

PrivateAggregationBindings::~PrivateAggregationBindings() = default;

void PrivateAggregationBindings::AttachToContext(
    v8::Local<v8::Context> context) {
  if (!base::FeatureList::IsEnabled(blink::features::kPrivateAggregationApi) ||
      !blink::features::kPrivateAggregationApiEnabledInProtectedAudience
           .Get()) {
    return;
  }

  v8::Local<v8::External> v8_this =
      v8::External::New(v8_helper_->isolate(), this);

  v8::Local<v8::Object> private_aggregation =
      v8::Object::New(v8_helper_->isolate());

  v8::Local<v8::Function> send_histogram_report_function =
      v8::Function::New(
          context, &PrivateAggregationBindings::ContributeToHistogram, v8_this)
          .ToLocalChecked();
  private_aggregation
      ->Set(context,
            v8_helper_->CreateStringFromLiteral("contributeToHistogram"),
            send_histogram_report_function)
      .Check();

  if (blink::features::kPrivateAggregationApiProtectedAudienceExtensionsEnabled
          .Get()) {
    v8::Local<v8::Function> report_contribution_for_event_function =
        v8::Function::New(
            context, &PrivateAggregationBindings::ContributeToHistogramOnEvent,
            v8_this)
            .ToLocalChecked();
    private_aggregation
        ->Set(
            context,
            v8_helper_->CreateStringFromLiteral("contributeToHistogramOnEvent"),
            report_contribution_for_event_function)
        .Check();
  }

  v8::Local<v8::Function> enable_debug_mode_function =
      v8::Function::New(context, &PrivateAggregationBindings::EnableDebugMode,
                        v8_this)
          .ToLocalChecked();
  private_aggregation
      ->Set(context, v8_helper_->CreateStringFromLiteral("enableDebugMode"),
            enable_debug_mode_function)
      .Check();

  context->Global()
      ->Set(context, v8_helper_->CreateStringFromLiteral("privateAggregation"),
            private_aggregation)
      .Check();
}

void PrivateAggregationBindings::Reset() {
  private_aggregation_contributions_.clear();
  debug_mode_details_.is_enabled = false;
  debug_mode_details_.debug_key = nullptr;
}

std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
PrivateAggregationBindings::TakePrivateAggregationRequests() {
  std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr> requests;

  requests.reserve(private_aggregation_contributions_.size());
  base::ranges::transform(
      private_aggregation_contributions_, std::back_inserter(requests),
      [this](auction_worklet::mojom::AggregatableReportContributionPtr&
                 contribution) {
        return auction_worklet::mojom::PrivateAggregationRequest::New(
            std::move(contribution),
            // TODO(alexmt): consider allowing this to be set
            blink::mojom::AggregationServiceMode::kDefault,
            debug_mode_details_.Clone());
      });
  private_aggregation_contributions_.clear();

  return requests;
}

void PrivateAggregationBindings::ContributeToHistogram(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  PrivateAggregationBindings* bindings =
      static_cast<PrivateAggregationBindings*>(
          v8::External::Cast(*args.Data())->Value());
  AuctionV8Helper* v8_helper = bindings->v8_helper_;
  v8::Isolate* isolate = v8_helper->isolate();

  if (!bindings->private_aggregation_permissions_policy_allowed_) {
    isolate->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "The \"private-aggregation\" Permissions Policy denied the method "
            "on "
            "privateAggregation")));
    return;
  }

  AuctionV8Helper::TimeLimitScope time_limit_scope(v8_helper->GetTimeLimit());
  ArgsConverter args_converter(
      v8_helper, time_limit_scope,
      "privateAggregation.contributeToHistogram(): ", &args,
      /*min_required_args=*/1);

  v8::Local<v8::Value> contribution_val;
  args_converter.ConvertArg(0, "contribution", contribution_val);
  v8::Local<v8::BigInt> idl_bucket;
  int32_t idl_value;
  std::optional<v8::Local<v8::BigInt>> idl_filtering_id;
  if (args_converter.is_success()) {
    // https://patcg-individual-drafts.github.io/private-aggregation-api/#dictdef-pahistogramcontribution
    //
    // arg 0 is:
    // dictionary PAHistogramContribution {
    //   required bigint bucket;
    //   required long value;
    //   bigint filteringId;
    // };
    DictConverter contribution_converter(
        v8_helper, time_limit_scope,
        "privateAggregation.contributeToHistogram() 'contribution' argument: ",
        contribution_val);

    // Note: alphabetical to match WebIDL.
    contribution_converter.GetRequired("bucket", idl_bucket);
    if (base::FeatureList::IsEnabled(
            blink::features::kPrivateAggregationApiFilteringIds)) {
      contribution_converter.GetOptional("filteringId", idl_filtering_id);
    }
    contribution_converter.GetRequired("value", idl_value);
    args_converter.SetStatus(contribution_converter.TakeStatus());
  }

  if (args_converter.is_failed()) {
    args_converter.TakeStatus().PropagateErrorsToV8(v8_helper);
    return;
  }

  std::string error;
  std::optional<absl::uint128> maybe_bucket =
      ConvertBigIntToUint128(idl_bucket, &error);
  if (!maybe_bucket.has_value()) {
    CHECK(base::IsStringUTF8(error));
    isolate->ThrowException(v8::Exception::TypeError(
        v8_helper->CreateUtf8String(error).ToLocalChecked()));
    return;
  }
  absl::uint128 bucket = maybe_bucket.value();

  if (idl_value < 0) {
    isolate->ThrowException(v8::Exception::TypeError(
        v8_helper->CreateStringFromLiteral("Value must be non-negative")));
    return;
  }

  std::optional<uint64_t> filtering_id;
  if (!GetFilteringId(isolate, std::move(idl_filtering_id), &filtering_id,
                      &error)) {
    CHECK(base::IsStringUTF8(error));
    isolate->ThrowException(v8::Exception::TypeError(
        v8_helper->CreateUtf8String(error).ToLocalChecked()));
    return;
  }

  bindings->private_aggregation_contributions_.push_back(
      auction_worklet::mojom::AggregatableReportContribution::
          NewHistogramContribution(
              blink::mojom::AggregatableReportHistogramContribution::New(
                  bucket, idl_value, filtering_id)));
}

void PrivateAggregationBindings::ContributeToHistogramOnEvent(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  PrivateAggregationBindings* bindings =
      static_cast<PrivateAggregationBindings*>(
          v8::External::Cast(*args.Data())->Value());
  AuctionV8Helper* v8_helper = bindings->v8_helper_;
  v8::Isolate* isolate = v8_helper->isolate();

  UMA_HISTOGRAM_BOOLEAN(
      "Ads.InterestGroup.Auction.ContributeToHistogramOnEventPermissionPolicy",
      bindings->private_aggregation_permissions_policy_allowed_);
  if (!bindings->private_aggregation_permissions_policy_allowed_) {
    if (bindings->enforce_permission_policy_for_on_event_) {
      isolate->ThrowException(
          v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
              "The \"private-aggregation\" Permissions Policy denied the "
              "method contributeToHistogramOnEvent on privateAggregation")));
      return;
    } else {
      bindings->v8_logger_->LogConsoleWarning(
          "privateAggregation.contributeToHistogramOnEvent called without "
          "appropriate \"private-aggregation\" Permissions Policy approval; "
          "accepting for backwards compatibility but this will be shortly "
          "throwing an exception");
    }
  }

  AuctionV8Helper::TimeLimitScope time_limit_scope(v8_helper->GetTimeLimit());
  ArgsConverter args_converter(
      v8_helper, time_limit_scope,
      "privateAggregation.contributeToHistogramOnEvent(): ", &args,
      /*min_required_args=*/2);
  std::string event_type_str;
  args_converter.ConvertArg(0, "event", event_type_str);

  // Arg 1 is:
  // https://patcg-individual-drafts.github.io/private-aggregation-api/#dictdef-paextendedhistogramcontribution
  //
  // dictionary PAExtendedHistogramContribution {
  //   required (PASignalValue or bigint) bucket;
  //   required (PASignalValue or long) value;
  //   bigint filteringId;
  // };

  absl::variant<PASignalValue, v8::Local<v8::BigInt>> bucket;
  absl::variant<PASignalValue, int32_t> value;
  std::optional<v8::Local<v8::BigInt>> filtering_id;
  if (args_converter.is_success()) {
    DictConverter contribution_converter(
        v8_helper, time_limit_scope,
        "privateAggregation.contributeToHistogramOnEvent() 'contribution' "
        "argument: ",
        args[1]);

    v8::Local<v8::Value> bucket_val, value_val;
    // Note: alphabetical to match WebIDL.
    contribution_converter.GetRequired("bucket", bucket_val) &&
        ConvertToPASignalValueOr<v8::Local<v8::BigInt>>(
            v8_helper, time_limit_scope,
            "privateAggregation.contributeToHistogramOnEvent() 'contribution' "
            "argument: ",
            "bucket", bucket_val, contribution_converter, bucket);
    if (base::FeatureList::IsEnabled(
            blink::features::kPrivateAggregationApiFilteringIds)) {
      contribution_converter.GetOptional("filteringId", filtering_id);
    }
    contribution_converter.GetRequired("value", value_val) &&
        ConvertToPASignalValueOr<int32_t>(
            v8_helper, time_limit_scope,
            "privateAggregation.contributeToHistogramOnEvent() 'contribution' "
            "argument: ",
            "value", value_val, contribution_converter, value);
    args_converter.SetStatus(contribution_converter.TakeStatus());
  }

  if (args_converter.is_failed()) {
    args_converter.TakeStatus().PropagateErrorsToV8(v8_helper);
    return;
  }

  auction_worklet::mojom::EventTypePtr event_type =
      ParsePrivateAggregationEventType(
          event_type_str, bindings->additional_extensions_allowed_);
  if (!event_type) {
    // Don't throw an error if an invalid reserved event type is provided, to
    // provide forward compatibility with new reserved event types added
    // later.
    return;
  }

  if (!bindings->reserved_once_allowed_ && event_type->is_reserved() &&
      event_type->get_reserved() ==
          auction_worklet::mojom::ReservedEventType::kReservedOnce) {
    // Do throw one if people use reserved.once when not permitted.
    isolate->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "privateAggregation.contributeToHistogramOnEvent() reserved.once "
            "is not available in reporting methods")));
    return;
  }

  std::string error;
  auction_worklet::mojom::AggregatableReportForEventContributionPtr
      contribution = ParseForEventContribution(
          isolate, std::move(event_type), std::move(bucket), std::move(value),
          std::move(filtering_id), bindings->additional_extensions_allowed_,
          &error);

  if (contribution.is_null()) {
    CHECK(base::IsStringUTF8(error));
    isolate->ThrowException(v8::Exception::TypeError(
        v8_helper->CreateUtf8String(error).ToLocalChecked()));
    return;
  }

  bindings->private_aggregation_contributions_.push_back(
      auction_worklet::mojom::AggregatableReportContribution::
          NewForEventContribution(std::move(contribution)));
}

void PrivateAggregationBindings::EnableDebugMode(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  PrivateAggregationBindings* bindings =
      static_cast<PrivateAggregationBindings*>(
          v8::External::Cast(*args.Data())->Value());

  AuctionV8Helper* v8_helper = bindings->v8_helper_;
  v8::Isolate* isolate = v8_helper->isolate();

  if (!bindings->private_aggregation_permissions_policy_allowed_) {
    isolate->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "The \"private-aggregation\" Permissions Policy denied the method "
            "on privateAggregation")));
    return;
  }

  // Do IDL typechecking first.
  v8::Local<v8::BigInt> js_debug_key;
  if (args.Length() >= 1) {
    AuctionV8Helper::TimeLimitScope time_limit_scope(v8_helper->GetTimeLimit());
    DictConverter debug_options_converter(
        v8_helper, time_limit_scope,
        "privateAggregation.enableDebugMode() 'options' argument: ", args[0]);
    if (!debug_options_converter.GetRequired("debugKey", js_debug_key)) {
      debug_options_converter.TakeStatus().PropagateErrorsToV8(v8_helper);
      return;
    }
  }

  // Now do the rest of the checks.
  if (bindings->debug_mode_details_.is_enabled) {
    isolate->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "enableDebugMode may be called at most once")));
    return;
  }

  if (!js_debug_key.IsEmpty()) {
    std::string error;
    std::optional<uint64_t> maybe_debug_key =
        ParseDebugKey(js_debug_key, &error);
    if (!maybe_debug_key.has_value()) {
      CHECK(base::IsStringUTF8(error));
      isolate->ThrowException(v8::Exception::TypeError(
          v8_helper->CreateUtf8String(error).ToLocalChecked()));
      return;
    }

    bindings->debug_mode_details_.debug_key =
        blink::mojom::DebugKey::New(maybe_debug_key.value());
  }

  bindings->debug_mode_details_.is_enabled = true;
}

}  // namespace auction_worklet
