// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_EVENT_TIMING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_EVENT_TIMING_H_

#include <memory>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"

namespace blink {

class Event;

// Event timing collects and records the event start time, processing start time
// and processing end time of long-latency events, providing a tool to evalute
// input latency.
// See also: https://github.com/wicg/event-timing
class CORE_EXPORT EventTiming final {
 public:
  explicit EventTiming(LocalDOMWindow*);

  void WillDispatchEvent(const Event&);
  void DidDispatchEvent(const Event&);

 private:
  bool ShouldReportForEventTiming(const Event& event) const;
  // The time the first event handler or default action started to execute.
  TimeTicks processing_start_;
  bool finished_will_dispatch_event_ = false;

  Persistent<WindowPerformance> performance_;
  DISALLOW_COPY_AND_ASSIGN(EventTiming);
};

}  // namespace blink

#endif
