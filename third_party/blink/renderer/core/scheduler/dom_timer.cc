/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include "third_party/blink/renderer/core/scheduler/dom_timer.h"

#include <limits>

#include "base/message_loop/message_pump.h"
#include "base/numerics/clamped_math.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/core_probes_inl.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/page_dismissal_scope.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/scheduler/scheduled_action.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"
#include "third_party/blink/renderer/platform/weborigin/reporting_disposition.h"

namespace blink {

namespace {

// Step 11 of the algorithm at
// https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html requires
// that a timeout less than 4ms is increased to 4ms when the nesting level is
// greater than 5.
constexpr int kMaxTimerNestingLevel = 5;
constexpr base::TimeDelta kMinimumInterval = base::Milliseconds(4);

base::TimeDelta GetMaxHighResolutionInterval() {
  return base::MessagePump::GetAlignWakeUpsEnabled() &&
                 base::FeatureList::IsEnabled(
                     features::kLowerHighResolutionTimerThreshold)
             ? base::Milliseconds(4)
             : base::Milliseconds(32);
}

// Maintains a set of DOMTimers for a given ExecutionContext. Assigns IDs to
// timers; these IDs are the ones returned to web authors from setTimeout or
// setInterval. It also tracks recursive creation or iterative scheduling of
// timers, which is used as a signal for throttling repetitive timers.
class DOMTimerCoordinator : public GarbageCollected<DOMTimerCoordinator>,
                            public Supplement<ExecutionContext> {
 public:
  constexpr static const char kSupplementName[] = "DOMTimerCoordinator";

  static DOMTimerCoordinator& From(ExecutionContext& context) {
    CHECK(!context.IsWorkletGlobalScope());
    auto* coordinator =
        Supplement<ExecutionContext>::From<DOMTimerCoordinator>(context);
    if (!coordinator) {
      coordinator = MakeGarbageCollected<DOMTimerCoordinator>(context);
      Supplement<ExecutionContext>::ProvideTo(context, coordinator);
    }
    return *coordinator;
  }

  explicit DOMTimerCoordinator(ExecutionContext& context)
      : Supplement<ExecutionContext>(context) {}

  int Install(DOMTimer* timer) {
    int timeout_id = NextID();
    timers_.insert(timeout_id, timer);
    return timeout_id;
  }

  // Removes and disposes the timer with the specified ID, if any. This may
  // destroy the timer.
  DOMTimer* RemoveTimeoutByID(int timeout_id) {
    if (timeout_id <= 0) {
      return nullptr;
    }
    DOMTimer* removed_timer = timers_.Take(timeout_id);
    if (removed_timer) {
      removed_timer->Stop();
    }
    return removed_timer;
  }

  // Timers created during the execution of other timers, and
  // repeating timers, are throttled. Timer nesting level tracks the
  // number of linked timers or repetitions of a timer. See
  // https://html.spec.whatwg.org/C/#timers
  int TimerNestingLevel() { return timer_nesting_level_; }

  // Sets the timer nesting level. Set when a timer executes so that
  // any timers created while the timer is executing will incur a
  // deeper timer nesting level, see DOMTimer::DOMTimer.
  void SetTimerNestingLevel(int level) { timer_nesting_level_ = level; }

  void Trace(Visitor* visitor) const final {
    visitor->Trace(timers_);
    Supplement<ExecutionContext>::Trace(visitor);
  }

 private:
  int NextID() {
    while (true) {
      if (circular_sequential_id_ == std::numeric_limits<int>::max()) {
        circular_sequential_id_ = 1;
      } else {
        ++circular_sequential_id_;
      }

      if (!timers_.Contains(circular_sequential_id_)) {
        return circular_sequential_id_;
      }
    }
  }

  HeapHashMap<int, Member<DOMTimer>> timers_;
  int circular_sequential_id_ = 0;
  int timer_nesting_level_ = 0;
};

bool IsAllowed(ExecutionContext& context, bool is_eval, const String& source) {
  if (context.IsContextDestroyed()) {
    return false;
  }
  if (is_eval && !context.GetContentSecurityPolicy()->AllowEval(
                     ReportingDisposition::kReport,
                     ContentSecurityPolicy::kWillNotThrowException, source)) {
    return false;
  }
  if (auto* window = DynamicTo<LocalDOMWindow>(context);
      window && PageDismissalScope::IsActive()) {
    UseCounter::Count(window, window->document()->ProcessingBeforeUnload()
                                  ? WebFeature::kTimerInstallFromBeforeUnload
                                  : WebFeature::kTimerInstallFromUnload);
  }
  return true;
}

}  // namespace

int DOMTimer::setTimeout(ScriptState* script_state,
                         ExecutionContext& context,
                         V8Function* handler,
                         int timeout,
                         const HeapVector<ScriptValue>& arguments) {
  if (!IsAllowed(context, false, g_empty_string)) {
    return 0;
  }
  auto* action = MakeGarbageCollected<ScheduledAction>(script_state, context,
                                                       handler, arguments);
  return MakeGarbageCollected<DOMTimer>(context, action,
                                        base::Milliseconds(timeout), true)
      ->timeout_id_;
}

int DOMTimer::setTimeout(ScriptState* script_state,
                         ExecutionContext& context,
                         const String& handler,
                         int timeout,
                         const HeapVector<ScriptValue>&) {
  if (!IsAllowed(context, true, handler)) {
    return 0;
  }
  // Don't allow setting timeouts to run empty functions.  Was historically a
  // performance issue.
  if (handler.empty()) {
    return 0;
  }
  auto* action =
      MakeGarbageCollected<ScheduledAction>(script_state, context, handler);
  return MakeGarbageCollected<DOMTimer>(context, action,
                                        base::Milliseconds(timeout), true)
      ->timeout_id_;
}

int DOMTimer::setInterval(ScriptState* script_state,
                          ExecutionContext& context,
                          V8Function* handler,
                          int timeout,
                          const HeapVector<ScriptValue>& arguments) {
  if (!IsAllowed(context, false, g_empty_string)) {
    return 0;
  }
  auto* action = MakeGarbageCollected<ScheduledAction>(script_state, context,
                                                       handler, arguments);
  return MakeGarbageCollected<DOMTimer>(context, action,
                                        base::Milliseconds(timeout), false)
      ->timeout_id_;
}

int DOMTimer::setInterval(ScriptState* script_state,
                          ExecutionContext& context,
                          const String& handler,
                          int timeout,
                          const HeapVector<ScriptValue>&) {
  if (!IsAllowed(context, true, handler)) {
    return 0;
  }
  // Don't allow setting timeouts to run empty functions.  Was historically a
  // performance issue.
  if (handler.empty()) {
    return 0;
  }
  auto* action =
      MakeGarbageCollected<ScheduledAction>(script_state, context, handler);
  return MakeGarbageCollected<DOMTimer>(context, action,
                                        base::Milliseconds(timeout), false)
      ->timeout_id_;
}

void DOMTimer::clearTimeout(ExecutionContext& context, int timeout_id) {
  RemoveByID(context, timeout_id);
}

void DOMTimer::clearInterval(ExecutionContext& context, int timeout_id) {
  RemoveByID(context, timeout_id);
}

void DOMTimer::RemoveByID(ExecutionContext& context, int timeout_id) {
  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT(
      "TimerRemove", inspector_timer_remove_event::Data, &context, timeout_id);
  // Eagerly unregister as ExecutionContext observer.
  if (DOMTimer* timer =
          DOMTimerCoordinator::From(context).RemoveTimeoutByID(timeout_id)) {
    // Eagerly unregister as ExecutionContext observer.
    timer->SetExecutionContext(nullptr);
  }
}

DOMTimer::DOMTimer(ExecutionContext& context,
                   ScheduledAction* action,
                   base::TimeDelta timeout,
                   bool single_shot)
    : ExecutionContextLifecycleObserver(&context),
      TimerBase(nullptr),
      timeout_id_(DOMTimerCoordinator::From(context).Install(this)),
      // Step 9:
      nesting_level_(DOMTimerCoordinator::From(context).TimerNestingLevel()),
      action_(action) {
  DCHECK_GT(timeout_id_, 0);

  // Step 10:
  if (timeout.is_negative()) {
    timeout = base::TimeDelta();
  }

  // Steps 12 and 13:
  // Note: The implementation increments the nesting level before using it to
  // adjust timeout, contrary to what the spec requires crbug.com/1108877.
  IncrementNestingLevel();

  // A timer with a long timeout probably doesn't need to run at a precise time,
  // so allow some leeway on it. On the other hand, a timer with a short timeout
  // may need to run on time to deliver the best user experience.
  // TODO(crbug.com/1153139): Remove IsAlignWakeUpsDisabledForProcess() in M121
  // once workaround is no longer needed by WebRTC apps.
  bool precise = (timeout < GetMaxHighResolutionInterval()) ||
                 scheduler::IsAlignWakeUpsDisabledForProcess();

  // Step 11:
  // Note: The implementation uses >= instead of >, contrary to what the spec
  // requires crbug.com/1108877.
  if (nesting_level_ >= kMaxTimerNestingLevel && timeout < kMinimumInterval) {
    timeout = kMinimumInterval;
  }

  // Select TaskType based on nesting level.
  TaskType task_type;
  if (nesting_level_ >= kMaxTimerNestingLevel) {
    task_type = TaskType::kJavascriptTimerDelayedHighNesting;
  } else if (timeout.is_zero()) {
    task_type = TaskType::kJavascriptTimerImmediate;
    DCHECK_LT(nesting_level_, kMaxTimerNestingLevel);
  } else {
    task_type = TaskType::kJavascriptTimerDelayedLowNesting;
  }
  MoveToNewTaskRunner(context.GetTaskRunner(task_type));

  // Clamping up to 1ms for historical reasons crbug.com/402694.
  // Removing clamp for single_shot behind a feature flag.
  if (!single_shot || !blink::features::IsSetTimeoutWithoutClampEnabled()) {
    timeout = std::max(timeout, base::Milliseconds(1));
  }

  if (single_shot) {
    StartOneShot(timeout, FROM_HERE, precise);
  } else {
    StartRepeating(timeout, FROM_HERE, precise);
  }

  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT(
      "TimerInstall", inspector_timer_install_event::Data, &context,
      timeout_id_, timeout, single_shot);
  const char* name = single_shot ? "setTimeout" : "setInterval";
  async_task_context_.Schedule(&context, name);
  probe::BreakableLocation(&context, name);
}

DOMTimer::~DOMTimer() = default;

void DOMTimer::Dispose() {
  Stop();
}

void DOMTimer::Stop() {
  if (!action_) {
    return;
  }

  async_task_context_.Cancel();
  const bool is_interval = !RepeatInterval().is_zero();
  probe::BreakableLocation(GetExecutionContext(),
                           is_interval ? "clearInterval" : "clearTimeout");

  // Need to release JS objects potentially protected by ScheduledAction
  // because they can form circular references back to the ExecutionContext
  // which will cause a memory leak.
  if (action_) {
    action_->Dispose();
  }
  action_ = nullptr;
  TimerBase::Stop();
}

void DOMTimer::ContextDestroyed() {
  Stop();
}

void DOMTimer::Fired() {
  ExecutionContext* context = GetExecutionContext();
  DCHECK(context);
  DOMTimerCoordinator::From(*context).SetTimerNestingLevel(nesting_level_);
  DCHECK(!context->IsContextPaused());
  // Only the first execution of a multi-shot timer should get an affirmative
  // user gesture indicator.

  DEVTOOLS_TIMELINE_TRACE_EVENT("TimerFire", inspector_timer_fire_event::Data,
                                context, timeout_id_);
  const bool is_interval = !RepeatInterval().is_zero();

  probe::UserCallback probe(context, is_interval ? "setInterval" : "setTimeout",
                            g_null_atom, true);
  probe::InvokeCallback invoke_probe(
      action_->GetScriptState(),
      is_interval ? "TimerHandler:setInterval" : "TimerHandler:setTimeout",
      action_->CallbackFunction());
  probe::AsyncTask async_task(context, &async_task_context_,
                              is_interval ? "fired" : nullptr);

  // Simple case for non-one-shot timers.
  if (IsActive()) {
    DCHECK(is_interval);

    // Steps 12 and 13:
    // Note: The implementation increments the nesting level before using it to
    // adjust timeout, contrary to what the spec requires crbug.com/1108877.
    IncrementNestingLevel();

    // Step 11:
    // Make adjustments when the nesting level becomes >= |kMaxNestingLevel|.
    // Note: The implementation uses >= instead of >, contrary to what the spec
    // requires crbug.com/1108877.
    if (nesting_level_ == kMaxTimerNestingLevel &&
        RepeatInterval() < kMinimumInterval) {
      AugmentRepeatInterval(kMinimumInterval - RepeatInterval());
    }
    if (nesting_level_ == kMaxTimerNestingLevel) {
      // Move to the TaskType that corresponds to nesting level >=
      // |kMaxNestingLevel|.
      MoveToNewTaskRunner(
          context->GetTaskRunner(TaskType::kJavascriptTimerDelayedHighNesting));
    }

    DCHECK(nesting_level_ < kMaxTimerNestingLevel ||
           RepeatInterval() >= kMinimumInterval);

    // No access to member variables after this point, it can delete the timer.
    action_->Execute(context);

    DOMTimerCoordinator::From(*context).SetTimerNestingLevel(0);

    return;
  }

  // Unregister the timer from ExecutionContext before executing the action
  // for one-shot timers.
  ScheduledAction* action = action_.Release();
  DOMTimerCoordinator::From(*context).RemoveTimeoutByID(timeout_id_);

  action->Execute(context);

  // Eagerly clear out |action|'s resources.
  action->Dispose();

  // ExecutionContext might be already gone when we executed action->execute().
  ExecutionContext* execution_context = GetExecutionContext();
  if (!execution_context) {
    return;
  }

  DOMTimerCoordinator::From(*execution_context).SetTimerNestingLevel(0);
  // Eagerly unregister as ExecutionContext observer.
  SetExecutionContext(nullptr);
}

void DOMTimer::Trace(Visitor* visitor) const {
  visitor->Trace(action_);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void DOMTimer::IncrementNestingLevel() {
  nesting_level_ = base::ClampAdd(nesting_level_, 1);
}

}  // namespace blink
