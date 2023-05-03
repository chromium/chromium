// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_TEST_REPORT_DESCRIPTORS_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_TEST_REPORT_DESCRIPTORS_H_

#include <stdint.h>

#include "base/containers/span.h"

namespace device {

class TestReportDescriptors {
 public:
  // Descriptors from the HID descriptor tool.
  // http://www.usb.org/developers/hidpage/dt2_4.zip
  static base::span<const uint8_t> Digitizer();
  static base::span<const uint8_t> Keyboard();
  static base::span<const uint8_t> Monitor();
  static base::span<const uint8_t> Mouse();

  // The report descriptor from a Logitech Unifying receiver.
  static base::span<const uint8_t> LogitechUnifyingReceiver();

  // The report descriptor from a Sony Dualshock 3 connected over USB.
  static base::span<const uint8_t> SonyDualshock3Usb();

  // The report descriptor from a Sony Dualshock 4 connected over USB.
  static base::span<const uint8_t> SonyDualshock4Usb();

  // The report descriptor from a Microsoft Xbox Wireless Controller connected
  // over Bluetooth.
  static base::span<const uint8_t> MicrosoftXboxWirelessControllerBluetooth();

  // The report descriptor from a Nintendo Switch Pro Controller connected over
  // USB.
  static base::span<const uint8_t> NintendoSwitchProControllerUsb();

  // The report descriptor from a Microsoft Xbox Adaptive Controller connected
  // over Bluetooth.
  static base::span<const uint8_t> MicrosoftXboxAdaptiveControllerBluetooth();

  // The report descriptor from a Nexus Player Controller.
  static base::span<const uint8_t> NexusPlayerController();

  // The report descriptors from a Steam Controller. Steam Controller exposes
  // three HID interfaces.
  static base::span<const uint8_t> SteamControllerKeyboard();
  static base::span<const uint8_t> SteamControllerMouse();
  static base::span<const uint8_t> SteamControllerVendor();

  // The report descriptor from an XSkills Gamecube USB controller adapter.
  static base::span<const uint8_t> XSkillsUsbAdapter();

  // The report descriptors from a Belkin Nostromo SpeedPad. The Nostromo
  // SpeedPad exposes two HID interfaces.
  static base::span<const uint8_t> BelkinNostromoKeyboard();
  static base::span<const uint8_t> BelkinNostromoMouseAndExtra();

  // The report descriptor from a Jabra Link 380c USB-C receiver.
  static base::span<const uint8_t> JabraLink380c();

  // The report descriptor for a FIDO U2F HID device.
  // https://fidoalliance.org/specs/fido-u2f-v1.0-ps-20141009/fido-u2f-hid-protocol-ps-20141009.html
  static base::span<const uint8_t> FidoU2fHid();

  // The report descriptor for an RFIDeas pcProx badge reader.
  static base::span<const uint8_t> RfideasPcproxBadgeReader();
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_TEST_REPORT_DESCRIPTORS_H_
