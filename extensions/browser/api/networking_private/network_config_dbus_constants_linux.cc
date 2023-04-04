// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/networking_private/network_config_dbus_constants_linux.h"

namespace extensions::networking_private {

// Network manager API strings.
const char kNetworkManagerPath[] = "/org/freedesktop/NetworkManager";
const char kNetworkManagerNamespace[] = "org.freedesktop.NetworkManager";
const char kNetworkManagerAccessPointNamespace[] =
    "org.freedesktop.NetworkManager.AccessPoint";
const char kNetworkManagerActiveConnectionNamespace[] =
    "org.freedesktop.NetworkManager.Connection.Active";
const char kNetworkManagerDeviceNamespace[] =
    "org.freedesktop.NetworkManager.Device";
const char kNetworkManagerWirelessDeviceNamespace[] =
    "org.freedesktop.NetworkManager.Device.Wireless";
const char kNetworkManagerActiveConnections[] = "ActiveConnections";
const char kNetworkManagerSpecificObject[] = "SpecificObject";
const char kNetworkManagerDeviceType[] = "DeviceType";
const char kNetworkManagerGetDevicesMethod[] = "GetDevices";
const char kNetworkManagerGetAccessPointsMethod[] = "GetAccessPoints";
const char kNetworkManagerDisconnectMethod[] = "Disconnect";
const char kNetworkManagerAddAndActivateConnectionMethod[] =
    "AddAndActivateConnection";
const char kNetworkManagerGetMethod[] = "Get";
const char kNetworkManagerSsidProperty[] = "Ssid";
const char kNetworkManagerStrengthProperty[] = "Strength";
const char kNetworkManagerRsnFlagsProperty[] = "RsnFlags";
const char kNetworkManagerWpaFlagsProperty[] = "WpaFlags";

// Network manager connection configuration strings.
const char kNetworkManagerConnectionConfig80211Wireless[] = "802-11-wireless";
const char kNetworkManagerConnectionConfigSsid[] = "ssid";

}  // namespace extensions::networking_private
