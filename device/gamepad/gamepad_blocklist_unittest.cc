// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/gamepad/gamepad_blocklist.h"

#include "device/gamepad/gamepad_id_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace {
// Blocked devices, taken from the gamepad blocklist.
constexpr std::pair<uint16_t, uint16_t> kBlockedDevices[] = {
    {0x045e, 0x0922},  // Microsoft keyboard
    {0x05ac, 0x3232},  // Apple(?) bluetooth mouse
    {0x17ef, 0x6099},  // Lenovo keyboard
};
constexpr size_t kBlockedDevicesLength = std::size(kBlockedDevices);

// Known devices from blocked vendors, taken from usb.ids.
// http://www.linux-usb.org/usb.ids
constexpr std::pair<uint16_t, uint16_t> kBlockedVendorDevices[] = {
    {0x056a, 0x50b8},  // Wacom touchpad
    {0x06cb, 0x000f},  // Synaptics touchpad
    {0x2833, 0x0001},  // Oculus Rift DK1 head tracker
    {0x2833, 0x0021},  // Oculus Rift DK2 USB hub
    {0x2833, 0x0031},  // Oculus Rift CV1 subdevice
    {0x2833, 0x0101},  // Oculus latency tester
    {0x2833, 0x0201},  // Oculus Rift DK2 camera
    {0x2833, 0x0211},  // Oculus Rift CV1 sensor
    {0x2833, 0x0330},  // Oculus Rift CV1 audio port
    {0x2833, 0x1031},  // Oculus Rift CV1 subdevice
    {0x2833, 0x2021},  // Oculus Rift DK2 main unit
    {0x2833, 0x2031},  // Oculus Rift CV1
    {0x2833, 0x3031},  // Oculus Rift CV1 subdevice
    {0xb58e, 0x9e84},  // Blue Yeti Stereo Microphone
};
constexpr size_t kBlockedVendorDevicesLength = std::size(kBlockedVendorDevices);

}  // namespace

TEST(GamepadBlocklistTest, KnownGamepadsNotBlocked) {
  // Known gamepads should not be excluded.
  const auto& gamepads = GamepadIdList::Get().GetGamepadListForTesting();
  for (const auto& item : gamepads) {
    uint16_t vendor = std::get<0>(item);
    uint16_t product = std::get<1>(item);
    EXPECT_FALSE(GamepadIsExcluded(vendor, product));
  }
}

TEST(GamepadBlocklistTest, BlockedDevices) {
  for (size_t i = 0; i < kBlockedDevicesLength; ++i) {
    const uint16_t vendor = kBlockedDevices[i].first;
    const uint16_t product = kBlockedDevices[i].second;

    // Blocked devices should be excluded.
    EXPECT_TRUE(GamepadIsExcluded(vendor, product));

    // Devices with product IDs close to a blocked device should not be
    // excluded.
    EXPECT_FALSE(GamepadIsExcluded(vendor, product - 1));
    EXPECT_FALSE(GamepadIsExcluded(vendor, product + 1));
  }
}

TEST(GamepadBlocklistTest, BlockedVendors) {
  for (size_t i = 0; i < kBlockedVendorDevicesLength; ++i) {
    const uint16_t vendor = kBlockedVendorDevices[i].first;
    const uint16_t product = kBlockedVendorDevices[i].second;

    // Known devices with a blocked vendor ID should be excluded.
    EXPECT_TRUE(GamepadIsExcluded(vendor, product));

    // Devices with nearby product IDs should also be excluded.
    EXPECT_TRUE(GamepadIsExcluded(vendor, product - 1));
    EXPECT_TRUE(GamepadIsExcluded(vendor, product + 1));
  }
}

}  // namespace device
