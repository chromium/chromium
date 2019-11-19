// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_pad_state_provider.h"

#include <cmath>

#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/public/cpp/gamepads.h"

namespace device {

namespace {

const float kMinAxisResetValue = 0.1f;

}  // namespace

PadState::PadState() = default;
PadState::~PadState() = default;

GamepadPadStateProvider::GamepadPadStateProvider() {
  pad_states_.reset(new PadState[Gamepads::kItemsLengthCap]);

  for (size_t i = 0; i < Gamepads::kItemsLengthCap; ++i)
    ClearPadState(pad_states_.get()[i]);
}

GamepadPadStateProvider::~GamepadPadStateProvider() = default;

PadState* GamepadPadStateProvider::GetPadState(GamepadSource source,
                                               int source_id,
                                               bool new_gamepad_recognized) {
  // Check to see if the device already has a reserved slot
  PadState* empty_slot = nullptr;
  PadState* unrecognized_slot = nullptr;
  for (size_t i = 0; i < Gamepads::kItemsLengthCap; ++i) {
    PadState& state = pad_states_.get()[i];
    if (state.source == source && state.source_id == source_id) {
      // Retrieving the pad state marks this gamepad as active.
      state.is_active = true;
      return &state;
    }
    if (!empty_slot && state.source == GAMEPAD_SOURCE_NONE)
      empty_slot = &state;
    if (!state.is_recognized)
      unrecognized_slot = &state;
  }

  if (!empty_slot && unrecognized_slot && new_gamepad_recognized) {
    DisconnectUnrecognizedGamepad(unrecognized_slot->source,
                                  unrecognized_slot->source_id);
    empty_slot = unrecognized_slot;
  }
  if (empty_slot) {
    empty_slot->source = source;
    empty_slot->source_id = source_id;
    empty_slot->is_active = true;
    empty_slot->is_newly_active = true;
    empty_slot->is_initialized = false;
    empty_slot->is_recognized = new_gamepad_recognized;
  }
  return empty_slot;
}

PadState* GamepadPadStateProvider::GetConnectedPadState(uint32_t pad_index) {
  if (pad_index >= Gamepads::kItemsLengthCap)
    return nullptr;

  PadState& pad_state = pad_states_.get()[pad_index];
  if (pad_state.source == GAMEPAD_SOURCE_NONE)
    return nullptr;

  return &pad_state;
}

void GamepadPadStateProvider::ClearPadState(PadState& state) {
  memset(&state, 0, sizeof(PadState));
}

void GamepadPadStateProvider::InitializeDataFetcher(
    GamepadDataFetcher* fetcher,
    service_manager::Connector* service_manager_connector) {
  fetcher->InitializeProvider(this, service_manager_connector);
}

void GamepadPadStateProvider::MapAndSanitizeGamepadData(PadState* pad_state,
                                                        Gamepad* pad,
                                                        bool sanitize) {
  DCHECK(pad_state);
  DCHECK(pad);

  if (!pad_state->data.connected) {
    memset(pad, 0, sizeof(Gamepad));
    return;
  }

  // Copy the current state to the output buffer, using the mapping
  // function, if there is one available.
  if (pad_state->mapper)
    pad_state->mapper(pad_state->data, pad);
  else
    *pad = pad_state->data;

  pad->connected = true;

  if (!sanitize)
    return;

  // About sanitization: Gamepads may report input event if the user is not
  // interacting with it, due to hardware problems or environmental ones (pad
  // has something heavy leaning against an axis.) This may cause user gestures
  // to be detected erroniously, exposing gamepad information when the user had
  // no intention of doing so. To avoid this we require that each button or axis
  // report being at rest (zero) at least once before exposing its value to the
  // Gamepad API. This state is tracked by the axis_mask and button_mask
  // bitfields. If the bit for an axis or button is 0 it means the axis has
  // never reported being at rest, and the value will be forced to zero.

  // We can skip axis sanitation if all available axes have been masked.
  uint32_t full_axis_mask = (1 << pad->axes_length) - 1;
  if (pad_state->axis_mask != full_axis_mask) {
    for (size_t axis = 0; axis < pad->axes_length; ++axis) {
      if (!(pad_state->axis_mask & 1 << axis)) {
        if (fabs(pad->axes[axis]) < kMinAxisResetValue) {
          pad_state->axis_mask |= 1 << axis;
        } else {
          pad->axes[axis] = 0.0f;
        }
      }
    }
  }

  // We can skip button sanitation if all available buttons have been masked.
  uint32_t full_button_mask = (1 << pad->buttons_length) - 1;
  if (pad_state->button_mask != full_button_mask) {
    for (size_t button = 0; button < pad->buttons_length; ++button) {
      if (!(pad_state->button_mask & 1 << button)) {
        if (!pad->buttons[button].pressed) {
          pad_state->button_mask |= 1 << button;
        } else {
          pad->buttons[button].pressed = false;
          pad->buttons[button].value = 0.0f;
        }
      }
    }
  }
}

}  // namespace device
