// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_BATTERY_BATTERY_STATUS_MANAGER_WIN_H_
#define SERVICES_DEVICE_BATTERY_BATTERY_STATUS_MANAGER_WIN_H_

#include <windows.h>

#include "services/device/public/mojom/battery_status.mojom.h"

namespace device {

enum WinACLineStatus {
  WIN_AC_LINE_STATUS_OFFLINE = 0,
  WIN_AC_LINE_STATUS_ONLINE = 1,
  WIN_AC_LINE_STATUS_UNKNOWN = 255,
};

// Returns WebBatteryStatus corresponding to the given SYSTEM_POWER_STATUS.
mojom::BatteryStatus ComputeWebBatteryStatus(
    const SYSTEM_POWER_STATUS& win_status);

}  // namespace device

#endif  // SERVICES_DEVICE_BATTERY_BATTERY_STATUS_MANAGER_WIN_H_
