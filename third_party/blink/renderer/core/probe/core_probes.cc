/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#include "third_party/blink/renderer/core/probe/core_probes.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/core_probes_inl.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/inspector/thread_debugger.h"
#include "third_party/blink/renderer/core/probe/async_task_id.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {
namespace probe {

namespace {
void* AsyncId(AsyncTaskId* task) {
  // Blink uses odd ids for network requests and even ids for everything else.
  // We should make all of them even before reporting to V8 to avoid collisions
  // with internal V8 async events.
  return reinterpret_cast<void*>(reinterpret_cast<intptr_t>(task) << 1);
}

void AsyncTaskCanceled(v8::Isolate* isolate, AsyncTaskId* task) {
  if (ThreadDebugger* debugger = ThreadDebugger::From(isolate))
    debugger->AsyncTaskCanceled(AsyncId(task));
  TRACE_EVENT_FLOW_END0("devtools.timeline.async", "AsyncTask",
                        TRACE_ID_LOCAL(reinterpret_cast<uintptr_t>(task)));
}

}  // namespace

base::TimeTicks ProbeBase::CaptureStartTime() const {
  if (start_time_.is_null())
    start_time_ = base::TimeTicks::Now();
  return start_time_;
}

base::TimeTicks ProbeBase::CaptureEndTime() const {
  if (end_time_.is_null())
    end_time_ = base::TimeTicks::Now();
  return end_time_;
}

base::TimeDelta ProbeBase::Duration() const {
  DCHECK(!start_time_.is_null());
  return CaptureEndTime() - start_time_;
}

AsyncTask::AsyncTask(ExecutionContext* context,
                     AsyncTaskId* task,
                     const char* step,
                     bool enabled)
    : debugger_(enabled && context ? ThreadDebugger::From(context->GetIsolate())
                                   : nullptr),
      task_(task),
      recurring_(step),
      ad_tracker_(AdTracker::FromExecutionContext(context)) {
  if (recurring_) {
    TRACE_EVENT_FLOW_STEP0("devtools.timeline.async", "AsyncTask",
                           TRACE_ID_LOCAL(reinterpret_cast<uintptr_t>(task)),
                           step ? step : "");
  } else {
    TRACE_EVENT_FLOW_END0("devtools.timeline.async", "AsyncTask",
                          TRACE_ID_LOCAL(reinterpret_cast<uintptr_t>(task)));
  }
  if (debugger_)
    debugger_->AsyncTaskStarted(AsyncId(task_));

  if (ad_tracker_)
    ad_tracker_->DidStartAsyncTask(task_);
}

AsyncTask::~AsyncTask() {
  if (debugger_) {
    debugger_->AsyncTaskFinished(AsyncId(task_));
    if (!recurring_)
      debugger_->AsyncTaskCanceled(AsyncId(task_));
  }

  if (ad_tracker_)
    ad_tracker_->DidFinishAsyncTask(task_);
}

void AsyncTaskScheduled(ExecutionContext* context,
                        const StringView& name,
                        AsyncTaskId* task) {
  TRACE_EVENT_FLOW_BEGIN1("devtools.timeline.async", "AsyncTask",
                          TRACE_ID_LOCAL(reinterpret_cast<uintptr_t>(task)),
                          "data", inspector_async_task::Data(name));
  if (!context)
    return;

  if (ThreadDebugger* debugger = ThreadDebugger::From(context->GetIsolate()))
    debugger->AsyncTaskScheduled(name, AsyncId(task), true);

  blink::AdTracker* ad_tracker = AdTracker::FromExecutionContext(context);
  if (ad_tracker)
    ad_tracker->DidCreateAsyncTask(task);
}

void AsyncTaskScheduledBreakable(ExecutionContext* context,
                                 const char* name,
                                 AsyncTaskId* task) {
  AsyncTaskScheduled(context, name, task);
  BreakableLocation(context, name);
}

void AsyncTaskCanceled(ExecutionContext* context, AsyncTaskId* task) {
  AsyncTaskCanceled(context ? context->GetIsolate() : nullptr, task);
}

void AsyncTaskCanceledBreakable(ExecutionContext* context,
                                const char* name,
                                AsyncTaskId* task) {
  AsyncTaskCanceled(context, task);
  BreakableLocation(context, name);
}

void AllAsyncTasksCanceled(ExecutionContext* context) {
  if (context) {
    if (ThreadDebugger* debugger = ThreadDebugger::From(context->GetIsolate()))
      debugger->AllAsyncTasksCanceled();
  }
}

}  // namespace probe
}  // namespace blink
