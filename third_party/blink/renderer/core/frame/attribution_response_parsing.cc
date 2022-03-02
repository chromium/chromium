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

bool ParseEventTriggerData(
    const AtomicString& json_string,
    WTF::Vector<mojom::blink::EventTriggerDataPtr>& event_trigger_data) {
  // Populate attribution data from provided JSON.
  std::unique_ptr<JSONValue> json = ParseJSON(json_string);

  // TODO(johnidel): Log a devtools issues if JSON parsing fails and on
  // individual early exits below.
  if (!json)
    return false;

  JSONArray* array_value = JSONArray::Cast(json.get());
  if (!array_value)
    return false;

  // Do not proceed if too many event trigger data are specified.
  if (array_value->size() > kMaxAttributionEventTriggerData)
    return false;

  // Process each event trigger.
  for (wtf_size_t i = 0; i < array_value->size(); ++i) {
    JSONValue* value = array_value->at(i);
    DCHECK(value);

    const auto* object_val = JSONObject::Cast(value);
    if (!object_val)
      return false;

    mojom::blink::EventTriggerDataPtr event_trigger =
        mojom::blink::EventTriggerData::New();

    String trigger_data_string;
    // A valid header must declare data for each sub-item.
    if (!object_val->GetString("trigger_data", &trigger_data_string))
      return false;
    bool trigger_data_is_valid = false;
    uint64_t trigger_data_value =
        trigger_data_string.ToUInt64Strict(&trigger_data_is_valid);

    // Default invalid data values to 0 so a report will get sent.
    event_trigger->data = trigger_data_is_valid ? trigger_data_value : 0;

    // Treat invalid priority and deduplication key as if they were not set.
    String priority_string;
    if (object_val->GetString("priority", &priority_string)) {
      bool priority_is_valid = false;
      int64_t priority = priority_string.ToInt64Strict(&priority_is_valid);
      if (priority_is_valid)
        event_trigger->priority = priority;
    }

    // Treat invalid priority and deduplication_key as if they were not set.
    String dedup_key_string;
    if (object_val->GetString("deduplication_key", &dedup_key_string)) {
      bool dedup_key_is_valid = false;
      uint64_t dedup_key = dedup_key_string.ToUInt64Strict(&dedup_key_is_valid);
      if (dedup_key_is_valid) {
        event_trigger->dedup_key =
            mojom::blink::AttributionTriggerDedupKey::New(dedup_key);
      }
    }

    event_trigger_data.push_back(std::move(event_trigger));
  }

  return true;
}

}  // namespace blink::attribution_response_parsing
