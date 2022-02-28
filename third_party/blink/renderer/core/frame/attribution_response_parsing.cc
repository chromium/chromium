// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/attribution_response_parsing.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "third_party/blink/public/common/attribution_reporting/constants.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-blink.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink::attribution_response_parsing {

namespace {

mojom::blink::AttributionAggregatableKeyPtr ParseAttributionAggregatableKey(
    const JSONObject* object) {
  String key_piece;
  if (!object->GetString("key_piece", &key_piece))
    return nullptr;

  // Final keys will be restricted to a maximum of 128 bits and the hex strings
  // should be limited to at most 32 digits.
  if (key_piece.length() < 3 || key_piece.length() > 34 ||
      !key_piece.StartsWith("0x", kTextCaseASCIIInsensitive)) {
    return nullptr;
  }

  for (wtf_size_t i = 2; i < key_piece.length(); ++i) {
    if (!IsASCIIHexDigit(key_piece[i]))
      return nullptr;
  }

  uint64_t low_bits;
  uint64_t high_bits;
  bool ok = false;

  // The rightmost 16 digits are low bits, and the rest are high bits.
  if (key_piece.length() <= 18) {
    low_bits = key_piece.Substring(2).HexToUInt64Strict(&ok);
    if (!ok)
      return nullptr;
    high_bits = 0;
  } else {
    low_bits = key_piece.Right(16).HexToUInt64Strict(&ok);
    if (!ok)
      return nullptr;
    high_bits =
        key_piece.Substring(2, key_piece.length() - 18).HexToUInt64Strict(&ok);
    if (!ok)
      return nullptr;
  }

  return mojom::blink::AttributionAggregatableKey::New(high_bits, low_bits);
}

}  // namespace

ResponseParseResult<mojom::blink::AttributionAggregatableSources>
ParseAttributionAggregatableSources(const AtomicString& json_string) {
  if (json_string.IsEmpty()) {
    return ResponseParseResult<mojom::blink::AttributionAggregatableSources>(
        ResponseParseStatus::kNotFound);
  }

  std::unique_ptr<JSONValue> json = ParseJSON(json_string);
  if (!json) {
    return ResponseParseResult<mojom::blink::AttributionAggregatableSources>(
        ResponseParseStatus::kParseError);
  }

  const auto* array = JSONArray::Cast(json.get());
  if (!array || array->size() > kMaxAttributionAggregatableKeysPerSource) {
    return ResponseParseResult<mojom::blink::AttributionAggregatableSources>(
        ResponseParseStatus::kInvalidFormat);
  }

  const wtf_size_t num_keys = array->size();

  auto sources = mojom::blink::AttributionAggregatableSources::New();
  sources->sources.ReserveCapacityForSize(num_keys);

  for (wtf_size_t i = 0; i < num_keys; ++i) {
    JSONValue* value = array->at(i);
    DCHECK(value);

    const auto* object = JSONObject::Cast(value);
    if (!object) {
      return ResponseParseResult<mojom::blink::AttributionAggregatableSources>(
          ResponseParseStatus::kInvalidFormat);
    }

    String key_id;
    if (!object->GetString("id", &key_id) ||
        key_id.CharactersSizeInBytes() >
            kMaxBytesPerAttributionAggregatableKeyId) {
      return ResponseParseResult<mojom::blink::AttributionAggregatableSources>(
          ResponseParseStatus::kInvalidFormat);
    }

    mojom::blink::AttributionAggregatableKeyPtr key =
        ParseAttributionAggregatableKey(object);
    if (!key) {
      return ResponseParseResult<mojom::blink::AttributionAggregatableSources>(
          ResponseParseStatus::kInvalidFormat);
    }

    sources->sources.insert(std::move(key_id), std::move(key));
  }

  return ResponseParseResult<mojom::blink::AttributionAggregatableSources>(
      ResponseParseStatus::kSuccess, std::move(sources));
}

}  // namespace blink::attribution_response_parsing
