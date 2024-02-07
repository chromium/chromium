// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_DUALSHOCK4_CONTROLLER_H_
#define DEVICE_GAMEPAD_DUALSHOCK4_CONTROLLER_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <tuple>

#include "base/memory/weak_ptr.h"
#include "device/gamepad/abstract_haptic_gamepad.h"
#include "device/gamepad/gamepad_export.h"
#include "device/gamepad/gamepad_id_list.h"
#include "device/gamepad/gamepad_standard_mappings.h"

namespace device {

class HidWriter;

class DEVICE_GAMEPAD_EXPORT Dualshock4Controller final
    : public AbstractHapticGamepad {
 public:
  Dualshock4Controller(GamepadId gamepad_id,
                       GamepadBusType bus_type,
                       std::unique_ptr<HidWriter> hid_writer);
  ~Dualshock4Controller() override;

  // Returns true if |gamepad_id| matches a Dualshock4 gamepad.
  static bool IsDualshock4(GamepadId gamepad_id);

  // Detects the transport in use (USB or Bluetooth) given the bcdVersion value
  // reported by the device. Used on Windows where the platform HID API does not
  // expose the transport type.
  static GamepadBusType BusTypeFromVersionNumber(uint32_t version_number);

  // Extracts gamepad inputs from an input report and updates the gamepad state
  // in |pad|. |report_id| is first byte of the report, |report| contains the
  // remaining bytes. When |ignore_button_axis| is true, only the touch data
  // is updated into the |pad|. Returns true if |pad| was modified.
  bool ProcessInputReport(uint8_t report_id,
                          base::span<const uint8_t> report,
                          Gamepad* pad,
                          bool ignore_button_axis = false,
                          bool is_multitouch_enabled = false);

  // AbstractHapticGamepad public implementation.
  void SetVibration(mojom::GamepadEffectParametersPtr params) override;
  base::WeakPtr<AbstractHapticGamepad> GetWeakPtr() override;

 private:
  // ExtenedCounter takes a smaller circular counter
  // and extends it to a larger type.
  // e.g., a one byte counter that goes from 0-127 and then
  // restarts at 0, can be extended to a 4 byte counter
  // where the restarting of smaller type increments the
  // larger type, the second time 0 -> 128, 1 -> 129 etc.
  template <typename ExtendedType = uint32_t, typename BaseType = uint8_t>
  class ExtendedCounter {
   public:
    ExtendedType operator()(BaseType num,
                            ExtendedCounter const* other = nullptr);

    ExtendedCounter() = default;
    ExtendedCounter(const ExtendedCounter&) = delete;
    ExtendedCounter& operator=(const ExtendedCounter&) = delete;

   private:
    static constexpr ExtendedType kLastMax =
        std::numeric_limits<ExtendedType>::max();

    ExtendedType prefix = std::numeric_limits<ExtendedType>::min();
    ExtendedType last = kLastMax;
  };

  // ContinueCircularIndexPair links two ExtendedCounters
  // This is peculiar/particular to the Dualshock4 always
  // sending pairs of touch points. Both must be tracked
  // along with their relationship to each other.
  // e.g.
  // first_base first_extended second_base second_extended
  //        127            127         125             125
  //          0            128         125             125
  //          1            129         125             125
  //          1            129           2             130
  struct ContinueCircularIndexPair {
    ExtendedCounter<uint32_t, uint8_t> first;
    ExtendedCounter<uint32_t, uint8_t> second;

    auto operator()(uint8_t idx1, uint8_t idx2) {
      // order of evaluation is important
      // do not move into the make_tuple call
      auto f = first(idx1, &second);
      auto s = second(idx2, &first);
      return std::make_tuple(f, s);
    }
  };

  // AbstractHapticGamepad private implementation.
  void DoShutdown() override;

  // Sends a vibration output report suitable for a USB-connected Dualshock4.
  void SetVibrationUsb(double strong_magnitude, double weak_magnitude);

  // Sends a vibration output report suitable for a Bluetooth-connected
  // Dualshock4.
  void SetVibrationBluetooth(double strong_magnitude, double weak_magnitude);

  GamepadId gamepad_id_;
  GamepadBusType bus_type_;
  // Used to offset touch ids sent to Gamepad
  std::optional<uint32_t> initial_touch_id_;
  ContinueCircularIndexPair transform_touch_id_;
  std::unique_ptr<HidWriter> writer_;
  base::WeakPtrFactory<Dualshock4Controller> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_DUALSHOCK4_CONTROLLER_H_
