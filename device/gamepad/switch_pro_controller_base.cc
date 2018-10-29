// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/switch_pro_controller_base.h"

#include <limits>

#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_standard_mappings.h"

namespace {
const uint16_t kVendorNintendo = 0x057e;
const uint16_t kProductSwitchProController = 0x2009;

const uint8_t kRumbleMagnitudeMax = 0xff;

// Switch Pro Controller USB packet types.
static const uint8_t kPacketTypeStatus = 0x81;
static const uint8_t kPacketTypeControllerData = 0x30;

// Status packet subtypes.
static const uint8_t kStatusTypeSerial = 0x01;
static const uint8_t kStatusTypeInit = 0x02;

// Axis extents, used for normalization.
static const int8_t kAxisMin = std::numeric_limits<int8_t>::min();
static const int8_t kAxisMax = std::numeric_limits<int8_t>::max();

enum ControllerType { UNKNOWN_CONTROLLER, SWITCH_PRO_CONTROLLER };

#pragma pack(push, 1)
struct ControllerDataReport {
  uint8_t type;  // must be kPacketTypeControllerData
  uint8_t timestamp;
  uint8_t dummy1;
  bool button_y : 1;
  bool button_x : 1;
  bool button_b : 1;
  bool button_a : 1;
  bool dummy2 : 2;
  bool button_r : 1;
  bool button_zr : 1;
  bool button_minus : 1;
  bool button_plus : 1;
  bool button_thumb_r : 1;
  bool button_thumb_l : 1;
  bool button_home : 1;
  bool button_capture : 1;
  bool dummy3 : 2;
  bool dpad_down : 1;
  bool dpad_up : 1;
  bool dpad_right : 1;
  bool dpad_left : 1;
  bool dummy4 : 2;
  bool button_l : 1;
  bool button_zl : 1;
  uint8_t analog[6];
};
#pragma pack(pop)

ControllerType ControllerTypeFromDeviceIds(uint16_t vendor_id,
                                           uint16_t product_id) {
  if (vendor_id == kVendorNintendo) {
    switch (product_id) {
      case kProductSwitchProController:
        return SWITCH_PRO_CONTROLLER;
      default:
        break;
    }
  }
  return UNKNOWN_CONTROLLER;
}

double NormalizeAxis(int value, int min, int max) {
  return (2.0 * (value - min) / static_cast<double>(max - min)) - 1.0;
}

void UpdatePadStateFromControllerData(const ControllerDataReport& report,
                                      device::Gamepad* pad) {
  pad->buttons[device::BUTTON_INDEX_PRIMARY].pressed = report.button_b;
  pad->buttons[device::BUTTON_INDEX_PRIMARY].value =
      report.button_b ? 1.0 : 0.0;

  pad->buttons[device::BUTTON_INDEX_SECONDARY].pressed = report.button_a;
  pad->buttons[device::BUTTON_INDEX_SECONDARY].value =
      report.button_a ? 1.0 : 0.0;

  pad->buttons[device::BUTTON_INDEX_TERTIARY].pressed = report.button_y;
  pad->buttons[device::BUTTON_INDEX_TERTIARY].value =
      report.button_y ? 1.0 : 0.0;

  pad->buttons[device::BUTTON_INDEX_QUATERNARY].pressed = report.button_x;
  pad->buttons[device::BUTTON_INDEX_QUATERNARY].value =
      report.button_x ? 1.0 : 0.0;

  pad->buttons[device::BUTTON_INDEX_LEFT_SHOULDER].pressed = report.button_l;
  pad->buttons[device::BUTTON_INDEX_LEFT_SHOULDER].value =
      report.button_l ? 1.0 : 0.0;

  pad->buttons[device::BUTTON_INDEX_RIGHT_SHOULDER].pressed = report.button_r;
  pad->buttons[device::BUTTON_INDEX_RIGHT_SHOULDER].value =
      report.button_r ? 1.0 : 0.0;

  pad->buttons[device::BUTTON_INDEX_LEFT_TRIGGER].pressed = report.button_zl;
  pad->buttons[device::BUTTON_INDEX_LEFT_TRIGGER].value =
      report.button_zl ? 1.0 : 0.0;

  pad->buttons[device::BUTTON_INDEX_RIGHT_TRIGGER].pressed = report.button_zr;
  pad->buttons[device::BUTTON_INDEX_RIGHT_TRIGGER].value =
      report.button_zr ? 1.0 : 0.0;

  pad->buttons[device::BUTTON_INDEX_BACK_SELECT].pressed = report.button_minus;
  pad->buttons[device::BUTTON_INDEX_BACK_SELECT].value =
      report.button_minus ? 1.0 : 0.0;

  pad->buttons[device::BUTTON_INDEX_START].pressed = report.button_plus;
  pad->buttons[device::BUTTON_INDEX_START].value =
      report.button_plus ? 1.0 : 0.0;

  pad->buttons[device::BUTTON_INDEX_LEFT_THUMBSTICK].pressed =
      report.button_thumb_l;
  pad->buttons[device::BUTTON_INDEX_LEFT_THUMBSTICK].value =
      report.button_thumb_l ? 1.0 : 0.0;

  pad->buttons[device::BUTTON_INDEX_RIGHT_THUMBSTICK].pressed =
      report.button_thumb_r;
  pad->buttons[device::BUTTON_INDEX_RIGHT_THUMBSTICK].value =
      report.button_thumb_r ? 1.0 : 0.0;

  pad->buttons[device::BUTTON_INDEX_DPAD_UP].pressed = report.dpad_up;
  pad->buttons[device::BUTTON_INDEX_DPAD_UP].value = report.dpad_up ? 1.0 : 0.0;

  pad->buttons[device::BUTTON_INDEX_DPAD_DOWN].pressed = report.dpad_down;
  pad->buttons[device::BUTTON_INDEX_DPAD_DOWN].value =
      report.dpad_down ? 1.0 : 0.0;

  pad->buttons[device::BUTTON_INDEX_DPAD_LEFT].pressed = report.dpad_left;
  pad->buttons[device::BUTTON_INDEX_DPAD_LEFT].value =
      report.dpad_left ? 1.0 : 0.0;

  pad->buttons[device::BUTTON_INDEX_DPAD_RIGHT].pressed = report.dpad_right;
  pad->buttons[device::BUTTON_INDEX_DPAD_RIGHT].value =
      report.dpad_right ? 1.0 : 0.0;

  pad->buttons[device::BUTTON_INDEX_META].pressed = report.button_home;
  pad->buttons[device::BUTTON_INDEX_META].value =
      report.button_home ? 1.0 : 0.0;

  pad->buttons[device::BUTTON_INDEX_META + 1].pressed = report.button_capture;
  pad->buttons[device::BUTTON_INDEX_META + 1].value =
      report.button_capture ? 1.0 : 0.0;

  int8_t axis_lx =
      (((report.analog[1] & 0x0F) << 4) | ((report.analog[0] & 0xF0) >> 4)) +
      127;
  int8_t axis_ly = report.analog[2] + 127;
  int8_t axis_rx =
      (((report.analog[4] & 0x0F) << 4) | ((report.analog[3] & 0xF0) >> 4)) +
      127;
  int8_t axis_ry = report.analog[5] + 127;
  pad->axes[device::AXIS_INDEX_LEFT_STICK_X] =
      NormalizeAxis(axis_lx, kAxisMin, kAxisMax);
  pad->axes[device::AXIS_INDEX_LEFT_STICK_Y] =
      NormalizeAxis(-axis_ly, kAxisMin, kAxisMax);
  pad->axes[device::AXIS_INDEX_RIGHT_STICK_X] =
      NormalizeAxis(axis_rx, kAxisMin, kAxisMax);
  pad->axes[device::AXIS_INDEX_RIGHT_STICK_Y] =
      NormalizeAxis(-axis_ry, kAxisMin, kAxisMax);

  pad->buttons_length = device::BUTTON_INDEX_COUNT + 1;
  pad->axes_length = device::AXIS_INDEX_COUNT;
}

}  // namespace

namespace device {

SwitchProControllerBase::~SwitchProControllerBase() = default;

// static
bool SwitchProControllerBase::IsSwitchPro(uint16_t vendor_id,
                                          uint16_t product_id) {
  return ControllerTypeFromDeviceIds(vendor_id, product_id) !=
         UNKNOWN_CONTROLLER;
}

void SwitchProControllerBase::DoShutdown() {
  if (force_usb_hid_)
    SendForceUsbHid(false);
  force_usb_hid_ = false;
}

void SwitchProControllerBase::ReadUsbPadState(Gamepad* pad) {
  DCHECK(pad);

  // Consume reports until the input pipe is empty.
  uint8_t report_bytes[kReportSize];
  while (true) {
    size_t report_length = ReadInputReport(report_bytes);
    if (report_length == 0)
      break;
    HandleInputReport(report_bytes, report_length, pad);
  }
}

void SwitchProControllerBase::HandleInputReport(void* report,
                                                size_t report_length,
                                                Gamepad* pad) {
  DCHECK(report);
  DCHECK_GE(report_length, 1U);
  DCHECK(pad);

  const uint8_t* report_bytes = static_cast<uint8_t*>(report);
  const uint8_t type = report_bytes[0];
  switch (type) {
    case kPacketTypeStatus:
      if (report_length >= 2) {
        const uint8_t status_type = report_bytes[1];
        switch (status_type) {
          case kStatusTypeSerial:
            if (!sent_handshake_) {
              sent_handshake_ = true;
              SendHandshake();
            }
            break;
          case kStatusTypeInit:
            force_usb_hid_ = true;
            SendForceUsbHid(true);
            break;
          default:
            break;
        }
      }
      break;
    case kPacketTypeControllerData: {
      ControllerDataReport* controller_data =
          reinterpret_cast<ControllerDataReport*>(report);
      UpdatePadStateFromControllerData(*controller_data, pad);
      pad->timestamp = GamepadDataFetcher::CurrentTimeInMicroseconds();
      break;
    }
    default:
      break;
  }
}

void SwitchProControllerBase::SendConnectionStatusQuery() {
  // Requests the current connection status and info about the connected
  // controller. The controller will respond with a status packet.
  uint8_t report[kReportSize];
  memset(report, 0, kReportSize);
  report[0] = 0x80;
  report[1] = 0x01;

  WriteOutputReport(report, kReportSize);
}

void SwitchProControllerBase::SendHandshake() {
  // Sends handshaking packets over UART. This command can only be called once
  // per session. The controller will respond with a status packet.
  uint8_t report[kReportSize];
  memset(report, 0, kReportSize);
  report[0] = 0x80;
  report[1] = 0x02;

  WriteOutputReport(report, kReportSize);
}

void SwitchProControllerBase::SendForceUsbHid(bool enable) {
  // By default, the controller will revert to Bluetooth mode if it does not
  // receive any USB HID commands within a timeout window. Enabling the
  // ForceUsbHid mode forces all communication to go through USB HID and
  // disables the timeout.
  uint8_t report[kReportSize];
  memset(report, 0, kReportSize);
  report[0] = 0x80;
  report[1] = (enable ? 0x04 : 0x05);

  WriteOutputReport(report, kReportSize);
}

void SwitchProControllerBase::SetVibration(double strong_magnitude,
                                           double weak_magnitude) {
  uint8_t strong_magnitude_scaled =
      static_cast<uint8_t>(strong_magnitude * kRumbleMagnitudeMax);
  uint8_t weak_magnitude_scaled =
      static_cast<uint8_t>(weak_magnitude * kRumbleMagnitudeMax);

  uint8_t report[kReportSize];
  memset(report, 0, kReportSize);
  report[0] = 0x10;
  report[1] = static_cast<uint8_t>(counter_++ & 0x0F);
  report[2] = 0x80;
  report[6] = 0x80;
  if (strong_magnitude_scaled > 0) {
    report[2] = 0x80;
    report[3] = 0x20;
    report[4] = 0x62;
    report[5] = strong_magnitude_scaled >> 2;
  }
  if (weak_magnitude_scaled > 0) {
    report[6] = 0x98;
    report[7] = 0x20;
    report[8] = 0x62;
    report[9] = weak_magnitude_scaled >> 2;
  }

  WriteOutputReport(report, kReportSize);
}

size_t SwitchProControllerBase::ReadInputReport(void* report) {
  return 0;
}

size_t SwitchProControllerBase::WriteOutputReport(void* report,
                                                  size_t report_length) {
  return 0;
}

}  // namespace device
