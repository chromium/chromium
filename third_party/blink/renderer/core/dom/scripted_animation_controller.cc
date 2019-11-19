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

#include "third_party/blink/renderer/core/css/media_query_list_listener.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"

namespace blink {

std::pair<EventTarget*, StringImpl*> EventTargetKey(const Event* event) {
  return std::make_pair(event->target(), event->type().Impl());
}

ScriptedAnimationController::ScriptedAnimationController(Document* document)
    : document_(document), callback_collection_(document), suspend_count_(0) {}

void ScriptedAnimationController::Trace(Visitor* visitor) {
  visitor->Trace(document_);
  visitor->Trace(callback_collection_);
  visitor->Trace(event_queue_);
  visitor->Trace(media_query_list_listeners_);
  visitor->Trace(per_frame_events_);
}

void ScriptedAnimationController::Pause() {
  ++suspend_count_;
}

void ScriptedAnimationController::Unpause() {
  // It would be nice to put an DCHECK_GT(suspend_count_, 0) here, but in WK1
  // resume() can be called even when suspend hasn't (if a tab was created in
  // the background).
  if (suspend_count_ > 0)
    --suspend_count_;
  ScheduleAnimationIfNeeded();
}

void ScriptedAnimationController::DispatchEventsAndCallbacksForPrinting() {
  DispatchEvents(event_interface_names::kMediaQueryListEvent);
  CallMediaQueryListListeners();
}

ScriptedAnimationController::CallbackId
ScriptedAnimationController::RegisterFrameCallback(
    FrameRequestCallbackCollection::FrameCallback* callback) {
  CallbackId id = callback_collection_.RegisterFrameCallback(callback);
  ScheduleAnimationIfNeeded();
  return id;
}

void ScriptedAnimationController::CancelFrameCallback(CallbackId id) {
  callback_collection_.CancelFrameCallback(id);
}

bool ScriptedAnimationController::HasFrameCallback() const {
  return callback_collection_.HasFrameCallback();
}

ScriptedAnimationController::CallbackId
ScriptedAnimationController::RegisterPostFrameCallback(
    FrameRequestCallbackCollection::FrameCallback* callback) {
  CallbackId id = callback_collection_.RegisterPostFrameCallback(callback);
  ScheduleAnimationIfNeeded();
  return id;
}

void ScriptedAnimationController::CancelPostFrameCallback(CallbackId id) {
  callback_collection_.CancelPostFrameCallback(id);
}

void ScriptedAnimationController::RunTasks() {
  Vector<base::OnceClosure> tasks;
  tasks.swap(task_queue_);
  for (auto& task : tasks)
    std::move(task).Run();
}

void ScriptedAnimationController::DispatchEvents(
    const AtomicString& event_interface_filter) {
  HeapVector<Member<Event>> events;
  if (event_interface_filter.IsEmpty()) {
    events.swap(event_queue_);
    per_frame_events_.clear();
  } else {
    HeapVector<Member<Event>> remaining;
    for (auto& event : event_queue_) {
      if (event && event->InterfaceName() == event_interface_filter) {
        per_frame_events_.erase(EventTargetKey(event.Get()));
        events.push_back(event.Release());
      } else {
        remaining.push_back(event.Release());
      }
    }
    remaining.swap(event_queue_);
  }

  for (const auto& event : events) {
    EventTarget* event_target = event->target();
    // FIXME: we should figure out how to make dispatchEvent properly virtual to
    // avoid special casting window.
    // FIXME: We should not fire events for nodes that are no longer in the
    // tree.
    probe::AsyncTask async_task(event_target->GetExecutionContext(),
                                event->async_task_id());
    if (LocalDOMWindow* window = event_target->ToLocalDOMWindow())
      window->DispatchEvent(*event, nullptr);
    else
      event_target->DispatchEvent(*event);
  }
}

void ScriptedAnimationController::ExecuteFrameCallbacks() {
  // dispatchEvents() runs script which can cause the document to be destroyed.
  if (!document_)
    return;

  callback_collection_.ExecuteFrameCallbacks(current_frame_time_ms_,
                                             current_frame_legacy_time_ms_);
}

void ScriptedAnimationController::CallMediaQueryListListeners() {
  MediaQueryListListeners listeners;
  listeners.Swap(media_query_list_listeners_);

  for (const auto& listener : listeners) {
    listener->NotifyMediaQueryChanged();
  }
}

bool ScriptedAnimationController::HasScheduledFrameTasks() const {
  if (suspend_count_)
    return false;

  return callback_collection_.HasFrameCallback() || !task_queue_.IsEmpty() ||
         !event_queue_.IsEmpty() || !media_query_list_listeners_.IsEmpty() ||
         (document_ && document_->HasAutofocusCandidates());
}

void ScriptedAnimationController::ServiceScriptedAnimations(
    base::TimeTicks monotonic_time_now) {
  if (document_ && document_->Loader()) {
    current_frame_time_ms_ =
        document_->Loader()
            ->GetTiming()
            .MonotonicTimeToZeroBasedDocumentTime(monotonic_time_now)
            .InMillisecondsF();
    current_frame_legacy_time_ms_ =
        document_->Loader()
            ->GetTiming()
            .MonotonicTimeToPseudoWallTime(monotonic_time_now)
            .InMillisecondsF();
  }
  current_frame_had_raf_ = HasFrameCallback();

  if (!HasScheduledFrameTasks())
    return;

  // https://html.spec.whatwg.org/C/#update-the-rendering

  // 10.5. For each fully active Document in docs, flush autofocus
  // candidates for that Document if its browsing context is a top-level
  // browsing context.
  if (document_)
    document_->FlushAutofocusCandidates();

  // 10.8. For each fully active Document in docs, evaluate media
  // queries and report changes for that Document, passing in now as the
  // timestamp
  CallMediaQueryListListeners();

  // 10.6. For each fully active Document in docs, run the resize steps
  // for that Document, passing in now as the timestamp.
  // 10.7. For each fully active Document in docs, run the scroll steps
  // for that Document, passing in now as the timestamp.
  // 10.9. For each fully active Document in docs, update animations and
  // send events for that Document, passing in now as the timestamp.
  //
  // We share a single event queue for them.
  DispatchEvents();

  // 10.10. For each fully active Document in docs, run the fullscreen
  // steps for that Document, passing in now as the timestamp.
  RunTasks();

  // 10.11. For each fully active Document in docs, run the animation
  // frame callbacks for that Document, passing in now as the timestamp.
  ExecuteFrameCallbacks();
  next_frame_has_pending_raf_ = HasFrameCallback();

  // See LocalFrameView::RunPostLifecycleSteps() for 10.12.

  ScheduleAnimationIfNeeded();
}

void ScriptedAnimationController::RunPostFrameCallbacks() {
  if (!callback_collection_.HasPostFrameCallback())
    return;
  DCHECK(current_frame_time_ms_ > 0.);
  DCHECK(current_frame_legacy_time_ms_ > 0.);
  callback_collection_.ExecutePostFrameCallbacks(current_frame_time_ms_,
                                                 current_frame_legacy_time_ms_);
}

void ScriptedAnimationController::EnqueueTask(base::OnceClosure task) {
  task_queue_.push_back(std::move(task));
  ScheduleAnimationIfNeeded();
}

void ScriptedAnimationController::EnqueueEvent(Event* event) {
  probe::AsyncTaskScheduled(event->target()->GetExecutionContext(),
                            event->type(), event->async_task_id());
  event_queue_.push_back(event);
  ScheduleAnimationIfNeeded();
}

void ScriptedAnimationController::EnqueuePerFrameEvent(Event* event) {
  if (!per_frame_events_.insert(EventTargetKey(event)).is_new_entry)
    return;
  EnqueueEvent(event);
}

void ScriptedAnimationController::EnqueueMediaQueryChangeListeners(
    HeapVector<Member<MediaQueryListListener>>& listeners) {
  for (const auto& listener : listeners) {
    media_query_list_listeners_.insert(listener);
  }
  ScheduleAnimationIfNeeded();
}

void ScriptedAnimationController::ScheduleAnimationIfNeeded() {
  if (suspend_count_ || !document_)
    return;
  LocalFrameView* frame_view = document_->View();
  if (!frame_view)
    return;

  // If there is any pre-frame work to do, schedule an animation
  // unconditionally.
  if (HasScheduledFrameTasks()) {
    frame_view->ScheduleAnimation();
    return;
  }

  // If there is post-frame work to do, only schedule an animation if we're not
  // currently running one -- if we're currently running an animation, then any
  // scheduled post-frame tasks will get run at the end of the current frame, so
  // no need to schedule another one.
  if (!callback_collection_.HasPostFrameCallback())
    return;
  if (Page* page = document_->GetPage()) {
    if (!page->Animator().IsServicingAnimations())
      frame_view->ScheduleAnimation();
  }
}

}  // namespace blink
