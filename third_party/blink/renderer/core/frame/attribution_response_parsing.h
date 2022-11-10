// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ATTRIBUTION_RESPONSE_PARSING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ATTRIBUTION_RESPONSE_PARSING_H_

#include <stdint.h>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class JSONValue;

namespace attribution_response_parsing {

// Helper functions to parse response headers. See details in the explainer.
// https://github.com/WICG/attribution-reporting-api/blob/main/EVENT.md
// https://github.com/WICG/attribution-reporting-api/blob/main/AGGREGATE.md

// Example JSON schema:
// [{
//   "id": "campaignCounts",
//   "key_piece": "0x159"
// },
// {
//   "id": "geoValue",
//   "key_piece": "0x5"
// }]
//
// Returns `nullptr` on failure.
CORE_EXPORT mojom::blink::AttributionAggregationKeysPtr ParseAggregationKeys(
    const JSONValue* json);

// Parses a 64-bit unsigned integer encoded as a base-10
// string. Returns `absl::nullopt` on failure.
CORE_EXPORT absl::optional<uint64_t> ParseUint64(const String& string);

CORE_EXPORT bool ParseSourceRegistrationHeader(
    const String& json_string,
    mojom::blink::AttributionSourceData& source_data);

CORE_EXPORT bool ParseTriggerRegistrationHeader(
    const String& json_string,
    mojom::blink::AttributionTriggerData& trigger_data);

// Parses event trigger data header of the form:
//
// [{
//   "trigger_data": "5"
//   "priority": "10",
//   "deduplication_key": "456"
// }]
//
// Returns whether parsing was successful.
CORE_EXPORT bool ParseEventTriggerData(
    const JSONValue* json,
    WTF::Vector<mojom::blink::EventTriggerDataPtr>& event_trigger_data);

// Parses filter header of the form:
//
// {
//   "abc": [],
//   "xyz": ["123", "456"]
// }
//
// Returns whether parsing was successful.
CORE_EXPORT bool ParseFilterValues(
    const JSONValue* json,
    WTF::HashMap<String, WTF::Vector<String>>& filter_values);

// Example JSON schema:
// [{
//   "key_piece": "0x400",
//   "source_keys": ["campaignCounts"]
// },
// {
//   "key_piece": "0xA80",
//   "source_keys": ["geoValue"]
// }]
//
// Returns whether parsing was successful.
CORE_EXPORT bool ParseAttributionAggregatableTriggerData(
    const JSONValue* json,
    WTF::Vector<mojom::blink::AttributionAggregatableTriggerDataPtr>&
        trigger_data);

// Example JSON schema:
// {
//  "campaignCounts": 32768,
//  "geoValue": 1664
// }
//
// Returns whether parsing was successful.
CORE_EXPORT bool ParseAttributionAggregatableValues(
    const JSONValue* json,
    WTF::HashMap<String, uint32_t>& values);

}  // namespace attribution_response_parsing

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ATTRIBUTION_RESPONSE_PARSING_H_
