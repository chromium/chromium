// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/dom_timer_coordinator.h"

#include <algorithm>
#include <memory>
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/dom_timer.h"

namespace blink {

DOMTimerCoordinator::DOMTimerCoordinator(
    scoped_refptr<base::SingleThreadTaskRunner> timer_task_runner)
    : circular_sequential_id_(0),
      timer_nesting_level_(0),
      timer_task_runner_(std::move(timer_task_runner)) {}

int DOMTimerCoordinator::InstallNewTimeout(ExecutionContext* context,
                                           ScheduledAction* action,
                                           base::TimeDelta timeout,
                                           bool single_shot) {
  // FIXME: DOMTimers depends heavily on ExecutionContext. Decouple them.
  DCHECK_EQ(context->Timers(), this);
  int timeout_id = NextID();
  timers_.insert(timeout_id,
                 MakeGarbageCollected<DOMTimer>(context, action, timeout,
                                                single_shot, timeout_id));
  return timeout_id;
}

DOMTimer* DOMTimerCoordinator::RemoveTimeoutByID(int timeout_id) {
  if (timeout_id <= 0)
    return nullptr;

  DOMTimer* removed_timer = timers_.Take(timeout_id);
  if (removed_timer)
    removed_timer->Stop();

  return removed_timer;
}

void DOMTimerCoordinator::Trace(blink::Visitor* visitor) {
  visitor->Trace(timers_);
}

int DOMTimerCoordinator::NextID() {
  while (true) {
    ++circular_sequential_id_;

    if (circular_sequential_id_ <= 0)
      circular_sequential_id_ = 1;

    if (!timers_.Contains(circular_sequential_id_))
      return circular_sequential_id_;
  }
}

}  // namespace blink
