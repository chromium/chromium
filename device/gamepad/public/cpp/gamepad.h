// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPAD_H_
#define DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPAD_H_

#include <stddef.h>
#include <cstdint>

#include "base/component_export.h"
#include "base/strings/string16.h"

namespace device {

#pragma pack(push, 4)

class GamepadButton {
 public:
  // Matches XInput's trigger deadzone.
  static constexpr float kDefaultButtonPressedThreshold = 30.f / 255.f;

  GamepadButton() : pressed(false), touched(false), value(0.) {}
  GamepadButton(bool pressed, bool touched, double value)
      : pressed(pressed), touched(touched), value(value) {}
  bool pressed;
  bool touched;
  double value;
};

enum class GamepadHapticActuatorType { kVibration = 0, kDualRumble = 1 };

enum class GamepadHapticEffectType { kDualRumble = 0 };

enum class GamepadHapticsResult {
  kError = 0,
  kComplete = 1,
  kPreempted = 2,
  kInvalidParameter = 3,
  kNotSupported = 4
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

  Gamepad();
  Gamepad(const Gamepad& other);

  // If src is too long, then the contents of id will be truncated to
  // kIdLengthCap-1. id will be null-terminated and any extra space in the
  // buffer will be zeroed out.
  void SetID(const base::string16& src);

  // Is there a gamepad connected at this index?
  bool connected;

  // Device identifier (based on manufacturer, model, etc.).
  base::char16 id[kIdLengthCap];

  // Time value representing the last time the data for this gamepad was
  // updated. Measured as TimeTicks::Now().since_origin().InMicroseconds().
  int64_t timestamp;

  // Number of valid entries in the axes array.
  unsigned axes_length;

  // Normalized values representing axes, in the range [-1..1].
  double axes[kAxesLengthCap];

  // Number of valid entries in the buttons array.
  unsigned buttons_length;

  // Button states
  GamepadButton buttons[kButtonsLengthCap];

  GamepadHapticActuator vibration_actuator;

  // Mapping type
  GamepadMapping mapping;

  GamepadPose pose;

  GamepadHand hand;

  unsigned display_id;

  bool is_xr = false;
};

#pragma pack(pop)

}  // namespace device

#endif  // DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPAD_H_
