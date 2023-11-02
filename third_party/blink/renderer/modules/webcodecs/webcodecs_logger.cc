// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/webcodecs_logger.h"

#include "third_party/blink/renderer/core/inspector/console_message.h"

namespace blink {

// How frequently we check for leaks.
constexpr base::TimeDelta kTimerInterval = base::Seconds(10);

// How long we wait before stopping the timer when there is no activity.
constexpr base::TimeDelta kTimerShutdownDelay = base::Seconds(60);

void WebCodecsLogger::VideoFrameCloseAuditor::ReportUnclosedFrame() {
  were_frames_not_closed_ = true;
}

void WebCodecsLogger::VideoFrameCloseAuditor::Clear() {
  were_frames_not_closed_ = false;
}

WebCodecsLogger::WebCodecsLogger(ExecutionContext& context)
    : Supplement<ExecutionContext>(context),
      close_auditor_(base::MakeRefCounted<VideoFrameCloseAuditor>()),
      timer_(context.GetTaskRunner(TaskType::kInternalMedia),
             this,
             &WebCodecsLogger::LogCloseErrors) {}

// static
WebCodecsLogger& WebCodecsLogger::From(ExecutionContext& context) {
  WebCodecsLogger* supplement =
      Supplement<ExecutionContext>::From<WebCodecsLogger>(context);
  if (!supplement) {
    supplement = MakeGarbageCollected<WebCodecsLogger>(context);
    Supplement<ExecutionContext>::ProvideTo(context, supplement);
  }

  return *supplement;
}

scoped_refptr<WebCodecsLogger::VideoFrameCloseAuditor>
WebCodecsLogger::GetCloseAuditor() {
  // We cannot directly log close errors: they are detected during garbage
  // collection, and it would be unsafe to access GC'ed objects from a GC'ed
  // object's destructor. Instead, start a timer here to periodically poll for
  // these errors. The timer should stop itself after a period of inactivity.
  if (!timer_.IsActive())
    timer_.StartRepeating(kTimerInterval, FROM_HERE);

  last_auditor_access_ = base::TimeTicks::Now();

  return close_auditor_;
}

void WebCodecsLogger::LogCloseErrors(TimerBase*) {
  // If it's been a while since this class was used and there are not other
  // references to |leak_status_|, stop the timer.
  if (base::TimeTicks::Now() - last_auditor_access_ > kTimerShutdownDelay &&
      close_auditor_->HasOneRef()) {
    timer_.Stop();
  }

  if (!close_auditor_->were_frames_not_closed())
    return;

  auto* execution_context = GetSupplementable();
  if (!execution_context->IsContextDestroyed()) {
    execution_context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kJavaScript,
        mojom::blink::ConsoleMessageLevel::kError,
        "A VideoFrame was garbage collected without being closed. "
        "Applications should call close() on frames when done with them to "
        "prevent stalls."));
  }

  close_auditor_->Clear();
}

void WebCodecsLogger::Trace(Visitor* visitor) const {
  visitor->Trace(timer_);
  Supplement<ExecutionContext>::Trace(visitor);
}

// static
const char WebCodecsLogger::kSupplementName[] = "WebCodecsLogger";

}  // namespace blink
