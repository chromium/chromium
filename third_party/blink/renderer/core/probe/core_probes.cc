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

#include "base/trace_event/typed_macros.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/core_probes_inl.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/probe/async_task_context.h"
#include "third_party/blink/renderer/platform/bindings/thread_debugger.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {
namespace probe {

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
                     AsyncTaskContext* task_context,
                     const char* step,
                     bool enabled,
                     AdTrackingType ad_tracking_type)
    : debugger_(enabled && context ? ThreadDebugger::From(context->GetIsolate())
                                   : nullptr),
      task_context_(task_context),
      recurring_(step),
      ad_tracker_(enabled && ad_tracking_type == AdTrackingType::kReport
                      ? AdTracker::FromExecutionContext(context)
                      : nullptr) {
  // TODO(crbug.com/1275875): Verify that `task_context` was scheduled, but
  // not yet canceled. Currently we don't have enough confidence that such
  // a CHECK wouldn't break blink.

  TRACE_EVENT_BEGIN("blink", "AsyncTask Run",
                    perfetto::Flow::FromPointer(task_context));
  if (debugger_)
    debugger_->AsyncTaskStarted(task_context->Id());

  if (ad_tracker_)
    ad_tracker_->DidStartAsyncTask(task_context);
}

AsyncTask::~AsyncTask() {
  if (debugger_) {
    debugger_->AsyncTaskFinished(task_context_->Id());
    if (!recurring_)
      debugger_->AsyncTaskCanceled(task_context_->Id());
  }

  if (ad_tracker_)
    ad_tracker_->DidFinishAsyncTask(task_context_);

  TRACE_EVENT_END("blink");  // "AsyncTask Run"
}

CoreProbeSink* ToCoreProbeSink(OffscreenCanvas* offscreen_canvas) {
  return offscreen_canvas
             ? ToCoreProbeSink(offscreen_canvas->GetExecutionContext())
             : nullptr;
}

void AllAsyncTasksCanceled(ExecutionContext* context) {
  if (context) {
    if (ThreadDebugger* debugger = ThreadDebugger::From(context->GetIsolate()))
      debugger->AllAsyncTasksCanceled();
  }
}

}  // namespace probe
}  // namespace blink
