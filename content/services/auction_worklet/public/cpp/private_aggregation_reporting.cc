// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/public/cpp/private_aggregation_reporting.h"

#include <optional>
#include <string>

#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"

namespace auction_worklet {

std::optional<auction_worklet::mojom::ReservedEventType> ParseReservedEventType(
    const std::string& event_type_str) {
  auto it = kReservedEventTypes.find(event_type_str);
  if (it != kReservedEventTypes.end()) {
    return it->second;
  } else {
    return std::nullopt;
  }
}

std::optional<auction_worklet::mojom::EventTypePtr>
ParsePrivateAggregationEventType(const std::string& event_type_str) {
  std::optional<auction_worklet::mojom::EventTypePtr> event_type;
  if (base::StartsWith(event_type_str, "reserved.")) {
    std::optional<auction_worklet::mojom::ReservedEventType> maybe_reserved =
        ParseReservedEventType(event_type_str);
    // Don't throw an error if an invalid reserved event type is provided, to
    // provide forward compatibility with new reserved event types added
    // later.
    if (maybe_reserved.has_value()) {
      event_type = auction_worklet::mojom::EventType::NewReserved(
          maybe_reserved.value());
    }
  } else {
    event_type =
        auction_worklet::mojom::EventType::NewNonReserved(event_type_str);
  }
  return event_type;
}

}  // namespace auction_worklet
