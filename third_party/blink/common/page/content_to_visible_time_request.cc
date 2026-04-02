// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/page/content_to_visible_time_request.h"

#include <algorithm>
#include <iterator>
#include <variant>

namespace blink {

std::optional<RecordContentToVisibleTimeRequest>
RecordContentToVisibleTimeRequest::ExtractTabSwitchEvents() {
  // Sort all events to extract to the end.
  auto first_result = std::partition(
      events.begin(), events.end(), [](const VisibleTimeEvent& event) {
        return !std::holds_alternative<VisibleTimeEvent::TabSwitchReason>(
            event.reason);
      });
  if (first_result == events.end()) {
    return std::nullopt;
  }

  // Move the extracted events to the result vector.
  auto result = std::make_optional<RecordContentToVisibleTimeRequest>();
  result->events.insert(result->events.end(),
                        std::make_move_iterator(first_result),
                        std::make_move_iterator(events.end()));
  events.erase(first_result, events.end());
  return result;
}

}  // namespace blink
