// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_EVENT_TIMING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_EVENT_TIMING_H_

#include "base/time/time.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {
class Event;

// Event timing collects and records the event start time, processing start time
// and processing end time of long-latency events, providing a tool to evaluate
// input latency.
// See also: https://github.com/wicg/event-timing
//
// The class follows the familiar RAII pattern; the object should be
// constructed before the event is dispatched and destructed after dispatch
// so that we can calculate the input delay and other values correctly.
//
// If |frame| is nullptr, or if the event is not relevant to the EventTimingAPI,
// the instance will amount to a no-op.
class CORE_EXPORT EventTiming final {
  STACK_ALLOCATED();

 public:
  EventTiming(LocalFrame* frame,
              const Event& event,
              EventTarget* hit_test_target);

  ~EventTiming();
  EventTiming(const EventTiming&) = delete;
  EventTiming& operator=(const EventTiming&) = delete;

  // The caller owns the |clock| which must outlive the EventTiming.
  static void SetTickClockForTesting(const base::TickClock* clock);

 private:
  WindowPerformance* performance_ = nullptr;
  const Event* event_ = nullptr;
  PerformanceEventTiming* entry_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_EVENT_TIMING_H_
