// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_CONTENT_TO_VISIBLE_TIME_REQUEST_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_CONTENT_TO_VISIBLE_TIME_REQUEST_H_

#include <compare>
#include <optional>
#include <variant>
#include <vector>

#include "base/time/time.h"
#include "third_party/blink/public/common/common_export.h"

namespace blink {

// Keeps track of parameters for recording metrics for content to visible time
// duration for different events. Here event indicates the reason for which the
// web contents are visible. These values are set with
// VisibleTimeRequestTrigger::UpdateRequest.
//
// These are typemapped to the Mojo structs in
// third_party/blink/public/mojom/widget/record_content_to_visible_time_request.mojom
// so that ContentToVisibleTimeReporter can use the same struct types in both
// Blink and non-Blink code.

struct BLINK_COMMON_EXPORT VisibleTimeEvent {
  struct TabSwitchReason {
    bool destination_is_loaded = false;
    bool had_saved_frame_at_start = false;

    friend bool operator==(const TabSwitchReason&,
                           const TabSwitchReason&) = default;
    friend auto operator<=>(const TabSwitchReason&,
                            const TabSwitchReason&) = default;
  };

  struct BFCacheRestoreReason {
    friend bool operator==(const BFCacheRestoreReason&,
                           const BFCacheRestoreReason&) = default;
    friend auto operator<=>(const BFCacheRestoreReason&,
                            const BFCacheRestoreReason&) = default;
  };

  using Reason = std::variant<TabSwitchReason, BFCacheRestoreReason>;

  base::TimeTicks event_start_time;
  Reason reason = {};

  friend bool operator==(const VisibleTimeEvent&,
                         const VisibleTimeEvent&) = default;
  friend auto operator<=>(const VisibleTimeEvent&,
                          const VisibleTimeEvent&) = default;
};

struct BLINK_COMMON_EXPORT RecordContentToVisibleTimeRequest {
  std::vector<VisibleTimeEvent> events;

  friend bool operator==(const RecordContentToVisibleTimeRequest&,
                         const RecordContentToVisibleTimeRequest&) = default;
  friend auto operator<=>(const RecordContentToVisibleTimeRequest&,
                          const RecordContentToVisibleTimeRequest&) = default;

  // Moves all the events with reason TabSwitchReason and
  // `had_saved_frame_at_start` to a new request and returns it (or returns
  // nullopt if there are none). Note this may leave `events` empty.
  std::optional<RecordContentToVisibleTimeRequest>
  ExtractTabSwitchEventsWithSavedFrame();

  // Returns true if the request has one or more events, and all of them have
  // reason TabSwitchReason and `had_saved_frame_at_start` set to true. This
  // will always be true for a request returned by
  // ExtractTabSwitchEventsWithSavedFrame().
  bool AllEventsAreTabSwitchesWithSavedFrame() const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_CONTENT_TO_VISIBLE_TIME_REQUEST_H_
