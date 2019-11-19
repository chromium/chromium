// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/dualshock4_controller.h"

#include <array>

#include "base/metrics/crc32.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_id_list.h"
#include "device/gamepad/hid_writer.h"

namespace device {

namespace {
// Expected values for |version_number| when connected over USB and Bluetooth.
const uint16_t kDualshock4VersionUsb = 0x0100;
const uint16_t kDualshock4VersionBluetooth = 0;

// Report IDs.
const uint8_t kReportId05 = 0x05;
const uint8_t kReportId11 = 0x11;

// Maximum in-range values for HID report fields.
const uint8_t kRumbleMagnitudeMax = 0xff;
const float kAxisMax = 255.0f;
const float kDpadMax = 7.0f;

#pragma pack(push, 1)
struct ControllerData {
  uint8_t axis_left_x;
  uint8_t axis_left_y;
  uint8_t axis_right_x;
  uint8_t axis_right_y;
  uint8_t axis_dpad : 4;
  bool button_square : 1;
  bool button_cross : 1;
  bool button_circle : 1;
  bool button_triangle : 1;
  bool button_left_1 : 1;
  bool button_right_1 : 1;
  bool button_left_2 : 1;
  bool button_right_2 : 1;
  bool button_share : 1;
  bool button_options : 1;
  bool button_left_3 : 1;
  bool button_right_3 : 1;
  bool button_ps : 1;
  bool button_touch : 1;
  uint8_t sequence_number : 6;
  uint8_t axis_left_2;
  uint8_t axis_right_2;
  uint16_t timestamp;
  uint8_t sensor_temperature;
  uint16_t axis_gyro_pitch;
  uint16_t axis_gyro_yaw;
  uint16_t axis_gyro_roll;
  uint16_t axis_accelerometer_x;
  uint16_t axis_accelerometer_y;
  uint16_t axis_accelerometer_z;
  uint8_t padding1[5];
  uint8_t battery_info : 5;
  uint8_t padding2 : 2;
  bool extension_detection : 1;
};
#pragma pack(pop)
static_assert(sizeof(ControllerData) == 30,
              "ControllerData has incorrect size");

#pragma pack(push, 1)
struct TouchPadData {
  uint8_t touch_data_timestamp;
  uint8_t touch0_id : 7;
  bool touch0_is_invalid : 1;
  uint8_t touch0_data[3];
  uint8_t touch1_id : 7;
  bool touch1_is_invalid : 1;
  uint8_t touch1_data[3];
};
#pragma pack(pop)
static_assert(sizeof(TouchPadData) == 9, "TouchPadData has incorrect size");

#pragma pack(push, 1)
struct Dualshock4InputReport11 {
  uint8_t padding1[2];
  ControllerData controller_data;
  uint8_t padding2[2];
  uint8_t touches_count;
  TouchPadData touches[4];
  uint8_t padding3[2];
  uint32_t crc32;
};
#pragma pack(pop)
static_assert(sizeof(Dualshock4InputReport11) == 77,
              "Dualshock4InputReport11 has incorrect size");

// Returns the CRC32 checksum for a Dualshock4 Bluetooth output report.
// |report_data| is the report data excluding the bytes where the checksum will
// be written.
uint32_t ComputeDualshock4Checksum(base::span<const uint8_t> report_data) {
  // The Bluetooth report checksum includes a constant header byte not contained
  // in the report data.
  constexpr uint8_t bt_header = 0xa2;
  uint32_t crc = base::Crc32(0xffffffff, &bt_header, 1);
  // Extend the checksum with the contents of the report.
  return ~base::Crc32(crc, report_data.data(), report_data.size_bytes());
}

// Scales |value| with range [0,255] to a float within [-1.0,+1.0].
static float NormalizeAxis(uint8_t value) {
  return (2.0f * value / kAxisMax) - 1.0f;
}

// Scales a D-pad value from a Dualshock4 report to an axis value. The D-pad
// report value has a logical range from 0 to 7, and uses an out-of-bounds value
// of 8 to indicate null input (no interaction). In-bounds values are scaled to
// the range [-1.0,+1.0]. The null input value returns +9/7 (about 1.29).
static float NormalizeDpad(uint8_t value) {
  return (2.0f * value / kDpadMax) - 1.0f;
}

}  // namespace

Dualshock4Controller::Dualshock4Controller(uint16_t vendor_id,
                                           uint16_t product_id,
                                           GamepadBusType bus_type,
                                           std::unique_ptr<HidWriter> writer)
    : vendor_id_(vendor_id),
      product_id_(product_id),
      bus_type_(bus_type),
      writer_(std::move(writer)) {}

Dualshock4Controller::~Dualshock4Controller() = default;

// static
bool Dualshock4Controller::IsDualshock4(uint16_t vendor_id,
                                        uint16_t product_id) {
  auto gamepad_id = GamepadIdList::Get().GetGamepadId(vendor_id, product_id);
  return gamepad_id == GamepadId::kSonyProduct05c4 ||
         gamepad_id == GamepadId::kSonyProduct09cc ||
         gamepad_id == GamepadId::kVendor2e95Product7725;
}

// static
GamepadBusType Dualshock4Controller::BusTypeFromVersionNumber(
    uint32_t version_number) {
  // RawInputDataFetcher on Windows does not distinguish devices by bus type.
  // Detect the transport in use by inspecting the version number reported by
  // the device.
  if (version_number == kDualshock4VersionUsb)
    return GAMEPAD_BUS_USB;
  if (version_number == kDualshock4VersionBluetooth)
    return GAMEPAD_BUS_BLUETOOTH;
  return GAMEPAD_BUS_UNKNOWN;
}

void Dualshock4Controller::DoShutdown() {
  writer_.reset();
}

bool Dualshock4Controller::ProcessInputReport(uint8_t report_id,
                                              base::span<const uint8_t> report,
                                              Gamepad* pad) {
  DCHECK(pad);

  // Input report 0x11 is the full-feature mode input report. It includes
  // gamepad button and axis state, touch inputs, motion inputs, battery level
  // and temperature. Dualshock4 starts sending this report after it has
  // received an output report with ID 0x11. Prior to receiving the output
  // report, input report 0x01 is sent which includes button and axis state.
  //
  // Here we only handle the full-feature report. Input report 0x01 is handled
  // by the platform's HID data fetcher.
  if (report_id != kReportId11 ||
      report.size_bytes() < sizeof(Dualshock4InputReport11)) {
    return false;
  }

  const auto* data =
      reinterpret_cast<const Dualshock4InputReport11*>(report.data());

  // Button and axis indices must match the ordering expected by the Dualshock4
  // mapping function.
  pad->axes[0] = NormalizeAxis(data->controller_data.axis_left_x);
  pad->axes[1] = NormalizeAxis(data->controller_data.axis_left_y);
  pad->axes[2] = NormalizeAxis(data->controller_data.axis_right_x);
  pad->axes[3] = NormalizeAxis(data->controller_data.axis_left_2);
  pad->axes[4] = NormalizeAxis(data->controller_data.axis_right_2);
  pad->axes[5] = NormalizeAxis(data->controller_data.axis_right_y);
  pad->axes[9] = NormalizeDpad(data->controller_data.axis_dpad);
  const bool button_values[] = {
      data->controller_data.button_square,
      data->controller_data.button_cross,
      data->controller_data.button_circle,
      data->controller_data.button_triangle,
      data->controller_data.button_left_1,
      data->controller_data.button_right_1,
      data->controller_data.button_left_2,
      data->controller_data.button_right_2,
      data->controller_data.button_share,
      data->controller_data.button_options,
      data->controller_data.button_left_3,
      data->controller_data.button_right_3,
      data->controller_data.button_ps,
      data->controller_data.button_touch,
  };
  for (size_t i = 0; i < base::size(button_values); ++i) {
    pad->buttons[i].pressed = button_values[i];
    pad->buttons[i].touched = button_values[i];
    pad->buttons[i].value = button_values[i] ? 1.0 : 0.0;
  }
  pad->timestamp = GamepadDataFetcher::CurrentTimeInMicroseconds();
  return true;
}

void Dualshock4Controller::SetVibration(double strong_magnitude,
                                        double weak_magnitude) {
  // Genuine DualShock 4 gamepads use an alternate output report when connected
  // over Bluetooth. Always send USB-mode reports to SCUF Vantage gamepads.
  if (bus_type_ == GAMEPAD_BUS_BLUETOOTH &&
      GamepadIdList::Get().GetGamepadId(vendor_id_, product_id_) !=
          GamepadId::kVendor2e95Product7725) {
    SetVibrationBluetooth(strong_magnitude, weak_magnitude);
    return;
  }

  SetVibrationUsb(strong_magnitude, weak_magnitude);
}

void Dualshock4Controller::SetVibrationUsb(double strong_magnitude,
                                           double weak_magnitude) {
  DCHECK(writer_);
  // Construct a USB output report with report ID 0x05. In USB mode, the 0x05
  // report is used to control vibration, LEDs, and audio volume.
  // https://github.com/torvalds/linux/blob/master/drivers/hid/hid-sony.c#L2105
  std::array<uint8_t, 32> control_report;
  control_report.fill(0);
  control_report[0] = kReportId05;
  control_report[1] = 0x01;  // motor only, don't update LEDs
  control_report[4] = uint8_t{weak_magnitude * kRumbleMagnitudeMax};
  control_report[5] = uint8_t{strong_magnitude * kRumbleMagnitudeMax};

  writer_->WriteOutputReport(control_report);
}

void Dualshock4Controller::SetVibrationBluetooth(double strong_magnitude,
                                                 double weak_magnitude) {
  DCHECK(writer_);
  // Construct a Bluetooth output report with report ID 0x11. In Bluetooth mode,
  // the 0x11 report is used to control vibration, LEDs, and audio volume.
  // https://www.psdevwiki.com/ps4/DS4-BT#0x11_2
  std::array<uint8_t, 78> control_report;
  control_report.fill(0);
  control_report[0] = kReportId11;
  control_report[1] = 0xc0;  // unknown
  control_report[2] = 0x20;  // unknown
  control_report[3] = 0xf1;  // motor only, don't update LEDs
  control_report[4] = 0x04;  // unknown
  control_report[6] = uint8_t{weak_magnitude * kRumbleMagnitudeMax};
  control_report[7] = uint8_t{strong_magnitude * kRumbleMagnitudeMax};
  control_report[21] = 0x43;  // volume left
  control_report[22] = 0x43;  // volume right
  control_report[24] = 0x4d;  // volume speaker
  control_report[25] = 0x85;  // unknown

  // The last four bytes of the report hold a CRC32 checksum. Compute the
  // checksum and store it in little-endian byte order.
  uint32_t crc = ComputeDualshock4Checksum(
      base::make_span(control_report.data(), control_report.size() - 4));
  control_report[control_report.size() - 4] = crc & 0xff;
  control_report[control_report.size() - 3] = (crc >> 8) & 0xff;
  control_report[control_report.size() - 2] = (crc >> 16) & 0xff;
  control_report[control_report.size() - 1] = (crc >> 24) & 0xff;

  writer_->WriteOutputReport(control_report);
}

base::WeakPtr<AbstractHapticGamepad> Dualshock4Controller::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
