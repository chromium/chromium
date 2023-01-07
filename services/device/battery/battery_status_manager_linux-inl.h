// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_BATTERY_BATTERY_STATUS_MANAGER_LINUX_INL_H_
#define SERVICES_DEVICE_BATTERY_BATTERY_STATUS_MANAGER_LINUX_INL_H_

namespace device {
const char kUPowerInterfaceName[] = "org.freedesktop.UPower";
const char kUPowerServiceName[] = "org.freedesktop.UPower";
const char kUPowerMethodEnumerateDevices[] = "EnumerateDevices";
const char kUPowerMethodGetDisplayDevice[] = "GetDisplayDevice";
const char kUPowerPath[] = "/org/freedesktop/UPower";
const char kUPowerPropertyDaemonVersion[] = "DaemonVersion";
const char kUPowerSignalDeviceAdded[] = "DeviceAdded";
const char kUPowerSignalDeviceRemoved[] = "DeviceRemoved";

const char kUPowerDeviceInterfaceName[] = "org.freedesktop.UPower.Device";
const char kUPowerDevicePropertyIsPresent[] = "IsPresent";
const char kUPowerDevicePropertyPercentage[] = "Percentage";
const char kUPowerDevicePropertyState[] = "State";
const char kUPowerDevicePropertyTimeToEmpty[] = "TimeToEmpty";
const char kUPowerDevicePropertyTimeToFull[] = "TimeToFull";
const char kUPowerDevicePropertyType[] = "Type";
const char kUPowerDeviceSignalChanged[] = "Changed";
}  // namespace device

#endif  // SERVICES_DEVICE_BATTERY_BATTERY_STATUS_MANAGER_LINUX_INL_H_
