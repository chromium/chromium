// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/dom_task_signal.h"

#include <utility>

#include "base/callback.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/scheduler/dom_scheduler.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_task_queue.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

DOMTaskSignal::DOMTaskSignal(ExecutionContext* context,
                             WebSchedulingPriority priority,
                             Type type)
    : AbortSignal(context),
      ExecutionContextLifecycleObserver(context),
      priority_(priority) {
  if (type == Type::kCreatedByController) {
    web_scheduling_task_queue_ = context->GetScheduler()
                                     ->ToFrameScheduler()
                                     ->CreateWebSchedulingTaskQueue(priority_);
  }
}

DOMTaskSignal::~DOMTaskSignal() = default;

AtomicString DOMTaskSignal::priority() {
  return WebSchedulingPriorityToString(priority_);
}

void DOMTaskSignal::ContextDestroyed() {
  web_scheduling_task_queue_.reset();
}

void DOMTaskSignal::SignalPriorityChange(WebSchedulingPriority priority) {
  if (priority_ == priority)
    return;
  priority_ = priority;
  if (web_scheduling_task_queue_)
    web_scheduling_task_queue_->SetPriority(priority);
  priority_change_status_ = PriorityChangeStatus::kPriorityHasChanged;
  DispatchEvent(*Event::Create(event_type_names::kPrioritychange), "DOMTaskSignal::SignalPriorityChange");
}

base::SingleThreadTaskRunner* DOMTaskSignal::GetTaskRunner() {
  auto* window = To<LocalDOMWindow>(
      ExecutionContextLifecycleObserver::GetExecutionContext());
  if (!window)
    return nullptr;
  if (web_scheduling_task_queue_)
    return web_scheduling_task_queue_->GetTaskRunner().get();
  return DOMScheduler::scheduler(*window)->GetTaskRunnerFor(priority_);
}

void DOMTaskSignal::Trace(Visitor* visitor) const {
  AbortSignal::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
