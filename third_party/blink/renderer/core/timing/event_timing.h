// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_EVENT_TIMING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_EVENT_TIMING_H_

#include <optional>

#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace base {
class TickClock;
}

namespace blink {

class Event;
class EventTarget;
class LocalFrame;
class PerformanceEventTiming;

class UIEventTiming;
class NavigationEventTiming;

// Event timing collects and records the event start time, processing start time
// and processing end time of long-latency events, providing a tool to evaluate
// input latency.
// See also: https://github.io/event-timing
class CORE_EXPORT EventTiming final {
  STACK_ALLOCATED();

 public:
  EventTiming(base::PassKey<UIEventTiming>,
              LocalFrame* frame,
              const Event& event,
              EventTarget* hit_test_target);
  EventTiming(base::PassKey<NavigationEventTiming>,
              LocalFrame* frame,
              const Event& event,
              EventTarget* hit_test_target);
  ~EventTiming();

  EventTiming(const EventTiming&) = delete;
  EventTiming& operator=(const EventTiming&) = delete;

  // The caller owns the |clock| which must outlive the EventTiming.
  static void SetTickClockForTesting(const base::TickClock* clock);

  std::optional<PerformanceTimelineEntryIdInfo> GetInteractionIdInfo() const {
    return entry_ ? entry_->GetInteractionIdInfo() : std::nullopt;
  }

 private:
  EventTiming(LocalFrame* frame,
              const Event& event,
              EventTarget* hit_test_target);

  WindowPerformance* performance_ = nullptr;
  const Event* event_ = nullptr;
  PerformanceEventTiming* entry_ = nullptr;
  std::optional<scheduler::TaskAttributionTracker::TaskScope> task_scope_;
};

class CORE_EXPORT UIEventTiming final {
  STACK_ALLOCATED();

 public:
  UIEventTiming(LocalFrame* frame,
                const Event& event,
                EventTarget* hit_test_target);

  std::optional<PerformanceTimelineEntryIdInfo> GetInteractionIdInfo() const {
    return timing_ ? timing_->GetInteractionIdInfo() : std::nullopt;
  }

 private:
  std::optional<EventTiming> timing_;
};

class CORE_EXPORT NavigationEventTiming final {
  STACK_ALLOCATED();

 public:
  NavigationEventTiming(LocalFrame* frame,
                        const Event& event,
                        EventTarget* hit_test_target);

  std::optional<PerformanceTimelineEntryIdInfo> GetInteractionIdInfo() const {
    return timing_ ? timing_->GetInteractionIdInfo() : std::nullopt;
  }

 private:
  std::optional<EventTiming> timing_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_EVENT_TIMING_H_
