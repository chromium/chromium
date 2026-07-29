// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_device_enumerator_win.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace device::serial_win_internal {

TEST(SerialDeviceEnumeratorWinTest, FindInterfaceStringDescriptorIndex) {
  const std::vector<uint8_t> configuration_descriptor = {
      9,    2, 27, 0, 1, 1, 0, 0x80, 50, 9,    4, 0, 0, 1,
      0xff, 0, 0,  7, 9, 4, 1, 0,    1,  0xff, 0, 0, 9,
  };

  EXPECT_EQ(7u,
            FindInterfaceStringDescriptorIndex(configuration_descriptor, 0));
  EXPECT_EQ(9u,
            FindInterfaceStringDescriptorIndex(configuration_descriptor, 1));
  EXPECT_EQ(std::nullopt,
            FindInterfaceStringDescriptorIndex(configuration_descriptor, 2));
}

TEST(SerialDeviceEnumeratorWinTest,
     FindInterfaceStringDescriptorIndexRejectsBadDescriptors) {
  EXPECT_EQ(std::nullopt,
            FindInterfaceStringDescriptorIndex(std::vector<uint8_t>{0}, 0));
  EXPECT_EQ(std::nullopt,
            FindInterfaceStringDescriptorIndex(std::vector<uint8_t>{3, 4}, 0));
  EXPECT_EQ(std::nullopt, FindInterfaceStringDescriptorIndex(
                              std::vector<uint8_t>{10, 4, 0}, 0));
}

TEST(SerialDeviceEnumeratorWinTest, BuildUsbDisplayName) {
  EXPECT_EQ("Arduino Mega",
            BuildUsbDisplayName(std::optional<std::string>("Arduino Mega"),
                                std::nullopt));
  EXPECT_EQ("CDC Data",
            BuildUsbDisplayName(std::nullopt,
                                std::optional<std::string>("CDC Data")));
  EXPECT_EQ("Arduino Mega",
            BuildUsbDisplayName(std::optional<std::string>("Arduino Mega"),
                                std::optional<std::string>("Arduino Mega")));
  EXPECT_EQ("Arduino Mega - CDC Data",
            BuildUsbDisplayName(std::optional<std::string>("Arduino Mega"),
                                std::optional<std::string>("CDC Data")));
  EXPECT_EQ(std::nullopt, BuildUsbDisplayName(std::nullopt, std::nullopt));
}

}  // namespace device::serial_win_internal
