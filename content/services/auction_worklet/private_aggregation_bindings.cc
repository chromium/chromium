// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/private_aggregation_bindings.h"

#include <stdint.h>

#include <cmath>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "content/services/auction_worklet/webidl_compat.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/private_aggregation/aggregatable_report.mojom.h"
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
  absl::optional<double> scale;
  absl::optional<absl::variant<int32_t, v8::Local<v8::BigInt>>> offset;
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

// Converts base value string to corresponding mojom enum.
absl::optional<auction_worklet::mojom::BaseValue> BaseValueStringToEnum(
    const std::string& base_value) {
  if (base_value == "winning-bid") {
    return auction_worklet::mojom::BaseValue::kWinningBid;
  } else if (base_value == "highest-scoring-other-bid") {
    return auction_worklet::mojom::BaseValue::kHighestScoringOtherBid;
  } else if (base_value == "script-run-time") {
    return auction_worklet::mojom::BaseValue::kScriptRunTime;
  } else if (base_value == "signals-fetch-time") {
    return auction_worklet::mojom::BaseValue::kSignalsFetchTime;
  } else if (base_value == "bid-reject-reason") {
    return auction_worklet::mojom::BaseValue::kBidRejectReason;
  }
  // Invalid (out of range) base_value.
  return absl::nullopt;
}

// If returns `absl::nullopt`, will output an error to `error`.
absl::optional<absl::uint128> ConvertBigIntToUint128(
    v8::Local<v8::BigInt> bigint,
    std::string* error) {
  if (bigint->WordCount() > 2) {
    *error = "BigInt is too large";
    return absl::nullopt;
  }
  // Signals the size of the `words` array to `ToWordsArray()`. The number of
  // elements actually used is then written here by the function.
  int word_count = 2;
  int sign_bit = 0;
  uint64_t words[2] = {0, 0};  // Least significant to most significant.
  bigint->ToWordsArray(&sign_bit, &word_count, words);
  if (sign_bit) {
    *error = "BigInt must be non-negative";
    return absl::nullopt;
  }

  return absl::MakeUint128(words[1], words[0]);
}

// If returns `absl::nullopt`, will output an error to `error`.
// Modified from `ConvertBigIntToUint128()`.
absl::optional<auction_worklet::mojom::BucketOffsetPtr>
ConvertBigIntToBucketOffset(v8::Local<v8::BigInt> bigint, std::string* error) {
  if (bigint->WordCount() > 2) {
    *error = "Bucket BigInt is too large";
    return absl::nullopt;
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

absl::optional<auction_worklet::mojom::SignalBucketPtr> GetSignalBucket(
    v8::Isolate* isolate,
    const PASignalValue& input,
    std::string* error) {
  absl::optional<auction_worklet::mojom::BaseValue> base_value_opt =
      BaseValueStringToEnum(input.base_value);
  if (!base_value_opt.has_value()) {
    *error = "Bucket's 'baseValue' is invalid";
    return absl::nullopt;
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
    return absl::nullopt;
  }

  absl::optional<auction_worklet::mojom::BucketOffsetPtr> offset_opt =
      ConvertBigIntToBucketOffset(*maybe_bigint, error);
  if (!offset_opt.has_value()) {
    return nullptr;
  }
  return auction_worklet::mojom::SignalBucket::New(*base_value_opt, scale,
                                                   std::move(*offset_opt));
}

absl::optional<auction_worklet::mojom::SignalValuePtr> GetSignalValue(
    v8::Isolate* isolate,
    const PASignalValue& input,
    std::string* error) {
  absl::optional<auction_worklet::mojom::BaseValue> base_value_opt =
      BaseValueStringToEnum(input.base_value);
  if (!base_value_opt.has_value()) {
    *error = "Value's 'baseValue' is invalid";
    return absl::nullopt;
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
    return absl::nullopt;
  }
  return auction_worklet::mojom::SignalValue::New(*base_value_opt, scale,
                                                  *maybe_long);
}

// Returns contribution's bucket from `idl_bucket`. Returns nullptr if there is
// an error.
auction_worklet::mojom::ForEventSignalBucketPtr GetBucket(
    v8::Isolate* isolate,
    const absl::variant<PASignalValue, v8::Local<v8::BigInt>>& idl_bucket,
    std::string* error) {
  const v8::Local<v8::BigInt>* big_int =
      absl::get_if<v8::Local<v8::BigInt>>(&idl_bucket);
  if (big_int) {
    absl::optional<absl::uint128> maybe_bucket =
        ConvertBigIntToUint128(*big_int, error);
    if (!maybe_bucket.has_value()) {
      CHECK(base::IsStringUTF8(*error));
      return nullptr;
    }
    return auction_worklet::mojom::ForEventSignalBucket::NewIdBucket(
        *maybe_bucket);
  } else {
    absl::optional<auction_worklet::mojom::SignalBucketPtr>
        maybe_signal_bucket_ptr = GetSignalBucket(
            isolate, absl::get<PASignalValue>(idl_bucket), error);
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
    std::string* error) {
  const int32_t* int_value = absl::get_if<int32_t>(&idl_value);
  if (int_value) {
    if (*int_value < 0) {
      *error = "Value must be non-negative";
      return nullptr;
    }
    return auction_worklet::mojom::ForEventSignalValue::NewIntValue(*int_value);
  } else {
    absl::optional<auction_worklet::mojom::SignalValuePtr>
        maybe_signal_value_ptr =
            GetSignalValue(isolate, absl::get<PASignalValue>(idl_value), error);
    if (!maybe_signal_value_ptr.has_value()) {
      CHECK(base::IsStringUTF8(*error));
      return nullptr;
    }
    return auction_worklet::mojom::ForEventSignalValue::NewSignalValue(
        std::move(*maybe_signal_value_ptr));
  }
}

auction_worklet::mojom::AggregatableReportForEventContributionPtr
ParseForEventContribution(
    v8::Isolate* isolate,
    const std::string& event_type,
    absl::variant<PASignalValue, v8::Local<v8::BigInt>> idl_bucket,
    absl::variant<PASignalValue, int32_t> idl_value,
    std::string* error) {
  auction_worklet::mojom::ForEventSignalBucketPtr bucket =
      GetBucket(isolate, std::move(idl_bucket), error);
  if (!bucket) {
    return nullptr;
  }
  auction_worklet::mojom::ForEventSignalValuePtr value =
      GetValue(isolate, std::move(idl_value), error);
  if (!value) {
    return nullptr;
  }

  return auction_worklet::mojom::AggregatableReportForEventContribution::New(
      std::move(bucket), std::move(value), std::move(event_type));
}

// In case of failure, will return `absl::nullopt` and output an error to
// `error`.
absl::optional<uint64_t> ParseDebugKey(v8::Local<v8::BigInt> js_debug_key,
                                       std::string* error) {
  absl::optional<absl::uint128> maybe_debug_key =
      ConvertBigIntToUint128(js_debug_key.As<v8::BigInt>(), error);
  if (!maybe_debug_key.has_value()) {
    return absl::nullopt;
  }
  if (absl::Uint128High64(maybe_debug_key.value()) != 0) {
    *error = "BigInt is too large";
    return absl::nullopt;
  }
  return absl::Uint128Low64(maybe_debug_key.value());
}

}  // namespace

const char kReservedAlways[] = "reserved.always";
const char kReservedWin[] = "reserved.win";
const char kReservedLoss[] = "reserved.loss";

PrivateAggregationBindings::PrivateAggregationBindings(
    AuctionV8Helper* v8_helper,
    bool private_aggregation_permissions_policy_allowed)
    : v8_helper_(v8_helper),
      private_aggregation_permissions_policy_allowed_(
          private_aggregation_permissions_policy_allowed) {}

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
  if (args_converter.is_success()) {
    // https://patcg-individual-drafts.github.io/private-aggregation-api/#dictdef-pahistogramcontribution
    //
    // arg 0 is:
    // dictionary PAHistogramContribution {
    //   required bigint bucket;
    //   required long value;
    // };
    DictConverter contribution_converter(
        v8_helper, time_limit_scope,
        "privateAggregation.contributeToHistogram() 'contribution' argument: ",
        contribution_val);
    contribution_converter.GetRequired("bucket", idl_bucket);
    contribution_converter.GetRequired("value", idl_value);
    args_converter.SetStatus(contribution_converter.TakeStatus());
  }

  if (args_converter.is_failed()) {
    args_converter.TakeStatus().PropagateErrorsToV8(v8_helper);
    return;
  }

  std::string error;
  absl::optional<absl::uint128> maybe_bucket =
      ConvertBigIntToUint128(idl_bucket, &error);
  if (!maybe_bucket.has_value()) {
    DCHECK(base::IsStringUTF8(error));
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

  bindings->private_aggregation_contributions_.push_back(
      auction_worklet::mojom::AggregatableReportContribution::
          NewHistogramContribution(
              blink::mojom::AggregatableReportHistogramContribution::New(
                  bucket, idl_value)));
}

void PrivateAggregationBindings::ContributeToHistogramOnEvent(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  PrivateAggregationBindings* bindings =
      static_cast<PrivateAggregationBindings*>(
          v8::External::Cast(*args.Data())->Value());
  AuctionV8Helper* v8_helper = bindings->v8_helper_;
  v8::Isolate* isolate = v8_helper->isolate();

  AuctionV8Helper::TimeLimitScope time_limit_scope(v8_helper->GetTimeLimit());
  ArgsConverter args_converter(
      v8_helper, time_limit_scope,
      "privateAggregation.contributeToHistogramOnEvent(): ", &args,
      /*min_required_args=*/2);
  std::string event_type;
  args_converter.ConvertArg(0, "event", event_type);

  // Arg 1 is:
  // https://patcg-individual-drafts.github.io/private-aggregation-api/#dictdef-paextendedhistogramcontribution
  //
  // dictionary PAExtendedHistogramContribution {
  //   required (PASignalValue or bigint) bucket;
  //   required (PASignalValue or long) value;
  // };

  absl::variant<PASignalValue, v8::Local<v8::BigInt>> bucket;
  absl::variant<PASignalValue, int32_t> value;
  if (args_converter.is_success()) {
    DictConverter contribution_converter(
        v8_helper, time_limit_scope,
        "privateAggregation.contributeToHistogramOnEvent() 'contribution' "
        "argument: ",
        args[1]);

    v8::Local<v8::Value> bucket_val, value_val;
    contribution_converter.GetRequired("bucket", bucket_val) &&
        ConvertToPASignalValueOr<v8::Local<v8::BigInt>>(
            v8_helper, time_limit_scope,
            "privateAggregation.contributeToHistogramOnEvent() 'contribution' "
            "argument: ",
            "bucket", bucket_val, contribution_converter, bucket) &&
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

  if (base::StartsWith(event_type, "reserved.") && event_type != kReservedWin &&
      event_type != kReservedLoss && event_type != kReservedAlways) {
    // Don't throw an error if an invalid reserved event type is provided, to
    // provide forward compatibility with new reserved event types added later.
    return;
  }

  std::string error;
  auction_worklet::mojom::AggregatableReportForEventContributionPtr
      contribution = ParseForEventContribution(
          isolate, event_type, std::move(bucket), std::move(value), &error);

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
            "on "
            "privateAggregation")));
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
    absl::optional<uint64_t> maybe_debug_key =
        ParseDebugKey(js_debug_key, &error);
    if (!maybe_debug_key.has_value()) {
      DCHECK(base::IsStringUTF8(error));
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
