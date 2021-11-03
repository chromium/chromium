// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_VISIBLE_TIME_REQUEST_TRIGGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_VISIBLE_TIME_REQUEST_TRIGGER_H_

#include "third_party/blink/public/mojom/widget/record_content_to_visible_time_request.mojom.h"

namespace base {
class TimeTicks;
}  // namespace base

namespace content {

// Records the time of an event that will make a WebContents become visible,
// and the reason it will become visible. This time will be passed to the
// compositor and used to calculate the latency before the visible WebContents
// appears.
//
// This class holds a single RecordContentToVisibleTimeRequestPtr that is
// updated whenever SetRecordContentToVisibleTimeRequest is called, and cleared
// whenever TakeRecordContentToVisibleTimeRequest is called. This means that
// calling Set for multiple overlapping events without calling Take in between
// will record metrics all the events using the same timestamps instead of
// tracking each event separately.
//
// TODO(crbug.com/1263687): Stop doing that. There should be a separate start
// time for each event.
class VisibleTimeRequestTrigger {
 public:
  VisibleTimeRequestTrigger();
  ~VisibleTimeRequestTrigger();

  VisibleTimeRequestTrigger(const VisibleTimeRequestTrigger&) = delete;
  VisibleTimeRequestTrigger& operator=(const VisibleTimeRequestTrigger&) =
      delete;

  // Set the last time a content to visible event starts to be processed for
  // this WebContents. Will merge with the previous value if exists (which
  // means that several events may happen at the same time and must be
  // individually reported). |start_time| marks event start time to calculate
  // the duration later.
  //
  // |destination_is_loaded| is true when
  //   ResourceCoordinatorTabHelper::IsLoaded() is true for the new tab
  //   contents. It is only used when |show_reason_tab_switching| is true.
  // |show_reason_tab_switching| is true when tab switch event should be
  //   reported.
  // |show_reason_unoccluded| is true when "unoccluded" event should be
  //   reported.
  // |show_reason_bfcache_restore| is true when page restored from bfcache event
  //   should be reported.
  void SetRecordContentToVisibleTimeRequest(base::TimeTicks start_time,
                                            bool destination_is_loaded,
                                            bool show_reason_tab_switching,
                                            bool show_reason_unoccluded,
                                            bool show_reason_bfcache_restore);

  // Returns the time set by SetRecordContentToVisibleTimeRequest. If this was
  // not preceded by a call to SetRecordContentToVisibleTimeRequest it will
  // return nullptr. Calling this will reset |last_request_| to null.
  blink::mojom::RecordContentToVisibleTimeRequestPtr
  TakeRecordContentToVisibleTimeRequest();

  // Returns true if blink::features::kTabSwitchMetrics2 is enabled, which
  // affects the measurement behaviour. This lets any caller with access to a
  // VisibleTimeRequestTrigger check the cached value to avoid slow feature
  // lookups on the critical path.
  bool is_tab_switch_metrics2_feature_enabled() const {
    return is_tab_switch_metrics2_feature_enabled_;
  }

 private:
  // The last visible event start request. This should only be set and
  // retrieved using SetRecordContentToVisibleTimeRequest and
  // TakeRecordContentToVisibleTimeRequest.
  blink::mojom::RecordContentToVisibleTimeRequestPtr last_request_;

  bool is_tab_switch_metrics2_feature_enabled_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_VISIBLE_TIME_REQUEST_TRIGGER_H_
