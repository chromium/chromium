// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/shared_storage_worklet/private_aggregation.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "content/common/aggregatable_report.mojom.h"
#include "content/common/private_aggregation_host.mojom.h"
#include "content/services/shared_storage_worklet/worklet_v8_helper.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-template.h"

namespace shared_storage_worklet {

// TODO(crbug.com/1351757): The argument parsing logic in this file should be
// factored out into a joint utils file and shared with the FLEDGE worklet.

namespace {

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
  if (!base::IsStringUTF8(utf8_string))
    return v8::MaybeLocal<v8::String>();
  return v8::String::NewFromUtf8(isolate, utf8_string.data(),
                                 v8::NewStringType::kNormal,
                                 utf8_string.length());
}

// If returns `absl::nullopt`, will output an error to `error_out`.
absl::optional<absl::uint128> ConvertBigIntToUint128(
    v8::MaybeLocal<v8::BigInt> maybe_bigint,
    std::string* error_out) {
  DCHECK(error_out);

  if (maybe_bigint.IsEmpty()) {
    *error_out = "Failed to interpret as BigInt";
    return absl::nullopt;
  }

  v8::Local<v8::BigInt> local_bigint = maybe_bigint.ToLocalChecked();
  if (local_bigint.IsEmpty()) {
    *error_out = "Failed to interpret as BigInt";
    return absl::nullopt;
  }
  if (local_bigint->WordCount() > 2) {
    *error_out = "BigInt is too large";
    return absl::nullopt;
  }
  int sign_bit;
  int word_count = 2;
  uint64_t words[2];  // Least significant to most significant.
  local_bigint->ToWordsArray(&sign_bit, &word_count, words);
  if (sign_bit) {
    *error_out = "BigInt must be non-negative";
    return absl::nullopt;
  }
  if (word_count == 0) {
    words[0] = 0;
  }
  if (word_count <= 1) {
    words[1] = 0;
  }

  return absl::MakeUint128(words[1], words[0]);
}

// In case of failure, will return `absl::nullopt` and output an error to
// `error_out`.
absl::optional<uint64_t> ParseDebugKey(gin::Dictionary dict,
                                       v8::Local<v8::Context>& context,
                                       std::string* error_out) {
  v8::Local<v8::Value> js_debug_key;

  if (!dict.Get("debug_key", &js_debug_key) || js_debug_key.IsEmpty() ||
      js_debug_key->IsNullOrUndefined()) {
    return absl::nullopt;
  }

  if (js_debug_key->IsUint32()) {
    v8::Maybe<uint32_t> maybe_debug_key = js_debug_key->Uint32Value(context);
    if (maybe_debug_key.IsNothing()) {
      *error_out = "Failed to interpret value as integer";
    }
    return maybe_debug_key.ToChecked();
  }

  if (js_debug_key->IsBigInt()) {
    absl::optional<absl::uint128> maybe_debug_key =
        ConvertBigIntToUint128(js_debug_key->ToBigInt(context), error_out);
    if (!maybe_debug_key.has_value()) {
      return absl::nullopt;
    }
    if (absl::Uint128High64(maybe_debug_key.value()) != 0) {
      *error_out = "BigInt is too large";
      return absl::nullopt;
    }
    return absl::Uint128Low64(maybe_debug_key.value());
  }

  *error_out =
      "debug_key must be either a non-negative integer Number or BigInt";
  return absl::nullopt;
}

}  // namespace

PrivateAggregation::PrivateAggregation(
    mojom::SharedStorageWorkletServiceClient& client,
    content::mojom::PrivateAggregationHost& private_aggregation_host)
    : client_(client), private_aggregation_host_(private_aggregation_host) {}

PrivateAggregation::~PrivateAggregation() = default;

gin::WrapperInfo PrivateAggregation::kWrapperInfo = {gin::kEmbedderNativeGin};

gin::ObjectTemplateBuilder PrivateAggregation::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return Wrappable<PrivateAggregation>::GetObjectTemplateBuilder(isolate)
      .SetMethod("sendHistogramReport",
                 &PrivateAggregation::SendHistogramReport)
      .SetMethod("enableDebugMode", &PrivateAggregation::EnableDebugMode);
}

const char* PrivateAggregation::GetTypeName() {
  return "PrivateAggregation";
}

void PrivateAggregation::SendHistogramReport(gin::Arguments* args) {
  EnsureUseCountersAreRecorded();

  v8::Isolate* isolate = args->isolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  std::vector<v8::Local<v8::Value>> argument_list = args->GetAll();

  // Any additional arguments are ignored.
  if (argument_list.size() == 0 || argument_list[0].IsEmpty() ||
      !argument_list[0]->IsObject()) {
    isolate->ThrowException(v8::Exception::TypeError(CreateStringFromLiteral(
        isolate, "sendHistogramReport requires 1 object parameter")));
    return;
  }

  gin::Dictionary dict(isolate);

  if (!gin::ConvertFromV8(isolate, argument_list[0], &dict)) {
    isolate->ThrowException(v8::Exception::TypeError(CreateStringFromLiteral(
        isolate, "Invalid argument in sendHistogramReport")));
    return;
  }

  v8::Local<v8::Value> js_bucket;
  v8::Local<v8::Value> js_value;

  if (!dict.Get("bucket", &js_bucket) || js_bucket.IsEmpty() ||
      js_bucket->IsNullOrUndefined()) {
    isolate->ThrowException(v8::Exception::TypeError(CreateStringFromLiteral(
        isolate, "Invalid or missing bucket in sendHistogramReport argument")));
    return;
  }

  if (!dict.Get("value", &js_value) || js_value.IsEmpty() ||
      js_value->IsNullOrUndefined()) {
    isolate->ThrowException(v8::Exception::TypeError(CreateStringFromLiteral(
        isolate, "Invalid or missing value in sendHistogramReport argument")));
    return;
  }

  absl::uint128 bucket;
  int value;

  if (js_bucket->IsUint32()) {
    v8::Maybe<uint32_t> maybe_bucket = js_bucket->Uint32Value(context);
    if (maybe_bucket.IsNothing()) {
      isolate->ThrowException(v8::Exception::TypeError(CreateStringFromLiteral(
          isolate, "Failed to interpret value as integer")));
      return;
    }
    bucket = maybe_bucket.ToChecked();
  } else if (js_bucket->IsBigInt()) {
    std::string error;
    absl::optional<absl::uint128> maybe_bucket =
        ConvertBigIntToUint128(js_bucket->ToBigInt(context), &error);
    if (!maybe_bucket.has_value()) {
      DCHECK(base::IsStringUTF8(error));
      isolate->ThrowException(v8::Exception::TypeError(
          CreateUtf8String(isolate, error).ToLocalChecked()));
      return;
    }
    bucket = maybe_bucket.value();
  } else {
    isolate->ThrowException(v8::Exception::TypeError(CreateStringFromLiteral(
        isolate, "Bucket must be either an integer Number or BigInt")));
    return;
  }

  if (js_value->IsInt32()) {
    v8::Maybe<int32_t> maybe_value = js_value->Int32Value(context);
    if (maybe_value.IsNothing()) {
      isolate->ThrowException(v8::Exception::TypeError(CreateStringFromLiteral(
          isolate, "Failed to interpret value as integer")));
      return;
    }
    value = maybe_value.ToChecked();
  } else if (js_value->IsBigInt()) {
    isolate->ThrowException(v8::Exception::TypeError(
        CreateStringFromLiteral(isolate, "Value cannot be a BigInt")));
    return;
  } else {
    isolate->ThrowException(v8::Exception::TypeError(
        CreateStringFromLiteral(isolate, "Value must be an integer Number")));
    return;
  }

  if (value < 0) {
    isolate->ThrowException(v8::Exception::TypeError(
        CreateStringFromLiteral(isolate, "Value must be non-negative")));
    return;
  }

  std::vector<content::mojom::AggregatableReportHistogramContributionPtr>
      contributions;
  contributions.push_back(
      content::mojom::AggregatableReportHistogramContribution::New(bucket,
                                                                   value));

  private_aggregation_host_->SendHistogramReport(
      std::move(contributions),
      // TODO(alexmt): consider allowing this to be set
      content::mojom::AggregationServiceMode::kDefault,
      debug_mode_details_.Clone());
}

void PrivateAggregation::EnableDebugMode(gin::Arguments* args) {
  EnsureUseCountersAreRecorded();

  v8::Isolate* isolate = args->isolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  std::vector<v8::Local<v8::Value>> argument_list = args->GetAll();

  if (debug_mode_details_.is_enabled) {
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

    std::string error;
    absl::optional<uint64_t> maybe_debug_key =
        ParseDebugKey(dict, context, &error);
    if (!maybe_debug_key.has_value()) {
      DCHECK(base::IsStringUTF8(error));
      isolate->ThrowException(v8::Exception::TypeError(
          CreateUtf8String(isolate, error).ToLocalChecked()));
      return;
    }

    debug_mode_details_.debug_key =
        content::mojom::DebugKey::New(maybe_debug_key.value());
  }

  debug_mode_details_.is_enabled = true;
}

void PrivateAggregation::EnsureUseCountersAreRecorded() {
  if (!has_recorded_use_counters_) {
    has_recorded_use_counters_ = true;
    client_->RecordUseCounters(
        {blink::mojom::WebFeature::kPrivateAggregationApiAll,
         blink::mojom::WebFeature::kPrivateAggregationApiSharedStorage});
  }
}

}  // namespace shared_storage_worklet
