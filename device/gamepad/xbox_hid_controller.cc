// Copyright 2018 The Chromium Authors. All rights reserved.
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
bool XboxHidController::IsXboxHid(uint16_t vendor_id, uint16_t product_id) {
  auto gamepad_id = GamepadIdList::Get().GetGamepadId(vendor_id, product_id);
  return gamepad_id == GamepadId::kMicrosoftProduct02e0 ||
         gamepad_id == GamepadId::kMicrosoftProduct02fd;
}

void XboxHidController::DoShutdown() {
  writer_.reset();
}

void XboxHidController::SetVibration(double strong_magnitude,
                                     double weak_magnitude) {
  DCHECK(writer_);
  std::array<uint8_t, 9> control_report;
  control_report.fill(0);
  control_report[0] = 0x03;  // report ID
  control_report[1] = 0x03;  // enable rumble motors, disable trigger haptics
  control_report[4] =
      static_cast<uint8_t>(strong_magnitude * kRumbleMagnitudeMax);
  control_report[5] =
      static_cast<uint8_t>(weak_magnitude * kRumbleMagnitudeMax);
  control_report[6] = 0xff;  // duration
  control_report[7] = 0x00;  // start delay
  control_report[8] = 0x01;  // loop count

  writer_->WriteOutputReport(control_report);
}

base::WeakPtr<AbstractHapticGamepad> XboxHidController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
