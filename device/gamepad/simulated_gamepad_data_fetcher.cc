// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/simulated_gamepad_data_fetcher.h"

#include "device/gamepad/gamepad_pad_state_provider.h"
#include "device/gamepad/normalization.h"

namespace device {

namespace {

void InitializeGamepadState(const SimulatedGamepadParams& params,
                            PadState& state) {
  // TODO(crbug.com/439640696): Select a mapping function.
  state.mapper = nullptr;
  Gamepad& pad = state.data;

  // Initialize Gamepad.buttons and Gamepad.axes.
  pad.buttons_length = params.button_bounds.size();
  pad.axes_length = params.axis_bounds.size();

  // Initialize Gamepad.vibrationActuator.
  // TODO(crbug.com/439630593): Represent effect types as a set so that an
  // actuator can support (for example) kTriggerRumble but not kDualRumble.
  if (params.vibration.contains(GamepadHapticEffectType::kTriggerRumble)) {
    pad.vibration_actuator.type = GamepadHapticActuatorType::kTriggerRumble;
    pad.vibration_actuator.not_null = true;
  } else if (params.vibration.contains(GamepadHapticEffectType::kDualRumble)) {
    pad.vibration_actuator.type = GamepadHapticActuatorType::kDualRumble;
    pad.vibration_actuator.not_null = true;
  } else {
    pad.vibration_actuator.not_null = false;
  }

  // Initialize Gamepad.timestamp.
  pad.timestamp = GamepadDataFetcher::CurrentTimeInMicroseconds();

  // Set Gamepad.id and Gamepad.mapping strings.
  std::string name;
  if (params.name.has_value()) {
    name = params.name.value();
  }
  if (params.vendor_product.has_value()) {
    const auto& [vendor, product] = params.vendor_product.value();
    GamepadDataFetcher::UpdateGamepadStrings(
        name, vendor, product, params.mapping == GamepadMapping::kStandard,
        pad);
  } else {
    GamepadDataFetcher::UpdateGamepadStrings(
        name, /*vendor_id=*/0, /*product_id=*/0,
        params.mapping == GamepadMapping::kStandard, pad);
  }

  // Override Gamepad.mapping if set in `params`.
  if (params.mapping.has_value()) {
    pad.mapping = params.mapping.value();
  }

  state.is_initialized = true;
}

}  // namespace

SimulatedGamepadDataFetcher::SimulatedGamepad::SimulatedGamepad(
    SimulatedGamepadParams params,
    uint32_t source_id)
    : params_(std::move(params)),
      source_id_(source_id),
      axis_count_(
          std::min(params_.axis_bounds.size(), Gamepad::kAxesLengthCap)),
      button_count_(
          std::min(params_.button_bounds.size(), Gamepad::kButtonsLengthCap)) {}

SimulatedGamepadDataFetcher::SimulatedGamepad::~SimulatedGamepad() = default;

void SimulatedGamepadDataFetcher::SimulatedGamepad::UpdateGamepadState(
    Gamepad& pad) {
  // Indicate that the gamepad is still connected.
  pad.connected = true;
  pad.timestamp = GamepadDataFetcher::CurrentTimeInMicroseconds();

  // Process axis inputs.
  for (const auto& entry : pending_axis_inputs_) {
    const auto& [index, logical_value] = entry;
    if (index < axis_count_) {
      auto& logical_bounds = params_.axis_bounds[index];
      if (logical_bounds.has_value()) {
        pad.axes[index] =
            NormalizeGamepadAxis(logical_value, logical_bounds.value());
      } else {
        pad.axes[index] = logical_value;
      }
    }
  }
  pending_axis_inputs_.clear();

  // Process button inputs.
  for (const auto& entry : pending_button_inputs_) {
    const auto& [index, simulated_button] = entry;
    if (index < button_count_) {
      GamepadButton& button = pad.buttons[index];
      auto& logical_bounds = params_.button_bounds[index];
      if (logical_bounds.has_value()) {
        button.value = NormalizeGamepadButton(simulated_button.logical_value,
                                              logical_bounds.value());
      } else {
        button.value = simulated_button.logical_value;
      }

      if (simulated_button.pressed.has_value()) {
        button.pressed = simulated_button.pressed.value();
      } else {
        button.pressed =
            button.value > GamepadButton::kDefaultButtonPressedThreshold;
      }

      if (simulated_button.touched.has_value()) {
        button.touched = simulated_button.touched.value();
      } else {
        button.touched = button.pressed || button.value > 0.0;
      }
    }
  }
  pending_button_inputs_.clear();

  // Process touch inputs.
  pad.touch_events_length = 0;
  for (const SimulatedGamepadTouch& simulated_touch : active_touches_) {
    if (pad.touch_events_length >= Gamepad::kTouchEventsLengthCap) {
      break;
    }

    // Ignore touches on non-existent surfaces.
    if (simulated_touch.surface_id >= params_.touch_surface_bounds.size()) {
      continue;
    }
    const auto& bounds =
        params_.touch_surface_bounds[simulated_touch.surface_id];
    GamepadTouch& touch = pad.touch_events[pad.touch_events_length];
    ++pad.touch_events_length;
    touch.touch_id = simulated_touch.touch_id;
    touch.surface_id = simulated_touch.surface_id;
    if (bounds.has_value()) {
      const auto& x_bounds = bounds.value().x;
      const auto& y_bounds = bounds.value().y;
      touch.x = NormalizeGamepadAxis(simulated_touch.logical_x, x_bounds);
      touch.y = NormalizeGamepadAxis(simulated_touch.logical_y, y_bounds);
      touch.has_surface_dimensions = true;
      touch.surface_width = x_bounds.maximum - x_bounds.minimum;
      touch.surface_height = y_bounds.maximum - y_bounds.minimum;
    } else {
      touch.x = simulated_touch.logical_x;
      touch.y = simulated_touch.logical_y;
      touch.has_surface_dimensions = false;
    }
  }
}

void SimulatedGamepadDataFetcher::SimulatedGamepad::SetVibration(
    mojom::GamepadEffectParametersPtr params) {
  // TODO(crbug.com/439630594): Fire an event with the effect parameters.
}

base::WeakPtr<AbstractHapticGamepad>
SimulatedGamepadDataFetcher::SimulatedGamepad::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

SimulatedGamepadDataFetcher::SimulatedGamepadDataFetcher() = default;

SimulatedGamepadDataFetcher::~SimulatedGamepadDataFetcher() {
  std::ranges::for_each(gamepads_, [](auto& e) { e.second.Shutdown(); });
}

GamepadSource SimulatedGamepadDataFetcher::source() {
  return Factory::static_source();
}

void SimulatedGamepadDataFetcher::GetGamepadData(bool) {
  for (auto& entry : gamepads_) {
    SimulatedGamepad& gamepad = entry.second;
    PadState* state = GetPadState(gamepad.source_id_);
    if (!state) {
      continue;
    }
    if (!state->is_initialized) {
      InitializeGamepadState(gamepad.params_, *state);
    }
    gamepad.UpdateGamepadState(state->data);
  }
  if (on_poll_) {
    on_poll_.Run();
  }
}

void SimulatedGamepadDataFetcher::PlayEffect(
    int source_id,
    mojom::GamepadHapticEffectType type,
    mojom::GamepadEffectParametersPtr params,
    mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  auto find_it = std::ranges::find_if(gamepads_, [&](const auto& entry) {
    return entry.second.source_id_ == static_cast<uint32_t>(source_id);
  });
  if (find_it == gamepads_.end()) {
    // No connected gamepad with this `source_id`. Handle as if it were
    // preempted by a disconnect.
    RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultPreempted);
    return;
  }
  find_it->second.PlayEffect(type, std::move(params), std::move(callback),
                             std::move(callback_runner));
}

void SimulatedGamepadDataFetcher::ResetVibration(
    int source_id,
    mojom::GamepadHapticsManager::ResetVibrationActuatorCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  auto find_it = std::ranges::find_if(gamepads_, [&](const auto& entry) {
    return entry.second.source_id_ == static_cast<uint32_t>(source_id);
  });
  if (find_it == gamepads_.end()) {
    // No connected gamepad with this `source_id`. Handle as if it were
    // preempted by a disconnect.
    RunVibrationCallback(
        std::move(callback), std::move(callback_runner),
        mojom::GamepadHapticsResult::GamepadHapticsResultPreempted);
    return;
  }
  find_it->second.ResetVibration(std::move(callback),
                                 std::move(callback_runner));
}

void SimulatedGamepadDataFetcher::AddSimulatedGamepad(
    base::UnguessableToken token,
    SimulatedGamepadParams params) {
  CHECK(token);
  gamepads_.emplace(
      std::piecewise_construct, std::forward_as_tuple(token),
      std::forward_as_tuple(std::move(params), next_source_id_++));
}

void SimulatedGamepadDataFetcher::RemoveSimulatedGamepad(
    base::UnguessableToken token) {
  auto find_it = gamepads_.find(token);
  if (find_it == gamepads_.end()) {
    return;
  }
  find_it->second.Shutdown();
  gamepads_.erase(find_it);
}

void SimulatedGamepadDataFetcher::SimulateInputFrame(
    base::UnguessableToken token,
    SimulatedGamepadInputs inputs) {
  auto find_it = gamepads_.find(token);
  if (find_it == gamepads_.end()) {
    return;
  }
  SimulatedGamepad& gamepad = find_it->second;
  std::ranges::for_each(inputs.pending_axis_inputs, [&](const auto& entry) {
    const auto& [index, value] = entry;
    gamepad.pending_axis_inputs_[index] = value;
  });
  std::ranges::for_each(inputs.pending_button_inputs, [&](const auto& entry) {
    const auto& [index, button] = entry;
    gamepad.pending_button_inputs_[index] = button;
  });
  gamepad.active_touches_ = std::move(inputs.active_touches);
}

void SimulatedGamepadDataFetcher::SetOnPollForTesting(
    base::RepeatingClosure on_poll) {
  on_poll_ = std::move(on_poll);
}

}  // namespace device
