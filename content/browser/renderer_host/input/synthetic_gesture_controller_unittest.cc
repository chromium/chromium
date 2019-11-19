// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_gesture_controller.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/browser/renderer_host/input/synthetic_pinch_gesture.h"
#include "content/browser/renderer_host/input/synthetic_pointer_action.h"
#include "content/browser/renderer_host/input/synthetic_smooth_drag_gesture.h"
#include "content/browser/renderer_host/input/synthetic_smooth_move_gesture.h"
#include "content/browser/renderer_host/input/synthetic_smooth_scroll_gesture.h"
#include "content/browser/renderer_host/input/synthetic_tap_gesture.h"
#include "content/browser/renderer_host/input/synthetic_touchpad_pinch_gesture.h"
#include "content/browser/renderer_host/input/synthetic_touchscreen_pinch_gesture.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/common/input/synthetic_pinch_gesture_params.h"
#include "content/common/input/synthetic_smooth_drag_gesture_params.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/common/input/synthetic_tap_gesture_params.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"

using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

namespace {

const int kFlushInputRateInMs = 16;
const int kPointerAssumedStoppedTimeMs = 43;
const float kTouchSlopInDips = 7.0f;
const float kMinScalingSpanInDips = 27.5f;
const int kTouchPointersLength = 16;
const int kMouseWheelTickMultiplier = 0;

enum TouchGestureType { TOUCH_SCROLL, TOUCH_DRAG };

WebTouchPoint::State ToWebTouchPointState(
    SyntheticPointerActionParams::PointerActionType action_type) {
  switch (action_type) {
    case SyntheticPointerActionParams::PointerActionType::PRESS:
      return WebTouchPoint::kStatePressed;
    case SyntheticPointerActionParams::PointerActionType::MOVE:
      return WebTouchPoint::kStateMoved;
    case SyntheticPointerActionParams::PointerActionType::RELEASE:
      return WebTouchPoint::kStateReleased;
    case SyntheticPointerActionParams::PointerActionType::CANCEL:
      return WebTouchPoint::kStateCancelled;
    case SyntheticPointerActionParams::PointerActionType::IDLE:
      return WebTouchPoint::kStateStationary;
    case SyntheticPointerActionParams::PointerActionType::LEAVE:
    case SyntheticPointerActionParams::PointerActionType::NOT_INITIALIZED:
      NOTREACHED()
          << "Invalid SyntheticPointerActionParams::PointerActionType.";
      return WebTouchPoint::kStateUndefined;
  }
  NOTREACHED() << "Invalid SyntheticPointerActionParams::PointerActionType.";
  return WebTouchPoint::kStateUndefined;
}

WebInputEvent::Type ToWebMouseEventType(
    SyntheticPointerActionParams::PointerActionType action_type) {
  switch (action_type) {
    case SyntheticPointerActionParams::PointerActionType::PRESS:
      return WebInputEvent::kMouseDown;
    case SyntheticPointerActionParams::PointerActionType::MOVE:
      return WebInputEvent::kMouseMove;
    case SyntheticPointerActionParams::PointerActionType::RELEASE:
      return WebInputEvent::kMouseUp;
    case SyntheticPointerActionParams::PointerActionType::LEAVE:
      return WebInputEvent::kMouseLeave;
    case SyntheticPointerActionParams::PointerActionType::CANCEL:
    case SyntheticPointerActionParams::PointerActionType::IDLE:
    case SyntheticPointerActionParams::PointerActionType::NOT_INITIALIZED:
      NOTREACHED()
          << "Invalid SyntheticPointerActionParams::PointerActionType.";
      return WebInputEvent::kUndefined;
  }
  NOTREACHED() << "Invalid SyntheticPointerActionParams::PointerActionType.";
  return WebInputEvent::kUndefined;
}

WebInputEvent::Type WebTouchPointStateToEventType(
    blink::WebTouchPoint::State state) {
  switch (state) {
    case blink::WebTouchPoint::kStateReleased:
      return WebInputEvent::kTouchEnd;
    case blink::WebTouchPoint::kStatePressed:
      return WebInputEvent::kTouchStart;
    case blink::WebTouchPoint::kStateMoved:
      return WebInputEvent::kTouchMove;
    case blink::WebTouchPoint::kStateCancelled:
      return WebInputEvent::kTouchCancel;
    default:
      return WebInputEvent::kUndefined;
  }
}

class MockSyntheticGesture : public SyntheticGesture {
 public:
  MockSyntheticGesture(bool* finished, int num_steps)
      : finished_(finished),
        num_steps_(num_steps),
        step_count_(0) {
    *finished_ = false;
  }
  ~MockSyntheticGesture() override {}

  Result ForwardInputEvents(const base::TimeTicks& timestamp,
                            SyntheticGestureTarget* target) override {
    step_count_++;
    if (step_count_ == num_steps_) {
      *finished_ = true;
      return SyntheticGesture::GESTURE_FINISHED;
    } else if (step_count_ > num_steps_) {
      *finished_ = true;
      // Return arbitrary failure.
      return SyntheticGesture::GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED;
    }

    return SyntheticGesture::GESTURE_RUNNING;
  }

 protected:
  bool* finished_;
  int num_steps_;
  int step_count_;
};

class MockSyntheticGestureTarget : public SyntheticGestureTarget {
 public:
  MockSyntheticGestureTarget()
      : flush_requested_(false),
        pointer_assumed_stopped_time_ms_(kPointerAssumedStoppedTimeMs) {}
  ~MockSyntheticGestureTarget() override {}

  // SyntheticGestureTarget:
  void DispatchInputEventToPlatform(const WebInputEvent& event) override {}

  SyntheticGestureParams::GestureSourceType
  GetDefaultSyntheticGestureSourceType() const override {
    return SyntheticGestureParams::TOUCH_INPUT;
  }

  base::TimeDelta PointerAssumedStoppedTime() const override {
    return base::TimeDelta::FromMilliseconds(pointer_assumed_stopped_time_ms_);
  }

  void set_pointer_assumed_stopped_time_ms(int time_ms) {
    pointer_assumed_stopped_time_ms_ = time_ms;
  }

  float GetTouchSlopInDips() const override { return kTouchSlopInDips; }
  float GetSpanSlopInDips() const override { return 2 * kTouchSlopInDips; }

  int GetMouseWheelMinimumGranularity() const override {
    return kMouseWheelTickMultiplier;
  }

  float GetMinScalingSpanInDips() const override {
    return kMinScalingSpanInDips;
  }

  bool flush_requested() const { return flush_requested_; }
  void ClearFlushRequest() { flush_requested_ = false; }

  void WaitForTargetAck(SyntheticGestureParams::GestureType type,
                        SyntheticGestureParams::GestureSourceType source,
                        base::OnceClosure callback) const override {
    // Must resolve synchronously since FlushInputUntilComplete will try the
    // next gesture after this one.
    std::move(callback).Run();
  }

 private:
  bool flush_requested_;

  int pointer_assumed_stopped_time_ms_;
};

class MockMoveGestureTarget : public MockSyntheticGestureTarget {
 public:
  MockMoveGestureTarget()
      : total_abs_move_distance_length_(0),
        granularity_(ui::input_types::ScrollGranularity::kScrollByPixel) {}
  ~MockMoveGestureTarget() override {}

  gfx::Vector2dF start_to_end_distance() const {
    return start_to_end_distance_;
  }
  float total_abs_move_distance_length() const {
    return total_abs_move_distance_length_;
  }

  ui::input_types::ScrollGranularity granularity() const {
    return granularity_;
  }

 protected:
  gfx::Vector2dF start_to_end_distance_;
  float total_abs_move_distance_length_;
  ui::input_types::ScrollGranularity granularity_;
};

class MockScrollMouseTarget : public MockMoveGestureTarget {
 public:
  MockScrollMouseTarget() {}
  ~MockScrollMouseTarget() override {}

  void DispatchInputEventToPlatform(const WebInputEvent& event) override {
    ASSERT_EQ(event.GetType(), WebInputEvent::kMouseWheel);
    const WebMouseWheelEvent& mouse_wheel_event =
        static_cast<const WebMouseWheelEvent&>(event);
    gfx::Vector2dF delta(mouse_wheel_event.delta_x, mouse_wheel_event.delta_y);
    start_to_end_distance_ += delta;
    total_abs_move_distance_length_ += delta.Length();
    granularity_ = mouse_wheel_event.delta_units;
  }
};

class MockMoveTouchTarget : public MockMoveGestureTarget {
 public:
  MockMoveTouchTarget() : started_(false) {}
  ~MockMoveTouchTarget() override {}

  void DispatchInputEventToPlatform(const WebInputEvent& event) override {
    ASSERT_TRUE(WebInputEvent::IsTouchEventType(event.GetType()));
    const WebTouchEvent& touch_event = static_cast<const WebTouchEvent&>(event);
    ASSERT_EQ(touch_event.touches_length, 1U);

    if (!started_) {
      ASSERT_EQ(touch_event.GetType(), WebInputEvent::kTouchStart);
      start_.SetPoint(touch_event.touches[0].PositionInWidget().x,
                      touch_event.touches[0].PositionInWidget().y);
      last_touch_point_ = gfx::PointF(start_);
      started_ = true;
    } else {
      ASSERT_NE(touch_event.GetType(), WebInputEvent::kTouchStart);
      ASSERT_NE(touch_event.GetType(), WebInputEvent::kTouchCancel);

      gfx::PointF touch_point(touch_event.touches[0].PositionInWidget().x,
                              touch_event.touches[0].PositionInWidget().y);
      gfx::Vector2dF delta = touch_point - last_touch_point_;
      total_abs_move_distance_length_ += delta.Length();

      if (touch_event.GetType() == WebInputEvent::kTouchEnd)
        start_to_end_distance_ = touch_point - gfx::PointF(start_);

      last_touch_point_ = touch_point;
    }
  }

 protected:
  gfx::Point start_;
  gfx::PointF last_touch_point_;
  bool started_;
};

class MockFlingGestureTarget : public MockMoveGestureTarget {
 public:
  MockFlingGestureTarget() : fling_velocity_x_(0), fling_velocity_y_(0) {}
  ~MockFlingGestureTarget() override {}

  void DispatchInputEventToPlatform(const WebInputEvent& event) override {
    if (event.GetType() == WebInputEvent::kGestureFlingStart) {
      const blink::WebGestureEvent& gesture_event =
          static_cast<const blink::WebGestureEvent&>(event);
      fling_velocity_x_ = gesture_event.data.fling_start.velocity_x;
      fling_velocity_y_ = gesture_event.data.fling_start.velocity_y;
    }
  }

  float fling_velocity_x() const { return fling_velocity_x_; }
  float fling_velocity_y() const { return fling_velocity_y_; }

 private:
  float fling_velocity_x_;
  float fling_velocity_y_;
};

class MockDragMouseTarget : public MockMoveGestureTarget {
 public:
  MockDragMouseTarget() : started_(false) {}
  ~MockDragMouseTarget() override {}

  void DispatchInputEventToPlatform(const WebInputEvent& event) override {
    ASSERT_TRUE(WebInputEvent::IsMouseEventType(event.GetType()));
    const WebMouseEvent& mouse_event = static_cast<const WebMouseEvent&>(event);
    if (!started_) {
      EXPECT_EQ(mouse_event.button, WebMouseEvent::Button::kLeft);
      EXPECT_EQ(mouse_event.click_count, 1);
      EXPECT_EQ(mouse_event.GetType(), WebInputEvent::kMouseDown);
      start_.SetPoint(mouse_event.PositionInWidget().x,
                      mouse_event.PositionInWidget().y);
      last_mouse_point_ = start_;
      started_ = true;
    } else {
      EXPECT_EQ(mouse_event.button, WebMouseEvent::Button::kLeft);
      ASSERT_NE(mouse_event.GetType(), WebInputEvent::kMouseDown);

      gfx::PointF mouse_point(mouse_event.PositionInWidget());
      gfx::Vector2dF delta = mouse_point - last_mouse_point_;
      total_abs_move_distance_length_ += delta.Length();
      if (mouse_event.GetType() == WebInputEvent::kMouseUp)
        start_to_end_distance_ = mouse_point - start_;
      last_mouse_point_ = mouse_point;
    }
  }

 private:
  bool started_;
  gfx::PointF start_, last_mouse_point_;
};

class MockSyntheticTouchscreenPinchTouchTarget
    : public MockSyntheticGestureTarget {
 public:
  enum ZoomDirection {
    ZOOM_DIRECTION_UNKNOWN,
    ZOOM_IN,
    ZOOM_OUT
  };

  MockSyntheticTouchscreenPinchTouchTarget()
      : initial_pointer_distance_(0),
        last_pointer_distance_(0),
        zoom_direction_(ZOOM_DIRECTION_UNKNOWN),
        started_(false) {}
  ~MockSyntheticTouchscreenPinchTouchTarget() override {}

  void DispatchInputEventToPlatform(const WebInputEvent& event) override {
    ASSERT_TRUE(WebInputEvent::IsTouchEventType(event.GetType()));
    const WebTouchEvent& touch_event = static_cast<const WebTouchEvent&>(event);
    ASSERT_EQ(touch_event.touches_length, 2U);

    if (!started_) {
      ASSERT_EQ(touch_event.GetType(), WebInputEvent::kTouchStart);

      start_0_ = gfx::PointF(touch_event.touches[0].PositionInWidget());
      start_1_ = gfx::PointF(touch_event.touches[1].PositionInWidget());
      last_pointer_distance_ = (start_0_ - start_1_).Length();
      initial_pointer_distance_ = last_pointer_distance_;
      EXPECT_GE(initial_pointer_distance_, GetMinScalingSpanInDips());

      started_ = true;
    } else {
      ASSERT_NE(touch_event.GetType(), WebInputEvent::kTouchStart);
      ASSERT_NE(touch_event.GetType(), WebInputEvent::kTouchCancel);

      gfx::PointF current_0 =
          gfx::PointF(touch_event.touches[0].PositionInWidget());
      gfx::PointF current_1 =
          gfx::PointF(touch_event.touches[1].PositionInWidget());

      float pointer_distance = (current_0 - current_1).Length();

      if (last_pointer_distance_ != pointer_distance) {
        if (zoom_direction_ == ZOOM_DIRECTION_UNKNOWN)
          zoom_direction_ =
              ComputeZoomDirection(last_pointer_distance_, pointer_distance);
        else
          EXPECT_EQ(
              zoom_direction_,
              ComputeZoomDirection(last_pointer_distance_, pointer_distance));
      }

      last_pointer_distance_ = pointer_distance;
    }
  }

  SyntheticGestureParams::GestureSourceType
  GetDefaultSyntheticGestureSourceType() const override {
    return SyntheticGestureParams::TOUCH_INPUT;
  }

  ZoomDirection zoom_direction() const { return zoom_direction_; }

  float ComputeScaleFactor() const {
    switch (zoom_direction_) {
      case ZOOM_IN:
        return last_pointer_distance_ /
               (initial_pointer_distance_ + GetSpanSlopInDips());
      case ZOOM_OUT:
        return last_pointer_distance_ /
               (initial_pointer_distance_ - GetSpanSlopInDips());
      case ZOOM_DIRECTION_UNKNOWN:
        return 1.0f;
      default:
        NOTREACHED();
        return 0.0f;
    }
  }

 private:
  ZoomDirection ComputeZoomDirection(float last_pointer_distance,
                                     float current_pointer_distance) {
    DCHECK_NE(last_pointer_distance, current_pointer_distance);
    return last_pointer_distance < current_pointer_distance ? ZOOM_IN
                                                            : ZOOM_OUT;
  }

  float initial_pointer_distance_;
  float last_pointer_distance_;
  ZoomDirection zoom_direction_;
  gfx::PointF start_0_;
  gfx::PointF start_1_;
  bool started_;
};

class MockSyntheticTouchpadPinchTouchTarget
    : public MockSyntheticGestureTarget {
 public:
  enum ZoomDirection { ZOOM_DIRECTION_UNKNOWN, ZOOM_IN, ZOOM_OUT };

  MockSyntheticTouchpadPinchTouchTarget()
      : zoom_direction_(ZOOM_DIRECTION_UNKNOWN),
        started_(false),
        ended_(false),
        scale_factor_(1.0f) {}
  ~MockSyntheticTouchpadPinchTouchTarget() override {}

  void DispatchInputEventToPlatform(const WebInputEvent& event) override {
    EXPECT_TRUE(WebInputEvent::IsGestureEventType(event.GetType()));
    const blink::WebGestureEvent& gesture_event =
        static_cast<const blink::WebGestureEvent&>(event);

    if (gesture_event.GetType() == WebInputEvent::kGesturePinchBegin) {
      EXPECT_FALSE(started_);
      EXPECT_FALSE(ended_);
      started_ = true;
    } else if (gesture_event.GetType() == WebInputEvent::kGesturePinchEnd) {
      EXPECT_TRUE(started_);
      EXPECT_FALSE(ended_);
      ended_ = true;
    } else {
      EXPECT_EQ(WebInputEvent::kGesturePinchUpdate, gesture_event.GetType());
      EXPECT_TRUE(started_);
      EXPECT_FALSE(ended_);
      const float scale = gesture_event.data.pinch_update.scale;
      if (scale != 1.0f) {
        if (zoom_direction_ == ZOOM_DIRECTION_UNKNOWN) {
          zoom_direction_ = scale > 1.0f ? ZOOM_IN : ZOOM_OUT;
        } else if (zoom_direction_ == ZOOM_IN) {
          EXPECT_GT(scale, 1.0f);
        } else {
          EXPECT_EQ(ZOOM_OUT, zoom_direction_);
          EXPECT_LT(scale, 1.0f);
        }

        scale_factor_ *= scale;
      }
    }
  }

  SyntheticGestureParams::GestureSourceType
  GetDefaultSyntheticGestureSourceType() const override {
    return SyntheticGestureParams::MOUSE_INPUT;
  }

  ZoomDirection zoom_direction() const { return zoom_direction_; }

  float scale_factor() const { return scale_factor_; }

 private:
  ZoomDirection zoom_direction_;
  bool started_;
  bool ended_;
  float scale_factor_;
};

class MockSyntheticTapGestureTarget : public MockSyntheticGestureTarget {
 public:
  MockSyntheticTapGestureTarget() : state_(NOT_STARTED) {}
  ~MockSyntheticTapGestureTarget() override {}

  bool GestureFinished() const { return state_ == FINISHED; }
  gfx::PointF position() const { return position_; }
  base::TimeDelta GetDuration() const { return stop_time_ - start_time_; }

 protected:
  enum GestureState {
    NOT_STARTED,
    STARTED,
    FINISHED
  };

  gfx::PointF position_;
  base::TimeDelta start_time_;
  base::TimeDelta stop_time_;
  GestureState state_;
};

class MockSyntheticTapTouchTarget : public MockSyntheticTapGestureTarget {
 public:
  MockSyntheticTapTouchTarget() {}
  ~MockSyntheticTapTouchTarget() override {}

  void DispatchInputEventToPlatform(const WebInputEvent& event) override {
    ASSERT_TRUE(WebInputEvent::IsTouchEventType(event.GetType()));
    const WebTouchEvent& touch_event = static_cast<const WebTouchEvent&>(event);
    ASSERT_EQ(touch_event.touches_length, 1U);

    switch (state_) {
      case NOT_STARTED:
        EXPECT_EQ(touch_event.GetType(), WebInputEvent::kTouchStart);
        position_ = gfx::PointF(touch_event.touches[0].PositionInWidget());
        start_time_ = touch_event.TimeStamp().since_origin();
        state_ = STARTED;
        break;
      case STARTED:
        EXPECT_EQ(touch_event.GetType(), WebInputEvent::kTouchEnd);
        EXPECT_EQ(position_,
                  gfx::PointF(touch_event.touches[0].PositionInWidget()));
        stop_time_ = touch_event.TimeStamp().since_origin();
        state_ = FINISHED;
        break;
      case FINISHED:
        EXPECT_FALSE(true);
        break;
    }
  }
};

class MockSyntheticTapMouseTarget : public MockSyntheticTapGestureTarget {
 public:
  MockSyntheticTapMouseTarget() {}
  ~MockSyntheticTapMouseTarget() override {}

  void DispatchInputEventToPlatform(const WebInputEvent& event) override {
    ASSERT_TRUE(WebInputEvent::IsMouseEventType(event.GetType()));
    const WebMouseEvent& mouse_event = static_cast<const WebMouseEvent&>(event);

    switch (state_) {
      case NOT_STARTED:
        EXPECT_EQ(mouse_event.GetType(), WebInputEvent::kMouseDown);
        EXPECT_EQ(mouse_event.button, WebMouseEvent::Button::kLeft);
        EXPECT_EQ(mouse_event.click_count, 1);
        position_ = gfx::PointF(mouse_event.PositionInWidget());
        start_time_ = mouse_event.TimeStamp().since_origin();
        state_ = STARTED;
        break;
      case STARTED:
        EXPECT_EQ(mouse_event.GetType(), WebInputEvent::kMouseUp);
        EXPECT_EQ(mouse_event.button, WebMouseEvent::Button::kLeft);
        EXPECT_EQ(mouse_event.click_count, 1);
        EXPECT_EQ(position_, gfx::PointF(mouse_event.PositionInWidget()));
        stop_time_ = mouse_event.TimeStamp().since_origin();
        state_ = FINISHED;
        break;
      case FINISHED:
        EXPECT_FALSE(true);
        break;
    }
  }
};

class MockSyntheticPointerActionTarget : public MockSyntheticGestureTarget {
 public:
  MockSyntheticPointerActionTarget() : num_dispatched_pointer_actions_(0) {}
  ~MockSyntheticPointerActionTarget() override {}

  WebInputEvent::Type type() const { return type_; }
  int num_dispatched_pointer_actions() const {
    return num_dispatched_pointer_actions_;
  }
  void reset_num_dispatched_pointer_actions() {
    num_dispatched_pointer_actions_ = 0;
  }

 protected:
  WebInputEvent::Type type_;
  int num_dispatched_pointer_actions_;
};

class MockSyntheticPointerTouchActionTarget
    : public MockSyntheticPointerActionTarget {
 public:
  MockSyntheticPointerTouchActionTarget() {}
  ~MockSyntheticPointerTouchActionTarget() override {}

  void DispatchInputEventToPlatform(const WebInputEvent& event) override {
    DCHECK(WebInputEvent::IsTouchEventType(event.GetType()));
    const WebTouchEvent& touch_event = static_cast<const WebTouchEvent&>(event);
    type_ = touch_event.GetType();
    for (size_t i = 0; i < WebTouchEvent::kTouchesLengthCap; ++i) {
      if (WebTouchPointStateToEventType(touch_event.touches[i].state) != type_)
        continue;
      indexes_[num_dispatched_pointer_actions_] = i;
      positions_[num_dispatched_pointer_actions_] =
          gfx::PointF(touch_event.touches[i].PositionInWidget());
      states_[num_dispatched_pointer_actions_] = touch_event.touches[i].state;
      num_dispatched_pointer_actions_++;
    }
  }

  testing::AssertionResult SyntheticTouchActionDispatchedCorrectly(
      const SyntheticPointerActionParams& param,
      int index,
      int touch_index) {
    if (param.pointer_action_type() ==
            SyntheticPointerActionParams::PointerActionType::PRESS ||
        param.pointer_action_type() ==
            SyntheticPointerActionParams::PointerActionType::MOVE) {
      if (indexes_[index] != touch_index) {
        return testing::AssertionFailure()
               << "Pointer index at index " << index << " was "
               << indexes_[index] << ", expected " << touch_index << ".";
      }

      if (positions_[index] != param.position()) {
        return testing::AssertionFailure()
               << "Pointer position at index " << index << " was "
               << positions_[index].ToString() << ", expected "
               << param.position().ToString() << ".";
      }
    }

    if (states_[index] != ToWebTouchPointState(param.pointer_action_type())) {
      return testing::AssertionFailure()
             << "Pointer states at index " << index << " was " << states_[index]
             << ", expected "
             << ToWebTouchPointState(param.pointer_action_type()) << ".";
    }
    return testing::AssertionSuccess();
  }

  testing::AssertionResult SyntheticTouchActionListDispatchedCorrectly(
      const std::vector<SyntheticPointerActionParams>& params_list,
      int start_index,
      int index_array[]) {
    testing::AssertionResult result = testing::AssertionSuccess();
    for (size_t i = 0; i < params_list.size(); ++i) {
      if (params_list[i].pointer_action_type() !=
          SyntheticPointerActionParams::PointerActionType::IDLE)
        result = SyntheticTouchActionDispatchedCorrectly(
            params_list[i], start_index + i, index_array[i]);
      if (result == testing::AssertionFailure())
        return result;
    }
    return testing::AssertionSuccess();
  }

 private:
  gfx::PointF positions_[kTouchPointersLength];
  int indexes_[kTouchPointersLength];
  WebTouchPoint::State states_[kTouchPointersLength];
};

class MockSyntheticPointerMouseActionTarget
    : public MockSyntheticPointerActionTarget {
 public:
  MockSyntheticPointerMouseActionTarget() {}
  ~MockSyntheticPointerMouseActionTarget() override {}

  void DispatchInputEventToPlatform(const WebInputEvent& event) override {
    ASSERT_TRUE(WebInputEvent::IsMouseEventType(event.GetType()));
    const WebMouseEvent& mouse_event = static_cast<const WebMouseEvent&>(event);
    type_ = mouse_event.GetType();
    position_ = gfx::PointF(mouse_event.PositionInWidget());
    clickCount_ = mouse_event.click_count;
    button_ = mouse_event.button;
    num_dispatched_pointer_actions_++;
  }

  testing::AssertionResult SyntheticMouseActionDispatchedCorrectly(
      const SyntheticPointerActionParams& param,
      int click_count,
      SyntheticPointerActionParams::Button button =
          SyntheticPointerActionParams::Button::NO_BUTTON) {
    if (type_ != ToWebMouseEventType(param.pointer_action_type())) {
      return testing::AssertionFailure()
             << "Pointer type was " << WebInputEvent::GetName(type_)
             << ", expected " << WebInputEvent::GetName(ToWebMouseEventType(
             param.pointer_action_type())) << ".";
    }

    if (clickCount_ != click_count) {
      return testing::AssertionFailure() << "Pointer click count was "
                                         << clickCount_ << ", expected "
                                         << click_count << ".";
    }

    if (button_ != WebMouseEvent::Button::kNoButton) {
      if (param.pointer_action_type() ==
              SyntheticPointerActionParams::PointerActionType::PRESS ||
          param.pointer_action_type() ==
              SyntheticPointerActionParams::PointerActionType::RELEASE) {
        if (clickCount_ != 1) {
          return testing::AssertionFailure() << "Pointer click count was "
                                             << clickCount_ << ", expected 1.";
        }
      }

      if (param.pointer_action_type() ==
          SyntheticPointerActionParams::PointerActionType::MOVE) {
        if (clickCount_ != 0) {
          return testing::AssertionFailure() << "Pointer click count was "
                                             << clickCount_ << ", expected 0.";
        }
      }

      if (button_ !=
          SyntheticPointerActionParams::GetWebMouseEventButton(button)) {
        return testing::AssertionFailure()
               << "Pointer button was " << static_cast<int>(button_)
               << ", expected " << static_cast<int>(button) << ".";
      }
    }

    if ((param.pointer_action_type() ==
             SyntheticPointerActionParams::PointerActionType::PRESS ||
         param.pointer_action_type() ==
             SyntheticPointerActionParams::PointerActionType::MOVE) &&
        position_ != param.position()) {
      return testing::AssertionFailure()
             << "Pointer position was " << position_.ToString() << ", expected "
             << param.position().ToString() << ".";
    }
    return testing::AssertionSuccess();
  }

 private:
  gfx::PointF position_;
  int clickCount_;
  WebMouseEvent::Button button_;
};

class DummySyntheticGestureControllerDelegate
    : public SyntheticGestureController::Delegate {
 public:
  DummySyntheticGestureControllerDelegate() {}
  ~DummySyntheticGestureControllerDelegate() override {}

 private:
  // SyntheticGestureController::Delegate:
  bool HasGestureStopped() override { return true; }

  DISALLOW_COPY_AND_ASSIGN(DummySyntheticGestureControllerDelegate);
};

}  // namespace

class SyntheticGestureControllerTestBase {
 public:
  SyntheticGestureControllerTestBase() {}
  ~SyntheticGestureControllerTestBase() {}

 protected:
  template<typename MockGestureTarget>
  void CreateControllerAndTarget() {
    target_ = new MockGestureTarget();
    controller_ = std::make_unique<SyntheticGestureController>(
        &delegate_, std::unique_ptr<SyntheticGestureTarget>(target_));
  }

  void QueueSyntheticGesture(std::unique_ptr<SyntheticGesture> gesture) {
    controller_->QueueSyntheticGesture(
        std::move(gesture),
        base::BindOnce(
            &SyntheticGestureControllerTestBase::OnSyntheticGestureCompleted,
            base::Unretained(this)));
  }

  void FlushInputUntilComplete() {
    // Start and stop the timer explicitly here, since the test does not need to
    // wait for begin-frame to start the timer.
    controller_->dispatch_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(1), base::DoNothing());
    do
      time_ += base::TimeDelta::FromMilliseconds(kFlushInputRateInMs);
    while (controller_->DispatchNextEvent(time_));
    controller_->dispatch_timer_.Stop();
  }

  void OnSyntheticGestureCompleted(SyntheticGesture::Result result) {
    DCHECK_NE(result, SyntheticGesture::GESTURE_RUNNING);
    if (result == SyntheticGesture::GESTURE_FINISHED)
      num_success_++;
    else
      num_failure_++;
  }

  bool DispatchTimerRunning() const {
    return controller_->dispatch_timer_.IsRunning();
  }

  base::TimeDelta GetTotalTime() const { return time_ - start_time_; }

  base::test::TaskEnvironment env_;
  MockSyntheticGestureTarget* target_;
  DummySyntheticGestureControllerDelegate delegate_;
  std::unique_ptr<SyntheticGestureController> controller_;
  base::TimeTicks start_time_;
  base::TimeTicks time_;
  int num_success_;
  int num_failure_;
};

class SyntheticGestureControllerTest
    : public SyntheticGestureControllerTestBase,
      public testing::Test {
 protected:
  void SetUp() override {
    start_time_ = base::TimeTicks::Now();
    time_ = start_time_;
    num_success_ = 0;
    num_failure_ = 0;
  }

  void TearDown() override {
    controller_.reset();
    target_ = nullptr;
    time_ = base::TimeTicks();
  }
};

class SyntheticGestureControllerTestWithParam
    : public SyntheticGestureControllerTestBase,
      public testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    start_time_ = base::TimeTicks::Now();
    time_ = start_time_;
    num_success_ = 0;
    num_failure_ = 0;
  }

  void TearDown() override {
    controller_.reset();
    target_ = nullptr;
    time_ = base::TimeTicks();
  }
};

TEST_F(SyntheticGestureControllerTest, SingleGesture) {
  CreateControllerAndTarget<MockSyntheticGestureTarget>();

  bool finished = false;
  std::unique_ptr<MockSyntheticGesture> gesture(
      new MockSyntheticGesture(&finished, 3));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  EXPECT_TRUE(finished);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
}

TEST_F(SyntheticGestureControllerTest, GestureFailed) {
  CreateControllerAndTarget<MockSyntheticGestureTarget>();

  bool finished = false;
  std::unique_ptr<MockSyntheticGesture> gesture(
      new MockSyntheticGesture(&finished, 0));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  EXPECT_TRUE(finished);
  EXPECT_EQ(1, num_failure_);
  EXPECT_EQ(0, num_success_);
}

TEST_F(SyntheticGestureControllerTest, SuccessiveGestures) {
  CreateControllerAndTarget<MockSyntheticGestureTarget>();

  bool finished_1 = false;
  std::unique_ptr<MockSyntheticGesture> gesture_1(
      new MockSyntheticGesture(&finished_1, 2));
  bool finished_2 = false;
  std::unique_ptr<MockSyntheticGesture> gesture_2(
      new MockSyntheticGesture(&finished_2, 4));

  // Queue first gesture and wait for it to finish
  QueueSyntheticGesture(std::move(gesture_1));
  FlushInputUntilComplete();

  EXPECT_TRUE(finished_1);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);

  // Queue second gesture.
  QueueSyntheticGesture(std::move(gesture_2));
  FlushInputUntilComplete();

  EXPECT_TRUE(finished_2);
  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
}

TEST_F(SyntheticGestureControllerTest, TwoGesturesInFlight) {
  CreateControllerAndTarget<MockSyntheticGestureTarget>();

  bool finished_1 = false;
  std::unique_ptr<MockSyntheticGesture> gesture_1(
      new MockSyntheticGesture(&finished_1, 2));
  bool finished_2 = false;
  std::unique_ptr<MockSyntheticGesture> gesture_2(
      new MockSyntheticGesture(&finished_2, 4));

  QueueSyntheticGesture(std::move(gesture_1));
  QueueSyntheticGesture(std::move(gesture_2));
  FlushInputUntilComplete();

  EXPECT_TRUE(finished_1);
  EXPECT_TRUE(finished_2);

  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
}

TEST_F(SyntheticGestureControllerTest, GestureCompletedOnDidFlushInput) {
  CreateControllerAndTarget<MockSyntheticGestureTarget>();

  bool finished_1, finished_2;
  std::unique_ptr<MockSyntheticGesture> gesture_1(
      new MockSyntheticGesture(&finished_1, 2));
  std::unique_ptr<MockSyntheticGesture> gesture_2(
      new MockSyntheticGesture(&finished_2, 4));

  QueueSyntheticGesture(std::move(gesture_1));
  QueueSyntheticGesture(std::move(gesture_2));

  FlushInputUntilComplete();
  EXPECT_EQ(2, num_success_);
}

gfx::Vector2d AddTouchSlopToVector(const gfx::Vector2dF& vector,
                                   SyntheticGestureTarget* target) {
  const int kTouchSlop = target->GetTouchSlopInDips();

  int x = vector.x();
  if (x > 0)
    x += kTouchSlop;
  else if (x < 0)
    x -= kTouchSlop;

  int y = vector.y();
  if (y > 0)
    y += kTouchSlop;
  else if (y < 0)
    y -= kTouchSlop;

  return gfx::Vector2d(x, y);
}

TEST_P(SyntheticGestureControllerTestWithParam,
       SingleMoveGestureTouchVertical) {
  CreateControllerAndTarget<MockMoveTouchTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::TOUCH_INPUT;
  if (GetParam() == TOUCH_DRAG) {
    params.add_slop = false;
  }
  params.start_point.SetPoint(89, 32);
  params.distances.push_back(gfx::Vector2d(0, 123));

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* scroll_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  if (GetParam() == TOUCH_SCROLL) {
    EXPECT_EQ(AddTouchSlopToVector(params.distances[0], target_),
              scroll_target->start_to_end_distance());
  } else {
    EXPECT_EQ(params.distances[0], scroll_target->start_to_end_distance());
  }
}

TEST_P(SyntheticGestureControllerTestWithParam,
       SingleScrollGestureTouchHorizontal) {
  CreateControllerAndTarget<MockMoveTouchTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::TOUCH_INPUT;
  if (GetParam() == TOUCH_DRAG) {
    params.add_slop = false;
  }
  params.start_point.SetPoint(12, -23);
  params.distances.push_back(gfx::Vector2d(-234, 0));

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* scroll_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  if (GetParam() == TOUCH_SCROLL) {
    EXPECT_EQ(AddTouchSlopToVector(params.distances[0], target_),
              scroll_target->start_to_end_distance());
  } else {
    EXPECT_EQ(params.distances[0], scroll_target->start_to_end_distance());
  }
}

void CheckIsWithinRangeSingle(float scroll_distance,
                              int target_distance,
                              SyntheticGestureTarget* target) {
  if (target_distance > 0) {
    EXPECT_LE(target_distance, scroll_distance);
    EXPECT_LE(scroll_distance, target_distance + target->GetTouchSlopInDips());
  } else {
    EXPECT_GE(target_distance, scroll_distance);
    EXPECT_GE(scroll_distance, target_distance - target->GetTouchSlopInDips());
  }
}

void CheckSingleScrollDistanceIsWithinRange(
    const gfx::Vector2dF& scroll_distance,
    const gfx::Vector2dF& target_distance,
    SyntheticGestureTarget* target) {
  CheckIsWithinRangeSingle(scroll_distance.x(), target_distance.x(), target);
  CheckIsWithinRangeSingle(scroll_distance.y(), target_distance.y(), target);
}

TEST_F(SyntheticGestureControllerTest, SingleScrollGestureTouchDiagonal) {
  CreateControllerAndTarget<MockMoveTouchTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::TOUCH_INPUT;
  params.start_point.SetPoint(0, 7);
  params.distances.push_back(gfx::Vector2d(413, -83));

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* scroll_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  CheckSingleScrollDistanceIsWithinRange(
      scroll_target->start_to_end_distance(), params.distances[0], target_);
}

TEST_F(SyntheticGestureControllerTest, SingleScrollGestureTouchLongStop) {
  CreateControllerAndTarget<MockMoveTouchTarget>();

  // Create a smooth scroll with a short distance and set the pointer assumed
  // stopped time high, so that the stopping should dominate the time the
  // gesture is active.
  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::TOUCH_INPUT;
  params.start_point.SetPoint(-98, -23);
  params.distances.push_back(gfx::Vector2d(21, -12));
  params.prevent_fling = true;
  target_->set_pointer_assumed_stopped_time_ms(543);

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* scroll_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  CheckSingleScrollDistanceIsWithinRange(
      scroll_target->start_to_end_distance(), params.distances[0], target_);
  EXPECT_GE(GetTotalTime(), target_->PointerAssumedStoppedTime());
}

TEST_F(SyntheticGestureControllerTest, SingleScrollGestureTouchFling) {
  CreateControllerAndTarget<MockMoveTouchTarget>();

  // Create a smooth scroll with a short distance and set the pointer assumed
  // stopped time high. Disable 'prevent_fling' and check that the gesture
  // finishes without waiting before it stops.
  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::TOUCH_INPUT;
  params.start_point.SetPoint(-89, 78);
  params.distances.push_back(gfx::Vector2d(-43, 19));
  params.prevent_fling = false;

  target_->set_pointer_assumed_stopped_time_ms(543);

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* scroll_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  CheckSingleScrollDistanceIsWithinRange(
      scroll_target->start_to_end_distance(), params.distances[0], target_);
  EXPECT_LE(GetTotalTime(), target_->PointerAssumedStoppedTime());
}

TEST_P(SyntheticGestureControllerTestWithParam,
       SingleScrollGestureTouchZeroDistance) {
  CreateControllerAndTarget<MockMoveTouchTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::TOUCH_INPUT;
  if (GetParam() == TOUCH_DRAG) {
    params.add_slop = false;
  }
  params.start_point.SetPoint(-32, 43);
  params.distances.push_back(gfx::Vector2d(0, 0));

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* scroll_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(gfx::Vector2dF(0, 0), scroll_target->start_to_end_distance());
}

TEST_F(SyntheticGestureControllerTest, SingleScrollGestureMouseVertical) {
  CreateControllerAndTarget<MockScrollMouseTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::MOUSE_WHEEL_INPUT;
  params.start_point.SetPoint(432, 89);
  params.distances.push_back(gfx::Vector2d(0, -234));

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* scroll_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(params.distances[0], scroll_target->start_to_end_distance());
}

TEST_F(SyntheticGestureControllerTest, SingleScrollGestureMouseHorizontal) {
  CreateControllerAndTarget<MockScrollMouseTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::MOUSE_WHEEL_INPUT;
  params.start_point.SetPoint(90, 12);
  params.distances.push_back(gfx::Vector2d(345, 0));

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* scroll_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(params.distances[0], scroll_target->start_to_end_distance());
}

TEST_F(SyntheticGestureControllerTest, SingleScrollGestureMouseDiagonal) {
  CreateControllerAndTarget<MockScrollMouseTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::MOUSE_WHEEL_INPUT;
  params.start_point.SetPoint(90, 12);
  params.distances.push_back(gfx::Vector2d(-194, 303));

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* scroll_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(params.distances[0], scroll_target->start_to_end_distance());
}

TEST_F(SyntheticGestureControllerTest, MultiScrollGestureMouse) {
  CreateControllerAndTarget<MockScrollMouseTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::MOUSE_WHEEL_INPUT;
  params.start_point.SetPoint(90, 12);
  params.distances.push_back(gfx::Vector2d(-129, 212));
  params.distances.push_back(gfx::Vector2d(8, -9));

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* scroll_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(params.distances[0] + params.distances[1],
            scroll_target->start_to_end_distance());
}

TEST_F(SyntheticGestureControllerTest, MultiScrollGestureMouseHorizontal) {
  CreateControllerAndTarget<MockScrollMouseTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::MOUSE_WHEEL_INPUT;
  params.start_point.SetPoint(90, 12);
  params.distances.push_back(gfx::Vector2d(-129, 0));
  params.distances.push_back(gfx::Vector2d(79, 0));

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* scroll_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  // This check only works for horizontal or vertical scrolls because of
  // floating point precision issues with diagonal scrolls.
  EXPECT_FLOAT_EQ(params.distances[0].Length() + params.distances[1].Length(),
                  scroll_target->total_abs_move_distance_length());
  EXPECT_FLOAT_EQ((params.distances[0] + params.distances[1]).x(),
                  scroll_target->start_to_end_distance().x());
}

TEST_F(SyntheticGestureControllerTest, SingleScrollGestureTouchpadSwipe) {
  CreateControllerAndTarget<MockFlingGestureTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::MOUSE_WHEEL_INPUT;
  params.start_point.SetPoint(39, 86);
  params.distances.push_back(gfx::Vector2d(0, -132));
  params.fling_velocity_x = 800;
  params.fling_velocity_y = -1000;
  params.prevent_fling = false;

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockFlingGestureTarget* swipe_target =
      static_cast<MockFlingGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(params.fling_velocity_x, swipe_target->fling_velocity_x());
  EXPECT_EQ(params.fling_velocity_y, swipe_target->fling_velocity_y());
}

TEST_F(SyntheticGestureControllerTest, SingleScrollGestureMousePreciseScroll) {
  CreateControllerAndTarget<MockScrollMouseTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::MOUSE_WHEEL_INPUT;
  params.start_point.SetPoint(39, 86);
  params.distances.push_back(gfx::Vector2d(0, -132));
  params.granularity =
      ui::input_types::ScrollGranularity::kScrollByPrecisePixel;

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* scroll_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(params.granularity, scroll_target->granularity());
}

TEST_F(SyntheticGestureControllerTest, SingleScrollGestureMouseScrollByPage) {
  CreateControllerAndTarget<MockScrollMouseTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::MOUSE_WHEEL_INPUT;
  params.start_point.SetPoint(39, 86);
  params.distances.push_back(gfx::Vector2d(0, -132));
  params.granularity = ui::input_types::ScrollGranularity::kScrollByPage;

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* scroll_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(params.granularity, scroll_target->granularity());
}

void CheckIsWithinRangeMulti(float scroll_distance,
                             int target_distance,
                             SyntheticGestureTarget* target) {
  if (target_distance > 0) {
    EXPECT_GE(scroll_distance, target_distance - target->GetTouchSlopInDips());
    EXPECT_LE(scroll_distance, target_distance + target->GetTouchSlopInDips());
  } else {
    EXPECT_LE(scroll_distance, target_distance + target->GetTouchSlopInDips());
    EXPECT_GE(scroll_distance, target_distance - target->GetTouchSlopInDips());
  }
}

void CheckMultiScrollDistanceIsWithinRange(
    const gfx::Vector2dF& scroll_distance,
    const gfx::Vector2dF& target_distance,
    SyntheticGestureTarget* target) {
  CheckIsWithinRangeMulti(scroll_distance.x(), target_distance.x(), target);
  CheckIsWithinRangeMulti(scroll_distance.y(), target_distance.y(), target);
}

TEST_F(SyntheticGestureControllerTest, MultiScrollGestureTouch) {
  CreateControllerAndTarget<MockMoveTouchTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::TOUCH_INPUT;
  params.start_point.SetPoint(8, -13);
  params.distances.push_back(gfx::Vector2d(234, 133));
  params.distances.push_back(gfx::Vector2d(-9, 78));

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* scroll_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  CheckMultiScrollDistanceIsWithinRange(
      scroll_target->start_to_end_distance(),
      params.distances[0] + params.distances[1],
      target_);
}

TEST_P(SyntheticGestureControllerTestWithParam,
       MultiScrollGestureTouchVertical) {
  CreateControllerAndTarget<MockMoveTouchTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::TOUCH_INPUT;
  if (GetParam() == TOUCH_DRAG) {
    params.add_slop = false;
  }
  params.start_point.SetPoint(234, -13);
  params.distances.push_back(gfx::Vector2d(0, 133));
  params.distances.push_back(gfx::Vector2d(0, 78));

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* scroll_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  if (GetParam() == TOUCH_SCROLL) {
    EXPECT_FLOAT_EQ(params.distances[0].Length() +
                        params.distances[1].Length() +
                        target_->GetTouchSlopInDips(),
                    scroll_target->total_abs_move_distance_length());
  EXPECT_EQ(AddTouchSlopToVector(params.distances[0] + params.distances[1],
                                 target_),
            scroll_target->start_to_end_distance());
  } else {
    EXPECT_FLOAT_EQ(params.distances[0].Length() + params.distances[1].Length(),
                    scroll_target->total_abs_move_distance_length());
    EXPECT_EQ(params.distances[0] + params.distances[1],
              scroll_target->start_to_end_distance());
  }
}

INSTANTIATE_TEST_SUITE_P(Single,
                         SyntheticGestureControllerTestWithParam,
                         testing::Values(TOUCH_SCROLL, TOUCH_DRAG));

TEST_F(SyntheticGestureControllerTest, SingleDragGestureMouseDiagonal) {
  CreateControllerAndTarget<MockDragMouseTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::MOUSE_DRAG_INPUT;
  params.start_point.SetPoint(0, 7);
  params.distances.push_back(gfx::Vector2d(413, -83));

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* drag_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(drag_target->start_to_end_distance(), params.distances[0]);
}

TEST_F(SyntheticGestureControllerTest, SingleDragGestureMouseZeroDistance) {
  CreateControllerAndTarget<MockDragMouseTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::MOUSE_DRAG_INPUT;
  params.start_point.SetPoint(-32, 43);
  params.distances.push_back(gfx::Vector2d(0, 0));

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* drag_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(gfx::Vector2dF(0, 0), drag_target->start_to_end_distance());
}

TEST_F(SyntheticGestureControllerTest, MultiDragGestureMouse) {
  CreateControllerAndTarget<MockDragMouseTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::MOUSE_DRAG_INPUT;
  params.start_point.SetPoint(8, -13);
  params.distances.push_back(gfx::Vector2d(234, 133));
  params.distances.push_back(gfx::Vector2d(-9, 78));

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* drag_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(drag_target->start_to_end_distance(),
            params.distances[0] + params.distances[1]);
}

TEST_F(SyntheticGestureControllerTest,
       SyntheticSmoothDragTestUsingSingleMouseDrag) {
  CreateControllerAndTarget<MockDragMouseTarget>();

  SyntheticSmoothDragGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  params.distances.push_back(gfx::Vector2d(234, 133));
  params.speed_in_pixels_s = 800;

  std::unique_ptr<SyntheticSmoothDragGesture> gesture(
      new SyntheticSmoothDragGesture(params));
  const base::TimeTicks timestamp;
  gesture->ForwardInputEvents(timestamp, target_);
}

TEST_F(SyntheticGestureControllerTest,
       SyntheticSmoothDragTestUsingSingleTouchDrag) {
  CreateControllerAndTarget<MockMoveTouchTarget>();

  SyntheticSmoothDragGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.start_point.SetPoint(89, 32);
  params.distances.push_back(gfx::Vector2d(0, 123));
  params.speed_in_pixels_s = 800;

  std::unique_ptr<SyntheticSmoothDragGesture> gesture(
      new SyntheticSmoothDragGesture(params));
  const base::TimeTicks timestamp;
  gesture->ForwardInputEvents(timestamp, target_);
}

TEST_F(SyntheticGestureControllerTest,
       SyntheticSmoothScrollTestUsingSingleTouchScroll) {
  CreateControllerAndTarget<MockMoveTouchTarget>();

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;

  std::unique_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  const base::TimeTicks timestamp;
  gesture->ForwardInputEvents(timestamp, target_);
}

TEST_F(SyntheticGestureControllerTest,
       SyntheticSmoothScrollTestUsingSingleMouseScroll) {
  CreateControllerAndTarget<MockScrollMouseTarget>();

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  params.anchor.SetPoint(432, 89);
  params.distances.push_back(gfx::Vector2d(0, -234));
  params.speed_in_pixels_s = 800;

  std::unique_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  const base::TimeTicks timestamp;
  gesture->ForwardInputEvents(timestamp, target_);
}

TEST_F(SyntheticGestureControllerTest,
       TouchscreenTouchpadPinchGestureTouchZoomIn) {
  CreateControllerAndTarget<MockSyntheticTouchscreenPinchTouchTarget>();

  SyntheticPinchGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.scale_factor = 2.3f;
  params.anchor.SetPoint(54, 89);

  std::unique_ptr<SyntheticTouchscreenPinchGesture> gesture(
      new SyntheticTouchscreenPinchGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockSyntheticTouchscreenPinchTouchTarget* pinch_target =
      static_cast<MockSyntheticTouchscreenPinchTouchTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pinch_target->zoom_direction(),
            MockSyntheticTouchscreenPinchTouchTarget::ZOOM_IN);
  EXPECT_FLOAT_EQ(params.scale_factor, pinch_target->ComputeScaleFactor());
}

TEST_F(SyntheticGestureControllerTest,
       TouchscreenTouchpadPinchGestureTouchZoomOut) {
  CreateControllerAndTarget<MockSyntheticTouchscreenPinchTouchTarget>();

  SyntheticPinchGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.scale_factor = 0.4f;
  params.anchor.SetPoint(-12, 93);

  std::unique_ptr<SyntheticTouchscreenPinchGesture> gesture(
      new SyntheticTouchscreenPinchGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockSyntheticTouchscreenPinchTouchTarget* pinch_target =
      static_cast<MockSyntheticTouchscreenPinchTouchTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pinch_target->zoom_direction(),
            MockSyntheticTouchscreenPinchTouchTarget::ZOOM_OUT);
  EXPECT_FLOAT_EQ(params.scale_factor, pinch_target->ComputeScaleFactor());
}

TEST_F(SyntheticGestureControllerTest,
       TouchscreenTouchpadPinchGestureTouchNoScaling) {
  CreateControllerAndTarget<MockSyntheticTouchscreenPinchTouchTarget>();

  SyntheticPinchGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.scale_factor = 1.0f;

  std::unique_ptr<SyntheticTouchscreenPinchGesture> gesture(
      new SyntheticTouchscreenPinchGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockSyntheticTouchscreenPinchTouchTarget* pinch_target =
      static_cast<MockSyntheticTouchscreenPinchTouchTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pinch_target->zoom_direction(),
            MockSyntheticTouchscreenPinchTouchTarget::ZOOM_DIRECTION_UNKNOWN);
  EXPECT_EQ(params.scale_factor, pinch_target->ComputeScaleFactor());
}

TEST_F(SyntheticGestureControllerTest, TouchpadPinchGestureTouchZoomIn) {
  CreateControllerAndTarget<MockSyntheticTouchpadPinchTouchTarget>();

  SyntheticPinchGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  params.scale_factor = 2.3f;
  params.anchor.SetPoint(54, 89);

  std::unique_ptr<SyntheticTouchpadPinchGesture> gesture(
      new SyntheticTouchpadPinchGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockSyntheticTouchpadPinchTouchTarget* pinch_target =
      static_cast<MockSyntheticTouchpadPinchTouchTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pinch_target->zoom_direction(),
            MockSyntheticTouchpadPinchTouchTarget::ZOOM_IN);
  EXPECT_FLOAT_EQ(params.scale_factor, pinch_target->scale_factor());
}

TEST_F(SyntheticGestureControllerTest, TouchpadPinchGestureTouchZoomOut) {
  CreateControllerAndTarget<MockSyntheticTouchpadPinchTouchTarget>();

  SyntheticPinchGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  params.scale_factor = 0.4f;
  params.anchor.SetPoint(-12, 93);

  std::unique_ptr<SyntheticTouchpadPinchGesture> gesture(
      new SyntheticTouchpadPinchGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockSyntheticTouchpadPinchTouchTarget* pinch_target =
      static_cast<MockSyntheticTouchpadPinchTouchTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pinch_target->zoom_direction(),
            MockSyntheticTouchpadPinchTouchTarget::ZOOM_OUT);
  EXPECT_FLOAT_EQ(params.scale_factor, pinch_target->scale_factor());
}

TEST_F(SyntheticGestureControllerTest, TouchpadPinchGestureTouchNoScaling) {
  CreateControllerAndTarget<MockSyntheticTouchpadPinchTouchTarget>();

  SyntheticPinchGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  params.scale_factor = 1.0f;

  std::unique_ptr<SyntheticTouchpadPinchGesture> gesture(
      new SyntheticTouchpadPinchGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockSyntheticTouchpadPinchTouchTarget* pinch_target =
      static_cast<MockSyntheticTouchpadPinchTouchTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pinch_target->zoom_direction(),
            MockSyntheticTouchpadPinchTouchTarget::ZOOM_DIRECTION_UNKNOWN);
  EXPECT_EQ(params.scale_factor, pinch_target->scale_factor());
}

// Ensure that if SyntheticPinchGesture is instantiated with TOUCH_INPUT it
// correctly creates a touchscreen gesture.
TEST_F(SyntheticGestureControllerTest, PinchGestureExplicitTouch) {
  CreateControllerAndTarget<MockSyntheticTouchscreenPinchTouchTarget>();

  SyntheticPinchGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.scale_factor = 2.3f;
  params.anchor.SetPoint(54, 89);

  std::unique_ptr<SyntheticPinchGesture> gesture(
      new SyntheticPinchGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  // Gesture target will fail expectations if the wrong underlying
  // SyntheticPinch*Gesture was instantiated.
}

// Ensure that if SyntheticPinchGesture is instantiated with MOUSE_INPUT it
// correctly creates a touchpad gesture.
TEST_F(SyntheticGestureControllerTest, PinchGestureExplicitMouse) {
  CreateControllerAndTarget<MockSyntheticTouchpadPinchTouchTarget>();

  SyntheticPinchGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  params.scale_factor = 2.3f;
  params.anchor.SetPoint(54, 89);

  std::unique_ptr<SyntheticPinchGesture> gesture(
      new SyntheticPinchGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  // Gesture target will fail expectations if the wrong underlying
  // SyntheticPinch*Gesture was instantiated.
}

// Ensure that if SyntheticPinchGesture is instantiated with DEFAULT_INPUT it
// correctly creates a touchscreen gesture for a touchscreen controller.
TEST_F(SyntheticGestureControllerTest, PinchGestureDefaultTouch) {
  CreateControllerAndTarget<MockSyntheticTouchscreenPinchTouchTarget>();

  SyntheticPinchGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::DEFAULT_INPUT;
  params.scale_factor = 2.3f;
  params.anchor.SetPoint(54, 89);

  std::unique_ptr<SyntheticPinchGesture> gesture(
      new SyntheticPinchGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  // Gesture target will fail expectations if the wrong underlying
  // SyntheticPinch*Gesture was instantiated.
}

// Ensure that if SyntheticPinchGesture is instantiated with DEFAULT_INPUT it
// correctly creates a touchpad gesture for a touchpad controller.
TEST_F(SyntheticGestureControllerTest, PinchGestureDefaultMouse) {
  CreateControllerAndTarget<MockSyntheticTouchpadPinchTouchTarget>();

  SyntheticPinchGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::DEFAULT_INPUT;
  params.scale_factor = 2.3f;
  params.anchor.SetPoint(54, 89);

  std::unique_ptr<SyntheticPinchGesture> gesture(
      new SyntheticPinchGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  // Gesture target will fail expectations if the wrong underlying
  // SyntheticPinch*Gesture was instantiated.
}

TEST_F(SyntheticGestureControllerTest, TapGestureTouch) {
  CreateControllerAndTarget<MockSyntheticTapTouchTarget>();

  SyntheticTapGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.duration_ms = 123;
  params.position.SetPoint(87, -124);

  std::unique_ptr<SyntheticTapGesture> gesture(new SyntheticTapGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockSyntheticTapTouchTarget* tap_target =
      static_cast<MockSyntheticTapTouchTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(tap_target->GestureFinished());
  EXPECT_EQ(tap_target->position(), params.position);
  EXPECT_EQ(tap_target->GetDuration().InMilliseconds(), params.duration_ms);
  EXPECT_GE(GetTotalTime(),
            base::TimeDelta::FromMilliseconds(params.duration_ms));
}

TEST_F(SyntheticGestureControllerTest, TapGestureMouse) {
  CreateControllerAndTarget<MockSyntheticTapMouseTarget>();

  SyntheticTapGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  params.duration_ms = 79;
  params.position.SetPoint(98, 123);

  std::unique_ptr<SyntheticTapGesture> gesture(new SyntheticTapGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockSyntheticTapMouseTarget* tap_target =
      static_cast<MockSyntheticTapMouseTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(tap_target->GestureFinished());
  EXPECT_EQ(tap_target->position(), params.position);
  EXPECT_EQ(tap_target->GetDuration().InMilliseconds(), params.duration_ms);
  EXPECT_GE(GetTotalTime(),
            base::TimeDelta::FromMilliseconds(params.duration_ms));
}

TEST_F(SyntheticGestureControllerTest, PointerTouchAction) {
  CreateControllerAndTarget<MockSyntheticPointerTouchActionTarget>();

  // First, send two touch presses for finger 0 and finger 1.
  SyntheticPointerActionListParams::ParamList param_list;
  SyntheticPointerActionParams param0 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  SyntheticPointerActionParams param1 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param0.set_position(gfx::PointF(54, 89));
  param0.set_pointer_id(0);
  param1.set_position(gfx::PointF(79, 132));
  param1.set_pointer_id(1);
  param_list.push_back(param0);
  param_list.push_back(param1);
  SyntheticPointerActionListParams params(param_list);
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  std::unique_ptr<SyntheticPointerAction> gesture(
      new SyntheticPointerAction(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockSyntheticPointerTouchActionTarget* pointer_touch_target =
      static_cast<MockSyntheticPointerTouchActionTarget*>(target_);
  int index_array[2] = {0, 1};
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_touch_target->num_dispatched_pointer_actions(), 2);
  EXPECT_TRUE(pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
      param_list, 0, index_array));

  // Second, send a touch release for finger 0, a touch move for finger 1.
  param0.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  param1.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  param1.set_position(gfx::PointF(183, 239));
  param_list.clear();
  param_list.push_back(param0);
  param_list.push_back(param1);
  params.PushPointerActionParamsList(param_list);
  gesture.reset(new SyntheticPointerAction(params));
  QueueSyntheticGesture(std::move(gesture));
  pointer_touch_target->reset_num_dispatched_pointer_actions();
  FlushInputUntilComplete();

  index_array[1] = 0;
  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_touch_target->num_dispatched_pointer_actions(), 4);
  EXPECT_TRUE(pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
      param_list, 2, index_array));

  // Third, send a touch release for finger 1.
  param1.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  param_list.clear();
  param_list.push_back(param1);
  params.PushPointerActionParamsList(param_list);
  gesture.reset(new SyntheticPointerAction(params));
  QueueSyntheticGesture(std::move(gesture));
  pointer_touch_target->reset_num_dispatched_pointer_actions();
  FlushInputUntilComplete();

  EXPECT_EQ(3, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_touch_target->num_dispatched_pointer_actions(), 5);
  EXPECT_TRUE(pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
      param_list, 4, index_array));
}

TEST_F(SyntheticGestureControllerTest, PointerMouseAction) {
  CreateControllerAndTarget<MockSyntheticPointerMouseActionTarget>();

  // First, send a mouse move.
  SyntheticPointerActionListParams::ParamList param_list;
  SyntheticPointerActionParams param = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::MOVE);

  param.set_position(gfx::PointF(54, 89));
  SyntheticPointerActionListParams params;
  params.PushPointerActionParams(param);
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  std::unique_ptr<SyntheticPointerAction> gesture(
      new SyntheticPointerAction(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockSyntheticPointerMouseActionTarget* pointer_mouse_target =
      static_cast<MockSyntheticPointerMouseActionTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_mouse_target->num_dispatched_pointer_actions(), 1);
  EXPECT_TRUE(
      pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(param, 0));

  // Second, send a mouse press.
  param.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param.set_position(gfx::PointF(183, 239));
  params.PushPointerActionParams(param);
  gesture.reset(new SyntheticPointerAction(params));
  QueueSyntheticGesture(std::move(gesture));
  pointer_mouse_target->reset_num_dispatched_pointer_actions();
  FlushInputUntilComplete();

  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_mouse_target->num_dispatched_pointer_actions(), 2);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param, 1, SyntheticPointerActionParams::Button::LEFT));

  // Third, send a mouse move.
  param.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  param.set_position(gfx::PointF(254, 279));
  params.PushPointerActionParams(param);
  gesture.reset(new SyntheticPointerAction(params));
  QueueSyntheticGesture(std::move(gesture));
  pointer_mouse_target->reset_num_dispatched_pointer_actions();
  FlushInputUntilComplete();

  EXPECT_EQ(3, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_mouse_target->num_dispatched_pointer_actions(), 3);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param, 0, SyntheticPointerActionParams::Button::LEFT));

  // Fourth, send a mouse release.
  param.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  params.PushPointerActionParams(param);
  gesture.reset(new SyntheticPointerAction(params));
  QueueSyntheticGesture(std::move(gesture));
  pointer_mouse_target->reset_num_dispatched_pointer_actions();
  FlushInputUntilComplete();

  EXPECT_EQ(4, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_mouse_target->num_dispatched_pointer_actions(), 4);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param, 1, SyntheticPointerActionParams::Button::LEFT));
}

TEST_F(SyntheticGestureControllerTest, PointerPenAction) {
  CreateControllerAndTarget<MockSyntheticPointerMouseActionTarget>();

  // First, send a pen move.
  SyntheticPointerActionListParams::ParamList param_list;
  SyntheticPointerActionParams param = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::MOVE);

  param.set_position(gfx::PointF(54, 89));
  SyntheticPointerActionListParams params;
  params.PushPointerActionParams(param);
  params.gesture_source_type = SyntheticGestureParams::PEN_INPUT;
  std::unique_ptr<SyntheticPointerAction> gesture(
      new SyntheticPointerAction(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockSyntheticPointerMouseActionTarget* pointer_pen_target =
      static_cast<MockSyntheticPointerMouseActionTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_pen_target->num_dispatched_pointer_actions(), 1);
  EXPECT_TRUE(
      pointer_pen_target->SyntheticMouseActionDispatchedCorrectly(param, 0));

  // Second, send a pen press.
  param.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param.set_position(gfx::PointF(183, 239));
  params.PushPointerActionParams(param);
  gesture.reset(new SyntheticPointerAction(params));
  QueueSyntheticGesture(std::move(gesture));
  pointer_pen_target->reset_num_dispatched_pointer_actions();
  FlushInputUntilComplete();

  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_pen_target->num_dispatched_pointer_actions(), 2);
  EXPECT_TRUE(pointer_pen_target->SyntheticMouseActionDispatchedCorrectly(
      param, 1, SyntheticPointerActionParams::Button::LEFT));

  // Third, send a pen move.
  param.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  param.set_position(gfx::PointF(254, 279));
  params.PushPointerActionParams(param);
  gesture.reset(new SyntheticPointerAction(params));
  QueueSyntheticGesture(std::move(gesture));
  pointer_pen_target->reset_num_dispatched_pointer_actions();
  FlushInputUntilComplete();

  EXPECT_EQ(3, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_pen_target->num_dispatched_pointer_actions(), 3);
  EXPECT_TRUE(pointer_pen_target->SyntheticMouseActionDispatchedCorrectly(
      param, 0, SyntheticPointerActionParams::Button::LEFT));

  // Fourth, send a pen release.
  param.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  params.PushPointerActionParams(param);
  gesture.reset(new SyntheticPointerAction(params));
  QueueSyntheticGesture(std::move(gesture));
  pointer_pen_target->reset_num_dispatched_pointer_actions();
  FlushInputUntilComplete();

  EXPECT_EQ(4, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_pen_target->num_dispatched_pointer_actions(), 4);
  EXPECT_TRUE(pointer_pen_target->SyntheticMouseActionDispatchedCorrectly(
      param, 1, SyntheticPointerActionParams::Button::LEFT));

  // Fifth, send a pen leave.
  param.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::LEAVE);
  params.PushPointerActionParams(param);
  gesture.reset(new SyntheticPointerAction(params));
  QueueSyntheticGesture(std::move(gesture));
  pointer_pen_target->reset_num_dispatched_pointer_actions();
  FlushInputUntilComplete();

  EXPECT_EQ(5, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_pen_target->num_dispatched_pointer_actions(), 5);
  EXPECT_TRUE(
      pointer_pen_target->SyntheticMouseActionDispatchedCorrectly(param, 0));
}

class MockSyntheticGestureTargetManualAck : public MockSyntheticGestureTarget {
 public:
  void WaitForTargetAck(SyntheticGestureParams::GestureType type,
                        SyntheticGestureParams::GestureSourceType source,
                        base::OnceClosure callback) const override {
    if (manually_ack_)
      target_ack_ = std::move(callback);
    else
      std::move(callback).Run();
  }
  bool HasOutstandingAck() const { return !target_ack_.is_null(); }
  void InvokeAck() {
    DCHECK(HasOutstandingAck());
    std::move(target_ack_).Run();
  }
  void SetManuallyAck(bool manually_ack) { manually_ack_ = manually_ack; }

 private:
  mutable base::OnceClosure target_ack_;
  bool manually_ack_ = true;
};

// Ensure the first time a gesture is queued, we wait for a renderer ACK before
// starting the gesture. Following gestures should start immediately. This test
// the renderer_known_to_be_initialized_ bit in the controller.
TEST_F(SyntheticGestureControllerTest, WaitForRendererInitialization) {
  CreateControllerAndTarget<MockSyntheticGestureTargetManualAck>();

  auto* target = static_cast<MockSyntheticGestureTargetManualAck*>(target_);

  EXPECT_FALSE(target->HasOutstandingAck());

  SyntheticTapGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.duration_ms = 123;
  params.position.SetPoint(87, -124);

  // Queue the first gesture.
  {
    auto gesture = std::make_unique<SyntheticTapGesture>(params);
    QueueSyntheticGesture(std::move(gesture));

    // We should have received a WaitForTargetAck and the dispatch timer won't
    // start until that's ACK'd.
    EXPECT_TRUE(target->HasOutstandingAck());
    EXPECT_FALSE(DispatchTimerRunning());

    target->InvokeAck();

    // The timer should now be running.
    EXPECT_FALSE(target->HasOutstandingAck());
    EXPECT_TRUE(DispatchTimerRunning());
  }

  // Finish the gesture.
  {
    target->SetManuallyAck(false);
    FlushInputUntilComplete();
    target->SetManuallyAck(true);
    EXPECT_FALSE(DispatchTimerRunning());
  }

  // Queue the second gesture.
  {
    auto gesture = std::make_unique<SyntheticTapGesture>(params);
    QueueSyntheticGesture(std::move(gesture));

    // This time, because we've already sent a gesuture to the renderer,
    // there's no need to wait for an ACK before starting the dispatch timer.
    EXPECT_FALSE(target->HasOutstandingAck());
    EXPECT_TRUE(DispatchTimerRunning());
  }
}

}  // namespace content
