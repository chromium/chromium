// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/event_timing.h"

#include "base/time/tick_clock.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance_event_timing.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace {
const base::TickClock* g_clock_for_testing = nullptr;

static base::TimeTicks Now() {
  return g_clock_for_testing ? g_clock_for_testing->NowTicks()
                             : base::TimeTicks::Now();
}
}  // namespace

namespace blink {

bool ShouldLogEvent(const Event& event) {
  return event.type() == event_type_names::kPointerdown ||
         event.type() == event_type_names::kPointerup ||
         event.type() == event_type_names::kClick ||
         event.type() == event_type_names::kKeydown ||
         event.type() == event_type_names::kMousedown;
}

bool IsEventTypeForEventTiming(const Event& event) {
  return (event.IsMouseEvent() || event.IsPointerEvent() ||
          event.IsTouchEvent() || event.IsKeyboardEvent() ||
          event.IsWheelEvent() || event.IsInputEvent() ||
          event.IsCompositionEvent()) &&
         event.isTrusted();
}

bool ShouldReportForEventTiming(WindowPerformance* performance) {
  if (!performance->FirstInputDetected())
    return true;

  if (!RuntimeEnabledFeatures::EventTimingEnabled(
          performance->GetExecutionContext()))
    return false;

  return (!performance->IsEventTimingBufferFull() ||
          performance->HasObserverFor(PerformanceEntry::kEvent));
}

EventTiming::EventTiming(base::TimeTicks processing_start,
                         base::TimeTicks event_timestamp,
                         WindowPerformance* performance)
    : processing_start_(processing_start),
      event_timestamp_(event_timestamp),
      performance_(performance) {}

// static
std::unique_ptr<EventTiming> EventTiming::Create(LocalDOMWindow* window,
                                                 const Event& event) {
  auto* performance = DOMWindowPerformance::performance(*window);
  if (!performance || !IsEventTypeForEventTiming(event))
    return nullptr;

  bool should_report_for_event_timing = ShouldReportForEventTiming(performance);
  bool should_log_event = ShouldLogEvent(event);

  if (!should_report_for_event_timing && !should_log_event)
    return nullptr;

  base::TimeTicks event_timestamp =
      event.IsPointerEvent() ? ToPointerEvent(&event)->OldestPlatformTimeStamp()
                             : event.PlatformTimeStamp();

  base::TimeTicks processing_start = Now();
  if (should_log_event) {
    Document* document =
        DynamicTo<Document>(performance->GetExecutionContext());
    InteractiveDetector* interactive_detector =
        InteractiveDetector::From(*document);
    if (interactive_detector) {
      interactive_detector->HandleForInputDelay(event, event_timestamp,
                                                processing_start);
    }
  }

  return should_report_for_event_timing
             ? std::make_unique<EventTiming>(processing_start, event_timestamp,
                                             performance)
             : nullptr;
}

void EventTiming::DidDispatchEvent(const Event& event) {
  performance_->RegisterEventTiming(event.type(), event_timestamp_,
                                    processing_start_, Now(),
                                    event.cancelable());
}

// static
void EventTiming::SetTickClockForTesting(const base::TickClock* clock) {
  g_clock_for_testing = clock;
}

}  // namespace blink
