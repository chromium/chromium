// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/battery/battery_status_manager_win.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

TEST(BatteryStatusManagerWinTest, ACLineStatusOffline) {
  SYSTEM_POWER_STATUS win_status;
  win_status.ACLineStatus = WIN_AC_LINE_STATUS_OFFLINE;
  win_status.BatteryLifePercent = 100;
  win_status.BatteryLifeTime = 200;

  mojom::BatteryStatus status = ComputeWebBatteryStatus(win_status);
  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(200, status.discharging_time);
  EXPECT_EQ(1, status.level);
}

TEST(BatteryStatusManagerWinTest, ACLineStatusOfflineDischargingTimeUnknown) {
  SYSTEM_POWER_STATUS win_status;
  win_status.ACLineStatus = WIN_AC_LINE_STATUS_OFFLINE;
  win_status.BatteryLifePercent = 100;
  win_status.BatteryLifeTime = (DWORD)-1;

  mojom::BatteryStatus status = ComputeWebBatteryStatus(win_status);
  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(1, status.level);
}

TEST(BatteryStatusManagerWinTest, ACLineStatusOnline) {
  SYSTEM_POWER_STATUS win_status;
  win_status.ACLineStatus = WIN_AC_LINE_STATUS_ONLINE;
  win_status.BatteryLifePercent = 50;
  win_status.BatteryLifeTime = 200;

  mojom::BatteryStatus status = ComputeWebBatteryStatus(win_status);
  EXPECT_TRUE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(.5, status.level);
}

TEST(BatteryStatusManagerWinTest, ACLineStatusOnlineFullBattery) {
  SYSTEM_POWER_STATUS win_status;
  win_status.ACLineStatus = WIN_AC_LINE_STATUS_ONLINE;
  win_status.BatteryLifePercent = 100;
  win_status.BatteryLifeTime = 200;

  mojom::BatteryStatus status = ComputeWebBatteryStatus(win_status);
  EXPECT_TRUE(status.charging);
  EXPECT_EQ(0, status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(1, status.level);
}

TEST(BatteryStatusManagerWinTest, ACLineStatusUnknown) {
  SYSTEM_POWER_STATUS win_status;
  win_status.ACLineStatus = WIN_AC_LINE_STATUS_UNKNOWN;
  win_status.BatteryLifePercent = 50;
  win_status.BatteryLifeTime = 200;

  mojom::BatteryStatus status = ComputeWebBatteryStatus(win_status);
  EXPECT_TRUE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.charging_time);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.discharging_time);
  EXPECT_EQ(.5, status.level);
}

}  // namespace

}  // namespace device
