// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/page/content_to_visible_time_request.h"

#include <algorithm>
#include <optional>

#include "base/time/time.h"
#include "base/types/optional_util.h"

namespace blink {

std::optional<RecordContentToVisibleTimeRequest>
ConsumeAndMergeContentToVisibleTimeRequests(
    std::optional<RecordContentToVisibleTimeRequest> request1,
    std::optional<RecordContentToVisibleTimeRequest> request2) {
  if (!request1 && !request2) {
    return std::nullopt;
  }

  // Pick any non-null request to merge into.
  RecordContentToVisibleTimeRequest* to = nullptr;
  RecordContentToVisibleTimeRequest* from = nullptr;
  if (request1) {
    to = base::OptionalToPtr(request1);
    from = base::OptionalToPtr(request2);
  } else {
    to = base::OptionalToPtr(request2);
    from = base::OptionalToPtr(request1);
  }

  if (from) {
    to->event_start_time =
        std::min(to->event_start_time, from->event_start_time);
    to->destination_is_loaded |= from->destination_is_loaded;
    to->show_reason_tab_switching |= from->show_reason_tab_switching;
    to->show_reason_bfcache_restore |= from->show_reason_bfcache_restore;
    to->show_reason_unfolding |= from->show_reason_unfolding;
  }
  return *to;
}

}  // namespace blink
