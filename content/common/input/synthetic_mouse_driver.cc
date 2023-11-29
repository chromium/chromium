// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/synthetic_mouse_driver.h"

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/input/synthetic_gesture_target.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"

namespace content {

SyntheticMouseDriverBase::SyntheticMouseDriverBase() : last_modifiers_(0) {
  mouse_event_.pointer_type = blink::WebPointerProperties::PointerType::kMouse;
}

SyntheticMouseDriverBase::~SyntheticMouseDriverBase() = default;

void SyntheticMouseDriverBase::DispatchEvent(SyntheticGestureTarget* target,
                                             const base::TimeTicks& timestamp) {
  mouse_event_.SetTimeStamp(timestamp);
  if (mouse_event_.GetType() != blink::WebInputEvent::Type::kUndefined) {
    base::WeakPtr<SyntheticPointerDriver> weak_this = AsWeakPtr();
    target->DispatchInputEventToPlatform(mouse_event_);
    // Dispatching a mouse event can cause the containing WebContents to be
    // synchronously deleted.
    if (!weak_this) {
      return;
    }
    mouse_event_.SetType(blink::WebInputEvent::Type::kUndefined);
  }
}

void SyntheticMouseDriverBase::Press(
    float x,
    float y,
    int index,
    SyntheticPointerActionParams::Button button,
    int key_modifiers,
    float width,
    float height,
    float rotation_angle,
    float force,
    float tangential_pressure,
    int tilt_x,
    int tilt_y,
    const base::TimeTicks& timestamp) {
  DCHECK_EQ(index, 0);
  blink::WebMouseEvent::Button pressed_button =
      SyntheticPointerActionParams::GetWebMouseEventButton(button);
  click_count_ = ComputeClickCount(timestamp, pressed_button, x, y);
  int modifiers =
      SyntheticPointerActionParams::GetWebMouseEventModifier(button);
  if (from_devtools_debugger_)
    key_modifiers |= blink::WebInputEvent::kFromDebugger;
  mouse_event_ = blink::SyntheticWebMouseEventBuilder::Build(
      blink::WebInputEvent::Type::kMouseDown, x, y,
      modifiers | key_modifiers | last_modifiers_, mouse_event_.pointer_type);
  mouse_event_.button = pressed_button;
  last_modifiers_ = modifiers | last_modifiers_;
  mouse_event_.click_count = click_count_;
  mouse_event_.force = force;
  mouse_event_.tangential_pressure = tangential_pressure;
  mouse_event_.twist = rotation_angle;
  mouse_event_.tilt_x = tilt_x;
  mouse_event_.tilt_y = tilt_y;
  last_mouse_click_time_ = timestamp;
  last_x_ = x;
  last_y_ = y;
}

void SyntheticMouseDriverBase::Move(
    float x,
    float y,
    int index,
    int key_modifiers,
    float width,
    float height,
    float rotation_angle,
    float force,
    float tangential_pressure,
    int tilt_x,
    int tilt_y,
    SyntheticPointerActionParams::Button button) {
  DCHECK_EQ(index, 0);
  int button_modifiers =
      SyntheticPointerActionParams::GetWebMouseEventModifier(button);
  if (from_devtools_debugger_)
    key_modifiers |= blink::WebInputEvent::kFromDebugger;
  mouse_event_ = blink::SyntheticWebMouseEventBuilder::Build(
      blink::WebInputEvent::Type::kMouseMove, x, y,
      button_modifiers | key_modifiers | last_modifiers_,
      mouse_event_.pointer_type);
  if (button != SyntheticPointerActionParams::Button::NO_BUTTON) {
    // If the caller specified a pressed button for this move event, use that.
    mouse_event_.button =
        SyntheticPointerActionParams::GetWebMouseEventButton(button);
  } else {
    // Otherwise, infer pressed button from a previous press event (if any) in
    // the same pointer action sequence.
    mouse_event_.button =
        SyntheticPointerActionParams::GetWebMouseEventButtonFromModifier(
            last_modifiers_);
  }
  mouse_event_.click_count = 0;
  mouse_event_.force =
      mouse_event_.button == blink::WebMouseEvent::Button::kNoButton ? 0
                                                                     : force;
  mouse_event_.tangential_pressure = tangential_pressure;
  mouse_event_.twist = rotation_angle;
  mouse_event_.tilt_x = tilt_x;
  mouse_event_.tilt_y = tilt_y;
}

void SyntheticMouseDriverBase::Release(
    int index,
    SyntheticPointerActionParams::Button button,
    int key_modifiers) {
  DCHECK_EQ(index, 0);
  if (from_devtools_debugger_)
    key_modifiers |= blink::WebInputEvent::kFromDebugger;
  mouse_event_ = blink::SyntheticWebMouseEventBuilder::Build(
      blink::WebInputEvent::Type::kMouseUp, mouse_event_.PositionInWidget().x(),
      mouse_event_.PositionInWidget().y(), key_modifiers | last_modifiers_,
      mouse_event_.pointer_type);
  mouse_event_.button =
      SyntheticPointerActionParams::GetWebMouseEventButton(button);

  // Set click count to 1 to allow pointer release without pointer down. This
  // prevents MouseEvent::SetClickCount from throwing DCHECK error
  click_count_ == 0 ? mouse_event_.click_count = 1
                    : mouse_event_.click_count = click_count_;
  last_modifiers_ =
      last_modifiers_ &
      (~SyntheticPointerActionParams::GetWebMouseEventModifier(button));
}

void SyntheticMouseDriverBase::Cancel(
    int index,
    SyntheticPointerActionParams::Button button,
    int key_modifiers) {
  NOTIMPLEMENTED();
}

void SyntheticMouseDriverBase::Leave(int index) {
  NOTIMPLEMENTED();
}

bool SyntheticMouseDriverBase::UserInputCheck(
    const SyntheticPointerActionParams& params) const {
  if (params.pointer_action_type() ==
      SyntheticPointerActionParams::PointerActionType::NOT_INITIALIZED) {
    return false;
  }

  if (params.pointer_action_type() ==
      SyntheticPointerActionParams::PointerActionType::PRESS) {
    int modifiers =
        SyntheticPointerActionParams::GetWebMouseEventModifier(params.button());
    if (last_modifiers_ & modifiers)
      return false;
  }

  if (params.pointer_action_type() ==
      SyntheticPointerActionParams::PointerActionType::RELEASE) {
    int modifiers =
        SyntheticPointerActionParams::GetWebMouseEventModifier(params.button());
    if (!modifiers)
      return false;
  }

  return true;
}

int SyntheticMouseDriverBase::ComputeClickCount(
    const base::TimeTicks& timestamp,
    blink::WebMouseEvent::Button pressed_button,
    float x,
    float y) {
  const int kDoubleClickTimeMS = 500;
  const int kDoubleClickRange = 4;

  if (click_count_ == 0)
    return 1;

  base::TimeDelta time_difference = timestamp - last_mouse_click_time_;
  if (time_difference.InMilliseconds() > kDoubleClickTimeMS)
    return 1;

  if (std::abs(x - last_x_) > kDoubleClickRange / 2)
    return 1;

  if (std::abs(y - last_y_) > kDoubleClickRange / 2)
    return 1;

  if (mouse_event_.button != pressed_button)
    return 1;

  ++click_count_;
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_WIN)
  // On Mac and Windows, we keep increasing the click count, but on the other
  // platforms, we reset the count to 1 when it is greater than 3.
  if (click_count_ > 3)
    click_count_ = 1;
#endif
  return click_count_;
}

SyntheticMouseDriver::SyntheticMouseDriver() = default;

SyntheticMouseDriver::~SyntheticMouseDriver() = default;

base::WeakPtr<SyntheticPointerDriver> SyntheticMouseDriver::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace content
