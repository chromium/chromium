// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/page/record_content_to_visible_time_request_mojom_traits.h"

#include <variant>
#include <vector>

#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace mojo {

// static
bool StructTraits<blink::mojom::VisibleTimeTabSwitchReasonDataView,
                  blink::VisibleTimeEvent::TabSwitchReason>::
    Read(blink::mojom::VisibleTimeTabSwitchReasonDataView data,
         blink::VisibleTimeEvent::TabSwitchReason* out) {
  out->destination_is_loaded = data.destination_is_loaded();
  out->had_saved_frame_at_start = data.had_saved_frame_at_start();
  return true;
}

// static
blink::mojom::VisibleTimeEventReasonDataView::Tag
UnionTraits<blink::mojom::VisibleTimeEventReasonDataView,
            blink::VisibleTimeEvent::Reason>::
    GetTag(const blink::VisibleTimeEvent::Reason& reason) {
  using Tag = blink::mojom::VisibleTimeEventReasonDataView::Tag;
  return std::visit(
      absl::Overload(
          [](const blink::VisibleTimeEvent::TabSwitchReason&) {
            return Tag::kTabSwitch;
          },
          [](const blink::VisibleTimeEvent::BFCacheRestoreReason&) {
            return Tag::kBfcacheRestore;
          }),
      reason);
}

// static
bool UnionTraits<blink::mojom::VisibleTimeEventReasonDataView,
                 blink::VisibleTimeEvent::Reason>::
    Read(blink::mojom::VisibleTimeEventReasonDataView data,
         blink::VisibleTimeEvent::Reason* out) {
  switch (data.tag()) {
    case blink::mojom::VisibleTimeEventReasonDataView::Tag::kTabSwitch: {
      blink::VisibleTimeEvent::TabSwitchReason tab_switch;
      if (!data.ReadTabSwitch(&tab_switch)) {
        return false;
      }
      *out = tab_switch;
      return true;
    }
    case blink::mojom::VisibleTimeEventReasonDataView::Tag::kBfcacheRestore:
      *out = blink::VisibleTimeEvent::BFCacheRestoreReason{};
      return true;
  }
  return false;
}

// static
bool StructTraits<
    blink::mojom::VisibleTimeEventDataView,
    blink::VisibleTimeEvent>::Read(blink::mojom::VisibleTimeEventDataView data,
                                   blink::VisibleTimeEvent* out) {
  if (!data.ReadEventStartTime(&out->event_start_time)) {
    return false;
  }
  if (!data.ReadReason(&out->reason)) {
    return false;
  }
  return true;
}

// static
bool StructTraits<blink::mojom::RecordContentToVisibleTimeRequestDataView,
                  blink::RecordContentToVisibleTimeRequest>::
    Read(blink::mojom::RecordContentToVisibleTimeRequestDataView data,
         blink::RecordContentToVisibleTimeRequest* out) {
  if (!data.ReadEvents(&out->events)) {
    return false;
  }
  return true;
}

}  // namespace mojo
