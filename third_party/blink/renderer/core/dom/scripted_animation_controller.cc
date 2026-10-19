/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
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
 *  THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "third_party/blink/renderer/core/dom/scripted_animation_controller.h"

#include <algorithm>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/css/media_query_list_listener.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

namespace {

bool CompareAnimationEvents(const Member<Event>& event1,
                            const Member<Event>& event2) {
  const Animation* animation1 = event1->GetAnimation();
  const Animation* animation2 = event2->GetAnimation();
  if (animation1 == animation2) {
    return false;
  }
  if (!animation1 || !animation2) {
    return !animation1;
  }
  return Animation::HasLowerCompositeOrdering(
      animation1, animation2,
      Animation::CompareAnimationsOrdering::kPointerOrder);
}

}  // namespace

bool ScriptedAnimationController::InsertToPerFrameEventsMap(
    const Event* event) {
  HashSet<const StringImpl*>& set =
      per_frame_events_.insert(event->RawTarget(), HashSet<const StringImpl*>())
          .stored_value->value;
  return set.insert(event->type().Impl()).is_new_entry;
}

void ScriptedAnimationController::EraseFromPerFrameEventsMap(
    const Event* event) {
  EventTarget* target = event->RawTarget();
  PerFrameEventsMap::iterator it = per_frame_events_.find(target);
  if (it != per_frame_events_.end()) {
    HashSet<const StringImpl*>& set = it->value;
    set.erase(event->type().Impl());
    if (set.empty())
      per_frame_events_.erase(target);
  }
}

ScriptedAnimationController::ScriptedAnimationController(LocalDOMWindow* window)
    : ExecutionContextLifecycleStateObserver(window),
      callback_collection_(window) {
  UpdateStateIfNeeded();
}

void ScriptedAnimationController::Trace(Visitor* visitor) const {
  ExecutionContextLifecycleStateObserver::Trace(visitor);
  visitor->Trace(callback_collection_);
  visitor->Trace(event_queue_);
  visitor->Trace(media_query_list_listeners_);
  visitor->Trace(media_query_list_listeners_set_);
  visitor->Trace(per_frame_events_);
}

void ScriptedAnimationController::ContextLifecycleStateChanged(
    mojom::FrameLifecycleState state) {
  if (state == mojom::FrameLifecycleState::kRunning)
    ScheduleAnimationIfNeeded();
}

void ScriptedAnimationController::DispatchMediaQueryListEventsAndCallbacks() {
  DispatchEvents(BindRepeating([](Event* event) {
    return event->InterfaceName() ==
           event_interface_names::kMediaQueryListEvent;
  }));
  CallMediaQueryListListeners();
}

void ScriptedAnimationController::ScheduleVideoFrameCallbacksExecution(
    ExecuteVfcCallback execute_vfc_callback) {
  vfc_execution_queue_.push_back(std::move(execute_vfc_callback));
  ScheduleAnimationIfNeeded(cc::BeginMainFrameReason::kVideoFrameCallback);
}

ScriptedAnimationController::CallbackId
ScriptedAnimationController::RegisterFrameCallback(FrameCallback* callback,
                                                   FrameCallbackType type) {
  // If we no longer have a context, there is no need to register the callback.
  if (!GetExecutionContext()) {
    return 0;
  }
  CallbackId id = callback_collection_.RegisterFrameCallback(callback, type);
  ScheduleAnimationIfNeeded(cc::BeginMainFrameReason::kRAF);
  return id;
}

void ScriptedAnimationController::CancelFrameCallback(CallbackId id,
                                                      FrameCallbackType type) {
  callback_collection_.CancelFrameCallback(id, type);
}

bool ScriptedAnimationController::HasFrameCallback() const {
  return callback_collection_.HasFrameCallback() ||
         !vfc_execution_queue_.empty();
}

void ScriptedAnimationController::RunTasks() {
  Vector<base::OnceClosure> tasks;
  tasks.swap(task_queue_);
  for (auto& task : tasks)
    std::move(task).Run();
}

bool ScriptedAnimationController::DispatchEvents(DispatchFilter filter) {
  HeapVector<Member<Event>> events;
  if (filter.is_null()) {
    events.swap(event_queue_);
    per_frame_events_.clear();
  } else {
    HeapVector<Member<Event>> remaining;
    for (auto& event : event_queue_) {
      if (event && filter.Run(event)) {
        EraseFromPerFrameEventsMap(event.Get());
        events.push_back(event.Release());
      } else {
        remaining.push_back(event.Release());
      }
    }
    remaining.swap(event_queue_);
  }

  bool did_dispatch = false;

  for (const auto& event : events) {
    did_dispatch = true;
    EventTarget* event_target = event->RawTarget();
    // FIXME: we should figure out how to make dispatchEvent properly virtual to
    // avoid special casting window.
    // FIXME: We should not fire events for nodes that are no longer in the
    // tree.
    probe::AsyncTask async_task(event_target->GetExecutionContext(),
                                event->async_task_context());
    if (LocalDOMWindow* window = event_target->ToLocalDOMWindow())
      window->DispatchEvent(*event, nullptr);
    else
      event_target->DispatchEvent(*event);
  }

  return did_dispatch;
}

void ScriptedAnimationController::ExecuteVideoFrameCallbacks() {
  // dispatchEvents() runs script which can cause the context to be destroyed.
  if (!GetExecutionContext())
    return;

  Vector<ExecuteVfcCallback> execute_vfc_callbacks;
  vfc_execution_queue_.swap(execute_vfc_callbacks);
  for (auto& callback : execute_vfc_callbacks)
    std::move(callback).Run(current_frame_time_ms_);
}

void ScriptedAnimationController::ExecuteFrameCallbacks() {
  // dispatchEvents() runs script which can cause the context to be destroyed.
  if (!GetExecutionContext())
    return;

  callback_collection_.ExecuteFrameCallbacks(current_frame_time_ms_,
                                             current_frame_legacy_time_ms_);
}

void ScriptedAnimationController::CallMediaQueryListListeners() {
  MediaQueryListListeners listeners;
  listeners.swap(media_query_list_listeners_);
  media_query_list_listeners_set_.clear();

  for (const auto& listener : listeners) {
    listener->NotifyMediaQueryChanged();
  }
}

bool ScriptedAnimationController::HasScheduledFrameTasks() const {
  return callback_collection_.HasFrameCallback() || !task_queue_.empty() ||
         !event_queue_.empty() || !media_query_list_listeners_.empty() ||
         GetWindow()->document()->HasAutofocusCandidates() ||
         !vfc_execution_queue_.empty();
}

PageAnimator* ScriptedAnimationController::GetPageAnimator() {
  if (GetWindow()->document() && GetWindow()->document()->GetPage())
    return &(GetWindow()->document()->GetPage()->Animator());
  return nullptr;
}

void ScriptedAnimationController::EnqueueTask(base::OnceClosure task) {
  task_queue_.push_back(std::move(task));
  ScheduleAnimationIfNeeded();
}

void ScriptedAnimationController::EnqueueEvent(Event* event) {
  event->async_task_context()->Schedule(
      event->RawTarget()->GetExecutionContext(), event->type());
  event_queue_.push_back(event);
  ScheduleAnimationIfNeeded();
}

void ScriptedAnimationController::EnqueuePerFrameEvent(Event* event) {
  if (!InsertToPerFrameEventsMap(event))
    return;
  EnqueueEvent(event);
}

void ScriptedAnimationController::SortAnimationEvents(wtf_size_t start_index) {
  if (event_queue_.size() <= start_index + 1 ||
      !base::FeatureList::IsEnabled(kSortAnimationEvents)) {
    return;
  }
  base::span<Member<Event>> events =
      base::span(event_queue_).subspan(start_index);
  // During the microtask checkpoint in
  // DocumentAnimations::UpdateAnimationTimingForAnimationFrame(),
  // AsyncFinishMicrotasks run first and enqueue `finish` events, followed by
  // RemoveReplacedAnimation microtasks (which enqueue `remove` events that are
  // already sorted in composite order) and any user `finished.then()` callbacks
  // (which may synchronously call `cancel()`). Stop before `remove` and
  // `cancel` events so they are not reordered ahead of `finish` events.
  auto end = std::find_if(events.begin(), events.end(), [](const Event* event) {
    const AtomicString& type = event->type();
    return type == event_type_names::kRemove ||
           type == event_type_names::kCancel ||
           type == event_type_names::kAnimationcancel ||
           type == event_type_names::kTransitioncancel;
  });
  if (end - events.begin() > 1) {
    std::stable_sort(events.begin(), end, CompareAnimationEvents);
  }
}

void ScriptedAnimationController::EnqueueMediaQueryChangeListeners(
    HeapVector<Member<MediaQueryListListener>>& listeners) {
  for (const auto& listener : listeners) {
    if (media_query_list_listeners_set_.insert(listener).is_new_entry) {
      media_query_list_listeners_.push_back(listener);
    }
  }
  DCHECK_EQ(media_query_list_listeners_.size(),
            media_query_list_listeners_set_.size());
  ScheduleAnimationIfNeeded();
}

void ScriptedAnimationController::ScheduleAnimationIfNeeded(
    cc::BeginMainFrameReason reason) {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextPaused())
    return;

  auto* frame = GetWindow()->GetFrame();
  if (!frame)
    return;

  if (HasScheduledFrameTasks()) {
    frame->View()->ScheduleAnimation(reason);
    return;
  }
}

LocalDOMWindow* ScriptedAnimationController::GetWindow() const {
  return To<LocalDOMWindow>(GetExecutionContext());
}

}  // namespace blink
