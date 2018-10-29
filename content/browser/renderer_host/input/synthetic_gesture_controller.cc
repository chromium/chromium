// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_gesture_controller.h"

#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/common/input_messages.h"
#include "content/public/browser/render_widget_host.h"

namespace content {

SyntheticGestureController::SyntheticGestureController(
    Delegate* delegate,
    std::unique_ptr<SyntheticGestureTarget> gesture_target)
    : delegate_(delegate),
      gesture_target_(std::move(gesture_target)),
      weak_ptr_factory_(this) {
  DCHECK(delegate_);
}

SyntheticGestureController::~SyntheticGestureController() {}

void SyntheticGestureController::QueueSyntheticGesture(
    std::unique_ptr<SyntheticGesture> synthetic_gesture,
    OnGestureCompleteCallback completion_callback) {
  DCHECK(synthetic_gesture);

  bool was_empty = pending_gesture_queue_.IsEmpty();

  pending_gesture_queue_.Push(std::move(synthetic_gesture),
                              std::move(completion_callback));

  if (was_empty)
    StartGesture(*pending_gesture_queue_.FrontGesture());
}

void SyntheticGestureController::StartTimer(bool high_frequency) {
  dispatch_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMicroseconds(high_frequency ? 8333 : 16666),
      base::BindRepeating(
          [](base::WeakPtr<SyntheticGestureController> weak_ptr) {
            if (weak_ptr)
              weak_ptr->DispatchNextEvent(base::TimeTicks::Now());
          },
          weak_ptr_factory_.GetWeakPtr()));
}

bool SyntheticGestureController::DispatchNextEvent(base::TimeTicks timestamp) {
  DCHECK(dispatch_timer_.IsRunning());
  TRACE_EVENT0("input", "SyntheticGestureController::Flush");
  if (pending_gesture_queue_.IsEmpty())
    return false;

  if (!pending_gesture_queue_.is_current_gesture_complete()) {
    SyntheticGesture::Result result =
        pending_gesture_queue_.FrontGesture()->ForwardInputEvents(
            timestamp, gesture_target_.get());

    if (result == SyntheticGesture::GESTURE_RUNNING) {
      return true;
    }
    pending_gesture_queue_.mark_current_gesture_complete(result);
  }

  if (!delegate_->HasGestureStopped())
    return true;

  StopGesture(*pending_gesture_queue_.FrontGesture(),
              pending_gesture_queue_.FrontCallback(),
              pending_gesture_queue_.current_gesture_result());
  pending_gesture_queue_.Pop();
  if (pending_gesture_queue_.IsEmpty()) {
    dispatch_timer_.Stop();
    return false;
  }
  StartGesture(*pending_gesture_queue_.FrontGesture());
  return true;
}

void SyntheticGestureController::StartGesture(const SyntheticGesture& gesture) {
  TRACE_EVENT_ASYNC_BEGIN0("input,benchmark",
                           "SyntheticGestureController::running",
                           &gesture);
  if (!dispatch_timer_.IsRunning())
    StartTimer(gesture.AllowHighFrequencyDispatch());
}

void SyntheticGestureController::StopGesture(
    const SyntheticGesture& gesture,
    OnGestureCompleteCallback completion_callback,
    SyntheticGesture::Result result) {
  DCHECK_NE(result, SyntheticGesture::GESTURE_RUNNING);
  TRACE_EVENT_ASYNC_END0("input,benchmark",
                         "SyntheticGestureController::running",
                         &gesture);

  std::move(completion_callback).Run(result);
}

SyntheticGestureController::GestureAndCallbackQueue::GestureAndCallbackQueue() {
}

SyntheticGestureController::GestureAndCallbackQueue::
    ~GestureAndCallbackQueue() {
}

}  // namespace content
