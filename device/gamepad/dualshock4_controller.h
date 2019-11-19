// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_DUALSHOCK4_CONTROLLER_H_
#define DEVICE_GAMEPAD_DUALSHOCK4_CONTROLLER_H_

#include <stdint.h>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "device/gamepad/abstract_haptic_gamepad.h"
#include "device/gamepad/gamepad_export.h"
#include "device/gamepad/gamepad_standard_mappings.h"

namespace device {

class HidWriter;

class DEVICE_GAMEPAD_EXPORT Dualshock4Controller final
    : public AbstractHapticGamepad {
 public:
  Dualshock4Controller(uint16_t vendor_id,
                       uint16_t product_id,
                       GamepadBusType bus_type,
                       std::unique_ptr<HidWriter> hid_writer);
  ~Dualshock4Controller() override;

  // Returns true if |vendor_id| and |product_id| match the device IDs for
  // a Dualshock4 gamepad.
  static bool IsDualshock4(uint16_t vendor_id, uint16_t product_id);

  // Detects the transport in use (USB or Bluetooth) given the bcdVersion value
  // reported by the device. Used on Windows where the platform HID API does not
  // expose the transport type.
  static GamepadBusType BusTypeFromVersionNumber(uint32_t version_number);

  // Extracts gamepad inputs from an input report and updates the gamepad state
  // in |pad|. |report_id| is first byte of the report, |report| contains the
  // remaining bytes. Returns true if |pad| was modified.
  bool ProcessInputReport(uint8_t report_id,
                          base::span<const uint8_t> report,
                          Gamepad* pad);

  // AbstractHapticGamepad public implementation.
  void SetVibration(double strong_magnitude, double weak_magnitude) override;
  base::WeakPtr<AbstractHapticGamepad> GetWeakPtr() override;

 private:
  // AbstractHapticGamepad private implementation.
  void DoShutdown() override;

  // Sends a vibration output report suitable for a USB-connected Dualshock4.
  void SetVibrationUsb(double strong_magnitude, double weak_magnitude);

  // Sends a vibration output report suitable for a Bluetooth-connected
  // Dualshock4.
  void SetVibrationBluetooth(double strong_magnitude, double weak_magnitude);

  uint16_t vendor_id_;
  uint16_t product_id_;
  GamepadBusType bus_type_;
  std::unique_ptr<HidWriter> writer_;
  base::WeakPtrFactory<Dualshock4Controller> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_DUALSHOCK4_CONTROLLER_H_
