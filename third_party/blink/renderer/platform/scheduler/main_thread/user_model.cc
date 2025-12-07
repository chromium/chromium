// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/user_model.h"

#include "base/trace_event/trace_event.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"

namespace blink {
namespace scheduler {

UserModel::UserModel() = default;

void UserModel::DidStartProcessingInputEvent(blink::WebInputEvent::Type type,
                                             const base::TimeTicks now) {
  last_input_signal_time_ = now;
  if (type == blink::WebInputEvent::Type::kTouchStart ||
      type == blink::WebInputEvent::Type::kGestureScrollBegin ||
      type == blink::WebInputEvent::Type::kGesturePinchBegin) {
    // Only update stats once per gesture.
    if (!is_gesture_active_)
      last_gesture_start_time_ = now;

    is_gesture_active_ = true;
  }

  // We need to track continuous gestures seperatly for scroll detection
  // because taps should not be confused with scrolls.
  if (type == blink::WebInputEvent::Type::kGestureScrollBegin ||
      type == blink::WebInputEvent::Type::kGestureScrollEnd ||
      type == blink::WebInputEvent::Type::kGestureScrollUpdate ||
      type == blink::WebInputEvent::Type::kGestureFlingStart ||
      type == blink::WebInputEvent::Type::kGestureFlingCancel ||
      type == blink::WebInputEvent::Type::kGesturePinchBegin ||
      type == blink::WebInputEvent::Type::kGesturePinchEnd ||
      type == blink::WebInputEvent::Type::kGesturePinchUpdate) {
    last_continuous_gesture_time_ = now;
  }

  // If the gesture has ended, clear |is_gesture_active_| and record a UMA
  // metric that tracks its duration.
  if (type == blink::WebInputEvent::Type::kGestureScrollEnd ||
      type == blink::WebInputEvent::Type::kGesturePinchEnd ||
      type == blink::WebInputEvent::Type::kGestureFlingStart ||
      type == blink::WebInputEvent::Type::kTouchEnd) {
    is_gesture_active_ = false;
  }

  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "is_gesture_active", is_gesture_active_);

  pending_input_event_count_++;
}

void UserModel::DidFinishProcessingInputEvent(const base::TimeTicks now) {
  last_input_signal_time_ = now;
  if (pending_input_event_count_ > 0)
    pending_input_event_count_--;
}

void UserModel::DidProcessDiscreteInputEvent(const base::TimeTicks now) {
  last_discrete_input_time_ = now;
}

void UserModel::DidProcessDiscreteInputResponse() {
  last_discrete_input_time_ = base::TimeTicks();
}

base::TimeDelta UserModel::TimeLeftInContinuousUserGesture(
    base::TimeTicks now) const {
  // If the input event is still pending, go into input prioritized policy and
  // check again later.
  if (pending_input_event_count_ > 0) {
    return kGestureEstimationLimit;
  }
  if (last_input_signal_time_.is_null() ||
      last_input_signal_time_ + kGestureEstimationLimit < now) {
    return base::TimeDelta();
  }
  return last_input_signal_time_ + kGestureEstimationLimit - now;
}

base::TimeDelta UserModel::TimeLeftUntilDiscreteInputResponseDeadline(
    base::TimeTicks now) const {
  if (last_discrete_input_time_.is_null() ||
      last_discrete_input_time_ + kDiscreteInputResponseDeadline < now) {
    return base::TimeDelta();
  }
  return last_discrete_input_time_ + kDiscreteInputResponseDeadline - now;
}

bool UserModel::IsGestureExpectedSoon(
    const base::TimeTicks now,
    base::TimeDelta* prediction_valid_duration) {
  bool was_gesture_expected = is_gesture_expected_;
  is_gesture_expected_ =
      IsGestureExpectedSoonImpl(now, prediction_valid_duration);

  // Track when we start expecting a gesture so we can work out later if a
  // gesture actually happened.
  if (!was_gesture_expected && is_gesture_expected_)
    last_gesture_expected_start_time_ = now;
  return is_gesture_expected_;
}

bool UserModel::IsGestureExpectedSoonImpl(
    const base::TimeTicks now,
    base::TimeDelta* prediction_valid_duration) const {
  if (is_gesture_active_) {
    if (IsGestureExpectedToContinue(now, prediction_valid_duration))
      return false;
    *prediction_valid_duration = kExpectSubsequentGestureDeadline;
    return true;
  } else {
    // If we have finished a gesture then a subsequent gesture is deemed likely.
    if (last_continuous_gesture_time_.is_null() ||
        last_continuous_gesture_time_ + kExpectSubsequentGestureDeadline <=
            now) {
      return false;
    }
    *prediction_valid_duration =
        last_continuous_gesture_time_ + kExpectSubsequentGestureDeadline - now;
    return true;
  }
}

bool UserModel::IsGestureExpectedToContinue(
    const base::TimeTicks now,
    base::TimeDelta* prediction_valid_duration) const {
  if (!is_gesture_active_)
    return false;

  base::TimeTicks expected_gesture_end_time =
      last_gesture_start_time_ + kMedianGestureDuration;

  if (expected_gesture_end_time > now) {
    *prediction_valid_duration = expected_gesture_end_time - now;
    return true;
  }
  return false;
}

void UserModel::Reset(base::TimeTicks now) {
  last_input_signal_time_ = base::TimeTicks();
  last_gesture_start_time_ = base::TimeTicks();
  last_continuous_gesture_time_ = base::TimeTicks();
  last_gesture_expected_start_time_ = base::TimeTicks();
  last_discrete_input_time_ = base::TimeTicks();
  last_reset_time_ = now;
  is_gesture_active_ = false;
  is_gesture_expected_ = false;
  pending_input_event_count_ = 0;
}

void UserModel::WriteIntoTrace(perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("pending_input_event_count", pending_input_event_count_);
  dict.Add("last_input_signal_time", last_input_signal_time_);
  dict.Add("last_gesture_start_time", last_gesture_start_time_);
  dict.Add("last_continuous_gesture_time", last_continuous_gesture_time_);
  dict.Add("last_gesture_expected_start_time",
           last_gesture_expected_start_time_);
  dict.Add("last_reset_time", last_reset_time_);
  dict.Add("is_gesture_expected", is_gesture_expected_);
  dict.Add("is_gesture_active", is_gesture_active_);
}

}  // namespace scheduler
}  // namespace blink
