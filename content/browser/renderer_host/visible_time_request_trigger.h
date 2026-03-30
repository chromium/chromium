// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_VISIBLE_TIME_REQUEST_TRIGGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_VISIBLE_TIME_REQUEST_TRIGGER_H_

#include <optional>

#include "content/common/content_export.h"
#include "third_party/blink/public/common/page/content_to_visible_time_request.h"

namespace content {

// Records the time of an event that will make a WebContents become visible,
// and the reason it will become visible. This time will be passed to the
// compositor and used to calculate the latency before the visible WebContents
// appears.
//
// This class holds a single RecordContentToVisibleTimeRequest that is modified
// whenever UpdateRequest is called, and cleared whenever TakeRequest is called.
// This means that calling UpdateRequest for multiple overlapping events without
// calling TakeRequest in between will record metrics for all events using the
// same timestamps instead of tracking each event separately.
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

  // Set the last time a content to visible event starts to be processed for
  // this WebContents. Will merge with the previous value if it exists (which
  // means that several events may happen at the same time and must be
  // individually reported). `new_request.event_start_time` marks event start
  // time to calculate the duration later.
  void UpdateRequest(blink::RecordContentToVisibleTimeRequest new_request);

  // Returns the time set by UpdateRequest. If this was not preceded by a call
  // to UpdateRequest it will return nullopt. Calling this will reset
  // |last_request_| to nullopt.
  std::optional<blink::RecordContentToVisibleTimeRequest> TakeRequest();

 private:
  // The last visible event start request. This should only be set and
  // retrieved using UpdateRequest and TakeRequest.
  std::optional<blink::RecordContentToVisibleTimeRequest> last_request_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_VISIBLE_TIME_REQUEST_TRIGGER_H_
