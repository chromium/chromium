// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediacapturefromelement/timed_canvas_draw_listener.h"

#include <memory>
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/mediacapturefromelement/canvas_capture_handler.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

TimedCanvasDrawListener::TimedCanvasDrawListener(
    std::unique_ptr<CanvasCaptureHandler> handler,
    double frame_rate,
    ExecutionContext* context)
    : OnRequestCanvasDrawListener(std::move(handler)),
      frame_interval_(base::Seconds(1 / frame_rate)),
      request_frame_timer_(context->GetTaskRunner(TaskType::kInternalMedia),
                           this,
                           &TimedCanvasDrawListener::RequestFrameTimerFired) {}

TimedCanvasDrawListener::~TimedCanvasDrawListener() = default;

// static
TimedCanvasDrawListener* TimedCanvasDrawListener::Create(
    std::unique_ptr<CanvasCaptureHandler> handler,
    double frame_rate,
    ExecutionContext* context) {
  TimedCanvasDrawListener* listener =
      MakeGarbageCollected<TimedCanvasDrawListener>(std::move(handler),
                                                    frame_rate, context);
  listener->request_frame_timer_.StartRepeating(listener->frame_interval_,
                                                FROM_HERE);
  return listener;
}

void TimedCanvasDrawListener::RequestFrameTimerFired(TimerBase*) {
  // TODO(emircan): Measure the jitter and log, see crbug.com/589974.
  frame_capture_requested_ = true;
}

void TimedCanvasDrawListener::Trace(Visitor* visitor) const {
  visitor->Trace(request_frame_timer_);
  OnRequestCanvasDrawListener::Trace(visitor);
}

}  // namespace blink
