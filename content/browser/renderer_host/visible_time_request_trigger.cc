// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/visible_time_request_trigger.h"

#include <utility>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/content_to_visible_time_reporter.h"
#include "third_party/blink/public/mojom/widget/record_content_to_visible_time_request.mojom.h"

namespace content {

VisibleTimeRequestTrigger::VisibleTimeRequestTrigger()
    : is_tab_switch_metrics2_feature_enabled_(
          base::FeatureList::IsEnabled(blink::features::kTabSwitchMetrics2)) {}

VisibleTimeRequestTrigger::~VisibleTimeRequestTrigger() = default;

void VisibleTimeRequestTrigger::SetRecordContentToVisibleTimeRequest(
    base::TimeTicks start_time,
    bool destination_is_loaded,
    bool show_reason_tab_switching,
    bool show_reason_unoccluded,
    bool show_reason_bfcache_restore) {
  auto new_request = blink::mojom::RecordContentToVisibleTimeRequest::New(
      start_time, destination_is_loaded, show_reason_tab_switching,
      show_reason_unoccluded, show_reason_bfcache_restore);

  if (last_request_) {
    blink::UpdateRecordContentToVisibleTimeRequest(*new_request,
                                                   *last_request_);
  } else {
    last_request_ = std::move(new_request);
  }
}

blink::mojom::RecordContentToVisibleTimeRequestPtr
VisibleTimeRequestTrigger::TakeRecordContentToVisibleTimeRequest() {
  return std::move(last_request_);
}

}  // namespace content
