// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ATTRIBUTION_RESPONSE_PARSING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ATTRIBUTION_RESPONSE_PARSING_H_

#include <utility>
#include <vector>

#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink::attribution_response_parsing {

// TODO(crbug.com/1285319): Add metrics for response parsing.
enum class ResponseParseStatus {
  kSuccess = 0,
  kNotFound = 1,
  kParseError = 2,
  kInvalidFormat = 3,
};

template <class T>
struct ResponseParseResult {
  explicit ResponseParseResult(ResponseParseStatus status,
                               mojo::StructPtr<T> value = T::New())
      : status(status), value(std::move(value)) {}

  ResponseParseStatus status;
  mojo::StructPtr<T> value;
};

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
CORE_EXPORT ResponseParseResult<mojom::blink::AttributionAggregatableSources>
ParseAttributionAggregatableSources(const AtomicString& json_string);

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

}  // namespace blink::attribution_response_parsing

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ATTRIBUTION_RESPONSE_PARSING_H_
