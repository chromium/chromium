/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/modules/gamepad/gamepad.h"

#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad_comparisons.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

#include <algorithm>

namespace blink {

Gamepad::Gamepad(Client* client,
                 int index,
                 base::TimeTicks time_origin,
                 base::TimeTicks time_floor)
    : client_(client),
      index_(index),
      timestamp_(0.0),
      has_vibration_actuator_(false),
      vibration_actuator_type_(device::GamepadHapticActuatorType::kDualRumble),
      is_axis_data_dirty_(true),
      is_button_data_dirty_(true),
      time_origin_(time_origin),
      time_floor_(time_floor) {
  DCHECK(!time_origin_.is_null());
  DCHECK(!time_floor_.is_null());
  DCHECK_LE(time_origin_, time_floor_);
}

Gamepad::~Gamepad() = default;

void Gamepad::UpdateFromDeviceState(const device::Gamepad& device_gamepad) {
  bool newly_connected;
  GamepadComparisons::HasGamepadConnectionChanged(
      connected(),                            // Old connected.
      device_gamepad.connected,               // New connected.
      id() != StringView(device_gamepad.id),  // ID changed.
      &newly_connected, nullptr);

  SetConnected(device_gamepad.connected);
  SetTimestamp(device_gamepad);
  SetAxes(device_gamepad.axes_length, device_gamepad.axes);
  SetButtons(device_gamepad.buttons_length, device_gamepad.buttons);
  // Always called as gamepads require additional steps to determine haptics
  // capability and thus may provide them when not |newly_connected|. This is
  // also simpler than logic to conditionally call.
  SetVibrationActuatorInfo(device_gamepad.vibration_actuator);

  // These fields are not expected to change and will only be written when the
  // gamepad is newly connected.
  if (newly_connected) {
    SetId(device_gamepad.id);
    SetMapping(device_gamepad.mapping);
  }
}

void Gamepad::SetMapping(device::GamepadMapping mapping) {
  switch (mapping) {
    case device::GamepadMapping::kNone:
      mapping_ = "";
      return;
    case device::GamepadMapping::kStandard:
      mapping_ = "standard";
      return;
    case device::GamepadMapping::kXrStandard:
      mapping_ = "xr-standard";
      return;
  }
  NOTREACHED();
}

const Gamepad::DoubleVector& Gamepad::axes() {
  is_axis_data_dirty_ = false;
  return axes_;
}

void Gamepad::SetAxes(unsigned count, const double* data) {
  bool skip_update =
      axes_.size() == count && std::equal(data, data + count, axes_.begin());
  if (skip_update)
    return;

  axes_.resize(count);
  if (count)
    std::copy(data, data + count, axes_.begin());
  is_axis_data_dirty_ = true;
}

const GamepadButtonVector& Gamepad::buttons() {
  is_button_data_dirty_ = false;
  return buttons_;
}

void Gamepad::SetButtons(unsigned count, const device::GamepadButton* data) {
  bool skip_update =
      buttons_.size() == count &&
      std::equal(data, data + count, buttons_.begin(),
                 [](const device::GamepadButton& device_gamepad_button,
                    const Member<GamepadButton>& gamepad_button) {
                   return gamepad_button->IsEqual(device_gamepad_button);
                 });
  if (skip_update)
    return;

  if (buttons_.size() != count) {
    buttons_.resize(count);
    for (unsigned i = 0; i < count; ++i)
      buttons_[i] = MakeGarbageCollected<GamepadButton>();
  }
  for (unsigned i = 0; i < count; ++i)
    buttons_[i]->UpdateValuesFrom(data[i]);
  is_button_data_dirty_ = true;
}

GamepadHapticActuator* Gamepad::vibrationActuator() const {
  return client_->GetVibrationActuatorForGamepad(*this);
}

void Gamepad::SetVibrationActuatorInfo(
    const device::GamepadHapticActuator& actuator) {
  has_vibration_actuator_ = actuator.not_null;
  vibration_actuator_type_ = actuator.type;
}

// Convert the raw timestamp from the device to a relative one and apply the
// floor.
void Gamepad::SetTimestamp(const device::Gamepad& device_gamepad) {
  base::TimeTicks last_updated =
      base::TimeTicks() +
      base::TimeDelta::FromMicroseconds(device_gamepad.timestamp);
  if (last_updated < time_floor_)
    last_updated = time_floor_;

  timestamp_ = Performance::MonotonicTimeToDOMHighResTimeStamp(
      time_origin_, last_updated, false);

  if (device_gamepad.is_xr) {
    base::TimeTicks now = base::TimeTicks::Now();
    TRACE_COUNTER1("input", "XR gamepad pose age (ms)",
                   (now - last_updated).InMilliseconds());
  }
}

void Gamepad::Trace(blink::Visitor* visitor) {
  visitor->Trace(client_);
  visitor->Trace(buttons_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
