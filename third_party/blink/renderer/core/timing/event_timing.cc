// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/event_timing.h"

#include <optional>

#include "base/time/tick_clock.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/hash_change_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/events/pop_state_event.h"
#include "third_party/blink/renderer/core/events/touch_event.h"
#include "third_party/blink/renderer/core/events/wheel_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
#include "third_party/blink/renderer/core/navigation_api/navigate_event.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance_event_timing.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"

namespace blink {
namespace {

// Returns true when the type of the event is one of the standard input events
// measured by Event Timing (e.g. keyboard, mouse, touch, etc.).
bool IsStandardEventType(const Event& event) {
  const AtomicString& type = event.type();

  // 1. Compositionend is measured even if untrusted (per spec).
  if (type == event_type_names::kCompositionend) {
    return true;
  }

  // 2. Reject all other untrusted events for standard types.
  // Note: FullyTrusted instead of Trusted, because some untrusted events
  // synthetically generate trusted events.
  if (!event.IsFullyTrusted()) {
    return false;
  }

  // 3. Fail-fast for high-frequency continuous events.
  if (type == event_type_names::kMousemove ||
      type == event_type_names::kPointermove ||
      type == event_type_names::kTouchmove ||
      type == event_type_names::kPointerrawupdate ||
      type == event_type_names::kWheel || type == event_type_names::kDrag) {
    return false;
  }

  // 4. Fallback for other types using virtual class-id checks (IsA).
  return IsA<MouseEvent>(event) || IsA<PointerEvent>(event) ||
         IsA<TouchEvent>(event) || IsA<KeyboardEvent>(event) ||
         IsA<WheelEvent>(event) || event.IsInputEvent() ||
         event.IsCompositionEvent() || event.IsDragEvent();
}

// Returns true for navigation-related events if they meet the user-initiation
// requirements.
bool IsNavigationEventType(const Event& event) {
  // TODO(crbug.com/418007230): The following events become trusted as part of
  // EventDispatch, but EventTiming is created right before dispatch.
  // These events are always unconditionally trusted, and we use userInitiated
  // value (below), anyway, so we don't need to check this.  But we should
  // re-architect to observe a little later in event dispatch flow.
  // if (!event.isTrusted()) {
  //   return false;
  // }

  if (const auto* navigate_event = DynamicTo<NavigateEvent>(event)) {
    return navigate_event->userInitiated();
  }

  if (const auto* popstate_event = DynamicTo<PopStateEvent>(event)) {
    return popstate_event->IsUserInitiated();
  }

  if (const auto* hashchange_event = DynamicTo<HashChangeEvent>(event)) {
    return hashchange_event->IsUserInitiated();
  }

  return false;
}

}  // namespace

UIEventTiming::UIEventTiming(LocalFrame* frame, const Event& event) {
  if (IsStandardEventType(event)) {
    timing_.emplace(base::PassKey<UIEventTiming>(), frame, event);
  }
}

NavigationEventTiming::NavigationEventTiming(LocalFrame* frame,
                                             const Event& event) {
  if (IsNavigationEventType(event)) {
    timing_.emplace(base::PassKey<NavigationEventTiming>(), frame, event);
  }
}

EventTiming::EventTiming(base::PassKey<UIEventTiming>,
                         LocalFrame* frame,
                         const Event& event)
    : EventTiming(frame, event) {}

EventTiming::EventTiming(base::PassKey<NavigationEventTiming>,
                         LocalFrame* frame,
                         const Event& event)
    : EventTiming(frame, event) {}

EventTiming::EventTiming(LocalFrame* frame, const Event& event) {
  if (!frame) {
    return;
  }
  LocalDOMWindow* window = frame->DomWindow();
  // The context is needed for performance->EventTimingProcessingStart.
  if (!window || window->IsContextDestroyed()) {
    return;
  }
  WindowPerformance* performance = DOMWindowPerformance::performance(*window);
  if (!performance) {
    return;
  }

  // Most events track their performance in EventDispatcher::Dispatch but
  // some event types which can be filtered are tracked at the point
  // where they may be filtered. This condition check ensures we don't create
  // two EventTiming objects for the same Event.
  if (performance->GetCurrentEventTimingEvent() == &event) {
    return;
  }
  performance_ = performance;
  event_ = &event;
  entry_ = performance->EventTimingProcessingStart(event);
  CHECK(entry_);

  if (auto* heuristics = window->GetSoftNavigationHeuristics()) {
    task_scope_ = heuristics->MaybeCreateTaskScopeForEvent(entry_);
  }
}

EventTiming::~EventTiming() {
  if (entry_) {
    CHECK(event_);
    CHECK(performance_);
    performance_->EventTimingProcessingEnd(entry_, *event_);
  }
}


}  // namespace blink
