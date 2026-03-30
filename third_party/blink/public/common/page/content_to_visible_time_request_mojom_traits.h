// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_CONTENT_TO_VISIBLE_TIME_REQUEST_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_CONTENT_TO_VISIBLE_TIME_REQUEST_MOJOM_TRAITS_H_

#include "base/time/time.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/page/content_to_visible_time_request.h"
#include "third_party/blink/public/mojom/widget/record_content_to_visible_time_request.mojom-shared.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT StructTraits<
    blink::mojom::RecordContentToVisibleTimeRequestDataView,
    blink::RecordContentToVisibleTimeRequest> {
  static base::TimeTicks event_start_time(
      const blink::RecordContentToVisibleTimeRequest& request) {
    return request.event_start_time;
  }

  static bool destination_is_loaded(
      const blink::RecordContentToVisibleTimeRequest& request) {
    return request.destination_is_loaded;
  }

  static bool show_reason_tab_switching(
      const blink::RecordContentToVisibleTimeRequest& request) {
    return request.show_reason_tab_switching;
  }

  static bool show_reason_bfcache_restore(
      const blink::RecordContentToVisibleTimeRequest& request) {
    return request.show_reason_bfcache_restore;
  }

  static bool show_reason_unfolding(
      const blink::RecordContentToVisibleTimeRequest& request) {
    return request.show_reason_unfolding;
  }

  static bool Read(blink::mojom::RecordContentToVisibleTimeRequestDataView data,
                   blink::RecordContentToVisibleTimeRequest* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_CONTENT_TO_VISIBLE_TIME_REQUEST_MOJOM_TRAITS_H_
