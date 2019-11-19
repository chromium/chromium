// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/gamepad/gamepad_comparisons.h"

#include "base/macros.h"
#include "device/gamepad/public/cpp/gamepad.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad_list.h"

namespace blink {

class GamepadComparisonsTest : public testing::Test {
 public:
  GamepadComparisonsTest() = default;

 protected:
  void InitGamepadQuaternion(device::GamepadQuaternion& q) {
    q.not_null = true;
    q.x = 0.f;
    q.y = 0.f;
    q.z = 0.f;
    q.w = 0.f;
  }

  void InitGamepadVector(device::GamepadVector& v) {
    v.not_null = true;
    v.x = 0.f;
    v.y = 0.f;
    v.z = 0.f;
  }

  Gamepad* CreateGamepad() {
    base::TimeTicks dummy_time_origin =
        base::TimeTicks() + base::TimeDelta::FromMicroseconds(1000);
    base::TimeTicks dummy_time_floor =
        base::TimeTicks() + base::TimeDelta::FromMicroseconds(2000);
    return MakeGarbageCollected<Gamepad>(nullptr, 0, dummy_time_origin,
                                         dummy_time_floor);
  }

  GamepadList* CreateEmptyGamepadList() {
    return MakeGarbageCollected<GamepadList>();
  }

  GamepadList* CreateGamepadListWithNeutralGamepad() {
    double axes[1] = {0.0};
    device::GamepadButton buttons[1] = {{false, false, 0.0}};
    auto* list = MakeGarbageCollected<GamepadList>();
    auto* gamepad = CreateGamepad();
    gamepad->SetId("gamepad");
    gamepad->SetAxes(1, axes);
    gamepad->SetButtons(1, buttons);
    gamepad->SetConnected(true);
    list->Set(0, gamepad);
    return list;
  }

  GamepadList* CreateGamepadListWithAxisTilt() {
    double axes[1] = {0.95};
    device::GamepadButton buttons[1] = {{false, false, 0.0}};

    auto* list = MakeGarbageCollected<GamepadList>();
    auto* gamepad = CreateGamepad();
    gamepad->SetId("gamepad");
    gamepad->SetAxes(1, axes);
    gamepad->SetButtons(1, buttons);
    gamepad->SetConnected(true);
    list->Set(0, gamepad);
    return list;
  }

  GamepadList* CreateGamepadListWithButtonDown() {
    double axes[1] = {0.0};
    device::GamepadButton buttons[1] = {{true, true, 1.0}};

    auto* list = MakeGarbageCollected<GamepadList>();
    auto* gamepad = CreateGamepad();
    gamepad->SetId("gamepad");
    gamepad->SetAxes(1, axes);
    gamepad->SetButtons(1, buttons);
    gamepad->SetConnected(true);
    list->Set(0, gamepad);
    return list;
  }

  GamepadList* CreateGamepadListWithButtonTouched() {
    double axes[1] = {0.0};
    device::GamepadButton buttons[1] = {{
        false,
        true,
        // Just before the "pressed" threshold.
        device::GamepadButton::kDefaultButtonPressedThreshold - 0.01,
    }};

    auto* list = MakeGarbageCollected<GamepadList>();
    auto* gamepad = CreateGamepad();
    gamepad->SetId("gamepad");
    gamepad->SetAxes(1, axes);
    gamepad->SetButtons(1, buttons);
    gamepad->SetConnected(true);
    list->Set(0, gamepad);
    return list;
  }

  GamepadList* CreateGamepadListWithButtonJustDown() {
    double axes[1] = {0.0};
    device::GamepadButton buttons[1] = {{
        true,
        true,
        // Just beyond the "pressed" threshold.
        device::GamepadButton::kDefaultButtonPressedThreshold + 0.01,
    }};

    auto* list = MakeGarbageCollected<GamepadList>();
    auto* gamepad = CreateGamepad();
    gamepad->SetId("gamepad");
    gamepad->SetAxes(1, axes);
    gamepad->SetButtons(1, buttons);
    gamepad->SetConnected(true);
    list->Set(0, gamepad);
    return list;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GamepadComparisonsTest);
};

TEST_F(GamepadComparisonsTest, EmptyListCausesNoActivation) {
  auto* list = CreateEmptyGamepadList();
  EXPECT_FALSE(GamepadComparisons::HasUserActivation(list));
}

TEST_F(GamepadComparisonsTest, NeutralGamepadCausesNoActivation) {
  auto* list = CreateGamepadListWithNeutralGamepad();
  EXPECT_FALSE(GamepadComparisons::HasUserActivation(list));
}

TEST_F(GamepadComparisonsTest, AxisTiltCausesNoActivation) {
  auto* list = CreateGamepadListWithAxisTilt();
  EXPECT_FALSE(GamepadComparisons::HasUserActivation(list));
}

TEST_F(GamepadComparisonsTest, ButtonDownCausesActivation) {
  auto* list = CreateGamepadListWithButtonDown();
  EXPECT_TRUE(GamepadComparisons::HasUserActivation(list));
}

TEST_F(GamepadComparisonsTest, CompareEmptyLists) {
  // Simulate no connected gamepads.
  auto* list1 = CreateEmptyGamepadList();
  auto* list2 = CreateEmptyGamepadList();
  auto compareResult = GamepadComparisons::Compare(
      list1, list2, /*compare_all_axes=*/true, /*compare_all_buttons=*/true);
  EXPECT_FALSE(compareResult.IsDifferent());
  EXPECT_FALSE(compareResult.IsGamepadConnected(0));
  EXPECT_FALSE(compareResult.IsGamepadDisconnected(0));
  EXPECT_FALSE(compareResult.IsAxisChanged(0, 0));
  EXPECT_FALSE(compareResult.IsButtonChanged(0, 0));
  EXPECT_FALSE(compareResult.IsButtonDown(0, 0));
  EXPECT_FALSE(compareResult.IsButtonUp(0, 0));
}

TEST_F(GamepadComparisonsTest, CompareNeutrals) {
  // Simulate a neutral gamepad with no input changes.
  auto* list1 = CreateGamepadListWithNeutralGamepad();
  auto* list2 = CreateGamepadListWithNeutralGamepad();
  auto compareResult = GamepadComparisons::Compare(
      list1, list2, /*compare_all_axes=*/true, /*compare_all_buttons=*/true);
  EXPECT_FALSE(compareResult.IsDifferent());
  EXPECT_FALSE(compareResult.IsGamepadConnected(0));
  EXPECT_FALSE(compareResult.IsGamepadDisconnected(0));
  EXPECT_FALSE(compareResult.IsAxisChanged(0, 0));
  EXPECT_FALSE(compareResult.IsButtonChanged(0, 0));
  EXPECT_FALSE(compareResult.IsButtonDown(0, 0));
  EXPECT_FALSE(compareResult.IsButtonUp(0, 0));
}

TEST_F(GamepadComparisonsTest, CompareEmptyListWithNeutral) {
  // Simulate a connection.
  auto* list1 = CreateEmptyGamepadList();
  auto* list2 = CreateGamepadListWithNeutralGamepad();
  auto compareResult = GamepadComparisons::Compare(
      list1, list2, /*compare_all_axes=*/true, /*compare_all_buttons=*/true);
  EXPECT_TRUE(compareResult.IsDifferent());
  EXPECT_TRUE(compareResult.IsGamepadConnected(0));
  EXPECT_FALSE(compareResult.IsGamepadDisconnected(0));
  EXPECT_FALSE(compareResult.IsAxisChanged(0, 0));
  EXPECT_FALSE(compareResult.IsButtonChanged(0, 0));
  EXPECT_FALSE(compareResult.IsButtonDown(0, 0));
  EXPECT_FALSE(compareResult.IsButtonUp(0, 0));
}

TEST_F(GamepadComparisonsTest, CompareNeutralWithEmptyList) {
  // Simulate a disconnection.
  auto* list1 = CreateGamepadListWithNeutralGamepad();
  auto* list2 = CreateEmptyGamepadList();
  auto compareResult = GamepadComparisons::Compare(
      list1, list2, /*compare_all_axes=*/true, /*compare_all_buttons=*/true);
  EXPECT_TRUE(compareResult.IsDifferent());
  EXPECT_FALSE(compareResult.IsGamepadConnected(0));
  EXPECT_TRUE(compareResult.IsGamepadDisconnected(0));
  EXPECT_FALSE(compareResult.IsAxisChanged(0, 0));
  EXPECT_FALSE(compareResult.IsButtonChanged(0, 0));
  EXPECT_FALSE(compareResult.IsButtonDown(0, 0));
  EXPECT_FALSE(compareResult.IsButtonUp(0, 0));
}

TEST_F(GamepadComparisonsTest, CompareNeutralWithAxisTilt) {
  // Simulate tilting an axis away from neutral.
  auto* list1 = CreateGamepadListWithNeutralGamepad();
  auto* list2 = CreateGamepadListWithAxisTilt();

  auto compareResult = GamepadComparisons::Compare(
      list1, list2, /*compare_all_axes=*/true, /*compare_all_buttons=*/true);
  EXPECT_TRUE(compareResult.IsDifferent());
  EXPECT_FALSE(compareResult.IsGamepadConnected(0));
  EXPECT_FALSE(compareResult.IsGamepadDisconnected(0));
  EXPECT_TRUE(compareResult.IsAxisChanged(0, 0));
  EXPECT_FALSE(compareResult.IsButtonChanged(0, 0));
  EXPECT_FALSE(compareResult.IsButtonDown(0, 0));
  EXPECT_FALSE(compareResult.IsButtonUp(0, 0));

  // Using compare_all_axes=false, comparison flags are not set for individual
  // axes.
  auto compareResult2 = GamepadComparisons::Compare(
      list1, list2, /*compare_all_axes*/ false, /*compare_all_buttons*/ true);
  EXPECT_TRUE(compareResult2.IsDifferent());
  EXPECT_FALSE(compareResult2.IsGamepadConnected(0));
  EXPECT_FALSE(compareResult2.IsGamepadDisconnected(0));
  EXPECT_FALSE(compareResult2.IsAxisChanged(0, 0));
  EXPECT_FALSE(compareResult2.IsButtonChanged(0, 0));
  EXPECT_FALSE(compareResult2.IsButtonDown(0, 0));
  EXPECT_FALSE(compareResult2.IsButtonUp(0, 0));
}

TEST_F(GamepadComparisonsTest, CompareNeutralWithButtonDown) {
  // Simulate pressing a digital (on/off) button.
  auto* list1 = CreateGamepadListWithNeutralGamepad();
  auto* list2 = CreateGamepadListWithButtonDown();

  auto compareResult = GamepadComparisons::Compare(
      list1, list2, /*compare_all_axes=*/true, /*compare_all_buttons=*/true);
  EXPECT_TRUE(compareResult.IsDifferent());
  EXPECT_FALSE(compareResult.IsGamepadConnected(0));
  EXPECT_FALSE(compareResult.IsGamepadDisconnected(0));
  EXPECT_FALSE(compareResult.IsAxisChanged(0, 0));
  EXPECT_TRUE(compareResult.IsButtonChanged(0, 0));
  EXPECT_TRUE(compareResult.IsButtonDown(0, 0));
  EXPECT_FALSE(compareResult.IsButtonUp(0, 0));

  // Using compare_all_buttons=false, comparison flags are not set for
  // individual buttons.
  auto compareResult2 = GamepadComparisons::Compare(
      list1, list2, /*compare_all_axes*/ true, /*compare_all_buttons*/ false);
  EXPECT_TRUE(compareResult2.IsDifferent());
  EXPECT_FALSE(compareResult2.IsGamepadConnected(0));
  EXPECT_FALSE(compareResult2.IsGamepadDisconnected(0));
  EXPECT_FALSE(compareResult2.IsAxisChanged(0, 0));
  EXPECT_FALSE(compareResult2.IsButtonChanged(0, 0));
  EXPECT_FALSE(compareResult2.IsButtonDown(0, 0));
  EXPECT_FALSE(compareResult2.IsButtonUp(0, 0));
}

TEST_F(GamepadComparisonsTest, CompareButtonDownWithNeutral) {
  // Simulate releasing a digital (on/off) button.
  auto* list1 = CreateGamepadListWithButtonDown();
  auto* list2 = CreateGamepadListWithNeutralGamepad();

  auto compareResult = GamepadComparisons::Compare(
      list1, list2, /*compare_all_axes=*/true, /*compare_all_buttons=*/true);
  EXPECT_TRUE(compareResult.IsDifferent());
  EXPECT_FALSE(compareResult.IsGamepadConnected(0));
  EXPECT_FALSE(compareResult.IsGamepadDisconnected(0));
  EXPECT_FALSE(compareResult.IsAxisChanged(0, 0));
  EXPECT_TRUE(compareResult.IsButtonChanged(0, 0));
  EXPECT_FALSE(compareResult.IsButtonDown(0, 0));
  EXPECT_TRUE(compareResult.IsButtonUp(0, 0));
}

TEST_F(GamepadComparisonsTest, CompareNeutralWithButtonTouched) {
  // Simulate touching an analog button or trigger.
  auto* list1 = CreateGamepadListWithNeutralGamepad();
  auto* list2 = CreateGamepadListWithButtonTouched();

  auto compareResult = GamepadComparisons::Compare(
      list1, list2, /*compare_all_axes=*/true, /*compare_all_buttons=*/true);
  EXPECT_TRUE(compareResult.IsDifferent());
  EXPECT_FALSE(compareResult.IsGamepadConnected(0));
  EXPECT_FALSE(compareResult.IsGamepadDisconnected(0));
  EXPECT_FALSE(compareResult.IsAxisChanged(0, 0));
  EXPECT_TRUE(compareResult.IsButtonChanged(0, 0));
  EXPECT_FALSE(compareResult.IsButtonDown(0, 0));
  EXPECT_FALSE(compareResult.IsButtonUp(0, 0));
}

TEST_F(GamepadComparisonsTest, CompareButtonTouchedWithButtonJustDown) {
  // Simulate pressing an analog button or trigger enough to register a button
  // press.
  auto* list1 = CreateGamepadListWithButtonTouched();
  auto* list2 = CreateGamepadListWithButtonJustDown();

  auto compareResult = GamepadComparisons::Compare(
      list1, list2, /*compare_all_axes=*/true, /*compare_all_buttons=*/true);
  EXPECT_TRUE(compareResult.IsDifferent());
  EXPECT_FALSE(compareResult.IsGamepadConnected(0));
  EXPECT_FALSE(compareResult.IsGamepadDisconnected(0));
  EXPECT_FALSE(compareResult.IsAxisChanged(0, 0));
  EXPECT_TRUE(compareResult.IsButtonChanged(0, 0));
  EXPECT_TRUE(compareResult.IsButtonDown(0, 0));
  EXPECT_FALSE(compareResult.IsButtonUp(0, 0));
}

TEST_F(GamepadComparisonsTest, CompareButtonJustDownWithButtonDown) {
  // Simulate continuing to press an analog button or trigger until it reaches
  // the maximum value.
  auto* list1 = CreateGamepadListWithButtonJustDown();
  auto* list2 = CreateGamepadListWithButtonDown();

  auto compareResult = GamepadComparisons::Compare(
      list1, list2, /*compare_all_axes=*/true, /*compare_all_buttons=*/true);
  EXPECT_TRUE(compareResult.IsDifferent());
  EXPECT_FALSE(compareResult.IsGamepadConnected(0));
  EXPECT_FALSE(compareResult.IsGamepadDisconnected(0));
  EXPECT_FALSE(compareResult.IsAxisChanged(0, 0));
  EXPECT_TRUE(compareResult.IsButtonChanged(0, 0));
  EXPECT_FALSE(compareResult.IsButtonDown(0, 0));
  EXPECT_FALSE(compareResult.IsButtonUp(0, 0));
}

TEST_F(GamepadComparisonsTest, CompareButtonDownWithButtonJustDown) {
  // Simulate releasing an analog button or trigger until it is just barely
  // pressed.
  auto* list1 = CreateGamepadListWithButtonDown();
  auto* list2 = CreateGamepadListWithButtonJustDown();

  auto compareResult = GamepadComparisons::Compare(
      list1, list2, /*compare_all_axes=*/true, /*compare_all_buttons=*/true);
  EXPECT_TRUE(compareResult.IsDifferent());
  EXPECT_FALSE(compareResult.IsGamepadConnected(0));
  EXPECT_FALSE(compareResult.IsGamepadDisconnected(0));
  EXPECT_FALSE(compareResult.IsAxisChanged(0, 0));
  EXPECT_TRUE(compareResult.IsButtonChanged(0, 0));
  EXPECT_FALSE(compareResult.IsButtonDown(0, 0));
  EXPECT_FALSE(compareResult.IsButtonUp(0, 0));
}

TEST_F(GamepadComparisonsTest, CompareButtonJustDownWithButtonTouched) {
  // Simulate releasing an analog button or trigger until it is no longer
  // pressed.
  auto* list1 = CreateGamepadListWithButtonJustDown();
  auto* list2 = CreateGamepadListWithButtonTouched();

  auto compareResult = GamepadComparisons::Compare(
      list1, list2, /*compare_all_axes=*/true, /*compare_all_buttons=*/true);
  EXPECT_TRUE(compareResult.IsDifferent());
  EXPECT_FALSE(compareResult.IsGamepadConnected(0));
  EXPECT_FALSE(compareResult.IsGamepadDisconnected(0));
  EXPECT_FALSE(compareResult.IsAxisChanged(0, 0));
  EXPECT_TRUE(compareResult.IsButtonChanged(0, 0));
  EXPECT_FALSE(compareResult.IsButtonDown(0, 0));
  EXPECT_TRUE(compareResult.IsButtonUp(0, 0));
}

TEST_F(GamepadComparisonsTest, CompareButtonTouchedWithNeutral) {
  // Simulate releasing an analog button or trigger until it is neutral.
  auto* list1 = CreateGamepadListWithButtonTouched();
  auto* list2 = CreateGamepadListWithNeutralGamepad();

  auto compareResult = GamepadComparisons::Compare(
      list1, list2, /*compare_all_axes=*/true, /*compare_all_buttons=*/true);
  EXPECT_TRUE(compareResult.IsDifferent());
  EXPECT_FALSE(compareResult.IsGamepadConnected(0));
  EXPECT_FALSE(compareResult.IsGamepadDisconnected(0));
  EXPECT_FALSE(compareResult.IsAxisChanged(0, 0));
  EXPECT_TRUE(compareResult.IsButtonChanged(0, 0));
  EXPECT_FALSE(compareResult.IsButtonDown(0, 0));
  EXPECT_FALSE(compareResult.IsButtonUp(0, 0));
}

}  // namespace blink
