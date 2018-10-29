// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_CONTROLLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_CONTROLLER_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/common/content_export.h"
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
  };
  SyntheticGestureController(
      Delegate* delegate,
      std::unique_ptr<SyntheticGestureTarget> gesture_target);
  virtual ~SyntheticGestureController();

  typedef base::OnceCallback<void(SyntheticGesture::Result)>
      OnGestureCompleteCallback;
  void QueueSyntheticGesture(
      std::unique_ptr<SyntheticGesture> synthetic_gesture,
      OnGestureCompleteCallback completion_callback);

  bool DispatchNextEvent(base::TimeTicks = base::TimeTicks::Now());

 private:
  friend class SyntheticGestureControllerTestBase;

  void StartTimer(bool high_frequency);
  void StartGesture(const SyntheticGesture& gesture);
  void StopGesture(const SyntheticGesture& gesture,
                   OnGestureCompleteCallback completion_callback,
                   SyntheticGesture::Result result);

  Delegate* const delegate_;
  std::unique_ptr<SyntheticGestureTarget> gesture_target_;

  // A queue of gesture/callback pairs.  Implemented as two queues to
  // simplify the ownership of SyntheticGesture pointers.
  class GestureAndCallbackQueue {
  public:
    GestureAndCallbackQueue();
    ~GestureAndCallbackQueue();
    void Push(std::unique_ptr<SyntheticGesture> gesture,
              OnGestureCompleteCallback callback) {
      gestures_.push_back(std::move(gesture));
      callbacks_.push(std::move(callback));
    }
    void Pop() {
      gestures_.erase(gestures_.begin());
      callbacks_.pop();
      result_of_current_gesture_ = SyntheticGesture::GESTURE_RUNNING;
    }
    SyntheticGesture* FrontGesture() { return gestures_.front().get(); }
    OnGestureCompleteCallback FrontCallback() {
      // TODO(dtapuska): This is odd moving the top callback. Pop really
      // should be rewritten to take two output parameters then we can
      // remove FrontGesture/FrontCallback.
      return std::move(callbacks_.front());
    }
    bool IsEmpty() const {
      CHECK(gestures_.empty() == callbacks_.empty());
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

    DISALLOW_COPY_AND_ASSIGN(GestureAndCallbackQueue);
  } pending_gesture_queue_;

  base::RepeatingTimer dispatch_timer_;
  base::WeakPtrFactory<SyntheticGestureController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyntheticGestureController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_CONTROLLER_H_
