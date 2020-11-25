// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_frame_logger.h"

#include "third_party/blink/renderer/core/inspector/console_message.h"

namespace blink {

// How frequently we check for leaks.
constexpr base::TimeDelta kTimerInterval = base::TimeDelta::FromSeconds(10);

// How long we wait before stopping the timer when there is no activity.
constexpr base::TimeDelta kTimerShutdownDelay =
    base::TimeDelta::FromSeconds(60);

void VideoFrameLogger::VideoFrameDestructionAuditor::ReportUndestroyedFrame() {
  were_frames_not_destroyed_ = true;
}

void VideoFrameLogger::VideoFrameDestructionAuditor::Clear() {
  were_frames_not_destroyed_ = false;
}

VideoFrameLogger::VideoFrameLogger(ExecutionContext& context)
    : Supplement<ExecutionContext>(context),
      destruction_auditor_(
          base::MakeRefCounted<VideoFrameDestructionAuditor>()) {
  timer_ = std::make_unique<TaskRunnerTimer<VideoFrameLogger>>(
      context.GetTaskRunner(TaskType::kInternalMedia), this,
      &VideoFrameLogger::LogDestructionErrors);
}

// static
VideoFrameLogger& VideoFrameLogger::From(ExecutionContext& context) {
  VideoFrameLogger* supplement =
      Supplement<ExecutionContext>::From<VideoFrameLogger>(context);
  if (!supplement) {
    supplement = MakeGarbageCollected<VideoFrameLogger>(context);
    Supplement<ExecutionContext>::ProvideTo(context, supplement);
  }

  return *supplement;
}

scoped_refptr<VideoFrameLogger::VideoFrameDestructionAuditor>
VideoFrameLogger::GetDestructionAuditor() {
  // We cannot directly log destruction errors: they are detected during
  // garbage collection, and it would be unsafe to access GC'ed objects from
  // a GC'ed object's destructor. Instead, start a timer here to periodically
  // poll for these errors. The timer should stop itself after a period of
  // inactivity.
  if (!timer_->IsActive())
    timer_->StartRepeating(kTimerInterval, FROM_HERE);

  last_auditor_access_ = base::TimeTicks::Now();

  return destruction_auditor_;
}

void VideoFrameLogger::LogDestructionErrors(TimerBase*) {
  // If it's been a while since this class was used and there are not other
  // references to |leak_status_|, stop the timer.
  if (base::TimeTicks::Now() - last_auditor_access_ > kTimerShutdownDelay &&
      destruction_auditor_->HasOneRef()) {
    timer_->Stop();
  }

  if (!destruction_auditor_->were_frames_not_destroyed())
    return;

  auto* execution_context = GetSupplementable();
  if (!execution_context->IsContextDestroyed()) {
    execution_context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kJavaScript,
        mojom::blink::ConsoleMessageLevel::kError,
        "A VideoFrame was garbage collected without being destroyed. "
        "Applications should call destroy() on frames when done with them to "
        "prevent stalls."));
  }

  destruction_auditor_->Clear();
}

void VideoFrameLogger::Trace(Visitor* visitor) const {
  Supplement<ExecutionContext>::Trace(visitor);
}

// static
const char VideoFrameLogger::kSupplementName[] = "VideoFrameLogger";

}  // namespace blink
