// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_CONTENT_TO_VISIBLE_TIME_REQUEST_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_CONTENT_TO_VISIBLE_TIME_REQUEST_H_

#include <compare>
#include <optional>

#include "base/time/time.h"
#include "third_party/blink/public/common/common_export.h"

namespace blink {

// Keeps track of parameters for recording metrics for content to visible time
// duration for different events. Here event indicates the reason for which the
// web contents are visible. These values are set with
// VisibleTimeRequestTrigger::SetRecordContentToVisibleTimeRequest. Note that
// |show_reason_tab_switching| and |show_reason_bfcache_restore| can both be
// true at the same time.
//
// This is typemapped to the Mojo struct in
// third_party/blink/public/mojom/widget/record_content_to_visible_time_request.mojom
// so that ContentToVisibleTimeReporter can use the same struct type in both
// Blink and non-Blink code.
struct BLINK_COMMON_EXPORT RecordContentToVisibleTimeRequest {
  // The time at which web contents become visible.
  base::TimeTicks event_start_time;
  // Indicates if the destination tab is loaded when initiating the tab switch.
  bool destination_is_loaded = false;
  // If |show_reason_tab_switching| is true, web contents has become visible
  // because of tab switching.
  bool show_reason_tab_switching = false;
  // If |show_reason_bfcache_restore| is true, web contents has become visible
  // because of restoring a page from bfcache.
  bool show_reason_bfcache_restore = false;

  friend bool operator==(const RecordContentToVisibleTimeRequest&,
                         const RecordContentToVisibleTimeRequest&) = default;
  friend auto operator<=>(const RecordContentToVisibleTimeRequest&,
                          const RecordContentToVisibleTimeRequest&) = default;
};

// Returns a RecordContentToVisibleTimeRequest that combines all passed
// requests, OR'ing all flags and using the minimum start time. Any null
// requests are ignored. The return value will only be nullopt if all arguments
// are nullopt. This function consumes its arguments.
BLINK_COMMON_EXPORT std::optional<RecordContentToVisibleTimeRequest>
ConsumeAndMergeContentToVisibleTimeRequests(
    std::optional<RecordContentToVisibleTimeRequest> request1,
    std::optional<RecordContentToVisibleTimeRequest> request2);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_CONTENT_TO_VISIBLE_TIME_REQUEST_H_
