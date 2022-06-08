// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/attribution_response_parsing.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/blink/public/common/attribution_reporting/constants.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-blink.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
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

bool ParseAttributionFilterData(
    const JSONValue* value,
    mojom::blink::AttributionFilterData& filter_data) {
  if (!value)
    return true;

  const JSONObject* object = JSONObject::Cast(value);
  if (!object)
    return false;

  const int kExclusiveMaxHistogramValue = 101;

  static_assert(kMaxValuesPerAttributionFilter < kExclusiveMaxHistogramValue,
                "Bump the version for histogram Conversions.ValuesPerFilter");

  static_assert(
      kMaxAttributionFiltersPerSource < kExclusiveMaxHistogramValue,
      "Bump the version for histogram Conversions.FiltersPerFilterData");

  const wtf_size_t num_filters = object->size();
  if (num_filters > kMaxAttributionFiltersPerSource)
    return false;

  // The metrics are called potentially many times while parsing an attribution
  // header, therefore using the macros to avoid the overhead of taking a lock
  // and performing a map lookup.
  UMA_HISTOGRAM_COUNTS_100("Conversions.FiltersPerFilterData", num_filters);

  for (wtf_size_t i = 0; i < num_filters; ++i) {
    const JSONObject::Entry entry = object->at(i);

    if (entry.first.CharactersSizeInBytes() >
        kMaxBytesPerAttributionFilterString) {
      return false;
    }

    const JSONArray* array = JSONArray::Cast(entry.second);
    if (!array)
      return false;

    const wtf_size_t num_values = array->size();
    if (num_values > kMaxValuesPerAttributionFilter)
      return false;

    UMA_HISTOGRAM_COUNTS_100("Conversions.ValuesPerFilter", num_values);

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

bool ParseAggregationKeys(
    const JSONValue* json,
    WTF::HashMap<String, absl::uint128>& aggregation_keys) {
  // Aggregation keys may be omitted.
  if (!json)
    return true;

  const int kExclusiveMaxHistogramValue = 101;

  static_assert(
      kMaxAttributionAggregationKeysPerSourceOrTrigger <
          kExclusiveMaxHistogramValue,
      "Bump the version for histogram Conversions.AggregatableKeysPerSource");

  const auto* object = JSONObject::Cast(json);
  if (!object)
    return false;

  const wtf_size_t num_keys = object->size();
  if (num_keys > kMaxAttributionAggregationKeysPerSourceOrTrigger)
    return false;

  base::UmaHistogramCounts100("Conversions.AggregatableKeysPerSource",
                              num_keys);

  aggregation_keys.ReserveCapacityForSize(num_keys);

  for (wtf_size_t i = 0; i < num_keys; ++i) {
    JSONObject::Entry entry = object->at(i);
    String key_id = entry.first;
    JSONValue* value = entry.second;
    DCHECK(value);

    if (key_id.CharactersSizeInBytes() >
        kMaxBytesPerAttributionAggregationKeyId) {
      return false;
    }

    absl::uint128 key;
    if (!ParseAttributionAggregationKey(value, &key))
      return false;

    aggregation_keys.insert(std::move(key_id), key);
  }

  return true;
}

mojom::blink::AttributionDebugKeyPtr ParseDebugKey(const String& string) {
  bool is_valid = false;
  uint64_t value = string.ToUInt64Strict(&is_valid);
  return is_valid ? mojom::blink::AttributionDebugKey::New(value) : nullptr;
}

bool ParseSourceRegistrationHeader(
    const String& json_string,
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
  if (object->GetString("debug_key", &debug_key_string))
    source_data.debug_key = ParseDebugKey(debug_key_string);

  source_data.filter_data = mojom::blink::AttributionFilterData::New();
  if (!ParseAttributionFilterData(object->Get("filter_data"),
                                  *source_data.filter_data)) {
    return false;
  }

  // "source_type" is automatically generated in source filter data during
  // attribution source matching, so it is an error to specify it here.
  // TODO(apaseltiner): Report a DevTools issue for this.
  if (source_data.filter_data->filter_values.Contains("source_type"))
    return false;

  if (!ParseAggregationKeys(object->Get("aggregation_keys"),
                            source_data.aggregation_keys)) {
    return false;
  }

  return true;
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

bool ParseAttributionAggregatableTriggerData(
    const JSONValue* json,
    WTF::Vector<mojom::blink::AttributionAggregatableTriggerDataPtr>&
        trigger_data) {
  if (!json)
    return true;

  const int kExclusiveMaxHistogramValue = 101;

  static_assert(kMaxAttributionAggregatableTriggerDataPerTrigger <
                    kExclusiveMaxHistogramValue,
                "Bump the version for histogram "
                "Conversions.AggregatableTriggerDataLength");

  const auto* array = JSONArray::Cast(json);
  if (!array)
    return false;

  const wtf_size_t num_trigger_data = array->size();
  if (num_trigger_data > kMaxAttributionAggregatableTriggerDataPerTrigger)
    return false;

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
            kMaxAttributionAggregationKeysPerSourceOrTrigger) {
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
              kMaxBytesPerAttributionAggregationKeyId) {
        return false;
      }
      data->source_keys.push_back(std::move(source_key));
    }

    data->filters = mojom::blink::AttributionFilterData::New();
    if (!ParseAttributionFilterData(object->Get("filters"), *data->filters)) {
      return false;
    }

    data->not_filters = mojom::blink::AttributionFilterData::New();
    if (!ParseAttributionFilterData(object->Get("not_filters"),
                                    *data->not_filters)) {
      return false;
    }

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
      object->size() > kMaxAttributionAggregationKeysPerSourceOrTrigger) {
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
        kMaxBytesPerAttributionAggregationKeyId) {
      return false;
    }

    int key_value;
    if (!value->AsInteger(&key_value) || key_value <= 0 ||
        key_value > kMaxAttributionAggregatableValue) {
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

  trigger_data.filters = mojom::blink::AttributionFilterData::New();

  if (!ParseAttributionFilterData(object->Get("filters"),
                                  *trigger_data.filters)) {
    return false;
  }

  if (!ParseAttributionAggregatableTriggerData(
          object->Get("aggregatable_trigger_data"),
          trigger_data.aggregatable_trigger_data)) {
    return false;
  }

  if (!ParseAttributionAggregatableValues(object->Get("aggregatable_values"),
                                          trigger_data.aggregatable_values)) {
    return false;
  }

  String debug_key_string;
  if (object->GetString("debug_key", &debug_key_string))
    trigger_data.debug_key = ParseDebugKey(debug_key_string);

  return true;
}

mojom::blink::AttributionTriggerDataPtr ParseAttributionTriggerData(
    const ResourceResponse& response) {
  auto trigger_data = mojom::blink::AttributionTriggerData::New();

  // Verify the current url is trustworthy and capable of registering triggers.
  scoped_refptr<const SecurityOrigin> reporting_origin =
      SecurityOrigin::Create(response.CurrentRequestUrl());
  if (!reporting_origin->IsPotentiallyTrustworthy())
    return nullptr;
  trigger_data->reporting_origin = std::move(reporting_origin);

  const AtomicString& trigger_json = response.HttpHeaderField(
      http_names::kAttributionReportingRegisterTrigger);
  if (!ParseTriggerRegistrationHeader(trigger_json, *trigger_data))
    return nullptr;

  return trigger_data;
}

}  // namespace blink::attribution_response_parsing
