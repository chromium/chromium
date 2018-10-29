// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_DUALSHOCK4_CONTROLLER_BASE_
#define DEVICE_GAMEPAD_DUALSHOCK4_CONTROLLER_BASE_

#include "device/gamepad/abstract_haptic_gamepad.h"

namespace device {

class Dualshock4ControllerBase : public AbstractHapticGamepad {
 public:
  Dualshock4ControllerBase() = default;
  ~Dualshock4ControllerBase() override;

  static bool IsDualshock4(uint16_t vendor_id, uint16_t product_id);

  void SetVibration(double strong_magnitude, double weak_magnitude) override;

  virtual size_t WriteOutputReport(void* report, size_t report_length);
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_DUALSHOCK4_CONTROLLER_BASE_
