// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/test/test_report_descriptors.h"

namespace device {

// static
base::span<const uint8_t> TestReportDescriptors::Digitizer() {
  // Digitizer descriptor from HID descriptor tool
  // http://www.usb.org/developers/hidpage/dt2_4.zip
  constexpr uint8_t kDigitizer[] = {
      0x05, 0x0d,        // Usage Page (Digitizer)
      0x09, 0x01,        // Usage (0x1)
      0xa1, 0x01,        // Collection (Application)
      0x85, 0x01,        //  Report ID (0x1)
      0x09, 0x21,        //  Usage (0x21)
      0xa1, 0x00,        //  Collection (Physical)
      0x05, 0x01,        //   Usage Page (Generic Desktop)
      0x09, 0x30,        //   Usage (0x30)
      0x09, 0x31,        //   Usage (0x31)
      0x75, 0x10,        //   Report Size (16)
      0x95, 0x02,        //   Report Count (2)
      0x15, 0x00,        //   Logical Minimum (0)
      0x26, 0xe0, 0x2e,  //   Logical Maximum (12000)
      0x35, 0x00,        //   Physical Minimum (0)
      0x45, 0x0c,        //   Physical Maximum (12)
      0x65, 0x13,        //   Unit (Inch)
      0x55, 0x00,        //   Unit Exponent (0)
      0xa4,              //   Push
      0x81, 0x02,        //   Input (Dat|Var|Abs|NoWrp|Lin|Prf|NoNull|BitF)
      0x05, 0x0d,        //   Usage Page (Digitizer)
      0x09, 0x32,        //   Usage (0x32)
      0x09, 0x44,        //   Usage (0x44)
      0x09, 0x42,        //   Usage (0x42)
      0x15, 0x00,        //   Logical Minimum (0)
      0x25, 0x01,        //   Logical Maximum (1)
      0x35, 0x00,        //   Physical Minimum (0)
      0x45, 0x01,        //   Physical Maximum (1)
      0x75, 0x01,        //   Report Size (1)
      0x95, 0x03,        //   Report Count (3)
      0x65, 0x00,        //   Unit (0)
      0x81, 0x02,        //   Input (Dat|Var|Abs|NoWrp|Lin|Prf|NoNull|BitF)
      0x95, 0x01,        //   Report Count (1)
      0x75, 0x05,        //   Report Size (5)
      0x81, 0x03,        //   Input (Con|Var|Abs|NoWrp|Lin|Prf|NoNull|BitF)
      0xc0,              //  End Collection
      0x85, 0x02,        //  Report ID (0x2)
      0x09, 0x20,        //  Usage (0x20)
      0xa1, 0x00,        //  Collection (Physical)
      0xb4,              //   Pop
      0xa4,              //   Push
      0x09, 0x30,        //   Usage (0x30)
      0x09, 0x31,        //   Usage (0x31)
      0x81, 0x02,        //   Input (Dat|Var|Abs|NoWrp|Lin|Prf|NoNull|BitF)
      0x05, 0x0d,        //   Usage Page (Digitizer)
      0x09, 0x32,        //   Usage (0x32)
      0x15, 0x00,        //   Logical Minimum (0)
      0x25, 0x01,        //   Logical Maximum (1)
      0x35, 0x00,        //   Physical Minimum (0)
      0x45, 0x01,        //   Physical Maximum (1)
      0x65, 0x00,        //   Unit (0)
      0x75, 0x01,        //   Report Size (1)
      0x81, 0x02,        //   Input (Dat|Var|Abs|NoWrp|Lin|Prf|NoNull|BitF)
      0x05, 0x09,        //   Usage Page (Button)
      0x19, 0x00,        //   Usage Minimum (0)
      0x29, 0x10,        //   Usage Maximum (16)
      0x25, 0x10,        //   Logical Maximum (16)
      0x75, 0x05,        //   Report Size (5)
      0x81, 0x40,        //   Input (Dat|Arr|Abs|NoWrp|Lin|Prf|Null|BitF)
      0x75, 0x02,        //   Report Size (2)
      0x81, 0x01,        //   Input (Con|Arr|Abs|NoWrp|Lin|Prf|NoNull|BitF)
      0xc0,              //  End Collection
      0x85, 0x03,        //  Report ID (0x3)
      0x05, 0x0d,        //  Usage Page (Digitizer)
      0x09, 0x20,        //  Usage (0x20)
      0xa1, 0x00,        //  Collection (Physical)
      0xb4,              //   Pop
      0x09, 0x30,        //   Usage (0x30)
      0x09, 0x31,        //   Usage (0x31)
      0x81, 0x02,        //   Input (Dat|Var|Abs|NoWrp|Lin|Prf|NoNull|BitF)
      0x05, 0x0d,        //   Usage Page (Digitizer)
      0x09, 0x32,        //   Usage (0x32)
      0x09, 0x44,        //   Usage (0x44)
      0x75, 0x01,        //   Report Size (1)
      0x15, 0x00,        //   Logical Minimum (0)
      0x25, 0x01,        //   Logical Maximum (1)
      0x35, 0x00,        //   Physical Minimum (0)
      0x45, 0x01,        //   Physical Maximum (1)
      0x65, 0x00,        //   Unit (0)
      0x81, 0x02,        //   Input (Dat|Var|Abs|NoWrp|Lin|Prf|NoNull|BitF)
      0x95, 0x06,        //   Report Count (6)
      0x81, 0x03,        //   Input (Con|Var|Abs|NoWrp|Lin|Prf|NoNull|BitF)
      0x09, 0x30,        //   Usage (0x30)
      0x15, 0x00,        //   Logical Minimum (0)
      0x25, 0x7f,        //   Logical Maximum (127)
      0x35, 0x00,        //   Physical Minimum (0)
      0x45, 0x2d,        //   Physical Maximum (45)
      0x67, 0x11, 0xe1,  //   Unit (Newtons)
      0x00, 0x00,        //   Default
      0x55, 0x04,        //   Unit Exponent (4)
      0x75, 0x08,        //   Report Size (8)
      0x95, 0x01,        //   Report Count (1)
      0x81, 0x12,        //   Input (Dat|Var|Abs|NoWrp|NoLin|Prf|NoNull|BitF)
      0xc0,              //  End Collection
      0xc0               // End Collection
  };
  return base::make_span(kDigitizer);
}

// static
base::span<const uint8_t> TestReportDescriptors::Keyboard() {
  // Keyboard descriptor from HID descriptor tool
  // http://www.usb.org/developers/hidpage/dt2_4.zip
  constexpr uint8_t kKeyboard[] = {
      0x05, 0x01,  // Usage Page (Generic Desktop)
      0x09, 0x06,  // Usage (0x6)
      0xa1, 0x01,  // Collection (Application)
      0x05, 0x07,  //  Usage Page (Keyboard)
      0x19, 0xe0,  //  Usage Minimum (224)
      0x29, 0xe7,  //  Usage Maximum (231)
      0x15, 0x00,  //  Logical Minimum (0)
      0x25, 0x01,  //  Logical Maximum (1)
      0x75, 0x01,  //  Report Size (1)
      0x95, 0x08,  //  Report Count (8)
      0x81, 0x02,  //  Input (Dat|Var|Abs|NoWrp|Lin|Prf|NoNull|BitF)
      0x95, 0x01,  //  Report Count (1)
      0x75, 0x08,  //  Report Size (8)
      0x81, 0x03,  //  Input (Con|Var|Abs|NoWrp|Lin|Prf|NoNull|BitF)
      0x95, 0x05,  //  Report Count (5)
      0x75, 0x01,  //  Report Size (1)
      0x05, 0x08,  //  Usage Page (Led)
      0x19, 0x01,  //  Usage Minimum (1)
      0x29, 0x05,  //  Usage Maximum (5)
      0x91, 0x02,  //  Output (Dat|Var|Abs|NoWrp|Lin|Prf|NoNull|BitF)
      0x95, 0x01,  //  Report Count (1)
      0x75, 0x03,  //  Report Size (3)
      0x91, 0x03,  //  Output (Con|Var|Abs|NoWrp|Lin|Prf|NoNull|BitF)
      0x95, 0x06,  //  Report Count (6)
      0x75, 0x08,  //  Report Size (8)
      0x15, 0x00,  //  Logical Minimum (0)
      0x25, 0x65,  //  Logical Maximum (101)
      0x05, 0x07,  //  Usage Page (Keyboard)
      0x19, 0x00,  //  Usage Minimum (0)
      0x29, 0x65,  //  Usage Maximum (101)
      0x81, 0x00,  //  Input (Dat|Arr|Abs|NoWrp|Lin|Prf|NoNull|BitF)
      0xc0         // End Collection
  };
  return base::make_span(kKeyboard);
}

// static
base::span<const uint8_t> TestReportDescriptors::Monitor() {
  // Monitor descriptor from HID descriptor tool
  // http://www.usb.org/developers/hidpage/dt2_4.zip
  constexpr uint8_t kMonitor[] = {
      0x05, 0x80,        // Usage Page (Monitor 0)
      0x09, 0x01,        // Usage (0x1)
      0xa1, 0x01,        // Collection (Application)
      0x85, 0x01,        //  Report ID (0x1)
      0x15, 0x00,        //  Logical Minimum (0)
      0x26, 0xff, 0x00,  //  Logical Maximum (255)
      0x75, 0x08,        //  Report Size (8)
      0x95, 0x80,        //  Report Count (128)
      0x09, 0x02,        //  Usage (0x2)
      0xb2, 0x02, 0x01,  //  Feature (Dat|Var|Abs|NoWrp|Lin|Prf|NoNull|Buff)
      0x85, 0x02,        //  Report ID (0x2)
      0x95, 0xf3,        //  Report Count (243)
      0x09, 0x03,        //  Usage (0x3)
      0xb2, 0x02, 0x01,  //  Feature (Dat|Var|Abs|NoWrp|Lin|Prf|NoNull|Buff)
      0x85, 0x03,        //  Report ID (0x3)
      0x05, 0x82,        //  Usage Page (Monitor 2)
      0x67, 0xE1, 0x00,  //  Unit (System: SI, Lum. Intensity: Candela)
      0x00, 0x01,        //  ... continuation
      0x55, 0x0E,        //  Unit Exponent (-2)
      0x95, 0x01,        //  Report Count (1)
      0x75, 0x10,        //  Report Size (16)
      0x26, 0xc8, 0x00,  //  Logical Maximum (200)
      0x09, 0x10,        //  Usage (0x10)
      0xb1, 0x02,        //  Feature (Dat|Var|Abs|NoWrp|Lin|Prf|NoNull|BitF)
      0x85, 0x04,        //  Report ID (0x4)
      0x25, 0x64,        //  Logical Maximum (100)
      0x09, 0x12,        //  Usage (0x12)
      0xb1, 0x02,        //  Feature (Dat|Var|Abs|NoWrp|Lin|Prf|NoNull|BitF)
      0x95, 0x06,        //  Report Count (6)
      0x26, 0xff, 0x00,  //  Logical Maximum (255)
      0x09, 0x16,        //  Usage (0x16)
      0x09, 0x18,        //  Usage (0x18)
      0x09, 0x1a,        //  Usage (0x1A)
      0x09, 0x6c,        //  Usage (0x6C)
      0x09, 0x6e,        //  Usage (0x6E)
      0x09, 0x70,        //  Usage (0x70)
      0xb1, 0x02,        //  Feature (Dat|Var|Abs|NoWrp|Lin|Prf|NoNull|BitF)
      0x85, 0x05,        //  Report ID (0x5)
      0x25, 0x7f,        //  Logical Maximum (127)
      0x09, 0x20,        //  Usage (0x20)
      0x09, 0x22,        //  Usage (0x22)
      0x09, 0x30,        //  Usage (0x30)
      0x09, 0x32,        //  Usage (0x32)
      0x09, 0x42,        //  Usage (0x42)
      0x09, 0x44,        //  Usage (0x44)
      0xb1, 0x02,        //  Feature (Dat|Var|Abs|NoWrp|Lin|Prf|NoNull|BitF)
      0xc0               // End Collection
  };
  return base::make_span(kMonitor);
}

// static
base::span<const uint8_t> TestReportDescriptors::Mouse() {
  // Mouse descriptor from HID descriptor tool
  // http://www.usb.org/developers/hidpage/dt2_4.zip
  constexpr uint8_t kMouse[] = {
      0x05, 0x01,  // Usage Page (Generic Desktop)
      0x09, 0x02,  // Usage (0x2)
      0xa1, 0x01,  // Collection (Application)
      0x09, 0x01,  //  Usage (0x1)
      0xa1, 0x00,  //  Collection (Physical)
      0x05, 0x09,  //   Usage Page (Button)
      0x19, 0x01,  //   Usage Minimum (1)
      0x29, 0x03,  //   Usage Maximum (3)
      0x15, 0x00,  //   Logical Minimum (0)
      0x25, 0x01,  //   Logical Maximum (1)
      0x95, 0x03,  //   Report Count (3)
      0x75, 0x01,  //   Report Size (1)
      0x81, 0x02,  //   Input (Dat|Var|Abs|NoWrp|Lin|Prf|NoNull|BitF)
      0x95, 0x01,  //   Report Count (1)
      0x75, 0x05,  //   Report Size (5)
      0x81, 0x03,  //   Input (Con|Var|Abs|NoWrp|Lin|Prf|NoNull|BitF)
      0x05, 0x01,  //   Usage Page (Generic Desktop)
      0x09, 0x30,  //   Usage (0x30)
      0x09, 0x31,  //   Usage (0x31)
      0x15, 0x81,  //   Logical Minimum (129)
      0x25, 0x7f,  //   Logical Maximum (127)
      0x75, 0x08,  //   Report Size (8)
      0x95, 0x02,  //   Report Count (2)
      0x81, 0x06,  //   Input (Dat|Var|Rel|NoWrp|Lin|Prf|NoNull|BitF)
      0xc0,        //  End Collection
      0xc0         // End Collection
  };
  return base::make_span(kMouse);
}

// static
base::span<const uint8_t> TestReportDescriptors::LogitechUnifyingReceiver() {
  // Logitech Unifying receiver descriptor
  constexpr uint8_t kLogitechUnifyingReceiver[] = {
      0x06, 0x00, 0xFF,  // Usage Page (Vendor)
      0x09, 0x01,        // Usage (0x1)
      0xA1, 0x01,        // Collection (Application)
      0x85, 0x10,        //  Report ID (0x10)
      0x75, 0x08,        //  Report Size (8)
      0x95, 0x06,        //  Report Count (6)
      0x15, 0x00,        //  Logical Minimum (0)
      0x26, 0xFF, 0x00,  //  Logical Maximum (255)
      0x09, 0x01,        //  Usage (0x1)
      0x81, 0x00,        //  Input (Dat|Arr|Abs|NoWrp|Lin|Prf|NoNull|BitF)
      0x09, 0x01,        //  Usage (0x1)
      0x91, 0x00,        //  Output (Dat|Arr|Abs|NoWrp|Lin|Prf|NoNull|BitF)
      0xC0,              // End Collection
      0x06, 0x00, 0xFF,  // Usage Page (Vendor)
      0x09, 0x02,        // Usage (0x2)
      0xA1, 0x01,        // Collection (Application)
      0x85, 0x11,        //  Report ID (0x11)
      0x75, 0x08,        //  Report Size (8)
      0x95, 0x13,        //  Report Count (19)
      0x15, 0x00,        //  Logical Minimum (0)
      0x26, 0xFF, 0x00,  //  Logical Maximum (255)
      0x09, 0x02,        //  Usage (0x2)
      0x81, 0x00,        //  Input (Dat|Arr|Abs|NoWrp|Lin|Prf|NoNull|BitF)
      0x09, 0x02,        //  Usage (0x2)
      0x91, 0x00,        //  Output (Dat|Arr|Abs|NoWrp|Lin|Prf|NoNull|BitF)
      0xC0,              // End Collection
      0x06, 0x00, 0xFF,  // Usage Page (Vendor)
      0x09, 0x04,        // Usage (0x4)
      0xA1, 0x01,        // Collection (Application)
      0x85, 0x20,        //  Report ID (0x20)
      0x75, 0x08,        //  Report Size (8)
      0x95, 0x0E,        //  Report Count (14)
      0x15, 0x00,        //  Logical Minimum (0)
      0x26, 0xFF, 0x00,  //  Logical Maximum (255)
      0x09, 0x41,        //  Usage (0x41)
      0x81, 0x00,        //  Input (Dat|Arr|Abs|NoWrp|Lin|Prf|NoNull|BitF)
      0x09, 0x41,        //  Usage (0x41)
      0x91, 0x00,        //  Output (Dat|Arr|Abs|NoWrp|Lin|Prf|NoNull|BitF)
      0x85, 0x21,        //  Report ID (0x21)
      0x95, 0x1F,        //  Report Count (31)
      0x15, 0x00,        //  Logical Minimum (0)
      0x26, 0xFF, 0x00,  //  Logical Maximum (255)
      0x09, 0x42,        //  Usage (0x42)
      0x81, 0x00,        //  Input (Dat|Arr|Abs|NoWrp|Lin|Prf|NoNull|BitF)
      0x09, 0x42,        //  Usage (0x42)
      0x91, 0x00,        //  Output (Dat|Arr|Abs|NoWrp|Lin|Prf|NoNull|BitF)
      0xC0               // End Collection
  };
  return base::make_span(kLogitechUnifyingReceiver);
}

// static
base::span<const uint8_t> TestReportDescriptors::SonyDualshock3Usb() {
  // http://eleccelerator.com/wiki/index.php?title=DualShock_4#HID_Report_Descriptor
  constexpr uint8_t kSonyDualshock3[] = {
      0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
      0x09, 0x04,        // Usage (Joystick)
      0xA1, 0x01,        // Collection (Application)
      0xA1, 0x02,        //   Collection (Logical)
      0x85, 0x01,        //     Report ID (1)
      0x75, 0x08,        //     Report Size (8)
      0x95, 0x01,        //     Report Count (1)
      0x15, 0x00,        //     Logical Minimum (0)
      0x26, 0xFF, 0x00,  //     Logical Maximum (255)
      0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred
                         //     State,No Null Position) NOTE: reserved byte
      0x75, 0x01,        //     Report Size (1)
      0x95, 0x13,        //     Report Count (19)
      0x15, 0x00,        //     Logical Minimum (0)
      0x25, 0x01,        //     Logical Maximum (1)
      0x35, 0x00,        //     Physical Minimum (0)
      0x45, 0x01,        //     Physical Maximum (1)
      0x05, 0x09,        //     Usage Page (Button)
      0x19, 0x01,        //     Usage Minimum (0x01)
      0x29, 0x13,        //     Usage Maximum (0x13)
      0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred
                         //     State,No Null Position)
      0x75, 0x01,        //     Report Size (1)
      0x95, 0x0D,        //     Report Count (13)
      0x06, 0x00, 0xFF,  //     Usage Page (Vendor Defined 0xFF00)
      0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred
                         //     State,No Null Position) NOTE: 32 bit integer,
                         //     where 0:18 are buttons and 19:31 are reserved
      0x15, 0x00,        //     Logical Minimum (0)
      0x26, 0xFF, 0x00,  //     Logical Maximum (255)
      0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
      0x09, 0x01,        //     Usage (Pointer)
      0xA1, 0x00,        //     Collection (Physical)
      0x75, 0x08,        //       Report Size (8)
      0x95, 0x04,        //       Report Count (4)
      0x35, 0x00,        //       Physical Minimum (0)
      0x46, 0xFF, 0x00,  //       Physical Maximum (255)
      0x09, 0x30,        //       Usage (X)
      0x09, 0x31,        //       Usage (Y)
      0x09, 0x32,        //       Usage (Z)
      0x09, 0x35,        //       Usage (Rz)
      0x81, 0x02,        //       Input (Data,Var,Abs,No Wrap,Linear,Preferred
                         //       State,No Null Position) NOTE: four joysticks
      0xC0,              //     End Collection
      0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
      0x75, 0x08,        //     Report Size (8)
      0x95, 0x27,        //     Report Count (39)
      0x09, 0x01,        //     Usage (Pointer)
      0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred
                         //     State,No Null Position)
      0x75, 0x08,        //     Report Size (8)
      0x95, 0x30,        //     Report Count (48)
      0x09, 0x01,        //     Usage (Pointer)
      0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred
                         //     State,No Null Position,Non-volatile)
      0x75, 0x08,        //     Report Size (8)
      0x95, 0x30,        //     Report Count (48)
      0x09, 0x01,        //     Usage (Pointer)
      0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //     State,No Null Position,Non-volatile)
      0xC0,              //   End Collection
      0xA1, 0x02,        //   Collection (Logical)
      0x85, 0x02,        //     Report ID (2)
      0x75, 0x08,        //     Report Size (8)
      0x95, 0x30,        //     Report Count (48)
      0x09, 0x01,        //     Usage (Pointer)
      0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //     State,No Null Position,Non-volatile)
      0xC0,              //   End Collection
      0xA1, 0x02,        //   Collection (Logical)
      0x85, 0xEE,        //     Report ID (238)
      0x75, 0x08,        //     Report Size (8)
      0x95, 0x30,        //     Report Count (48)
      0x09, 0x01,        //     Usage (Pointer)
      0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //     State,No Null Position,Non-volatile)
      0xC0,              //   End Collection
      0xA1, 0x02,        //   Collection (Logical)
      0x85, 0xEF,        //     Report ID (239)
      0x75, 0x08,        //     Report Size (8)
      0x95, 0x30,        //     Report Count (48)
      0x09, 0x01,        //     Usage (Pointer)
      0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //     State,No Null Position,Non-volatile)
      0xC0,              //   End Collection
      0xC0,              // End Collection
  };
  return base::make_span(kSonyDualshock3);
}

// static
base::span<const uint8_t> TestReportDescriptors::SonyDualshock4Usb() {
  // http://eleccelerator.com/wiki/index.php?title=DualShock_4#HID_Report_Descriptor
  constexpr uint8_t kSonyDualshock4[] = {
      0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
      0x09, 0x05,        // Usage (Game Pad)
      0xA1, 0x01,        // Collection (Application)
      0x85, 0x01,        //   Report ID (1)
      0x09, 0x30,        //   Usage (X)
      0x09, 0x31,        //   Usage (Y)
      0x09, 0x32,        //   Usage (Z)
      0x09, 0x35,        //   Usage (Rz)
      0x15, 0x00,        //   Logical Minimum (0)
      0x26, 0xFF, 0x00,  //   Logical Maximum (255)
      0x75, 0x08,        //   Report Size (8)
      0x95, 0x04,        //   Report Count (4)
      0x81, 0x02,  //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,
                   //   No Null Position)
      0x09, 0x39,  //   Usage (Hat switch)
      0x15, 0x00,  //   Logical Minimum (0)
      0x25, 0x07,  //   Logical Maximum (7)
      0x35, 0x00,  //   Physical Minimum (0)
      0x46, 0x3B, 0x01,  //   Physical Maximum (315)
      0x65, 0x14,  //   Unit (System: English Rotation, Length: Centimeter)
      0x75, 0x04,  //   Report Size (4)
      0x95, 0x01,  //   Report Count (1)
      0x81, 0x42,  //   Input (Data,Var,Abs,No Wrap,Linear,Preferred
                   //   State,Null State)
      0x65, 0x00,  //   Unit (None)
      0x05, 0x09,  //   Usage Page (Button)
      0x19, 0x01,  //   Usage Minimum (0x01)
      0x29, 0x0E,  //   Usage Maximum (0x0E)
      0x15, 0x00,  //   Logical Minimum (0)
      0x25, 0x01,  //   Logical Maximum (1)
      0x75, 0x01,  //   Report Size (1)
      0x95, 0x0E,  //   Report Count (14)
      0x81, 0x02,  //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,
                   //   No Null Position)
      0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
      0x09, 0x20,        //   Usage (0x20)
      0x75, 0x06,        //   Report Size (6)
      0x95, 0x01,        //   Report Count (1)
      0x15, 0x00,        //   Logical Minimum (0)
      0x25, 0x7F,        //   Logical Maximum (127)
      0x81, 0x02,  //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,
                   //   No Null Position)
      0x05, 0x01,  //   Usage Page (Generic Desktop Ctrls)
      0x09, 0x33,  //   Usage (Rx)
      0x09, 0x34,  //   Usage (Ry)
      0x15, 0x00,  //   Logical Minimum (0)
      0x26, 0xFF, 0x00,  //   Logical Maximum (255)
      0x75, 0x08,        //   Report Size (8)
      0x95, 0x02,        //   Report Count (2)
      0x81, 0x02,  //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,
                   //   No Null Position)
      0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
      0x09, 0x21,        //   Usage (0x21)
      0x95, 0x36,        //   Report Count (54)
      0x81, 0x02,  //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,
                   //   No Null Position)
      0x85, 0x05,  //   Report ID (5)
      0x09, 0x22,  //   Usage (0x22)
      0x95, 0x1F,  //   Report Count (31)
      0x91, 0x02,  //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,
                   //   No Null Position,Non-volatile)
      0x85, 0x04,  //   Report ID (4)
      0x09, 0x23,  //   Usage (0x23)
      0x95, 0x24,  //   Report Count (36)
      0xB1, 0x02,  //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                   //   State,No Null Position,Non-volatile)
      0x85, 0x02,  //   Report ID (2)
      0x09, 0x24,  //   Usage (0x24)
      0x95, 0x24,  //   Report Count (36)
      0xB1, 0x02,  //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                   //   State,No Null Position,Non-volatile)
      0x85, 0x08,  //   Report ID (8)
      0x09, 0x25,  //   Usage (0x25)
      0x95, 0x03,  //   Report Count (3)
      0xB1, 0x02,  //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                   //   State,No Null Position,Non-volatile)
      0x85, 0x10,  //   Report ID (16)
      0x09, 0x26,  //   Usage (0x26)
      0x95, 0x04,  //   Report Count (4)
      0xB1, 0x02,  //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                   //   State,No Null Position,Non-volatile)
      0x85, 0x11,  //   Report ID (17)
      0x09, 0x27,  //   Usage (0x27)
      0x95, 0x02,  //   Report Count (2)
      0xB1, 0x02,  //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                   //   State,No Null Position,Non-volatile)
      0x85, 0x12,  //   Report ID (18)
      0x06, 0x02, 0xFF,  //   Usage Page (Vendor Defined 0xFF02)
      0x09, 0x21,        //   Usage (0x21)
      0x95, 0x0F,        //   Report Count (15)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0x13,        //   Report ID (19)
      0x09, 0x22,        //   Usage (0x22)
      0x95, 0x16,        //   Report Count (22)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0x14,        //   Report ID (20)
      0x06, 0x05, 0xFF,  //   Usage Page (Vendor Defined 0xFF05)
      0x09, 0x20,        //   Usage (0x20)
      0x95, 0x10,        //   Report Count (16)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0x15,        //   Report ID (21)
      0x09, 0x21,        //   Usage (0x21)
      0x95, 0x2C,        //   Report Count (44)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x06, 0x80, 0xFF,  //   Usage Page (Vendor Defined 0xFF80)
      0x85, 0x80,        //   Report ID (128)
      0x09, 0x20,        //   Usage (0x20)
      0x95, 0x06,        //   Report Count (6)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0x81,        //   Report ID (129)
      0x09, 0x21,        //   Usage (0x21)
      0x95, 0x06,        //   Report Count (6)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0x82,        //   Report ID (130)
      0x09, 0x22,        //   Usage (0x22)
      0x95, 0x05,        //   Report Count (5)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0x83,        //   Report ID (131)
      0x09, 0x23,        //   Usage (0x23)
      0x95, 0x01,        //   Report Count (1)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0x84,        //   Report ID (132)
      0x09, 0x24,        //   Usage (0x24)
      0x95, 0x04,        //   Report Count (4)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0x85,        //   Report ID (133)
      0x09, 0x25,        //   Usage (0x25)
      0x95, 0x06,        //   Report Count (6)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0x86,        //   Report ID (134)
      0x09, 0x26,        //   Usage (0x26)
      0x95, 0x06,        //   Report Count (6)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0x87,        //   Report ID (135)
      0x09, 0x27,        //   Usage (0x27)
      0x95, 0x23,        //   Report Count (35)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0x88,        //   Report ID (136)
      0x09, 0x28,        //   Usage (0x28)
      0x95, 0x22,        //   Report Count (34)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0x89,        //   Report ID (137)
      0x09, 0x29,        //   Usage (0x29)
      0x95, 0x02,        //   Report Count (2)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0x90,        //   Report ID (144)
      0x09, 0x30,        //   Usage (0x30)
      0x95, 0x05,        //   Report Count (5)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0x91,        //   Report ID (145)
      0x09, 0x31,        //   Usage (0x31)
      0x95, 0x03,        //   Report Count (3)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0x92,        //   Report ID (146)
      0x09, 0x32,        //   Usage (0x32)
      0x95, 0x03,        //   Report Count (3)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0x93,        //   Report ID (147)
      0x09, 0x33,        //   Usage (0x33)
      0x95, 0x0C,        //   Report Count (12)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0xA0,        //   Report ID (160)
      0x09, 0x40,        //   Usage (0x40)
      0x95, 0x06,        //   Report Count (6)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0xA1,        //   Report ID (161)
      0x09, 0x41,        //   Usage (0x41)
      0x95, 0x01,        //   Report Count (1)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0xA2,        //   Report ID (162)
      0x09, 0x42,        //   Usage (0x42)
      0x95, 0x01,        //   Report Count (1)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0xA3,        //   Report ID (163)
      0x09, 0x43,        //   Usage (0x43)
      0x95, 0x30,        //   Report Count (48)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0xA4,        //   Report ID (164)
      0x09, 0x44,        //   Usage (0x44)
      0x95, 0x0D,        //   Report Count (13)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0xA5,        //   Report ID (165)
      0x09, 0x45,        //   Usage (0x45)
      0x95, 0x15,        //   Report Count (21)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0xA6,        //   Report ID (166)
      0x09, 0x46,        //   Usage (0x46)
      0x95, 0x15,        //   Report Count (21)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0xF0,        //   Report ID (240)
      0x09, 0x47,        //   Usage (0x47)
      0x95, 0x3F,        //   Report Count (63)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0xF1,        //   Report ID (241)
      0x09, 0x48,        //   Usage (0x48)
      0x95, 0x3F,        //   Report Count (63)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0xF2,        //   Report ID (242)
      0x09, 0x49,        //   Usage (0x49)
      0x95, 0x0F,        //   Report Count (15)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0xA7,        //   Report ID (167)
      0x09, 0x4A,        //   Usage (0x4A)
      0x95, 0x01,        //   Report Count (1)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0xA8,        //   Report ID (168)
      0x09, 0x4B,        //   Usage (0x4B)
      0x95, 0x01,        //   Report Count (1)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0xA9,        //   Report ID (169)
      0x09, 0x4C,        //   Usage (0x4C)
      0x95, 0x08,        //   Report Count (8)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0xAA,        //   Report ID (170)
      0x09, 0x4E,        //   Usage (0x4E)
      0x95, 0x01,        //   Report Count (1)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0xAB,        //   Report ID (171)
      0x09, 0x4F,        //   Usage (0x4F)
      0x95, 0x39,        //   Report Count (57)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0xAC,        //   Report ID (172)
      0x09, 0x50,        //   Usage (0x50)
      0x95, 0x39,        //   Report Count (57)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0xAD,        //   Report ID (173)
      0x09, 0x51,        //   Usage (0x51)
      0x95, 0x0B,        //   Report Count (11)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0xAE,        //   Report ID (174)
      0x09, 0x52,        //   Usage (0x52)
      0x95, 0x01,        //   Report Count (1)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0xAF,        //   Report ID (175)
      0x09, 0x53,        //   Usage (0x53)
      0x95, 0x02,        //   Report Count (2)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x85, 0xB0,        //   Report ID (176)
      0x09, 0x54,        //   Usage (0x54)
      0x95, 0x3F,        //   Report Count (63)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0xC0,              // End Collection
  };
  return base::make_span(kSonyDualshock4);
}

// static
base::span<const uint8_t>
TestReportDescriptors::MicrosoftXboxWirelessControllerBluetooth() {
  constexpr uint8_t kMicrosoftXboxWirelessController[] = {
      0x05, 0x01,                    // Usage Page (Generic Desktop Ctrls)
      0x09, 0x05,                    // Usage (Game Pad)
      0xA1, 0x01,                    // Collection (Application)
      0x85, 0x01,                    //   Report ID (1)
      0x09, 0x01,                    //   Usage (Pointer)
      0xA1, 0x00,                    //   Collection (Physical)
      0x09, 0x30,                    //     Usage (X)
      0x09, 0x31,                    //     Usage (Y)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x27, 0xFF, 0xFF, 0x00, 0x00,  //     Logical Maximum (65534)
      0x95, 0x02,                    //     Report Count (2)
      0x75, 0x10,                    //     Report Size (16)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0xC0,                          //   End Collection
      0x09, 0x01,                    //   Usage (Pointer)
      0xA1, 0x00,                    //   Collection (Physical)
      0x09, 0x33,                    //     Usage (Rx)
      0x09, 0x34,                    //     Usage (Ry)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x27, 0xFF, 0xFF, 0x00, 0x00,  //     Logical Maximum (65534)
      0x95, 0x02,                    //     Report Count (2)
      0x75, 0x10,                    //     Report Size (16)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0xC0,                          //   End Collection
      0x05, 0x01,                    //   Usage Page (Generic Desktop Ctrls)
      0x09, 0x32,                    //   Usage (Z)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x26, 0xFF, 0x03,              //   Logical Maximum (1023)
      0x95, 0x01,                    //   Report Count (1)
      0x75, 0x0A,                    //   Report Size (10)
      0x81, 0x02,                    //   Input (Data,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x25, 0x00,                    //   Logical Maximum (0)
      0x75, 0x06,                    //   Report Size (6)
      0x95, 0x01,                    //   Report Count (1)
      0x81, 0x03,                    //   Input (Const,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x05, 0x01,                    //   Usage Page (Generic Desktop Ctrls)
      0x09, 0x35,                    //   Usage (Rz)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x26, 0xFF, 0x03,              //   Logical Maximum (1023)
      0x95, 0x01,                    //   Report Count (1)
      0x75, 0x0A,                    //   Report Size (10)
      0x81, 0x02,                    //   Input (Data,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x25, 0x00,                    //   Logical Maximum (0)
      0x75, 0x06,                    //   Report Size (6)
      0x95, 0x01,                    //   Report Count (1)
      0x81, 0x03,                    //   Input (Const,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x05, 0x01,                    //   Usage Page (Generic Desktop Ctrls)
      0x09, 0x39,                    //   Usage (Hat switch)
      0x15, 0x01,                    //   Logical Minimum (1)
      0x25, 0x08,                    //   Logical Maximum (8)
      0x35, 0x00,                    //   Physical Minimum (0)
      0x46, 0x3B, 0x01,              //   Physical Maximum (315)
      0x66, 0x14, 0x00,              //   Unit (System: English Rotation,
                                     //   Length: Centimeter)
      0x75, 0x04,                    //   Report Size (4)
      0x95, 0x01,                    //   Report Count (1)
      0x81, 0x42,                    //   Input (Data,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,Null State)
      0x75, 0x04,                    //   Report Size (4)
      0x95, 0x01,                    //   Report Count (1)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x25, 0x00,                    //   Logical Maximum (0)
      0x35, 0x00,                    //   Physical Minimum (0)
      0x45, 0x00,                    //   Physical Maximum (0)
      0x65, 0x00,                    //   Unit (None)
      0x81, 0x03,                    //   Input (Const,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x05, 0x09,                    //   Usage Page (Button)
      0x19, 0x01,                    //   Usage Minimum (0x01)
      0x29, 0x0A,                    //   Usage Maximum (0x0A)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x25, 0x01,                    //   Logical Maximum (1)
      0x75, 0x01,                    //   Report Size (1)
      0x95, 0x0A,                    //   Report Count (10)
      0x81, 0x02,                    //   Input (Data,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x25, 0x00,                    //   Logical Maximum (0)
      0x75, 0x06,                    //   Report Size (6)
      0x95, 0x01,                    //   Report Count (1)
      0x81, 0x03,                    //   Input (Const,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x05, 0x01,                    //   Usage Page (Generic Desktop Ctrls)
      0x09, 0x80,                    //   Usage (Sys Control)
      0x85, 0x02,                    //   Report ID (2)
      0xA1, 0x00,                    //   Collection (Physical)
      0x09, 0x85,                    //     Usage (Sys Main Menu)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x25, 0x01,                    //     Logical Maximum (1)
      0x95, 0x01,                    //     Report Count (1)
      0x75, 0x01,                    //     Report Size (1)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x25, 0x00,                    //     Logical Maximum (0)
      0x75, 0x07,                    //     Report Size (7)
      0x95, 0x01,                    //     Report Count (1)
      0x81, 0x03,                    //     Input (Const,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0xC0,                          //   End Collection
      0x05, 0x0F,                    //   Usage Page (PID Page)
      0x09, 0x21,                    //   Usage (0x21)
      0x85, 0x03,                    //   Report ID (3)
      0xA1, 0x02,                    //   Collection (Logical)
      0x09, 0x97,                    //     Usage (0x97)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x25, 0x01,                    //     Logical Maximum (1)
      0x75, 0x04,                    //     Report Size (4)
      0x95, 0x01,                    //     Report Count (1)
      0x91, 0x02,                    //     Output (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position,
                                     //     Non-volatile)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x25, 0x00,                    //     Logical Maximum (0)
      0x75, 0x04,                    //     Report Size (4)
      0x95, 0x01,                    //     Report Count (1)
      0x91, 0x03,                    //     Output (Const,Var,Abs,No Wrap,
                                     //     Linear,Preferred State,No Null
                                     //     Position,Non-volatile)
      0x09, 0x70,                    //     Usage (0x70)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x25, 0x64,                    //     Logical Maximum (100)
      0x75, 0x08,                    //     Report Size (8)
      0x95, 0x04,                    //     Report Count (4)
      0x91, 0x02,                    //     Output (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position,
                                     //     Non-volatile)
      0x09, 0x50,                    //     Usage (0x50)
      0x66, 0x01, 0x10,              //     Unit (System: SI Linear, Time:
                                     //     Seconds)
      0x55, 0x0E,                    //     Unit Exponent (-2)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x75, 0x08,                    //     Report Size (8)
      0x95, 0x01,                    //     Report Count (1)
      0x91, 0x02,                    //     Output (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position,
                                     //     Non-volatile)
      0x09, 0xA7,                    //     Usage (0xA7)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x75, 0x08,                    //     Report Size (8)
      0x95, 0x01,                    //     Report Count (1)
      0x91, 0x02,                    //     Output (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position,
                                     //     Non-volatile)
      0x65, 0x00,                    //     Unit (None)
      0x55, 0x00,                    //     Unit Exponent (0)
      0x09, 0x7C,                    //     Usage (0x7C)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x75, 0x08,                    //     Report Size (8)
      0x95, 0x01,                    //     Report Count (1)
      0x91, 0x02,                    //     Output (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position,
                                     //     Non-volatile)
      0xC0,                          //   End Collection
      0x85, 0x04,                    //   Report ID (4)
      0x05, 0x06,                    //   Usage Page (Generic Dev Ctrls)
      0x09, 0x20,                    //   Usage (Battery Strength)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x26, 0xFF, 0x00,              //   Logical Maximum (255)
      0x75, 0x08,                    //   Report Size (8)
      0x95, 0x01,                    //   Report Count (1)
      0x81, 0x02,                    //   Input (Data,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null
                                     //   Position)
      0xC0,                          // End Collection
      0x00,                          // Unknown (bTag: 0x00, bType: 0x00)
  };
  return base::make_span(kMicrosoftXboxWirelessController);
}

// static
base::span<const uint8_t>
TestReportDescriptors::NintendoSwitchProControllerUsb() {
  constexpr uint8_t kNintendoSwitchProController[] = {
      0x05, 0x01,                    // Usage Page (Generic Desktop Ctrls)
      0x15, 0x00,                    // Logical Minimum (0)
      0x09, 0x04,                    // Usage (Joystick)
      0xA1, 0x01,                    // Collection (Application)
      0x85, 0x30,                    //   Report ID (48)
      0x05, 0x01,                    //   Usage Page (Generic Desktop Ctrls)
      0x05, 0x09,                    //   Usage Page (Button)
      0x19, 0x01,                    //   Usage Minimum (0x01)
      0x29, 0x0A,                    //   Usage Maximum (0x0A)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x25, 0x01,                    //   Logical Maximum (1)
      0x75, 0x01,                    //   Report Size (1)
      0x95, 0x0A,                    //   Report Count (10)
      0x55, 0x00,                    //   Unit Exponent (0)
      0x65, 0x00,                    //   Unit (None)
      0x81, 0x02,                    //   Input (Data,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x05, 0x09,                    //   Usage Page (Button)
      0x19, 0x0B,                    //   Usage Minimum (0x0B)
      0x29, 0x0E,                    //   Usage Maximum (0x0E)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x25, 0x01,                    //   Logical Maximum (1)
      0x75, 0x01,                    //   Report Size (1)
      0x95, 0x04,                    //   Report Count (4)
      0x81, 0x02,                    //   Input (Data,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x75, 0x01,                    //   Report Size (1)
      0x95, 0x02,                    //   Report Count (2)
      0x81, 0x03,                    //   Input (Const,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x0B, 0x01, 0x00, 0x01, 0x00,  //   Usage (0x010001)
      0xA1, 0x00,                    //   Collection (Physical)
      0x0B, 0x30, 0x00, 0x01, 0x00,  //     Usage (0x010030)
      0x0B, 0x31, 0x00, 0x01, 0x00,  //     Usage (0x010031)
      0x0B, 0x32, 0x00, 0x01, 0x00,  //     Usage (0x010032)
      0x0B, 0x35, 0x00, 0x01, 0x00,  //     Usage (0x010035)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x27, 0xFF, 0xFF, 0x00, 0x00,  //     Logical Maximum (65534)
      0x75, 0x10,                    //     Report Size (16)
      0x95, 0x04,                    //     Report Count (4)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0xC0,                          //   End Collection
      0x0B, 0x39, 0x00, 0x01, 0x00,  //   Usage (0x010039)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x25, 0x07,                    //   Logical Maximum (7)
      0x35, 0x00,                    //   Physical Minimum (0)
      0x46, 0x3B, 0x01,              //   Physical Maximum (315)
      0x65, 0x14,                    //   Unit (System: English Rotation,
                                     //   Length: Centimeter)
      0x75, 0x04,                    //   Report Size (4)
      0x95, 0x01,                    //   Report Count (1)
      0x81, 0x02,                    //   Input (Data,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null  Position)
      0x05, 0x09,                    //   Usage Page (Button)
      0x19, 0x0F,                    //   Usage Minimum (0x0F)
      0x29, 0x12,                    //   Usage Maximum (0x12)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x25, 0x01,                    //   Logical Maximum (1)
      0x75, 0x01,                    //   Report Size (1)
      0x95, 0x04,                    //   Report Count (4)
      0x81, 0x02,                    //   Input (Data,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x75, 0x08,                    //   Report Size (8)
      0x95, 0x34,                    //   Report Count (52)
      0x81, 0x03,                    //   Input (Const,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x06, 0x00, 0xFF,              //   Usage Page (Vendor Defined 0xFF00)
      0x85, 0x21,                    //   Report ID (33)
      0x09, 0x01,                    //   Usage (0x01)
      0x75, 0x08,                    //   Report Size (8)
      0x95, 0x3F,                    //   Report Count (63)
      0x81, 0x03,                    //   Input (Const,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x85, 0x81,                    //   Report ID (-127)
      0x09, 0x02,                    //   Usage (0x02)
      0x75, 0x08,                    //   Report Size (8)
      0x95, 0x3F,                    //   Report Count (63)
      0x81, 0x03,                    //   Input (Const,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x85, 0x01,                    //   Report ID (1)
      0x09, 0x03,                    //   Usage (0x03)
      0x75, 0x08,                    //   Report Size (8)
      0x95, 0x3F,                    //   Report Count (63)
      0x91, 0x83,                    //   Output (Const,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position,
                                     //   Volatile)
      0x85, 0x10,                    //   Report ID (16)
      0x09, 0x04,                    //   Usage (0x04)
      0x75, 0x08,                    //   Report Size (8)
      0x95, 0x3F,                    //   Report Count (63)
      0x91, 0x83,                    //   Output (Const,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position,
                                     //   Volatile)
      0x85, 0x80,                    //   Report ID (-128)
      0x09, 0x05,                    //   Usage (0x05)
      0x75, 0x08,                    //   Report Size (8)
      0x95, 0x3F,                    //   Report Count (63)
      0x91, 0x83,                    //   Output (Const,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position,
                                     //   Volatile)
      0x85, 0x82,                    //   Report ID (-126)
      0x09, 0x06,                    //   Usage (0x06)
      0x75, 0x08,                    //   Report Size (8)
      0x95, 0x3F,                    //   Report Count (63)
      0x91, 0x83,                    //   Output (Const,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position,
                                     //   Volatile)
      0xC0,                          // End Collection
  };
  return base::make_span(kNintendoSwitchProController);
}

// static
base::span<const uint8_t>
TestReportDescriptors::MicrosoftXboxAdaptiveControllerBluetooth() {
  constexpr uint8_t kMicrosoftXboxAdaptiveController[] = {
      0x05, 0x01,                    // Usage Page (Generic Desktop Ctrls)
      0x09, 0x05,                    // Usage (Game Pad)
      0xA1, 0x01,                    // Collection (Application)
      0x85, 0x01,                    //   Report ID (1)
      0x09, 0x01,                    //   Usage (Pointer)
      0xA1, 0x00,                    //   Collection (Physical)
      0x09, 0x30,                    //     Usage (X)
      0x09, 0x31,                    //     Usage (Y)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x27, 0xFF, 0xFF, 0x00, 0x00,  //     Logical Maximum (65534)
      0x95, 0x02,                    //     Report Count (2)
      0x75, 0x10,                    //     Report Size (16)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0xC0,                          //   End Collection
      0x09, 0x01,                    //   Usage (Pointer)
      0xA1, 0x00,                    //   Collection (Physical)
      0x09, 0x32,                    //     Usage (Z)
      0x09, 0x35,                    //     Usage (Rz)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x27, 0xFF, 0xFF, 0x00, 0x00,  //     Logical Maximum (65534)
      0x95, 0x02,                    //     Report Count (2)
      0x75, 0x10,                    //     Report Size (16)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0xC0,                          //   End Collection
      0x05, 0x02,                    //   Usage Page (Sim Ctrls)
      0x09, 0xC5,                    //   Usage (Brake)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x26, 0xFF, 0x03,              //   Logical Maximum (1023)
      0x95, 0x01,                    //   Report Count (1)
      0x75, 0x0A,                    //   Report Size (10)
      0x81, 0x02,                    //   Input (Data,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x25, 0x00,                    //   Logical Maximum (0)
      0x75, 0x06,                    //   Report Size (6)
      0x95, 0x01,                    //   Report Count (1)
      0x81, 0x03,                    //   Input (Const,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x05, 0x02,                    //   Usage Page (Sim Ctrls)
      0x09, 0xC4,                    //   Usage (Accelerator)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x26, 0xFF, 0x03,              //   Logical Maximum (1023)
      0x95, 0x01,                    //   Report Count (1)
      0x75, 0x0A,                    //   Report Size (10)
      0x81, 0x02,                    //   Input (Data,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x25, 0x00,                    //   Logical Maximum (0)
      0x75, 0x06,                    //   Report Size (6)
      0x95, 0x01,                    //   Report Count (1)
      0x81, 0x03,                    //   Input (Const,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x05, 0x01,                    //   Usage Page (Generic Desktop Ctrls)
      0x09, 0x39,                    //   Usage (Hat switch)
      0x15, 0x01,                    //   Logical Minimum (1)
      0x25, 0x08,                    //   Logical Maximum (8)
      0x35, 0x00,                    //   Physical Minimum (0)
      0x46, 0x3B, 0x01,              //   Physical Maximum (315)
      0x66, 0x14, 0x00,              //   Unit (System: English Rotation,
                                     //   Length: Centimeter)
      0x75, 0x04,                    //   Report Size (4)
      0x95, 0x01,                    //   Report Count (1)
      0x81, 0x42,                    //   Input (Data,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,Null State)
      0x75, 0x04,                    //   Report Size (4)
      0x95, 0x01,                    //   Report Count (1)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x25, 0x00,                    //   Logical Maximum (0)
      0x35, 0x00,                    //   Physical Minimum (0)
      0x45, 0x00,                    //   Physical Maximum (0)
      0x65, 0x00,                    //   Unit (None)
      0x81, 0x03,                    //   Input (Const,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x05, 0x09,                    //   Usage Page (Button)
      0x19, 0x01,                    //   Usage Minimum (0x01)
      0x29, 0x0F,                    //   Usage Maximum (0x0F)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x25, 0x01,                    //   Logical Maximum (1)
      0x75, 0x01,                    //   Report Size (1)
      0x95, 0x0F,                    //   Report Count (15)
      0x81, 0x02,                    //   Input (Data,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x25, 0x00,                    //   Logical Maximum (0)
      0x75, 0x01,                    //   Report Size (1)
      0x95, 0x01,                    //   Report Count (1)
      0x81, 0x03,                    //   Input (Const,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x05, 0x0C,                    //   Usage Page (Consumer)
      0x0A, 0x24, 0x02,              //   Usage (AC Back)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x25, 0x01,                    //   Logical Maximum (1)
      0x95, 0x01,                    //   Report Count (1)
      0x75, 0x01,                    //   Report Size (1)
      0x81, 0x02,                    //   Input (Data,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x25, 0x00,                    //   Logical Maximum (0)
      0x75, 0x07,                    //   Report Size (7)
      0x95, 0x01,                    //   Report Count (1)
      0x81, 0x03,                    //   Input (Const,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x05, 0x01,                    //   Usage Page (Generic Desktop Ctrls)
      0x09, 0x01,                    //   Usage (Pointer)
      0xA1, 0x00,                    //   Collection (Physical)
      0x09, 0x40,                    //     Usage (Vx)
      0x09, 0x41,                    //     Usage (Vy)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x27, 0xFF, 0xFF, 0x00, 0x00,  //     Logical Maximum (65534)
      0x95, 0x02,                    //     Report Count (2)
      0x75, 0x10,                    //     Report Size (16)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0xC0,                          //   End Collection
      0x05, 0x01,                    //   Usage Page (Generic Desktop Ctrls)
      0x09, 0x01,                    //   Usage (Pointer)
      0xA1, 0x00,                    //   Collection (Physical)
      0x09, 0x43,                    //     Usage (Vbrx)
      0x09, 0x44,                    //     Usage (Vbry)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x27, 0xFF, 0xFF, 0x00, 0x00,  //     Logical Maximum (65534)
      0x95, 0x02,                    //     Report Count (2)
      0x75, 0x10,                    //     Report Size (16)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0xC0,                          //   End Collection
      0x05, 0x01,                    //   Usage Page (Generic Desktop Ctrls)
      0x09, 0x42,                    //   Usage (Vz)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x26, 0xFF, 0x03,              //   Logical Maximum (1023)
      0x95, 0x01,                    //   Report Count (1)
      0x75, 0x0A,                    //   Report Size (10)
      0x81, 0x02,                    //   Input (Data,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x25, 0x00,                    //   Logical Maximum (0)
      0x75, 0x06,                    //   Report Size (6)
      0x95, 0x01,                    //   Report Count (1)
      0x81, 0x03,                    //   Input (Const,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x05, 0x01,                    //   Usage Page (Generic Desktop Ctrls)
      0x09, 0x45,                    //   Usage (Vbrz)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x26, 0xFF, 0x03,              //   Logical Maximum (1023)
      0x95, 0x01,                    //   Report Count (1)
      0x75, 0x0A,                    //   Report Size (10)
      0x81, 0x02,                    //   Input (Data,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x25, 0x00,                    //   Logical Maximum (0)
      0x75, 0x06,                    //   Report Size (6)
      0x95, 0x01,                    //   Report Count (1)
      0x81, 0x03,                    //   Input (Const,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x05, 0x01,                    //   Usage Page (Generic Desktop Ctrls)
      0x09, 0x37,                    //   Usage (Dial)
      0x15, 0x01,                    //   Logical Minimum (1)
      0x25, 0x08,                    //   Logical Maximum (8)
      0x35, 0x00,                    //   Physical Minimum (0)
      0x46, 0x3B, 0x01,              //   Physical Maximum (315)
      0x66, 0x14, 0x00,              //   Unit (System: English Rotation,
                                     //   Length: Centimeter)
      0x75, 0x04,                    //   Report Size (4)
      0x95, 0x01,                    //   Report Count (1)
      0x81, 0x42,                    //   Input (Data,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,Null State)
      0x75, 0x04,                    //   Report Size (4)
      0x95, 0x01,                    //   Report Count (1)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x25, 0x00,                    //   Logical Maximum (0)
      0x35, 0x00,                    //   Physical Minimum (0)
      0x45, 0x00,                    //   Physical Maximum (0)
      0x65, 0x00,                    //   Unit (None)
      0x81, 0x03,                    //   Input (Const,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x05, 0x09,                    //   Usage Page (Button)
      0x19, 0x10,                    //   Usage Minimum (0x10)
      0x29, 0x1E,                    //   Usage Maximum (0x1E)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x25, 0x01,                    //   Logical Maximum (1)
      0x75, 0x01,                    //   Report Size (1)
      0x95, 0x0F,                    //   Report Count (15)
      0x81, 0x02,                    //   Input (Data,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x25, 0x00,                    //   Logical Maximum (0)
      0x75, 0x01,                    //   Report Size (1)
      0x95, 0x01,                    //   Report Count (1)
      0x81, 0x03,                    //   Input (Const,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x05, 0x0C,                    //   Usage Page (Consumer)
      0x0A, 0x82, 0x00,              //   Usage (Mode Step)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x25, 0x01,                    //   Logical Maximum (1)
      0x95, 0x01,                    //   Report Count (1)
      0x75, 0x01,                    //   Report Size (1)
      0x81, 0x02,                    //   Input (Data,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x25, 0x00,                    //   Logical Maximum (0)
      0x75, 0x07,                    //   Report Size (7)
      0x95, 0x01,                    //   Report Count (1)
      0x81, 0x03,                    //   Input (Const,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x05, 0x0C,                    //   Usage Page (Consumer)
      0x09, 0x01,                    //   Usage (Consumer Control)
      0xA1, 0x01,                    //   Collection (Application)
      0x0A, 0x81, 0x00,              //     Usage (Assign Selection)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x95, 0x01,                    //     Report Count (1)
      0x75, 0x04,                    //     Report Size (4)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x25, 0x00,                    //     Logical Maximum (0)
      0x95, 0x01,                    //     Report Count (1)
      0x75, 0x04,                    //     Report Size (4)
      0x81, 0x03,                    //     Input (Const,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0x0A, 0x84, 0x00,              //     Usage (Enter Channel)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x95, 0x01,                    //     Report Count (1)
      0x75, 0x04,                    //     Report Size (4)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x25, 0x00,                    //     Logical Maximum (0)
      0x95, 0x01,                    //     Report Count (1)
      0x75, 0x04,                    //     Report Size (4)
      0x81, 0x03,                    //     Input (Const,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0x0A, 0x85, 0x00,              //     Usage (Order Movie)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x95, 0x01,                    //     Report Count (1)
      0x75, 0x08,                    //     Report Size (8)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0x0A, 0x99, 0x00,              //     Usage (Media Select Security)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x95, 0x01,                    //     Report Count (1)
      0x75, 0x04,                    //     Report Size (4)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x25, 0x00,                    //     Logical Maximum (0)
      0x95, 0x01,                    //     Report Count (1)
      0x75, 0x04,                    //     Report Size (4)
      0x81, 0x03,                    //     Input (Const,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0x0A, 0x9E, 0x00,              //     Usage (Media Select SAP)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x95, 0x01,                    //     Report Count (1)
      0x75, 0x08,                    //     Report Size (8)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0x0A, 0xA1, 0x00,              //     Usage (Once)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x95, 0x01,                    //     Report Count (1)
      0x75, 0x08,                    //     Report Size (8)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0x0A, 0xA2, 0x00,              //     Usage (Daily)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x95, 0x01,                    //     Report Count (1)
      0x75, 0x08,                    //     Report Size (8)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0x0A, 0xA3, 0x00,              //     Usage (Weekly)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x95, 0x01,                    //     Report Count (1)
      0x75, 0x08,                    //     Report Size (8)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0x0A, 0xA4, 0x00,              //     Usage (Monthly)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x95, 0x01,                    //     Report Count (1)
      0x75, 0x08,                    //     Report Size (8)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0x0A, 0xB9, 0x00,              //     Usage (Random Play)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x95, 0x01,                    //     Report Count (1)
      0x75, 0x08,                    //     Report Size (8)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0x0A, 0xBA, 0x00,              //     Usage (Select Disc)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x95, 0x01,                    //     Report Count (1)
      0x75, 0x08,                    //     Report Size (8)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0x0A, 0xBB, 0x00,              //     Usage (Enter Disc)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x95, 0x01,                    //     Report Count (1)
      0x75, 0x08,                    //     Report Size (8)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0x0A, 0xBE, 0x00,              //     Usage (Track Normal)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x95, 0x01,                    //     Report Count (1)
      0x75, 0x08,                    //     Report Size (8)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0x0A, 0xC0, 0x00,              //     Usage (Frame Forward)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x95, 0x01,                    //     Report Count (1)
      0x75, 0x08,                    //     Report Size (8)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0x0A, 0xC1, 0x00,              //     Usage (Frame Back)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x95, 0x01,                    //     Report Count (1)
      0x75, 0x08,                    //     Report Size (8)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0x0A, 0xC2, 0x00,              //     Usage (Mark)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x95, 0x01,                    //     Report Count (1)
      0x75, 0x08,                    //     Report Size (8)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0x0A, 0xC3, 0x00,              //     Usage (Clear Mark)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x95, 0x01,                    //     Report Count (1)
      0x75, 0x08,                    //     Report Size (8)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0x0A, 0xC4, 0x00,              //     Usage (Repeat From Mark)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x95, 0x01,                    //     Report Count (1)
      0x75, 0x08,                    //     Report Size (8)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0x0A, 0xC5, 0x00,              //     Usage (Return To Mark)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x95, 0x01,                    //     Report Count (1)
      0x75, 0x08,                    //     Report Size (8)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0x0A, 0xC6, 0x00,              //     Usage (Search Mark Forward)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x95, 0x01,                    //     Report Count (1)
      0x75, 0x08,                    //     Report Size (8)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0x0A, 0xC7, 0x00,              //     Usage (Search Mark Backwards)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x95, 0x01,                    //     Report Count (1)
      0x75, 0x08,                    //     Report Size (8)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0x0A, 0xC8, 0x00,              //     Usage (Counter Reset)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x95, 0x01,                    //     Report Count (1)
      0x75, 0x08,                    //     Report Size (8)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0xC0,                          //   End Collection
      0x05, 0x0C,                    //   Usage Page (Consumer)
      0x09, 0x01,                    //   Usage (Consumer Control)
      0x85, 0x02,                    //   Report ID (2)
      0xA1, 0x01,                    //   Collection (Application)
      0x05, 0x0C,                    //     Usage Page (Consumer)
      0x0A, 0x23, 0x02,              //     Usage (AC Home)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x25, 0x01,                    //     Logical Maximum (1)
      0x95, 0x01,                    //     Report Count (1)
      0x75, 0x01,                    //     Report Size (1)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x25, 0x00,                    //     Logical Maximum (0)
      0x75, 0x07,                    //     Report Size (7)
      0x95, 0x01,                    //     Report Count (1)
      0x81, 0x03,                    //     Input (Const,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0xC0,                          //   End Collection
      0x05, 0x0F,                    //   Usage Page (PID Page)
      0x09, 0x21,                    //   Usage (0x21)
      0x85, 0x03,                    //   Report ID (3)
      0xA1, 0x02,                    //   Collection (Logical)
      0x09, 0x97,                    //     Usage (0x97)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x25, 0x01,                    //     Logical Maximum (1)
      0x75, 0x04,                    //     Report Size (4)
      0x95, 0x01,                    //     Report Count (1)
      0x91, 0x02,                    //     Output (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position,
                                     //     Non-volatile)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x25, 0x00,                    //     Logical Maximum (0)
      0x75, 0x04,                    //     Report Size (4)
      0x95, 0x01,                    //     Report Count (1)
      0x91, 0x03,                    //     Output (Const,Var,Abs,No Wrap,
                                     //     Linear,Preferred State,No Null
                                     //     Position,Non-volatile)
      0x09, 0x70,                    //     Usage (0x70)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x25, 0x64,                    //     Logical Maximum (100)
      0x75, 0x08,                    //     Report Size (8)
      0x95, 0x04,                    //     Report Count (4)
      0x91, 0x02,                    //     Output (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position,
                                     //     Non-volatile)
      0x09, 0x50,                    //     Usage (0x50)
      0x66, 0x01, 0x10,              //     Unit (System: SI Linear, Time:
                                     //     Seconds)
      0x55, 0x0E,                    //     Unit Exponent (-2)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x75, 0x08,                    //     Report Size (8)
      0x95, 0x01,                    //     Report Count (1)
      0x91, 0x02,                    //     Output (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position,
                                     //     Non-volatile)
      0x09, 0xA7,                    //     Usage (0xA7)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x75, 0x08,                    //     Report Size (8)
      0x95, 0x01,                    //     Report Count (1)
      0x91, 0x02,                    //     Output (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position,
                                     //     Non-volatile)
      0x65, 0x00,                    //     Unit (None)
      0x55, 0x00,                    //     Unit Exponent (0)
      0x09, 0x7C,                    //     Usage (0x7C)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x75, 0x08,                    //     Report Size (8)
      0x95, 0x01,                    //     Report Count (1)
      0x91, 0x02,                    //     Output (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position,
                                     //     Non-volatile)
      0xC0,                          //   End Collection
      0x05, 0x06,                    //   Usage Page (Generic Dev Ctrls)
      0x09, 0x20,                    //   Usage (Battery Strength)
      0x85, 0x04,                    //   Report ID (4)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x26, 0xFF, 0x00,              //   Logical Maximum (255)
      0x75, 0x08,                    //   Report Size (8)
      0x95, 0x01,                    //   Report Count (1)
      0x81, 0x02,                    //   Input (Data,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x06, 0x00, 0xFF,              //   Usage Page (Vendor Defined 0xFF00)
      0x09, 0x01,                    //   Usage (0x01)
      0xA1, 0x02,                    //   Collection (Logical)
      0x85, 0x06,                    //     Report ID (6)
      0x09, 0x01,                    //     Usage (0x01)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x25, 0x64,                    //     Logical Maximum (100)
      0x75, 0x08,                    //     Report Size (8)
      0x95, 0x01,                    //     Report Count (1)
      0xB1, 0x02,                    //     Feature (Data,Var,Abs,No Wrap,
                                     //     Linear,Preferred State,No Null
                                     //     Position,Non-volatile)
      0x09, 0x02,                    //     Usage (0x02)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x25, 0x64,                    //     Logical Maximum (100)
      0x75, 0x08,                    //     Report Size (8)
      0x95, 0x01,                    //     Report Count (1)
      0xB1, 0x02,                    //     Feature (Data,Var,Abs,No Wrap,
                                     //     Linear,Preferred State,No Null
                                     //     Position,Non-volatile)
      0x09, 0x03,                    //     Usage (0x03)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x75, 0x08,                    //     Report Size (8)
      0x95, 0x01,                    //     Report Count (1)
      0xB1, 0x02,                    //     Feature (Data,Var,Abs,No Wrap,
                                     //     Linear,Preferred State,No Null
                                     //     Position,Non-volatile)
      0x09, 0x04,                    //     Usage (0x04)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x75, 0x08,                    //     Report Size (8)
      0x95, 0x3C,                    //     Report Count (60)
      0xB2, 0x02, 0x01,              //     Feature (Data,Var,Abs,No Wrap,
                                     //     Linear,Preferred State,No Null
                                     //     Position,Non-volatile,Buffered
                                     //     Bytes)
      0xC0,                          //   End Collection
      0x06, 0x00, 0xFF,              //   Usage Page (Vendor Defined 0xFF00)
      0x09, 0x02,                    //   Usage (0x02)
      0xA1, 0x02,                    //   Collection (Logical)
      0x85, 0x07,                    //     Report ID (7)
      0x09, 0x05,                    //     Usage (0x05)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x25, 0x64,                    //     Logical Maximum (100)
      0x75, 0x08,                    //     Report Size (8)
      0x95, 0x01,                    //     Report Count (1)
      0xB1, 0x02,                    //     Feature (Data,Var,Abs,No Wrap,
                                     //     Linear,Preferred State,No Null
                                     //     Position,Non-volatile)
      0x09, 0x06,                    //     Usage (0x06)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x25, 0x64,                    //     Logical Maximum (100)
      0x75, 0x08,                    //     Report Size (8)
      0x95, 0x01,                    //     Report Count (1)
      0xB1, 0x02,                    //     Feature (Data,Var,Abs,No Wrap,
                                     //     Linear,Preferred State,No Null
                                     //     Position,Non-volatile)
      0x09, 0x07,                    //     Usage (0x07)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x25, 0x64,                    //     Logical Maximum (100)
      0x75, 0x08,                    //     Report Size (8)
      0x95, 0x01,                    //     Report Count (1)
      0xB1, 0x02,                    //     Feature (Data,Var,Abs,No Wrap,
                                     //     Linear,Preferred State,No Null
                                     //     Position,Non-volatile)
      0xC0,                          //   End Collection
      0x06, 0x00, 0xFF,              //   Usage Page (Vendor Defined 0xFF00)
      0x09, 0x03,                    //   Usage (0x03)
      0xA1, 0x02,                    //   Collection (Logical)
      0x85, 0x08,                    //     Report ID (8)
      0x09, 0x08,                    //     Usage (0x08)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x25, 0x64,                    //     Logical Maximum (100)
      0x75, 0x08,                    //     Report Size (8)
      0x95, 0x01,                    //     Report Count (1)
      0xB1, 0x02,                    //     Feature (Data,Var,Abs,No Wrap,
                                     //     Linear,Preferred State,No Null
                                     //     Position,Non-volatile)
      0x09, 0x09,                    //     Usage (0x09)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x25, 0x64,                    //     Logical Maximum (100)
      0x75, 0x08,                    //     Report Size (8)
      0x95, 0x01,                    //     Report Count (1)
      0xB1, 0x02,                    //     Feature (Data,Var,Abs,No Wrap,
                                     //     Linear,Preferred State,No Null
                                     //     Position,Non-volatile)
      0x09, 0x0A,                    //     Usage (0x0A)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x75, 0x08,                    //     Report Size (8)
      0x95, 0x01,                    //     Report Count (1)
      0xB1, 0x02,                    //     Feature (Data,Var,Abs,No Wrap,
                                     //     Linear,Preferred State,No Null
                                     //     Position,Non-volatile)
      0xC0,                          //   End Collection
      0x06, 0x00, 0xFF,              //   Usage Page (Vendor Defined 0xFF00)
      0x09, 0x04,                    //   Usage (0x04)
      0xA1, 0x01,                    //   Collection (Application)
      0x85, 0x09,                    //     Report ID (9)
      0x09, 0x0B,                    //     Usage (0x0B)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x25, 0x64,                    //     Logical Maximum (100)
      0x75, 0x08,                    //     Report Size (8)
      0x95, 0x01,                    //     Report Count (1)
      0xB1, 0x02,                    //     Feature (Data,Var,Abs,No Wrap,
                                     //     Linear,Preferred State,No Null
                                     //     Position,Non-volatile)
      0x09, 0x0C,                    //     Usage (0x0C)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x25, 0x64,                    //     Logical Maximum (100)
      0x75, 0x08,                    //     Report Size (8)
      0x95, 0x01,                    //     Report Count (1)
      0xB1, 0x02,                    //     Feature (Data,Var,Abs,No Wrap,
                                     //     Linear,Preferred State,No Null
                                     //     Position,Non-volatile)
      0x09, 0x0D,                    //     Usage (0x0D)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x25, 0x64,                    //     Logical Maximum (100)
      0x75, 0x08,                    //     Report Size (8)
      0x95, 0x01,                    //     Report Count (1)
      0xB1, 0x02,                    //     Feature (Data,Var,Abs,No Wrap,
                                     //     Linear,Preferred State,No Null
                                     //     Position,Non-volatile)
      0x09, 0x0E,                    //     Usage (0x0E)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x75, 0x08,                    //     Report Size (8)
      0x95, 0x01,                    //     Report Count (1)
      0xB1, 0x02,                    //     Feature (Data,Var,Abs,No Wrap,
                                     //     Linear,Preferred State,No Null
                                     //     Position,Non-volatile)
      0x09, 0x0F,                    //     Usage (0x0F)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x75, 0x08,                    //     Report Size (8)
      0x95, 0x3C,                    //     Report Count (60)
      0xB2, 0x02, 0x01,              //     Feature (Data,Var,Abs,No Wrap,
                                     //     Linear,Preferred State,No Null
                                     //     Position,Non-volatile,Buffered
                                     //     Bytes)
      0xC0,                          //   End Collection
      0x06, 0x00, 0xFF,              //   Usage Page (Vendor Defined 0xFF00)
      0x09, 0x05,                    //   Usage (0x05)
      0xA1, 0x01,                    //   Collection (Application)
      0x85, 0x0A,                    //     Report ID (10)
      0x09, 0x10,                    //     Usage (0x10)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x27, 0xFF, 0xFF, 0xFF, 0x7F,  //     Logical Maximum (2147483646)
      0x75, 0x20,                    //     Report Size (32)
      0x95, 0x01,                    //     Report Count (1)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0x09, 0x11,                    //     Usage (0x11)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x27, 0xFF, 0xFF, 0xFF, 0x7F,  //     Logical Maximum (2147483646)
      0x75, 0x20,                    //     Report Size (32)
      0x95, 0x01,                    //     Report Count (1)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0x09, 0x12,                    //     Usage (0x12)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x75, 0x08,                    //     Report Size (8)
      0x95, 0x02,                    //     Report Count (2)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0x09, 0x13,                    //     Usage (0x13)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x26, 0xFF, 0x00,              //     Logical Maximum (255)
      0x75, 0x08,                    //     Report Size (8)
      0x95, 0x01,                    //     Report Count (1)
      0x81, 0x02,                    //     Input (Data,Var,Abs,No Wrap,Linear,
                                     //     Preferred State,No Null Position)
      0xC0,                          //   End Collection
      0x06, 0x00, 0xFF,              //   Usage Page (Vendor Defined 0xFF00)
      0x09, 0x06,                    //   Usage (0x06)
      0xA1, 0x02,                    //   Collection (Logical)
      0x85, 0x0B,                    //     Report ID (11)
      0x09, 0x14,                    //     Usage (0x14)
      0x15, 0x00,                    //     Logical Minimum (0)
      0x25, 0x64,                    //     Logical Maximum (100)
      0x75, 0x08,                    //     Report Size (8)
      0x95, 0x01,                    //     Report Count (1)
      0xB1, 0x02,                    //     Feature (Data,Var,Abs,No Wrap,
                                     //     Linear,Preferred State,No Null
                                     //     Position,Non-volatile)
      0xC0,                          //   End Collection
      0xC0,                          // End Collection
      0x05, 0x01,                    // Usage Page (Generic Desktop Ctrls)
      0x09, 0x06,                    // Usage (Keyboard)
      0xA1, 0x01,                    // Collection (Application)
      0x85, 0x05,                    //   Report ID (5)
      0x05, 0x07,                    //   Usage Page (Kbrd/Keypad)
      0x19, 0xE0,                    //   Usage Minimum (0xE0)
      0x29, 0xE7,                    //   Usage Maximum (0xE7)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x25, 0x01,                    //   Logical Maximum (1)
      0x75, 0x01,                    //   Report Size (1)
      0x95, 0x08,                    //   Report Count (8)
      0x81, 0x02,                    //   Input (Data,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x95, 0x01,                    //   Report Count (1)
      0x75, 0x08,                    //   Report Size (8)
      0x81, 0x03,                    //   Input (Const,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0x95, 0x06,                    //   Report Count (6)
      0x75, 0x08,                    //   Report Size (8)
      0x15, 0x00,                    //   Logical Minimum (0)
      0x25, 0x65,                    //   Logical Maximum (101)
      0x05, 0x07,                    //   Usage Page (Kbrd/Keypad)
      0x19, 0x00,                    //   Usage Minimum (0x00)
      0x29, 0x65,                    //   Usage Maximum (0x65)
      0x81, 0x00,                    //   Input (Data,Array,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0xC0,                          // End Collection
      0x00,                          // Unknown (bTag: 0x00, bType: 0x00)
  };
  return base::make_span(kMicrosoftXboxAdaptiveController);
}

// static
base::span<const uint8_t> TestReportDescriptors::NexusPlayerController() {
  constexpr uint8_t kNexusPlayerController[] = {
      0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
      0x09, 0x05,        // Usage (Game Pad)
      0xA1, 0x01,        // Collection (Application)
      0x85, 0x01,        //   Report ID (1)
      0x05, 0x09,        //   Usage Page (Button)
      0x0A, 0x01, 0x00,  //   Usage (0x01)
      0x0A, 0x02, 0x00,  //   Usage (0x02)
      0x0A, 0x04, 0x00,  //   Usage (0x04)
      0x0A, 0x05, 0x00,  //   Usage (0x05)
      0x0A, 0x07, 0x00,  //   Usage (0x07)
      0x0A, 0x08, 0x00,  //   Usage (0x08)
      0x0A, 0x0E, 0x00,  //   Usage (0x0E)
      0x0A, 0x0F, 0x00,  //   Usage (0x0F)
      0x0A, 0x0D, 0x00,  //   Usage (0x0D)
      0x05, 0x0C,        //   Usage Page (Consumer)
      0x0A, 0x24, 0x02,  //   Usage (AC Back)
      0x0A, 0x23, 0x02,  //   Usage (AC Home)
      0x15, 0x00,        //   Logical Minimum (0)
      0x25, 0x01,        //   Logical Maximum (1)
      0x75, 0x01,        //   Report Size (1)
      0x95, 0x0B,        //   Report Count (11)
      0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position)
      0x75, 0x01,        //   Report Size (1)
      0x95, 0x01,        //   Report Count (1)
      0x81, 0x03,        //   Input (Const,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position)
      0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
      0x75, 0x04,        //   Report Size (4)
      0x95, 0x01,        //   Report Count (1)
      0x25, 0x07,        //   Logical Maximum (7)
      0x46, 0x3B, 0x01,  //   Physical Maximum (315)
      0x66, 0x14, 0x00,  //   Unit (System: English Rotation, Length:
                         //   Centimeter)
      0x09, 0x39,        //   Usage (Hat switch)
      0x81, 0x42,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,Null State)
      0x66, 0x00, 0x00,  //   Unit (None)
      0xA1, 0x00,        //   Collection (Physical)
      0x09, 0x30,        //     Usage (X)
      0x09, 0x31,        //     Usage (Y)
      0x09, 0x32,        //     Usage (Z)
      0x09, 0x35,        //     Usage (Rz)
      0x05, 0x02,        //     Usage Page (Sim Ctrls)
      0x09, 0xC5,        //     Usage (Brake)
      0x09, 0xC4,        //     Usage (Accelerator)
      0x15, 0x00,        //     Logical Minimum (0)
      0x26, 0xFF, 0x00,  //     Logical Maximum (255)
      0x35, 0x00,        //     Physical Minimum (0)
      0x46, 0xFF, 0x00,  //     Physical Maximum (255)
      0x75, 0x08,        //     Report Size (8)
      0x95, 0x06,        //     Report Count (6)
      0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred
                         //     State,No Null Position)
      0xC0,              //   End Collection
      0x85, 0x02,        //   Report ID (2)
      0x05, 0x08,        //   Usage Page (LEDs)
      0x0A, 0x01, 0x00,  //   Usage (Num Lock)
      0x0A, 0x02, 0x00,  //   Usage (Caps Lock)
      0x0A, 0x03, 0x00,  //   Usage (Scroll Lock)
      0x0A, 0x04, 0x00,  //   Usage (Compose)
      0x15, 0x00,        //   Logical Minimum (0)
      0x25, 0x01,        //   Logical Maximum (1)
      0x75, 0x01,        //   Report Size (1)
      0x95, 0x04,        //   Report Count (4)
      0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x75, 0x04,        //   Report Size (4)
      0x95, 0x01,        //   Report Count (1)
      0x91, 0x03,        //   Output (Const,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0xC0,              // End Collection
      0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
      0x09, 0x05,        // Usage (Game Pad)
      0xA1, 0x01,        // Collection (Application)
      0x85, 0x03,        //   Report ID (3)
      0x05, 0x06,        //   Usage Page (Generic Dev Ctrls)
      0x09, 0x20,        //   Usage (Battery Strength)
      0x15, 0x00,        //   Logical Minimum (0)
      0x26, 0xFF, 0x00,  //   Logical Maximum (255)
      0x75, 0x08,        //   Report Size (8)
      0x95, 0x01,        //   Report Count (1)
      0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position)
      0x06, 0xBC, 0xFF,  //   Usage Page (Vendor Defined 0xFFBC)
      0x0A, 0xAD, 0xBD,  //   Usage (0xBDAD)
      0x75, 0x08,        //   Report Size (8)
      0x95, 0x06,        //   Report Count (6)
      0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position)
      0xC0,              // End Collection
      0x00,              // Unknown (bTag: 0x00, bType: 0x00)
  };
  return base::make_span(kNexusPlayerController);
}

// static
base::span<const uint8_t> TestReportDescriptors::SteamControllerKeyboard() {
  constexpr uint8_t kSteamControllerKeyboard[] = {
      0x05, 0x01,  // Usage Page (Generic Desktop Ctrls)
      0x09, 0x06,  // Usage (Keyboard)
      0x95, 0x01,  // Report Count (1)
      0xA1, 0x01,  // Collection (Application)
      0x05, 0x07,  //   Usage Page (Kbrd/Keypad)
      0x19, 0xE0,  //   Usage Minimum (0xE0)
      0x29, 0xE7,  //   Usage Maximum (0xE7)
      0x15, 0x00,  //   Logical Minimum (0)
      0x25, 0x01,  //   Logical Maximum (1)
      0x75, 0x01,  //   Report Size (1)
      0x95, 0x08,  //   Report Count (8)
      0x81, 0x02,  //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,
                   //   No Null Position)
      0x95, 0x01,  //   Report Count (1)
      0x75, 0x08,  //   Report Size (8)
      0x81, 0x01,  //   Input (Const,Array,Abs,No Wrap,Linear,Preferred
                   //   State,No Null Position)
      0x95, 0x05,  //   Report Count (5)
      0x75, 0x01,  //   Report Size (1)
      0x05, 0x08,  //   Usage Page (LEDs)
      0x19, 0x01,  //   Usage Minimum (Num Lock)
      0x29, 0x05,  //   Usage Maximum (Kana)
      0x91, 0x02,  //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,
                   //   No Null Position,Non-volatile)
      0x95, 0x01,  //   Report Count (1)
      0x75, 0x03,  //   Report Size (3)
      0x91, 0x01,  //   Output (Const,Array,Abs,No Wrap,Linear,Preferred
                   //   State,No Null Position,Non-volatile)
      0x95, 0x06,  //   Report Count (6)
      0x75, 0x08,  //   Report Size (8)
      0x15, 0x00,  //   Logical Minimum (0)
      0x25, 0x65,  //   Logical Maximum (101)
      0x05, 0x07,  //   Usage Page (Kbrd/Keypad)
      0x19, 0x00,  //   Usage Minimum (0x00)
      0x29, 0x65,  //   Usage Maximum (0x65)
      0x81, 0x00,  //   Input (Data,Array,Abs,No Wrap,Linear,Preferred
                   //   State,No Null Position)
      0xC0,        // End Collection
  };
  return base::make_span(kSteamControllerKeyboard);
}

// static
base::span<const uint8_t> TestReportDescriptors::SteamControllerMouse() {
  constexpr uint8_t kSteamControllerMouse[] = {
      0x05, 0x01,  // Usage Page (Generic Desktop Ctrls)
      0x09, 0x02,  // Usage (Mouse)
      0xA1, 0x01,  // Collection (Application)
      0x09, 0x01,  //   Usage (Pointer)
      0xA1, 0x00,  //   Collection (Physical)
      0x05, 0x09,  //     Usage Page (Button)
      0x19, 0x01,  //     Usage Minimum (0x01)
      0x29, 0x05,  //     Usage Maximum (0x05)
      0x15, 0x00,  //     Logical Minimum (0)
      0x25, 0x01,  //     Logical Maximum (1)
      0x95, 0x05,  //     Report Count (5)
      0x75, 0x01,  //     Report Size (1)
      0x81, 0x02,  //     Input (Data,Var,Abs,No Wrap,Linear,Preferred
                   //     State,No Null Position)
      0x95, 0x01,  //     Report Count (1)
      0x75, 0x03,  //     Report Size (3)
      0x81, 0x01,  //     Input (Const,Array,Abs,No Wrap,Linear,Preferred
                   //     State,No Null Position)
      0x05, 0x01,  //     Usage Page (Generic Desktop Ctrls)
      0x09, 0x30,  //     Usage (X)
      0x09, 0x31,  //     Usage (Y)
      0x09, 0x38,  //     Usage (Wheel)
      0x15, 0x81,  //     Logical Minimum (-127)
      0x25, 0x7F,  //     Logical Maximum (127)
      0x75, 0x08,  //     Report Size (8)
      0x95, 0x03,  //     Report Count (3)
      0x81, 0x06,  //     Input (Data,Var,Rel,No Wrap,Linear,Preferred
                   //     State,No Null Position)
      0xC0,        //   End Collection
      0xC0,        // End Collection
  };
  return base::make_span(kSteamControllerMouse);
}

// static
base::span<const uint8_t> TestReportDescriptors::SteamControllerVendor() {
  constexpr uint8_t kSteamControllerVendor[] = {
      0x06, 0x00, 0xFF,  // Usage Page (Vendor Defined 0xFF00)
      0x09, 0x01,        // Usage (0x01)
      0xA1, 0x01,        // Collection (Application)
      0x15, 0x00,        //   Logical Minimum (0)
      0x26, 0xFF, 0x00,  //   Logical Maximum (255)
      0x75, 0x08,        //   Report Size (8)
      0x95, 0x40,        //   Report Count (64)
      0x09, 0x01,        //   Usage (0x01)
      0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position)
      0x95, 0x40,        //   Report Count (64)
      0x09, 0x01,        //   Usage (0x01)
      0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x95, 0x40,        //   Report Count (64)
      0x09, 0x01,        //   Usage (0x01)
      0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0xC0,              // End Collection
  };
  return base::make_span(kSteamControllerVendor);
}

// static
base::span<const uint8_t> TestReportDescriptors::XSkillsUsbAdapter() {
  constexpr uint8_t kXSkillsUsbAdapter[] = {
      0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
      0x09, 0x04,        // Usage (Joystick)
      0xA1, 0x01,        // Collection (Application)
      0x05, 0x09,        //   Usage Page (Button)
      0x19, 0x01,        //   Usage Minimum (0x01)
      0x29, 0x0C,        //   Usage Maximum (0x0C)
      0x15, 0x00,        //   Logical Minimum (0)
      0x25, 0x01,        //   Logical Maximum (1)
      0x35, 0x00,        //   Physical Minimum (0)
      0x45, 0x01,        //   Physical Maximum (1)
      0x75, 0x01,        //   Report Size (1)
      0x95, 0x0C,        //   Report Count (12)
      0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position)
      0x95, 0x04,        //   Report Count (4)
      0x81, 0x03,        //   Input (Const,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position)
      0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
      0x09, 0x30,        //   Usage (X)
      0x09, 0x31,        //   Usage (Y)
      0x09, 0x35,        //   Usage (Rz)
      0x09, 0x32,        //   Usage (Z)
      0x26, 0xFF, 0x00,  //   Logical Maximum (255)
      0x46, 0xFF, 0x00,  //   Physical Maximum (255)
      0x66, 0x00, 0x00,  //   Unit (None)
      0x75, 0x08,        //   Report Size (8)
      0x95, 0x04,        //   Report Count (4)
      0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position)
      0x09, 0x33,        //   Usage (Rx)
      0x09, 0x34,        //   Usage (Ry)
      0x26, 0x0F, 0x00,  //   Logical Maximum (15)
      0x46, 0x0F, 0x00,  //   Physical Maximum (15)
      0x75, 0x04,        //   Report Size (4)
      0x95, 0x02,        //   Report Count (2)
      0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position)
      0x75, 0x08,        //   Report Size (8)
      0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
      0x19, 0x01,        //   Usage Minimum (0x01)
      0x29, 0x04,        //   Usage Maximum (0x04)
      0x95, 0x04,        //   Report Count (4)
      0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0xC0,              // End Collection
  };
  return base::make_span(kXSkillsUsbAdapter);
}

// static
base::span<const uint8_t> TestReportDescriptors::BelkinNostromoKeyboard() {
  constexpr uint8_t kBelkinNostromoKeyboard[] = {
      0x05, 0x01,  // Usage Page (Generic Desktop Ctrls)
      0x09, 0x06,  // Usage (Keyboard)
      0xA1, 0x01,  // Collection (Application)
      0x05, 0x07,  //   Usage Page (Kbrd/Keypad)
      0x19, 0xE0,  //   Usage Minimum (0xE0)
      0x29, 0xE7,  //   Usage Maximum (0xE7)
      0x15, 0x00,  //   Logical Minimum (0)
      0x25, 0x01,  //   Logical Maximum (1)
      0x75, 0x01,  //   Report Size (1)
      0x95, 0x08,  //   Report Count (8)
      0x81, 0x02,  //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,
                   //   No Null Position)
      0x95, 0x01,  //   Report Count (1)
      0x75, 0x08,  //   Report Size (8)
      0x81, 0x01,  //   Input (Const,Array,Abs,No Wrap,Linear,Preferred
                   //   State,No Null Position)
      0x95, 0x06,  //   Report Count (6)
      0x75, 0x08,  //   Report Size (8)
      0x15, 0x00,  //   Logical Minimum (0)
      0x25, 0x65,  //   Logical Maximum (101)
      0x05, 0x07,  //   Usage Page (Kbrd/Keypad)
      0x19, 0x00,  //   Usage Minimum (0x00)
      0x29, 0x65,  //   Usage Maximum (0x65)
      0x81, 0x00,  //   Input (Data,Array,Abs,No Wrap,Linear,Preferred
                   //   State,No Null Position)
      0xC0,        // End Collection
  };
  return base::make_span(kBelkinNostromoKeyboard);
}

// static
base::span<const uint8_t> TestReportDescriptors::BelkinNostromoMouseAndExtra() {
  constexpr uint8_t kBelkinNostromoMouseAndExtra[] = {
      0x05, 0x01,  // Usage Page (Generic Desktop Ctrls)
      0x09, 0x02,  // Usage (Mouse)
      0xA1, 0x01,  // Collection (Application)
      0x09, 0x01,  //   Usage (Pointer)
      0xA1, 0x00,  //   Collection (Physical)
      0x05, 0x09,  //     Usage Page (Button)
      0x19, 0x01,  //     Usage Minimum (0x01)
      0x29, 0x03,  //     Usage Maximum (0x03)
      0x15, 0x00,  //     Logical Minimum (0)
      0x25, 0x01,  //     Logical Maximum (1)
      0x95, 0x03,  //     Report Count (3)
      0x75, 0x01,  //     Report Size (1)
      0x81, 0x02,  //     Input (Data,Var,Abs,No Wrap,Linear,Preferred
                   //     State,No Null Position)
      0x95, 0x01,  //     Report Count (1)
      0x75, 0x05,  //     Report Size (5)
      0x81, 0x01,  //     Input (Const,Array,Abs,No Wrap,Linear,Preferred
                   //     State,No Null Position)
      0x05, 0x01,  //     Usage Page (Generic Desktop Ctrls)
      0x09, 0x30,  //     Usage (X)
      0x09, 0x31,  //     Usage (Y)
      0x09, 0x38,  //     Usage (Wheel)
      0x15, 0x81,  //     Logical Minimum (-127)
      0x25, 0x7F,  //     Logical Maximum (127)
      0x75, 0x08,  //     Report Size (8)
      0x95, 0x03,  //     Report Count (3)
      0x81, 0x06,  //     Input (Data,Var,Rel,No Wrap,Linear,Preferred
                   //     State,No Null Position)
      0x05, 0x08,  //     Usage Page (LEDs)
      0x19, 0x01,  //     Usage Minimum (Num Lock)
      0x29, 0x03,  //     Usage Maximum (Scroll Lock)
      0x15, 0x00,  //     Logical Minimum (0)
      0x25, 0x01,  //     Logical Maximum (1)
      0x35, 0x00,  //     Physical Minimum (0)
      0x45, 0x01,  //     Physical Maximum (1)
      0x75, 0x01,  //     Report Size (1)
      0x95, 0x03,  //     Report Count (3)
      0x91, 0x02,  //     Output (Data,Var,Abs,No Wrap,Linear,Preferred
                   //     State,No Null Position,Non-volatile)
      0x75, 0x05,  //     Report Size (5)
      0x95, 0x01,  //     Report Count (1)
      0x91, 0x01,  //     Output (Const,Array,Abs,No Wrap,Linear,Preferred
                   //     State,No Null Position,Non-volatile)
      0xC0,        //   End Collection
      0xC0,        // End Collection
  };
  return base::make_span(kBelkinNostromoMouseAndExtra);
}

// static
base::span<const uint8_t> TestReportDescriptors::JabraLink380c() {
  constexpr uint8_t kJabraLink380c[] = {
      0x05, 0x0B,        // Usage Page (Telephony)
      0x09, 0x05,        // Usage (Headset)
      0xA1, 0x01,        // Collection (Application)
      0x85, 0x02,        //   Report ID (2)
      0x05, 0x0B,        //   Usage Page (Telephony)
      0x15, 0x00,        //   Logical Minimum (0)
      0x25, 0x01,        //   Logical Maximum (1)
      0x09, 0x20,        //   Usage (Hook Switch)
      0x09, 0x97,        //   Usage (Line Busy Tone)
      0x09, 0x2A,        //   Usage (Line)
      0x75, 0x01,        //   Report Size (1)
      0x95, 0x03,        //   Report Count (3)
      0x81, 0x23,        //   Input (Const,Var,Abs,No Wrap,Linear,No Preferred
                         //   State,No Null Position)
      0x09, 0x2F,        //   Usage (Phone Mute)
      0x09, 0x21,        //   Usage (Flash)
      0x09, 0x24,        //   Usage (Redial)
      0x09, 0x50,        //   Usage (Speed Dial)
      0x75, 0x01,        //   Report Size (1)
      0x95, 0x04,        //   Report Count (4)
      0x81, 0x07,        //   Input (Const,Var,Rel,No Wrap,Linear,Preferred
                         //   State,No Null Position)
      0x09, 0x06,        //   Usage (Telephony Key Pad)
      0xA1, 0x02,        //   Collection (Logical)
      0x19, 0xB0,        //     Usage Minimum (Phone Key 0)
      0x29, 0xBB,        //     Usage Maximum (Phone Key Pound)
      0x15, 0x00,        //     Logical Minimum (0)
      0x25, 0x0C,        //     Logical Maximum (12)
      0x75, 0x04,        //     Report Size (4)
      0x95, 0x01,        //     Report Count (1)
      0x81, 0x40,        //     Input (Data,Array,Abs,No Wrap,Linear,Preferred
                         //     State,Null State)
      0xC0,              //   End Collection
      0x09, 0x07,        //   Usage (Programmable Button)
      0x15, 0x00,        //   Logical Minimum (0)
      0x25, 0x01,        //   Logical Maximum (1)
      0x05, 0x09,        //   Usage Page (Button)
      0x75, 0x01,        //   Report Size (1)
      0x95, 0x01,        //   Report Count (1)
      0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position)
      0x75, 0x01,        //   Report Size (1)
      0x95, 0x04,        //   Report Count (4)
      0x81, 0x01,        //   Input (Const,Array,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position)
      0x05, 0x08,        //   Usage Page (LEDs)
      0x15, 0x00,        //   Logical Minimum (0)
      0x25, 0x01,        //   Logical Maximum (1)
      0x09, 0x17,        //   Usage (Off-Hook)
      0x09, 0x1E,        //   Usage (Speaker)
      0x09, 0x09,        //   Usage (Mute)
      0x09, 0x18,        //   Usage (Ring)
      0x09, 0x20,        //   Usage (Hold)
      0x09, 0x21,        //   Usage (Microphone)
      0x09, 0x2A,        //   Usage (On-Line)
      0x75, 0x01,        //   Report Size (1)
      0x95, 0x07,        //   Report Count (7)
      0x91, 0x22,        //   Output (Data,Var,Abs,No Wrap,Linear,No Preferred
                         //   State,No Null Position,Non-volatile)
      0x05, 0x0B,        //   Usage Page (Telephony)
      0x15, 0x00,        //   Logical Minimum (0)
      0x25, 0x01,        //   Logical Maximum (1)
      0x09, 0x9E,        //   Usage (Ringer)
      0x75, 0x01,        //   Report Size (1)
      0x95, 0x01,        //   Report Count (1)
      0x91, 0x22,        //   Output (Data,Var,Abs,No Wrap,Linear,No Preferred
                         //   State,No Null Position,Non-volatile)
      0x75, 0x01,        //   Report Size (1)
      0x95, 0x08,        //   Report Count (8)
      0x91, 0x01,        //   Output (Const,Array,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0xC0,              // End Collection
      0x06, 0x00, 0xFF,  // Usage Page (Vendor Defined 0xFF00)
      0x09, 0x01,        // Usage (0x01)
      0xA1, 0x01,        // Collection (Application)
      0x85, 0x05,        //   Report ID (5)
      0x09, 0x01,        //   Usage (0x01)
      0x15, 0x00,        //   Logical Minimum (0)
      0x26, 0xFF, 0x00,  //   Logical Maximum (255)
      0x75, 0x08,        //   Report Size (8)
      0x95, 0x3E,        //   Report Count (62)
      0x92, 0x02, 0x01,  //   Output (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile,Buffered
                         //   Bytes)
      0x09, 0x01,        //   Usage (0x01)
      0x15, 0x00,        //   Logical Minimum (0)
      0x26, 0xFF, 0x00,  //   Logical Maximum (255)
      0x75, 0x08,        //   Report Size (8)
      0x95, 0x3E,        //   Report Count (62)
      0x82, 0x02, 0x01,  //   Input (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Buffered Bytes)
      0x85, 0x04,        //   Report ID (4)
      0x06, 0x30, 0xFF,  //   Usage Page (Vendor Defined 0xFF30)
      0x15, 0x00,        //   Logical Minimum (0)
      0x25, 0x01,        //   Logical Maximum (1)
      0x09, 0x20,        //   Usage (0x20)
      0x0A, 0xFB, 0xFF,  //   Usage (0xFFFB)
      0x09, 0x97,        //   Usage (0x97)
      0x09, 0x2A,        //   Usage (0x2A)
      0x75, 0x01,        //   Report Size (1)
      0x95, 0x04,        //   Report Count (4)
      0x81, 0x23,        //   Input (Const,Var,Abs,No Wrap,Linear,No Preferred
                         //   State,No Null Position)
      0x09, 0x2F,        //   Usage (0x2F)
      0x09, 0x21,        //   Usage (0x21)
      0x09, 0x24,        //   Usage (0x24)
      0x0A, 0xFD, 0xFF,  //   Usage (0xFFFD)
      0x09, 0x50,        //   Usage (0x50)
      0x75, 0x01,        //   Report Size (1)
      0x95, 0x05,        //   Report Count (5)
      0x81, 0x07,        //   Input (Const,Var,Rel,No Wrap,Linear,Preferred
                         //   State,No Null Position)
      0x09, 0x06,        //   Usage (0x06)
      0xA1, 0x02,        //   Collection (Logical)
      0x19, 0xB0,        //     Usage Minimum (0xB0)
      0x29, 0xBB,        //     Usage Maximum (0xBB)
      0x15, 0x00,        //     Logical Minimum (0)
      0x25, 0x0C,        //     Logical Maximum (12)
      0x75, 0x04,        //     Report Size (4)
      0x95, 0x01,        //     Report Count (1)
      0x81, 0x40,        //     Input (Data,Array,Abs,No Wrap,Linear,Preferred
                         //     State,Null State)
      0xC0,              //   End Collection
      0x0A, 0xFC, 0xFF,  //   Usage (0xFFFC)
      0x15, 0x00,        //   Logical Minimum (0)
      0x25, 0x01,        //   Logical Maximum (1)
      0x75, 0x01,        //   Report Size (1)
      0x95, 0x01,        //   Report Count (1)
      0x81, 0x23,        //   Input (Const,Var,Abs,No Wrap,Linear,No Preferred
                         //   State,No Null Position)
      0x75, 0x01,        //   Report Size (1)
      0x95, 0x02,        //   Report Count (2)
      0x81, 0x01,        //   Input (Const,Array,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position)
      0x06, 0x40, 0xFF,  //   Usage Page (Vendor Defined 0xFF40)
      0x15, 0x00,        //   Logical Minimum (0)
      0x25, 0x01,        //   Logical Maximum (1)
      0x09, 0x17,        //   Usage (0x17)
      0x0A, 0xFB, 0xFF,  //   Usage (0xFFFB)
      0x09, 0x09,        //   Usage (0x09)
      0x09, 0x18,        //   Usage (0x18)
      0x09, 0x20,        //   Usage (0x20)
      0x09, 0x21,        //   Usage (0x21)
      0x09, 0x2A,        //   Usage (0x2A)
      0x75, 0x01,        //   Report Size (1)
      0x95, 0x07,        //   Report Count (7)
      0x91, 0x22,        //   Output (Data,Var,Abs,No Wrap,Linear,No Preferred
                         //   State,No Null Position,Non-volatile)
      0x06, 0x30, 0xFF,  //   Usage Page (Vendor Defined 0xFF30)
      0x15, 0x00,        //   Logical Minimum (0)
      0x25, 0x01,        //   Logical Maximum (1)
      0x09, 0x9E,        //   Usage (0x9E)
      0x75, 0x01,        //   Report Size (1)
      0x95, 0x01,        //   Report Count (1)
      0x91, 0x22,        //   Output (Data,Var,Abs,No Wrap,Linear,No Preferred
                         //   State,No Null Position,Non-volatile)
      0x75, 0x01,        //   Report Size (1)
      0x95, 0x08,        //   Report Count (8)
      0x91, 0x01,        //   Output (Const,Array,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0x0A, 0xFF, 0xFF,  //   Usage (0xFFFF)
      0x15, 0x00,        //   Logical Minimum (0)
      0x25, 0x01,        //   Logical Maximum (1)
      0x75, 0x01,        //   Report Size (1)
      0x95, 0x01,        //   Report Count (1)
      0xB1, 0x22,        //   Feature (Data,Var,Abs,No Wrap,Linear,No Preferred
                         //   State,No Null Position,Non-volatile)
      0x75, 0x01,        //   Report Size (1)
      0x95, 0x07,        //   Report Count (7)
      0xB1, 0x01,        //   Feature (Const,Array,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0xC0,              // End Collection
      0x05, 0x0C,        // Usage Page (Consumer)
      0x09, 0x01,        // Usage (Consumer Control)
      0xA1, 0x01,        // Collection (Application)
      0x85, 0x01,        //   Report ID (1)
      0x05, 0x0C,        //   Usage Page (Consumer)
      0x15, 0x00,        //   Logical Minimum (0)
      0x25, 0x01,        //   Logical Maximum (1)
      0x09, 0xEA,        //   Usage (Volume Decrement)
      0x09, 0xE9,        //   Usage (Volume Increment)
      0x09, 0xE2,        //   Usage (Mute)
      0x09, 0xCD,        //   Usage (Play/Pause)
      0x09, 0xB7,        //   Usage (Stop)
      0x09, 0xB5,        //   Usage (Scan Next Track)
      0x09, 0xB6,        //   Usage (Scan Previous Track)
      0x09, 0xB3,        //   Usage (Fast Forward)
      0x09, 0xB4,        //   Usage (Rewind)
      0x75, 0x01,        //   Report Size (1)
      0x95, 0x09,        //   Report Count (9)
      0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position)
      0x75, 0x01,        //   Report Size (1)
      0x95, 0x07,        //   Report Count (7)
      0x81, 0x01,        //   Input (Const,Array,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position)
      0xC0,              // End Collection
  };
  return base::make_span(kJabraLink380c);
}

// static
base::span<const uint8_t> TestReportDescriptors::FidoU2fHid() {
  constexpr uint8_t kFidoU2fHid[] = {
      0x06, 0xD0, 0xF1,  // Usage Page (Reserved 0xF1D0)
      0x09, 0x01,        // Usage (0x01)
      0xA1, 0x01,        // Collection (Application)
      0x09, 0x20,        //   Usage (0x20)
      0x15, 0x00,        //   Logical Minimum (0)
      0x26, 0xFF, 0x00,  //   Logical Maximum (255)
      0x75, 0x08,        //   Report Size (8)
      0x95, 0x40,        //   Report Count (64)
      0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position)
      0x09, 0x21,        //   Usage (0x21)
      0x15, 0x00,        //   Logical Minimum (0)
      0x26, 0xFF, 0x00,  //   Logical Maximum (255)
      0x75, 0x08,        //   Report Size (8)
      0x95, 0x40,        //   Report Count (64)
      0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position,Non-volatile)
      0xC0,              // End Collection
  };
  return base::make_span(kFidoU2fHid);
}

base::span<const uint8_t> TestReportDescriptors::RfideasPcproxBadgeReader() {
  constexpr uint8_t kRfideasPcproxBadgeReader[] = {
      0x05, 0x01,  // Usage Page (Generic Desktop Ctrls)
      0x09, 0x06,  // Usage (Keyboard)
      0xA1, 0x01,  // Collection (Application)
      0x05, 0x07,  //   Usage Page (Kbrd/Keypad)
      0x19, 0xE0,  //   Usage Minimum (0xE0)
      0x29, 0xE7,  //   Usage Maximum (0xE7)
      0x15, 0x00,  //   Logical Minimum (0)
      0x25, 0x01,  //   Logical Maximum (1)
      0x75, 0x01,  //   Report Size (1)
      0x95, 0x08,  //   Report Count (8)
      0x81, 0x02,  //   Input (Data,Var,Abs,No Wrap,Linear,Preferred
                   //   State,No Null Position)
      0x95, 0x01,  //   Report Count (1)
      0x75, 0x08,  //   Report Size (8)
      0x81, 0x03,  //   Input (Const,Var,Abs,No Wrap,Linear,Preferred
                   //   State,No Null Position)
      0x95, 0x05,  //   Report Count (5)
      0x75, 0x01,  //   Report Size (1)
      0x05, 0x08,  //   Usage Page (LEDs)
      0x19, 0x01,  //   Usage Minimum (Num Lock)
      0x29, 0x05,  //   Usage Maximum (Kana)
      0x91, 0x02,  //   Output (Data,Var,Abs,No Wrap,Linear,Preferred
                   //   State,No Null Position,Non-volatile)
      0x95, 0x01,  //   Report Count (1)
      0x75, 0x03,  //   Report Size (3)
      0x91, 0x03,  //   Output (Const,Var,Abs,No Wrap,Linear,Preferred
                   //   State,No Null Position,Non-volatile)
      0x95, 0x06,  //   Report Count (6)
      0x75, 0x08,  //   Report Size (8)
      0x15, 0x00,  //   Logical Minimum (0)
      0x25, 0x65,  //   Logical Maximum (101)
      0x05, 0x07,  //   Usage Page (Kbrd/Keypad)
      0x19, 0x00,  //   Usage Minimum (0x00)
      0x29, 0x65,  //   Usage Maximum (0x65)
      0x81, 0x00,  //   Input (Data,Array,Abs,No Wrap,Linear,Preferred
                   //   State,No Null Position)
      0x09, 0xA0,  //   Usage (0xA0)
      0x15, 0x80,  //   Logical Minimum (-128)
      0x25, 0x7F,  //   Logical Maximum (127)
      0x75, 0x08,  //   Report Size (8)
      0x95, 0x08,  //   Report Count (8)
      0xB1, 0x02,  //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred
                   //   State,No Null Position,Non-volatile)
      0xC0,        // End Collection
  };
  return base::make_span(kRfideasPcproxBadgeReader);
}

}  // namespace device
