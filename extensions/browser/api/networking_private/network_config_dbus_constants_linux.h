// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORK_CONFIG_DBUS_CONSTANTS_LINUX_H_
#define EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORK_CONFIG_DBUS_CONSTANTS_LINUX_H_

namespace extensions::networking_private {

// Network manager API strings.
extern const char kNetworkManagerPath[];
extern const char kNetworkManagerNamespace[];
extern const char kNetworkManagerAccessPointNamespace[];
extern const char kNetworkManagerActiveConnectionNamespace[];
extern const char kNetworkManagerDeviceNamespace[];
extern const char kNetworkManagerWirelessDeviceNamespace[];
extern const char kNetworkManagerActiveConnections[];
extern const char kNetworkManagerSpecificObject[];
extern const char kNetworkManagerDeviceType[];
extern const char kNetworkManagerGetDevicesMethod[];
extern const char kNetworkManagerGetAccessPointsMethod[];
extern const char kNetworkManagerDisconnectMethod[];
extern const char kNetworkManagerAddAndActivateConnectionMethod[];
extern const char kNetworkManagerGetMethod[];
extern const char kNetworkManagerSsidProperty[];
extern const char kNetworkManagerStrengthProperty[];
extern const char kNetworkManagerRsnFlagsProperty[];
extern const char kNetworkManagerWpaFlagsProperty[];

// Network manager connection configuration strings.
extern const char kNetworkManagerConnectionConfig80211Wireless[];
extern const char kNetworkManagerConnectionConfigSsid[];

}  // namespace extensions::networking_private

#endif  // EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORK_CONFIG_DBUS_CONSTANTS_LINUX_H_
