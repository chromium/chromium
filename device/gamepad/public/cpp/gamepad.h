// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPAD_H_
#define DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPAD_H_

#include <stddef.h>
#include <cstdint>
#include <string>

#include <limits>

#include "base/component_export.h"

namespace device {

class GamepadButton {
 public:
  // Matches XInput's trigger deadzone.
  static constexpr float kDefaultButtonPressedThreshold = 30.f / 255.f;

  GamepadButton() = default;
  GamepadButton(bool pressed, bool touched, double value)
      : used(true), pressed(pressed), touched(touched), value(value) {}
  bool operator==(const GamepadButton& other) const {
    return this->used == other.used && this->pressed == other.pressed &&
           this->touched == other.touched && this->value == other.value;
  }
  // Whether the button is actually reported by the gamepad at all.
  bool used{false};
  bool pressed{false};
  bool touched{false};
  double value{0.0};
};

enum class GamepadHapticActuatorType {
  kVibration = 0,
  kDualRumble = 1,
  kTriggerRumble = 2
};

enum class GamepadHapticEffectType { kDualRumble = 0 };

enum class GamepadHapticsResult {
  kError = 0,
  kComplete = 1,
  kPreempted = 2,
  kInvalidParameter = 3,
  kNotSupported = 4
};

struct GamepadTouch {
  uint32_t touch_id = 0;
  uint8_t surface_id = 0;
  bool has_surface_dimensions = false;
  float x = 0.0f;
  float y = 0.0f;
  uint32_t surface_width;
  uint32_t surface_height;
};

class GamepadHapticActuator {
 public:
  static constexpr double kMaxEffectDurationMillis = 5000.0;

  GamepadHapticActuator() : not_null(false) {}

  bool not_null;
  GamepadHapticActuatorType type;
};

class GamepadEffectParameters {
 public:
  double duration;
  double start_delay;
  double strong_magnitude;
  double weak_magnitude;
};

class GamepadVector {
 public:
  GamepadVector() : not_null(false) {}

  bool not_null;
  float x, y, z;
};

class GamepadQuaternion {
 public:
  GamepadQuaternion() : not_null(false) {}

  bool not_null;
  float x, y, z, w;
};

class GamepadPose {
 public:
  GamepadPose() : not_null(false) {}

  bool not_null;

  bool has_orientation;
  bool has_position;

  GamepadQuaternion orientation;
  GamepadVector position;
  GamepadVector angular_velocity;
  GamepadVector linear_velocity;
  GamepadVector angular_acceleration;
  GamepadVector linear_acceleration;
};

enum class GamepadMapping { kNone = 0, kStandard = 1, kXrStandard = 2 };

enum class GamepadHand { kNone = 0, kLeft = 1, kRight = 2 };

// This structure is intentionally POD and fixed size so that it can be shared
// memory between hardware polling threads and the rest of the browser. See
// also gamepads.h.
class COMPONENT_EXPORT(GAMEPAD_PUBLIC) Gamepad {
 public:
  static constexpr size_t kIdLengthCap = 128;
  static constexpr size_t kAxesLengthCap = 16;
  static constexpr size_t kButtonsLengthCap = 32;
  static constexpr size_t kTouchEventsLengthCap = 8;

  Gamepad();
  Gamepad(const Gamepad& other);
  Gamepad& operator=(const Gamepad& other);

  // If src is too long, then the contents of id will be truncated to
  // kIdLengthCap-1. id will be null-terminated and any extra space in the
  // buffer will be zeroed out.
  void SetID(const std::u16string& src);

  // Is there a gamepad connected at this index?
  bool connected;

  // Device identifier (based on manufacturer, model, etc.).
  char16_t id[kIdLengthCap];

  // Time value representing the last time the data for this gamepad was
  // updated. Measured as TimeTicks::Now().since_origin().InMicroseconds().
  int64_t timestamp;

  // Number of valid entries in the axes array.
  unsigned axes_length;

  // Bitfield indicating which entries of the axes array are actually used. If
  // the axes index is actually used for this gamepad then the corresponding bit
  // will be 1.
  uint32_t axes_used;
  static_assert(Gamepad::kAxesLengthCap <=
                    std::numeric_limits<uint32_t>::digits,
                "axes_used is not large enough");

  // Normalized values representing axes, in the range [-1..1].
  double axes[kAxesLengthCap];

  // Number of valid entries in the buttons array.
  unsigned buttons_length;

  // Button states
  GamepadButton buttons[kButtonsLengthCap];

  // Number of valid entries in the touch_events array.
  uint32_t touch_events_length;

  // Touch events states
  bool supports_touch_events_ = false;

  GamepadTouch touch_events[kTouchEventsLengthCap];

  GamepadHapticActuator vibration_actuator;

  // Mapping type
  GamepadMapping mapping;

  GamepadPose pose;

  GamepadHand hand;

  unsigned display_id;

  bool is_xr = false;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPAD_H_
