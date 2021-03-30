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

void VideoFrameLogger::VideoFrameCloseAuditor::ReportUnclosedFrame() {
  were_frames_not_closed_ = true;
}

void VideoFrameLogger::VideoFrameCloseAuditor::Clear() {
  were_frames_not_closed_ = false;
}

VideoFrameLogger::VideoFrameLogger(ExecutionContext& context)
    : Supplement<ExecutionContext>(context),
      close_auditor_(base::MakeRefCounted<VideoFrameCloseAuditor>()),
      timer_(context.GetTaskRunner(TaskType::kInternalMedia),
             this,
             &VideoFrameLogger::LogCloseErrors) {}

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

scoped_refptr<VideoFrameLogger::VideoFrameCloseAuditor>
VideoFrameLogger::GetCloseAuditor() {
  // We cannot directly log close errors: they are detected during garbage
  // collection, and it would be unsafe to access GC'ed objects from a GC'ed
  // object's destructor. Instead, start a timer here to periodically poll for
  // these errors. The timer should stop itself after a period of inactivity.
  if (!timer_.IsActive())
    timer_.StartRepeating(kTimerInterval, FROM_HERE);

  last_auditor_access_ = base::TimeTicks::Now();

  return close_auditor_;
}

void VideoFrameLogger::LogCreateImageBitmapDeprecationNotice() {
  if (already_logged_create_image_bitmap_deprecation_)
    return;

  already_logged_create_image_bitmap_deprecation_ = true;
  GetSupplementable()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kDeprecation,
      mojom::blink::ConsoleMessageLevel::kWarning,
      "VideoFrame.createImageBitmap() is deprecated; please use "
      "createImageBitmap(VideoFrame)."));
}

void VideoFrameLogger::LogCloseErrors(TimerBase*) {
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

void VideoFrameLogger::Trace(Visitor* visitor) const {
  visitor->Trace(timer_);
  Supplement<ExecutionContext>::Trace(visitor);
}

// static
const char VideoFrameLogger::kSupplementName[] = "VideoFrameLogger";

}  // namespace blink
