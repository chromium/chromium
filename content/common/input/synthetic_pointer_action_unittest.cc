// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/synthetic_pointer_action.h"

#include <array>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/input/synthetic_gesture.h"
#include "content/common/input/synthetic_gesture_controller.h"
#include "content/common/input/synthetic_gesture_target.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"

using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebMouseEvent;
using blink::WebTouchPoint;

namespace content {

WebTouchPoint::State ToWebTouchPointState(
    SyntheticPointerActionParams::PointerActionType action_type) {
  switch (action_type) {
    case SyntheticPointerActionParams::PointerActionType::PRESS:
      return WebTouchPoint::State::kStatePressed;
    case SyntheticPointerActionParams::PointerActionType::MOVE:
      return WebTouchPoint::State::kStateMoved;
    case SyntheticPointerActionParams::PointerActionType::RELEASE:
      return WebTouchPoint::State::kStateReleased;
    case SyntheticPointerActionParams::PointerActionType::CANCEL:
      return WebTouchPoint::State::kStateCancelled;
    case SyntheticPointerActionParams::PointerActionType::IDLE:
      return WebTouchPoint::State::kStateStationary;
    case SyntheticPointerActionParams::PointerActionType::LEAVE:
    case SyntheticPointerActionParams::PointerActionType::NOT_INITIALIZED:
      NOTREACHED_IN_MIGRATION()
          << "Invalid SyntheticPointerActionParams::PointerActionType.";
      return WebTouchPoint::State::kStateUndefined;
  }
  NOTREACHED_IN_MIGRATION()
      << "Invalid SyntheticPointerActionParams::PointerActionType.";
  return WebTouchPoint::State::kStateUndefined;
}

WebInputEvent::Type ToWebMouseEventType(
    SyntheticPointerActionParams::PointerActionType action_type) {
  switch (action_type) {
    case SyntheticPointerActionParams::PointerActionType::PRESS:
      return WebInputEvent::Type::kMouseDown;
    case SyntheticPointerActionParams::PointerActionType::MOVE:
      return WebInputEvent::Type::kMouseMove;
    case SyntheticPointerActionParams::PointerActionType::RELEASE:
      return WebInputEvent::Type::kMouseUp;
    case SyntheticPointerActionParams::PointerActionType::LEAVE:
      return WebInputEvent::Type::kMouseLeave;
    case SyntheticPointerActionParams::PointerActionType::CANCEL:
    case SyntheticPointerActionParams::PointerActionType::IDLE:
    case SyntheticPointerActionParams::PointerActionType::NOT_INITIALIZED:
      NOTREACHED_IN_MIGRATION()
          << "Invalid SyntheticPointerActionParams::PointerActionType.";
      return WebInputEvent::Type::kUndefined;
  }
  NOTREACHED_IN_MIGRATION()
      << "Invalid SyntheticPointerActionParams::PointerActionType.";
  return WebInputEvent::Type::kUndefined;
}

WebInputEvent::Type WebTouchPointStateToEventType(
    blink::WebTouchPoint::State state) {
  switch (state) {
    case blink::WebTouchPoint::State::kStateReleased:
      return WebInputEvent::Type::kTouchEnd;
    case blink::WebTouchPoint::State::kStatePressed:
      return WebInputEvent::Type::kTouchStart;
    case blink::WebTouchPoint::State::kStateMoved:
      return WebInputEvent::Type::kTouchMove;
    case blink::WebTouchPoint::State::kStateCancelled:
      return WebInputEvent::Type::kTouchCancel;
    default:
      return WebInputEvent::Type::kUndefined;
  }
}

class MockSyntheticPointerActionTarget : public SyntheticGestureTarget {
 public:
  MockSyntheticPointerActionTarget() {}
  ~MockSyntheticPointerActionTarget() override {}

  base::TimeDelta PointerAssumedStoppedTime() const override {
    NOTIMPLEMENTED();
    return base::TimeDelta();
  }

  float GetTouchSlopInDips() const override {
    NOTIMPLEMENTED();
    return 0.0f;
  }

  float GetSpanSlopInDips() const override {
    NOTIMPLEMENTED();
    return 0.0f;
  }

  float GetMinScalingSpanInDips() const override {
    NOTIMPLEMENTED();
    return 0.0f;
  }

  int GetMouseWheelMinimumGranularity() const override {
    NOTIMPLEMENTED();
    return 0.0f;
  }

  void WaitForTargetAck(SyntheticGestureParams::GestureType type,
                        content::mojom::GestureSourceType source,
                        base::OnceClosure callback) const override {
    std::move(callback).Run();
  }

  void GetVSyncParameters(base::TimeTicks& timebase,
                          base::TimeDelta& interval) const override {
    timebase = base::TimeTicks();
    interval = base::Microseconds(16667);
  }

  WebInputEvent::Type type() const { return type_; }

  void ExpectFromDebugger() { expect_from_debugger_ = true; }

 protected:
  WebInputEvent::Type type_;
  bool expect_from_debugger_ = false;
};

class MockSyntheticPointerTouchActionTarget
    : public MockSyntheticPointerActionTarget {
 public:
  MockSyntheticPointerTouchActionTarget()
      : num_dispatched_pointer_actions_(0) {}
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
      widths_[num_dispatched_pointer_actions_] =
          2 * touch_event.touches[i].radius_x;
      heights_[num_dispatched_pointer_actions_] =
          2 * touch_event.touches[i].radius_y;
      rotation_angles_[num_dispatched_pointer_actions_] =
          touch_event.touches[i].rotation_angle;
      forces_[num_dispatched_pointer_actions_] = touch_event.touches[i].force;
      timestamps_[num_dispatched_pointer_actions_] = touch_event.TimeStamp();
      modifiers_[num_dispatched_pointer_actions_] = touch_event.GetModifiers();
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
      if (indexes_[index] != param.pointer_id()) {
        return testing::AssertionFailure()
               << "Pointer index at index " << index << " was "
               << indexes_[index] << ", expected " << param.pointer_id() << ".";
      }

      if (positions_[index] != param.position()) {
        return testing::AssertionFailure()
               << "Pointer position at index " << index << " was "
               << positions_[index].ToString() << ", expected "
               << param.position().ToString() << ".";
      }

      if (widths_[index] != param.width()) {
        return testing::AssertionFailure()
               << "Pointer width at index " << index << " was "
               << widths_[index] << ", expected " << param.width() << ".";
      }

      if (heights_[index] != param.height()) {
        return testing::AssertionFailure()
               << "Pointer height at index " << index << " was "
               << heights_[index] << ", expected " << param.height() << ".";
      }

      if (rotation_angles_[index] != param.rotation_angle()) {
        return testing::AssertionFailure()
               << "Pointer rotation_angle at index " << index << " was "
               << rotation_angles_[index] << ", expected "
               << param.rotation_angle() << ".";
      }

      if (forces_[index] != param.force()) {
        return testing::AssertionFailure()
               << "Pointer force at index " << index << " was "
               << forces_[index] << ", expected " << param.force() << ".";
      }
    }

    if (states_[index] != ToWebTouchPointState(param.pointer_action_type())) {
      return testing::AssertionFailure()
             << "Pointer states at index " << index << " was " << states_[index]
             << ", expected "
             << ToWebTouchPointState(param.pointer_action_type()) << ".";
    }

    if (!param.timestamp().is_null() &&
        timestamps_[index] != param.timestamp()) {
      return testing::AssertionFailure()
             << "Dispatched event's time stamp  was " << timestamps_[index]
             << ", expected " << param.timestamp() << ".";
    }
    if (expect_from_debugger_ &&
        !(modifiers_[index] & blink::WebInputEvent::kFromDebugger)) {
      return testing::AssertionFailure()
             << "Dispatched event's modifers did not include expected "
                "kFromDebugger bit.";
    } else if (!expect_from_debugger_ &&
               (modifiers_[index] & blink::WebInputEvent::kFromDebugger)) {
      return testing::AssertionFailure()
             << "Dispatched event's modifers included unexpected "
                "kFromDebugger bit.";
    }
    return testing::AssertionSuccess();
  }

  testing::AssertionResult SyntheticTouchActionListDispatchedCorrectly(
      const std::vector<SyntheticPointerActionParams>& params_list,
      const std::vector<int>& index_array) {
    testing::AssertionResult result = testing::AssertionSuccess();
    num_dispatched_pointer_actions_ = 0;
    int result_index = 0;
    for (size_t i = 0; i < params_list.size(); ++i) {
      if (params_list[i].pointer_action_type() !=
          SyntheticPointerActionParams::PointerActionType::IDLE)
        result = SyntheticTouchActionDispatchedCorrectly(
            params_list[i], result_index++, index_array[i]);
      if (result == testing::AssertionFailure())
        return result;
    }
    return testing::AssertionSuccess();
  }

  content::mojom::GestureSourceType GetDefaultSyntheticGestureSourceType()
      const override {
    return content::mojom::GestureSourceType::kTouchInput;
  }

 private:
  int num_dispatched_pointer_actions_;
  std::array<gfx::PointF, WebTouchEvent::kTouchesLengthCap> positions_;
  std::array<uint32_t, WebTouchEvent::kTouchesLengthCap> indexes_;
  std::array<WebTouchPoint::State, WebTouchEvent::kTouchesLengthCap> states_;
  std::array<float, WebTouchEvent::kTouchesLengthCap> widths_;
  std::array<float, WebTouchEvent::kTouchesLengthCap> heights_;
  std::array<float, WebTouchEvent::kTouchesLengthCap> rotation_angles_;
  std::array<float, WebTouchEvent::kTouchesLengthCap> forces_;
  std::array<base::TimeTicks, WebTouchEvent::kTouchesLengthCap> timestamps_;
  std::array<int, WebTouchEvent::kTouchesLengthCap> modifiers_;
};

class MockSyntheticPointerMouseActionTarget
    : public MockSyntheticPointerActionTarget {
 public:
  MockSyntheticPointerMouseActionTarget() : click_count_(0), modifiers_(0) {}
  ~MockSyntheticPointerMouseActionTarget() override {}

  void DispatchInputEventToPlatform(const WebInputEvent& event) override {
    DCHECK(WebInputEvent::IsMouseEventType(event.GetType()));
    const WebMouseEvent& mouse_event = static_cast<const WebMouseEvent&>(event);
    type_ = mouse_event.GetType();
    position_ = gfx::PointF(mouse_event.PositionInWidget());
    click_count_ = mouse_event.click_count;
    modifiers_ = mouse_event.GetModifiers();
    button_ = mouse_event.button;
    timestamp_ = mouse_event.TimeStamp();
  }

  testing::AssertionResult SyntheticMouseActionDispatchedCorrectly(
      const SyntheticPointerActionParams& param,
      int click_count,
      std::vector<SyntheticPointerActionParams::Button> buttons,
      SyntheticPointerActionParams::Button button =
          SyntheticPointerActionParams::Button::NO_BUTTON,
      content::mojom::GestureSourceType source_type =
          content::mojom::GestureSourceType::kMouseInput) {
    if (GetDefaultSyntheticGestureSourceType() != source_type) {
      return testing::AssertionFailure()
             << "Pointer source type was "
             << static_cast<int>(GetDefaultSyntheticGestureSourceType())
             << ", expected " << static_cast<int>(source_type) << ".";
    }

    if (type_ != ToWebMouseEventType(param.pointer_action_type())) {
      return testing::AssertionFailure()
             << "Pointer type was " << WebInputEvent::GetName(type_)
             << ", expected " << WebInputEvent::GetName(ToWebMouseEventType(
             param.pointer_action_type())) << ".";
    }

    if (click_count_ != click_count) {
      return testing::AssertionFailure() << "Pointer click count was "
                                         << click_count_ << ", expected "
                                         << click_count << ".";
    }

    if (button_ != WebMouseEvent::Button::kNoButton) {
      if (param.pointer_action_type() ==
              SyntheticPointerActionParams::PointerActionType::PRESS ||
          param.pointer_action_type() ==
              SyntheticPointerActionParams::PointerActionType::RELEASE) {
        if (click_count_ < 1) {
          return testing::AssertionFailure()
                 << "Pointer click count was " << click_count_ << ", expected "
                 << "greater or equal to 1.";
        }
      }

      if (param.pointer_action_type() ==
          SyntheticPointerActionParams::PointerActionType::MOVE) {
        if (click_count_ != 0) {
          return testing::AssertionFailure() << "Pointer click count was "
                                             << click_count_ << ", expected 0.";
        }
      }

      if (button_ !=
          SyntheticPointerActionParams::GetWebMouseEventButton(button)) {
        return testing::AssertionFailure()
               << "Pointer button was " << static_cast<int>(button_)
               << ", expected " << static_cast<int>(button) << ".";
      }
    }

    int modifiers = 0;
    for (size_t index = 0; index < buttons.size(); ++index) {
      modifiers |= SyntheticPointerActionParams::GetWebMouseEventModifier(
          buttons[index]);
    }
    modifiers |= param.key_modifiers();
    if (expect_from_debugger_)
      modifiers |= blink::WebInputEvent::kFromDebugger;

    if (modifiers_ != modifiers) {
      return testing::AssertionFailure() << "Pointer modifiers was "
                                         << modifiers_ << ", expected "
                                         << modifiers << ".";
    }

    if ((param.pointer_action_type() ==
             SyntheticPointerActionParams::PointerActionType::PRESS ||
         param.pointer_action_type() ==
             SyntheticPointerActionParams::PointerActionType::MOVE) &&
        position_ != param.position()) {
      return testing::AssertionFailure()
             << "Pointer position was " << position_.ToString()
             << ", expected " << param.position().ToString() << ".";
    }

    if (!param.timestamp().is_null() && timestamp_ != param.timestamp()) {
      return testing::AssertionFailure()
             << "Dispatched event's time stamp  was " << timestamp_
             << ", expected " << param.timestamp() << ".";
    }
    return testing::AssertionSuccess();
  }

  content::mojom::GestureSourceType GetDefaultSyntheticGestureSourceType()
      const override {
    return content::mojom::GestureSourceType::kMouseInput;
  }

 private:
  gfx::PointF position_;
  int click_count_;
  int modifiers_;
  WebMouseEvent::Button button_;
  base::TimeTicks timestamp_;
};

class MockSyntheticPointerPenActionTarget
    : public MockSyntheticPointerMouseActionTarget {
 public:
  MockSyntheticPointerPenActionTarget() {}
  ~MockSyntheticPointerPenActionTarget() override {}

  content::mojom::GestureSourceType GetDefaultSyntheticGestureSourceType()
      const override {
    return content::mojom::GestureSourceType::kPenInput;
  }
};

class DummySyntheticGestureControllerDelegate
    : public SyntheticGestureController::Delegate {
 public:
  DummySyntheticGestureControllerDelegate() = default;

  DummySyntheticGestureControllerDelegate(
      const DummySyntheticGestureControllerDelegate&) = delete;
  DummySyntheticGestureControllerDelegate& operator=(
      const DummySyntheticGestureControllerDelegate&) = delete;

  ~DummySyntheticGestureControllerDelegate() override = default;

 private:
  // SyntheticGestureController::Delegate:
  bool HasGestureStopped() override { return true; }
  bool IsHidden() const override { return false; }
};

class SyntheticPointerActionTest : public testing::Test {
 public:
  SyntheticPointerActionTest() {
    params_ = SyntheticPointerActionListParams();
    num_success_ = 0;
    num_failure_ = 0;
  }
  ~SyntheticPointerActionTest() override {}

 protected:
  template <typename MockGestureTarget>
  void CreateSyntheticPointerActionTargetAndController() {
    auto target = std::make_unique<MockGestureTarget>();
    target_ = target.get();
    synthetic_pointer_driver_ = SyntheticPointerDriver::Create(
        target_->GetDefaultSyntheticGestureSourceType());
    controller_ = std::make_unique<SyntheticGestureController>(
        &controller_delegate_, std::move(target),
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }

  std::unique_ptr<SyntheticPointerAction> CreatePointerAction(
      const SyntheticPointerActionListParams& params) {
    DCHECK(controller_);
    auto pointer_action = std::make_unique<SyntheticPointerAction>(params_);
    pointer_action->DidQueue(controller_->GetWeakPtr());
    return pointer_action;
  }

  void ForwardSyntheticPointerAction() {
    SyntheticGesture::Result result = pointer_action_->ForwardInputEvents(
        base::TimeTicks::Now(), target_.get());

    if (result == SyntheticGesture::GESTURE_FINISHED ||
        result == SyntheticGesture::GESTURE_RUNNING)
      num_success_++;
    else
      num_failure_++;
  }

  int num_success_;
  int num_failure_;
  std::unique_ptr<SyntheticGestureController> controller_;
  DummySyntheticGestureControllerDelegate controller_delegate_;
  raw_ptr<MockSyntheticPointerActionTarget> target_;
  std::unique_ptr<SyntheticPointerAction> pointer_action_;
  std::unique_ptr<SyntheticPointerDriver> synthetic_pointer_driver_;
  SyntheticPointerActionListParams params_;
  base::test::TaskEnvironment env_;
};

TEST_F(SyntheticPointerActionTest, PointerTouchAction) {
  CreateSyntheticPointerActionTargetAndController<
      MockSyntheticPointerTouchActionTarget>();

  // Send a touch press for one finger.
  SyntheticPointerActionParams param1 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param1.set_pointer_id(0);
  param1.set_position(gfx::PointF(54, 89));
  param1.set_width(30);
  param1.set_height(45);
  param1.set_rotation_angle(10);
  param1.set_force(15);
  SyntheticPointerActionListParams::ParamList param_list1;
  param_list1.push_back(param1);
  params_.PushPointerActionParamsList(param_list1);

  // Send a touch move for the first finger and a touch press for the second
  // finger.
  param1.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  param1.set_position(gfx::PointF(133, 156));
  SyntheticPointerActionParams param2 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param2.set_pointer_id(1);
  param2.set_position(gfx::PointF(79, 132));
  param2.set_width(10);
  param2.set_height(35);
  param2.set_rotation_angle(30);
  param2.set_force(10);
  SyntheticPointerActionListParams::ParamList param_list2;
  param_list2.push_back(param1);
  param_list2.push_back(param2);
  params_.PushPointerActionParamsList(param_list2);

  // Send a touch move for the second finger.
  param1.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::IDLE);
  param2.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  param2.set_position(gfx::PointF(87, 253));
  SyntheticPointerActionListParams::ParamList param_list3;
  param_list3.push_back(param1);
  param_list3.push_back(param2);
  params_.PushPointerActionParamsList(param_list3);

  // Send touch releases for both fingers.
  SyntheticPointerActionListParams::ParamList param_list4;
  param1.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  param2.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  param_list4.push_back(param1);
  param_list4.push_back(param2);
  params_.PushPointerActionParamsList(param_list4);
  pointer_action_ = CreatePointerAction(params_);

  ForwardSyntheticPointerAction();
  MockSyntheticPointerTouchActionTarget* pointer_touch_target =
      static_cast<MockSyntheticPointerTouchActionTarget*>(target_.get());
  std::vector<int> index_array = {0, 1};
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::Type::kTouchStart);
  EXPECT_TRUE(pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
      param_list1, index_array));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
  // The type of the SyntheticWebTouchEvent is the action of the last finger.
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::Type::kTouchStart);
  EXPECT_TRUE(pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
      param_list2, index_array));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(3, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::Type::kTouchMove);
  EXPECT_TRUE(pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
      param_list3, index_array));

  ForwardSyntheticPointerAction();
  index_array[1] = 0;
  EXPECT_EQ(4, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::Type::kTouchEnd);
  EXPECT_TRUE(pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
      param_list4, index_array));
}

TEST_F(SyntheticPointerActionTest, PointerTouchActionsMultiPressRelease) {
  CreateSyntheticPointerActionTargetAndController<
      MockSyntheticPointerTouchActionTarget>();
  int count_success = 1;

  // Send a touch press for one finger.
  SyntheticPointerActionParams param1 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param1.set_pointer_id(0);
  param1.set_position(gfx::PointF(54, 89));
  SyntheticPointerActionListParams::ParamList param_list1;
  param_list1.push_back(param1);
  params_.PushPointerActionParamsList(param_list1);

  SyntheticPointerActionParams param2 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param2.set_pointer_id(1);
  param2.set_position(gfx::PointF(123, 69));
  param1.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::IDLE);
  SyntheticPointerActionListParams::ParamList param_list2;
  param_list2.push_back(param1);
  param_list2.push_back(param2);

  param2.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  SyntheticPointerActionListParams::ParamList param_list3;
  param_list3.push_back(param1);
  param_list3.push_back(param2);
  for (int i = 0; i < 3; ++i) {
    // Send a touch press for the second finger and not move the first finger.
    params_.PushPointerActionParamsList(param_list2);

    // Send a touch release for the second finger and not move the first finger.
    params_.PushPointerActionParamsList(param_list3);
  }
  pointer_action_ = CreatePointerAction(params_);

  ForwardSyntheticPointerAction();
  MockSyntheticPointerTouchActionTarget* pointer_touch_target =
      static_cast<MockSyntheticPointerTouchActionTarget*>(target_.get());
  std::vector<int> index_array = {0, 1};
  EXPECT_EQ(count_success++, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::Type::kTouchStart);
  EXPECT_TRUE(pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
      param_list1, index_array));

  for (int index = 1; index < 4; ++index) {
    ForwardSyntheticPointerAction();
    EXPECT_EQ(count_success++, num_success_);
    EXPECT_EQ(0, num_failure_);
    // The type of the SyntheticWebTouchEvent is the action of the last finger.
    EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::Type::kTouchStart);
    EXPECT_TRUE(
        pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
            param_list2, index_array));

    ForwardSyntheticPointerAction();
    EXPECT_EQ(count_success++, num_success_);
    EXPECT_EQ(0, num_failure_);
    // The type of the SyntheticWebTouchEvent is the action of the last finger.
    EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::Type::kTouchEnd);
    EXPECT_TRUE(
        pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
            param_list3, index_array));
  }
}

TEST_F(SyntheticPointerActionTest, PointerTouchActionCancel) {
  CreateSyntheticPointerActionTargetAndController<
      MockSyntheticPointerTouchActionTarget>();

  // Send a touch press for one finger.
  SyntheticPointerActionParams param1 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param1.set_pointer_id(0);
  param1.set_position(gfx::PointF(54, 89));
  SyntheticPointerActionListParams::ParamList param_list1;
  param_list1.push_back(param1);
  params_.PushPointerActionParamsList(param_list1);

  // Send a touch move for the first finger and a touch press for the second
  // finger.
  param1.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  param1.set_position(gfx::PointF(133, 156));
  SyntheticPointerActionParams param2 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param2.set_pointer_id(1);
  param2.set_position(gfx::PointF(79, 132));
  SyntheticPointerActionListParams::ParamList param_list2;
  param_list2.push_back(param1);
  param_list2.push_back(param2);
  params_.PushPointerActionParamsList(param_list2);

  // Send touch cancel for both fingers.
  SyntheticPointerActionListParams::ParamList param_list3;
  param1.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::CANCEL);
  param2.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::CANCEL);
  param_list3.push_back(param1);
  param_list3.push_back(param2);
  params_.PushPointerActionParamsList(param_list3);
  pointer_action_ = CreatePointerAction(params_);

  ForwardSyntheticPointerAction();
  MockSyntheticPointerTouchActionTarget* pointer_touch_target =
      static_cast<MockSyntheticPointerTouchActionTarget*>(target_.get());
  std::vector<int> index_array = {0, 1};
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::Type::kTouchStart);
  EXPECT_TRUE(pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
      param_list1, index_array));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
  // The type of the SyntheticWebTouchEvent is the action of the last finger.
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::Type::kTouchStart);
  EXPECT_TRUE(pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
      param_list2, index_array));

  ForwardSyntheticPointerAction();
  index_array[1] = 0;
  EXPECT_EQ(3, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::Type::kTouchCancel);
  EXPECT_TRUE(pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
      param_list3, index_array));
}

TEST_F(SyntheticPointerActionTest, PointerTouchActionTypeInvalid) {
  CreateSyntheticPointerActionTargetAndController<
      MockSyntheticPointerTouchActionTarget>();

  // Cannot send a touch move or touch release without sending a touch press
  // first.
  SyntheticPointerActionParams param = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  param.set_pointer_id(0);
  param.set_position(gfx::PointF(54, 89));
  params_.PushPointerActionParams(param);
  pointer_action_ = CreatePointerAction(params_);

  ForwardSyntheticPointerAction();
  EXPECT_EQ(0, num_success_);
  EXPECT_EQ(1, num_failure_);

  param.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  params_ = SyntheticPointerActionListParams();
  params_.PushPointerActionParams(param);
  pointer_action_ = CreatePointerAction(params_);

  ForwardSyntheticPointerAction();
  EXPECT_EQ(0, num_success_);
  EXPECT_EQ(2, num_failure_);

  // Send a touch press for one finger.
  param.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  params_ = SyntheticPointerActionListParams();
  params_.PushPointerActionParams(param);
  params_.PushPointerActionParams(param);
  pointer_action_ = CreatePointerAction(params_);

  ForwardSyntheticPointerAction();
  MockSyntheticPointerTouchActionTarget* pointer_touch_target =
      static_cast<MockSyntheticPointerTouchActionTarget*>(target_.get());
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(2, num_failure_);
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::Type::kTouchStart);
  EXPECT_TRUE(pointer_touch_target->SyntheticTouchActionDispatchedCorrectly(
      param, 0, 0));

  // Cannot send a touch press again without releasing the finger.
  ForwardSyntheticPointerAction();
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(3, num_failure_);
}

TEST_F(SyntheticPointerActionTest, PointerTouchActionFromDebugger) {
  CreateSyntheticPointerActionTargetAndController<
      MockSyntheticPointerTouchActionTarget>();
  target_->ExpectFromDebugger();
  params_.from_devtools_debugger = true;

  // Send a touch press for one finger.
  SyntheticPointerActionParams param1 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param1.set_pointer_id(0);
  param1.set_position(gfx::PointF(54, 89));
  param1.set_width(30);
  param1.set_height(45);
  param1.set_rotation_angle(10);
  param1.set_force(15);
  SyntheticPointerActionListParams::ParamList param_list1;
  param_list1.push_back(param1);
  params_.PushPointerActionParamsList(param_list1);

  // Send a touch move for the first finger and a touch press for the second
  // finger.
  param1.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  param1.set_position(gfx::PointF(133, 156));
  SyntheticPointerActionParams param2 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param2.set_pointer_id(1);
  param2.set_position(gfx::PointF(79, 132));
  param2.set_width(10);
  param2.set_height(35);
  param2.set_rotation_angle(30);
  param2.set_force(10);
  SyntheticPointerActionListParams::ParamList param_list2;
  param_list2.push_back(param1);
  param_list2.push_back(param2);
  params_.PushPointerActionParamsList(param_list2);
  pointer_action_ = CreatePointerAction(params_);

  ForwardSyntheticPointerAction();
  MockSyntheticPointerTouchActionTarget* pointer_touch_target =
      static_cast<MockSyntheticPointerTouchActionTarget*>(target_.get());
  std::vector<int> index_array = {0, 1};
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
      param_list1, index_array));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
      param_list2, index_array));
}

TEST_F(SyntheticPointerActionTest, PointerMouseAction) {
  CreateSyntheticPointerActionTargetAndController<
      MockSyntheticPointerMouseActionTarget>();

  // Send a mouse move.
  SyntheticPointerActionParams param1 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  param1.set_position(gfx::PointF(189, 62));
  params_.PushPointerActionParams(param1);

  // Send a mouse down.
  SyntheticPointerActionParams param2 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param2.set_position(gfx::PointF(189, 62));
  params_.PushPointerActionParams(param2);

  // Send a mouse drag.
  SyntheticPointerActionParams param3 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  param3.set_position(gfx::PointF(326, 298));
  params_.PushPointerActionParams(param3);

  // Send a mouse up.
  SyntheticPointerActionParams param4 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  params_.PushPointerActionParams(param4);
  pointer_action_ = CreatePointerAction(params_);

  ForwardSyntheticPointerAction();
  MockSyntheticPointerMouseActionTarget* pointer_mouse_target =
      static_cast<MockSyntheticPointerMouseActionTarget*>(target_.get());
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  std::vector<SyntheticPointerActionParams::Button> buttons;
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param1, 0, buttons));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
  buttons.push_back(SyntheticPointerActionParams::Button::LEFT);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param2, 1, buttons, SyntheticPointerActionParams::Button::LEFT));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(3, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param3, 0, buttons, SyntheticPointerActionParams::Button::LEFT));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(4, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param4, 1, buttons, SyntheticPointerActionParams::Button::LEFT));
}

TEST_F(SyntheticPointerActionTest, PointerMouseActionMultiPress) {
  CreateSyntheticPointerActionTargetAndController<
      MockSyntheticPointerMouseActionTarget>();

  // Press a mouse's left button.
  SyntheticPointerActionParams param1 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param1.set_position(gfx::PointF(189, 62));
  param1.set_button(SyntheticPointerActionParams::Button::LEFT);
  params_.PushPointerActionParams(param1);

  // Move the mouse while left button is pressed.
  SyntheticPointerActionParams param2 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  param2.set_position(gfx::PointF(139, 98));
  params_.PushPointerActionParams(param2);

  // Press the mouse's middle button.
  SyntheticPointerActionParams param3 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param3.set_position(gfx::PointF(139, 98));
  param3.set_button(SyntheticPointerActionParams::Button::MIDDLE);
  params_.PushPointerActionParams(param3);

  // Move the mouse while left and middle buttons are pressed.
  SyntheticPointerActionParams param4 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  param4.set_position(gfx::PointF(86, 69));
  params_.PushPointerActionParams(param4);

  // Release a mouse's middle button.
  SyntheticPointerActionParams param5 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  param5.set_button(SyntheticPointerActionParams::Button::MIDDLE);
  params_.PushPointerActionParams(param5);

  // Release a mouse's left button.
  SyntheticPointerActionParams param6 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  param6.set_button(SyntheticPointerActionParams::Button::LEFT);
  params_.PushPointerActionParams(param6);
  pointer_action_ = CreatePointerAction(params_);

  ForwardSyntheticPointerAction();
  MockSyntheticPointerMouseActionTarget* pointer_mouse_target =
      static_cast<MockSyntheticPointerMouseActionTarget*>(target_.get());
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  std::vector<SyntheticPointerActionParams::Button> buttons(
      1, SyntheticPointerActionParams::Button::LEFT);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param1, 1, buttons, SyntheticPointerActionParams::Button::LEFT));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
  buttons.push_back(SyntheticPointerActionParams::Button::LEFT);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param2, 0, buttons, SyntheticPointerActionParams::Button::LEFT));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(3, num_success_);
  EXPECT_EQ(0, num_failure_);
  buttons.push_back(SyntheticPointerActionParams::Button::MIDDLE);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param3, 1, buttons, SyntheticPointerActionParams::Button::MIDDLE));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(4, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param4, 0, buttons, SyntheticPointerActionParams::Button::MIDDLE));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(5, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param5, 1, buttons, SyntheticPointerActionParams::Button::MIDDLE));
  buttons.pop_back();

  ForwardSyntheticPointerAction();
  EXPECT_EQ(6, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param6, 1, buttons, SyntheticPointerActionParams::Button::LEFT));
  buttons.pop_back();
}

TEST_F(SyntheticPointerActionTest, PointerMouseActionWithKey) {
  CreateSyntheticPointerActionTargetAndController<
      MockSyntheticPointerMouseActionTarget>();

  // Send a mouse move.
  SyntheticPointerActionParams param1 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  param1.set_position(gfx::PointF(189, 62));
  params_.PushPointerActionParams(param1);

  // Move the mouse while alt and control keys are pressed.
  SyntheticPointerActionParams param2 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  param2.set_position(gfx::PointF(139, 98));
  param2.set_key_modifiers(6);
  params_.PushPointerActionParams(param2);

  // Send a mouse down with left button while alt and control keys are pressed.
  SyntheticPointerActionParams param3 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param3.set_position(gfx::PointF(139, 98));
  param3.set_button(SyntheticPointerActionParams::Button::LEFT);
  param3.set_key_modifiers(6);
  params_.PushPointerActionParams(param3);
  pointer_action_ = CreatePointerAction(params_);

  ForwardSyntheticPointerAction();
  MockSyntheticPointerMouseActionTarget* pointer_mouse_target =
      static_cast<MockSyntheticPointerMouseActionTarget*>(target_.get());
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  std::vector<SyntheticPointerActionParams::Button> buttons;
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param1, 0, buttons));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param2, 0, buttons));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(3, num_success_);
  EXPECT_EQ(0, num_failure_);
  buttons.push_back(SyntheticPointerActionParams::Button::LEFT);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param3, 1, buttons, SyntheticPointerActionParams::Button::LEFT));
}

TEST_F(SyntheticPointerActionTest, PointerMouseActionWithTime) {
  CreateSyntheticPointerActionTargetAndController<
      MockSyntheticPointerMouseActionTarget>();

  // Send a mouse move.
  base::TimeTicks timestamp = base::TimeTicks::Now();
  SyntheticPointerActionParams param1 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  param1.set_position(gfx::PointF(189, 62));
  param1.set_position(gfx::PointF(189, 62));
  param1.set_timestamp(timestamp + base::Seconds(1));
  params_.PushPointerActionParams(param1);

  // Move the mouse while alt and control keys are pressed.
  SyntheticPointerActionParams param2 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  param2.set_position(gfx::PointF(139, 98));
  param2.set_key_modifiers(6);
  param2.set_timestamp(timestamp + base::Seconds(2));
  params_.PushPointerActionParams(param2);

  // Send a mouse down with left button while alt and control keys are pressed.
  SyntheticPointerActionParams param3 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param3.set_position(gfx::PointF(139, 98));
  param3.set_button(SyntheticPointerActionParams::Button::LEFT);
  param3.set_key_modifiers(6);
  param3.set_timestamp(timestamp + base::Seconds(3));
  params_.PushPointerActionParams(param3);
  pointer_action_ = CreatePointerAction(params_);

  ForwardSyntheticPointerAction();
  MockSyntheticPointerMouseActionTarget* pointer_mouse_target =
      static_cast<MockSyntheticPointerMouseActionTarget*>(target_.get());
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  std::vector<SyntheticPointerActionParams::Button> buttons;
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param1, 0, buttons));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param2, 0, buttons));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(3, num_success_);
  EXPECT_EQ(0, num_failure_);
  buttons.push_back(SyntheticPointerActionParams::Button::LEFT);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param3, 1, buttons, SyntheticPointerActionParams::Button::LEFT));
}

TEST_F(SyntheticPointerActionTest, PointerMouseRelease) {
  CreateSyntheticPointerActionTargetAndController<
      MockSyntheticPointerMouseActionTarget>();

  // Verify a mouse up sends without a prior mouse down
  SyntheticPointerActionParams param = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  params_.PushPointerActionParams(param);
  pointer_action_ = CreatePointerAction(params_);

  ForwardSyntheticPointerAction();
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
}

TEST_F(SyntheticPointerActionTest, PointerMouseActionTypeInvalid) {
  CreateSyntheticPointerActionTargetAndController<
      MockSyntheticPointerMouseActionTarget>();

  // Send a mouse down for one finger.
  SyntheticPointerActionParams param = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param.set_position(gfx::PointF(54, 89));
  params_ = SyntheticPointerActionListParams();
  params_.PushPointerActionParams(param);

  // Cannot send a mouse down again without releasing the mouse button.
  params_.PushPointerActionParams(param);
  pointer_action_ = CreatePointerAction(params_);

  ForwardSyntheticPointerAction();
  MockSyntheticPointerMouseActionTarget* pointer_mouse_target =
      static_cast<MockSyntheticPointerMouseActionTarget*>(target_.get());
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  std::vector<SyntheticPointerActionParams::Button> buttons(
      1, SyntheticPointerActionParams::Button::LEFT);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param, 1, buttons, SyntheticPointerActionParams::Button::LEFT));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(1, num_failure_);
}

TEST_F(SyntheticPointerActionTest, PointerMouseFromDebugger) {
  CreateSyntheticPointerActionTargetAndController<
      MockSyntheticPointerMouseActionTarget>();
  target_->ExpectFromDebugger();
  params_.from_devtools_debugger = true;

  // Send a mouse down.
  SyntheticPointerActionParams param1 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param1.set_position(gfx::PointF(189, 62));
  params_.PushPointerActionParams(param1);

  // Send a mouse drag.
  SyntheticPointerActionParams param2 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  param2.set_position(gfx::PointF(326, 298));
  params_.PushPointerActionParams(param2);

  // Send a mouse up.
  SyntheticPointerActionParams param3 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  params_.PushPointerActionParams(param3);
  pointer_action_ = CreatePointerAction(params_);

  ForwardSyntheticPointerAction();
  MockSyntheticPointerMouseActionTarget* pointer_mouse_target =
      static_cast<MockSyntheticPointerMouseActionTarget*>(target_.get());
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  std::vector<SyntheticPointerActionParams::Button> buttons;
  buttons.push_back(SyntheticPointerActionParams::Button::LEFT);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param1, 1, buttons, SyntheticPointerActionParams::Button::LEFT));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param2, 0, buttons, SyntheticPointerActionParams::Button::LEFT));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(3, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param3, 1, buttons, SyntheticPointerActionParams::Button::LEFT));
}

TEST_F(SyntheticPointerActionTest, PointerPenAction) {
  CreateSyntheticPointerActionTargetAndController<
      MockSyntheticPointerPenActionTarget>();

  // Send a pen move.
  SyntheticPointerActionParams param1 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  param1.set_position(gfx::PointF(189, 62));
  params_.PushPointerActionParams(param1);

  // Send a pen down.
  SyntheticPointerActionParams param2 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param2.set_position(gfx::PointF(189, 62));
  params_.PushPointerActionParams(param2);

  // Send a pen up.
  SyntheticPointerActionParams param3 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  params_.PushPointerActionParams(param3);

  // Send a pen leave.
  SyntheticPointerActionParams param4 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::LEAVE);
  params_.PushPointerActionParams(param4);
  pointer_action_ = CreatePointerAction(params_);

  ForwardSyntheticPointerAction();
  MockSyntheticPointerPenActionTarget* pointer_pen_target =
      static_cast<MockSyntheticPointerPenActionTarget*>(target_.get());
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  std::vector<SyntheticPointerActionParams::Button> buttons;
  EXPECT_TRUE(pointer_pen_target->SyntheticMouseActionDispatchedCorrectly(
      param1, 0, buttons, SyntheticPointerActionParams::Button::NO_BUTTON,
      content::mojom::GestureSourceType::kPenInput));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
  buttons.push_back(SyntheticPointerActionParams::Button::LEFT);
  EXPECT_TRUE(pointer_pen_target->SyntheticMouseActionDispatchedCorrectly(
      param2, 1, buttons, SyntheticPointerActionParams::Button::LEFT,
      content::mojom::GestureSourceType::kPenInput));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(3, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(pointer_pen_target->SyntheticMouseActionDispatchedCorrectly(
      param3, 1, buttons, SyntheticPointerActionParams::Button::LEFT,
      content::mojom::GestureSourceType::kPenInput));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(4, num_success_);
  EXPECT_EQ(0, num_failure_);
  buttons.pop_back();
  EXPECT_TRUE(pointer_pen_target->SyntheticMouseActionDispatchedCorrectly(
      param4, 0, buttons, SyntheticPointerActionParams::Button::NO_BUTTON,
      content::mojom::GestureSourceType::kPenInput));
}

TEST_F(SyntheticPointerActionTest, PointerPenActionFromDebugger) {
  CreateSyntheticPointerActionTargetAndController<
      MockSyntheticPointerPenActionTarget>();
  target_->ExpectFromDebugger();
  params_.from_devtools_debugger = true;

  // Send a pen move.
  SyntheticPointerActionParams param1 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  param1.set_position(gfx::PointF(189, 62));
  params_.PushPointerActionParams(param1);

  // Send a pen down.
  SyntheticPointerActionParams param2 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param2.set_position(gfx::PointF(189, 62));
  params_.PushPointerActionParams(param2);

  // Send a pen up.
  SyntheticPointerActionParams param3 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  params_.PushPointerActionParams(param3);

  // Send a pen leave.
  SyntheticPointerActionParams param4 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::LEAVE);
  params_.PushPointerActionParams(param4);
  pointer_action_ = CreatePointerAction(params_);

  ForwardSyntheticPointerAction();
  MockSyntheticPointerPenActionTarget* pointer_pen_target =
      static_cast<MockSyntheticPointerPenActionTarget*>(target_.get());
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  std::vector<SyntheticPointerActionParams::Button> buttons;
  EXPECT_TRUE(pointer_pen_target->SyntheticMouseActionDispatchedCorrectly(
      param1, 0, buttons, SyntheticPointerActionParams::Button::NO_BUTTON,
      content::mojom::GestureSourceType::kPenInput));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
  buttons.push_back(SyntheticPointerActionParams::Button::LEFT);
  EXPECT_TRUE(pointer_pen_target->SyntheticMouseActionDispatchedCorrectly(
      param2, 1, buttons, SyntheticPointerActionParams::Button::LEFT,
      content::mojom::GestureSourceType::kPenInput));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(3, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(pointer_pen_target->SyntheticMouseActionDispatchedCorrectly(
      param3, 1, buttons, SyntheticPointerActionParams::Button::LEFT,
      content::mojom::GestureSourceType::kPenInput));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(4, num_success_);
  EXPECT_EQ(0, num_failure_);
  buttons.pop_back();
  EXPECT_TRUE(pointer_pen_target->SyntheticMouseActionDispatchedCorrectly(
      param4, 0, buttons, SyntheticPointerActionParams::Button::NO_BUTTON,
      content::mojom::GestureSourceType::kPenInput));
}

TEST_F(SyntheticPointerActionTest, EmptyParams) {
  CreateSyntheticPointerActionTargetAndController<
      MockSyntheticPointerPenActionTarget>();
  pointer_action_ = CreatePointerAction(params_);

  ForwardSyntheticPointerAction();
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
}

TEST_F(SyntheticPointerActionTest, UsesCorrectPointerDriver) {
  CreateSyntheticPointerActionTargetAndController<
      MockSyntheticPointerPenActionTarget>();
  pointer_action_ = CreatePointerAction(params_);

  // Before events are forwarded, no PointerDriver is set yet.
  EXPECT_FALSE(pointer_action_->PointerDriver());

  // If an external driver isn't set, forwarding the first event should
  // initialize an internal pointer driver.
  ForwardSyntheticPointerAction();
  EXPECT_NE(pointer_action_->PointerDriver(), nullptr);
  EXPECT_EQ(pointer_action_->PointerDriver(),
            pointer_action_->internal_synthetic_pointer_driver_.get());
  EXPECT_EQ(pointer_action_->external_synthetic_pointer_driver_.get(), nullptr);

  // Create a new PointerAction and set an external pointer driver on it.
  // Ensure it is used instead of creating an internal one.
  pointer_action_ = CreatePointerAction(params_);
  auto driver = SyntheticPointerDriver::Create(
      target_->GetDefaultSyntheticGestureSourceType());
  pointer_action_->SetSyntheticPointerDriver(driver->AsWeakPtr());
  EXPECT_NE(pointer_action_->PointerDriver(), nullptr);
  EXPECT_EQ(pointer_action_->PointerDriver(), driver.get());
  EXPECT_EQ(pointer_action_->internal_synthetic_pointer_driver_.get(), nullptr);
}

TEST_F(SyntheticPointerActionTest, PointerMouseActionIncreaseClickCount) {
  CreateSyntheticPointerActionTargetAndController<
      MockSyntheticPointerMouseActionTarget>();

  // Send a mouse move.
  SyntheticPointerActionParams param1 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  param1.set_position(gfx::PointF(189, 62));
  params_.PushPointerActionParams(param1);

  // Send a mouse down.
  SyntheticPointerActionParams param2 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param2.set_position(gfx::PointF(189, 62));
  params_.PushPointerActionParams(param2);

  // Send a mouse up.
  SyntheticPointerActionParams param3 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  params_.PushPointerActionParams(param3);

  // Send a second mouse down.
  SyntheticPointerActionParams param4 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param4.set_position(gfx::PointF(189, 62));
  params_.PushPointerActionParams(param4);

  // Send a second mouse up.
  SyntheticPointerActionParams param5 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  params_.PushPointerActionParams(param5);

  // Send a third mouse down.
  SyntheticPointerActionParams param6 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param6.set_position(gfx::PointF(189, 62));
  params_.PushPointerActionParams(param6);

  // Send a third mouse up.
  SyntheticPointerActionParams param7 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  params_.PushPointerActionParams(param7);
  pointer_action_ = CreatePointerAction(params_);

  // Send a fourth mouse down.
  SyntheticPointerActionParams param8 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param8.set_position(gfx::PointF(189, 62));
  params_.PushPointerActionParams(param8);

  // Send a fourth mouse up.
  SyntheticPointerActionParams param9 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  params_.PushPointerActionParams(param9);
  pointer_action_ = CreatePointerAction(params_);

  ForwardSyntheticPointerAction();
  MockSyntheticPointerMouseActionTarget* pointer_mouse_target =
      static_cast<MockSyntheticPointerMouseActionTarget*>(target_.get());
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  std::vector<SyntheticPointerActionParams::Button> buttons;
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param1, 0, buttons));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
  buttons.push_back(SyntheticPointerActionParams::Button::LEFT);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param2, 1, buttons, SyntheticPointerActionParams::Button::LEFT));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(3, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param3, 1, buttons, SyntheticPointerActionParams::Button::LEFT));
  buttons.pop_back();

  ForwardSyntheticPointerAction();
  EXPECT_EQ(4, num_success_);
  EXPECT_EQ(0, num_failure_);
  buttons.push_back(SyntheticPointerActionParams::Button::LEFT);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param4, 2, buttons, SyntheticPointerActionParams::Button::LEFT));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(5, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param5, 2, buttons, SyntheticPointerActionParams::Button::LEFT));
  buttons.pop_back();

  ForwardSyntheticPointerAction();
  EXPECT_EQ(6, num_success_);
  EXPECT_EQ(0, num_failure_);
  buttons.push_back(SyntheticPointerActionParams::Button::LEFT);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param6, 3, buttons, SyntheticPointerActionParams::Button::LEFT));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(7, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param7, 3, buttons, SyntheticPointerActionParams::Button::LEFT));
  buttons.pop_back();

  int click_count = 4;
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_WIN)
  click_count = 1;
#endif
  ForwardSyntheticPointerAction();
  EXPECT_EQ(8, num_success_);
  EXPECT_EQ(0, num_failure_);
  buttons.push_back(SyntheticPointerActionParams::Button::LEFT);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param8, click_count, buttons,
      SyntheticPointerActionParams::Button::LEFT));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(9, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param9, click_count, buttons,
      SyntheticPointerActionParams::Button::LEFT));
}

TEST_F(SyntheticPointerActionTest, PointerMouseActionResetCountOnOtherButton) {
  CreateSyntheticPointerActionTargetAndController<
      MockSyntheticPointerMouseActionTarget>();

  // Send a mouse move.
  SyntheticPointerActionParams param1 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  param1.set_position(gfx::PointF(189, 62));
  params_.PushPointerActionParams(param1);

  // Send a mouse down.
  SyntheticPointerActionParams param2 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param2.set_position(gfx::PointF(189, 62));
  params_.PushPointerActionParams(param2);

  // Send a mouse up.
  SyntheticPointerActionParams param3 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  params_.PushPointerActionParams(param3);

  // Send a second mouse down with another button.
  SyntheticPointerActionParams param4 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param4.set_position(gfx::PointF(189, 62));
  param4.set_button(SyntheticPointerActionParams::Button::MIDDLE);
  params_.PushPointerActionParams(param4);

  // Send a second mouse up.
  SyntheticPointerActionParams param5 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  param5.set_button(SyntheticPointerActionParams::Button::MIDDLE);
  params_.PushPointerActionParams(param5);
  pointer_action_ = CreatePointerAction(params_);

  ForwardSyntheticPointerAction();
  MockSyntheticPointerMouseActionTarget* pointer_mouse_target =
      static_cast<MockSyntheticPointerMouseActionTarget*>(target_.get());
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  std::vector<SyntheticPointerActionParams::Button> buttons;
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param1, 0, buttons));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
  buttons.push_back(SyntheticPointerActionParams::Button::LEFT);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param2, 1, buttons, SyntheticPointerActionParams::Button::LEFT));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(3, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param3, 1, buttons, SyntheticPointerActionParams::Button::LEFT));
  buttons.pop_back();

  ForwardSyntheticPointerAction();
  EXPECT_EQ(4, num_success_);
  EXPECT_EQ(0, num_failure_);
  buttons.push_back(SyntheticPointerActionParams::Button::MIDDLE);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param4, 1, buttons, SyntheticPointerActionParams::Button::MIDDLE));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(5, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param5, 1, buttons, SyntheticPointerActionParams::Button::MIDDLE));
}

TEST_F(SyntheticPointerActionTest, PointerMouseActionResetCountAfterMove) {
  CreateSyntheticPointerActionTargetAndController<
      MockSyntheticPointerMouseActionTarget>();

  // Send a mouse move.
  SyntheticPointerActionParams param1 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  param1.set_position(gfx::PointF(189, 62));
  params_.PushPointerActionParams(param1);

  // Send a mouse down.
  SyntheticPointerActionParams param2 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param2.set_position(gfx::PointF(189, 62));
  params_.PushPointerActionParams(param2);

  // Send a mouse up.
  SyntheticPointerActionParams param3 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  params_.PushPointerActionParams(param3);

  // Send a second mouse down close to the last one.
  SyntheticPointerActionParams param4 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param4.set_position(gfx::PointF(190, 60));
  params_.PushPointerActionParams(param4);

  // Send a second mouse up.
  SyntheticPointerActionParams param5 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  params_.PushPointerActionParams(param5);
  pointer_action_ = CreatePointerAction(params_);

  // Send a third mouse down far enough from the last one.
  SyntheticPointerActionParams param6 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param6.set_position(gfx::PointF(290, 60));
  params_.PushPointerActionParams(param6);

  // Send a third mouse up.
  SyntheticPointerActionParams param7 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  params_.PushPointerActionParams(param7);
  pointer_action_ = CreatePointerAction(params_);

  ForwardSyntheticPointerAction();
  MockSyntheticPointerMouseActionTarget* pointer_mouse_target =
      static_cast<MockSyntheticPointerMouseActionTarget*>(target_.get());
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  std::vector<SyntheticPointerActionParams::Button> buttons;
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param1, 0, buttons));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
  buttons.push_back(SyntheticPointerActionParams::Button::LEFT);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param2, 1, buttons, SyntheticPointerActionParams::Button::LEFT));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(3, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param3, 1, buttons, SyntheticPointerActionParams::Button::LEFT));
  buttons.pop_back();

  ForwardSyntheticPointerAction();
  EXPECT_EQ(4, num_success_);
  EXPECT_EQ(0, num_failure_);
  buttons.push_back(SyntheticPointerActionParams::Button::LEFT);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param4, 2, buttons, SyntheticPointerActionParams::Button::LEFT));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(5, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param5, 2, buttons, SyntheticPointerActionParams::Button::LEFT));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(6, num_success_);
  EXPECT_EQ(0, num_failure_);
  buttons.push_back(SyntheticPointerActionParams::Button::LEFT);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param6, 1, buttons, SyntheticPointerActionParams::Button::LEFT));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(7, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(
      param7, 1, buttons, SyntheticPointerActionParams::Button::LEFT));
}

}  // namespace content
