// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/event_timing.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance_event_timing.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

EventTiming::EventTiming(LocalDOMWindow* window) {
  performance_ = DOMWindowPerformance::performance(*window);
}

bool EventTiming::ShouldReportForEventTiming(const Event& event) const {
  return (event.IsMouseEvent() || event.IsPointerEvent() ||
          event.IsTouchEvent() || event.IsKeyboardEvent() ||
          event.IsWheelEvent() || event.IsInputEvent() ||
          event.IsCompositionEvent()) &&
         event.isTrusted();
}

void EventTiming::WillDispatchEvent(const Event& event) {
  // Assume each event can be dispatched only once.
  DCHECK(!finished_will_dispatch_event_);
  if (!performance_ || !ShouldReportForEventTiming(event))
    return;

  // Although we screen the events for timing by setting these conditions here,
  // we cannot assume that the conditions should still hold true in
  // DidDispatchEvent. These conditions have to be re-tested before an entry is
  // dispatched.
  if ((performance_->ShouldBufferEventTiming() &&
       !performance_->IsEventTimingBufferFull()) ||
      performance_->HasObserverFor(PerformanceEntry::kEvent)
      || (performance_->HasObserverFor(PerformanceEntry::kFirstInput)
         && !performance_->FirstInputDetected())) {
    processing_start_ = CurrentTimeTicks();
    finished_will_dispatch_event_ = true;
  }
}

void EventTiming::DidDispatchEvent(const Event& event) {
  if (!finished_will_dispatch_event_ ||
      (!event.executedListenerOrDefaultAction() && !event.DefaultHandled())) {
    return;
  }

  TimeTicks start_time;
  if (event.IsPointerEvent())
    start_time = ToPointerEvent(&event)->OldestPlatformTimeStamp();
  else
    start_time = event.PlatformTimeStamp();

  performance_->RegisterEventTiming(event.type(), start_time, processing_start_,
                                    CurrentTimeTicks(), event.cancelable());
}

}  // namespace blink
