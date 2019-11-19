// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/gamepad/gamepad_comparisons.h"

#include "third_party/blink/renderer/modules/gamepad/gamepad.h"

namespace blink {

namespace {

// A button press must have a value at least this large to qualify as a user
// activation. The selected value should be greater than 0.5 so that axes
// incorrectly mapped as triggers do not generate activations in the idle
// position.
const double kButtonActivationThreshold = 0.9;

}  // namespace

// static
bool GamepadComparisons::HasUserActivation(GamepadList* gamepads) {
  if (!gamepads)
    return false;
  // A button press counts as a user activation if the button's value is greater
  // than the activation threshold. A threshold is used so that analog buttons
  // or triggers do not generate an activation from a light touch.
  for (wtf_size_t pad_index = 0; pad_index < gamepads->length(); ++pad_index) {
    Gamepad* pad = gamepads->item(pad_index);
    if (pad) {
      for (auto button : pad->buttons()) {
        if (button->value() > kButtonActivationThreshold)
          return true;
      }
    }
  }
  return false;
}

// static
void GamepadComparisons::HasGamepadConnectionChanged(bool old_connected,
                                                     bool new_connected,
                                                     bool id_changed,
                                                     bool* gamepad_found,
                                                     bool* gamepad_lost) {
  if (gamepad_found)
    *gamepad_found = id_changed || (!old_connected && new_connected);
  if (gamepad_lost)
    *gamepad_lost = id_changed || (old_connected && !new_connected);
}

GamepadStateCompareResult::GamepadStateCompareResult(GamepadList* old_gamepads,
                                                     GamepadList* new_gamepads,
                                                     bool compare_all_axes,
                                                     bool compare_all_buttons) {
  any_change_ = CompareGamepads(old_gamepads, new_gamepads, compare_all_axes,
                                compare_all_buttons);
}

bool GamepadStateCompareResult::IsDifferent() const {
  return any_change_;
}

bool GamepadStateCompareResult::IsGamepadConnected(size_t pad_index) const {
  DCHECK_LT(pad_index, device::Gamepads::kItemsLengthCap);
  return gamepad_connected_.test(pad_index);
}

bool GamepadStateCompareResult::IsGamepadDisconnected(size_t pad_index) const {
  DCHECK_LT(pad_index, device::Gamepads::kItemsLengthCap);
  return gamepad_disconnected_.test(pad_index);
}

bool GamepadStateCompareResult::IsAxisChanged(size_t pad_index,
                                              size_t axis_index) const {
  DCHECK_LT(pad_index, device::Gamepads::kItemsLengthCap);
  DCHECK_LT(axis_index, device::Gamepad::kAxesLengthCap);
  return axis_changed_[pad_index].test(axis_index);
}

bool GamepadStateCompareResult::IsButtonChanged(size_t pad_index,
                                                size_t button_index) const {
  DCHECK_LT(pad_index, device::Gamepads::kItemsLengthCap);
  DCHECK_LT(button_index, device::Gamepad::kButtonsLengthCap);
  return button_changed_[pad_index].test(button_index);
}

bool GamepadStateCompareResult::IsButtonDown(size_t pad_index,
                                             size_t button_index) const {
  DCHECK_LT(pad_index, device::Gamepads::kItemsLengthCap);
  DCHECK_LT(button_index, device::Gamepad::kButtonsLengthCap);
  return button_down_[pad_index].test(button_index);
}

bool GamepadStateCompareResult::IsButtonUp(size_t pad_index,
                                           size_t button_index) const {
  DCHECK_LT(pad_index, device::Gamepads::kItemsLengthCap);
  DCHECK_LT(button_index, device::Gamepad::kButtonsLengthCap);
  return button_up_[pad_index].test(button_index);
}

bool GamepadStateCompareResult::CompareGamepads(GamepadList* old_gamepads,
                                                GamepadList* new_gamepads,
                                                bool compare_all_axes,
                                                bool compare_all_buttons) {
  if (!new_gamepads)
    return false;
  bool any_change = false;
  for (uint32_t i = 0; i < device::Gamepads::kItemsLengthCap; ++i) {
    Gamepad* old_gamepad = old_gamepads ? old_gamepads->item(i) : nullptr;
    Gamepad* new_gamepad = new_gamepads->item(i);
    // Check whether the gamepad is newly connected or disconnected.
    bool newly_connected = false;
    bool newly_disconnected = false;
    bool old_connected = old_gamepad && old_gamepad->connected();
    bool new_connected = new_gamepad && new_gamepad->connected();
    if (old_gamepad && new_gamepad) {
      GamepadComparisons::HasGamepadConnectionChanged(
          old_connected, new_connected, old_gamepad->id() != new_gamepad->id(),
          &newly_connected, &newly_disconnected);
    } else {
      newly_connected = new_connected;
      newly_disconnected = old_connected;
    }

    bool any_axis_updated =
        CompareAxes(old_gamepad, new_gamepad, i, compare_all_axes);
    bool any_button_updated =
        CompareButtons(old_gamepad, new_gamepad, i, compare_all_buttons);

    if (newly_connected)
      gamepad_connected_.set(i);
    if (newly_disconnected)
      gamepad_disconnected_.set(i);
    if (newly_connected || newly_disconnected || any_axis_updated ||
        any_button_updated) {
      any_change = true;
    }
  }
  return any_change;
}

bool GamepadStateCompareResult::CompareAxes(Gamepad* old_gamepad,
                                            Gamepad* new_gamepad,
                                            size_t index,
                                            bool compare_all) {
  DCHECK_LT(index, device::Gamepads::kItemsLengthCap);
  if (!new_gamepad)
    return false;
  auto& changed_set = axis_changed_[index];
  const auto& new_axes = new_gamepad->axes();
  const auto* old_axes = old_gamepad ? &old_gamepad->axes() : nullptr;
  bool any_axis_changed = false;
  for (wtf_size_t i = 0; i < new_axes.size(); ++i) {
    double new_value = new_axes[i];
    if (old_axes && i < old_axes->size()) {
      double old_value = old_axes->at(i);
      if (old_value != new_value) {
        any_axis_changed = true;
        if (!compare_all)
          break;
        changed_set.set(i);
      }
    } else {
      if (new_value) {
        any_axis_changed = true;
        if (!compare_all)
          break;
        changed_set.set(i);
      }
    }
  }
  return any_axis_changed;
}

bool GamepadStateCompareResult::CompareButtons(Gamepad* old_gamepad,
                                               Gamepad* new_gamepad,
                                               size_t index,
                                               bool compare_all) {
  DCHECK_LT(index, device::Gamepads::kItemsLengthCap);
  if (!new_gamepad)
    return false;
  auto& changed_set = button_changed_[index];
  auto& down_set = button_down_[index];
  auto& up_set = button_up_[index];
  const auto& new_buttons = new_gamepad->buttons();
  const auto* old_buttons = old_gamepad ? &old_gamepad->buttons() : nullptr;
  bool any_button_changed = false;
  for (wtf_size_t i = 0; i < new_buttons.size(); ++i) {
    double new_value = new_buttons[i]->value();
    bool new_pressed = new_buttons[i]->pressed();
    if (old_buttons && i < old_buttons->size()) {
      double old_value = old_buttons->at(i)->value();
      bool old_pressed = old_buttons->at(i)->pressed();
      if (old_value != new_value) {
        any_button_changed = true;
        if (!compare_all)
          break;
        changed_set.set(i);
      }
      if (old_pressed != new_pressed) {
        any_button_changed = true;
        if (!compare_all)
          break;
        if (new_pressed)
          down_set.set(i);
        else
          up_set.set(i);
      }
    } else {
      if (new_value > 0.0) {
        any_button_changed = true;
        if (!compare_all)
          break;
        changed_set.set(i);
      }
      if (new_pressed) {
        any_button_changed = true;
        if (!compare_all)
          break;
        down_set.set(i);
      }
    }
  }
  return any_button_changed;
}

GamepadStateCompareResult GamepadComparisons::Compare(
    GamepadList* old_gamepads,
    GamepadList* new_gamepads,
    bool compare_all_axes,
    bool compare_all_buttons) {
  return GamepadStateCompareResult(old_gamepads, new_gamepads, compare_all_axes,
                                   compare_all_buttons);
}

}  // namespace blink
