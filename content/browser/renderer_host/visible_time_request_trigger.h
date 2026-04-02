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
// Calling UpdateRequest will append an event to the current request.
class CONTENT_EXPORT VisibleTimeRequestTrigger {
 public:
  VisibleTimeRequestTrigger();
  ~VisibleTimeRequestTrigger();

  VisibleTimeRequestTrigger(const VisibleTimeRequestTrigger&) = delete;
  VisibleTimeRequestTrigger& operator=(const VisibleTimeRequestTrigger&) =
      delete;

  // Set the last time a content to visible event starts to be processed for
  // this WebContents. The new event will be added to the current request.
  // `new_event.event_start_time` marks event start time to calculate the
  // duration later.
  void UpdateRequest(blink::VisibleTimeEvent new_event);

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
