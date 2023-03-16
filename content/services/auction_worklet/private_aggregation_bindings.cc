// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/private_aggregation_bindings.h"

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
#include "content/services/worklet_utils/private_aggregation_utils.h"
#include "gin/arguments.h"
#include "gin/dictionary.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/private_aggregation/aggregatable_report.mojom.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-template.h"

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
// Modified from `worklet_utils::ConvertBigIntToUint128()`.
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
        worklet_utils::ConvertBigIntToUint128(js_bucket.As<v8::BigInt>(),
                                              &bucket_error);
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
  if (js_value->IsInt32()) {
    int int_value = js_value.As<v8::Int32>()->Value();
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
    *error = "Value must be an integer or a dictionary";
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
        "Invalid or missing bucket in reportContributionForEvent's argument";
    return nullptr;
  }

  v8::Local<v8::Value> js_value;
  if (!dict.Get("value", &js_value) || js_value.IsEmpty() ||
      js_value->IsNullOrUndefined()) {
    *error =
        "Invalid or missing value in reportContributionForEvent's argument";
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

void PrivateAggregationBindings::FillInGlobalTemplate(
    v8::Local<v8::ObjectTemplate> global_template) {
  if (!base::FeatureList::IsEnabled(blink::features::kPrivateAggregationApi) ||
      !blink::features::kPrivateAggregationApiEnabledInFledge.Get()) {
    return;
  }

  v8::Local<v8::External> v8_this =
      v8::External::New(v8_helper_->isolate(), this);

  v8::Local<v8::ObjectTemplate> private_aggregation_template =
      v8::ObjectTemplate::New(v8_helper_->isolate());

  v8::Local<v8::FunctionTemplate> send_histogram_report_template =
      v8::FunctionTemplate::New(
          v8_helper_->isolate(),
          &PrivateAggregationBindings::SendHistogramReport, v8_this);
  send_histogram_report_template->RemovePrototype();
  private_aggregation_template->Set(
      v8_helper_->CreateStringFromLiteral("sendHistogramReport"),
      send_histogram_report_template);

  if (blink::features::kPrivateAggregationApiFledgeExtensionsEnabled.Get()) {
    v8::Local<v8::FunctionTemplate> report_contribution_for_event_template =
        v8::FunctionTemplate::New(
            v8_helper_->isolate(),
            &PrivateAggregationBindings::ReportContributionForEvent, v8_this);
    report_contribution_for_event_template->RemovePrototype();
    private_aggregation_template->Set(
        v8_helper_->CreateStringFromLiteral("reportContributionForEvent"),
        report_contribution_for_event_template);
  }

  v8::Local<v8::FunctionTemplate> enable_debug_mode_template =
      v8::FunctionTemplate::New(v8_helper_->isolate(),
                                &PrivateAggregationBindings::EnableDebugMode,
                                v8_this);
  enable_debug_mode_template->RemovePrototype();
  private_aggregation_template->Set(
      v8_helper_->CreateStringFromLiteral("enableDebugMode"),
      enable_debug_mode_template);

  global_template->Set(
      v8_helper_->CreateStringFromLiteral("privateAggregation"),
      private_aggregation_template);
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

void PrivateAggregationBindings::SendHistogramReport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  PrivateAggregationBindings* bindings =
      static_cast<PrivateAggregationBindings*>(
          v8::External::Cast(*args.Data())->Value());

  blink::mojom::AggregatableReportHistogramContributionPtr contribution =
      worklet_utils::ParseSendHistogramReportArguments(
          gin::Arguments(args),
          bindings->private_aggregation_permissions_policy_allowed_);
  if (contribution.is_null()) {
    // Indicates an exception was thrown.
    return;
  }

  bindings->private_aggregation_contributions_.push_back(
      auction_worklet::mojom::AggregatableReportContribution::
          NewHistogramContribution(std::move(contribution)));
}

void PrivateAggregationBindings::ReportContributionForEvent(
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
            "reportContributionForEvent requires 2 parameters, with first "
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

  worklet_utils::ParseAndApplyEnableDebugModeArguments(
      gin::Arguments(args),
      bindings->private_aggregation_permissions_policy_allowed_,
      bindings->debug_mode_details_);
}

}  // namespace auction_worklet
