// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_RECORD_CONTENT_TO_VISIBLE_TIME_REQUEST_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_RECORD_CONTENT_TO_VISIBLE_TIME_REQUEST_MOJOM_TRAITS_H_

#include <vector>

#include "base/time/time.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/union_traits.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/page/content_to_visible_time_request.h"
#include "third_party/blink/public/mojom/widget/record_content_to_visible_time_request.mojom-shared.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT StructTraits<
    blink::mojom::VisibleTimeTabSwitchReasonDataView,
    blink::VisibleTimeEvent::TabSwitchReason> {
  static bool destination_is_loaded(
      const blink::VisibleTimeEvent::TabSwitchReason& reason) {
    return reason.destination_is_loaded;
  }

  static bool had_saved_frame_at_start(
      const blink::VisibleTimeEvent::TabSwitchReason& reason) {
    return reason.had_saved_frame_at_start;
  }

  static bool Read(blink::mojom::VisibleTimeTabSwitchReasonDataView data,
                   blink::VisibleTimeEvent::TabSwitchReason* out);
};

template <>
struct BLINK_COMMON_EXPORT UnionTraits<
    blink::mojom::VisibleTimeEventReasonDataView,
    blink::VisibleTimeEvent::Reason> {
  static blink::mojom::VisibleTimeEventReasonDataView::Tag GetTag(
      const blink::VisibleTimeEvent::Reason& reason);

  static const blink::VisibleTimeEvent::TabSwitchReason& tab_switch(
      const blink::VisibleTimeEvent::Reason& reason) {
    return std::get<blink::VisibleTimeEvent::TabSwitchReason>(reason);
  }

  static bool bfcache_restore(const blink::VisibleTimeEvent::Reason& reason) {
    return true;
  }

  static bool Read(blink::mojom::VisibleTimeEventReasonDataView data,
                   blink::VisibleTimeEvent::Reason* out);
};

template <>
struct BLINK_COMMON_EXPORT StructTraits<blink::mojom::VisibleTimeEventDataView,
                                        blink::VisibleTimeEvent> {
  static base::TimeTicks event_start_time(
      const blink::VisibleTimeEvent& event) {
    return event.event_start_time;
  }

  static const blink::VisibleTimeEvent::Reason& reason(
      const blink::VisibleTimeEvent& event) {
    return event.reason;
  }

  static bool Read(blink::mojom::VisibleTimeEventDataView data,
                   blink::VisibleTimeEvent* out);
};

template <>
struct BLINK_COMMON_EXPORT StructTraits<
    blink::mojom::RecordContentToVisibleTimeRequestDataView,
    blink::RecordContentToVisibleTimeRequest> {
  static std::vector<blink::VisibleTimeEvent> events(
      const blink::RecordContentToVisibleTimeRequest& request) {
    return request.events;
  }

  static bool Read(blink::mojom::RecordContentToVisibleTimeRequestDataView data,
                   blink::RecordContentToVisibleTimeRequest* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_RECORD_CONTENT_TO_VISIBLE_TIME_REQUEST_MOJOM_TRAITS_H_
