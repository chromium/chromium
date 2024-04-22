// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/xbox_hid_controller.h"

#include "device/gamepad/gamepad_id_list.h"
#include "device/gamepad/hid_writer.h"

namespace device {

namespace {
const uint8_t kRumbleMagnitudeMax = 0xff;
}  // namespace

XboxHidController::XboxHidController(std::unique_ptr<HidWriter> writer)
    : writer_(std::move(writer)) {}

XboxHidController::~XboxHidController() = default;

// static
bool XboxHidController::IsXboxHid(GamepadId gamepad_id) {
  // TODO(crbug.com/40662063): Detect haptics functionality through HID usages
  // instead of relying on a hard-coded list of supported device IDs.
  // Bluetooth-connected Xbox One gamepads expose usages from the Physical
  // Interface Device usage page.
  // https://www.usb.org/document-library/device-class-definition-pid-10
  return gamepad_id == GamepadId::kMicrosoftProduct02e0 ||
         gamepad_id == GamepadId::kMicrosoftProduct02fd ||
         gamepad_id == GamepadId::kMicrosoftProduct0b05 ||
         gamepad_id == GamepadId::kMicrosoftProduct0b13 ||
         gamepad_id == GamepadId::kMicrosoftProduct0b20 ||
         gamepad_id == GamepadId::kMicrosoftProduct0b22;
}

void XboxHidController::DoShutdown() {
  writer_.reset();
}

void XboxHidController::SetVibration(mojom::GamepadEffectParametersPtr params) {
  DCHECK(writer_);
  std::array<uint8_t, 9> control_report;
  control_report.fill(0);
  control_report[0] = 0x03;  // report ID
  control_report[1] = 0x0f;  // enable rumble motors, enable trigger haptics
  control_report[2] =
      static_cast<uint8_t>(params->right_trigger * kRumbleMagnitudeMax);
  control_report[3] =
      static_cast<uint8_t>(params->left_trigger * kRumbleMagnitudeMax);
  control_report[4] =
      static_cast<uint8_t>(params->strong_magnitude * kRumbleMagnitudeMax);
  control_report[5] =
      static_cast<uint8_t>(params->weak_magnitude * kRumbleMagnitudeMax);
  control_report[6] = 0xff;  // duration
  control_report[7] = 0x00;  // start delay
  control_report[8] = 0x01;  // loop count

  writer_->WriteOutputReport(control_report);
}

base::WeakPtr<AbstractHapticGamepad> XboxHidController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
