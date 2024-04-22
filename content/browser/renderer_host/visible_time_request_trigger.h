// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_VISIBLE_TIME_REQUEST_TRIGGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_VISIBLE_TIME_REQUEST_TRIGGER_H_

#include "content/common/content_export.h"
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
// modified whenever UpdateRequest is called, and cleared whenever TakeRequest
// is called. This means that calling UpdateRequest for multiple overlapping
// events without calling TakeRequest in between will record metrics for all
// events using the same timestamps instead of tracking each event separately.
//
// TODO(crbug.com/40203057): Stop doing that. There should be a separate start
// time for each event.
class CONTENT_EXPORT VisibleTimeRequestTrigger {
 public:
  VisibleTimeRequestTrigger();
  ~VisibleTimeRequestTrigger();

  VisibleTimeRequestTrigger(const VisibleTimeRequestTrigger&) = delete;
  VisibleTimeRequestTrigger& operator=(const VisibleTimeRequestTrigger&) =
      delete;

  // Returns a RecordContentToVisibleTimeRequestPtr that combines all passed
  // requests, OR'ing all flags and using the minimum start time. Any null
  // requests are ignored. The return value will only be nullptr if all
  // arguments are nullptr. This function consumes its arguments.
  static blink::mojom::RecordContentToVisibleTimeRequestPtr
  ConsumeAndMergeRequests(
      blink::mojom::RecordContentToVisibleTimeRequestPtr request1,
      blink::mojom::RecordContentToVisibleTimeRequestPtr request2);

  // Set the last time a content to visible event starts to be processed for
  // this WebContents. Will merge with the previous value if it exists (which
  // means that several events may happen at the same time and must be
  // individually reported). |start_time| marks event start time to calculate
  // the duration later.
  //
  // |destination_is_loaded| is true when
  //   ResourceCoordinatorTabHelper::IsLoaded() is true for the new tab
  //   contents. It is only used when |show_reason_tab_switching| is true.
  // |show_reason_tab_switching| is true when tab switch event should be
  //   reported.
  // |show_reason_bfcache_restore| is true when page restored from bfcache event
  //   should be reported.
  void UpdateRequest(base::TimeTicks start_time,
                     bool destination_is_loaded,
                     bool show_reason_tab_switching,
                     bool show_reason_bfcache_restore);

  // Returns the time set by UpdateRequest. If this was not preceded by a call
  // to UpdateRequest it will return nullptr. Calling this will reset
  // |last_request_| to null.
  blink::mojom::RecordContentToVisibleTimeRequestPtr TakeRequest();

 private:
  // The last visible event start request. This should only be set and
  // retrieved using UpdateRequest and TakeRequest.
  blink::mojom::RecordContentToVisibleTimeRequestPtr last_request_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_VISIBLE_TIME_REQUEST_TRIGGER_H_
