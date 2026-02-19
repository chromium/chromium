// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/event_timing.h"

#include <optional>

#include "base/time/tick_clock.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/events/touch_event.h"
#include "third_party/blink/renderer/core/events/wheel_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance_event_timing.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {
namespace {
const base::TickClock* g_clock_for_testing = nullptr;

static base::TimeTicks Now() {
  return g_clock_for_testing ? g_clock_for_testing->NowTicks()
                             : base::TimeTicks::Now();
}

bool ShouldLogEvent(const Event& event) {
  return event.type() == event_type_names::kPointerdown ||
         event.type() == event_type_names::kPointerup ||
         event.type() == event_type_names::kClick ||
         event.type() == event_type_names::kKeydown ||
         event.type() == event_type_names::kMousedown ||
         event.type() == event_type_names::kMouseup;
}

void HandleInputDelay(LocalDOMWindow* window,
                      const Event& event,
                      base::TimeTicks processing_start) {
  auto* pointer_event = DynamicTo<PointerEvent>(&event);
  base::TimeTicks event_timestamp =
      pointer_event ? pointer_event->OldestPlatformTimeStamp()
                    : event.PlatformTimeStamp();

  if (ShouldLogEvent(event) && event.isTrusted()) {
    InteractiveDetector* interactive_detector =
        InteractiveDetector::From(*window->document());
    if (interactive_detector) {
      interactive_detector->HandleForInputDelay(event, event_timestamp,
                                                processing_start);
    }
  }
}

// Returns true when the type of the event is included in the EventTiming.
bool IsEventTypeForEventTiming(const Event& event) {
  // Include only trusted events of certain kinds. Explicitly excluding input
  // events that are considered continuous: event types for which the user agent
  // may have timer-based dispatch under certain conditions. These are excluded
  // since EventCounts cannot be used to properly computed percentiles on those.
  // See spec: https://wicg.github.io/event-timing/#sec-events-exposed.
  // Needs to be kept in sync with WebInputEvent::IsWebInteractionEvent(),
  // except non-raw web input event types, for example kCompositionend.
  return (event.isTrusted() ||
          event.type() == event_type_names::kCompositionend) &&
         (IsA<MouseEvent>(event) || IsA<PointerEvent>(event) ||
          IsA<TouchEvent>(event) || IsA<KeyboardEvent>(event) ||
          IsA<WheelEvent>(event) || event.IsInputEvent() ||
          event.IsCompositionEvent() || event.IsDragEvent()) &&
         event.type() != event_type_names::kMousemove &&
         event.type() != event_type_names::kPointermove &&
         event.type() != event_type_names::kPointerrawupdate &&
         event.type() != event_type_names::kTouchmove &&
         event.type() != event_type_names::kWheel &&
         event.type() != event_type_names::kDrag;
}
}  // namespace

EventTiming::EventTiming(LocalFrame* frame,
                         const Event& event,
                         EventTarget* hit_test_target) {
  if (!frame) {
    return;
  }
  LocalDOMWindow* window = frame->DomWindow();
  // The context is needed for performance->EventTimingProcessingStart.
  if (!window || window->IsContextDestroyed()) {
    return;
  }
  WindowPerformance* performance = DOMWindowPerformance::performance(*window);
  if (!performance || !IsEventTypeForEventTiming(event)) {
    return;
  }

  // Most events track their performance in EventDispatcher::Dispatch but
  // some event types which can be filtered are tracked at the point
  // where they may be filtered. This condition check ensures we don't create
  // two EventTiming objects for the same Event.
  if (performance->GetCurrentEventTimingEvent() == &event) {
    return;
  }
  base::TimeTicks processing_start = Now();

  // TODO(mmocny): Move into window_performance / responsiveness_metrics?
  HandleInputDelay(window, event, processing_start);

  performance_ = performance;
  event_ = &event;
  entry_ = performance->EventTimingProcessingStart(event, processing_start,
                                                   hit_test_target);
  CHECK(entry_);
}

EventTiming::~EventTiming() {
  if (entry_) {
    CHECK(event_);
    CHECK(performance_);
    performance_->EventTimingProcessingEnd(entry_, *event_, Now());
  }
}

// static
void EventTiming::SetTickClockForTesting(const base::TickClock* clock) {
  g_clock_for_testing = clock;
}

}  // namespace blink
