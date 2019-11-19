// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/public/cpp/gamepad_mojom_traits.h"

#include "base/test/task_environment.h"
#include "device/gamepad/public/cpp/gamepad.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

enum GamepadTestDataType {
  GamepadCommon = 0,
  GamepadPose_HasOrientation = 1,
  GamepadPose_HasPosition = 2,
  GamepadPose_Null = 3,
};

Gamepad GetWebGamepadInstance(GamepadTestDataType type) {
  GamepadButton wgb(true, false, 1.0f);

  GamepadVector wgv;
  memset(&wgv, 0, sizeof(GamepadVector));
  wgv.not_null = true;
  wgv.x = wgv.y = wgv.z = 1.0f;

  GamepadQuaternion wgq;
  memset(&wgq, 0, sizeof(GamepadQuaternion));
  wgq.not_null = true;
  wgq.x = wgq.y = wgq.z = wgq.w = 2.0f;

  GamepadPose wgp;
  memset(&wgp, 0, sizeof(GamepadPose));
  if (type == GamepadPose_Null) {
    wgp.not_null = false;
  } else if (type == GamepadCommon) {
    wgp.not_null = wgp.has_orientation = wgp.has_position = true;
    wgp.orientation = wgq;
    wgp.position = wgv;
    wgp.angular_acceleration = wgv;
  } else if (type == GamepadPose_HasOrientation) {
    wgp.not_null = wgp.has_orientation = true;
    wgp.has_position = false;
    wgp.orientation = wgq;
    wgp.angular_acceleration = wgv;
  } else if (type == GamepadPose_HasPosition) {
    wgp.not_null = wgp.has_position = true;
    wgp.has_orientation = false;
    wgp.position = wgv;
    wgp.angular_acceleration = wgv;
  }

  constexpr base::char16 kTestIdString[] = {L'M', L'o', L'c', L'k', L'S',
                                            L't', L'i', L'c', L'k', L' ',
                                            L'3', L'0', L'0', L'0', L'\0'};
  constexpr size_t kTestIdStringLength = base::size(kTestIdString);

  Gamepad send;
  memset(&send, 0, sizeof(Gamepad));

  send.connected = true;
  for (size_t i = 0; i < kTestIdStringLength; i++) {
    send.id[i] = kTestIdString[i];
  }
  send.mapping = GamepadMapping::kNone;
  send.timestamp = base::TimeTicks::Now().since_origin().InMicroseconds();
  send.axes_length = 0U;
  for (size_t i = 0; i < Gamepad::kAxesLengthCap; i++) {
    send.axes_length++;
    send.axes[i] = 1.0;
  }
  send.buttons_length = 0U;
  for (size_t i = 0; i < Gamepad::kButtonsLengthCap; i++) {
    send.buttons_length++;
    send.buttons[i] = wgb;
  }
  send.pose = wgp;
  send.hand = GamepadHand::kRight;
  send.display_id = static_cast<unsigned short>(16);

  return send;
}

bool isWebGamepadButtonEqual(const GamepadButton& lhs,
                             const GamepadButton& rhs) {
  return (lhs.pressed == rhs.pressed && lhs.touched == rhs.touched &&
          lhs.value == rhs.value);
}

bool isWebGamepadVectorEqual(const GamepadVector& lhs,
                             const GamepadVector& rhs) {
  return ((lhs.not_null == false && rhs.not_null == false) ||
          (lhs.not_null == rhs.not_null && lhs.x == rhs.x && lhs.y == rhs.y &&
           lhs.z == rhs.z));
}

bool isWebGamepadQuaternionEqual(const GamepadQuaternion& lhs,
                                 const GamepadQuaternion& rhs) {
  return ((lhs.not_null == false && rhs.not_null == false) ||
          (lhs.not_null == rhs.not_null && lhs.x == rhs.x && lhs.y == rhs.y &&
           lhs.z == rhs.z && lhs.w == rhs.w));
}

bool isWebGamepadPoseEqual(const GamepadPose& lhs, const GamepadPose& rhs) {
  if (lhs.not_null == false && rhs.not_null == false) {
    return true;
  }
  if (lhs.not_null != rhs.not_null ||
      lhs.has_orientation != rhs.has_orientation ||
      lhs.has_position != rhs.has_position ||
      !isWebGamepadVectorEqual(lhs.angular_velocity, rhs.angular_velocity) ||
      !isWebGamepadVectorEqual(lhs.linear_velocity, rhs.linear_velocity) ||
      !isWebGamepadVectorEqual(lhs.angular_acceleration,
                               rhs.angular_acceleration) ||
      !isWebGamepadVectorEqual(lhs.linear_acceleration,
                               rhs.linear_acceleration)) {
    return false;
  }
  if (lhs.has_orientation &&
      !isWebGamepadQuaternionEqual(lhs.orientation, rhs.orientation)) {
    return false;
  }
  if (lhs.has_position &&
      !isWebGamepadVectorEqual(lhs.position, rhs.position)) {
    return false;
  }
  return true;
}

bool isWebGamepadEqual(const Gamepad& send, const Gamepad& echo) {
  if (send.connected != echo.connected || send.timestamp != echo.timestamp ||
      send.axes_length != echo.axes_length ||
      send.buttons_length != echo.buttons_length ||
      !isWebGamepadPoseEqual(send.pose, echo.pose) || send.hand != echo.hand ||
      send.display_id != echo.display_id || send.mapping != echo.mapping) {
    return false;
  }
  for (size_t i = 0; i < Gamepad::kIdLengthCap; i++) {
    if (send.id[i] != echo.id[i]) {
      return false;
    }
  }
  for (size_t i = 0; i < Gamepad::kAxesLengthCap; i++) {
    if (send.axes[i] != echo.axes[i]) {
      return false;
    }
  }
  for (size_t i = 0; i < Gamepad::kButtonsLengthCap; i++) {
    if (!isWebGamepadButtonEqual(send.buttons[i], echo.buttons[i])) {
      return false;
    }
  }
  return true;
}
}  // namespace

class GamepadStructTraitsTest : public testing::Test {
 protected:
  GamepadStructTraitsTest() {}

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(GamepadStructTraitsTest);
};

TEST_F(GamepadStructTraitsTest, GamepadCommon) {
  Gamepad gamepad_in = GetWebGamepadInstance(GamepadCommon);
  Gamepad gamepad_out;

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Gamepad>(
      &gamepad_in, &gamepad_out));
  EXPECT_EQ(true, isWebGamepadEqual(gamepad_in, gamepad_out));
}

TEST_F(GamepadStructTraitsTest, GamepadPose_HasOrientation) {
  Gamepad gamepad_in = GetWebGamepadInstance(GamepadPose_HasOrientation);
  Gamepad gamepad_out;

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Gamepad>(
      &gamepad_in, &gamepad_out));
  EXPECT_EQ(true, isWebGamepadEqual(gamepad_in, gamepad_out));
}

TEST_F(GamepadStructTraitsTest, GamepadPose_HasPosition) {
  Gamepad gamepad_in = GetWebGamepadInstance(GamepadPose_HasPosition);
  Gamepad gamepad_out;

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Gamepad>(
      &gamepad_in, &gamepad_out));
  EXPECT_EQ(true, isWebGamepadEqual(gamepad_in, gamepad_out));
}

TEST_F(GamepadStructTraitsTest, GamepadPose_Null) {
  Gamepad gamepad_in = GetWebGamepadInstance(GamepadPose_Null);
  Gamepad gamepad_out;

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Gamepad>(
      &gamepad_in, &gamepad_out));
  EXPECT_EQ(true, isWebGamepadEqual(gamepad_in, gamepad_out));
}
}  // namespace device
