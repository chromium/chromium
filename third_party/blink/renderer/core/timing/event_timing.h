// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_EVENT_TIMING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_EVENT_TIMING_H_

#include <memory>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Event;

// Event timing collects and records the event start time, processing start time
// and processing end time of long-latency events, providing a tool to evalute
// input latency.
// See also: https://github.com/wicg/event-timing
class CORE_EXPORT EventTiming final {
  USING_FAST_MALLOC(EventTiming);

 public:
  // Processes an event that will be dispatched. Notifies the
  // InteractiveDetector if it needs to be logged into input delay histograms.
  // Returns an object only if the event is relevant for the EventTiming API.
  static std::unique_ptr<EventTiming> Create(LocalDOMWindow*, const Event&);

  explicit EventTiming(base::TimeTicks processing_start,
                       base::TimeTicks event_timestamp,
                       WindowPerformance* performance);

  // Notifies the Performance object that the event has been dispatched.
  void DidDispatchEvent(const Event&);

  // The caller owns the |clock| which must outlive the EventTiming.
  static void SetTickClockForTesting(const base::TickClock* clock);

 private:
  // The time the first event handler or default action started to execute.
  base::TimeTicks processing_start_;
  // The event timestamp to be used in EventTiming and in histograms.
  base::TimeTicks event_timestamp_;

  Persistent<WindowPerformance> performance_;
  DISALLOW_COPY_AND_ASSIGN(EventTiming);
};

}  // namespace blink

#endif
