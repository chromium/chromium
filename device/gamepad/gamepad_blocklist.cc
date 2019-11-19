// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_blocklist.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>

namespace device {
namespace {

constexpr uint16_t kVendorAlps = 0x044e;
constexpr uint16_t kVendorApple = 0x05ac;
constexpr uint16_t kVendorAtmel = 0x03eb;
constexpr uint16_t kVendorAwardSoftware = 0x0412;
constexpr uint16_t kVendorBlue = 0xb58e;
constexpr uint16_t kVendorCorsair = 0x1b3c;
constexpr uint16_t kVendorCypressSemiconductor = 0x04b4;
constexpr uint16_t kVendorDarfonElectronics = 0x0d62;
constexpr uint16_t kVendorDWav = 0x0eef;
constexpr uint16_t kVendorElanMicroelectronics = 0x04f3;
constexpr uint16_t kVendorEloTouchSystems = 0x04e7;
constexpr uint16_t kVendorHoltekSemiconductor = 0x04d9;
constexpr uint16_t kVendorLenovo = 0x17ef;
constexpr uint16_t kVendorLgd = 0x1fd2;
constexpr uint16_t kVendorMicrosoft = 0x045e;
constexpr uint16_t kVendorOculus = 0x2833;
constexpr uint16_t kVendorQuantaComputer = 0x0408;
constexpr uint16_t kVendorSiliconIntegratedSystems = 0x0457;
constexpr uint16_t kVendorSunMicrosystems = 0x0430;
constexpr uint16_t kVendorSynaptics = 0x06cb;
constexpr uint16_t kVendorWacom = 0x056a;

constexpr struct VendorProductPair {
  uint16_t vendor;
  uint16_t product;
} kBlockedDevices[] = {
    // BLUETOOTH HID v0.01 Mouse.
    {kVendorApple, 0x3232},
    // Wooting one keyboard.
    {kVendorAtmel, 0xff01},
    // Wooting two keyboard.
    {kVendorAtmel, 0xff02},
    // Keyboard.
    {kVendorAwardSoftware, 0x7121},
    // Corsair Gaming HARPOON RGB Mouse.
    {kVendorCorsair, 0x1b3c},
    // PenPower Touchpad.
    {kVendorCypressSemiconductor, 0xfef3},
    // USB-HID Keyboard.
    {kVendorDarfonElectronics, 0x9a1a},
    // USB-HID Keyboards.
    {kVendorHoltekSemiconductor, 0x8008},
    {kVendorHoltekSemiconductor, 0x8009},
    {kVendorHoltekSemiconductor, 0xa292},
    // LiteOn Lenovo USB Keyboard with TrackPoint.
    {kVendorLenovo, 0x6009},
    // LiteOn Lenovo Traditional USB Keyboard.
    {kVendorLenovo, 0x6099},
    // Microsoft Wired Keyboard 600.
    {kVendorMicrosoft, 0x0750},
    // Surface Keyboard.
    {kVendorMicrosoft, 0x07cd},
    // Surface Keyboard.
    {kVendorMicrosoft, 0x0922},
};

// Devices from these vendors are always blocked.
constexpr uint16_t kBlockedVendors[] = {
    // Some Blue Yeti microphones are recognized as gamepads.
    kVendorBlue,
    // Block all Oculus devices. Oculus VR controllers are handled by WebXR.
    kVendorOculus,
    // Touchpad and touchscreen vendors.
    kVendorAlps,
    kVendorDWav,
    kVendorElanMicroelectronics,
    kVendorEloTouchSystems,
    kVendorLgd,
    kVendorQuantaComputer,
    kVendorSiliconIntegratedSystems,
    kVendorSunMicrosystems,
    kVendorSynaptics,
    kVendorWacom,
};

}  // namespace

bool GamepadIsExcluded(uint16_t vendor_id, uint16_t product_id) {
  const uint16_t* vendors_begin = std::begin(kBlockedVendors);
  const uint16_t* vendors_end = std::end(kBlockedVendors);
  if (std::find(vendors_begin, vendors_end, vendor_id) != vendors_end)
    return true;

  const VendorProductPair* devices_begin = std::begin(kBlockedDevices);
  const VendorProductPair* devices_end = std::end(kBlockedDevices);
  return std::find_if(
             devices_begin, devices_end, [=](const VendorProductPair& item) {
               return vendor_id == item.vendor && product_id == item.product;
             }) != devices_end;
}

}  // namespace device
