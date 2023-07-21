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
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

// If returns `absl::nullopt`, will output an error to `error_out`.
absl::optional<absl::uint128> ConvertBigIntToUint128(
    v8::Local<v8::BigInt> bigint,
    std::string* error_out) {
  if (bigint.IsEmpty()) {
    *error_out = "Failed to interpret as BigInt";
    return absl::nullopt;
  }
  if (bigint->WordCount() > 2) {
    *error_out = "BigInt is too large";
    return absl::nullopt;
  }
  // Signals the size of the `words` array to `ToWordsArray()`. The number of
  // elements actually used is then written here by the function.
  int word_count = 2;
  int sign_bit = 0;
  uint64_t words[2] = {0, 0};  // Least significant to most significant.
  bigint->ToWordsArray(&sign_bit, &word_count, words);
  if (sign_bit) {
    *error_out = "BigInt must be non-negative";
    return absl::nullopt;
  }

  return absl::MakeUint128(words[1], words[0]);
}

// If returns `absl::nullopt`, will output an error to `error_out`.
// Modified from `ConvertBigIntToUint128()`.
absl::optional<auction_worklet::mojom::BucketOffsetPtr>
ConvertBigIntToBucketOffset(v8::Local<v8::BigInt> bigint,
                            std::string* error_out) {
  if (bigint.IsEmpty()) {
    *error_out = "Failed to interpret as BigInt";
    return absl::nullopt;
  }
  if (bigint->WordCount() > 2) {
    *error_out = "BigInt is too large";
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

absl::optional<auction_worklet::mojom::BaseValue> GetBaseValue(
    gin::Dictionary& dictionary) {
  std::string base_value_string;
  if (!dictionary.Get("baseValue", &base_value_string)) {
    return absl::nullopt;
  }
  return BaseValueStringToEnum(base_value_string);
}

// Returns scale field in `dictionary` if it exists and is valid. Returns 1.0
// if it does not exist. Returns absl::nullopt if it exists but is invalid.
absl::optional<double> GetScale(gin::Dictionary& dictionary) {
  double scale = 1.0;
  v8::Local<v8::Value> js_scale;
  if (dictionary.Get("scale", &js_scale) && !js_scale.IsEmpty() &&
      !js_scale->IsNullOrUndefined()) {
    if (!js_scale->IsNumber()) {
      return absl::nullopt;
    }
    scale = js_scale.As<v8::Number>()->Value();
    if (!std::isfinite(scale)) {
      return absl::nullopt;
    }
  }
  return scale;
}

absl::optional<auction_worklet::mojom::SignalBucketPtr> GetSignalBucket(
    v8::Isolate* isolate,
    v8::Local<v8::Value> input) {
  CHECK(input->IsObject());
  gin::Dictionary result_dict(isolate, input.As<v8::Object>());

  absl::optional<auction_worklet::mojom::BaseValue> base_value_opt =
      GetBaseValue(result_dict);
  if (!base_value_opt.has_value()) {
    return absl::nullopt;
  }

  absl::optional<double> scale_opt = GetScale(result_dict);
  if (!scale_opt.has_value()) {
    return absl::nullopt;
  }

  v8::Local<v8::Value> js_offset;
  if (!result_dict.Get("offset", &js_offset) || js_offset.IsEmpty() ||
      js_offset->IsNullOrUndefined()) {
    return auction_worklet::mojom::SignalBucket::New(*base_value_opt,
                                                     *scale_opt,
                                                     /*offset=*/nullptr);
  }

  // Offset must be BigInt for bucket.
  if (!js_offset->IsBigInt()) {
    return absl::nullopt;
  }

  // TODO(qingxinwu): `error` is ignored currently. Report it and consider
  // surfacing more informative errors like "offset must be BigInt for bucket".
  std::string error;
  absl::optional<auction_worklet::mojom::BucketOffsetPtr> offset_opt =
      ConvertBigIntToBucketOffset(js_offset.As<v8::BigInt>(), &error);
  if (!offset_opt.has_value()) {
    return nullptr;
  }
  return auction_worklet::mojom::SignalBucket::New(*base_value_opt, *scale_opt,
                                                   std::move(*offset_opt));
}

absl::optional<auction_worklet::mojom::SignalValuePtr> GetSignalValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> input) {
  CHECK(input->IsObject());
  gin::Dictionary result_dict(isolate, input.As<v8::Object>());

  absl::optional<auction_worklet::mojom::BaseValue> base_value_opt =
      GetBaseValue(result_dict);
  if (!base_value_opt.has_value()) {
    return absl::nullopt;
  }

  absl::optional<double> scale_opt = GetScale(result_dict);
  if (!scale_opt.has_value()) {
    return absl::nullopt;
  }

  v8::Local<v8::Value> js_offset;
  if (!result_dict.Get("offset", &js_offset) || js_offset.IsEmpty() ||
      js_offset->IsNullOrUndefined()) {
    return auction_worklet::mojom::SignalValue::New(*base_value_opt, *scale_opt,
                                                    /*offset=*/0);
  }

  // Offset must be int32 for value.
  if (!js_offset->IsInt32()) {
    return absl::nullopt;
  }
  int32_t offset = js_offset.As<v8::Int32>()->Value();
  return auction_worklet::mojom::SignalValue::New(*base_value_opt, *scale_opt,
                                                  offset);
}

// Returns contribution's bucket from `js_value`. Returns nullptr if there is an
// error.
auction_worklet::mojom::ForEventSignalBucketPtr GetBucket(
    v8::Isolate* isolate,
    v8::Local<v8::Value> js_bucket,
    std::string* error) {
  auction_worklet::mojom::ForEventSignalBucketPtr bucket;
  if (js_bucket->IsBigInt()) {
    std::string bucket_error;
    absl::optional<absl::uint128> maybe_bucket =
        ConvertBigIntToUint128(js_bucket.As<v8::BigInt>(), &bucket_error);
    if (!maybe_bucket.has_value()) {
      CHECK(base::IsStringUTF8(bucket_error));
      *error = bucket_error;
      return nullptr;
    }
    bucket = auction_worklet::mojom::ForEventSignalBucket::NewIdBucket(
        *maybe_bucket);
  } else if (js_bucket->IsObject()) {
    absl::optional<auction_worklet::mojom::SignalBucketPtr>
        maybe_signal_bucket_ptr = GetSignalBucket(isolate, js_bucket);
    if (!maybe_signal_bucket_ptr.has_value()) {
      *error = "Invalid bucket dictionary";
      return nullptr;
    }
    bucket = auction_worklet::mojom::ForEventSignalBucket::NewSignalBucket(
        std::move(*maybe_signal_bucket_ptr));
  } else {
    *error = "Bucket must be a BigInt or a dictionary";
    return nullptr;
  }
  return bucket;
}

// Returns contribution's value from `js_value`. Returns nullptr if there is an
// error.
auction_worklet::mojom::ForEventSignalValuePtr GetValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> js_value,
    std::string* error) {
  auction_worklet::mojom::ForEventSignalValuePtr value;
  if (js_value->IsNumber()) {
    v8::Maybe<int32_t> converted_value =
        js_value->Int32Value(isolate->GetCurrentContext());
    CHECK(converted_value.IsJust());
    int int_value = converted_value.ToChecked();
    value = auction_worklet::mojom::ForEventSignalValue::NewIntValue(int_value);
    if (int_value < 0) {
      *error = "Value must be non-negative";
      return nullptr;
    }
  } else if (js_value->IsObject()) {
    absl::optional<auction_worklet::mojom::SignalValuePtr>
        maybe_signal_value_ptr = GetSignalValue(isolate, js_value);
    if (!maybe_signal_value_ptr.has_value()) {
      *error = "Invalid value dictionary";
      return nullptr;
    }
    value = auction_worklet::mojom::ForEventSignalValue::NewSignalValue(
        std::move(*maybe_signal_value_ptr));
  } else if (js_value->IsBigInt()) {
    *error = "Value cannot be a BigInt";
    return nullptr;
  } else {
    *error = "Value must be a Number or a dictionary";
    return nullptr;
  }
  return value;
}

auction_worklet::mojom::AggregatableReportForEventContributionPtr
ParseForEventContribution(v8::Isolate* isolate,
                          const std::string& event_type,
                          v8::Local<v8::Value> arg_contribution,
                          std::string* error) {
  gin::Dictionary dict(isolate);
  bool success = gin::ConvertFromV8(isolate, arg_contribution, &dict);
  CHECK(success);

  v8::Local<v8::Value> js_bucket;
  if (!dict.Get("bucket", &js_bucket) || js_bucket.IsEmpty() ||
      js_bucket->IsNullOrUndefined()) {
    *error =
        "Invalid or missing bucket in contributeToHistogramOnEvent's argument";
    return nullptr;
  }

  v8::Local<v8::Value> js_value;
  if (!dict.Get("value", &js_value) || js_value.IsEmpty() ||
      js_value->IsNullOrUndefined()) {
    *error =
        "Invalid or missing value in contributeToHistogramOnEvent's argument";
    return nullptr;
  }

  auction_worklet::mojom::ForEventSignalBucketPtr bucket =
      GetBucket(isolate, std::move(js_bucket), error);
  if (!bucket) {
    return nullptr;
  }
  auction_worklet::mojom::ForEventSignalValuePtr value =
      GetValue(isolate, std::move(js_value), error);
  if (!value) {
    return nullptr;
  }

  return auction_worklet::mojom::AggregatableReportForEventContribution::New(
      std::move(bucket), std::move(value), std::move(event_type));
}

v8::Local<v8::String> CreateStringFromLiteral(v8::Isolate* isolate,
                                              const char* ascii_string) {
  DCHECK(base::IsStringASCII(ascii_string));
  return v8::String::NewFromUtf8(isolate, ascii_string,
                                 v8::NewStringType::kNormal,
                                 strlen(ascii_string))
      .ToLocalChecked();
}

v8::MaybeLocal<v8::String> CreateUtf8String(v8::Isolate* isolate,
                                            base::StringPiece utf8_string) {
  if (!base::IsStringUTF8(utf8_string)) {
    return v8::MaybeLocal<v8::String>();
  }
  return v8::String::NewFromUtf8(isolate, utf8_string.data(),
                                 v8::NewStringType::kNormal,
                                 utf8_string.length());
}

// In case of failure, will return `absl::nullopt` and output an error to
// `error_out`.
absl::optional<uint64_t> ParseDebugKey(v8::Local<v8::Value> js_debug_key,
                                       v8::Local<v8::Context>& context,
                                       std::string* error_out) {
  if (js_debug_key.IsEmpty() || js_debug_key->IsNullOrUndefined()) {
    return absl::nullopt;
  }

  if (js_debug_key->IsBigInt()) {
    absl::optional<absl::uint128> maybe_debug_key =
        ConvertBigIntToUint128(js_debug_key.As<v8::BigInt>(), error_out);
    if (!maybe_debug_key.has_value()) {
      return absl::nullopt;
    }
    if (absl::Uint128High64(maybe_debug_key.value()) != 0) {
      *error_out = "BigInt is too large";
      return absl::nullopt;
    }
    return absl::Uint128Low64(maybe_debug_key.value());
  }

  *error_out = "debugKey must be a BigInt";
  return absl::nullopt;
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

  gin::Arguments gin_args(args);
  v8::Isolate* isolate = gin_args.isolate();

  if (!bindings->private_aggregation_permissions_policy_allowed_) {
    isolate->ThrowException(v8::Exception::TypeError(CreateStringFromLiteral(
        isolate,
        "The \"private-aggregation\" Permissions Policy denied the method on "
        "privateAggregation")));
    return;
  }

  std::vector<v8::Local<v8::Value>> argument_list = gin_args.GetAll();

  // Any additional arguments are ignored.
  if (argument_list.size() == 0 || argument_list[0].IsEmpty() ||
      !argument_list[0]->IsObject()) {
    isolate->ThrowException(v8::Exception::TypeError(CreateStringFromLiteral(
        isolate, "contributeToHistogram requires 1 object parameter")));
    return;
  }

  gin::Dictionary dict(isolate);

  bool success = gin::ConvertFromV8(isolate, argument_list[0], &dict);
  DCHECK(success);

  v8::Local<v8::Value> js_bucket;
  v8::Local<v8::Value> js_value;

  if (!dict.Get("bucket", &js_bucket)) {
    // Propagate any exception
    return;
  }
  if (js_bucket.IsEmpty() || js_bucket->IsNullOrUndefined()) {
    isolate->ThrowException(v8::Exception::TypeError(CreateStringFromLiteral(
        isolate,
        "Invalid or missing bucket in contributeToHistogram argument")));
    return;
  }

  if (!dict.Get("value", &js_value)) {
    // Propagate any exception
    return;
  }
  if (js_value.IsEmpty() || js_value->IsNullOrUndefined()) {
    isolate->ThrowException(v8::Exception::TypeError(CreateStringFromLiteral(
        isolate,
        "Invalid or missing value in contributeToHistogram argument")));
    return;
  }

  absl::uint128 bucket;
  int value;

  if (js_bucket->IsBigInt()) {
    std::string error;
    absl::optional<absl::uint128> maybe_bucket =
        ConvertBigIntToUint128(js_bucket.As<v8::BigInt>(), &error);
    if (!maybe_bucket.has_value()) {
      DCHECK(base::IsStringUTF8(error));
      isolate->ThrowException(v8::Exception::TypeError(
          CreateUtf8String(isolate, error).ToLocalChecked()));
      return;
    }
    bucket = maybe_bucket.value();
  } else {
    isolate->ThrowException(v8::Exception::TypeError(
        CreateStringFromLiteral(isolate, "bucket must be a BigInt")));
    return;
  }

  if (js_value->IsNumber()) {
    v8::Maybe<int32_t> converted_value =
        js_value->Int32Value(isolate->GetCurrentContext());
    CHECK(converted_value.IsJust());
    value = converted_value.ToChecked();
  } else if (js_value->IsBigInt()) {
    isolate->ThrowException(v8::Exception::TypeError(
        CreateStringFromLiteral(isolate, "Value cannot be a BigInt")));
    return;
  } else {
    isolate->ThrowException(v8::Exception::TypeError(
        CreateStringFromLiteral(isolate, "Value must be a Number")));
    return;
  }

  if (value < 0) {
    isolate->ThrowException(v8::Exception::TypeError(
        CreateStringFromLiteral(isolate, "Value must be non-negative")));
    return;
  }

  bindings->private_aggregation_contributions_.push_back(
      auction_worklet::mojom::AggregatableReportContribution::
          NewHistogramContribution(
              blink::mojom::AggregatableReportHistogramContribution::New(
                  bucket, value)));
}

void PrivateAggregationBindings::ContributeToHistogramOnEvent(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  PrivateAggregationBindings* bindings =
      static_cast<PrivateAggregationBindings*>(
          v8::External::Cast(*args.Data())->Value());
  AuctionV8Helper* v8_helper = bindings->v8_helper_;
  v8::Isolate* isolate = v8_helper->isolate();

  // Any additional arguments are ignored.
  std::string event_type;
  if (args.Length() < 2 || args[0].IsEmpty() || args[1].IsEmpty() ||
      !gin::ConvertFromV8(isolate, args[0], &event_type) ||
      !args[1]->IsObject()) {
    isolate->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "contributeToHistogramOnEvent requires 2 parameters, with first "
            "parameter being a string and second parameter being an object")));
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
      contribution =
          ParseForEventContribution(isolate, event_type, args[1], &error);

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

  gin::Arguments gin_args(args);
  v8::Isolate* isolate = gin_args.isolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  if (!bindings->private_aggregation_permissions_policy_allowed_) {
    isolate->ThrowException(v8::Exception::TypeError(CreateStringFromLiteral(
        isolate,
        "The \"private-aggregation\" Permissions Policy denied the method on "
        "privateAggregation")));
    return;
  }

  std::vector<v8::Local<v8::Value>> argument_list = gin_args.GetAll();

  if (bindings->debug_mode_details_.is_enabled) {
    isolate->ThrowException(v8::Exception::TypeError(CreateStringFromLiteral(
        isolate, "enableDebugMode may be called at most once")));
    return;
  }

  // If no arguments are provided, no debug key is set.
  if (argument_list.size() >= 1 && !argument_list[0].IsEmpty()) {
    gin::Dictionary dict(isolate);

    if (!gin::ConvertFromV8(isolate, argument_list[0], &dict)) {
      isolate->ThrowException(v8::Exception::TypeError(CreateStringFromLiteral(
          isolate, "Invalid argument in enableDebugMode")));
      return;
    }

    v8::Local<v8::Value> js_debug_key;

    if (!dict.Get("debugKey", &js_debug_key)) {
      // Propagate any exception
      return;
    }

    std::string error;
    absl::optional<uint64_t> maybe_debug_key =
        ParseDebugKey(js_debug_key, context, &error);
    if (!maybe_debug_key.has_value()) {
      DCHECK(base::IsStringUTF8(error));
      isolate->ThrowException(v8::Exception::TypeError(
          CreateUtf8String(isolate, error).ToLocalChecked()));
      return;
    }

    bindings->debug_mode_details_.debug_key =
        blink::mojom::DebugKey::New(maybe_debug_key.value());
  }

  bindings->debug_mode_details_.is_enabled = true;
}

}  // namespace auction_worklet
