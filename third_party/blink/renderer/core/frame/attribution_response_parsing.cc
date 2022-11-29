// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/attribution_response_parsing.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/filters.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-blink.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink::attribution_response_parsing {

namespace {

bool ParseAttributionAggregationKey(const JSONValue* value,
                                    absl::uint128* out) {
  if (!value)
    return false;

  String key_piece;
  if (!value->AsString(&key_piece))
    return false;

  // Final keys will be restricted to a maximum of 128 bits and the hex strings
  // should be limited to at most 32 digits.
  if (key_piece.length() < 3 || key_piece.length() > 34 ||
      !key_piece.StartsWith("0x", kTextCaseASCIIInsensitive)) {
    return false;
  }

  for (wtf_size_t i = 2; i < key_piece.length(); ++i) {
    if (!IsASCIIHexDigit(key_piece[i]))
      return false;
  }

  uint64_t low_bits;
  uint64_t high_bits;
  bool ok = false;

  // The rightmost 16 digits are low bits, and the rest are high bits.
  if (key_piece.length() <= 18) {
    low_bits = key_piece.Substring(2).HexToUInt64Strict(&ok);
    if (!ok)
      return false;
    high_bits = 0;
  } else {
    low_bits = key_piece.Right(16).HexToUInt64Strict(&ok);
    if (!ok)
      return false;
    high_bits =
        key_piece.Substring(2, key_piece.length() - 18).HexToUInt64Strict(&ok);
    if (!ok)
      return false;
  }

  *out = absl::MakeUint128(high_bits, low_bits);
  return true;
}

}  // namespace

mojom::blink::AttributionFiltersPtr ParseFilters(const JSONValue* value) {
  auto filters = mojom::blink::AttributionFilters::New();

  if (!value)
    return filters;

  const JSONObject* object = JSONObject::Cast(value);
  if (!object)
    return nullptr;

  const wtf_size_t num_filters = object->size();
  if (num_filters > attribution_reporting::kMaxFiltersPerSource)
    return nullptr;

  attribution_reporting::RecordFiltersPerFilterData(num_filters);

  for (wtf_size_t i = 0; i < num_filters; ++i) {
    const JSONObject::Entry entry = object->at(i);

    if (entry.first.CharactersSizeInBytes() >
        attribution_reporting::kMaxBytesPerFilterString) {
      return nullptr;
    }

    const JSONArray* array = JSONArray::Cast(entry.second);
    if (!array)
      return nullptr;

    const wtf_size_t num_values = array->size();
    if (num_values > attribution_reporting::kMaxValuesPerFilter)
      return nullptr;

    attribution_reporting::RecordValuesPerFilter(num_values);

    WTF::Vector<String> values;

    for (wtf_size_t j = 0; j < num_values; ++j) {
      String value_str;
      if (!array->at(j)->AsString(&value_str))
        return nullptr;

      if (value_str.CharactersSizeInBytes() >
          attribution_reporting::kMaxBytesPerFilterString) {
        return nullptr;
      }

      values.push_back(std::move(value_str));
    }

    filters->filter_values.insert(entry.first, std::move(values));
  }

  return filters;
}

absl::optional<uint64_t> ParseUint64(const String& string) {
  bool is_valid = false;
  uint64_t value = string.ToUInt64Strict(&is_valid);
  if (is_valid)
    return value;
  return absl::nullopt;
}

bool ParseEventTriggerData(
    const JSONValue* json,
    WTF::Vector<mojom::blink::EventTriggerDataPtr>& event_trigger_data) {
  if (!json)
    return true;

  // TODO(apaseltiner): Log a devtools issues on individual early exits below.

  const JSONArray* array_value = JSONArray::Cast(json);
  if (!array_value)
    return false;

  // Do not proceed if too many event trigger data are specified.
  if (array_value->size() > attribution_reporting::kMaxEventTriggerData)
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

    // Treat invalid trigger data, priority and deduplication key as if they
    // were not set.

    if (String s; object_val->GetString("trigger_data", &s)) {
      bool valid = false;
      uint64_t trigger_data = s.ToUInt64Strict(&valid);
      if (valid)
        event_trigger->data = trigger_data;
    }

    if (String s; object_val->GetString("priority", &s)) {
      bool valid = false;
      int64_t priority = s.ToInt64Strict(&valid);
      if (valid)
        event_trigger->priority = priority;
    }

    if (String s; object_val->GetString("deduplication_key", &s))
      event_trigger->dedup_key = ParseUint64(s);

    event_trigger->filters = ParseFilters(object_val->Get("filters"));
    if (!event_trigger->filters)
      return false;

    event_trigger->not_filters = ParseFilters(object_val->Get("not_filters"));
    if (!event_trigger->not_filters)
      return false;

    event_trigger_data.push_back(std::move(event_trigger));
  }

  return true;
}

bool ParseAttributionAggregatableTriggerData(
    const JSONValue* json,
    WTF::Vector<mojom::blink::AttributionAggregatableTriggerDataPtr>&
        trigger_data) {
  if (!json)
    return true;

  const int kExclusiveMaxHistogramValue = 101;

  static_assert(attribution_reporting::kMaxAggregatableTriggerDataPerTrigger <
                    kExclusiveMaxHistogramValue,
                "Bump the version for histogram "
                "Conversions.AggregatableTriggerDataLength");

  const auto* array = JSONArray::Cast(json);
  if (!array)
    return false;

  const wtf_size_t num_trigger_data = array->size();
  if (num_trigger_data >
      attribution_reporting::kMaxAggregatableTriggerDataPerTrigger) {
    return false;
  }

  base::UmaHistogramCounts100("Conversions.AggregatableTriggerDataLength",
                              num_trigger_data);

  trigger_data.ReserveInitialCapacity(num_trigger_data);

  for (wtf_size_t i = 0; i < num_trigger_data; ++i) {
    JSONValue* value = array->at(i);
    DCHECK(value);

    const auto* object = JSONObject::Cast(value);
    if (!object)
      return false;

    auto data = mojom::blink::AttributionAggregatableTriggerData::New();

    if (!ParseAttributionAggregationKey(object->Get("key_piece"),
                                        &data->key_piece)) {
      return false;
    }

    JSONArray* source_keys_val = object->GetArray("source_keys");
    if (!source_keys_val ||
        source_keys_val->size() >
            attribution_reporting::kMaxAggregationKeysPerSourceOrTrigger) {
      return false;
    }

    const wtf_size_t num_source_keys = source_keys_val->size();
    data->source_keys.ReserveInitialCapacity(num_source_keys);

    for (wtf_size_t j = 0; j < num_source_keys; ++j) {
      JSONValue* source_key_val = source_keys_val->at(j);
      DCHECK(source_key_val);

      String source_key;
      if (!source_key_val->AsString(&source_key) ||
          source_key.CharactersSizeInBytes() >
              attribution_reporting::kMaxBytesPerAggregationKeyId) {
        return false;
      }
      data->source_keys.push_back(std::move(source_key));
    }

    data->filters = ParseFilters(object->Get("filters"));
    if (!data->filters)
      return false;

    data->not_filters = ParseFilters(object->Get("not_filters"));
    if (!data->not_filters)
      return false;

    trigger_data.push_back(std::move(data));
  }

  return true;
}

bool ParseAttributionAggregatableValues(
    const JSONValue* json,
    WTF::HashMap<String, uint32_t>& values) {
  if (!json)
    return true;

  const auto* object = JSONObject::Cast(json);
  if (!object ||
      object->size() >
          attribution_reporting::kMaxAggregationKeysPerSourceOrTrigger) {
    return false;
  }

  const wtf_size_t num_values = object->size();
  values.ReserveCapacityForSize(num_values);

  for (wtf_size_t i = 0; i < num_values; ++i) {
    JSONObject::Entry entry = object->at(i);
    String key_id = entry.first;
    JSONValue* value = entry.second;
    DCHECK(value);

    if (key_id.CharactersSizeInBytes() >
        attribution_reporting::kMaxBytesPerAggregationKeyId) {
      return false;
    }

    int key_value;
    if (!value->AsInteger(&key_value) || key_value <= 0 ||
        key_value > attribution_reporting::kMaxAggregatableValue) {
      return false;
    }

    values.insert(std::move(key_id), key_value);
  }

  return true;
}

bool ParseTriggerRegistrationHeader(
    const String& json_string,
    mojom::blink::AttributionTriggerData& trigger_data) {
  std::unique_ptr<JSONValue> json = ParseJSON(json_string);
  if (!json)
    return false;

  const JSONObject* object = JSONObject::Cast(json.get());
  if (!object)
    return false;

  // Populate event triggers.
  if (!ParseEventTriggerData(object->Get("event_trigger_data"),
                             trigger_data.event_triggers)) {
    return false;
  }

  trigger_data.filters = ParseFilters(object->Get("filters"));
  if (!trigger_data.filters)
    return false;

  trigger_data.not_filters = ParseFilters(object->Get("not_filters"));
  if (!trigger_data.not_filters)
    return false;

  if (!ParseAttributionAggregatableTriggerData(
          object->Get("aggregatable_trigger_data"),
          trigger_data.aggregatable_trigger_data)) {
    return false;
  }

  if (!ParseAttributionAggregatableValues(object->Get("aggregatable_values"),
                                          trigger_data.aggregatable_values)) {
    return false;
  }

  if (String s; object->GetString("debug_key", &s))
    trigger_data.debug_key = ParseUint64(s);

  if (String s; object->GetString("aggregatable_deduplication_key", &s))
    trigger_data.aggregatable_dedup_key = ParseUint64(s);

  if (bool debug_reporting;
      object->GetBoolean("debug_reporting", &debug_reporting)) {
    trigger_data.debug_reporting = debug_reporting;
  }

  return true;
}

}  // namespace blink::attribution_response_parsing
