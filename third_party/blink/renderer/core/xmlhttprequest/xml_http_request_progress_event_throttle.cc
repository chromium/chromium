/*
 * Copyright (C) 2010 Julien Chaffraix <jchaffraix@webkit.org>  All right
 * reserved.
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/xmlhttprequest/xml_http_request_progress_event_throttle.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/progress_event.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/xmlhttprequest/xml_http_request.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

static constexpr base::TimeDelta kMinimumProgressEventDispatchingInterval =
    base::TimeDelta::FromMilliseconds(50);  // 50 ms per specification.

XMLHttpRequestProgressEventThrottle::DeferredEvent::DeferredEvent() {
  Clear();
}

void XMLHttpRequestProgressEventThrottle::DeferredEvent::Set(
    bool length_computable,
    uint64_t loaded,
    uint64_t total) {
  is_set_ = true;

  length_computable_ = length_computable;
  loaded_ = loaded;
  total_ = total;
}

void XMLHttpRequestProgressEventThrottle::DeferredEvent::Clear() {
  is_set_ = false;

  length_computable_ = false;
  loaded_ = 0;
  total_ = 0;
}

Event* XMLHttpRequestProgressEventThrottle::DeferredEvent::Take() {
  DCHECK(is_set_);

  Event* event = ProgressEvent::Create(event_type_names::kProgress,
                                       length_computable_, loaded_, total_);
  Clear();
  return event;
}

XMLHttpRequestProgressEventThrottle::XMLHttpRequestProgressEventThrottle(
    XMLHttpRequest* target)
    : TimerBase(
          target->GetExecutionContext()->GetTaskRunner(TaskType::kNetworking)),
      target_(target),
      has_dispatched_progress_progress_event_(false) {
  DCHECK(target);
}

XMLHttpRequestProgressEventThrottle::~XMLHttpRequestProgressEventThrottle() =
    default;

void XMLHttpRequestProgressEventThrottle::DispatchProgressEvent(
    const AtomicString& type,
    bool length_computable,
    uint64_t loaded,
    uint64_t total) {
  // Given that ResourceDispatcher doesn't deliver an event when suspended,
  // we don't have to worry about event dispatching while suspended.
  if (type != event_type_names::kProgress) {
    target_->DispatchEvent(
        *ProgressEvent::Create(type, length_computable, loaded, total));
    return;
  }

  if (IsActive()) {
    deferred_.Set(length_computable, loaded, total);
  } else {
    DispatchProgressProgressEvent(ProgressEvent::Create(
        event_type_names::kProgress, length_computable, loaded, total));
    StartOneShot(kMinimumProgressEventDispatchingInterval, FROM_HERE);
  }
}

void XMLHttpRequestProgressEventThrottle::DispatchReadyStateChangeEvent(
    Event* event,
    DeferredEventAction action) {
  XMLHttpRequest::State state = target_->readyState();
  // Given that ResourceDispatcher doesn't deliver an event when suspended,
  // we don't have to worry about event dispatching while suspended.
  if (action == kFlush) {
    if (deferred_.IsSet())
      DispatchProgressProgressEvent(deferred_.Take());

    Stop();
  } else if (action == kClear) {
    deferred_.Clear();
    Stop();
  }

  has_dispatched_progress_progress_event_ = false;
  if (state == target_->readyState()) {
    // We don't dispatch the event when an event handler associated with
    // the previously dispatched event changes the readyState (e.g. when
    // the event handler calls xhr.abort()). In such cases a
    // readystatechange should have been already dispatched if necessary.
    probe::AsyncTask async_task(target_->GetExecutionContext(),
                                target_->async_task_id(), "progress",
                                target_->IsAsync());
    target_->DispatchEvent(*event);
  }
}

void XMLHttpRequestProgressEventThrottle::DispatchProgressProgressEvent(
    Event* progress_event) {
  XMLHttpRequest::State state = target_->readyState();
  if (target_->readyState() == XMLHttpRequest::kLoading &&
      has_dispatched_progress_progress_event_) {
    TRACE_EVENT1("devtools.timeline", "XHRReadyStateChange", "data",
                 inspector_xhr_ready_state_change_event::Data(
                     target_->GetExecutionContext(), target_));
    probe::AsyncTask async_task(target_->GetExecutionContext(),
                                target_->async_task_id(), "progress",
                                target_->IsAsync());
    target_->DispatchEvent(*Event::Create(event_type_names::kReadystatechange));
  }

  if (target_->readyState() != state)
    return;

  has_dispatched_progress_progress_event_ = true;
  probe::AsyncTask async_task(target_->GetExecutionContext(),
                              target_->async_task_id(), "progress",
                              target_->IsAsync());
  target_->DispatchEvent(*progress_event);
}

void XMLHttpRequestProgressEventThrottle::Fired() {
  if (!deferred_.IsSet()) {
    // No "progress" event was queued since the previous dispatch, we can
    // safely stop the timer.
    return;
  }

  DispatchProgressProgressEvent(deferred_.Take());

  // Watch if another "progress" ProgressEvent arrives in the next 50ms.
  StartOneShot(kMinimumProgressEventDispatchingInterval, FROM_HERE);
}

void XMLHttpRequestProgressEventThrottle::Trace(blink::Visitor* visitor) {
  visitor->Trace(target_);
}

}  // namespace blink
