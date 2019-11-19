// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/cooperative_scheduling_manager.h"

#include "base/auto_reset.h"
#include "base/run_loop.h"
#include "base/time/default_tick_clock.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

namespace blink {
namespace scheduler {

namespace {
// Minimum time interval between nested loop runs.
constexpr base::TimeDelta kNestedLoopMinimumInterval =
    base::TimeDelta::FromMilliseconds(15);
}  // namespace

// static
CooperativeSchedulingManager* CooperativeSchedulingManager::Instance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<CooperativeSchedulingManager>,
                                  manager, ());
  return &(*manager);
}

CooperativeSchedulingManager::AllowedStackScope::AllowedStackScope(
    CooperativeSchedulingManager* manager)
    : cooperative_scheduling_manager_(manager) {
  DCHECK(cooperative_scheduling_manager_);
  cooperative_scheduling_manager_->EnterAllowedStackScope();
}

CooperativeSchedulingManager::AllowedStackScope::~AllowedStackScope() {
  cooperative_scheduling_manager_->LeaveAllowedStackScope();
}

CooperativeSchedulingManager::CooperativeSchedulingManager()
    : clock_(base::DefaultTickClock::GetInstance()),
      feature_enabled_(RuntimeEnabledFeatures::CooperativeSchedulingEnabled()) {
}

void CooperativeSchedulingManager::EnterAllowedStackScope() {
  TRACE_EVENT_ASYNC_BEGIN0("renderer.scheduler", "PreemptionAllowedStackScope",
                           this);

  allowed_stack_scope_depth_++;
}

void CooperativeSchedulingManager::LeaveAllowedStackScope() {
  TRACE_EVENT_ASYNC_END0("renderer.scheduler", "PreemptionAllowedStackScope",
                         this);
  allowed_stack_scope_depth_--;
  DCHECK_GE(allowed_stack_scope_depth_, 0);
}

void CooperativeSchedulingManager::SafepointSlow() {
  // Avoid nesting more than two levels.
  if (running_nested_loop_ || base::RunLoop::IsNestedOnCurrentThread())
    return;

  if (!feature_enabled_)
    return;

  // TODO(keishi): Also bail if V8 EnteredContextCount is more than 1
  // This task slice completes here.
  Thread::MainThread()->Scheduler()->OnSafepointEntered();

  RunNestedLoop();

  // A new task slice starts here.
  Thread::MainThread()->Scheduler()->OnSafepointExited();
}

void CooperativeSchedulingManager::RunNestedLoop() {
  TRACE_EVENT0("renderer.scheduler",
               "CooperativeSchedulingManager::RunNestedLoop");
  base::AutoReset<bool> nested_loop_scope(&running_nested_loop_, true);
  wait_until_ = clock_->NowTicks() + kNestedLoopMinimumInterval;

  // TODO(keishi): Ask scheduler to run high priority tasks from different
  // EventLoops.
}

void CooperativeSchedulingManager::SetTickClockForTesting(
    const base::TickClock* clock) {
  clock_ = clock;
}

}  // namespace scheduler
}  // namespace blink
