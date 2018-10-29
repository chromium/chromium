// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCOPED_WEB_INPUT_EVENT_WITH_LATENCY_INFO_H_
#define CONTENT_RENDERER_SCOPED_WEB_INPUT_EVENT_WITH_LATENCY_INFO_H_

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/platform/web_coalesced_input_event.h"
#include "third_party/blink/public/platform/web_gesture_event.h"
#include "third_party/blink/public/platform/web_mouse_wheel_event.h"
#include "third_party/blink/public/platform/web_touch_event.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/latency/latency_info.h"

namespace content {

class ScopedWebInputEventWithLatencyInfo {
 public:
  ScopedWebInputEventWithLatencyInfo(ui::WebScopedInputEvent,
                                     const ui::LatencyInfo&);

  ~ScopedWebInputEventWithLatencyInfo();

  bool CanCoalesceWith(const ScopedWebInputEventWithLatencyInfo& other) const
      WARN_UNUSED_RESULT;

  const blink::WebInputEvent& event() const;
  const blink::WebCoalescedInputEvent& coalesced_event() const;
  blink::WebInputEvent& event();
  blink::WebCoalescedInputEvent& coalesced_event();
  const ui::LatencyInfo latencyInfo() const { return latency_; }

  void CoalesceWith(const ScopedWebInputEventWithLatencyInfo& other);

 private:
  blink::WebScopedCoalescedInputEvent event_;
  mutable ui::LatencyInfo latency_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SCOPED_WEB_INPUT_EVENT_WITH_LATENCY_INFO_H_
