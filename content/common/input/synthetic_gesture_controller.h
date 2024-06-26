// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_SYNTHETIC_GESTURE_CONTROLLER_H_
#define CONTENT_COMMON_INPUT_SYNTHETIC_GESTURE_CONTROLLER_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_gesture.h"
#include "content/common/input/synthetic_gesture_params.h"

namespace content {

class SyntheticGestureTarget;

// Controls a synthetic gesture.
// Repeatedly invokes the gesture object's ForwardInputEvent method to send
// input events to the platform until the gesture has finished.
class CONTENT_EXPORT SyntheticGestureController {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Returns whether any gesture created by dispatched input events has
    // completed or not.
    virtual bool HasGestureStopped() = 0;

    virtual bool IsHidden() const = 0;
  };
  SyntheticGestureController(
      Delegate* delegate,
      std::unique_ptr<SyntheticGestureTarget> gesture_target,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  SyntheticGestureController(const SyntheticGestureController&) = delete;
  SyntheticGestureController& operator=(const SyntheticGestureController&) =
      delete;

  virtual ~SyntheticGestureController();

  typedef base::OnceCallback<void(SyntheticGesture::Result)>
      OnGestureCompleteCallback;
  void QueueSyntheticGesture(
      std::unique_ptr<SyntheticGesture> synthetic_gesture,
      OnGestureCompleteCallback completion_callback);

  // Like QueueSyntheticGesture, but the gesture is considered complete as soon
  // as the SyntheticGestureController is done dispatching the events.
  void QueueSyntheticGestureCompleteImmediately(
      std::unique_ptr<SyntheticGesture> synthetic_gesture);

  // Returns true if there are more gestures in the queue after processing the
  // current one.
  bool DispatchNextEvent(base::TimeTicks = base::TimeTicks::Now());

  void EnsureRendererInitialized(base::OnceClosure on_completed);

  // Returns true if the current gesture requires the RenderWidgetHost to be
  // visible in order to correctly dispatch events to it and the RWH is
  // currently hidden.
  bool IsHiddenAndNeedsVisible() const;

  // If a gesture start was deferred, this will start it.
  void StartIfNeeded();

  base::WeakPtr<SyntheticGestureController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  friend class SyntheticGestureControllerTestBase;

  void QueueSyntheticGesture(
      std::unique_ptr<SyntheticGesture> synthetic_gesture,
      OnGestureCompleteCallback completion_callback,
      bool complete_immediately);

  void StartOrUpdateTimer();
  void StartGesture();
  void StopGesture(const SyntheticGesture& gesture,
                   SyntheticGesture::Result result,
                   bool complete_immediately);
  void GestureCompleted(SyntheticGesture::Result result);
  void ResolveCompletionCallback();

  raw_ptr<Delegate> delegate_;
  std::unique_ptr<SyntheticGestureTarget> gesture_target_;

  // A queue of gesture/callback/bool tuples.  Implemented as multiple queues to
  // simplify the ownership of SyntheticGesture pointers.
  class GestureAndCallbackQueue {
  public:
    GestureAndCallbackQueue();

    GestureAndCallbackQueue(const GestureAndCallbackQueue&) = delete;
    GestureAndCallbackQueue& operator=(const GestureAndCallbackQueue&) = delete;

    ~GestureAndCallbackQueue();
    void Push(std::unique_ptr<SyntheticGesture> gesture,
              OnGestureCompleteCallback callback,
              bool complete_immediately) {
      gestures_.push_back(std::move(gesture));
      callbacks_.push(std::move(callback));
      complete_immediately_.push(complete_immediately);
    }
    void Pop() {
      gestures_.erase(gestures_.begin());
      callbacks_.pop();
      complete_immediately_.pop();
      result_of_current_gesture_ = SyntheticGesture::GESTURE_RUNNING;
    }
    SyntheticGesture* FrontGesture() { return gestures_.front().get(); }
    const SyntheticGesture* FrontGesture() const {
      return gestures_.front().get();
    }
    OnGestureCompleteCallback FrontCallback() {
      // TODO(dtapuska): This is odd moving the top callback. Pop really
      // should be rewritten to take two output parameters then we can
      // remove FrontGesture/FrontCallback.
      return std::move(callbacks_.front());
    }
    bool CompleteCurrentGestureImmediately() {
      return complete_immediately_.front();
    }
    bool IsEmpty() const {
      CHECK(gestures_.empty() == callbacks_.empty());
      CHECK(gestures_.empty() == complete_immediately_.empty());
      return gestures_.empty();
    }

    bool is_current_gesture_complete() const {
      return result_of_current_gesture_ != SyntheticGesture::GESTURE_RUNNING;
    }

    SyntheticGesture::Result current_gesture_result() const {
      return result_of_current_gesture_;
    }

    void mark_current_gesture_complete(SyntheticGesture::Result result) {
      result_of_current_gesture_ = result;
    }

   private:
    SyntheticGesture::Result result_of_current_gesture_ =
        SyntheticGesture::GESTURE_RUNNING;
    std::vector<std::unique_ptr<SyntheticGesture>> gestures_;
    base::queue<OnGestureCompleteCallback> callbacks_;
    base::queue<bool> complete_immediately_;
  } pending_gesture_queue_;

  // The first time we start sending a gesture, the renderer may not yet be
  // ready to receive events. e.g. Tests often start a gesture from script
  // before load. The renderer  may not yet have produced a compositor frame
  // and geometry data may not yet be available in the browser. The first time
  // we try to start a gesture, we'll first force a redraw in the renderer and
  // wait until it produces a compositor frame. The gesture will begin after
  // that happens.
  // TODO(bokan): The renderer currently just waits for a CompositorFrame to be
  // generated. We should be waiting for hit test data to be available to be
  // truly robust. https://crbug.com/985374.
  bool renderer_known_to_be_initialized_ = false;

  // Set when StartGesture was called but had to be deferred due to the
  // RenderWidgetHost being hidden. Deferred gestures will be started when the
  // widget becomes visible via StartIfNeeded().
  bool deferred_start_ = false;

  base::TimeTicks event_timebase_;
  base::TimeDelta event_interval_{base::Microseconds(8333)};

  base::DeadlineTimer dispatch_timer_;
  base::WeakPtrFactory<SyntheticGestureController> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_SYNTHETIC_GESTURE_CONTROLLER_H_
