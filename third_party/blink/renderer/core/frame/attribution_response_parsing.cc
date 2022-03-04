// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/attribution_response_parsing.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/attribution_reporting/constants.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-blink.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
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

bool ParseAttributionFilterData(
    JSONValue* value,
    mojom::blink::AttributionFilterData& filter_data) {
  if (!value)
    return true;

  JSONObject* object = JSONObject::Cast(value);
  if (!object)
    return false;

  const wtf_size_t num_filters = object->size();
  if (num_filters > kMaxAttributionFiltersPerSource)
    return false;

  for (wtf_size_t i = 0; i < num_filters; ++i) {
    JSONObject::Entry entry = object->at(i);

    if (entry.first.CharactersSizeInBytes() >
        kMaxBytesPerAttributionFilterString) {
      return false;
    }

    JSONArray* array = JSONArray::Cast(entry.second);
    if (!array)
      return false;

    const wtf_size_t num_values = array->size();
    if (num_values > kMaxValuesPerAttributionFilter)
      return false;

    WTF::Vector<String> values;

    for (wtf_size_t j = 0; j < num_values; ++j) {
      String value;
      if (!array->at(j)->AsString(&value))
        return false;

      if (value.CharactersSizeInBytes() > kMaxBytesPerAttributionFilterString)
        return false;

      values.push_back(std::move(value));
    }

    filter_data.filter_values.insert(entry.first, std::move(values));
  }

  return true;
}

}  // namespace

ResponseParseResult<mojom::blink::AttributionAggregatableSources>
ParseAttributionAggregatableSources(const AtomicString& json_string) {
  if (json_string.IsNull()) {
    return ResponseParseResult<mojom::blink::AttributionAggregatableSources>(
        ResponseParseStatus::kNotFound);
  }

  std::unique_ptr<JSONValue> json = ParseJSON(json_string);
  if (!json) {
    return ResponseParseResult<mojom::blink::AttributionAggregatableSources>(
        ResponseParseStatus::kParseError);
  }

  const auto* array = JSONArray::Cast(json.get());
  if (!array ||
      array->size() > kMaxAttributionAggregatableKeysPerSourceOrTrigger) {
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

bool ParseSourceRegistrationHeader(
    const AtomicString& json_string,
    mojom::blink::AttributionSourceData& source_data) {
  // TODO(apaseltiner): Consider applying a max stack depth to this.
  std::unique_ptr<JSONValue> json = ParseJSON(json_string);

  if (!json)
    return false;

  JSONObject* object = JSONObject::Cast(json.get());
  if (!object)
    return false;

  String event_id_string;
  if (!object->GetString("source_event_id", &event_id_string))
    return false;
  bool event_id_is_valid = false;
  uint64_t event_id = event_id_string.ToUInt64Strict(&event_id_is_valid);

  // For source registrations where there is no mechanism to raise an error,
  // such as on an img element, it is more useful to log the source with
  // default data so that a reporting origin can learn the failure mode.
  source_data.source_event_id = event_id_is_valid ? event_id : 0;

  String destination_string;
  if (!object->GetString("destination", &destination_string))
    return false;
  scoped_refptr<const SecurityOrigin> destination =
      SecurityOrigin::CreateFromString(destination_string);
  if (!destination->IsPotentiallyTrustworthy())
    return false;
  source_data.destination = std::move(destination);

  // Treat invalid expiry, priority, and debug key as if they were not set.
  String priority_string;
  if (object->GetString("priority", &priority_string)) {
    bool priority_is_valid = false;
    int64_t priority = priority_string.ToInt64Strict(&priority_is_valid);
    if (priority_is_valid)
      source_data.priority = priority;
  }

  String expiry_string;
  if (object->GetString("expiry", &expiry_string)) {
    bool expiry_is_valid = false;
    int64_t expiry = expiry_string.ToInt64Strict(&expiry_is_valid);
    if (expiry_is_valid)
      source_data.expiry = base::Seconds(expiry);
  }

  String debug_key_string;
  if (object->GetString("debug_key", &debug_key_string)) {
    bool debug_key_is_valid = false;
    uint64_t debug_key = debug_key_string.ToUInt64Strict(&debug_key_is_valid);
    if (debug_key_is_valid) {
      source_data.debug_key = mojom::blink::AttributionDebugKey::New(debug_key);
    }
  }

  source_data.filter_data = mojom::blink::AttributionFilterData::New();
  if (!ParseAttributionFilterData(object->Get("filter_data"),
                                  *source_data.filter_data)) {
    return false;
  }

  return true;
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

    event_trigger->filters = mojom::blink::AttributionFilterData::New();
    if (!ParseAttributionFilterData(object_val->Get("filters"),
                                    *event_trigger->filters)) {
      return false;
    }

    event_trigger->not_filters = mojom::blink::AttributionFilterData::New();
    if (!ParseAttributionFilterData(object_val->Get("not_filters"),
                                    *event_trigger->not_filters)) {
      return false;
    }

    event_trigger_data.push_back(std::move(event_trigger));
  }

  return true;
}

bool ParseFilters(const AtomicString& json_string,
                  mojom::blink::AttributionFilterData& filter_data) {
  // TODO(apaseltiner): Consider applying a max stack depth to this.
  std::unique_ptr<JSONValue> json = ParseJSON(json_string);
  if (!json)
    return false;

  return ParseAttributionFilterData(json.get(), filter_data);
}

ResponseParseResult<mojom::blink::AttributionAggregatableTrigger>
ParseAttributionAggregatableTrigger(const AtomicString& json_string) {
  if (json_string.IsNull()) {
    return ResponseParseResult<mojom::blink::AttributionAggregatableTrigger>(
        ResponseParseStatus::kNotFound);
  }

  std::unique_ptr<JSONValue> json = ParseJSON(json_string);
  if (!json) {
    return ResponseParseResult<mojom::blink::AttributionAggregatableTrigger>(
        ResponseParseStatus::kParseError);
  }

  const auto* array = JSONArray::Cast(json.get());
  if (!array ||
      array->size() > kMaxAttributionAggregatableTriggerDataPerTrigger) {
    return ResponseParseResult<mojom::blink::AttributionAggregatableTrigger>(
        ResponseParseStatus::kInvalidFormat);
  }

  const wtf_size_t num_trigger_data = array->size();

  auto trigger = mojom::blink::AttributionAggregatableTrigger::New();
  trigger->trigger_data.ReserveInitialCapacity(num_trigger_data);

  for (wtf_size_t i = 0; i < num_trigger_data; ++i) {
    JSONValue* value = array->at(i);
    DCHECK(value);

    const auto* object = JSONObject::Cast(value);
    if (!object) {
      return ResponseParseResult<mojom::blink::AttributionAggregatableTrigger>(
          ResponseParseStatus::kInvalidFormat);
    }

    auto trigger_data = mojom::blink::AttributionAggregatableTriggerData::New();

    trigger_data->key = ParseAttributionAggregatableKey(object);
    if (!trigger_data->key) {
      return ResponseParseResult<mojom::blink::AttributionAggregatableTrigger>(
          ResponseParseStatus::kInvalidFormat);
    }

    JSONArray* source_keys_val = object->GetArray("source_keys");
    if (!source_keys_val ||
        source_keys_val->size() >
            kMaxAttributionAggregatableKeysPerSourceOrTrigger) {
      return ResponseParseResult<mojom::blink::AttributionAggregatableTrigger>(
          ResponseParseStatus::kInvalidFormat);
    }

    const wtf_size_t num_sources_keys = source_keys_val->size();
    trigger_data->source_keys.ReserveInitialCapacity(num_sources_keys);

    for (wtf_size_t j = 0; j < num_sources_keys; ++j) {
      JSONValue* source_key_val = source_keys_val->at(j);
      DCHECK(source_key_val);

      String source_key;
      if (!source_key_val->AsString(&source_key) ||
          source_key.CharactersSizeInBytes() >
              kMaxBytesPerAttributionAggregatableKeyId) {
        return ResponseParseResult<
            mojom::blink::AttributionAggregatableTrigger>(
            ResponseParseStatus::kInvalidFormat);
      }
      trigger_data->source_keys.push_back(std::move(source_key));
    }

    trigger_data->filters = mojom::blink::AttributionFilterData::New();
    if (!ParseAttributionFilterData(object->Get("filters"),
                                    *trigger_data->filters)) {
      return ResponseParseResult<mojom::blink::AttributionAggregatableTrigger>(
          ResponseParseStatus::kInvalidFormat);
    }

    trigger_data->not_filters = mojom::blink::AttributionFilterData::New();
    if (!ParseAttributionFilterData(object->Get("not_filters"),
                                    *trigger_data->not_filters)) {
      return ResponseParseResult<mojom::blink::AttributionAggregatableTrigger>(
          ResponseParseStatus::kInvalidFormat);
    }

    trigger->trigger_data.push_back(std::move(trigger_data));
  }

  return ResponseParseResult<mojom::blink::AttributionAggregatableTrigger>(
      ResponseParseStatus::kSuccess, std::move(trigger));
}

ResponseParseResult<mojom::blink::AttributionAggregatableValues>
ParseAttributionAggregatableValues(const AtomicString& json_string) {
  if (json_string.IsNull()) {
    return ResponseParseResult<mojom::blink::AttributionAggregatableValues>(
        ResponseParseStatus::kNotFound);
  }

  std::unique_ptr<JSONValue> json = ParseJSON(json_string);
  if (!json) {
    return ResponseParseResult<mojom::blink::AttributionAggregatableValues>(
        ResponseParseStatus::kParseError);
  }

  const auto* object = JSONObject::Cast(json.get());
  if (!object ||
      object->size() > kMaxAttributionAggregatableKeysPerSourceOrTrigger) {
    return ResponseParseResult<mojom::blink::AttributionAggregatableValues>(
        ResponseParseStatus::kInvalidFormat);
  }

  const wtf_size_t num_values = object->size();

  auto values = mojom::blink::AttributionAggregatableValues::New();
  values->values.ReserveCapacityForSize(num_values);

  for (wtf_size_t i = 0; i < num_values; ++i) {
    JSONObject::Entry entry = object->at(i);
    String key_id = entry.first;
    JSONValue* value = entry.second;
    DCHECK(value);

    if (key_id.CharactersSizeInBytes() >
        kMaxBytesPerAttributionAggregatableKeyId) {
      return ResponseParseResult<mojom::blink::AttributionAggregatableValues>(
          ResponseParseStatus::kInvalidFormat);
    }

    int key_value;
    if (!value->AsInteger(&key_value) || key_value <= 0) {
      return ResponseParseResult<mojom::blink::AttributionAggregatableValues>(
          ResponseParseStatus::kInvalidFormat);
    }

    values->values.insert(std::move(key_id), key_value);
  }

  return ResponseParseResult<mojom::blink::AttributionAggregatableValues>(
      ResponseParseStatus::kSuccess, std::move(values));
}

}  // namespace blink::attribution_response_parsing
