// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/synthetic_gesture_controller.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "content/common/input/synthetic_gesture_target.h"

namespace content {

SyntheticGestureController::SyntheticGestureController(
    Delegate* delegate,
    std::unique_ptr<SyntheticGestureTarget> gesture_target,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : delegate_(delegate), gesture_target_(std::move(gesture_target)) {
  DCHECK(delegate_);
  dispatch_timer_.SetTaskRunner(task_runner);
}

SyntheticGestureController::~SyntheticGestureController() {
  while (!pending_gesture_queue_.IsEmpty()) {
    pending_gesture_queue_.FrontCallback().Run(
        SyntheticGesture::GESTURE_FINISHED);
    pending_gesture_queue_.Pop();
  }
}

void SyntheticGestureController::EnsureRendererInitialized(
    base::OnceClosure on_completed) {
  if (renderer_known_to_be_initialized_)
    return;

  base::OnceClosure wrapper = base::BindOnce(
      [](base::WeakPtr<SyntheticGestureController> weak_ptr,
         base::OnceClosure on_completed) {
        if (weak_ptr)
          weak_ptr->renderer_known_to_be_initialized_ = true;

        std::move(on_completed).Run();
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(on_completed));

  // TODO(bokan): This will wait for the renderer to produce a frame and the
  // GPU to present it but we should really be waiting for hit testing data to
  // have been updated in the browser. https://crbug.com/985374.
  gesture_target_->WaitForTargetAck(
      SyntheticGestureParams::WAIT_FOR_INPUT_PROCESSED,
      content::mojom::GestureSourceType::kDefaultInput, std::move(wrapper));
}

bool SyntheticGestureController::IsHiddenAndNeedsVisible() const {
  CHECK(!pending_gesture_queue_.IsEmpty());

  // If the gesture is from DevTools it'll be injected directly to the correct
  // renderer so it doesn't need to be visible.
  const SyntheticGesture& gesture = *pending_gesture_queue_.FrontGesture();
  if (gesture.IsFromDevToolsDebugger()) {
    return false;
  }

  // Other gestures inject events at the UI layer so require the source
  // RenderWidgetHost to be visible to be correctly targeted.
  return delegate_->IsHidden();
}

void SyntheticGestureController::StartIfNeeded() {
  if (!deferred_start_) {
    return;
  }
  deferred_start_ = false;

  CHECK(!pending_gesture_queue_.IsEmpty());
  CHECK(!dispatch_timer_.IsRunning());

  StartGesture();
}

void SyntheticGestureController::QueueSyntheticGesture(
    std::unique_ptr<SyntheticGesture> synthetic_gesture,
    OnGestureCompleteCallback completion_callback) {
  QueueSyntheticGesture(std::move(synthetic_gesture),
                        std::move(completion_callback), false);
}

void SyntheticGestureController::QueueSyntheticGestureCompleteImmediately(
    std::unique_ptr<SyntheticGesture> synthetic_gesture) {
  QueueSyntheticGesture(std::move(synthetic_gesture), base::DoNothing(), true);
}

void SyntheticGestureController::QueueSyntheticGesture(
    std::unique_ptr<SyntheticGesture> synthetic_gesture,
    OnGestureCompleteCallback completion_callback,
    bool complete_immediately) {
  DCHECK(synthetic_gesture);

  bool was_empty = pending_gesture_queue_.IsEmpty();

  SyntheticGesture* raw_gesture = synthetic_gesture.get();

  pending_gesture_queue_.Push(std::move(synthetic_gesture),
                              std::move(completion_callback),
                              complete_immediately);

  raw_gesture->DidQueue(weak_ptr_factory_.GetWeakPtr());

  if (was_empty) {
    StartGesture();
  }
}

void SyntheticGestureController::StartOrUpdateTimer() {
  base::TimeTicks vsync_timebase;
  base::TimeDelta vsync_interval;
  const SyntheticGesture& gesture = *pending_gesture_queue_.FrontGesture();

  gesture_target_->GetVSyncParameters(vsync_timebase, vsync_interval);
  event_timebase_ =
      vsync_timebase + base::Milliseconds(gesture.GetVsyncOffsetMs());
  switch (gesture.InputEventPattern()) {
    case content::mojom::InputEventPattern::kDefaultPattern:
      event_interval_ =
          vsync_interval * (gesture.AllowHighFrequencyDispatch() ? 0.5f : 1.0f);
      break;
    case content::mojom::InputEventPattern::kOnePerVsync:
      event_interval_ = vsync_interval;
      break;
    case content::mojom::InputEventPattern::kTwoPerVsync:
      event_interval_ = vsync_interval * 0.5f;
      break;
    case content::mojom::InputEventPattern::kEveryOtherVsync:
      event_interval_ = vsync_interval * 2.0f;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  if (dispatch_timer_.IsRunning()) {
    dispatch_timer_.Stop();
  }
  // The next deadline is scheduled at the next aligned time which is at least
  // `interval_ / 2` after now. `interval_ / 2` is added to avoid playing
  // "catch-up" if wake ups are late.
  base::TimeTicks deadline =
      (base::TimeTicks::Now() + event_interval_ / 2)
          .SnappedToNextTick(event_timebase_, event_interval_);

  dispatch_timer_.Start(
      FROM_HERE, deadline,
      base::BindRepeating(
          [](base::WeakPtr<SyntheticGestureController> weak_ptr) {
            if (weak_ptr)
              weak_ptr->DispatchNextEvent(base::TimeTicks::Now());
          },
          weak_ptr_factory_.GetWeakPtr()),
      base::subtle::DelayPolicy::kPrecise);
}

bool SyntheticGestureController::DispatchNextEvent(base::TimeTicks timestamp) {
  TRACE_EVENT0("input", "SyntheticGestureController::Flush");
  if (pending_gesture_queue_.IsEmpty()) {
    return false;
  }

  if (IsHiddenAndNeedsVisible()) {
    dispatch_timer_.Stop();
    GestureCompleted(SyntheticGesture::GESTURE_ABORT);
    return false;
  }

  if (!pending_gesture_queue_.is_current_gesture_complete()) {
    SyntheticGesture::Result result =
        pending_gesture_queue_.FrontGesture()->ForwardInputEvents(
            timestamp, gesture_target_.get());

    if (result == SyntheticGesture::GESTURE_ABORT) {
      // This means we've been destroyed from the call to ForwardInputEvents,
      // return immediately.
      return false;
    } else if (result == SyntheticGesture::GESTURE_RUNNING) {
      StartOrUpdateTimer();
      return true;
    }
    pending_gesture_queue_.mark_current_gesture_complete(result);
  }

  if (!pending_gesture_queue_.CompleteCurrentGestureImmediately() &&
      !delegate_->HasGestureStopped()) {
    StartOrUpdateTimer();
    return true;
  }

  StopGesture(*pending_gesture_queue_.FrontGesture(),
              pending_gesture_queue_.current_gesture_result(),
              pending_gesture_queue_.CompleteCurrentGestureImmediately());

  return !pending_gesture_queue_.IsEmpty();
}

void SyntheticGestureController::StartGesture() {
  if (!renderer_known_to_be_initialized_) {
    base::OnceClosure on_initialized = base::BindOnce(
        [](base::WeakPtr<SyntheticGestureController> weak_ptr) {
          if (!weak_ptr)
            return;

          // The renderer_known_to_be_initialized_ bit should be flipped before
          // this callback is invoked in EnsureRendererInitialized so we don't
          // call EnsureRendererInitialized again.
          DCHECK(weak_ptr->renderer_known_to_be_initialized_);
          weak_ptr->StartGesture();
        },
        weak_ptr_factory_.GetWeakPtr());

    // We don't yet know whether the renderer is ready for input. Force it to
    // produce a compositor frame and once it does we'll callback into this
    // function to start the gesture.
    EnsureRendererInitialized(std::move(on_initialized));
    return;
  }

  if (IsHiddenAndNeedsVisible()) {
    // If the gesture is started while the host is hidden, wait until the host
    // becomes visible.  Once the gesture is running, hiding the host will
    // abort the gesture but starting one while hidden will queue it so that
    // tests don't race with the visibility signal.
    deferred_start_ = true;
    return;
  }

  {
    DCHECK(!pending_gesture_queue_.IsEmpty());
    TRACE_EVENT_ASYNC_BEGIN0("input,benchmark",
                             "SyntheticGestureController::running",
                             pending_gesture_queue_.FrontGesture());
    StartOrUpdateTimer();
  }
}

void SyntheticGestureController::StopGesture(const SyntheticGesture& gesture,
                                             SyntheticGesture::Result result,
                                             bool complete_immediately) {
  DCHECK_NE(result, SyntheticGesture::GESTURE_RUNNING);
  TRACE_EVENT_ASYNC_END0("input,benchmark",
                         "SyntheticGestureController::running",
                         &gesture);

  dispatch_timer_.Stop();

  if (result != SyntheticGesture::GESTURE_FINISHED || complete_immediately) {
    GestureCompleted(result);
    return;
  }

  // If the gesture finished successfully, wait until all the input has been
  // propagated throughout the entire input pipeline before we resolve the
  // completion callback. This ensures all the effects of this gesture are
  // visible to subsequent input (e.g. OOPIF hit testing).
  gesture.WaitForTargetAck(
      base::BindOnce(&SyntheticGestureController::GestureCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     SyntheticGesture::GESTURE_FINISHED),
      gesture_target_.get());
}

void SyntheticGestureController::GestureCompleted(
    SyntheticGesture::Result result) {
  pending_gesture_queue_.FrontCallback().Run(result);
  pending_gesture_queue_.Pop();
  if (!pending_gesture_queue_.IsEmpty())
    StartGesture();
}

SyntheticGestureController::GestureAndCallbackQueue::GestureAndCallbackQueue() {
}

SyntheticGestureController::GestureAndCallbackQueue::
    ~GestureAndCallbackQueue() {
}

}  // namespace content
