// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_DOM_TIMER_COORDINATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_DOM_TIMER_COORDINATOR_H_

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class DOMTimer;
class ExecutionContext;
class ScheduledAction;

// Maintains a set of DOMTimers for a given page or
// worker. DOMTimerCoordinator assigns IDs to timers; these IDs are
// the ones returned to web authors from setTimeout or setInterval. It
// also tracks recursive creation or iterative scheduling of timers,
// which is used as a signal for throttling repetitive timers.
class DOMTimerCoordinator {
  DISALLOW_NEW();

 public:
  DOMTimerCoordinator() = default;
  DOMTimerCoordinator(const DOMTimerCoordinator&) = delete;
  DOMTimerCoordinator& operator=(const DOMTimerCoordinator&) = delete;

  // Creates and installs a new timer. Returns the assigned ID.
  int InstallNewTimeout(ExecutionContext*,
                        ScheduledAction*,
                        base::TimeDelta timeout,
                        bool single_shot);

  // Removes and disposes the timer with the specified ID, if any. This may
  // destroy the timer.
  DOMTimer* RemoveTimeoutByID(int id);

  // Timers created during the execution of other timers, and
  // repeating timers, are throttled. Timer nesting level tracks the
  // number of linked timers or repetitions of a timer. See
  // https://html.spec.whatwg.org/C/#timers
  int TimerNestingLevel() { return timer_nesting_level_; }

  // Sets the timer nesting level. Set when a timer executes so that
  // any timers created while the timer is executing will incur a
  // deeper timer nesting level, see DOMTimer::DOMTimer.
  void SetTimerNestingLevel(int level) { timer_nesting_level_ = level; }

  void Trace(Visitor*) const;  // Oilpan.

 private:
  int NextID();

  using TimeoutMap = HeapHashMap<int, Member<DOMTimer>>;
  TimeoutMap timers_;

  int circular_sequential_id_ = 0;
  int timer_nesting_level_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_DOM_TIMER_COORDINATOR_H_
