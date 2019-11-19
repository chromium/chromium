// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_USER_MODEL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_USER_MODEL_H_

#include "base/macros.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {
namespace scheduler {

class PLATFORM_EXPORT UserModel {
  USING_FAST_MALLOC(UserModel);

 public:
  UserModel();
  ~UserModel();

  // Tells us that the system started processing an input event. Must be paired
  // with a call to DidFinishProcessingInputEvent.
  void DidStartProcessingInputEvent(WebInputEvent::Type type,
                                    const base::TimeTicks now);

  // Tells us that the system finished processing an input event.
  void DidFinishProcessingInputEvent(const base::TimeTicks now);

  // Returns the estimated amount of time left in the current user gesture, to a
  // maximum of |kGestureEstimationLimitMillis|.  After that time has elapased
  // this function should be called again.
  base::TimeDelta TimeLeftInUserGesture(base::TimeTicks now) const;

  // Tries to guess if a user gesture is expected soon. Currently this is
  // very simple, but one day I hope to do something more sophisticated here.
  // The prediction may change after |prediction_valid_duration| has elapsed.
  bool IsGestureExpectedSoon(const base::TimeTicks now,
                             base::TimeDelta* prediction_valid_duration);

  // Returns true if a gesture has been in progress for less than the median
  // gesture duration. The prediction may change after
  // |prediction_valid_duration| has elapsed.
  bool IsGestureExpectedToContinue(
      const base::TimeTicks now,
      base::TimeDelta* prediction_valid_duration) const;

  void AsValueInto(base::trace_event::TracedValue* state) const;

  // The time we should stay in a priority-escalated mode after an input event.
  static const int kGestureEstimationLimitMillis = 100;

  // This is based on two weeks of Android usage data.
  static const int kMedianGestureDurationMillis = 300;

  // We consider further gesture start events to be likely if the user has
  // interacted with the device in the past two seconds.
  // Based on Android usage data, 2000ms between gestures is the 75th percentile
  // with 700ms being the 50th.
  static const int kExpectSubsequentGestureMillis = 2000;

  // Clears input signals.
  void Reset(base::TimeTicks now);

 private:
  bool IsGestureExpectedSoonImpl(
      const base::TimeTicks now,
      base::TimeDelta* prediction_valid_duration) const;

  int pending_input_event_count_;
  base::TimeTicks last_input_signal_time_;
  base::TimeTicks last_gesture_start_time_;
  base::TimeTicks last_continuous_gesture_time_;  // Doesn't include Taps.
  base::TimeTicks last_gesture_expected_start_time_;
  base::TimeTicks last_reset_time_;
  bool is_gesture_active_;  // This typically means the user's finger is down.
  bool is_gesture_expected_;

  DISALLOW_COPY_AND_ASSIGN(UserModel);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_USER_MODEL_H_
