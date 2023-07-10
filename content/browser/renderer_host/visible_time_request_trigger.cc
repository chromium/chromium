// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/visible_time_request_trigger.h"

#include <algorithm>
#include <utility>

#include "base/time/time.h"
#include "third_party/blink/public/mojom/widget/record_content_to_visible_time_request.mojom.h"

namespace content {

VisibleTimeRequestTrigger::VisibleTimeRequestTrigger() = default;

VisibleTimeRequestTrigger::~VisibleTimeRequestTrigger() = default;

// static
blink::mojom::RecordContentToVisibleTimeRequestPtr
VisibleTimeRequestTrigger::ConsumeAndMergeRequests(
    blink::mojom::RecordContentToVisibleTimeRequestPtr request1,
    blink::mojom::RecordContentToVisibleTimeRequestPtr request2) {
  if (!request1 && !request2)
    return nullptr;

  // Pick any non-null request to merge into.
  blink::mojom::RecordContentToVisibleTimeRequestPtr to;
  blink::mojom::RecordContentToVisibleTimeRequestPtr from;
  if (request1) {
    to = std::move(request1);
    from = std::move(request2);
  } else {
    to = std::move(request2);
    from = std::move(request1);
  }

  if (from) {
    to->event_start_time =
        std::min(to->event_start_time, from->event_start_time);
    to->destination_is_loaded |= from->destination_is_loaded;
    to->show_reason_tab_switching |= from->show_reason_tab_switching;
    to->show_reason_bfcache_restore |= from->show_reason_bfcache_restore;
  }
  return to;
}

void VisibleTimeRequestTrigger::UpdateRequest(
    base::TimeTicks start_time,
    bool destination_is_loaded,
    bool show_reason_tab_switching,
    bool show_reason_bfcache_restore) {
  auto new_request = blink::mojom::RecordContentToVisibleTimeRequest::New(
      start_time, destination_is_loaded, show_reason_tab_switching,
      show_reason_bfcache_restore, /*show_reason_unfold=*/false);
  // If `last_request_` is null, this will return `new_request` unchanged.
  last_request_ =
      ConsumeAndMergeRequests(std::move(last_request_), std::move(new_request));
}

blink::mojom::RecordContentToVisibleTimeRequestPtr
VisibleTimeRequestTrigger::TakeRequest() {
  return std::move(last_request_);
}

}  // namespace content
