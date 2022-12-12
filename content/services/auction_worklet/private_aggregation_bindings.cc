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
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "content/common/aggregatable_report.mojom.h"
#include "content/common/private_aggregation_features.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "content/services/worklet_utils/private_aggregation_utils.h"
#include "gin/arguments.h"
#include "gin/dictionary.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
  if (base_value == "winningBid") {
    return auction_worklet::mojom::BaseValue::kWinningBid;
  } else if (base_value == "highestScoringOtherBid") {
    return auction_worklet::mojom::BaseValue::kHighestScoringOtherBid;
  } else if (base_value == "scriptRunTime") {
    return auction_worklet::mojom::BaseValue::kScriptRunTime;
  } else if (base_value == "signalsFetchTime") {
    return auction_worklet::mojom::BaseValue::kSignalsFetchTime;
  } else if (base_value == "bidRejectReason") {
    return auction_worklet::mojom::BaseValue::kBidRejectReason;
  }
  // Invalid (out of range) base_value.
  return absl::nullopt;
}

// If returns `absl::nullopt`, will output an error to `error_out`.
// Modified from worklet_utils::ConvertBigIntToUint128().
absl::optional<auction_worklet::mojom::OffsetPtr> ConvertBigIntToOffset(
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

  return auction_worklet::mojom::Offset::New(
      absl::MakeUint128(words[1], words[0]),
      /*is_negative=*/sign_bit);
}

absl::optional<auction_worklet::mojom::SignalBucketOrValuePtr>
GetSignalBucketOrValue(v8::Isolate* isolate,
                       v8::Local<v8::Value> input,
                       bool is_bucket) {
  DCHECK(input->IsObject());
  gin::Dictionary result_dict(isolate, input.As<v8::Object>());

  std::string base_value_string;
  if (!result_dict.Get("base_value", &base_value_string))
    return absl::nullopt;

  absl::optional<auction_worklet::mojom::BaseValue> base_value_opt =
      BaseValueStringToEnum(base_value_string);
  if (!base_value_opt.has_value())
    return absl::nullopt;

  double scale = 0.0;
  v8::Local<v8::Value> js_scale;
  bool has_scale = false;
  if (result_dict.Get("scale", &js_scale) && !js_scale.IsEmpty() &&
      !js_scale->IsNullOrUndefined()) {
    if (!js_scale->IsNumber())
      return absl::nullopt;

    scale = js_scale.As<v8::Number>()->Value();
    has_scale = true;
  }

  v8::Local<v8::Value> js_offset;
  if (!result_dict.Get("offset", &js_offset)) {
    return auction_worklet::mojom::SignalBucketOrValue::New(
        base_value_opt.value(), scale, has_scale,
        /*offset=*/nullptr);
  }

  auction_worklet::mojom::OffsetPtr offset;
  // Offset has to be BigInt for bucket, and int for value.
  if (is_bucket && js_offset->IsBigInt()) {
    std::string error;

    absl::optional<auction_worklet::mojom::OffsetPtr> maybe_offset =
        ConvertBigIntToOffset(js_offset.As<v8::BigInt>(), &error);
    if (!maybe_offset.has_value())
      return nullptr;

    offset = std::move(maybe_offset.value());
  } else if (!is_bucket && js_offset->IsInt32()) {
    // Convert it to int128 as well to allow value dictionary share the same
    // mojo type with bucket for simplicity. It will be parsed back to int
    // when used.
    int value_offset = js_offset.As<v8::Int32>()->Value();
    offset =
        auction_worklet::mojom::Offset::New(std::abs(value_offset),
                                            /*is_negative=*/value_offset < 0);
  } else {
    return absl::nullopt;
  }

  return auction_worklet::mojom::SignalBucketOrValue::New(
      base_value_opt.value(), scale, has_scale, std::move(offset));
}

auction_worklet::mojom::AggregatableReportForEventContributionPtr
ParseForEventContribution(v8::Isolate* isolate,
                          v8::Local<v8::Value> arg,
                          std::string* error) {
  gin::Dictionary dict(isolate);
  bool success = gin::ConvertFromV8(isolate, arg, &dict);
  DCHECK(success);

  v8::Local<v8::Value> js_bucket;
  v8::Local<v8::Value> js_value;

  if (!dict.Get("bucket", &js_bucket) || js_bucket.IsEmpty() ||
      js_bucket->IsNullOrUndefined()) {
    *error =
        "Invalid or missing bucket in reportContributionForEvent's argument";
    return nullptr;
  }

  if (!dict.Get("value", &js_value) || js_value.IsEmpty() ||
      js_value->IsNullOrUndefined()) {
    *error =
        "Invalid or missing value in reportContributionForEvent's argument";
    return nullptr;
  }

  auction_worklet::mojom::ForEventSignalBucketPtr bucket;
  auction_worklet::mojom::ForEventSignalValuePtr value;

  if (js_bucket->IsBigInt()) {
    std::string bucket_error;
    absl::optional<absl::uint128> maybe_bucket =
        worklet_utils::ConvertBigIntToUint128(js_bucket.As<v8::BigInt>(),
                                              &bucket_error);
    if (!maybe_bucket.has_value()) {
      DCHECK(base::IsStringUTF8(bucket_error));
      *error = bucket_error;
      return nullptr;
    }
    bucket = auction_worklet::mojom::ForEventSignalBucket::NewIdBucket(
        maybe_bucket.value());
  } else if (js_bucket->IsObject()) {
    absl::optional<auction_worklet::mojom::SignalBucketOrValuePtr>
        maybe_signal_bucket_ptr =
            GetSignalBucketOrValue(isolate, js_bucket, /*is_bucket=*/true);
    if (!maybe_signal_bucket_ptr.has_value()) {
      *error = "Invalid bucket dictionary";
      return nullptr;
    }
    bucket = auction_worklet::mojom::ForEventSignalBucket::NewSignalBucket(
        std::move(maybe_signal_bucket_ptr.value()));
  } else {
    *error = "Bucket must be a BigInt or a dictionary";
    return nullptr;
  }

  if (js_value->IsInt32()) {
    int int_value = js_value.As<v8::Int32>()->Value();
    value = auction_worklet::mojom::ForEventSignalValue::NewIntValue(int_value);
    if (int_value < 0) {
      *error = "Value must be non-negative";
      return nullptr;
    }
  } else if (js_value->IsObject()) {
    absl::optional<auction_worklet::mojom::SignalBucketOrValuePtr>
        maybe_signal_value_ptr =
            GetSignalBucketOrValue(isolate, js_value, /*is_bucket=*/false);
    if (!maybe_signal_value_ptr.has_value()) {
      *error = "Invalid value dictionary";
      return nullptr;
    }
    value = auction_worklet::mojom::ForEventSignalValue::NewSignalValue(
        std::move(maybe_signal_value_ptr.value()));
  } else if (js_value->IsBigInt()) {
    *error = "Value cannot be a BigInt";
    return nullptr;
  } else {
    *error = "Value must be an integer or a dictionary";
    return nullptr;
  }

  return auction_worklet::mojom::AggregatableReportForEventContribution::New(
      std::move(bucket), std::move(value));
}

}  // namespace

PrivateAggregationBindings::PrivateAggregationBindings(
    AuctionV8Helper* v8_helper)
    : v8_helper_(v8_helper) {}

PrivateAggregationBindings::~PrivateAggregationBindings() = default;

void PrivateAggregationBindings::FillInGlobalTemplate(
    v8::Local<v8::ObjectTemplate> global_template) {
  if (!base::FeatureList::IsEnabled(content::kPrivateAggregationApi) ||
      !content::kPrivateAggregationApiEnabledInFledge.Get()) {
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

  v8::Local<v8::FunctionTemplate> report_contribution_for_event_template =
      v8::FunctionTemplate::New(
          v8_helper_->isolate(),
          &PrivateAggregationBindings::ReportContributionForEvent, v8_this);
  report_contribution_for_event_template->RemovePrototype();
  private_aggregation_template->Set(
      v8_helper_->CreateStringFromLiteral("reportContributionForEvent"),
      report_contribution_for_event_template);

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
  private_aggregation_for_event_win_contributions_.clear();
  private_aggregation_for_event_loss_contributions_.clear();
  debug_mode_details_.is_enabled = false;
  debug_mode_details_.debug_key = nullptr;
}

std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
PrivateAggregationBindings::TakePrivateAggregationRequests() {
  std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr> requests;

  requests.reserve(private_aggregation_contributions_.size());
  base::ranges::transform(
      private_aggregation_contributions_, std::back_inserter(requests),
      [this](content::mojom::AggregatableReportHistogramContributionPtr&
                 contribution) {
        return auction_worklet::mojom::PrivateAggregationRequest::New(
            std::move(contribution),
            // TODO(alexmt): consider allowing this to be set
            content::mojom::AggregationServiceMode::kDefault,
            debug_mode_details_.Clone());
      });
  private_aggregation_contributions_.clear();

  return requests;
}

std::vector<auction_worklet::mojom::PrivateAggregationForEventRequestPtr>
PrivateAggregationBindings::TakePrivateAggregationForEventRequests(
    const std::string& event_type) {
  if (event_type == "reserved.win") {
    return PrivateAggregationRequestsFromContribution(
        std::move(private_aggregation_for_event_win_contributions_));
  } else if (event_type == "reserved.loss") {
    return PrivateAggregationRequestsFromContribution(
        std::move(private_aggregation_for_event_loss_contributions_));
  } else {
    // Todo(qingxinwu): Support other event types (maybe arbitrary), such as
    // "click".
    NOTREACHED();
    return {};
  }
}

void PrivateAggregationBindings::SendHistogramReport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  PrivateAggregationBindings* bindings =
      static_cast<PrivateAggregationBindings*>(
          v8::External::Cast(*args.Data())->Value());

  content::mojom::AggregatableReportHistogramContributionPtr contribution =
      worklet_utils::ParseSendHistogramReportArguments(gin::Arguments(args));
  if (contribution.is_null()) {
    // Indicates an exception was thrown.
    return;
  }

  bindings->private_aggregation_contributions_.push_back(
      std::move(contribution));
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

  std::string error;
  auction_worklet::mojom::AggregatableReportForEventContributionPtr
      contribution = ParseForEventContribution(isolate, args[1], &error);

  if (contribution.is_null()) {
    DCHECK(base::IsStringUTF8(error));
    isolate->ThrowException(v8::Exception::TypeError(
        v8_helper->CreateUtf8String(error).ToLocalChecked()));
    return;
  }

  // TODO(qingxinwu): Consider throwing an error if `event_type` has "reserved."
  // prefix, but is not recognized as one of the reserved event types.
  if (event_type == "reserved.win") {
    bindings->private_aggregation_for_event_win_contributions_.push_back(
        std::move(contribution));
  } else if (event_type == "reserved.loss") {
    bindings->private_aggregation_for_event_loss_contributions_.push_back(
        std::move(contribution));
  }
}

void PrivateAggregationBindings::EnableDebugMode(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  PrivateAggregationBindings* bindings =
      static_cast<PrivateAggregationBindings*>(
          v8::External::Cast(*args.Data())->Value());

  worklet_utils::ParseAndApplyEnableDebugModeArguments(
      gin::Arguments(args), bindings->debug_mode_details_);
}

std::vector<auction_worklet::mojom::PrivateAggregationForEventRequestPtr>
PrivateAggregationBindings::PrivateAggregationRequestsFromContribution(
    std::vector<
        auction_worklet::mojom::AggregatableReportForEventContributionPtr>
        contributions) {
  std::vector<auction_worklet::mojom::PrivateAggregationForEventRequestPtr>
      requests;
  requests.reserve(contributions.size());
  base::ranges::transform(
      contributions, std::back_inserter(requests),
      [this](auction_worklet::mojom::AggregatableReportForEventContributionPtr&
                 contribution) {
        return auction_worklet::mojom::PrivateAggregationForEventRequest::New(
            std::move(contribution),
            // TODO(alexmt): consider allowing this to be set
            content::mojom::AggregationServiceMode::kDefault,
            debug_mode_details_.Clone());
      });
  contributions.clear();

  return requests;
}

}  // namespace auction_worklet
