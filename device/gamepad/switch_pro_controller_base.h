// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_SWITCH_PRO_CONTROLLER_BASE_
#define DEVICE_GAMEPAD_SWITCH_PRO_CONTROLLER_BASE_

#include "device/gamepad/abstract_haptic_gamepad.h"
#include "device/gamepad/public/cpp/gamepad.h"

namespace device {

class SwitchProControllerBase : public AbstractHapticGamepad {
 public:
  // Maximum size of a Switch HID report, in bytes.
  static const int kReportSize = 64;

  SwitchProControllerBase() = default;
  ~SwitchProControllerBase() override;

  static bool IsSwitchPro(uint16_t vendor_id, uint16_t product_id);

  void DoShutdown() override;

  void ReadUsbPadState(Gamepad* pad);
  void HandleInputReport(void* report, size_t report_length, Gamepad* pad);

  void SendConnectionStatusQuery();
  void SendHandshake();
  void SendForceUsbHid(bool enable);
  void SetVibration(double strong_magnitude, double weak_magnitude) override;

  virtual size_t ReadInputReport(void* report);
  virtual size_t WriteOutputReport(void* report, size_t report_length);

 private:
  uint32_t counter_ = 0;
  bool force_usb_hid_ = false;
  bool sent_handshake_ = false;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_SWITCH_PRO_CONTROLLER_BASE_
