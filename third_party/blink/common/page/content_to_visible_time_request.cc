// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/page/content_to_visible_time_request.h"

#include <algorithm>
#include <iterator>
#include <variant>

namespace blink {

std::optional<RecordContentToVisibleTimeRequest>
RecordContentToVisibleTimeRequest::ExtractTabSwitchEventsWithSavedFrame() {
  // Sort all events to extract to the end.
  auto first_result = std::partition(
      events.begin(), events.end(), [](const VisibleTimeEvent& event) {
        const auto* tab_switch =
            std::get_if<VisibleTimeEvent::TabSwitchReason>(&event.reason);
        return !tab_switch || !tab_switch->had_saved_frame_at_start;
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

bool RecordContentToVisibleTimeRequest::AllEventsAreTabSwitchesWithSavedFrame()
    const {
  return !events.empty() &&
         std::ranges::all_of(events, [](const blink::VisibleTimeEvent& event) {
           const auto* tab_switch =
               std::get_if<blink::VisibleTimeEvent::TabSwitchReason>(
                   &event.reason);
           return tab_switch && tab_switch->had_saved_frame_at_start;
         });
}

}  // namespace blink
