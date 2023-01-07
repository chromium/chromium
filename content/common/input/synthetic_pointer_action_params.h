// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_SYNTHETIC_POINTER_ACTION_PARAMS_H_
#define CONTENT_COMMON_INPUT_SYNTHETIC_POINTER_ACTION_PARAMS_H_

#include "base/check_op.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "ui/gfx/geometry/point_f.h"

namespace content {

namespace mojom {
class SyntheticPointerActionParamsDataView;
}  // namespace mojom

// It contains all the parameters to create the synthetic events of touch,
// mouse and pen inputs in SyntheticPointerAction::ForwardInputEvents function.
struct CONTENT_EXPORT SyntheticPointerActionParams {
 public:
  // All the pointer actions that will be dispatched together will be grouped
  // in an array.
  enum class PointerActionType {
    NOT_INITIALIZED,
    PRESS,
    MOVE,
    RELEASE,
    CANCEL,
    LEAVE,
    IDLE,
    POINTER_ACTION_TYPE_MAX = IDLE
  };

  enum class Button {
    NO_BUTTON,
    LEFT,
    MIDDLE,
    RIGHT,
    BACK,
    FORWARD,
    BUTTON_MAX = FORWARD
  };

  SyntheticPointerActionParams();
  explicit SyntheticPointerActionParams(PointerActionType action_type);
  SyntheticPointerActionParams(const SyntheticPointerActionParams& other);
  ~SyntheticPointerActionParams();

  void set_pointer_action_type(PointerActionType pointer_action_type) {
    pointer_action_type_ = pointer_action_type;
  }

  void set_pointer_id(uint32_t pointer_id) { pointer_id_ = pointer_id; }

  void set_position(const gfx::PointF& position) {
    DCHECK(pointer_action_type_ == PointerActionType::PRESS ||
           pointer_action_type_ == PointerActionType::MOVE);
    position_ = position;
  }

  void set_button(Button button) {
    DCHECK(pointer_action_type_ == PointerActionType::PRESS ||
           pointer_action_type_ == PointerActionType::MOVE ||
           pointer_action_type_ == PointerActionType::RELEASE ||
           pointer_action_type_ == PointerActionType::CANCEL);
    button_ = button;
  }

  void set_key_modifiers(int key_modifiers) {
    DCHECK_NE(PointerActionType::IDLE, pointer_action_type_);
    key_modifiers_ = key_modifiers;
  }

  void set_width(float width) {
    DCHECK(pointer_action_type_ == PointerActionType::PRESS ||
           pointer_action_type_ == PointerActionType::MOVE);
    width_ = width;
  }

  void set_height(float height) {
    DCHECK(pointer_action_type_ == PointerActionType::PRESS ||
           pointer_action_type_ == PointerActionType::MOVE);
    height_ = height;
  }

  void set_rotation_angle(float rotation_angle) {
    DCHECK(pointer_action_type_ == PointerActionType::PRESS ||
           pointer_action_type_ == PointerActionType::MOVE);
    rotation_angle_ = rotation_angle;
  }

  void set_force(float force) {
    DCHECK(pointer_action_type_ == PointerActionType::PRESS ||
           pointer_action_type_ == PointerActionType::MOVE);
    force_ = force;
  }

  void set_tangential_pressure(float tangential_pressure) {
    DCHECK(pointer_action_type_ == PointerActionType::PRESS ||
           pointer_action_type_ == PointerActionType::MOVE);
    tangential_pressure_ = tangential_pressure;
  }

  void set_tilt_x(int tilt_x) {
    DCHECK(pointer_action_type_ == PointerActionType::PRESS ||
           pointer_action_type_ == PointerActionType::MOVE);
    tilt_x_ = tilt_x;
  }

  void set_tilt_y(int tilt_y) {
    DCHECK(pointer_action_type_ == PointerActionType::PRESS ||
           pointer_action_type_ == PointerActionType::MOVE);
    tilt_y_ = tilt_y;
  }

  void set_timestamp(base::TimeTicks timestamp) { timestamp_ = timestamp; }

  void set_duration(base::TimeDelta duration) {
    DCHECK_EQ(PointerActionType::IDLE, pointer_action_type_);
    duration_ = duration;
  }

  PointerActionType pointer_action_type() const { return pointer_action_type_; }

  uint32_t pointer_id() const { return pointer_id_; }

  gfx::PointF position() const {
    DCHECK(pointer_action_type_ == PointerActionType::PRESS ||
           pointer_action_type_ == PointerActionType::MOVE);
    return position_;
  }

  Button button() const {
    DCHECK(pointer_action_type_ == PointerActionType::PRESS ||
           pointer_action_type_ == PointerActionType::MOVE ||
           pointer_action_type_ == PointerActionType::RELEASE ||
           pointer_action_type_ == PointerActionType::CANCEL);
    return button_;
  }

  int key_modifiers() const {
    DCHECK_NE(PointerActionType::IDLE, pointer_action_type_);
    return key_modifiers_;
  }

  float width() const {
    DCHECK(pointer_action_type_ == PointerActionType::PRESS ||
           pointer_action_type_ == PointerActionType::MOVE);
    return width_;
  }

  float height() const {
    DCHECK(pointer_action_type_ == PointerActionType::PRESS ||
           pointer_action_type_ == PointerActionType::MOVE);
    return height_;
  }

  float rotation_angle() const {
    DCHECK(pointer_action_type_ == PointerActionType::PRESS ||
           pointer_action_type_ == PointerActionType::MOVE);
    return rotation_angle_;
  }

  float force() const {
    DCHECK(pointer_action_type_ == PointerActionType::PRESS ||
           pointer_action_type_ == PointerActionType::MOVE);
    return force_;
  }

  float tangential_pressure() const {
    DCHECK(pointer_action_type_ == PointerActionType::PRESS ||
           pointer_action_type_ == PointerActionType::MOVE);
    return tangential_pressure_;
  }

  int tilt_x() const {
    DCHECK(pointer_action_type_ == PointerActionType::PRESS ||
           pointer_action_type_ == PointerActionType::MOVE);
    return tilt_x_;
  }

  int tilt_y() const {
    DCHECK(pointer_action_type_ == PointerActionType::PRESS ||
           pointer_action_type_ == PointerActionType::MOVE);
    return tilt_y_;
  }

  base::TimeTicks timestamp() const { return timestamp_; }

  base::TimeDelta duration() const {
    DCHECK_EQ(PointerActionType::IDLE, pointer_action_type_);
    return duration_;
  }

  static unsigned GetWebMouseEventModifier(
      SyntheticPointerActionParams::Button button);
  static blink::WebMouseEvent::Button GetWebMouseEventButton(
      SyntheticPointerActionParams::Button button);
  static blink::WebMouseEvent::Button GetWebMouseEventButtonFromModifier(
      unsigned modifiers);

 private:
  friend struct mojo::StructTraits<
      content::mojom::SyntheticPointerActionParamsDataView,
      content::SyntheticPointerActionParams>;

  PointerActionType pointer_action_type_ = PointerActionType::NOT_INITIALIZED;
  // The position of the pointer, where it presses or moves to.
  gfx::PointF position_;
  // The id of the pointer given by the users.
  uint32_t pointer_id_ = 0;
  Button button_ = Button::LEFT;
  // “Alt“, ”Control“, ”Meta“, ”Shift“, ”CapsLock“, ”NumLock“, ”AltGraph”
  // buttons are supported right now. It stores a matching modifiers defined
  // in WebInputEvent class.
  int key_modifiers_ = 0;
  float width_ = 40.f;
  float height_ = 40.f;
  float rotation_angle_ = 0.f;
  float force_ = 1.f;
  float tangential_pressure_ = 0.f;
  int tilt_x_ = 0;
  int tilt_y_ = 0;
  base::TimeTicks timestamp_;
  // The duration of the pause action is in milliseconds.
  base::TimeDelta duration_;
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_SYNTHETIC_POINTER_ACTION_PARAMS_H_
