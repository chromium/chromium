// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ATTRIBUTION_RESPONSE_PARSING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ATTRIBUTION_RESPONSE_PARSING_H_

#include <stdint.h>

#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ResourceResponse;

namespace attribution_response_parsing {

// Helper functions to parse response headers. See details in the explainer.
// https://github.com/WICG/conversion-measurement-api/blob/main/EVENT.md
// https://github.com/WICG/conversion-measurement-api/blob/main/AGGREGATE.md

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
// Returns whether parsing was successful.
CORE_EXPORT bool ParseAttributionAggregatableSource(
    const AtomicString& json_string,
    mojom::blink::AttributionAggregatableSource& source);

// Parses a debug key, which is a 64-bit unsigned integer encoded as a base-10
// string. Returns `nullptr` on failure.
CORE_EXPORT mojom::blink::AttributionDebugKeyPtr ParseDebugKey(
    const String& string);

CORE_EXPORT bool ParseSourceRegistrationHeader(
    const AtomicString& json_string,
    mojom::blink::AttributionSourceData& source_data);

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
    const AtomicString& json_string,
    WTF::Vector<mojom::blink::EventTriggerDataPtr>& event_trigger_data);

// Parses filter header of the form:
//
// {
//   "abc": [],
//   "xyz": ["123", "456"]
// }
//
// Returns whether parsing was successful.
CORE_EXPORT bool ParseFilters(const String& json_string,
                              mojom::blink::AttributionFilterData& filter_data);

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
    const AtomicString& json_string,
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
    const AtomicString& json_string,
    WTF::HashMap<String, uint32_t>& values);

// Returns the attribution trigger data parsed from the response. Returns
// `nullptr` in case of error.
mojom::blink::AttributionTriggerDataPtr ParseAttributionTriggerData(
    const ResourceResponse& response);

}  // namespace attribution_response_parsing

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ATTRIBUTION_RESPONSE_PARSING_H_
