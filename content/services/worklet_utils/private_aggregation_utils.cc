// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/worklet_utils/private_aggregation_utils.h"

#include <stdint.h>

#include <string>

#include "base/check.h"
#include "base/strings/string_util.h"
#include "content/common/aggregatable_report.mojom.h"
#include "content/common/private_aggregation_host.mojom.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-primitive.h"

namespace worklet_utils {

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

  *error_out = "debug_key must be a BigInt";
  return absl::nullopt;
}

}  // namespace

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

content::mojom::AggregatableReportHistogramContributionPtr
ParseSendHistogramReportArguments(const gin::Arguments& args) {
  v8::Isolate* isolate = args.isolate();
  std::vector<v8::Local<v8::Value>> argument_list = args.GetAll();

  // Any additional arguments are ignored.
  if (argument_list.size() == 0 || argument_list[0].IsEmpty() ||
      !argument_list[0]->IsObject()) {
    isolate->ThrowException(v8::Exception::TypeError(CreateStringFromLiteral(
        isolate, "sendHistogramReport requires 1 object parameter")));
    return nullptr;
  }

  gin::Dictionary dict(isolate);

  bool success = gin::ConvertFromV8(isolate, argument_list[0], &dict);
  DCHECK(success);

  v8::Local<v8::Value> js_bucket;
  v8::Local<v8::Value> js_value;

  if (!dict.Get("bucket", &js_bucket)) {
    // Propagate any exception
    return nullptr;
  }
  if (js_bucket.IsEmpty() || js_bucket->IsNullOrUndefined()) {
    isolate->ThrowException(v8::Exception::TypeError(CreateStringFromLiteral(
        isolate, "Invalid or missing bucket in sendHistogramReport argument")));
    return nullptr;
  }

  if (!dict.Get("value", &js_value)) {
    // Propagate any exception
    return nullptr;
  }
  if (js_value.IsEmpty() || js_value->IsNullOrUndefined()) {
    isolate->ThrowException(v8::Exception::TypeError(CreateStringFromLiteral(
        isolate, "Invalid or missing value in sendHistogramReport argument")));
    return nullptr;
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
      return nullptr;
    }
    bucket = maybe_bucket.value();
  } else {
    isolate->ThrowException(v8::Exception::TypeError(
        CreateStringFromLiteral(isolate, "bucket must be a BigInt")));
    return nullptr;
  }

  if (js_value->IsInt32()) {
    value = js_value.As<v8::Int32>()->Value();
  } else if (js_value->IsBigInt()) {
    isolate->ThrowException(v8::Exception::TypeError(
        CreateStringFromLiteral(isolate, "Value cannot be a BigInt")));
    return nullptr;
  } else {
    isolate->ThrowException(v8::Exception::TypeError(
        CreateStringFromLiteral(isolate, "Value must be an integer Number")));
    return nullptr;
  }

  if (value < 0) {
    isolate->ThrowException(v8::Exception::TypeError(
        CreateStringFromLiteral(isolate, "Value must be non-negative")));
    return nullptr;
  }

  return content::mojom::AggregatableReportHistogramContribution::New(bucket,
                                                                      value);
}

void ParseAndApplyEnableDebugModeArguments(
    const gin::Arguments& args,
    content::mojom::DebugModeDetails& debug_mode_details) {
  v8::Isolate* isolate = args.isolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  std::vector<v8::Local<v8::Value>> argument_list = args.GetAll();

  if (debug_mode_details.is_enabled) {
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
    if (!dict.Get("debug_key", &js_debug_key)) {
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

    debug_mode_details.debug_key =
        content::mojom::DebugKey::New(maybe_debug_key.value());
  }

  debug_mode_details.is_enabled = true;
}

}  // namespace worklet_utils
