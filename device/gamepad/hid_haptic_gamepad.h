// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_HID_HAPTIC_GAMEPAD_H_
#define DEVICE_GAMEPAD_HID_HAPTIC_GAMEPAD_H_

#include <stddef.h>
#include <stdint.h>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "device/gamepad/abstract_haptic_gamepad.h"
#include "device/gamepad/gamepad_export.h"

namespace device {

class HidWriter;

class DEVICE_GAMEPAD_EXPORT HidHapticGamepad final
    : public AbstractHapticGamepad {
 public:
  // Devices that support HID haptic effects with a simple output report can
  // be supported through HidHapticGamepad by adding a new HapticReportData
  // item to kHapticReportData.
  //
  // Example:
  //   { 0x1234, 0xabcd, 0x42, 5, 1, 3, 2 * 8, 0, 0xffff }
  // This item recognizes a device 1234:abcd that accepts a 5-byte output report
  // containing report ID 0x42 at byte 0 and two 16-bit vibration magnitudes at
  // bytes 1:2 and 3:4.
  //    SetVibration(1.0, 0.5)
  // This call describes a vibration effect with 100% intensity on the strong
  // actuator and 50% intensity on the weak actuator. For the above device, it
  // will scale each magnitude to the [0,0xffff] range when constructing the
  // output report. The first value is scaled to 0xffff and the second is scaled
  // to 0x7fff.
  //    uint8_t[] {0x42, 0xff, 0xff, 0xff, 0x7f}
  // The above bytes represent the output report data sent to the device. The
  // report ID is written to byte 0, the strong magnitude 0xffff is written to
  // bytes 1 and 2 in LE-order, and the weak magnitude is written to bytes 3 and
  // 4 in LE-order.
  struct HapticReportData {
    // Supported HID gamepads are identified by their vendor and product IDs.
    const uint16_t vendor_id;
    const uint16_t product_id;
    // The 8-bit report ID value to send at the start of the output report.
    // Set to zero if the device does not use report IDs.
    const uint8_t report_id;
    // The total length of the output report in bytes, including the report ID.
    const size_t report_length_bytes;
    // The byte offsets of the strong and weak vibration magnitude fields within
    // the output report. If |strong_offset_bytes| == |weak_offset_bytes| then
    // the device is considered a single-channel haptic actuator. Magnitudes
    // from the strong and weak channels will be combined into a single value.
    const size_t strong_offset_bytes;
    const size_t weak_offset_bytes;
    // The width of a vibration magnitude field within the output report. Both
    // strong and weak magnitude fields must have the same size. The report size
    // must be a multiple of 8. Report fields with size greater than one byte
    // will be stored in little-endian byte order.
    const size_t report_size_bits;
    // The logical extents of the vibration magnitude field, used for scaling
    // the vibration value. If |logical_min| == 0 and |logical_max| == 1 then
    // the device is considered an "on-off" actuator and any non-zero vibration
    // magnitude will cause a vibration effect with maximum intensity.
    const uint32_t logical_min;
    const uint32_t logical_max;
  };

  HidHapticGamepad(const HapticReportData& data,
                   std::unique_ptr<HidWriter> writer);

  ~HidHapticGamepad() override;

  static std::unique_ptr<HidHapticGamepad> Create(
      uint16_t vendor_id,
      uint16_t product_id,
      std::unique_ptr<HidWriter> writer);

  // Return true if the device IDs match an item in kHapticReportData.
  static bool IsHidHaptic(uint16_t vendor_id, uint16_t product_id);

  // Return the HapticReportData for the device with matching device IDs.
  static const HidHapticGamepad::HapticReportData* GetHapticReportData(
      uint16_t vendor_id,
      uint16_t product_id);

  // AbstractHapticGamepad public implementation.
  void SetVibration(double strong_magnitude, double weak_magnitude) override;
  base::WeakPtr<AbstractHapticGamepad> GetWeakPtr() override;

 private:
  // AbstractHapticGamepad private implementation.
  void DoShutdown() override;

  // Report ID of the report to use for vibration commands, or zero if report
  // IDs are not used.
  uint8_t report_id_;

  // The total size of the report, including the report ID (if any).
  size_t report_length_bytes_;

  // Byte offsets for the strong and weak magnitude values within the report.
  size_t strong_offset_bytes_;
  size_t weak_offset_bytes_;

  // Width of the vibration magnitude values, in bits.
  size_t report_size_bits_;

  // Logical bounds of the vibration magnitude. Assumed to be positive.
  uint32_t logical_min_;
  uint32_t logical_max_;

  std::unique_ptr<HidWriter> writer_;

  base::WeakPtrFactory<HidHapticGamepad> weak_factory_{this};
};

extern HidHapticGamepad::HapticReportData kHapticReportData[];
extern size_t kHapticReportDataLength;

}  // namespace device

#endif  // DEVICE_GAMEPAD_HID_HAPTIC_GAMEPAD_H_
