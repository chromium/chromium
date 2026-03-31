// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/page/content_to_visible_time_request_mojom_traits.h"

#include "mojo/public/cpp/base/time_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<blink::mojom::RecordContentToVisibleTimeRequestDataView,
                  blink::RecordContentToVisibleTimeRequest>::
    Read(blink::mojom::RecordContentToVisibleTimeRequestDataView data,
         blink::RecordContentToVisibleTimeRequest* out) {
  if (!data.ReadEventStartTime(&out->event_start_time)) {
    return false;
  }
  out->destination_is_loaded = data.destination_is_loaded();
  out->show_reason_tab_switching = data.show_reason_tab_switching();
  out->show_reason_bfcache_restore = data.show_reason_bfcache_restore();
  return true;
}

}  // namespace mojo
